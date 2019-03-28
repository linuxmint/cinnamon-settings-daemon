/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright © 2001 Ximian, Inc.
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 */

#include "config.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libnotify/notify.h>

#include <X11/XKBlib.h>
#include <X11/extensions/XKBstr.h>

#include "cinnamon-settings-profile.h"
#include "csd-a11y-keyboard-manager.h"
#include "csd-a11y-preferences-dialog.h"

#define KEYBOARD_A11Y_SCHEMA "org.cinnamon.desktop.a11y.keyboard"
#define NOTIFICATION_TIMEOUT 30

#define CSD_A11Y_KEYBOARD_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CSD_TYPE_A11Y_KEYBOARD_MANAGER, CsdA11yKeyboardManagerPrivate))

struct CsdA11yKeyboardManagerPrivate
{
        guint             start_idle_id;
        int               xkbEventBase;
        GdkDeviceManager *device_manager;
        guint             device_added_id;
        gboolean          stickykeys_shortcut_val;
        gboolean          slowkeys_shortcut_val;
        GtkWidget        *stickykeys_alert;
        GtkWidget        *slowkeys_alert;
        GtkWidget        *preferences_dialog;

        GtkStatusIcon    *status_icon;

        GSettings        *settings;

        NotifyNotification *notification;
};

static void     csd_a11y_keyboard_manager_finalize    (GObject             *object);
static void     csd_a11y_keyboard_manager_ensure_status_icon (CsdA11yKeyboardManager *manager);
static void     set_server_from_gsettings (CsdA11yKeyboardManager *manager);

G_DEFINE_TYPE (CsdA11yKeyboardManager, csd_a11y_keyboard_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static void
device_added_cb (GdkDeviceManager       *device_manager,
                 GdkDevice              *device,
                 CsdA11yKeyboardManager *manager)
{
        if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
                set_server_from_gsettings (manager);
}

static void
set_devicepresence_handler (CsdA11yKeyboardManager *manager)
{
        GdkDeviceManager *device_manager;

        device_manager = gdk_display_get_device_manager (gdk_display_get_default ());
        if (device_manager == NULL)
                return;

        manager->priv->device_manager = device_manager;
        manager->priv->device_added_id = g_signal_connect (G_OBJECT (device_manager), "device-added",
                                                           G_CALLBACK (device_added_cb), manager);
}

static gboolean
xkb_enabled (CsdA11yKeyboardManager *manager)
{
        int opcode, errorBase, major, minor;

        if (!XkbQueryExtension (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                &opcode,
                                &manager->priv->xkbEventBase,
                                &errorBase,
                                &major,
                               &minor))
                return FALSE;

        if (!XkbUseExtension (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), &major, &minor))
                return FALSE;

        return TRUE;
}

static XkbDescRec *
get_xkb_desc_rec (CsdA11yKeyboardManager *manager)
{
        XkbDescRec *desc;
        Status      status = Success;

        gdk_x11_display_error_trap_push (gdk_display_get_default ());
        desc = XkbGetMap (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), XkbAllMapComponentsMask, XkbUseCoreKbd);
        if (desc != NULL) {
                desc->ctrls = NULL;
                status = XkbGetControls (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), XkbAllControlsMask, desc);
        }
        gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());

        g_return_val_if_fail (desc != NULL, NULL);
        g_return_val_if_fail (desc->ctrls != NULL, NULL);
        g_return_val_if_fail (status == Success, NULL);

        return desc;
}

static int
get_int (GSettings  *settings,
         char const *key)
{
        int res = g_settings_get_int  (settings, key);
        if (res <= 0) {
                res = 1;
        }
        return res;
}

static gboolean
set_int (GSettings      *settings,
         char const     *key,
         int             val)
{
        int prev_val;

        prev_val = g_settings_get_int (settings, key);
        g_settings_set_int (settings, key, val);
        if (val != prev_val) {
                g_debug ("%s changed", key);
        }

        return val != prev_val;
}

static gboolean
set_bool (GSettings      *settings,
          char const     *key,
          int             val)
{
        gboolean bval = (val != 0);
        gboolean prev_val;

        prev_val = g_settings_get_boolean (settings, key);
        g_settings_set_boolean (settings, key, bval ? TRUE : FALSE);
        if (bval != prev_val) {
                g_debug ("%s changed", key);
                return TRUE;
        }
        return (bval != prev_val);
}

static unsigned long
set_clear (gboolean      flag,
           unsigned long value,
           unsigned long mask)
{
        if (flag) {
                return value | mask;
        }
        return value & ~mask;
}

static gboolean
set_ctrl_from_gsettings (XkbDescRec   *desc,
                         GSettings    *settings,
                         char const   *key,
                         unsigned long mask)
{
        gboolean result = g_settings_get_boolean (settings, key);
        desc->ctrls->enabled_ctrls = set_clear (result, desc->ctrls->enabled_ctrls, mask);
        return result;
}

static void
set_server_from_gsettings (CsdA11yKeyboardManager *manager)
{
        XkbDescRec      *desc;
        gboolean         enable_accessX;
        GSettings       *settings;

        cinnamon_settings_profile_start (NULL);

        desc = get_xkb_desc_rec (manager);
        if (!desc) {
                return;
        }

        settings = manager->priv->settings;

        /* general */
        enable_accessX = g_settings_get_boolean (settings, "enable");

        desc->ctrls->enabled_ctrls = set_clear (enable_accessX,
                                                desc->ctrls->enabled_ctrls,
                                                XkbAccessXKeysMask);

        if (set_ctrl_from_gsettings (desc, settings, "timeout-enable",
                                     XkbAccessXTimeoutMask)) {
                desc->ctrls->ax_timeout = get_int (settings, "disable-timeout");
                /* disable only the master flag via the server we will disable
                 * the rest on the rebound without affecting GSettings state
                 * don't change the option flags at all.
                 */
                desc->ctrls->axt_ctrls_mask = XkbAccessXKeysMask | XkbAccessXFeedbackMask;
                desc->ctrls->axt_ctrls_values = 0;
                desc->ctrls->axt_opts_mask = 0;
        }

        desc->ctrls->ax_options = set_clear (g_settings_get_boolean (settings, "feature-state-change-beep"),
                                             desc->ctrls->ax_options,
                                             XkbAccessXFeedbackMask | XkbAX_FeatureFBMask | XkbAX_SlowWarnFBMask);

        /* bounce keys */
        if (set_ctrl_from_gsettings (desc, settings, "bouncekeys-enable", XkbBounceKeysMask)) {
                desc->ctrls->debounce_delay = get_int (settings, "bouncekeys-delay");
                desc->ctrls->ax_options = set_clear (g_settings_get_boolean (settings, "bouncekeys-beep-reject"),
                                                     desc->ctrls->ax_options,
                                                     XkbAccessXFeedbackMask | XkbAX_BKRejectFBMask);
        }

        /* mouse keys */
        if (set_ctrl_from_gsettings (desc, settings, "mousekeys-enable", XkbMouseKeysMask | XkbMouseKeysAccelMask)) {
                desc->ctrls->mk_interval     = 100;     /* msec between mousekey events */
                desc->ctrls->mk_curve        = 50;

                /* We store pixels / sec, XKB wants pixels / event */
                desc->ctrls->mk_max_speed    = get_int (settings, "mousekeys-max-speed") / (1000 / desc->ctrls->mk_interval);
                if (desc->ctrls->mk_max_speed <= 0)
                        desc->ctrls->mk_max_speed = 1;

                desc->ctrls->mk_time_to_max = get_int (settings, /* events before max */
                                                       "mousekeys-accel-time") / desc->ctrls->mk_interval;
                if (desc->ctrls->mk_time_to_max <= 0)
                        desc->ctrls->mk_time_to_max = 1;

                desc->ctrls->mk_delay = get_int (settings, /* ms before 1st event */
                                                 "mousekeys-init-delay");
        }

        /* slow keys */
        if (set_ctrl_from_gsettings (desc, settings, "slowkeys-enable", XkbSlowKeysMask)) {
                desc->ctrls->ax_options = set_clear (g_settings_get_boolean (settings, "slowkeys-beep-press"),
                                                     desc->ctrls->ax_options,
                                                     XkbAccessXFeedbackMask | XkbAX_SKPressFBMask);
                desc->ctrls->ax_options = set_clear (g_settings_get_boolean (settings, "slowkeys-beep-accept"),
                                                     desc->ctrls->ax_options,
                                                     XkbAccessXFeedbackMask | XkbAX_SKAcceptFBMask);
                desc->ctrls->ax_options = set_clear (g_settings_get_boolean (settings, "slowkeys-beep-reject"),
                                                     desc->ctrls->ax_options,
                                                     XkbAccessXFeedbackMask | XkbAX_SKRejectFBMask);
                desc->ctrls->slow_keys_delay = get_int (settings, "slowkeys-delay");
                /* anything larger than 500 seems to loose all keyboard input */
                if (desc->ctrls->slow_keys_delay > 500)
                        desc->ctrls->slow_keys_delay = 500;
        }

        /* sticky keys */
        if (set_ctrl_from_gsettings (desc, settings, "stickykeys-enable", XkbStickyKeysMask)) {
                desc->ctrls->ax_options |= XkbAX_LatchToLockMask;
                desc->ctrls->ax_options = set_clear (g_settings_get_boolean (settings, "stickykeys-two-key-off"),
                                                     desc->ctrls->ax_options,
                                                     XkbAccessXFeedbackMask | XkbAX_TwoKeysMask);
                desc->ctrls->ax_options = set_clear (g_settings_get_boolean (settings, "stickykeys-modifier-beep"),
                                                     desc->ctrls->ax_options,
                                                     XkbAccessXFeedbackMask | XkbAX_StickyKeysFBMask);
        }

        /*
        g_debug ("CHANGE to : 0x%x", desc->ctrls->enabled_ctrls);
        g_debug ("CHANGE to : 0x%x (2)", desc->ctrls->ax_options);
        */

        gdk_x11_display_error_trap_push (gdk_display_get_default ());
        XkbSetControls (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                        XkbSlowKeysMask         |
                        XkbBounceKeysMask       |
                        XkbStickyKeysMask       |
                        XkbMouseKeysMask        |
                        XkbMouseKeysAccelMask   |
                        XkbAccessXKeysMask      |
                        XkbAccessXTimeoutMask   |
                        XkbAccessXFeedbackMask  |
                        XkbControlsEnabledMask,
                        desc);

        XkbFreeKeyboard (desc, XkbAllComponentsMask, True);

        XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), FALSE);
        gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());

        cinnamon_settings_profile_end (NULL);
}

static gboolean
ax_response_callback (CsdA11yKeyboardManager *manager,
                      GtkWindow              *parent,
                      gint                    response_id,
                      guint                   revert_controls_mask,
                      gboolean                enabled)
{
        GSettings *settings;
        GdkScreen *screen;
        GError *err;

        settings = manager->priv->settings;

        switch (response_id) {
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_REJECT:
        case GTK_RESPONSE_CANCEL:

                /* we're reverting, so we invert sense of 'enabled' flag */
                g_debug ("cancelling AccessX request");
                if (revert_controls_mask == XkbStickyKeysMask) {
                        g_settings_set_boolean (settings,
                                                "stickykeys-enable",
                                                !enabled);
                } else if (revert_controls_mask == XkbSlowKeysMask) {
                        g_settings_set_boolean (settings,
                                                "slowkeys-enable",
                                                !enabled);
                }

                set_server_from_gsettings (manager);
                break;

        case GTK_RESPONSE_HELP:
                if (!parent)
                        screen = gdk_screen_get_default ();
                else
                        screen = gtk_widget_get_screen (GTK_WIDGET (parent));

                err = NULL;
                if (!gtk_show_uri (screen,
                                   "help:gnome-help/a11y",
                                   gtk_get_current_event_time(),
                                   &err)) {
                        GtkWidget *error_dialog = gtk_message_dialog_new (parent,
                                                                          0,
                                                                          GTK_MESSAGE_ERROR,
                                                                          GTK_BUTTONS_CLOSE,
                                                                          _("There was an error displaying help: %s"),
                                                                          err->message);
                        g_signal_connect (error_dialog, "response",
                                          G_CALLBACK (gtk_widget_destroy), NULL);
                        gtk_window_set_resizable (GTK_WINDOW (error_dialog), FALSE);
                        gtk_widget_show (error_dialog);
                        g_error_free (err);
                }
                return FALSE;
        default:
                break;
        }
        return TRUE;
}

static void
ax_stickykeys_response (GtkDialog              *dialog,
                        gint                    response_id,
                        CsdA11yKeyboardManager *manager)
{
        if (ax_response_callback (manager, GTK_WINDOW (dialog),
                                  response_id, XkbStickyKeysMask,
                                  manager->priv->stickykeys_shortcut_val)) {
                gtk_widget_destroy (GTK_WIDGET (dialog));
        }
}

static void
ax_slowkeys_response (GtkDialog              *dialog,
                      gint                    response_id,
                      CsdA11yKeyboardManager *manager)
{
        if (ax_response_callback (manager, GTK_WINDOW (dialog),
                                  response_id, XkbSlowKeysMask,
                                  manager->priv->slowkeys_shortcut_val)) {
                gtk_widget_destroy (GTK_WIDGET (dialog));
        }
}

static void
maybe_show_status_icon (CsdA11yKeyboardManager *manager)
{
        gboolean     show;

        /* for now, show if accessx is enabled */
        show = g_settings_get_boolean (manager->priv->settings, "enable");

        if (!show && manager->priv->status_icon == NULL)
                return;

        csd_a11y_keyboard_manager_ensure_status_icon (manager);
        gtk_status_icon_set_visible (manager->priv->status_icon, show);
}

static void
on_notification_closed (NotifyNotification     *notification,
                        CsdA11yKeyboardManager *manager)
{
        g_object_unref (manager->priv->notification);
        manager->priv->notification = NULL;
}

static void
on_slow_keys_action (NotifyNotification     *notification,
                     const char             *action,
                     CsdA11yKeyboardManager *manager)
{
        gboolean res;
        int      response_id;

        g_assert (action != NULL);

        if (strcmp (action, "accept") == 0) {
                response_id = GTK_RESPONSE_ACCEPT;
        } else if (strcmp (action, "reject") == 0) {
                response_id = GTK_RESPONSE_REJECT;
        } else {
                return;
        }

        res = ax_response_callback (manager, NULL,
                                    response_id, XkbSlowKeysMask,
                                    manager->priv->slowkeys_shortcut_val);
        if (res) {
                notify_notification_close (manager->priv->notification, NULL);
        }
}

static void
on_sticky_keys_action (NotifyNotification     *notification,
                       const char             *action,
                       CsdA11yKeyboardManager *manager)
{
        gboolean res;
        int      response_id;

        g_assert (action != NULL);

        if (strcmp (action, "accept") == 0) {
                response_id = GTK_RESPONSE_ACCEPT;
        } else if (strcmp (action, "reject") == 0) {
                response_id = GTK_RESPONSE_REJECT;
        } else {
                return;
        }

        res = ax_response_callback (manager, NULL,
                                    response_id, XkbStickyKeysMask,
                                    manager->priv->stickykeys_shortcut_val);
        if (res) {
                notify_notification_close (manager->priv->notification, NULL);
        }
}

static gboolean
ax_slowkeys_warning_post_bubble (CsdA11yKeyboardManager *manager,
                                 gboolean                enabled)
{
        gboolean    res;
        const char *title;
        const char *message;
        GError     *error;

        title = enabled ?
                _("Slow Keys Turned On") :
                _("Slow Keys Turned Off");
        message = _("You just held down the Shift key for 8 seconds.  This is the shortcut "
                    "for the Slow Keys feature, which affects the way your keyboard works.");
        if (manager->priv->status_icon == NULL || ! gtk_status_icon_is_embedded (manager->priv->status_icon)) {
                return FALSE;
        }

        if (manager->priv->slowkeys_alert != NULL) {
                gtk_widget_destroy (manager->priv->slowkeys_alert);
        }

        if (manager->priv->notification != NULL) {
                notify_notification_close (manager->priv->notification, NULL);
        }

        csd_a11y_keyboard_manager_ensure_status_icon (manager);
        manager->priv->notification = notify_notification_new (title,
                                                               message,
                                                               "preferences-desktop-accessibility-symbolic");
        notify_notification_set_app_name (manager->priv->notification, _("Universal Access"));
        notify_notification_set_timeout (manager->priv->notification, NOTIFICATION_TIMEOUT * 1000);
        notify_notification_set_hint (manager->priv->notification, "transient", g_variant_new_boolean (TRUE));

        notify_notification_add_action (manager->priv->notification,
                                        "reject",
                                        enabled ? _("Turn Off") : _("Turn On"),
                                        (NotifyActionCallback) on_slow_keys_action,
                                        manager,
                                        NULL);
        notify_notification_add_action (manager->priv->notification,
                                        "accept",
                                        enabled ? _("Leave On") : _("Leave Off"),
                                        (NotifyActionCallback) on_slow_keys_action,
                                        manager,
                                        NULL);

        g_signal_connect (manager->priv->notification,
                          "closed",
                          G_CALLBACK (on_notification_closed),
                          manager);

        error = NULL;
        res = notify_notification_show (manager->priv->notification, &error);
        if (! res) {
                g_warning ("CsdA11yKeyboardManager: unable to show notification: %s", error->message);
                g_error_free (error);
                notify_notification_close (manager->priv->notification, NULL);
        }

        return res;
}


static void
ax_slowkeys_warning_post_dialog (CsdA11yKeyboardManager *manager,
                                 gboolean                enabled)
{
        const char *title;
        const char *message;

        title = enabled ?
                _("Slow Keys Turned On") :
                _("Slow Keys Turned Off");
        message = _("You just held down the Shift key for 8 seconds.  This is the shortcut "
                    "for the Slow Keys feature, which affects the way your keyboard works.");

        if (manager->priv->slowkeys_alert != NULL) {
                gtk_widget_show (manager->priv->slowkeys_alert);
                return;
        }

        manager->priv->slowkeys_alert = gtk_message_dialog_new (NULL,
                                                                0,
                                                                GTK_MESSAGE_WARNING,
                                                                GTK_BUTTONS_NONE,
                                                                "%s", title);

        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (manager->priv->slowkeys_alert),
                                                  "%s", message);

        gtk_dialog_add_button (GTK_DIALOG (manager->priv->slowkeys_alert),
                               GTK_STOCK_HELP,
                               GTK_RESPONSE_HELP);
        gtk_dialog_add_button (GTK_DIALOG (manager->priv->slowkeys_alert),
                               enabled ? _("_Turn Off") : _("_Turn On"),
                               GTK_RESPONSE_REJECT);
        gtk_dialog_add_button (GTK_DIALOG (manager->priv->slowkeys_alert),
                               enabled ? _("_Leave On") : _("_Leave Off"),
                               GTK_RESPONSE_ACCEPT);

        gtk_window_set_title (GTK_WINDOW (manager->priv->slowkeys_alert), "");
        gtk_window_set_icon_name (GTK_WINDOW (manager->priv->slowkeys_alert),
                                  "preferences-desktop-accessibility");
        gtk_dialog_set_default_response (GTK_DIALOG (manager->priv->slowkeys_alert),
                                         GTK_RESPONSE_ACCEPT);

        g_signal_connect (manager->priv->slowkeys_alert,
                          "response",
                          G_CALLBACK (ax_slowkeys_response),
                          manager);
        gtk_widget_show (manager->priv->slowkeys_alert);

        g_object_add_weak_pointer (G_OBJECT (manager->priv->slowkeys_alert),
                                   (gpointer*) &manager->priv->slowkeys_alert);
}

static void
ax_slowkeys_warning_post (CsdA11yKeyboardManager *manager,
                          gboolean                enabled)
{

        manager->priv->slowkeys_shortcut_val = enabled;

        /* alway try to show something */
        if (! ax_slowkeys_warning_post_bubble (manager, enabled)) {
                ax_slowkeys_warning_post_dialog (manager, enabled);
        }
}

static gboolean
ax_stickykeys_warning_post_bubble (CsdA11yKeyboardManager *manager,
                                   gboolean                enabled)
{
#if 1
        gboolean    res;
        const char *title;
        const char *message;
        GError     *error;

        title = enabled ?
                _("Sticky Keys Turned On") :
                _("Sticky Keys Turned Off");
        message = enabled ?
                _("You just pressed the Shift key 5 times in a row.  This is the shortcut "
                  "for the Sticky Keys feature, which affects the way your keyboard works.") :
                _("You just pressed two keys at once, or pressed the Shift key 5 times in a row.  "
                  "This turns off the Sticky Keys feature, which affects the way your keyboard works.");

        if (manager->priv->status_icon == NULL || ! gtk_status_icon_is_embedded (manager->priv->status_icon)) {
                return FALSE;
        }

        if (manager->priv->slowkeys_alert != NULL) {
                gtk_widget_destroy (manager->priv->slowkeys_alert);
        }

        if (manager->priv->notification != NULL) {
                notify_notification_close (manager->priv->notification, NULL);
        }

        csd_a11y_keyboard_manager_ensure_status_icon (manager);
        manager->priv->notification = notify_notification_new (title,
                                                               message,
                                                               "preferences-desktop-accessibility-symbolic");
        notify_notification_set_app_name (manager->priv->notification, _("Universal Access"));
        notify_notification_set_timeout (manager->priv->notification, NOTIFICATION_TIMEOUT * 1000);
        notify_notification_set_hint (manager->priv->notification, "transient", g_variant_new_boolean (TRUE));

        notify_notification_add_action (manager->priv->notification,
                                        "reject",
                                        enabled ? _("Turn Off") : _("Turn On"),
                                        (NotifyActionCallback) on_sticky_keys_action,
                                        manager,
                                        NULL);
        notify_notification_add_action (manager->priv->notification,
                                        "accept",
                                        enabled ? _("Leave On") : _("Leave Off"),
                                        (NotifyActionCallback) on_sticky_keys_action,
                                        manager,
                                        NULL);

        g_signal_connect (manager->priv->notification,
                          "closed",
                          G_CALLBACK (on_notification_closed),
                          manager);

        error = NULL;
        res = notify_notification_show (manager->priv->notification, &error);
        if (! res) {
                g_warning ("CsdA11yKeyboardManager: unable to show notification: %s", error->message);
                g_error_free (error);
                notify_notification_close (manager->priv->notification, NULL);
        }

        return res;
#endif /* 1 */
}

static void
ax_stickykeys_warning_post_dialog (CsdA11yKeyboardManager *manager,
                                   gboolean                enabled)
{
        const char *title;
        const char *message;

        title = enabled ?
                _("Sticky Keys Turned On") :
                _("Sticky Keys Turned Off");
        message = enabled ?
                _("You just pressed the Shift key 5 times in a row.  This is the shortcut "
                  "for the Sticky Keys feature, which affects the way your keyboard works.") :
                _("You just pressed two keys at once, or pressed the Shift key 5 times in a row.  "
                  "This turns off the Sticky Keys feature, which affects the way your keyboard works.");

        if (manager->priv->stickykeys_alert != NULL) {
                gtk_widget_show (manager->priv->stickykeys_alert);
                return;
        }

        manager->priv->stickykeys_alert = gtk_message_dialog_new (NULL,
                                                                  0,
                                                                  GTK_MESSAGE_WARNING,
                                                                  GTK_BUTTONS_NONE,
                                                                  "%s", title);

        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (manager->priv->stickykeys_alert),
                                                  "%s", message);

        gtk_dialog_add_button (GTK_DIALOG (manager->priv->stickykeys_alert),
                               GTK_STOCK_HELP,
                               GTK_RESPONSE_HELP);
        gtk_dialog_add_button (GTK_DIALOG (manager->priv->stickykeys_alert),
                               enabled ? _("_Turn Off") : _("_Turn On"),
                               GTK_RESPONSE_REJECT);
        gtk_dialog_add_button (GTK_DIALOG (manager->priv->stickykeys_alert),
                               enabled ? _("_Leave On") : _("_Leave Off"),
                               GTK_RESPONSE_ACCEPT);

        gtk_window_set_title (GTK_WINDOW (manager->priv->stickykeys_alert), "");
        gtk_window_set_icon_name (GTK_WINDOW (manager->priv->stickykeys_alert),
                                  "preferences-desktop-accessibility");
        gtk_dialog_set_default_response (GTK_DIALOG (manager->priv->stickykeys_alert),
                                         GTK_RESPONSE_ACCEPT);

        g_signal_connect (manager->priv->stickykeys_alert,
                          "response",
                          G_CALLBACK (ax_stickykeys_response),
                          manager);
        gtk_widget_show (manager->priv->stickykeys_alert);

        g_object_add_weak_pointer (G_OBJECT (manager->priv->stickykeys_alert),
                                   (gpointer*) &manager->priv->stickykeys_alert);
}

static void
ax_stickykeys_warning_post (CsdA11yKeyboardManager *manager,
                            gboolean                enabled)
{

        manager->priv->stickykeys_shortcut_val = enabled;

        /* alway try to show something */
        if (! ax_stickykeys_warning_post_bubble (manager, enabled)) {
                ax_stickykeys_warning_post_dialog (manager, enabled);
        }
}

static void
set_gsettings_from_server (CsdA11yKeyboardManager *manager)
{
        XkbDescRec     *desc;
        gboolean        changed = FALSE;
        gboolean        slowkeys_changed;
        gboolean        stickykeys_changed;
        GSettings      *settings;

        desc = get_xkb_desc_rec (manager);
        if (! desc) {
                return;
        }

	/* Create a new one, so that only those settings
	 * are delayed */
        settings = g_settings_new (KEYBOARD_A11Y_SCHEMA);
        g_settings_delay (settings);

        /*
          fprintf (stderr, "changed to : 0x%x\n", desc->ctrls->enabled_ctrls);
          fprintf (stderr, "changed to : 0x%x (2)\n", desc->ctrls->ax_options);
        */

        changed |= set_bool (settings,
                             "enable",
                             desc->ctrls->enabled_ctrls & XkbAccessXKeysMask);

        changed |= set_bool (settings,
                             "feature-state-change-beep",
                             desc->ctrls->ax_options & (XkbAX_FeatureFBMask | XkbAX_SlowWarnFBMask));
        changed |= set_bool (settings,
                             "timeout-enable",
                             desc->ctrls->enabled_ctrls & XkbAccessXTimeoutMask);
        changed |= set_int (settings,
                            "disable-timeout",
                            desc->ctrls->ax_timeout);

        changed |= set_bool (settings,
                             "bouncekeys-enable",
                             desc->ctrls->enabled_ctrls & XkbBounceKeysMask);
        changed |= set_int (settings,
                            "bouncekeys-delay",
                            desc->ctrls->debounce_delay);
        changed |= set_bool (settings,
                             "bouncekeys-beep-reject",
                             desc->ctrls->ax_options & XkbAX_BKRejectFBMask);

        changed |= set_bool (settings,
                             "mousekeys-enable",
                             desc->ctrls->enabled_ctrls & XkbMouseKeysMask);
        changed |= set_int (settings,
                            "mousekeys-max-speed",
                            desc->ctrls->mk_max_speed * (1000 / desc->ctrls->mk_interval));
        /* NOTE : mk_time_to_max is measured in events not time */
        changed |= set_int (settings,
                            "mousekeys-accel-time",
                            desc->ctrls->mk_time_to_max * desc->ctrls->mk_interval);
        changed |= set_int (settings,
                            "mousekeys-init-delay",
                            desc->ctrls->mk_delay);

        slowkeys_changed = set_bool (settings,
                                     "slowkeys-enable",
                                     desc->ctrls->enabled_ctrls & XkbSlowKeysMask);
        changed |= set_bool (settings,
                             "slowkeys-beep-press",
                             desc->ctrls->ax_options & XkbAX_SKPressFBMask);
        changed |= set_bool (settings,
                             "slowkeys-beep-accept",
                             desc->ctrls->ax_options & XkbAX_SKAcceptFBMask);
        changed |= set_bool (settings,
                             "slowkeys-beep-reject",
                             desc->ctrls->ax_options & XkbAX_SKRejectFBMask);
        changed |= set_int (settings,
                            "slowkeys-delay",
                            desc->ctrls->slow_keys_delay);

        stickykeys_changed = set_bool (settings,
                                       "stickykeys-enable",
                                       desc->ctrls->enabled_ctrls & XkbStickyKeysMask);
        changed |= set_bool (settings,
                             "stickykeys-two-key-off",
                             desc->ctrls->ax_options & XkbAX_TwoKeysMask);
        changed |= set_bool (settings,
                             "stickykeys-modifier-beep",
                             desc->ctrls->ax_options & XkbAX_StickyKeysFBMask);

        if (!changed && stickykeys_changed ^ slowkeys_changed) {
                /*
                 * sticky or slowkeys has changed, singly, without our intervention.
                 * 99% chance this is due to a keyboard shortcut being used.
                 * we need to detect via this hack until we get
                 *  XkbAXN_AXKWarning notifications working (probable XKB bug),
                 *  at which time we can directly intercept such shortcuts instead.
                 * See cb_xkb_event_filter () below.
                 */

                /* sanity check: are keyboard shortcuts available? */
                if (desc->ctrls->enabled_ctrls & XkbAccessXKeysMask) {
                        if (slowkeys_changed) {
                                ax_slowkeys_warning_post (manager,
                                                          desc->ctrls->enabled_ctrls & XkbSlowKeysMask);
                        } else {
                                ax_stickykeys_warning_post (manager,
                                                            desc->ctrls->enabled_ctrls & XkbStickyKeysMask);
                        }
                }
        }

        XkbFreeKeyboard (desc, XkbAllComponentsMask, True);

        g_settings_apply (settings);
        g_object_unref (settings);
}

static GdkFilterReturn
cb_xkb_event_filter (GdkXEvent              *xevent,
                     GdkEvent               *ignored1,
                     CsdA11yKeyboardManager *manager)
{
        XEvent   *xev   = (XEvent *) xevent;
        XkbEvent *xkbEv = (XkbEvent *) xevent;

        /* 'event_type' is set to zero on notifying us of updates in
         * response to client requests (including our own) and non-zero
         * to notify us of key/mouse events causing changes (like
         * pressing shift 5 times to enable sticky keys).
         *
         * We only want to update GSettings when it's in response to an
         * explicit user input event, so require a non-zero event_type.
         */
        if (xev->xany.type == (manager->priv->xkbEventBase + XkbEventCode) &&
            xkbEv->any.xkb_type == XkbControlsNotify &&
            xkbEv->ctrls.event_type != 0) {
                g_debug ("XKB state changed");
                set_gsettings_from_server (manager);
        } else if (xev->xany.type == (manager->priv->xkbEventBase + XkbEventCode) &&
                   xkbEv->any.xkb_type == XkbAccessXNotify) {
                if (xkbEv->accessx.detail == XkbAXN_AXKWarning) {
                        g_debug ("About to turn on an AccessX feature from the keyboard!");
                        /*
                         * TODO: when XkbAXN_AXKWarnings start working, we need to
                         * invoke ax_keys_warning_dialog_run here instead of in
                         * set_gsettings_from_server().
                         */
                }
        }

        return GDK_FILTER_CONTINUE;
}

static void
keyboard_callback (GSettings              *settings,
                   const char             *key,
                   CsdA11yKeyboardManager *manager)
{
        set_server_from_gsettings (manager);
        maybe_show_status_icon (manager);
}

static gboolean
start_a11y_keyboard_idle_cb (CsdA11yKeyboardManager *manager)
{
        guint        event_mask;

        g_debug ("Starting a11y_keyboard manager");
        cinnamon_settings_profile_start (NULL);

        if (!xkb_enabled (manager))
                goto out;

        manager->priv->settings = g_settings_new (KEYBOARD_A11Y_SCHEMA);
        g_signal_connect (G_OBJECT (manager->priv->settings), "changed",
                          G_CALLBACK (keyboard_callback), manager);

        set_devicepresence_handler (manager);

        event_mask = XkbControlsNotifyMask;
        event_mask |= XkbAccessXNotifyMask; /* make default when AXN_AXKWarning works */

        /* be sure to init before starting to monitor the server */
        set_server_from_gsettings (manager);

        XkbSelectEvents (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                         XkbUseCoreKbd,
                         event_mask,
                         event_mask);

        gdk_window_add_filter (NULL,
                               (GdkFilterFunc) cb_xkb_event_filter,
                               manager);

        maybe_show_status_icon (manager);

 out:
        cinnamon_settings_profile_end (NULL);

        manager->priv->start_idle_id = 0;

        return FALSE;
}

gboolean
csd_a11y_keyboard_manager_start (CsdA11yKeyboardManager *manager,
                                 GError                **error)
{
        cinnamon_settings_profile_start (NULL);

        manager->priv->start_idle_id = g_idle_add ((GSourceFunc) start_a11y_keyboard_idle_cb, manager);

        cinnamon_settings_profile_end (NULL);

        return TRUE;
}

void
csd_a11y_keyboard_manager_stop (CsdA11yKeyboardManager *manager)
{
        CsdA11yKeyboardManagerPrivate *p = manager->priv;

        g_debug ("Stopping a11y_keyboard manager");

        if (p->start_idle_id != 0) {
                g_source_remove (p->start_idle_id);
                p->start_idle_id = 0;
        }

        if (p->device_manager != NULL) {
                g_signal_handler_disconnect (p->device_manager, p->device_added_id);
                p->device_manager = NULL;
        }

        if (p->status_icon) {
                gtk_status_icon_set_visible (p->status_icon, FALSE);
                p->status_icon = NULL;
        }

        if (p->settings != NULL) {
                g_signal_handlers_disconnect_by_func (p->settings, keyboard_callback, manager);
                g_object_unref (p->settings);
                p->settings = NULL;
        }

        gdk_window_remove_filter (NULL,
                                  (GdkFilterFunc) cb_xkb_event_filter,
                                  manager);

        if (p->slowkeys_alert != NULL) {
                gtk_widget_destroy (p->slowkeys_alert);
                p->slowkeys_alert = NULL;
        }

        if (p->stickykeys_alert != NULL) {
                gtk_widget_destroy (p->stickykeys_alert);
                p->stickykeys_alert = NULL;
        }

        p->slowkeys_shortcut_val = FALSE;
        p->stickykeys_shortcut_val = FALSE;
}

static GObject *
csd_a11y_keyboard_manager_constructor (GType                  type,
                                       guint                  n_construct_properties,
                                       GObjectConstructParam *construct_properties)
{
        CsdA11yKeyboardManager      *a11y_keyboard_manager;

        a11y_keyboard_manager = CSD_A11Y_KEYBOARD_MANAGER (G_OBJECT_CLASS (csd_a11y_keyboard_manager_parent_class)->constructor (type,
                                                                                                      n_construct_properties,
                                                                                                      construct_properties));

        return G_OBJECT (a11y_keyboard_manager);
}

static void
csd_a11y_keyboard_manager_class_init (CsdA11yKeyboardManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = csd_a11y_keyboard_manager_constructor;
        object_class->finalize = csd_a11y_keyboard_manager_finalize;

        g_type_class_add_private (klass, sizeof (CsdA11yKeyboardManagerPrivate));
}

static void
on_preferences_dialog_response (GtkDialog              *dialog,
                                int                     response,
                                CsdA11yKeyboardManager *manager)
{
        g_signal_handlers_disconnect_by_func (dialog,
                                              on_preferences_dialog_response,
                                              manager);

        gtk_widget_destroy (GTK_WIDGET (dialog));
        manager->priv->preferences_dialog = NULL;
}

static void
on_status_icon_activate (GtkStatusIcon          *status_icon,
                         CsdA11yKeyboardManager *manager)
{
        if (manager->priv->preferences_dialog == NULL) {
                manager->priv->preferences_dialog = csd_a11y_preferences_dialog_new ();
                g_signal_connect (manager->priv->preferences_dialog,
                                  "response",
                                  G_CALLBACK (on_preferences_dialog_response),
                                  manager);

                gtk_window_present (GTK_WINDOW (manager->priv->preferences_dialog));
        } else {
                g_signal_handlers_disconnect_by_func (manager->priv->preferences_dialog,
                                                      on_preferences_dialog_response,
                                                      manager);
                gtk_widget_destroy (GTK_WIDGET (manager->priv->preferences_dialog));
                manager->priv->preferences_dialog = NULL;
        }
}

static void
on_status_icon_popup_menu (GtkStatusIcon *status_icon,
                           guint          button,
                           guint          activate_time,
                           CsdA11yKeyboardManager *manager)
{
        on_status_icon_activate (status_icon, manager);
}

static void
csd_a11y_keyboard_manager_ensure_status_icon (CsdA11yKeyboardManager *manager)
{
        cinnamon_settings_profile_start (NULL);

        if (!manager->priv->status_icon) {

                manager->priv->status_icon = gtk_status_icon_new_from_icon_name ("preferences-desktop-accessibility");
                gtk_status_icon_set_name (manager->priv->status_icon, "a11y-keyboard");
                g_signal_connect (manager->priv->status_icon,
                                  "activate",
                                  G_CALLBACK (on_status_icon_activate),
                                  manager);
                g_signal_connect (manager->priv->status_icon,
                                  "popup-menu",
                                  G_CALLBACK (on_status_icon_popup_menu),
                                  manager);
        }

        cinnamon_settings_profile_end (NULL);
}

static void
csd_a11y_keyboard_manager_init (CsdA11yKeyboardManager *manager)
{
        manager->priv = CSD_A11Y_KEYBOARD_MANAGER_GET_PRIVATE (manager);
}

static void
csd_a11y_keyboard_manager_finalize (GObject *object)
{
        CsdA11yKeyboardManager *a11y_keyboard_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_A11Y_KEYBOARD_MANAGER (object));

        a11y_keyboard_manager = CSD_A11Y_KEYBOARD_MANAGER (object);

        g_return_if_fail (a11y_keyboard_manager->priv != NULL);

        if (a11y_keyboard_manager->priv->start_idle_id != 0) {
                g_source_remove (a11y_keyboard_manager->priv->start_idle_id);
                a11y_keyboard_manager->priv->start_idle_id = 0;
        }

        G_OBJECT_CLASS (csd_a11y_keyboard_manager_parent_class)->finalize (object);
}

CsdA11yKeyboardManager *
csd_a11y_keyboard_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CSD_TYPE_A11Y_KEYBOARD_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return CSD_A11Y_KEYBOARD_MANAGER (manager_object);
}

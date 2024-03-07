/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Ritesh Khadgaray <khadgaray@gmail.com>
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

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libupower-glib/upower.h>
#include <libnotify/notify.h>
#include <canberra-gtk.h>
#include <gio/gunixfdlist.h>

#include <X11/extensions/dpms.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libcinnamon-desktop/gnome-rr.h>

#include "gpm-common.h"
#include "gpm-phone.h"
#include "gpm-idletime.h"
#include "cinnamon-settings-profile.h"
#include "cinnamon-settings-session.h"
#include "csd-enums.h"
#include "csd-power-manager.h"
#include "csd-power-helper.h"
#include "csd-power-proxy.h"
#include "csd-power-screen-proxy.h"
#include "csd-power-keyboard-proxy.h"

#define GNOME_SESSION_DBUS_NAME                 "org.gnome.SessionManager"
#define GNOME_SESSION_DBUS_PATH                 "/org/gnome/SessionManager"
#define GNOME_SESSION_DBUS_PATH_PRESENCE        "/org/gnome/SessionManager/Presence"
#define GNOME_SESSION_DBUS_INTERFACE            "org.gnome.SessionManager"
#define GNOME_SESSION_DBUS_INTERFACE_PRESENCE   "org.gnome.SessionManager.Presence"

#define UPOWER_DBUS_NAME                        "org.freedesktop.UPower"
#define UPOWER_DBUS_PATH_KBDBACKLIGHT           "/org/freedesktop/UPower/KbdBacklight"
#define UPOWER_DBUS_INTERFACE_KBDBACKLIGHT      "org.freedesktop.UPower.KbdBacklight"

#define CSD_POWER_SETTINGS_SCHEMA               "org.cinnamon.settings-daemon.plugins.power"
#define CSD_XRANDR_SETTINGS_SCHEMA              "org.cinnamon.settings-daemon.plugins.xrandr"
#define CSD_SAVER_SETTINGS_SCHEMA               "org.cinnamon.desktop.screensaver"
#define CSD_SESSION_SETTINGS_SCHEMA             "org.cinnamon.desktop.session"
#define CSD_CINNAMON_SESSION_SCHEMA             "org.cinnamon.SessionManager"

#define CSD_POWER_DBUS_PATH                     "/org/cinnamon/SettingsDaemon/Power"
#define CSD_POWER_DBUS_INTERFACE                "org.cinnamon.SettingsDaemon.Power"
#define CSD_POWER_DBUS_INTERFACE_SCREEN         "org.cinnamon.SettingsDaemon.Power.Screen"
#define CSD_POWER_DBUS_INTERFACE_KEYBOARD       "org.cinnamon.SettingsDaemon.Power.Keyboard"

#define GS_DBUS_NAME                            "org.cinnamon.ScreenSaver"
#define GS_DBUS_PATH                            "/org/cinnamon/ScreenSaver"
#define GS_DBUS_INTERFACE                       "org.cinnamon.ScreenSaver"

#define CSD_POWER_MANAGER_NOTIFY_TIMEOUT_NEVER          0 /* ms */
#define CSD_POWER_MANAGER_NOTIFY_TIMEOUT_SHORT          10 * 1000 /* ms */
#define CSD_POWER_MANAGER_NOTIFY_TIMEOUT_LONG           30 * 1000 /* ms */

#define CSD_POWER_MANAGER_CRITICAL_ALERT_TIMEOUT        5 /* seconds */
#define CSD_POWER_MANAGER_LID_CLOSE_SAFETY_TIMEOUT      30 /* seconds */

#define LOGIND_DBUS_NAME                       "org.freedesktop.login1"
#define LOGIND_DBUS_PATH                       "/org/freedesktop/login1"
#define LOGIND_DBUS_INTERFACE                  "org.freedesktop.login1.Manager"

/* Keep this in sync with gnome-shell */
#define SCREENSAVER_FADE_TIME                           10 /* seconds */

#define XSCREENSAVER_WATCHDOG_TIMEOUT                   120 /* seconds */

enum {
        CSD_POWER_IDLETIME_NULL_ID,
        CSD_POWER_IDLETIME_DIM_ID,
        CSD_POWER_IDLETIME_LOCK_ID,
        CSD_POWER_IDLETIME_BLANK_ID,
        CSD_POWER_IDLETIME_SLEEP_ID
};

/* on ACPI machines we have 4-16 levels, on others it's ~150 */
#define BRIGHTNESS_STEP_AMOUNT(max) ((max) < 20 ? 1 : (max) / 20)

/* take a discrete value with offset and convert to percentage */
static int
abs_to_percentage (int min, int max, int value)
{
        g_return_val_if_fail (max > min, -1);
        g_return_val_if_fail (value >= min, -1);
        g_return_val_if_fail (value <= max, -1);
        return (CLAMP(((value - min) * 100) / (max - min), 0, 100));
}
#define ABS_TO_PERCENTAGE(min, max, value) abs_to_percentage(min, max, value)
#define PERCENTAGE_TO_ABS(min, max, value) (min + (((max - min) * value) / 100))

#define CSD_POWER_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CSD_TYPE_POWER_MANAGER, CsdPowerManagerPrivate))

typedef enum {
        CSD_POWER_IDLE_MODE_NORMAL,
        CSD_POWER_IDLE_MODE_DIM,
        CSD_POWER_IDLE_MODE_BLANK,
        CSD_POWER_IDLE_MODE_SLEEP
} CsdPowerIdleMode;

struct CsdPowerManagerPrivate
{
        CinnamonSettingsSession    *session;
        guint                    p_name_id;
        guint                    s_name_id;
        guint                    k_name_id;
        CsdPower                 *power_iface;
        CsdScreen                *screen_iface;
        CsdKeyboard              *keyboard_iface;
        gboolean                 lid_is_closed;
        gboolean                 on_battery;
        GSettings               *settings;
        GSettings               *settings_screensaver;
        GSettings               *settings_xrandr;
        GSettings               *settings_desktop_session;
        GSettings               *settings_cinnamon_session;
        UpClient                *up_client;
        GDBusConnection         *connection;
        GCancellable            *bus_cancellable;
        GDBusProxy              *upower_kbd_proxy;
        gboolean				backlight_helper_force;
        gchar*                  backlight_helper_preference_args;
        gint                     kbd_brightness_now;
        gint                     kbd_brightness_max;
        gint                     kbd_brightness_old;
        gint                     kbd_brightness_pre_dim;
        GnomeRRScreen           *x11_screen;
        gboolean                 use_time_primary;
        gchar                   *previous_summary;
        GIcon                   *previous_icon;
        GpmPhone                *phone;
        GPtrArray               *devices_array;
        guint                    action_percentage;
        guint                    action_time;
        guint                    critical_percentage;
        guint                    critical_time;
        guint                    low_percentage;
        guint                    low_time;
        gboolean                 notify_keyboard;
        gboolean                 notify_mouse;
        gboolean                 notify_other_devices;
        gint                     pre_dim_brightness; /* level, not percentage */
        UpDevice                *device_composite;
        NotifyNotification      *notification_discharging;
        NotifyNotification      *notification_low;
        ca_context              *canberra_context;
        ca_proplist             *critical_alert_loop_props;
        guint32                  critical_alert_timeout_id;
        GDBusProxy              *screensaver_proxy;
        GDBusProxy              *session_proxy;
        GDBusProxy              *session_presence_proxy;
        GpmIdletime             *idletime;
        CsdPowerIdleMode         current_idle_mode;
        guint                    lid_close_safety_timer_id;
        GtkStatusIcon           *status_icon;
        guint                    xscreensaver_watchdog_timer_id;
        gboolean                 is_virtual_machine;
        gint                     fd_close_loop_end;

        /* logind stuff */
        GDBusProxy              *logind_proxy;
        gboolean                 inhibit_lid_switch_enabled;
        gint                     inhibit_lid_switch_fd;
        gboolean                 inhibit_lid_switch_taken;
        gint                     inhibit_suspend_fd;
        gboolean                 inhibit_suspend_taken;
        guint                    inhibit_lid_switch_timer_id;
};

enum {
        PROP_0,
};

static void     csd_power_manager_finalize    (GObject              *object);

static UpDevice *engine_get_composite_device (CsdPowerManager *manager, UpDevice *original_device);
static UpDevice *engine_update_composite_device (CsdPowerManager *manager, UpDevice *original_device);
static GIcon    *engine_get_icon (CsdPowerManager *manager);
static gchar    *engine_get_summary (CsdPowerManager *manager);
static UpDevice *engine_get_primary_device (CsdPowerManager *manager);
static void      engine_charge_low (CsdPowerManager *manager, UpDevice *device);
static void      engine_charge_critical (CsdPowerManager *manager, UpDevice *device);
static void      engine_charge_action (CsdPowerManager *manager, UpDevice *device);

static gboolean  external_monitor_is_connected (GnomeRRScreen *screen);
static void      do_power_action_type (CsdPowerManager *manager, CsdPowerActionType action_type);
static void      do_lid_closed_action (CsdPowerManager *manager);
static void      inhibit_lid_switch (CsdPowerManager *manager);
static void      uninhibit_lid_switch (CsdPowerManager *manager);
static void      setup_locker_process (gpointer user_data);
static void      lock_screen_with_custom_saver (CsdPowerManager *manager, gchar *custom_saver, gboolean idle_lock);
static void      activate_screensaver (CsdPowerManager *manager, gboolean force_lock);
static void      kill_lid_close_safety_timer (CsdPowerManager *manager);

static void      backlight_get_output_id (CsdPowerManager *manager, gint *xout, gint *yout);

#if UP_CHECK_VERSION(0,99,0)
static void device_properties_changed_cb (UpDevice *device, GParamSpec *pspec, CsdPowerManager *manager);
#endif

G_DEFINE_TYPE (CsdPowerManager, csd_power_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

GQuark
csd_power_manager_error_quark (void)
{
        static GQuark quark = 0;
        if (!quark)
                quark = g_quark_from_static_string ("csd_power_manager_error");
        return quark;
}

static gboolean
system_on_battery (CsdPowerManager *manager)
{
    UpDevice *primary;
    UpDeviceState state;

    // this will only return a device if it's the battery, it's present,
    // and discharging.
    primary = engine_get_primary_device (manager);

    return primary != NULL;
}

static gboolean
play_loop_timeout_cb (CsdPowerManager *manager)
{
        ca_context *context;
        context = ca_gtk_context_get_for_screen (gdk_screen_get_default ());
        ca_context_play_full (context, 0,
                              manager->priv->critical_alert_loop_props,
                              NULL,
                              NULL);
        return TRUE;
}

static gboolean
play_loop_stop (CsdPowerManager *manager)
{
        if (manager->priv->critical_alert_timeout_id == 0) {
                g_warning ("no sound loop present to stop");
                return FALSE;
        }

        if (manager->priv->critical_alert_timeout_id) {
            g_source_remove (manager->priv->critical_alert_timeout_id);
            manager->priv->critical_alert_timeout_id = 0;
        }
        
        ca_proplist_destroy (manager->priv->critical_alert_loop_props);

        manager->priv->critical_alert_loop_props = NULL;
        manager->priv->critical_alert_timeout_id = 0;

        return TRUE;
}

static gboolean
play_loop_start (CsdPowerManager *manager,
                 const gchar *id,
                 const gchar *desc,
                 gboolean force,
                 guint timeout)
{
        ca_context *context;

        if (timeout == 0) {
                g_warning ("received invalid timeout");
                return FALSE;
        }

        /* if a sound loop is already running, stop the existing loop */
        if (manager->priv->critical_alert_timeout_id != 0) {
                g_warning ("was instructed to play a sound loop with one already playing");
                play_loop_stop (manager);
        }

        ca_proplist_create (&(manager->priv->critical_alert_loop_props));
        ca_proplist_sets (manager->priv->critical_alert_loop_props,
                          CA_PROP_EVENT_ID, id);
        ca_proplist_sets (manager->priv->critical_alert_loop_props,
                          CA_PROP_EVENT_DESCRIPTION, desc);

        manager->priv->critical_alert_timeout_id = g_timeout_add_seconds (timeout,
                                                                          (GSourceFunc) play_loop_timeout_cb,
                                                                          manager);
        g_source_set_name_by_id (manager->priv->critical_alert_timeout_id,
                                 "[CsdPowerManager] play-loop");

        /* play the sound, using sounds from the naming spec */
        context = ca_gtk_context_get_for_screen (gdk_screen_get_default ());
        ca_context_play (context, 0,
                         CA_PROP_EVENT_ID, id,
                         CA_PROP_EVENT_DESCRIPTION, desc, NULL);
        return TRUE;
}

static gboolean
should_lock_on_suspend (CsdPowerManager *manager)
{
    gboolean lock;

    lock = g_settings_get_boolean (manager->priv->settings,
                                   "lock-on-suspend");

    return lock;
}

static void
notify_close_if_showing (NotifyNotification *notification)
{
        gboolean ret;
        GError *error = NULL;

        if (notification == NULL)
                return;
        ret = notify_notification_close (notification, &error);
        if (!ret) {
                g_warning ("failed to close notification: %s",
                           error->message);
                g_error_free (error);
        }
}

static const gchar *
get_first_themed_icon_name (GIcon *icon)
{
        const gchar* const *icon_names;
        const gchar *icon_name = NULL;

        /* no icon */
        if (icon == NULL)
                goto out;

        /* just use the first icon */
        icon_names = g_themed_icon_get_names (G_THEMED_ICON (icon));
        if (icon_names != NULL)
                icon_name = icon_names[0];
out:
        return icon_name;
}

typedef enum {
        WARNING_NONE            = 0,
        WARNING_DISCHARGING     = 1,
        WARNING_LOW             = 2,
        WARNING_CRITICAL        = 3,
        WARNING_ACTION          = 4
} CsdPowerManagerWarning;

static void
engine_emit_changed (CsdPowerManager *manager,
                     gboolean         icon_changed,
                     gboolean         state_changed)
{
        /* not yet connected to the bus */
        if (manager->priv->power_iface == NULL)
                return;

        gboolean need_flush = FALSE;

        if (icon_changed) {
                GIcon *gicon;
                gchar *gicon_str;

                gicon = engine_get_icon (manager);
                gicon_str = g_icon_to_string (gicon);

                csd_power_set_icon (manager->priv->power_iface, gicon_str);
                need_flush = TRUE;

                g_free (gicon_str);
                g_object_unref (gicon);
        }

        if (state_changed) {
                gchar *tooltip;

                tooltip = engine_get_summary (manager);

                csd_power_set_tooltip (manager->priv->power_iface, tooltip);
                need_flush = TRUE;

                g_free (tooltip);
        }

        if (need_flush) {
                g_dbus_interface_skeleton_flush (G_DBUS_INTERFACE_SKELETON (manager->priv->power_iface));
        }
}

static CsdPowerManagerWarning
engine_get_warning_csr (CsdPowerManager *manager, UpDevice *device)
{
        gdouble percentage;

        /* get device properties */
        g_object_get (device, "percentage", &percentage, NULL);

        if (percentage < 26.0f)
                return WARNING_LOW;
        else if (percentage < 13.0f)
                return WARNING_CRITICAL;
        return WARNING_NONE;
}

static CsdPowerManagerWarning
engine_get_warning_percentage (CsdPowerManager *manager, UpDevice *device)
{
        gdouble percentage;

        /* get device properties */
        g_object_get (device, "percentage", &percentage, NULL);

        if (percentage <= manager->priv->action_percentage)
                return WARNING_ACTION;
        if (percentage <= manager->priv->critical_percentage)
                return WARNING_CRITICAL;
        if (percentage <= manager->priv->low_percentage)
                return WARNING_LOW;
        return WARNING_NONE;
}

static CsdPowerManagerWarning
engine_get_warning_time (CsdPowerManager *manager, UpDevice *device)
{
        UpDeviceKind kind;
        gint64 time_to_empty;

        /* get device properties */
        g_object_get (device,
                      "kind", &kind,
                      "time-to-empty", &time_to_empty,
                      NULL);

        /* this is probably an error condition */
        if (time_to_empty == 0) {
                g_debug ("time zero, falling back to percentage for %s",
                         up_device_kind_to_string (kind));
                return engine_get_warning_percentage (manager, device);
        }

        if (time_to_empty <= manager->priv->action_time)
                return WARNING_ACTION;
        if (time_to_empty <= manager->priv->critical_time)
                return WARNING_CRITICAL;
        if (time_to_empty <= manager->priv->low_time)
                return WARNING_LOW;
        return WARNING_NONE;
}

/**
 * This gets the possible engine state for the device according to the
 * policy, which could be per-percent, or per-time.
 **/
static CsdPowerManagerWarning
engine_get_warning (CsdPowerManager *manager, UpDevice *device)
{
        UpDeviceKind kind;
        UpDeviceState state;
        CsdPowerManagerWarning warning_type;

        /* get device properties */
        g_object_get (device,
                      "kind", &kind,
                      "state", &state,
                      NULL);

        /* default to no engine */
        warning_type = WARNING_NONE;

        /* if the device in question is on ac, don't give a warning */
        if (state == UP_DEVICE_STATE_CHARGING)
                goto out;

        /* filter out unwanted warnings */
        if (kind == UP_DEVICE_KIND_KEYBOARD) {

                if (!manager->priv->notify_keyboard)
                        goto out;

        } else if (kind == UP_DEVICE_KIND_MOUSE) {

                if (!manager->priv->notify_mouse)
                        goto out;

        } else if (kind != UP_DEVICE_KIND_BATTERY &&
                   kind != UP_DEVICE_KIND_UPS) {

                if (!manager->priv->notify_other_devices)
                        goto out;

        }

        if (kind == UP_DEVICE_KIND_MOUSE ||
            kind == UP_DEVICE_KIND_KEYBOARD) {

                warning_type = engine_get_warning_csr (manager, device);

        } else if (kind == UP_DEVICE_KIND_UPS ||
                   kind == UP_DEVICE_KIND_MEDIA_PLAYER ||
                   kind == UP_DEVICE_KIND_TABLET ||
                   kind == UP_DEVICE_KIND_COMPUTER ||
                   kind == UP_DEVICE_KIND_PDA) {

                warning_type = engine_get_warning_percentage (manager, device);

        } else if (kind == UP_DEVICE_KIND_PHONE) {

                warning_type = engine_get_warning_percentage (manager, device);

        } else if (kind == UP_DEVICE_KIND_BATTERY) {
                /* only use the time when it is accurate, and settings is not disabled */
                if (manager->priv->use_time_primary)
                        warning_type = engine_get_warning_time (manager, device);
                else
                        warning_type = engine_get_warning_percentage (manager, device);
        }

        /* If we have no important engines, we should test for discharging */
        if (warning_type == WARNING_NONE) {
                if (state == UP_DEVICE_STATE_DISCHARGING)
                        warning_type = WARNING_DISCHARGING;
        }

 out:
        return warning_type;
}

static gchar *
engine_get_summary (CsdPowerManager *manager)
{
        guint i;
        GPtrArray *array;
        UpDevice *device;
        UpDeviceState state;
        GString *tooltip = NULL;
        gchar *part;
        gboolean is_present;


        /* need to get AC state */
        tooltip = g_string_new ("");

        /* do we have specific device types? */
        array = manager->priv->devices_array;
        for (i=0;i<array->len;i++) {
                device = g_ptr_array_index (array, i);
                g_object_get (device,
                              "is-present", &is_present,
                              "state", &state,
                              NULL);
                if (!is_present)
                        continue;
                if (state == UP_DEVICE_STATE_EMPTY)
                        continue;
                part = gpm_upower_get_device_summary (device);
                if (part != NULL)
                        g_string_append_printf (tooltip, "%s\n", part);
                g_free (part);
        }

        /* remove the last \n */
        g_string_truncate (tooltip, tooltip->len-1);

        g_debug ("tooltip: %s", tooltip->str);

        return g_string_free (tooltip, FALSE);
}

static GIcon *
engine_get_icon_priv (CsdPowerManager *manager,
                      UpDeviceKind device_kind,
                      CsdPowerManagerWarning warning,
                      gboolean use_state)
{
        guint i;
        GPtrArray *array;
        UpDevice *device;
        CsdPowerManagerWarning warning_temp;
        UpDeviceKind kind;
        UpDeviceState state;
        gboolean is_present;

        /* do we have specific device types? */
        array = manager->priv->devices_array;
        for (i=0;i<array->len;i++) {
                device = g_ptr_array_index (array, i);

                /* get device properties */
                g_object_get (device,
                              "kind", &kind,
                              "state", &state,
                              "is-present", &is_present,
                              NULL);

                /* if battery then use composite device to cope with multiple batteries */
                if (kind == UP_DEVICE_KIND_BATTERY)
                        device = engine_get_composite_device (manager, device);

                warning_temp = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(device),
                                                                  "engine-warning-old"));
                if (kind == device_kind && is_present) {
                        if (warning != WARNING_NONE) {
                                if (warning_temp == warning)
                                        return gpm_upower_get_device_icon (device, TRUE);
                                continue;
                        }
                        if (use_state) {
                                if (state == UP_DEVICE_STATE_CHARGING ||
                                    state == UP_DEVICE_STATE_DISCHARGING)
                                        return gpm_upower_get_device_icon (device, TRUE);
                                continue;
                        }
                        return gpm_upower_get_device_icon (device, TRUE);
                }
        }
        return NULL;
}

static GIcon *
engine_get_icon (CsdPowerManager *manager)
{
        GIcon *icon = NULL;


        /* we try CRITICAL: BATTERY, UPS, MOUSE, KEYBOARD */
        icon = engine_get_icon_priv (manager, UP_DEVICE_KIND_BATTERY, WARNING_CRITICAL, FALSE);
        if (icon != NULL)
                return icon;
        icon = engine_get_icon_priv (manager, UP_DEVICE_KIND_UPS, WARNING_CRITICAL, FALSE);
        if (icon != NULL)
                return icon;
        icon = engine_get_icon_priv (manager, UP_DEVICE_KIND_MOUSE, WARNING_CRITICAL, FALSE);
        if (icon != NULL)
                return icon;
        icon = engine_get_icon_priv (manager, UP_DEVICE_KIND_KEYBOARD, WARNING_CRITICAL, FALSE);
        if (icon != NULL)
                return icon;

        /* we try CRITICAL: BATTERY, UPS, MOUSE, KEYBOARD */
        icon = engine_get_icon_priv (manager, UP_DEVICE_KIND_BATTERY, WARNING_LOW, FALSE);
        if (icon != NULL)
                return icon;
        icon = engine_get_icon_priv (manager, UP_DEVICE_KIND_UPS, WARNING_LOW, FALSE);
        if (icon != NULL)
                return icon;
        icon = engine_get_icon_priv (manager, UP_DEVICE_KIND_MOUSE, WARNING_LOW, FALSE);
        if (icon != NULL)
                return icon;
        icon = engine_get_icon_priv (manager, UP_DEVICE_KIND_KEYBOARD, WARNING_LOW, FALSE);
        if (icon != NULL)
                return icon;

        /* we try (DIS)CHARGING: BATTERY, UPS */
        icon = engine_get_icon_priv (manager, UP_DEVICE_KIND_BATTERY, WARNING_NONE, TRUE);
        if (icon != NULL)
                return icon;
        icon = engine_get_icon_priv (manager, UP_DEVICE_KIND_UPS, WARNING_NONE, TRUE);
        if (icon != NULL)
                return icon;

        /* we try PRESENT: BATTERY, UPS */
        icon = engine_get_icon_priv (manager, UP_DEVICE_KIND_BATTERY, WARNING_NONE, FALSE);
        if (icon != NULL)
                return icon;
        icon = engine_get_icon_priv (manager, UP_DEVICE_KIND_UPS, WARNING_NONE, FALSE);
        if (icon != NULL)
                return icon;

        /* do not show an icon */
        return NULL;
}

static gboolean
engine_recalculate_state_icon (CsdPowerManager *manager)
{
        GIcon *icon;

        /* show a different icon if we are disconnected */
        icon = engine_get_icon (manager);
        gtk_status_icon_set_visible (manager->priv->status_icon, FALSE);

        if (icon == NULL) {
                /* none before, now none */
                if (manager->priv->previous_icon == NULL)
                        return FALSE;

                g_object_unref (manager->priv->previous_icon);
                manager->priv->previous_icon = NULL;
                return TRUE;
        }

        /* no icon before, now icon */
        if (manager->priv->previous_icon == NULL) {

                /* set fallback icon */
                gtk_status_icon_set_visible (manager->priv->status_icon, FALSE);
                gtk_status_icon_set_from_gicon (manager->priv->status_icon, icon);
                manager->priv->previous_icon = icon;
                return TRUE;
        }

        /* icon before, now different */
        if (!g_icon_equal (manager->priv->previous_icon, icon)) {

                /* set fallback icon */
                gtk_status_icon_set_from_gicon (manager->priv->status_icon, icon);

                g_object_unref (manager->priv->previous_icon);
                manager->priv->previous_icon = icon;
                return TRUE;
        }

        g_debug ("no change");
        /* nothing to do */
        g_object_unref (icon);
        return FALSE;
}

static gboolean
engine_recalculate_state_summary (CsdPowerManager *manager)
{
        gchar *summary;

        summary = engine_get_summary (manager);
        if (manager->priv->previous_summary == NULL) {
                manager->priv->previous_summary = summary;

                /* set fallback tooltip */
                gtk_status_icon_set_tooltip_text (manager->priv->status_icon,
                                                  summary);

                return TRUE;
        }

        if (strcmp (manager->priv->previous_summary, summary) != 0) {
                g_free (manager->priv->previous_summary);
                manager->priv->previous_summary = summary;

                /* set fallback tooltip */
                gtk_status_icon_set_tooltip_text (manager->priv->status_icon,
                                                  summary);

                return TRUE;
        }
        g_debug ("no change");
        /* nothing to do */
        g_free (summary);
        return FALSE;
}

static void
engine_recalculate_state (CsdPowerManager *manager)
{
        gboolean icon_changed = FALSE;
        gboolean state_changed = FALSE;

        icon_changed = engine_recalculate_state_icon (manager);
        state_changed = engine_recalculate_state_summary (manager);

        /* only emit if the icon or summary has changed */
        if (icon_changed || state_changed)
                engine_emit_changed (manager, icon_changed, state_changed);
}

static UpDevice *
engine_get_composite_device (CsdPowerManager *manager,
                             UpDevice *original_device)
{
        guint battery_devices = 0;
        GPtrArray *array;
        UpDevice *device;
        UpDeviceKind kind;
        UpDeviceKind original_kind;
        guint i;

        /* get the type of the original device */
        g_object_get (original_device,
                      "kind", &original_kind,
                      NULL);

        /* find out how many batteries in the system */
        array = manager->priv->devices_array;
        for (i=0;i<array->len;i++) {
                device = g_ptr_array_index (array, i);
                g_object_get (device,
                              "kind", &kind,
                              NULL);
                if (kind == original_kind)
                        battery_devices++;
        }

        /* just use the original device if only one primary battery */
        if (battery_devices <= 1) {
                g_debug ("using original device as only one primary battery");
                device = original_device;
                goto out;
        }

        /* use the composite device */
        device = manager->priv->device_composite;
out:
        /* return composite device or original device */
        return device;
}

static UpDevice *
engine_update_composite_device (CsdPowerManager *manager,
                                UpDevice *original_device)
{
        guint i;
        gdouble percentage = 0.0;
        gdouble energy = 0.0;
        gdouble energy_full = 0.0;
        gdouble energy_rate = 0.0;
        gdouble energy_total = 0.0;
        gdouble energy_full_total = 0.0;
        gdouble energy_rate_total = 0.0;
        gint64 time_to_empty = 0;
        gint64 time_to_full = 0;
        guint battery_devices = 0;
        gboolean is_charging = FALSE;
        gboolean is_discharging = FALSE;
        gboolean is_fully_charged = TRUE;
        GPtrArray *array;
        UpDevice *device;
        UpDeviceState state;
        UpDeviceKind kind;
        UpDeviceKind original_kind;

        /* get the type of the original device */
        g_object_get (original_device,
                      "kind", &original_kind,
                      NULL);

        /* update the composite device */
        array = manager->priv->devices_array;
        for (i=0;i<array->len;i++) {
                device = g_ptr_array_index (array, i);
                g_object_get (device,
                              "kind", &kind,
                              "state", &state,
                              "energy", &energy,
                              "energy-full", &energy_full,
                              "energy-rate", &energy_rate,
                              NULL);
                if (kind != original_kind)
                        continue;

                /* one of these will be charging or discharging */
                if (state == UP_DEVICE_STATE_CHARGING)
                        is_charging = TRUE;
                if (state == UP_DEVICE_STATE_DISCHARGING)
                        is_discharging = TRUE;
                if (state != UP_DEVICE_STATE_FULLY_CHARGED)
                        is_fully_charged = FALSE;

                /* sum up composite */
                energy_total += energy;
                energy_full_total += energy_full;
                energy_rate_total += energy_rate;
                battery_devices++;
        }

        /* just use the original device if only one primary battery */
        if (battery_devices == 1) {
                g_debug ("using original device as only one primary battery");
                device = original_device;
                goto out;
        }

        /* use percentage weighted for each battery capacity */
        if (energy_full_total > 0.0)
                percentage = 100.0 * energy_total / energy_full_total;

        /* set composite state */
        if (is_charging)
                state = UP_DEVICE_STATE_CHARGING;
        else if (is_discharging)
                state = UP_DEVICE_STATE_DISCHARGING;
        else if (is_fully_charged)
                state = UP_DEVICE_STATE_FULLY_CHARGED;
        else
                state = UP_DEVICE_STATE_UNKNOWN;

        /* calculate a quick and dirty time remaining value */
        if (energy_rate_total > 0) {
                if (state == UP_DEVICE_STATE_DISCHARGING)
                        time_to_empty = 3600 * (energy_total / energy_rate_total);
                else if (state == UP_DEVICE_STATE_CHARGING)
                        time_to_full = 3600 * ((energy_full_total - energy_total) / energy_rate_total);
        }

        /* okay, we can use the composite device */
        device = manager->priv->device_composite;

        g_debug ("printing composite device");
        g_object_set (device,
                      "energy", energy,
                      "energy-full", energy_full,
                      "energy-rate", energy_rate,
                      "time-to-empty", time_to_empty,
                      "time-to-full", time_to_full,
                      "percentage", percentage,
                      "state", state,
                      NULL);

out:
        /* force update of icon */
	engine_recalculate_state (manager);
	
        /* return composite device or original device */
        return device;
}

static void
engine_device_add (CsdPowerManager *manager, UpDevice *device)
{
        CsdPowerManagerWarning warning;
        UpDeviceState state;
        UpDeviceKind kind;
        UpDevice *composite;

        /* assign warning */
        warning = engine_get_warning (manager, device);
        g_object_set_data (G_OBJECT(device),
                           "engine-warning-old",
                           GUINT_TO_POINTER(warning));

        /* get device properties */
        g_object_get (device,
                      "kind", &kind,
                      "state", &state,
                      NULL);

        /* add old state for transitions */
        g_debug ("adding %s with state %s",
                 up_device_get_object_path (device), up_device_state_to_string (state));
        g_object_set_data (G_OBJECT(device),
                           "engine-state-old",
                           GUINT_TO_POINTER(state));

#if UP_CHECK_VERSION(0,99,0)
        g_ptr_array_add (manager->priv->devices_array, g_object_ref(device));

        g_signal_connect (device, "notify",
                          G_CALLBACK (device_properties_changed_cb), manager);
#endif

        if (kind == UP_DEVICE_KIND_BATTERY) {
                g_debug ("updating because we added a device");
                composite = engine_update_composite_device (manager, device);

                /* get the same values for the composite device */
                warning = engine_get_warning (manager, composite);

                if (warning == WARNING_LOW) {
                        g_debug ("** EMIT: charge-low");
                        engine_charge_low (manager, device);
                } else if (warning == WARNING_CRITICAL) {
                        g_debug ("** EMIT: charge-critical");
                        engine_charge_critical (manager, device);
                } else if (warning == WARNING_ACTION) {
                        g_debug ("charge-action");
                        engine_charge_action (manager, device);
                }
                g_object_set_data (G_OBJECT(composite),
                                   "engine-warning-old",
                                   GUINT_TO_POINTER(warning));
                g_object_get (composite, "state", &state, NULL);
                g_object_set_data (G_OBJECT(composite),
                                   "engine-state-old",
                                   GUINT_TO_POINTER(state));
        }
}

static gboolean
engine_coldplug (CsdPowerManager *manager)
{
        guint i;
        GPtrArray *array = NULL;
        UpDevice *device;
#if ! UP_CHECK_VERSION(0,99,0)
        gboolean ret;
        GError *error = NULL;
 
        /* get devices from UPower */
        ret = up_client_enumerate_devices_sync (manager->priv->up_client, NULL, &error);
        if (!ret) {
                g_warning ("failed to get device list: %s", error->message);
                g_error_free (error);
                goto out;
        }
#endif

        /* connected mobile phones */
        gpm_phone_coldplug (manager->priv->phone);

        engine_recalculate_state (manager);

        /* add to database */
        array = up_client_get_devices (manager->priv->up_client);
        for (i = 0; array != NULL && i < array->len; i++) {
                device = g_ptr_array_index (array, i);
                engine_device_add (manager, device);
        }
#if ! UP_CHECK_VERSION(0,99,0)
out:
#endif
        if (array != NULL)
                g_ptr_array_unref (array);
        /* never repeat */
        return FALSE;
}

static void
engine_device_added_cb (UpClient *client, UpDevice *device, CsdPowerManager *manager)
{
        /* add to list */
        g_ptr_array_add (manager->priv->devices_array, g_object_ref (device));
        engine_recalculate_state (manager);
}

static void
#if UP_CHECK_VERSION(0,99,0)
engine_device_removed_cb (UpClient *client, const char *object_path, CsdPowerManager *manager)
{
        guint i;

        for (i = 0; i < manager->priv->devices_array->len; i++) {
                UpDevice *device = g_ptr_array_index (manager->priv->devices_array, i);

                if (g_strcmp0 (object_path, up_device_get_object_path (device)) == 0) {
                        g_ptr_array_remove_index (manager->priv->devices_array, i);
                        break;
                }
        }
        engine_recalculate_state (manager);
}

#else

engine_device_removed_cb (UpClient *client, UpDevice *device, CsdPowerManager *manager)
{
        gboolean ret;
        ret = g_ptr_array_remove (manager->priv->devices_array, device);
        if (!ret)
                return;
        engine_recalculate_state (manager);
}
#endif

static void
on_notification_closed (NotifyNotification *notification, gpointer data)
{
    g_object_unref (notification);
}

static void
create_notification (const char *summary,
                     const char *body,
                     const char *icon,
                     NotifyNotification **weak_pointer_location)
{
        NotifyNotification *notification;

        notification = notify_notification_new (summary, body, icon);
        *weak_pointer_location = notification;
        g_object_add_weak_pointer (G_OBJECT (notification),
                                   (gpointer *) weak_pointer_location);
        g_signal_connect (notification, "closed",
                          G_CALLBACK (on_notification_closed), NULL);
}

static void
engine_ups_discharging (CsdPowerManager *manager, UpDevice *device)
{
        const gchar *title;
        gboolean ret;
        gchar *remaining_text = NULL;
        gdouble percentage;
        GError *error = NULL;
        GIcon *icon = NULL;
        gint64 time_to_empty;
        GString *message;
        UpDeviceKind kind;

        /* get device properties */
        g_object_get (device,
                      "kind", &kind,
                      "percentage", &percentage,
                      "time-to-empty", &time_to_empty,
                      NULL);

        if (kind != UP_DEVICE_KIND_UPS)
                return;

        /* only show text if there is a valid time */
        if (time_to_empty > 0)
                remaining_text = gpm_get_timestring (time_to_empty);

        /* TRANSLATORS: UPS is now discharging */
        title = _("UPS Discharging");

        message = g_string_new ("");
        if (remaining_text != NULL) {
                /* TRANSLATORS: tell the user how much time they have got */
                g_string_append_printf (message, _("%s of UPS backup power remaining"),
                                        remaining_text);
        } else {
                g_string_append (message, gpm_device_to_localised_string (device));
        }
        g_string_append_printf (message, " (%.0f%%)", percentage);

        icon = gpm_upower_get_device_icon (device, TRUE);

        /* close any existing notification of this class */
        notify_close_if_showing (manager->priv->notification_discharging);

        /* create a new notification */
        create_notification (title, message->str,
                             get_first_themed_icon_name (icon),
                             &manager->priv->notification_discharging);
        notify_notification_set_timeout (manager->priv->notification_discharging,
                                         CSD_POWER_MANAGER_NOTIFY_TIMEOUT_LONG);
        notify_notification_set_urgency (manager->priv->notification_discharging,
                                         NOTIFY_URGENCY_NORMAL);
        /* TRANSLATORS: this is the notification application name */
        notify_notification_set_app_name (manager->priv->notification_discharging, _("Power"));
        notify_notification_set_hint (manager->priv->notification_discharging,
                                      "transient", g_variant_new_boolean (TRUE));

        /* try to show */
        ret = notify_notification_show (manager->priv->notification_discharging,
                                        &error);
        if (!ret) {
                g_warning ("failed to show notification: %s", error->message);
                g_error_free (error);
                g_object_unref (manager->priv->notification_discharging);
        }
        g_string_free (message, TRUE);
        if (icon != NULL)
                g_object_unref (icon);
        g_free (remaining_text);
}

static CsdPowerActionType
manager_critical_action_get (CsdPowerManager *manager,
                             gboolean         is_ups)
{
        CsdPowerActionType policy;

        policy = g_settings_get_enum (manager->priv->settings, "critical-battery-action");
        if (policy == CSD_POWER_ACTION_SUSPEND) {
                if (is_ups == FALSE
#if ! UP_CHECK_VERSION(0,99,0)
                    && up_client_get_can_suspend (manager->priv->up_client)
#endif
                )
                        return policy;
                return CSD_POWER_ACTION_SHUTDOWN;
        } else if (policy == CSD_POWER_ACTION_HIBERNATE) {
#if ! UP_CHECK_VERSION(0,99,0)
                if (up_client_get_can_hibernate (manager->priv->up_client))
#endif
                        return policy;
                return CSD_POWER_ACTION_SHUTDOWN;
        }

        return policy;
}

static gboolean
manager_critical_action_do (CsdPowerManager *manager,
                            gboolean         is_ups)
{
        CsdPowerActionType action_type;

        /* stop playing the alert as it's too late to do anything now */
        if (manager->priv->critical_alert_timeout_id > 0)
                play_loop_stop (manager);

        action_type = manager_critical_action_get (manager, is_ups);
        do_power_action_type (manager, action_type);

        return FALSE;
}

static gboolean
manager_critical_action_do_cb (CsdPowerManager *manager)
{
        manager_critical_action_do (manager, FALSE);
        return FALSE;
}

static gboolean
manager_critical_ups_action_do_cb (CsdPowerManager *manager)
{
        manager_critical_action_do (manager, TRUE);
        return FALSE;
}

static gboolean
engine_just_laptop_battery (CsdPowerManager *manager)
{
        UpDevice *device;
        UpDeviceKind kind;
        GPtrArray *array;
        gboolean ret = TRUE;
        guint i;

        /* find if there are any other device types that mean we have to
         * be more specific in our wording */
        array = manager->priv->devices_array;
        for (i=0; i<array->len; i++) {
                device = g_ptr_array_index (array, i);
                g_object_get (device, "kind", &kind, NULL);
                if (kind != UP_DEVICE_KIND_BATTERY) {
                        ret = FALSE;
                        break;
                }
        }
        return ret;
}

static void
engine_charge_low (CsdPowerManager *manager, UpDevice *device)
{
        const gchar *title = NULL;
        gboolean ret;
        gchar *message = NULL;
        gchar *tmp;
        gchar *remaining_text;
        gdouble percentage;
        GIcon *icon = NULL;
        gint64 time_to_empty;
        UpDeviceKind kind;
        GError *error = NULL;

        /* get device properties */
        g_object_get (device,
                      "kind", &kind,
                      "percentage", &percentage,
                      "time-to-empty", &time_to_empty,
                      NULL);

        /* check to see if the batteries have not noticed we are on AC */
        if (kind == UP_DEVICE_KIND_BATTERY) {
                if (!system_on_battery (manager)) {
                        g_warning ("ignoring low message as we are not on battery power");
                        goto out;
                }
        }

        if (kind == UP_DEVICE_KIND_BATTERY) {

                /* if the user has no other batteries, drop the "Laptop" wording */
                ret = engine_just_laptop_battery (manager);
                if (ret) {
                        /* TRANSLATORS: laptop battery low, and we only have one battery */
                        title = _("Battery low");
                } else {
                        /* TRANSLATORS: laptop battery low, and we have more than one kind of battery */
                        title = _("Laptop battery low");
                }
                tmp = gpm_get_timestring (time_to_empty);
                remaining_text = g_strconcat ("<b>", tmp, "</b>", NULL);
                g_free (tmp);

                /* TRANSLATORS: tell the user how much time they have got */
                message = g_strdup_printf (_("Approximately %s remaining (%.0f%%)"), remaining_text, percentage);
                g_free (remaining_text);

        } else if (kind == UP_DEVICE_KIND_UPS) {
                /* TRANSLATORS: UPS is starting to get a little low */
                title = _("UPS low");
                tmp = gpm_get_timestring (time_to_empty);
                remaining_text = g_strconcat ("<b>", tmp, "</b>", NULL);
                g_free (tmp);

                /* TRANSLATORS: tell the user how much time they have got */
                message = g_strdup_printf (_("Approximately %s of remaining UPS backup power (%.0f%%)"),
                                           remaining_text, percentage);
                g_free (remaining_text);
        } else if (kind == UP_DEVICE_KIND_MOUSE) {
                /* TRANSLATORS: mouse is getting a little low */
                title = _("Mouse battery low");

                /* TRANSLATORS: tell user more details */
                message = g_strdup_printf (_("Wireless mouse is low in power"));

        } else if (kind == UP_DEVICE_KIND_KEYBOARD) {
                /* TRANSLATORS: keyboard is getting a little low */
                title = _("Keyboard battery low");

                /* TRANSLATORS: tell user more details */
                message = g_strdup_printf (_("Wireless keyboard is low in power"));

        } else if (kind == UP_DEVICE_KIND_PDA) {
                /* TRANSLATORS: PDA is getting a little low */
                title = _("PDA battery low");

                /* TRANSLATORS: tell user more details */
                message = g_strdup_printf (_("PDA is low in power (%.0f%%)"), percentage);

        } else if (kind == UP_DEVICE_KIND_PHONE) {
                /* TRANSLATORS: cell phone (mobile) is getting a little low */
                title = _("Cell phone battery low");

                /* TRANSLATORS: tell user more details */
                message = g_strdup_printf (_("Cell phone is low in power (%.0f%%)"), percentage);

        } else if (kind == UP_DEVICE_KIND_MEDIA_PLAYER) {
                /* TRANSLATORS: media player, e.g. mp3 is getting a little low */
                title = _("Media player battery low");

                /* TRANSLATORS: tell user more details */
                message = g_strdup_printf (_("Media player is low in power (%.0f%%)"), percentage);

        } else if (kind == UP_DEVICE_KIND_TABLET) {
                /* TRANSLATORS: graphics tablet, e.g. wacom is getting a little low */
                title = _("Tablet battery low");

                /* TRANSLATORS: tell user more details */
                message = g_strdup_printf (_("Tablet is low in power (%.0f%%)"), percentage);

        } else if (kind == UP_DEVICE_KIND_COMPUTER) {
                /* TRANSLATORS: computer, e.g. ipad is getting a little low */
                title = _("Attached computer battery low");

                /* TRANSLATORS: tell user more details */
                message = g_strdup_printf (_("Attached computer is low in power (%.0f%%)"), percentage);
        }

        /* get correct icon */
        icon = gpm_upower_get_device_icon (device, TRUE);

        /* close any existing notification of this class */
        notify_close_if_showing (manager->priv->notification_low);

        /* create a new notification */
        create_notification (title, message,
                             get_first_themed_icon_name (icon),
                             &manager->priv->notification_low);
        notify_notification_set_timeout (manager->priv->notification_low,
                                         CSD_POWER_MANAGER_NOTIFY_TIMEOUT_LONG);
        notify_notification_set_urgency (manager->priv->notification_low,
                                         NOTIFY_URGENCY_NORMAL);
        notify_notification_set_app_name (manager->priv->notification_low, _("Power"));
        notify_notification_set_hint (manager->priv->notification_low,
                                      "transient", g_variant_new_boolean (TRUE));

        /* try to show */
        ret = notify_notification_show (manager->priv->notification_low,
                                        &error);
        if (!ret) {
                g_warning ("failed to show notification: %s", error->message);
                g_error_free (error);
                g_object_unref (manager->priv->notification_low);
        }

        /* play the sound, using sounds from the naming spec */
        ca_context_play (manager->priv->canberra_context, 0,
                         CA_PROP_EVENT_ID, "battery-low",
                         /* TRANSLATORS: this is the sound description */
                         CA_PROP_EVENT_DESCRIPTION, _("Battery is low"), NULL);

out:
        if (icon != NULL)
                g_object_unref (icon);
        g_free (message);
}

static void
engine_charge_critical (CsdPowerManager *manager, UpDevice *device)
{
        const gchar *title = NULL;
        gboolean ret;
        gchar *message = NULL;
        gdouble percentage;
        GIcon *icon = NULL;
        gint64 time_to_empty;
        CsdPowerActionType policy;
        UpDeviceKind kind;
        GError *error = NULL;

        /* get device properties */
        g_object_get (device,
                      "kind", &kind,
                      "percentage", &percentage,
                      "time-to-empty", &time_to_empty,
                      NULL);

        /* check to see if the batteries have not noticed we are on AC */
        if (kind == UP_DEVICE_KIND_BATTERY) {
                if (!system_on_battery (manager)) {
                        g_warning ("ignoring critically low message as we are not on battery power");
                        goto out;
                }
        }

        if (kind == UP_DEVICE_KIND_BATTERY) {

                /* if the user has no other batteries, drop the "Laptop" wording */
                ret = engine_just_laptop_battery (manager);
                if (ret) {
                        /* TRANSLATORS: laptop battery critically low, and only have one kind of battery */
                        title = _("Battery critically low");
                } else {
                        /* TRANSLATORS: laptop battery critically low, and we have more than one type of battery */
                        title = _("Laptop battery critically low");
                }

                /* we have to do different warnings depending on the policy */
                policy = manager_critical_action_get (manager, FALSE);

                /* use different text for different actions */
                if (policy == CSD_POWER_ACTION_NOTHING) {
                        /* TRANSLATORS: tell the use to insert the plug, as we're not going to do anything */
                        message = g_strdup (_("Plug in your AC adapter to avoid losing data."));

                } else if (policy == CSD_POWER_ACTION_SUSPEND) {
                        /* TRANSLATORS: give the user a ultimatum */
                        message = g_strdup_printf (_("Computer will suspend very soon unless it is plugged in."));

                } else if (policy == CSD_POWER_ACTION_HIBERNATE) {
                        /* TRANSLATORS: give the user a ultimatum */
                        message = g_strdup_printf (_("Computer will hibernate very soon unless it is plugged in."));

                } else if (policy == CSD_POWER_ACTION_SHUTDOWN) {
                        /* TRANSLATORS: give the user a ultimatum */
                        message = g_strdup_printf (_("Computer will shutdown very soon unless it is plugged in."));
                }

        } else if (kind == UP_DEVICE_KIND_UPS) {
                gchar *remaining_text;
                gchar *tmp;

                /* TRANSLATORS: the UPS is very low */
                title = _("UPS critically low");
                tmp = gpm_get_timestring (time_to_empty);
                remaining_text = g_strconcat ("<b>", tmp, "</b>", NULL);
                g_free (tmp);

                /* TRANSLATORS: give the user a ultimatum */
                message = g_strdup_printf (_("Approximately %s of remaining UPS power (%.0f%%). "
                                             "Restore AC power to your computer to avoid losing data."),
                                           remaining_text, percentage);
                g_free (remaining_text);
        } else if (kind == UP_DEVICE_KIND_MOUSE) {
                /* TRANSLATORS: the mouse battery is very low */
                title = _("Mouse battery low");

                /* TRANSLATORS: the device is just going to stop working */
                message = g_strdup_printf (_("Wireless mouse is very low in power. "
                                             "This device will soon stop functioning if not charged."));
        } else if (kind == UP_DEVICE_KIND_KEYBOARD) {
                /* TRANSLATORS: the keyboard battery is very low */
                title = _("Keyboard battery low");

                /* TRANSLATORS: the device is just going to stop working */
                message = g_strdup_printf (_("Wireless keyboard is very low in power. "
                                             "This device will soon stop functioning if not charged."));
        } else if (kind == UP_DEVICE_KIND_PDA) {

                /* TRANSLATORS: the PDA battery is very low */
                title = _("PDA battery low");

                /* TRANSLATORS: the device is just going to stop working */
                message = g_strdup_printf (_("PDA is very low in power (%.0f%%). "
                                             "This device will soon stop functioning if not charged."),
                                           percentage);

        } else if (kind == UP_DEVICE_KIND_PHONE) {

                /* TRANSLATORS: the cell battery is very low */
                title = _("Cell phone battery low");

                /* TRANSLATORS: the device is just going to stop working */
                message = g_strdup_printf (_("Cell phone is very low in power (%.0f%%). "
                                             "This device will soon stop functioning if not charged."),
                                           percentage);

        } else if (kind == UP_DEVICE_KIND_MEDIA_PLAYER) {

                /* TRANSLATORS: the cell battery is very low */
                title = _("Cell phone battery low");

                /* TRANSLATORS: the device is just going to stop working */
                message = g_strdup_printf (_("Media player is very low in power (%.0f%%). "
                                             "This device will soon stop functioning if not charged."),
                                           percentage);
        } else if (kind == UP_DEVICE_KIND_TABLET) {

                /* TRANSLATORS: the cell battery is very low */
                title = _("Tablet battery low");

                /* TRANSLATORS: the device is just going to stop working */
                message = g_strdup_printf (_("Tablet is very low in power (%.0f%%). "
                                             "This device will soon stop functioning if not charged."),
                                           percentage);
        } else if (kind == UP_DEVICE_KIND_COMPUTER) {

                /* TRANSLATORS: the cell battery is very low */
                title = _("Attached computer battery low");

                /* TRANSLATORS: the device is just going to stop working */
                message = g_strdup_printf (_("Attached computer is very low in power (%.0f%%). "
                                             "The device will soon shutdown if not charged."),
                                           percentage);
        }

        /* get correct icon */
        icon = gpm_upower_get_device_icon (device, TRUE);

        /* close any existing notification of this class */
        notify_close_if_showing (manager->priv->notification_low);

        /* create a new notification */
        create_notification (title, message,
                             get_first_themed_icon_name (icon),
                             &manager->priv->notification_low);
        notify_notification_set_timeout (manager->priv->notification_low,
                                         CSD_POWER_MANAGER_NOTIFY_TIMEOUT_LONG);
        notify_notification_set_urgency (manager->priv->notification_low,
                                         NOTIFY_URGENCY_CRITICAL);
        notify_notification_set_app_name (manager->priv->notification_low, _("Power"));

        /* try to show */
        ret = notify_notification_show (manager->priv->notification_low,
                                        &error);
        if (!ret) {
                g_warning ("failed to show notification: %s", error->message);
                g_error_free (error);
                g_object_unref (manager->priv->notification_low);
        }

        switch (kind) {

        case UP_DEVICE_KIND_BATTERY:
        case UP_DEVICE_KIND_UPS:
                g_debug ("critical charge level reached, starting sound loop");
                play_loop_start (manager,
                                 "battery-caution",
                                 _("Battery is critically low"),
                                 TRUE,
                                 CSD_POWER_MANAGER_CRITICAL_ALERT_TIMEOUT);
                break;

        default:
                /* play the sound, using sounds from the naming spec */
                ca_context_play (manager->priv->canberra_context, 0,
                                 CA_PROP_EVENT_ID, "battery-caution",
                                 /* TRANSLATORS: this is the sound description */
                                 CA_PROP_EVENT_DESCRIPTION, _("Battery is critically low"), NULL);
                break;
        }
out:
        if (icon != NULL)
                g_object_unref (icon);
        g_free (message);
}

static void
engine_charge_action (CsdPowerManager *manager, UpDevice *device)
{
        const gchar *title = NULL;
        gboolean ret;
        gchar *message = NULL;
        GError *error = NULL;
        GIcon *icon = NULL;
        CsdPowerActionType policy;
        guint timer_id;
        UpDeviceKind kind;

        /* get device properties */
        g_object_get (device,
                      "kind", &kind,
                      NULL);

        /* check to see if the batteries have not noticed we are on AC */
        if (kind == UP_DEVICE_KIND_BATTERY) {
                if (!system_on_battery (manager)) {
                        g_warning ("ignoring critically low message as we are not on battery power");
                        goto out;
                }
        }

        if (kind == UP_DEVICE_KIND_BATTERY) {

                /* TRANSLATORS: laptop battery is really, really, low */
                title = _("Laptop battery critically low");

                /* we have to do different warnings depending on the policy */
                policy = manager_critical_action_get (manager, FALSE);

                /* use different text for different actions */
                if (policy == CSD_POWER_ACTION_NOTHING) {
                        /* TRANSLATORS: computer will shutdown without saving data */
                        message = g_strdup (_("The battery is below the critical level and "
                                              "this computer will <b>power-off</b> when the "
                                              "battery becomes completely empty."));

                } else if (policy == CSD_POWER_ACTION_SUSPEND) {
                        /* TRANSLATORS: computer will suspend */
                        message = g_strdup (_("The battery is below the critical level and "
                                              "this computer is about to suspend.\n"
                                              "<b>NOTE:</b> A small amount of power is required "
                                              "to keep your computer in a suspended state."));

                } else if (policy == CSD_POWER_ACTION_HIBERNATE) {
                        /* TRANSLATORS: computer will hibernate */
                        message = g_strdup (_("The battery is below the critical level and "
                                              "this computer is about to hibernate."));

                } else if (policy == CSD_POWER_ACTION_SHUTDOWN) {
                        /* TRANSLATORS: computer will just shutdown */
                        message = g_strdup (_("The battery is below the critical level and "
                                              "this computer is about to shutdown."));
                }

                /* wait 20 seconds for user-panic */
                timer_id = g_timeout_add_seconds (20, (GSourceFunc) manager_critical_action_do_cb, manager);
                g_source_set_name_by_id (timer_id, "[CsdPowerManager] battery critical-action");

        } else if (kind == UP_DEVICE_KIND_UPS) {
                /* TRANSLATORS: UPS is really, really, low */
                title = _("UPS critically low");

                /* we have to do different warnings depending on the policy */
                policy = manager_critical_action_get (manager, TRUE);

                /* use different text for different actions */
                if (policy == CSD_POWER_ACTION_NOTHING) {
                        /* TRANSLATORS: computer will shutdown without saving data */
                        message = g_strdup (_("UPS is below the critical level and "
                                              "this computer will <b>power-off</b> when the "
                                              "UPS becomes completely empty."));

                } else if (policy == CSD_POWER_ACTION_HIBERNATE) {
                        /* TRANSLATORS: computer will hibernate */
                        message = g_strdup (_("UPS is below the critical level and "
                                              "this computer is about to hibernate."));

                } else if (policy == CSD_POWER_ACTION_SHUTDOWN) {
                        /* TRANSLATORS: computer will just shutdown */
                        message = g_strdup (_("UPS is below the critical level and "
                                              "this computer is about to shutdown."));
                }

                /* wait 20 seconds for user-panic */
                timer_id = g_timeout_add_seconds (20, (GSourceFunc) manager_critical_ups_action_do_cb, manager);
                g_source_set_name_by_id (timer_id, "[CsdPowerManager] ups critical-action");
        }

        /* not all types have actions */
        if (title == NULL) {
                g_free (message);
                return;
        }

        /* get correct icon */
        icon = gpm_upower_get_device_icon (device, TRUE);

        /* close any existing notification of this class */
        notify_close_if_showing (manager->priv->notification_low);

        /* create a new notification */
        create_notification (title, message,
                             get_first_themed_icon_name (icon),
                             &manager->priv->notification_low);
        notify_notification_set_timeout (manager->priv->notification_low,
                                         CSD_POWER_MANAGER_NOTIFY_TIMEOUT_LONG);
        notify_notification_set_urgency (manager->priv->notification_low,
                                         NOTIFY_URGENCY_CRITICAL);
        notify_notification_set_app_name (manager->priv->notification_low, _("Power"));

        /* try to show */
        ret = notify_notification_show (manager->priv->notification_low,
                                        &error);
        if (!ret) {
                g_warning ("failed to show notification: %s", error->message);
                g_error_free (error);
                g_object_unref (manager->priv->notification_low);
        }

        /* play the sound, using sounds from the naming spec */
        ca_context_play (manager->priv->canberra_context, 0,
                         CA_PROP_EVENT_ID, "battery-caution",
                         /* TRANSLATORS: this is the sound description */
                         CA_PROP_EVENT_DESCRIPTION, _("Battery is critically low"), NULL);
out:
        if (icon != NULL)
                g_object_unref (icon);
        g_free (message);
}

static void
#if UP_CHECK_VERSION(0,99,0)
device_properties_changed_cb (UpDevice *device, GParamSpec *pspec, CsdPowerManager *manager)
#else
engine_device_changed_cb (UpClient *client, UpDevice *device, CsdPowerManager *manager)
#endif
{
        UpDeviceKind kind;
        UpDeviceState state;
        UpDeviceState state_old;
        CsdPowerManagerWarning warning_old;
        CsdPowerManagerWarning warning;

        /* get device properties */
        g_object_get (device,
                      "kind", &kind,
                      NULL);

        /* if battery then use composite device to cope with multiple batteries */
        if (kind == UP_DEVICE_KIND_BATTERY) {
                g_debug ("updating because %s changed", up_device_get_object_path (device));
                device = engine_update_composite_device (manager, device);
        }

        /* get device properties (may be composite) */
        g_object_get (device,
                      "state", &state,
                      NULL);

        g_debug ("%s state is now %s", up_device_get_object_path (device), up_device_state_to_string (state));

        /* see if any interesting state changes have happened */
        state_old = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(device), "engine-state-old"));
        if (state_old != state) {
                if (state == UP_DEVICE_STATE_DISCHARGING) {
                        g_debug ("discharging");
                        engine_ups_discharging (manager, device);
                } else if (state == UP_DEVICE_STATE_FULLY_CHARGED ||
                           state == UP_DEVICE_STATE_CHARGING) {
                        g_debug ("fully charged or charging, hiding notifications if any");
                        notify_close_if_showing (manager->priv->notification_low);
                        notify_close_if_showing (manager->priv->notification_discharging);
                }

                /* save new state */
                g_object_set_data (G_OBJECT(device), "engine-state-old", GUINT_TO_POINTER(state));
        }

        /* check the warning state has not changed */
        warning_old = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(device), "engine-warning-old"));
        warning = engine_get_warning (manager, device);
        if (warning != warning_old) {
                if (warning == WARNING_LOW) {
                        g_debug ("** EMIT: charge-low");
                        engine_charge_low (manager, device);
                } else if (warning == WARNING_CRITICAL) {
                        g_debug ("** EMIT: charge-critical");
                        engine_charge_critical (manager, device);
                } else if (warning == WARNING_ACTION) {
                        g_debug ("charge-action");
                        engine_charge_action (manager, device);
                }
                /* save new state */
                g_object_set_data (G_OBJECT(device), "engine-warning-old", GUINT_TO_POINTER(warning));
        }

        engine_recalculate_state (manager);
}

static void
refresh_notification_settings (CsdPowerManager *manager)
{
        manager->priv->notify_keyboard = g_settings_get_boolean (manager->priv->settings,
                                                                      "power-notifications-for-keyboard");
        manager->priv->notify_mouse = g_settings_get_boolean (manager->priv->settings,
                                                                      "power-notifications-for-mouse");
        manager->priv->notify_other_devices = g_settings_get_boolean (manager->priv->settings,
                                                                      "power-notifications-for-other-devices");
}

static UpDevice *
engine_get_primary_device (CsdPowerManager *manager)
{
        guint i;
        UpDevice *device = NULL;
        UpDevice *device_tmp;
        UpDeviceKind kind;
        UpDeviceState state;
        gboolean is_present;

        for (i=0; i<manager->priv->devices_array->len; i++) {
                device_tmp = g_ptr_array_index (manager->priv->devices_array, i);

                /* get device properties */
                g_object_get (device_tmp,
                              "kind", &kind,
                              "state", &state,
                              "is-present", &is_present,
                              NULL);

                /* not present */
                if (!is_present)
                        continue;

                /* not discharging */
                if (state != UP_DEVICE_STATE_DISCHARGING)
                        continue;

                /* not battery */
                if (kind != UP_DEVICE_KIND_BATTERY)
                        continue;

                /* use composite device to cope with multiple batteries */
                device = g_object_ref (engine_get_composite_device (manager, device_tmp));
                break;
        }
        return device;
}

static void
phone_device_added_cb (GpmPhone *phone, guint idx, CsdPowerManager *manager)
{
        UpDevice *device;
        device = up_device_new ();

        g_debug ("phone added %i", idx);

        /* get device properties */
        g_object_set (device,
                      "kind", UP_DEVICE_KIND_PHONE,
                      "is-rechargeable", TRUE,
                      "native-path", g_strdup_printf ("dummy:phone_%i", idx),
                      "is-present", TRUE,
                      NULL);

        /* state changed */
        engine_device_add (manager, device);
        g_ptr_array_add (manager->priv->devices_array, g_object_ref (device));
        engine_recalculate_state (manager);
}

static void
phone_device_removed_cb (GpmPhone *phone, guint idx, CsdPowerManager *manager)
{
        guint i;
        UpDevice *device;
        UpDeviceKind kind;

        g_debug ("phone removed %i", idx);

        for (i=0; i<manager->priv->devices_array->len; i++) {
                device = g_ptr_array_index (manager->priv->devices_array, i);

                /* get device properties */
                g_object_get (device,
                              "kind", &kind,
                              NULL);

                if (kind == UP_DEVICE_KIND_PHONE) {
                        g_ptr_array_remove_index (manager->priv->devices_array, i);
                        break;
                }
        }

        /* state changed */
        engine_recalculate_state (manager);
}

static void
phone_device_refresh_cb (GpmPhone *phone, guint idx, CsdPowerManager *manager)
{
        guint i;
        UpDevice *device;
        UpDeviceKind kind;
        UpDeviceState state;
        gboolean is_present;
        gdouble percentage;

        g_debug ("phone refresh %i", idx);

        for (i=0; i<manager->priv->devices_array->len; i++) {
                device = g_ptr_array_index (manager->priv->devices_array, i);

                /* get device properties */
                g_object_get (device,
                              "kind", &kind,
                              "state", &state,
                              "percentage", &percentage,
                              "is-present", &is_present,
                              NULL);

                if (kind == UP_DEVICE_KIND_PHONE) {
                        is_present = gpm_phone_get_present (phone, idx);
                        state = gpm_phone_get_on_ac (phone, idx) ? UP_DEVICE_STATE_CHARGING : UP_DEVICE_STATE_DISCHARGING;
                        percentage = gpm_phone_get_percentage (phone, idx);
                        break;
                }
        }

        /* state changed */
        engine_recalculate_state (manager);
}

static void
cinnamon_session_shutdown_cb (GObject *source_object,
                           GAsyncResult *res,
                           gpointer user_data)
{
        GVariant *result;
        GError *error = NULL;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                           res,
                                           &error);
        if (result == NULL) {
                g_warning ("couldn't shutdown using cinnamon-session: %s",
                           error->message);
                g_error_free (error);
        } else {
                g_variant_unref (result);
        }
}

static void
cinnamon_session_shutdown (void)
{
        GError *error = NULL;
        GDBusProxy *proxy;

        /* ask cinnamon-session to show the shutdown dialog with a timeout */
        proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                               NULL,
                                               GNOME_SESSION_DBUS_NAME,
                                               GNOME_SESSION_DBUS_PATH,
                                               GNOME_SESSION_DBUS_INTERFACE,
                                               NULL, &error);
        if (proxy == NULL) {
                g_warning ("cannot connect to cinnamon-session: %s",
                           error->message);
                g_error_free (error);
                return;
        }
        g_dbus_proxy_call (proxy,
                           "Shutdown",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL,
                           cinnamon_session_shutdown_cb, NULL);
        g_object_unref (proxy);
}

static void
turn_monitors_off (CsdPowerManager *manager)
{
    gboolean ret;
    GError *error = NULL;

    ret = gnome_rr_screen_set_dpms_mode (manager->priv->x11_screen,
                                         GNOME_RR_DPMS_OFF,
                                         &error);
    if (!ret) {
            g_warning ("failed to turn the panel off for policy action: %s",
                       error->message);
            g_error_free (error);
    }
}

static void
do_power_action_type (CsdPowerManager *manager,
                      CsdPowerActionType action_type)
{
        switch (action_type) {
        case CSD_POWER_ACTION_SUSPEND:
                if (should_lock_on_suspend (manager)) {
                        activate_screensaver (manager, TRUE);
                }

                turn_monitors_off (manager);

                gboolean hybrid = g_settings_get_boolean (manager->priv->settings_cinnamon_session,
                                                          "prefer-hybrid-sleep");
                gboolean suspend_then_hibernate = g_settings_get_boolean (manager->priv->settings_cinnamon_session,
                                                          "suspend-then-hibernate");

                csd_power_suspend (hybrid, suspend_then_hibernate);
                break;
        case CSD_POWER_ACTION_INTERACTIVE:
                cinnamon_session_shutdown ();
                break;
        case CSD_POWER_ACTION_HIBERNATE:
                if (should_lock_on_suspend (manager)) {
                        activate_screensaver (manager, TRUE);
                }

                turn_monitors_off (manager);
                csd_power_hibernate ();
                break;
        case CSD_POWER_ACTION_SHUTDOWN:
                /* this is only used on critically low battery where
                 * hibernate is not available and is marginally better
                 * than just powering down the computer mid-write */
                csd_power_poweroff ();
                break;
        case CSD_POWER_ACTION_BLANK:
                /* Lock first or else xrandr might reconfigure stuff and the ss's coverage
                 * may be incorrect upon return. */
                activate_screensaver (manager, FALSE);
                turn_monitors_off (manager);
                break;
        case CSD_POWER_ACTION_NOTHING:
                break;
        }
}

static gboolean
upower_kbd_get_percentage (CsdPowerManager *manager, GError **error)
{
        GVariant *k_now = NULL;

        k_now = g_dbus_proxy_call_sync (manager->priv->upower_kbd_proxy,
                                        "GetBrightness",
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        error);
        if (k_now != NULL) {
                g_variant_get (k_now, "(i)", &manager->priv->kbd_brightness_now);
                g_variant_unref (k_now);
                return TRUE;
        }

        return FALSE;
}

static void
upower_kbd_emit_changed (CsdPowerManager *manager)
{
        /* not yet connected to the bus */
        if (manager->priv->keyboard_iface == NULL)
                return;

        csd_keyboard_emit_changed (manager->priv->keyboard_iface);
}

static gboolean
upower_kbd_set_brightness (CsdPowerManager *manager, guint value, GError **error)
{
        GVariant *retval;

        /* same as before */
        if (manager->priv->kbd_brightness_now == value)
                return TRUE;

        /* update h/w value */
        retval = g_dbus_proxy_call_sync (manager->priv->upower_kbd_proxy,
                                         "SetBrightness",
                                         g_variant_new ("(i)", (gint) value),
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         error);
        if (retval == NULL)
                return FALSE;

        /* save new value */
        manager->priv->kbd_brightness_now = value;
        g_variant_unref (retval);
        upower_kbd_emit_changed(manager);
        return TRUE;
}

static gboolean
upower_kbd_toggle (CsdPowerManager *manager,
                   GError **error)
{
        gboolean ret;

        if (manager->priv->kbd_brightness_old >= 0) {
                g_debug ("keyboard toggle off");
                ret = upower_kbd_set_brightness (manager,
                                                 manager->priv->kbd_brightness_old,
                                                 error);
                if (ret) {
                        /* succeeded, set to -1 since now no old value */
                        manager->priv->kbd_brightness_old = -1;
                }
        } else {
                g_debug ("keyboard toggle on");
                /* save the current value to restore later when untoggling */
                manager->priv->kbd_brightness_old = manager->priv->kbd_brightness_now;
                ret = upower_kbd_set_brightness (manager, 0, error);
                if (!ret) {
                        /* failed, reset back to -1 */
                        manager->priv->kbd_brightness_old = -1;
                }
        }

        upower_kbd_emit_changed(manager);
        return ret;
}

static void
upower_kbd_handle_changed (GDBusProxy *proxy,
                           gchar      *sender_name,
                           gchar      *signal_name,
                           GVariant   *parameters,
                           gpointer    user_data)
{
        CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);

        g_debug("keyboard changed signal");

        if (g_strcmp0 (signal_name, "BrightnessChangedWithSource") == 0) {
                g_debug ("Received upower kbdbacklight BrightnessChangedWithSource");
                const gchar *source;
                gint brightness;

                g_variant_get (parameters, "(i&s)", &brightness, &source);

                if (g_strcmp0 (source, "external") == 0) {
                    return;
                }

                manager->priv->kbd_brightness_now = brightness;
                upower_kbd_emit_changed(manager);
        }

}

static gboolean
suspend_on_lid_close (CsdPowerManager *manager)
{
        CsdXrandrBootBehaviour val;

        if (!external_monitor_is_connected (manager->priv->x11_screen))
                return TRUE;

        val = g_settings_get_enum (manager->priv->settings_xrandr, "default-monitors-setup");
        return val == CSD_XRANDR_BOOT_BEHAVIOUR_DO_NOTHING;
}

static gboolean
inhibit_lid_switch_timer_cb (CsdPowerManager *manager)
{
        if (suspend_on_lid_close (manager)) {
                g_debug ("no external monitors for a while; uninhibiting lid close");
                uninhibit_lid_switch (manager);
                manager->priv->inhibit_lid_switch_timer_id = 0;
                return G_SOURCE_REMOVE;
        }

        g_debug ("external monitor still there; trying again later");
        return G_SOURCE_CONTINUE;
}

/* Sets up a timer to be triggered some seconds after closing the laptop lid
 * when the laptop is *not* suspended for some reason.  We'll check conditions
 * again in the timeout handler to see if we can suspend then.
 */
static void
setup_inhibit_lid_switch_timer (CsdPowerManager *manager)
{
        if (manager->priv->inhibit_lid_switch_timer_id != 0) {
                g_debug ("lid close safety timer already set up");
                return;
        }

        g_debug ("setting up lid close safety timer");

        manager->priv->inhibit_lid_switch_timer_id = g_timeout_add_seconds (CSD_POWER_MANAGER_LID_CLOSE_SAFETY_TIMEOUT,
                                                                          (GSourceFunc) inhibit_lid_switch_timer_cb,
                                                                          manager);
        g_source_set_name_by_id (manager->priv->inhibit_lid_switch_timer_id, "[CsdPowerManager] lid close safety timer");
}

static void
restart_inhibit_lid_switch_timer (CsdPowerManager *manager)
{
        if (manager->priv->inhibit_lid_switch_timer_id != 0) {
                g_debug ("restarting lid close safety timer");
                g_source_remove (manager->priv->inhibit_lid_switch_timer_id);
                manager->priv->inhibit_lid_switch_timer_id = 0;
                setup_inhibit_lid_switch_timer (manager);
        }
}


static gboolean
randr_output_is_on (GnomeRROutput *output)
{
        GnomeRRCrtc *crtc;

        crtc = gnome_rr_output_get_crtc (output);
        if (!crtc)
                return FALSE;
        return gnome_rr_crtc_get_current_mode (crtc) != NULL;
}

static gboolean
external_monitor_is_connected (GnomeRRScreen *screen)
{
        GnomeRROutput **outputs;
        guint i;

        /* see if we have more than one screen plugged in */
        outputs = gnome_rr_screen_list_outputs (screen);
        for (i = 0; outputs[i] != NULL; i++) {
                if (randr_output_is_on (outputs[i]) &&
                    !gnome_rr_output_is_laptop (outputs[i]))
                        return TRUE;
        }

        return FALSE;
}

static void
on_randr_event (GnomeRRScreen *screen, gpointer user_data)
{
        CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);

        if (suspend_on_lid_close (manager)) {
                restart_inhibit_lid_switch_timer (manager);
                return;
        }

        /* when a second monitor is plugged in, we take the
        * handle-lid-switch inhibitor lock of logind to prevent
        * it from suspending.
        *
        * Uninhibiting is done in the inhibit_lid_switch_timer,
        * since we want to give users a few seconds when unplugging
        * and replugging an external monitor, not suspend right away.
        */
        inhibit_lid_switch (manager);
        setup_inhibit_lid_switch_timer (manager);
}

static void
do_lid_open_action (CsdPowerManager *manager)
{
        gboolean ret;
        GError *error = NULL;

        /* play a sound, using sounds from the naming spec */
        ca_context_play (manager->priv->canberra_context, 0,
                         CA_PROP_EVENT_ID, "lid-open",
                         /* TRANSLATORS: this is the sound description */
                         CA_PROP_EVENT_DESCRIPTION, _("Lid has been opened"),
                         NULL);

        /* ensure we turn the panel back on after lid open */
        ret = gnome_rr_screen_set_dpms_mode (manager->priv->x11_screen,
                                             GNOME_RR_DPMS_ON,
                                             &error);
        if (!ret) {
                g_warning ("failed to turn the panel on after lid open: %s",
                           error->message);
                g_clear_error (&error);
        }

        /* only toggle keyboard if present and already toggled off */
        if (manager->priv->upower_kbd_proxy != NULL &&
            manager->priv->kbd_brightness_old != -1) {
                ret = upower_kbd_toggle (manager, &error);
                if (!ret) {
                        g_warning ("failed to turn the kbd backlight on: %s",
                                   error->message);
                        g_error_free (error);
                }
        }

        kill_lid_close_safety_timer (manager);
}

static gboolean
is_on (GnomeRROutput *output)
{
	GnomeRRCrtc *crtc;

	crtc = gnome_rr_output_get_crtc (output);
	if (!crtc)
		return FALSE;
	return gnome_rr_crtc_get_current_mode (crtc) != NULL;
}

static gboolean
non_laptop_outputs_are_all_off (GnomeRRScreen *screen)
{
        GnomeRROutput **outputs;
        int i;

        outputs = gnome_rr_screen_list_outputs (screen);
        for (i = 0; outputs[i] != NULL; i++) {
                if (gnome_rr_output_is_laptop (outputs[i]))
                        continue;

                if (is_on (outputs[i]))
                        return FALSE;
        }

        return TRUE;
}

/* Timeout callback used to check conditions when the laptop's lid is closed but
 * the machine is not suspended yet.  We try to suspend again, so that the laptop
 * won't overheat if placed in a backpack.
 */
static gboolean
lid_close_safety_timer_cb (CsdPowerManager *manager)
{
        manager->priv->lid_close_safety_timer_id = 0;

        g_debug ("lid has been closed for a while; trying to suspend again");
        do_lid_closed_action (manager);

        return FALSE;
}

/* Sets up a timer to be triggered some seconds after closing the laptop lid
 * when the laptop is *not* suspended for some reason.  We'll check conditions
 * again in the timeout handler to see if we can suspend then.
 */
static void
setup_lid_close_safety_timer (CsdPowerManager *manager)
{
        if (manager->priv->lid_close_safety_timer_id != 0)
                return;

        manager->priv->lid_close_safety_timer_id = g_timeout_add_seconds (CSD_POWER_MANAGER_LID_CLOSE_SAFETY_TIMEOUT,
                                                                          (GSourceFunc) lid_close_safety_timer_cb,
                                                                          manager);
        g_source_set_name_by_id (manager->priv->lid_close_safety_timer_id, "[CsdPowerManager] lid close safety timer");
}

static void
kill_lid_close_safety_timer (CsdPowerManager *manager)
{
        if (manager->priv->lid_close_safety_timer_id != 0) {
                g_source_remove (manager->priv->lid_close_safety_timer_id);
                manager->priv->lid_close_safety_timer_id = 0;
        }
}

static void
suspend_with_lid_closed (CsdPowerManager *manager)
{
        gboolean ret;
        GError *error = NULL;
        CsdPowerActionType action_type;

        /* we have different settings depending on AC state */
        if (system_on_battery (manager)) {
                action_type = g_settings_get_enum (manager->priv->settings,
                                                   "lid-close-battery-action");
        } else {
                action_type = g_settings_get_enum (manager->priv->settings,
                                                   "lid-close-ac-action");
        }

        /* check we won't melt when the lid is closed */
        if (action_type != CSD_POWER_ACTION_SUSPEND &&
            action_type != CSD_POWER_ACTION_HIBERNATE) {
#if ! UP_CHECK_VERSION(0,99,0)
                if (up_client_get_lid_force_sleep (manager->priv->up_client)) {
                        g_warning ("to prevent damage, now forcing suspend");
                        do_power_action_type (manager, CSD_POWER_ACTION_SUSPEND);
                        return;
                }
#endif
        }

        /* only toggle keyboard if present and not already toggled */
        if (manager->priv->upower_kbd_proxy &&
            manager->priv->kbd_brightness_old == -1) {
                ret = upower_kbd_toggle (manager, &error);
                if (!ret) {
                        g_warning ("failed to turn the kbd backlight off: %s",
                                   error->message);
                        g_error_free (error);
                }
        }

        do_power_action_type (manager, action_type);
}

static void
do_lid_closed_action (CsdPowerManager *manager)
{
        /* play a sound, using sounds from the naming spec */
        ca_context_play (manager->priv->canberra_context, 0,
                         CA_PROP_EVENT_ID, "lid-close",
                         /* TRANSLATORS: this is the sound description */
                         CA_PROP_EVENT_DESCRIPTION, _("Lid has been closed"),
                         NULL);

        /* refresh RANDR so we get an accurate view of what monitors are plugged in when the lid is closed */
        gnome_rr_screen_refresh (manager->priv->x11_screen, NULL); /* NULL-GError */

        /* perform policy action */
        if (g_settings_get_boolean (manager->priv->settings, "lid-close-suspend-with-external-monitor")
            || non_laptop_outputs_are_all_off (manager->priv->x11_screen)) {
                g_debug ("lid is closed; suspending or hibernating");
                suspend_with_lid_closed (manager);
        } else {
                g_debug ("lid is closed; not suspending nor hibernating since some external monitor outputs are still active");
                setup_lid_close_safety_timer (manager);
        }
}


static void
#if UP_CHECK_VERSION(0,99,0)
lid_state_changed_cb (UpClient *client, GParamSpec *pspec, CsdPowerManager *manager)
#else
up_client_changed_cb (UpClient *client, CsdPowerManager *manager)
#endif
{
        gboolean lid_is_closed;
        gboolean on_battery;
        
        on_battery = system_on_battery(manager);
        if (!on_battery) {
            /* if we are playing a critical charge sound loop on AC, stop it */
            if (manager->priv->critical_alert_timeout_id > 0) {
                 g_debug ("stopping alert loop due to ac being present");
                 play_loop_stop (manager);
            }
            notify_close_if_showing (manager->priv->notification_low);
        }

        /* same state */
        lid_is_closed = up_client_get_lid_is_closed (manager->priv->up_client);

        if (manager->priv->lid_is_closed == lid_is_closed &&
                manager->priv->on_battery == on_battery)
                return;

        manager->priv->lid_is_closed = lid_is_closed;
        manager->priv->on_battery = on_battery;

        /* fake a keypress */
        if (lid_is_closed)
                do_lid_closed_action (manager);
        else
                do_lid_open_action (manager);
	
	engine_recalculate_state (manager);
}

typedef enum {
        SESSION_STATUS_CODE_AVAILABLE = 0,
        SESSION_STATUS_CODE_INVISIBLE,
        SESSION_STATUS_CODE_BUSY,
        SESSION_STATUS_CODE_IDLE,
        SESSION_STATUS_CODE_UNKNOWN
} SessionStatusCode;

typedef enum {
        SESSION_INHIBIT_MASK_LOGOUT = 1,
        SESSION_INHIBIT_MASK_SWITCH = 2,
        SESSION_INHIBIT_MASK_SUSPEND = 4,
        SESSION_INHIBIT_MASK_IDLE = 8
} SessionInhibitMask;

static const gchar *
idle_mode_to_string (CsdPowerIdleMode mode)
{
        if (mode == CSD_POWER_IDLE_MODE_NORMAL)
                return "normal";
        if (mode == CSD_POWER_IDLE_MODE_DIM)
                return "dim";
        if (mode == CSD_POWER_IDLE_MODE_BLANK)
                return "blank";
        if (mode == CSD_POWER_IDLE_MODE_SLEEP)
                return "sleep";
        return "unknown";
}

static GnomeRROutput *
get_primary_output (CsdPowerManager *manager)
{
        GnomeRROutput *output = NULL;
        GnomeRROutput **outputs;
        guint i;

        /* search all X11 outputs for the device id */
        outputs = gnome_rr_screen_list_outputs (manager->priv->x11_screen);
        if (outputs == NULL)
                goto out;

        for (i = 0; outputs[i] != NULL; i++) {
                if (gnome_rr_output_is_connected (outputs[i]) &&
                    gnome_rr_output_is_laptop (outputs[i]) &&
                    gnome_rr_output_get_backlight_min (outputs[i]) >= 0 &&
                    gnome_rr_output_get_backlight_max (outputs[i]) > 0) {
                        output = outputs[i];
                        break;
                }
        }
out:
        return output;
}

static void
backlight_override_settings_refresh (CsdPowerManager *manager)
{
        int i = 0;
        /* update all the stored backlight override properties
         * this is called on startup and by engine_settings_key_changed_cb */
        manager->priv->backlight_helper_force = g_settings_get_boolean
                        (manager->priv->settings, "backlight-helper-force");

        /* concatenate all the search preferences into a single argument string */
        gchar** backlight_preference_order = g_settings_get_strv
                (manager->priv->settings, "backlight-helper-preference-order");

        gchar* tmp1 = NULL;
        gchar* tmp2 = NULL;

        if (backlight_preference_order[0] != NULL) {
                tmp1 = g_strdup_printf("-b %s", backlight_preference_order[0]);
        }

        for (i=1; backlight_preference_order[i] != NULL; i++ )
        {
                tmp2 = tmp1;
                tmp1 = g_strdup_printf("%s -b %s", tmp2,
                                backlight_preference_order[i]);
                g_free(tmp2);
        }

        tmp2 = manager->priv->backlight_helper_preference_args;
        manager->priv->backlight_helper_preference_args = tmp1;
        g_free(tmp2);
        tmp2 = NULL;

        g_free(backlight_preference_order);
        backlight_preference_order = NULL;
}

/**
 * backlight_helper_get_value:
 *
 * Gets a brightness value from the PolicyKit helper.
 *
 * Return value: the signed integer value from the helper, or -1
 * for failure. If -1 then @error is set.
 **/
static gint64
backlight_helper_get_value (const gchar *argument, CsdPowerManager* manager,
                GError **error)
{
        gboolean ret;
        gchar *stdout_data = NULL;
        gint exit_status = 0;
        gint64 value = -1;
        gchar *command = NULL;
        gchar *endptr = NULL;

#ifndef __linux__
        /* non-Linux platforms won't have /sys/class/backlight */
        g_set_error_literal (error,
                             CSD_POWER_MANAGER_ERROR,
                             CSD_POWER_MANAGER_ERROR_FAILED,
                             "The sysfs backlight helper is only for Linux");
        goto out;
#endif

        /* get the data */
        command = g_strdup_printf (LIBEXECDIR "/csd-backlight-helper --%s %s",
                                   argument,
                                   manager->priv->backlight_helper_preference_args);
        ret = g_spawn_command_line_sync (command,
                                         &stdout_data,
                                         NULL,
                                         &exit_status,
                                         error);
        g_debug ("executed %s retval: %i", command, exit_status);

        if (!ret)
                goto out;

        if (WEXITSTATUS (exit_status) != 0) {
                 g_set_error (error,
                             CSD_POWER_MANAGER_ERROR,
                             CSD_POWER_MANAGER_ERROR_FAILED,
                             "csd-backlight-helper failed: %s",
                             stdout_data ? stdout_data : "No reason");
                goto out;
        }

        /* parse */
        value = g_ascii_strtoll (stdout_data, &endptr, 10);

        /* parsing error */
        if (endptr == stdout_data) {
                value = -1;
                g_set_error (error,
                             CSD_POWER_MANAGER_ERROR,
                             CSD_POWER_MANAGER_ERROR_FAILED,
                             "failed to parse value: %s",
                             stdout_data);
                goto out;
        }

        /* out of range */
        if (value > G_MAXINT) {
                value = -1;
                g_set_error (error,
                             CSD_POWER_MANAGER_ERROR,
                             CSD_POWER_MANAGER_ERROR_FAILED,
                             "value out of range: %s",
                             stdout_data);
                goto out;
        }

        /* Fetching the value failed, for some other reason */
        if (value < 0) {
                g_set_error (error,
                             CSD_POWER_MANAGER_ERROR,
                             CSD_POWER_MANAGER_ERROR_FAILED,
                             "value negative, but helper did not fail: %s",
                             stdout_data);
                goto out;
        }

out:
        g_free (command);
        g_free (stdout_data);
        return value;
}

/**
 * backlight_helper_set_value:
 *
 * Sets a brightness value using the PolicyKit helper.
 *
 * Return value: Success. If FALSE then @error is set.
 **/
static gboolean
backlight_helper_set_value (const gchar *argument,
                            gint value,
                            CsdPowerManager* manager,
                            GError **error)
{
        gboolean ret;
        gint exit_status = 0;
        gchar *command = NULL;

#ifndef __linux__
        /* non-Linux platforms won't have /sys/class/backlight */
        g_set_error_literal (error,
                             CSD_POWER_MANAGER_ERROR,
                             CSD_POWER_MANAGER_ERROR_FAILED,
                             "The sysfs backlight helper is only for Linux");
        goto out;
#endif

        /* get the data */
        command = g_strdup_printf ("pkexec " LIBEXECDIR "/csd-backlight-helper --%s %i %s",
                                   argument, value,
                                   manager->priv->backlight_helper_preference_args);
        ret = g_spawn_command_line_sync (command,
                                         NULL,
                                         NULL,
                                         &exit_status,
                                         error);

        g_debug ("executed %s retval: %i", command, exit_status);

        if (!ret || WEXITSTATUS (exit_status) != 0)
                goto out;

out:
        g_free (command);
        return ret;
}

static void
backlight_get_output_id (CsdPowerManager *manager,
                         gint *xout, gint *yout)
{
        GnomeRROutput *output = NULL;
        GnomeRROutput **outputs;
        GnomeRRCrtc *crtc;
        gint x, y;
        guint i;

        outputs = gnome_rr_screen_list_outputs (manager->priv->x11_screen);
        if (outputs == NULL)
                return;

        for (i = 0; outputs[i] != NULL; i++) {
                if (gnome_rr_output_is_connected (outputs[i]) &&
                    gnome_rr_output_is_laptop (outputs[i])) {
                        output = outputs[i];
                        break;
                }
        }

        if (output == NULL)
                return;

        crtc = gnome_rr_output_get_crtc (output);
        if (crtc == NULL)
                return;

        gnome_rr_crtc_get_position (crtc, &x, &y);

        *xout = x;
        *yout = y;
}

static gint
backlight_get_abs (CsdPowerManager *manager, GError **error)
{
        GnomeRROutput *output;

        /* prioritize user override settings */
        if (!manager->priv->backlight_helper_force)
        {
                /* prefer xbacklight */
                output = get_primary_output (manager);
                if (output != NULL) {
                        return gnome_rr_output_get_backlight (output,
                                                              error);
                }
        }

        /* fall back to the polkit helper */
        return backlight_helper_get_value ("get-brightness", manager, error);
}

static gint
min_abs_brightness (CsdPowerManager *manager, gint min, gint max)
{
    guint min_percent = g_settings_get_uint (manager->priv->settings, "minimum-display-brightness");

    return (min + ((max - min) / 100 * min_percent));
}

static gint
backlight_get_percentage (CsdPowerManager *manager, GError **error)
{
        GnomeRROutput *output;
        gint now;
        gint value = -1;
        gint min = 0;
        gint max;

        /* prioritize user override settings */
        if (!manager->priv->backlight_helper_force)
        {
                /* prefer xbacklight */
                output = get_primary_output (manager);
                if (output != NULL) {

                        min = gnome_rr_output_get_backlight_min (output);
                        max = gnome_rr_output_get_backlight_max (output);
                        now = gnome_rr_output_get_backlight (output, error);
                        if (now < 0)
                                goto out;

                        value = ABS_TO_PERCENTAGE (min_abs_brightness (manager, min, max), max, now);
                        goto out;
                }
        }

        /* fall back to the polkit helper */
        max = backlight_helper_get_value ("get-max-brightness", manager, error);
        if (max < 0)
                goto out;
        now = backlight_helper_get_value ("get-brightness", manager, error);
        if (now < 0)
                goto out;

        value = ABS_TO_PERCENTAGE (min_abs_brightness (manager, min, max), max, now);
out:
        return value;
}

static gint
backlight_get_min (CsdPowerManager *manager)
{
        /* if we have no xbacklight device, then hardcode zero as sysfs
        * offsets everything to 0 as min */

        /* user override means we will be using sysfs */
        if (manager->priv->backlight_helper_force)
                return 0;

        GnomeRROutput *output;

        output = get_primary_output (manager);
        if (output == NULL)
                return 0;

        /* get xbacklight value, which maybe non-zero */
        return gnome_rr_output_get_backlight_min (output);
}

static gint
backlight_get_max (CsdPowerManager *manager, GError **error)
{
        gint value;
        GnomeRROutput *output;

        /* prioritize user override settings */
        if (!manager->priv->backlight_helper_force)
        {
                /* prefer xbacklight */
                output = get_primary_output (manager);
                if (output != NULL) {
                        value = gnome_rr_output_get_backlight_max (output);
                        if (value < 0) {
                                g_set_error (error,
                                             CSD_POWER_MANAGER_ERROR,
                                             CSD_POWER_MANAGER_ERROR_FAILED,
                                             "failed to get backlight max");
                        }
                        return value;
                }
        }

        /* fall back to the polkit helper */
        return  backlight_helper_get_value ("get-max-brightness", manager, error);
}

static void
backlight_emit_changed (CsdPowerManager *manager)
{
        /* not yet connected to the bus */
        if (manager->priv->screen_iface == NULL)
                return;

        csd_screen_emit_changed (manager->priv->screen_iface);
}

static gboolean
backlight_set_percentage (CsdPowerManager *manager,
                          guint value,
                          gboolean emit_changed,
                          GError **error)
{
        GnomeRROutput *output;
        gboolean ret = FALSE;
        gint min = 0;
        gint max;
        guint discrete;

        /* prioritize user override settings */
        if (!manager->priv->backlight_helper_force)
        {
                /* prefer xbacklight */
                output = get_primary_output (manager);
                if (output != NULL) {
                        min = gnome_rr_output_get_backlight_min (output);
                        max = gnome_rr_output_get_backlight_max (output);
                        if (min < 0 || max < 0) {
                                g_warning ("no xrandr backlight capability");
                                goto out;
                        }
                        discrete = CLAMP (PERCENTAGE_TO_ABS (min_abs_brightness (manager, min, max), max, value), min_abs_brightness (manager, min, max), max);
                        ret = gnome_rr_output_set_backlight (output,
                                                             discrete,
                                                             error);
                        goto out;
                }
        }

        /* fall back to the polkit helper */
        max = backlight_helper_get_value ("get-max-brightness", manager, error);
        if (max < 0)
                goto out;

        discrete = CLAMP (PERCENTAGE_TO_ABS (min_abs_brightness (manager, min, max), max, value), min_abs_brightness (manager, min, max), max);
        ret = backlight_helper_set_value ("set-brightness",
                                          discrete,
                                          manager,
                                          error);
out:
        if (ret && emit_changed)
                backlight_emit_changed (manager);
        return ret;
}

static gint
backlight_step_up (CsdPowerManager *manager, GError **error)
{
        GnomeRROutput *output;
        gboolean ret = FALSE;
        gint percentage_value = -1;
        gint min = 0;
        gint max;
        gint now;
        gint step;
        guint discrete;
        GnomeRRCrtc *crtc;

        /* prioritize user override settings */
        if (!manager->priv->backlight_helper_force)
        {
                /* prefer xbacklight */
                output = get_primary_output (manager);
                if (output != NULL) {

                        crtc = gnome_rr_output_get_crtc (output);
                        if (crtc == NULL) {
                                g_set_error (error,
                                             CSD_POWER_MANAGER_ERROR,
                                             CSD_POWER_MANAGER_ERROR_FAILED,
                                             "no crtc for %s",
                                             gnome_rr_output_get_name (output));
                                goto out;
                        }
                        min = gnome_rr_output_get_backlight_min (output);
                        max = gnome_rr_output_get_backlight_max (output);
                        now = gnome_rr_output_get_backlight (output, error);
                        if (now < 0)
                               goto out;

                        step = BRIGHTNESS_STEP_AMOUNT (max - min_abs_brightness (manager, min, max));
                        discrete = MIN (now + step, max);
                        ret = gnome_rr_output_set_backlight (output,
                                                             discrete,
                                                             error);
                        if (ret)
                                percentage_value = ABS_TO_PERCENTAGE (min_abs_brightness (manager, min, max), max, discrete);
                        goto out;
                }
        }

        /* fall back to the polkit helper */
        now = backlight_helper_get_value ("get-brightness", manager, error);
        if (now < 0)
                goto out;
        max = backlight_helper_get_value ("get-max-brightness", manager, error);
        if (max < 0)
                goto out;
        step = BRIGHTNESS_STEP_AMOUNT (max - min_abs_brightness (manager, min, max));
        discrete = MIN (now + step, max);
        ret = backlight_helper_set_value ("set-brightness",
                                          discrete,
                                          manager,
                                          error);
        if (ret)
                percentage_value = ABS_TO_PERCENTAGE (min_abs_brightness (manager, min, max), max, discrete);
out:
        if (ret)
                backlight_emit_changed (manager);
        return percentage_value;
}

static gint
backlight_step_down (CsdPowerManager *manager, GError **error)
{
        GnomeRROutput *output;
        gboolean ret = FALSE;
        gint percentage_value = -1;
        gint min = 0;
        gint max;
        gint now;
        gint step;
        guint discrete;
        GnomeRRCrtc *crtc;

        /* prioritize user override settings */
        if (!manager->priv->backlight_helper_force)
        {
                /* prefer xbacklight */
                output = get_primary_output (manager);
                if (output != NULL) {

                        crtc = gnome_rr_output_get_crtc (output);
                        if (crtc == NULL) {
                                g_set_error (error,
                                             CSD_POWER_MANAGER_ERROR,
                                             CSD_POWER_MANAGER_ERROR_FAILED,
                                             "no crtc for %s",
                                             gnome_rr_output_get_name (output));
                                goto out;
                        }
                        min = gnome_rr_output_get_backlight_min (output);
                        max = gnome_rr_output_get_backlight_max (output);
                        now = gnome_rr_output_get_backlight (output, error);
                        if (now < 0)
                               goto out;
                        step = BRIGHTNESS_STEP_AMOUNT (max - min_abs_brightness (manager, min, max));
                        discrete = MAX (now - step, min_abs_brightness (manager, min, max));
                        ret = gnome_rr_output_set_backlight (output,
                                                             discrete,
                                                             error);
                        if (ret)
                                percentage_value = ABS_TO_PERCENTAGE (min_abs_brightness (manager, min, max), max, discrete);
                        goto out;
                }
        }

        /* fall back to the polkit helper */
        now = backlight_helper_get_value ("get-brightness", manager, error);
        if (now < 0)
                goto out;
        max = backlight_helper_get_value ("get-max-brightness", manager, error);
        if (max < 0)
                goto out;
        step = BRIGHTNESS_STEP_AMOUNT (max - min_abs_brightness (manager, min, max));
        discrete = MAX (now - step, min_abs_brightness (manager, min, max));
        ret = backlight_helper_set_value ("set-brightness",
                                          discrete,
                                          manager,
                                          error);
        if (ret)
                percentage_value = ABS_TO_PERCENTAGE (min_abs_brightness (manager, min, max), max, discrete);
out:
        if (ret)
                backlight_emit_changed (manager);
        return percentage_value;
}

static gint
backlight_set_abs (CsdPowerManager *manager,
                   guint value,
                   gboolean emit_changed,
                   GError **error)
{
        GnomeRROutput *output;
        gboolean ret = FALSE;

        /* prioritize user override settings */
        if (!manager->priv->backlight_helper_force)
        {
                /* prefer xbacklight */
                output = get_primary_output (manager);
                if (output != NULL) {
                        ret = gnome_rr_output_set_backlight (output,
                                                             value,
                                                             error);
                        goto out;
                }
        }
        /* fall back to the polkit helper */
        ret = backlight_helper_set_value ("set-brightness",
                                          value,
                                          manager,
                                          error);
out:
        if (ret && emit_changed)
                backlight_emit_changed (manager);
        return ret;
}

static gboolean
display_backlight_dim (CsdPowerManager *manager,
                       gint idle_percentage,
                       GError **error)
{
        gint min;
        gint max;
        gint now;
        gint idle;
        gboolean ret = FALSE;

        now = backlight_get_abs (manager, error);
        if (now < 0) {
                goto out;
        }

        /* is the dim brightness actually *dimmer* than the
         * brightness we have now? */
        min = backlight_get_min (manager);
        max = backlight_get_max (manager, error);
        if (max < 0) {
                goto out;
        }
        idle = PERCENTAGE_TO_ABS (min, max, idle_percentage);
        if (idle > now) {
                g_debug ("brightness already now %i/%i, so "
                         "ignoring dim to %i/%i",
                         now, max, idle, max);
                ret = TRUE;
                goto out;
        }
        ret = backlight_set_abs (manager,
                                 idle,
                                 FALSE,
                                 error);
        if (!ret) {
                goto out;
        }

        /* save for undim */
        manager->priv->pre_dim_brightness = now;

out:
        return ret;
}

static gboolean
kbd_backlight_dim (CsdPowerManager *manager,
                   gint idle_percentage,
                   GError **error)
{
        gboolean ret;
        gint idle;
        gint max;
        gint now;

        if (manager->priv->upower_kbd_proxy == NULL)
                return TRUE;

        now = manager->priv->kbd_brightness_now;
        max = manager->priv->kbd_brightness_max;
        idle = PERCENTAGE_TO_ABS (0, max, idle_percentage);
        if (idle > now) {
                g_debug ("kbd brightness already now %i/%i, so "
                         "ignoring dim to %i/%i",
                         now, max, idle, max);
                return TRUE;
        }
        ret = upower_kbd_set_brightness (manager, idle, error);
        if (!ret)
                return FALSE;

        /* save for undim */
        manager->priv->kbd_brightness_pre_dim = now;
        return TRUE;
}

static void
idle_set_mode (CsdPowerManager *manager, CsdPowerIdleMode mode)
{
        gboolean ret = FALSE;
        GError *error = NULL;
        gint idle_percentage;
        CsdPowerActionType action_type;
        CinnamonSettingsSessionState state;

        if (mode == manager->priv->current_idle_mode)
                return;

        /* Ignore attempts to set "less idle" modes */
        if (mode < manager->priv->current_idle_mode &&
            mode != CSD_POWER_IDLE_MODE_NORMAL)
                return;

        /* ensure we're still on an active console */
        state = cinnamon_settings_session_get_state (manager->priv->session);
        if (state == CINNAMON_SETTINGS_SESSION_STATE_INACTIVE) {
                g_debug ("ignoring state transition to %s as inactive",
                         idle_mode_to_string (mode));
                return;
        }

        manager->priv->current_idle_mode = mode;
        g_debug ("Doing a state transition: %s", idle_mode_to_string (mode));

        /* don't do any power saving if we're a VM */
        if (manager->priv->is_virtual_machine) {
                g_debug ("ignoring state transition to %s as virtual machine",
                         idle_mode_to_string (mode));
                return;
        }

        /* save current brightness, and set dim level */
        if (mode == CSD_POWER_IDLE_MODE_DIM) {

                /* have we disabled the action */
                if (system_on_battery (manager)) {
                        ret = g_settings_get_boolean (manager->priv->settings,
                                                      "idle-dim-battery");
                } else {
                        ret = g_settings_get_boolean (manager->priv->settings,
                                                      "idle-dim-ac");
                }
                if (!ret) {
                        g_debug ("not dimming due to policy");
                        return;
                }

                /* display backlight */
                idle_percentage = g_settings_get_int (manager->priv->settings,
                                                      "idle-brightness");
                ret = display_backlight_dim (manager, idle_percentage, &error);
                if (!ret) {
                        g_warning ("failed to set dim backlight to %i%%: %s",
                                   idle_percentage,
                                   error->message);
                        g_clear_error (&error);
                }

                /* keyboard backlight */
                ret = kbd_backlight_dim (manager, idle_percentage, &error);
                if (!ret) {
                        g_warning ("failed to set dim kbd backlight to %i%%: %s",
                                   idle_percentage,
                                   error->message);
                        g_clear_error (&error);
                }

        /* turn off screen and kbd */
        } else if (mode == CSD_POWER_IDLE_MODE_BLANK) {

                ret = gnome_rr_screen_set_dpms_mode (manager->priv->x11_screen,
                                                     GNOME_RR_DPMS_OFF,
                                                     &error);
                if (!ret) {
                        g_warning ("failed to turn the panel off: %s",
                                   error->message);
                        g_clear_error (&error);
                }

                /* only toggle keyboard if present and not already toggled */
                if (manager->priv->upower_kbd_proxy &&
                    manager->priv->kbd_brightness_old == -1) {
                        ret = upower_kbd_toggle (manager, &error);
                        if (!ret) {
                                g_warning ("failed to turn the kbd backlight off: %s",
                                           error->message);
                                g_error_free (error);
                        }
                }

        /* sleep */
        } else if (mode == CSD_POWER_IDLE_MODE_SLEEP) {

                if (system_on_battery (manager)) {
                        action_type = g_settings_get_enum (manager->priv->settings,
                                                           "sleep-inactive-battery-type");
                } else {
                        action_type = g_settings_get_enum (manager->priv->settings,
                                                           "sleep-inactive-ac-type");
                }
                do_power_action_type (manager, action_type);

        /* turn on screen and restore user-selected brightness level */
        } else if (mode == CSD_POWER_IDLE_MODE_NORMAL) {

                ret = gnome_rr_screen_set_dpms_mode (manager->priv->x11_screen,
                                                     GNOME_RR_DPMS_ON,
                                                     &error);
                if (!ret) {
                        g_warning ("failed to turn the panel on: %s",
                                   error->message);
                        g_clear_error (&error);
                }

                /* reset brightness if we dimmed */
                if (manager->priv->pre_dim_brightness >= 0) {
                        ret = backlight_set_abs (manager,
                                                 manager->priv->pre_dim_brightness,
                                                 FALSE,
                                                 &error);
                        if (!ret) {
                                g_warning ("failed to restore backlight to %i: %s",
                                           manager->priv->pre_dim_brightness,
                                           error->message);
                                g_clear_error (&error);
                        } else {
                                manager->priv->pre_dim_brightness = -1;
                        }
                }

                /* only toggle keyboard if present and already toggled off */
                if (manager->priv->upower_kbd_proxy &&
                    manager->priv->kbd_brightness_old != -1) {
                        ret = upower_kbd_toggle (manager, &error);
                        if (!ret) {
                                g_warning ("failed to turn the kbd backlight on: %s",
                                           error->message);
                                g_clear_error (&error);
                        }
                }

                /* reset kbd brightness if we dimmed */
                if (manager->priv->kbd_brightness_pre_dim >= 0) {
                        ret = upower_kbd_set_brightness (manager,
                                                         manager->priv->kbd_brightness_pre_dim,
                                                         &error);
                        if (!ret) {
                                g_warning ("failed to restore kbd backlight to %i: %s",
                                           manager->priv->kbd_brightness_pre_dim,
                                           error->message);
                                g_error_free (error);
                        }
                        manager->priv->kbd_brightness_pre_dim = -1;
                }

        }
}

static gboolean
idle_is_session_inhibited (CsdPowerManager *manager, guint mask)
{
        gboolean ret;
        GVariant *retval = NULL;
        GError *error = NULL;

        /* not yet connected to cinnamon-session */
        if (manager->priv->session_proxy == NULL) {
                g_debug ("session inhibition not available, cinnamon-session is not available");
                return FALSE;
        }

        retval = g_dbus_proxy_call_sync (manager->priv->session_proxy,
                                         "IsInhibited",
                                         g_variant_new ("(u)",
                                                        mask),
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1, NULL,
                                         &error);
        if (retval == NULL) {
                /* abort as the DBUS method failed */
                g_warning ("IsInhibited failed: %s", error->message);
                g_error_free (error);
                return FALSE;
        }

        g_variant_get (retval, "(b)", &ret);
        g_variant_unref (retval);

        return ret;
}

/**
 *  idle_adjust_timeout:
 *  @idle_time: Current idle time, in seconds.
 *  @timeout: The new timeout we want to set, in seconds.
 *
 *  On slow machines, or machines that have lots to load duing login,
 *  the current idle time could be bigger than the requested timeout.
 *  In this case the scheduled idle timeout will never fire, unless
 *  some user activity (keyboard, mouse) resets the current idle time.
 *  Instead of relying on user activity to correct this issue, we need
 *  to adjust timeout, as related to current idle time, so the idle
 *  timeout will fire as designed.
 *
 *  Return value: timeout to set, adjusted according to current idle time.
 **/
static guint
idle_adjust_timeout (guint idle_time, guint timeout)
{
        /* allow 2 sec margin for messaging delay. */
        idle_time += 2;

        /* Double timeout until it's larger than current idle time.
         * Give up for ultra slow machines. (86400 sec = 24 hours) */
        while (timeout < idle_time &&
               timeout < 86400 &&
               timeout > 0) {
                timeout *= 2;
        }
        return timeout;
}

/**
 * @timeout: The new timeout we want to set, in seconds
 **/
static void
idle_set_timeout_dim (CsdPowerManager *manager, guint timeout)
{
        guint idle_time;
        gboolean is_idle_inhibited;

        /* are we inhibited from going idle */
        is_idle_inhibited = idle_is_session_inhibited (manager,
                                                       SESSION_INHIBIT_MASK_IDLE);
        if (is_idle_inhibited) {
                g_debug ("inhibited, so using normal state");
                idle_set_mode (manager, CSD_POWER_IDLE_MODE_NORMAL);

                gpm_idletime_alarm_remove (manager->priv->idletime,
                                           CSD_POWER_IDLETIME_DIM_ID);
                return;
        }

        idle_time = gpm_idletime_get_time (manager->priv->idletime) / 1000;

        g_debug ("Setting dim idle timeout: %ds", timeout);
        if (timeout > 0) {
                gpm_idletime_alarm_set (manager->priv->idletime,
                                        CSD_POWER_IDLETIME_DIM_ID,
                                        idle_adjust_timeout (idle_time, timeout) * 1000);
        } else {
                gpm_idletime_alarm_remove (manager->priv->idletime,
                                           CSD_POWER_IDLETIME_DIM_ID);
        }
        return;
}

static void
refresh_idle_dim_settings (CsdPowerManager *manager)
{
        gint timeout_dim;
        timeout_dim = g_settings_get_int (manager->priv->settings,
                                          "idle-dim-time");
        g_debug ("idle dim set with timeout %i", timeout_dim);
        idle_set_timeout_dim (manager, timeout_dim);
}

/**
 * idle_adjust_timeout_blank:
 * @idle_time: current idle time, in seconds.
 * @timeout: the new timeout we want to set, in seconds.
 *
 * Same as idle_adjust_timeout(), but also accounts for the duration
 * of the fading animation in the screensaver (so that blanking happens
 * exactly at the end of it, if configured with the same timeouts)
 */
static guint
idle_adjust_timeout_blank (guint idle_time, guint timeout)
{
        return idle_adjust_timeout (idle_time,
                                    timeout + SCREENSAVER_FADE_TIME);
}

static void
idle_configure (CsdPowerManager *manager)
{
        gboolean is_idle_inhibited;
        guint current_idle_time;
        guint timeout_lock;
        guint timeout_blank;
        guint timeout_sleep;
        gboolean on_battery;

        /* are we inhibited from going idle */
        is_idle_inhibited = idle_is_session_inhibited (manager,
                                                       SESSION_INHIBIT_MASK_IDLE);
        if (is_idle_inhibited) {
                g_debug ("inhibited, so using normal state");
                idle_set_mode (manager, CSD_POWER_IDLE_MODE_NORMAL);

                gpm_idletime_alarm_remove (manager->priv->idletime,
                                           CSD_POWER_IDLETIME_LOCK_ID);
                gpm_idletime_alarm_remove (manager->priv->idletime,
                                           CSD_POWER_IDLETIME_BLANK_ID);
                gpm_idletime_alarm_remove (manager->priv->idletime,
                                           CSD_POWER_IDLETIME_SLEEP_ID);

                refresh_idle_dim_settings (manager);
                return;
        }

        current_idle_time = gpm_idletime_get_time (manager->priv->idletime) / 1000;

        /* set up blank callback even when session is not idle,
         * but only if we actually want to blank. */
        on_battery = system_on_battery (manager);
        if (on_battery) {
                timeout_blank = g_settings_get_int (manager->priv->settings,
                                                    "sleep-display-battery");
        } else {
                timeout_blank = g_settings_get_int (manager->priv->settings,
                                                    "sleep-display-ac");
        }

        /* set up custom screensaver lock after idle time trigger */
        timeout_lock = g_settings_get_uint (manager->priv->settings_desktop_session,
                                            "idle-delay");
        if (timeout_lock != 0) {
                if (timeout_blank != 0 && timeout_lock > timeout_blank) {
                        g_debug ("reducing lock timeout to match blank timeout");
                        timeout_lock = timeout_blank;
                }
                g_debug ("setting up lock callback for %is", timeout_lock);

                gpm_idletime_alarm_set (manager->priv->idletime,
                                        CSD_POWER_IDLETIME_LOCK_ID,
                                        idle_adjust_timeout (current_idle_time, timeout_lock) * 1000);
        } else {
                gpm_idletime_alarm_remove (manager->priv->idletime,
                                           CSD_POWER_IDLETIME_LOCK_ID);
        }

        if (timeout_blank != 0) {
                g_debug ("setting up blank callback for %is", timeout_blank);

                gpm_idletime_alarm_set (manager->priv->idletime,
                                        CSD_POWER_IDLETIME_BLANK_ID,
                                        idle_adjust_timeout_blank (current_idle_time, timeout_blank) * 1000);
        } else {
                gpm_idletime_alarm_remove (manager->priv->idletime,
                                           CSD_POWER_IDLETIME_BLANK_ID);
        }

        gboolean is_sleep_inhibited = idle_is_session_inhibited (manager,
                                                                 SESSION_INHIBIT_MASK_SUSPEND);
        /* only do the sleep timeout when the session is idle
         * and we aren't inhibited from sleeping */
        if (on_battery) {
                timeout_sleep = g_settings_get_int (manager->priv->settings,
                                                    "sleep-inactive-battery-timeout");
        } else {
                timeout_sleep = g_settings_get_int (manager->priv->settings,
                                                    "sleep-inactive-ac-timeout");
        }
        if (!is_sleep_inhibited && timeout_sleep != 0) {
                g_debug ("setting up sleep callback %is", timeout_sleep);

                gpm_idletime_alarm_set (manager->priv->idletime,
                                        CSD_POWER_IDLETIME_SLEEP_ID,
                                        idle_adjust_timeout (current_idle_time, timeout_sleep) * 1000);
        } else {
                gpm_idletime_alarm_remove (manager->priv->idletime,
                                           CSD_POWER_IDLETIME_SLEEP_ID);
        }

        refresh_idle_dim_settings (manager);
}

#if UP_CHECK_VERSION(0,99,0)
static void
up_client_on_battery_cb (UpClient *client,
                         GParamSpec *pspec,
                         CsdPowerManager *manager)
{
        idle_configure (manager);
        lid_state_changed_cb(client, pspec, manager);
}
#endif

static void
csd_power_manager_class_init (CsdPowerManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = csd_power_manager_finalize;

        g_type_class_add_private (klass, sizeof (CsdPowerManagerPrivate));
}

static void
setup_locker_process (gpointer user_data)
{
        /* This function should only contain signal safe code, as it is invoked
         * between fork and exec. See signal-safety(7) for more information. */
        CsdPowerManager *manager = user_data;

        /* close all FDs except stdin, stdout, stderr and the inhibition fd */
        for (gint fd = 3; fd < manager->priv->fd_close_loop_end; fd++)
                if (fd != manager->priv->inhibit_suspend_fd)
                        close (fd);

        /* make sure the inhibit fd does not get closed on exec, as it's options
         * are not specified in the logind inhibitor interface documentation. */
        if (-1 != manager->priv->inhibit_suspend_fd)
                fcntl (manager->priv->inhibit_suspend_fd,
                       F_SETFD,
                       ~FD_CLOEXEC & fcntl (manager->priv->inhibit_suspend_fd, F_GETFD));
}

static void
lock_screen_with_custom_saver (CsdPowerManager *manager,
                               gchar *custom_saver,
                               gboolean idle_lock)
{
        gboolean res;
        gchar *fd = NULL;
        gchar **argv = NULL;
        gchar **env = NULL;
        GError *error = NULL;

        /* environment setup */
        fd = g_strdup_printf ("%d", manager->priv->inhibit_suspend_fd);
        if (!fd) {
                g_warning ("failed to printf inhibit_suspend_fd");
                goto quit;
        }
        if (!(env = g_get_environ ())) {
                g_warning ("failed to get environment");
                goto quit;
        }
        env = g_environ_setenv (env, "XSS_SLEEP_LOCK_FD", fd, FALSE);
        if (!env) {
                g_warning ("failed to set XSS_SLEEP_LOCK_FD");
                goto quit;
        }
        env = g_environ_setenv (env,
                                "LOCKED_BY_SESSION_IDLE",
                                idle_lock ? "true" : "false",
                                TRUE);
        if (!env) {
                g_warning ("failed to set LOCKED_BY_SESSION_IDLE");
                goto quit;
        }

        /* argv setup */
        res = g_shell_parse_argv (custom_saver, NULL, &argv, &error);
        if (!res) {
                g_warning ("failed to parse custom saver cmd '%s': %s",
                           custom_saver,
                           error->message);
                goto quit;
        }

        /* get the max number of open file descriptors */
        manager->priv->fd_close_loop_end = sysconf (_SC_OPEN_MAX);
        if (-1 == manager->priv->fd_close_loop_end)
                /* use some sane default */
                manager->priv->fd_close_loop_end = 32768;

        /* spawn the custom screen locker */
        res = g_spawn_async (NULL,
                             argv,
                             env,
                             G_SPAWN_LEAVE_DESCRIPTORS_OPEN | G_SPAWN_SEARCH_PATH,
                             &setup_locker_process,
                             manager,
                             NULL,
                             &error);
        if (!res)
                g_warning ("failed to run custom screensaver '%s': %s",
                           custom_saver,
                           error->message);

quit:
        g_free (fd);
        g_strfreev (argv);
        g_strfreev (env);
        g_clear_error (&error);
}

static void
activate_screensaver (CsdPowerManager *manager, gboolean force_lock)
{
    GError *error;
    gboolean ret;
    gchar *custom_saver = g_settings_get_string (manager->priv->settings_screensaver,
                                                 "custom-screensaver-command");

    g_debug ("Locking screen before sleep/hibernate");

    if (custom_saver && g_strcmp0 (custom_saver, "") != 0) {
            lock_screen_with_custom_saver (manager, custom_saver, FALSE);
            goto quit;
    }

    /* if we fail to get the gsettings entry, or if the user did not select
     * a custom screen saver, default to invoking cinnamon-screensaver */
    /* do this sync to ensure it's on the screen when we start suspending */
    error = NULL;

    if (force_lock) {
        ret = g_spawn_command_line_sync ("cinnamon-screensaver-command --lock", NULL, NULL, NULL, &error);
    } else {
        ret = g_spawn_command_line_sync ("cinnamon-screensaver-command -a", NULL, NULL, NULL, &error);
    }

    if (!ret) {
        g_warning ("Couldn't lock screen: %s", error->message);
        g_error_free (error);
    }

quit:
    g_free (custom_saver);
}

static void
idle_dbus_signal_cb (GDBusProxy *proxy,
                     const gchar *sender_name,
                     const gchar *signal_name,
                     GVariant *parameters,
                     gpointer user_data)
{
        CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);

        if (g_strcmp0 (signal_name, "InhibitorAdded") == 0 ||
            g_strcmp0 (signal_name, "InhibitorRemoved") == 0) {
                g_debug ("Received gnome session inhibitor change");
                idle_configure (manager);
        }
        if (g_strcmp0 (signal_name, "StatusChanged") == 0) {
                guint status;

                g_variant_get (parameters, "(u)", &status);
                g_dbus_proxy_set_cached_property (proxy, "status",
                                                  g_variant_new ("u", status));
                g_debug ("Received gnome session status change");
                idle_configure (manager);
        }
}

static void
session_proxy_ready_cb (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
        GError *error = NULL;
        CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);

        manager->priv->session_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (manager->priv->session_proxy == NULL) {
                g_warning ("Could not connect to cinnamon-session: %s",
                           error->message);
                g_error_free (error);
        } else {
                g_signal_connect (manager->priv->session_proxy, "g-signal",
                                  G_CALLBACK (idle_dbus_signal_cb), manager);
        }

        idle_configure (manager);
}

static void
session_presence_proxy_ready_cb (GObject *source_object,
                                 GAsyncResult *res,
                                 gpointer user_data)
{
        GError *error = NULL;
        CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);

        manager->priv->session_presence_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (manager->priv->session_presence_proxy == NULL) {
                g_warning ("Could not connect to gnome-sesson: %s",
                           error->message);
                g_error_free (error);
                return;
        }
        g_signal_connect (manager->priv->session_presence_proxy, "g-signal",
                          G_CALLBACK (idle_dbus_signal_cb), manager);
}

static void
power_keyboard_proxy_ready_cb (GObject             *source_object,
                               GAsyncResult        *res,
                               gpointer             user_data)
{
        GVariant *k_now = NULL;
        GVariant *k_max = NULL;
        GError *error = NULL;
        CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);

        manager->priv->upower_kbd_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (manager->priv->upower_kbd_proxy == NULL) {
                g_warning ("Could not connect to UPower: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        k_now = g_dbus_proxy_call_sync (manager->priv->upower_kbd_proxy,
                                        "GetBrightness",
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
        if (k_now == NULL) {
                if (error->domain != G_DBUS_ERROR ||
                    error->code != G_DBUS_ERROR_UNKNOWN_METHOD) {
                        g_warning ("Failed to get brightness: %s",
                                   error->message);
                }
                g_error_free (error);
                goto out;
        }

        k_max = g_dbus_proxy_call_sync (manager->priv->upower_kbd_proxy,
                                        "GetMaxBrightness",
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
        if (k_max == NULL) {
                g_warning ("Failed to get max brightness: %s", error->message);
                g_error_free (error);
                goto out;
        }

        g_signal_connect (manager->priv->upower_kbd_proxy, "g-signal", G_CALLBACK(upower_kbd_handle_changed), manager);

        g_variant_get (k_now, "(i)", &manager->priv->kbd_brightness_now);
        g_variant_get (k_max, "(i)", &manager->priv->kbd_brightness_max);

        /* Set keyboard brightness to zero if the current value is out of valid range.
        Unlike display brightness, keyboard backlight brightness should be dim by default.*/
        if ((manager->priv->kbd_brightness_now  < 0) || (manager->priv->kbd_brightness_now > manager->priv->kbd_brightness_max)) {
                gboolean ret;
                ret = upower_kbd_set_brightness (manager,
                                                 0,
                                                 &error);
                if (!ret) {
                        g_warning ("failed to initialize kbd backlight to %i: %s",
                                   0,
                                   error->message);
                        g_error_free (error);
                }
        }
out:
        if (k_now != NULL)
                g_variant_unref (k_now);
        if (k_max != NULL)
                g_variant_unref (k_max);
}

static void
idle_idletime_alarm_expired_cb (GpmIdletime *idletime,
                                guint alarm_id,
                                CsdPowerManager *manager)
{
        g_debug ("idletime alarm: %i", alarm_id);

        switch (alarm_id) {
        case CSD_POWER_IDLETIME_DIM_ID:
                idle_set_mode (manager, CSD_POWER_IDLE_MODE_DIM);
                break;
        case CSD_POWER_IDLETIME_LOCK_ID:
                ; /* empty statement, because C does not allow a declaration to
                   * follow a label */
                gchar *custom_saver = g_settings_get_string (manager->priv->settings_screensaver,
                                                             "custom-screensaver-command");
                if (custom_saver && g_strcmp0 (custom_saver, "") != 0) {
                        lock_screen_with_custom_saver (manager,
                                                       custom_saver,
                                                       TRUE);
                } else {
                    activate_screensaver (manager, FALSE);
                }

                g_free (custom_saver);

                break;
        case CSD_POWER_IDLETIME_BLANK_ID:
                idle_set_mode (manager, CSD_POWER_IDLE_MODE_BLANK);
                break;
        case CSD_POWER_IDLETIME_SLEEP_ID:
                idle_set_mode (manager, CSD_POWER_IDLE_MODE_SLEEP);
                break;
        }
}

static void
idle_idletime_reset_cb (GpmIdletime *idletime,
                        CsdPowerManager *manager)
{
        g_debug ("idletime reset");

        idle_set_mode (manager, CSD_POWER_IDLE_MODE_NORMAL);
}

static void
engine_settings_key_changed_cb (GSettings *settings,
                                const gchar *key,
                                CsdPowerManager *manager)
{
        /* note: you *have* to check if your key was changed here before
         * doing anything here. this gets invoked on module stop, and
         * will crash c-s-d if you don't. */
        if (g_strcmp0 (key, "use-time-for-policy") == 0) {
                manager->priv->use_time_primary = g_settings_get_boolean (settings, key);
                return;
        }
        if (g_strcmp0 (key, "idle-dim-time") == 0) {
                refresh_idle_dim_settings (manager);
                return;
        }
        if (g_str_has_prefix (key, "sleep-inactive") ||
            g_str_has_prefix (key, "sleep-display") ||
            g_str_has_prefix (key, "idle-delay")) {
                idle_configure (manager);
                return;
        }

        if (g_str_has_prefix (key, "backlight-helper")) {
                backlight_override_settings_refresh (manager);
                return;
        }

        if (g_str_has_prefix (key, "power-notifications")) {
                refresh_notification_settings (manager);
                return;
        }
}

static void
engine_session_active_changed_cb (CinnamonSettingsSession *session,
                                  GParamSpec *pspec,
                                  CsdPowerManager *manager)
{
        /* when doing the fast-user-switch into a new account,
         * ensure the new account is undimmed and with the backlight on */
        idle_set_mode (manager, CSD_POWER_IDLE_MODE_NORMAL);
}

/* This timer goes off every few minutes, whether the user is idle or not,
   to try and clean up anything that has gone wrong.

   It calls disable_builtin_screensaver() so that if xset has been used,
   or some other program (like xlock) has messed with the XSetScreenSaver()
   settings, they will be set back to sensible values (if a server extension
   is in use, messing with xlock can cause the screensaver to never get a wakeup
   event, and could cause monitor power-saving to occur, and all manner of
   heinousness.)

   This code was originally part of cinnamon-screensaver, see
   http://git.gnome.org/browse/cinnamon-screensaver/tree/src/gs-watcher-x11.c?id=fec00b12ec46c86334cfd36b37771cc4632f0d4d#n530
 */
static gboolean
disable_builtin_screensaver (gpointer unused)
{
        int current_server_timeout, current_server_interval;
        int current_prefer_blank,   current_allow_exp;
        int desired_server_timeout, desired_server_interval;
        int desired_prefer_blank,   desired_allow_exp;

        XGetScreenSaver (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                         &current_server_timeout,
                         &current_server_interval,
                         &current_prefer_blank,
                         &current_allow_exp);

        desired_server_timeout  = current_server_timeout;
        desired_server_interval = current_server_interval;
        desired_prefer_blank    = current_prefer_blank;
        desired_allow_exp       = current_allow_exp;

        desired_server_interval = 0;

        /* I suspect (but am not sure) that DontAllowExposures might have
           something to do with powering off the monitor as well, at least
           on some systems that don't support XDPMS?  Who know... */
        desired_allow_exp = AllowExposures;

        /* When we're not using an extension, set the server-side timeout to 0,
           so that the server never gets involved with screen blanking, and we
           do it all ourselves.  (However, when we *are* using an extension,
           we tell the server when to notify us, and rather than blanking the
           screen, the server will send us an X event telling us to blank.)
        */
        desired_server_timeout = 0;

        if (desired_server_timeout     != current_server_timeout
            || desired_server_interval != current_server_interval
            || desired_prefer_blank    != current_prefer_blank
            || desired_allow_exp       != current_allow_exp) {

                g_debug ("disabling server builtin screensaver:"
                         " (xset s %d %d; xset s %s; xset s %s)",
                         desired_server_timeout,
                         desired_server_interval,
                         (desired_prefer_blank ? "blank" : "noblank"),
                         (desired_allow_exp ? "expose" : "noexpose"));

                XSetScreenSaver (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                 desired_server_timeout,
                                 desired_server_interval,
                                 desired_prefer_blank,
                                 desired_allow_exp);

                XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), FALSE);
        }

        return TRUE;
}

static void
inhibit_lid_switch_done (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
        GDBusProxy *proxy = G_DBUS_PROXY (source);
        CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);
        GError *error = NULL;
        GVariant *res;
        GUnixFDList *fd_list = NULL;
        gint idx;

        res = g_dbus_proxy_call_with_unix_fd_list_finish (proxy, &fd_list, result, &error);
        if (res == NULL) {
                g_warning ("Unable to inhibit lid switch: %s", error->message);
                g_error_free (error);
        } else {
                g_variant_get (res, "(h)", &idx);
                manager->priv->inhibit_lid_switch_fd = g_unix_fd_list_get (fd_list, idx, &error);
                if (manager->priv->inhibit_lid_switch_fd == -1) {
                        g_warning ("Failed to receive system inhibitor fd: %s", error->message);
                        g_error_free (error);
                }
                g_debug ("System inhibitor fd is %d", manager->priv->inhibit_lid_switch_fd);
                g_object_unref (fd_list);
                g_variant_unref (res);
        }
}

static void
inhibit_lid_switch (CsdPowerManager *manager)
{
        if (!manager->priv->inhibit_lid_switch_enabled)  {
                // The users asks us not to interfere with what logind does
                // w.r.t. handling the lid switch
                g_debug ("inhibiting lid-switch disabled");
                return;
        }

        GVariant *params;

        if (manager->priv->inhibit_lid_switch_taken) {
                g_debug ("already inhibited lid-switch");
                return;
        }
        g_debug ("Adding lid switch system inhibitor");
        manager->priv->inhibit_lid_switch_taken = TRUE;

        params = g_variant_new ("(ssss)",
                                "handle-lid-switch",
                                g_get_user_name (),
                                "Multiple displays attached",
                                "block");
        g_dbus_proxy_call_with_unix_fd_list (manager->priv->logind_proxy,
                                             "Inhibit",
                                             params,
                                             0,
                                             G_MAXINT,
                                             NULL,
                                             NULL,
                                             inhibit_lid_switch_done,
                                             manager);
}

static void
uninhibit_lid_switch (CsdPowerManager *manager)
{
        if (manager->priv->inhibit_lid_switch_fd == -1) {
                g_debug ("no lid-switch inhibitor");
                return;
        }
        g_debug ("Removing lid switch system inhibitor");
        close (manager->priv->inhibit_lid_switch_fd);
        manager->priv->inhibit_lid_switch_fd = -1;
        manager->priv->inhibit_lid_switch_taken = FALSE;
}


static void
inhibit_suspend_done (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
        GDBusProxy *proxy = G_DBUS_PROXY (source);
        CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);
        GError *error = NULL;
        GVariant *res;
        GUnixFDList *fd_list = NULL;
        gint idx;

        res = g_dbus_proxy_call_with_unix_fd_list_finish (proxy, &fd_list, result, &error);
        if (res == NULL) {
                g_warning ("Unable to inhibit suspend: %s", error->message);
                g_error_free (error);
        } else {
                g_variant_get (res, "(h)", &idx);
                manager->priv->inhibit_suspend_fd = g_unix_fd_list_get (fd_list, idx, &error);
                if (manager->priv->inhibit_suspend_fd == -1) {
                        g_warning ("Failed to receive system inhibitor fd: %s", error->message);
                        g_error_free (error);
                }
                g_debug ("System inhibitor fd is %d", manager->priv->inhibit_suspend_fd);
                g_object_unref (fd_list);
                g_variant_unref (res);
        }
}

/* We take a delay inhibitor here, which causes logind to send a
 * PrepareToSleep signal, which gives us a chance to lock the screen
 * and do some other preparations.
 */
static void
inhibit_suspend (CsdPowerManager *manager)
{
        if (manager->priv->inhibit_suspend_taken) {
                g_debug ("already inhibited lid-switch");
                return;
        }
        g_debug ("Adding suspend delay inhibitor");
        manager->priv->inhibit_suspend_taken = TRUE;
        g_dbus_proxy_call_with_unix_fd_list (manager->priv->logind_proxy,
                                             "Inhibit",
                                             g_variant_new ("(ssss)",
                                                            "sleep",
                                                            g_get_user_name (),
                                                            "Cinnamon needs to lock the screen",
                                                            "delay"),
                                             0,
                                             G_MAXINT,
                                             NULL,
                                             NULL,
                                             inhibit_suspend_done,
                                             manager);
}

static void
uninhibit_suspend (CsdPowerManager *manager)
{
        if (manager->priv->inhibit_suspend_fd == -1) {
                g_debug ("no suspend delay inhibitor");
                return;
        }
        g_debug ("Removing suspend delay inhibitor");
        close (manager->priv->inhibit_suspend_fd);
        manager->priv->inhibit_suspend_fd = -1;
        manager->priv->inhibit_suspend_taken = FALSE;
}

static void
handle_suspend_actions (CsdPowerManager *manager)
{
        /* Is this even necessary? We lock ahead of the suspend initiation,
         * during do_power_action_type().  This is a signal from logind or
         * upower that we're about to suspend.  That may have originated in
         * this module, or elsewhere (cinnamon-session via menu or user
         * applet.  Lock is handled there as well... but just in case I
         * suppose.)
         */
        if (should_lock_on_suspend (manager)) {
            activate_screensaver (manager, TRUE);
        }

        /* lift the delay inhibit, so logind can proceed */
        uninhibit_suspend (manager);
}

static void
handle_resume_actions (CsdPowerManager *manager)
{
        gboolean ret;
        GError *error = NULL;

        /* this displays the unlock dialogue so the user doesn't have
         * to move the mouse or press any key before the window comes up */
        g_dbus_connection_call (manager->priv->connection,
                                GS_DBUS_NAME,
                                GS_DBUS_PATH,
                                GS_DBUS_INTERFACE,
                                "SimulateUserActivity",
                                NULL, NULL,
                                G_DBUS_CALL_FLAGS_NONE, -1,
                                NULL, NULL, NULL);

        /* close existing notifications on resume, the system power
         * state is probably different now */
        notify_close_if_showing (manager->priv->notification_low);
        notify_close_if_showing (manager->priv->notification_discharging);

        /* ensure we turn the panel back on after resume */
        ret = gnome_rr_screen_set_dpms_mode (manager->priv->x11_screen,
                                             GNOME_RR_DPMS_ON,
                                             &error);
        if (!ret) {
                g_warning ("failed to turn the panel on after resume: %s",
                           error->message);
                g_error_free (error);
        }

        /* set up the delay again */
        inhibit_suspend (manager);
}

#if ! UP_CHECK_VERSION(0,99,0)
static void
upower_notify_sleep_cb (UpClient *client,
                        UpSleepKind sleep_kind,
                        CsdPowerManager *manager)
{
        handle_suspend_actions (manager);
}

static void
upower_notify_resume_cb (UpClient *client,
                         UpSleepKind sleep_kind,
                         CsdPowerManager *manager)
{
        handle_resume_actions (manager);
}
#endif

static void
logind_proxy_signal_cb (GDBusProxy  *proxy,
                        const gchar *sender_name,
                        const gchar *signal_name,
                        GVariant    *parameters,
                        gpointer     user_data)
{
        CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);
        gboolean is_about_to_suspend;

        if (g_strcmp0 (signal_name, "PrepareForSleep") != 0)
                return;
        g_variant_get (parameters, "(b)", &is_about_to_suspend);
        if (is_about_to_suspend) {
                handle_suspend_actions (manager);
        } else {
                handle_resume_actions (manager);
        }
}

static gboolean
is_hardware_a_virtual_machine (void)
{
        const gchar *str;
        gboolean ret = FALSE;
        GError *error = NULL;
        GVariant *inner;
        GVariant *variant = NULL;
        GDBusConnection *connection;

        connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM,
                                     NULL,
                                     &error);
        if (connection == NULL) {
                g_warning ("system bus not available: %s", error->message);
                g_error_free (error);
                goto out;
        }
        variant = g_dbus_connection_call_sync (connection,
                                               "org.freedesktop.systemd1",
                                               "/org/freedesktop/systemd1",
                                               "org.freedesktop.DBus.Properties",
                                               "Get",
                                               g_variant_new ("(ss)",
                                                              "org.freedesktop.systemd1.Manager",
                                                              "Virtualization"),
                                               NULL,
                                               G_DBUS_CALL_FLAGS_NONE,
                                               -1,
                                               NULL,
                                               &error);
        if (variant == NULL) {
                g_debug ("Failed to get property '%s': %s", "Virtualization", error->message);
                g_error_free (error);
                goto out;
        }

        /* on bare-metal hardware this is the empty string,
         * otherwise an identifier such as "kvm", "vmware", etc. */
        g_variant_get (variant, "(v)", &inner);
        str = g_variant_get_string (inner, NULL);
        if (str != NULL && str[0] != '\0')
                ret = TRUE;
out:
        if (connection != NULL)
                g_object_unref (connection);
        if (variant != NULL)
                g_variant_unref (variant);
        return ret;
}

gboolean
csd_power_manager_start (CsdPowerManager *manager,
                         GError **error)
{
        gboolean ret;

        g_debug ("Starting power manager");
        cinnamon_settings_profile_start (NULL);

        /* coldplug the list of screens */
        manager->priv->x11_screen = gnome_rr_screen_new (gdk_screen_get_default (), error);
        if (manager->priv->x11_screen == NULL)
                return FALSE;

        /* Set up the logind proxy */
        manager->priv->logind_proxy =
                g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                               0,
                                               NULL,
                                               LOGIND_DBUS_NAME,
                                               LOGIND_DBUS_PATH,
                                               LOGIND_DBUS_INTERFACE,
                                               NULL,
                                               error);
        g_signal_connect (manager->priv->logind_proxy, "g-signal",
                          G_CALLBACK (logind_proxy_signal_cb),
                          manager);

        /* Set up a delay inhibitor to be informed about suspend attempts */
        inhibit_suspend (manager);

        /* track the active session */
        manager->priv->session = cinnamon_settings_session_new ();
        g_signal_connect (manager->priv->session, "notify::state",
                          G_CALLBACK (engine_session_active_changed_cb),
                          manager);

        manager->priv->kbd_brightness_old = -1;
        manager->priv->kbd_brightness_pre_dim = -1;
        manager->priv->pre_dim_brightness = -1;
        manager->priv->settings = g_settings_new (CSD_POWER_SETTINGS_SCHEMA);
        g_signal_connect (manager->priv->settings, "changed",
                          G_CALLBACK (engine_settings_key_changed_cb), manager);
        manager->priv->settings_screensaver = g_settings_new (CSD_SAVER_SETTINGS_SCHEMA);
        manager->priv->settings_xrandr = g_settings_new (CSD_XRANDR_SETTINGS_SCHEMA);
        manager->priv->settings_desktop_session = g_settings_new (CSD_SESSION_SETTINGS_SCHEMA);
        g_signal_connect (manager->priv->settings_desktop_session, "changed",
                          G_CALLBACK (engine_settings_key_changed_cb), manager);
        manager->priv->settings_cinnamon_session = g_settings_new (CSD_CINNAMON_SESSION_SCHEMA);
        manager->priv->inhibit_lid_switch_enabled =
                          g_settings_get_boolean (manager->priv->settings, "inhibit-lid-switch");

        /* Disable logind's lid handling while g-s-d is active */
        inhibit_lid_switch (manager);

        manager->priv->up_client = up_client_new ();
#if ! UP_CHECK_VERSION(0,99,0)
        g_signal_connect (manager->priv->up_client, "notify-sleep",
                          G_CALLBACK (upower_notify_sleep_cb), manager);
        g_signal_connect (manager->priv->up_client, "notify-resume",
                          G_CALLBACK (upower_notify_resume_cb), manager);
#endif
        manager->priv->lid_is_closed = up_client_get_lid_is_closed (manager->priv->up_client);
        manager->priv->on_battery = up_client_get_on_battery(manager->priv->up_client);
        g_signal_connect (manager->priv->up_client, "device-added",
                          G_CALLBACK (engine_device_added_cb), manager);
        g_signal_connect (manager->priv->up_client, "device-removed",
                          G_CALLBACK (engine_device_removed_cb), manager);
#if UP_CHECK_VERSION(0,99,0)
        g_signal_connect_after (manager->priv->up_client, "notify::lid-is-closed",
                                G_CALLBACK (lid_state_changed_cb), manager);

        g_signal_connect (manager->priv->up_client, "notify::on-battery",
                          G_CALLBACK (up_client_on_battery_cb), manager);
#else
        g_signal_connect (manager->priv->up_client, "device-changed",
                          G_CALLBACK (engine_device_changed_cb), manager);
        g_signal_connect_after (manager->priv->up_client, "changed",
                                G_CALLBACK (up_client_changed_cb), manager);
#endif

        /* use the fallback name from gnome-power-manager so the shell
         * blocks this, and uses the power extension instead */
        manager->priv->status_icon = gtk_status_icon_new ();
        gtk_status_icon_set_name (manager->priv->status_icon,
                                  "gnome-power-manager");
        /* TRANSLATORS: this is the title of the power manager status icon
         * that is only shown in fallback mode */
        gtk_status_icon_set_title (manager->priv->status_icon, _("Power Manager"));
        gtk_status_icon_set_visible (manager->priv->status_icon, FALSE);

        /* connect to UPower for keyboard backlight control */
        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                  NULL,
                                  UPOWER_DBUS_NAME,
                                  UPOWER_DBUS_PATH_KBDBACKLIGHT,
                                  UPOWER_DBUS_INTERFACE_KBDBACKLIGHT,
                                  NULL,
                                  power_keyboard_proxy_ready_cb,
                                  manager);

        /* connect to the session */
        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                  NULL,
                                  GNOME_SESSION_DBUS_NAME,
                                  GNOME_SESSION_DBUS_PATH,
                                  GNOME_SESSION_DBUS_INTERFACE,
                                  NULL,
                                  session_proxy_ready_cb,
                                  manager);
        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  0,
                                  NULL,
                                  GNOME_SESSION_DBUS_NAME,
                                  GNOME_SESSION_DBUS_PATH_PRESENCE,
                                  GNOME_SESSION_DBUS_INTERFACE_PRESENCE,
                                  NULL,
                                  session_presence_proxy_ready_cb,
                                  manager);

        manager->priv->devices_array = g_ptr_array_new_with_free_func (g_object_unref);
        manager->priv->canberra_context = ca_gtk_context_get_for_screen (gdk_screen_get_default ());

        manager->priv->phone = gpm_phone_new ();
        g_signal_connect (manager->priv->phone, "device-added",
                          G_CALLBACK (phone_device_added_cb), manager);
        g_signal_connect (manager->priv->phone, "device-removed",
                          G_CALLBACK (phone_device_removed_cb), manager);
        g_signal_connect (manager->priv->phone, "device-refresh",
                          G_CALLBACK (phone_device_refresh_cb), manager);

        /* create a fake virtual composite battery */
        manager->priv->device_composite = up_device_new ();
        g_object_set (manager->priv->device_composite,
                      "kind", UP_DEVICE_KIND_BATTERY,
                      "is-rechargeable", TRUE,
                      "native-path", "dummy:composite_battery",
                      "power-supply", TRUE,
                      "is-present", TRUE,
                      NULL);

        /* get backlight setting overrides */
        manager->priv->backlight_helper_preference_args = NULL;
        backlight_override_settings_refresh (manager);

        /* get percentage policy */
        manager->priv->low_percentage = g_settings_get_int (manager->priv->settings,
                                                            "percentage-low");
        manager->priv->critical_percentage = g_settings_get_int (manager->priv->settings,
                                                                 "percentage-critical");
        manager->priv->action_percentage = g_settings_get_int (manager->priv->settings,
                                                               "percentage-action");

        /* get time policy */
        manager->priv->low_time = g_settings_get_int (manager->priv->settings,
                                                      "time-low");
        manager->priv->critical_time = g_settings_get_int (manager->priv->settings,
                                                           "time-critical");
        manager->priv->action_time = g_settings_get_int (manager->priv->settings,
                                                         "time-action");

        /* we can disable this if the time remaining is inaccurate or just plain wrong */
        manager->priv->use_time_primary = g_settings_get_boolean (manager->priv->settings,
                                                                  "use-time-for-policy");

        refresh_notification_settings (manager);

        /* create IDLETIME watcher */
        manager->priv->idletime = gpm_idletime_new ();
        g_signal_connect (manager->priv->idletime, "reset",
                          G_CALLBACK (idle_idletime_reset_cb), manager);
        g_signal_connect (manager->priv->idletime, "alarm-expired",
                          G_CALLBACK (idle_idletime_alarm_expired_cb), manager);

        /* set up the screens */
        g_signal_connect (manager->priv->x11_screen, "changed", G_CALLBACK (on_randr_event), manager);
        on_randr_event (manager->priv->x11_screen, manager);

        /* ensure the default dpms timeouts are cleared */
        ret = gnome_rr_screen_set_dpms_mode (manager->priv->x11_screen,
                                             GNOME_RR_DPMS_ON,
                                             error);
        if (!ret) {
                g_warning ("Failed set DPMS mode: %s", (*error)->message);
                g_clear_error (error);
        }

        /* coldplug the engine */
        engine_coldplug (manager);

        /* Make sure that Xorg's DPMS extension never gets in our way. The defaults seem to have changed in Xorg 1.14
         * being "0" by default to being "600" by default 
         * https://bugzilla.gnome.org/show_bug.cgi?id=709114
         */
        gdk_x11_display_error_trap_push (gdk_display_get_default ());
        int dummy;
        if (DPMSQueryExtension(GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), &dummy, &dummy)) {
            DPMSSetTimeouts (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), 0, 0, 0);
        }
        gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());

        manager->priv->xscreensaver_watchdog_timer_id = g_timeout_add_seconds (XSCREENSAVER_WATCHDOG_TIMEOUT,
                                                                               disable_builtin_screensaver,
                                                                               NULL);
        /* don't blank inside a VM */
        manager->priv->is_virtual_machine = is_hardware_a_virtual_machine ();

        cinnamon_settings_profile_end (NULL);
        return TRUE;
}

void
csd_power_manager_stop (CsdPowerManager *manager)
{
        g_debug ("Stopping power manager");

        if (manager->priv->bus_cancellable != NULL) {
                g_cancellable_cancel (manager->priv->bus_cancellable);
                g_object_unref (manager->priv->bus_cancellable);
                manager->priv->bus_cancellable = NULL;
        }

        kill_lid_close_safety_timer (manager);

        g_signal_handlers_disconnect_by_data (manager->priv->up_client, manager);

        if (manager->priv->connection != NULL) {
                g_object_unref (manager->priv->connection);
                manager->priv->connection = NULL;
        }

        if (manager->priv->session != NULL) {
                g_object_unref (manager->priv->session);
                manager->priv->session = NULL;
        }

        if (manager->priv->settings != NULL) {
                g_object_unref (manager->priv->settings);
                manager->priv->settings = NULL;
        }

        if (manager->priv->settings_screensaver != NULL) {
                g_object_unref (manager->priv->settings_screensaver);
                manager->priv->settings_screensaver = NULL;
        }

        if (manager->priv->settings_xrandr != NULL) {
                g_object_unref (manager->priv->settings_xrandr);
                manager->priv->settings_xrandr = NULL;
        }

        if (manager->priv->settings_desktop_session != NULL) {
                g_object_unref (manager->priv->settings_desktop_session);
                manager->priv->settings_desktop_session = NULL;
        }

        if (manager->priv->settings_cinnamon_session != NULL) {
                g_object_unref (manager->priv->settings_cinnamon_session);
                manager->priv->settings_cinnamon_session = NULL;
        }

        if (manager->priv->up_client != NULL) {
                g_object_unref (manager->priv->up_client);
                manager->priv->up_client = NULL;
        }

        if (manager->priv->inhibit_lid_switch_fd != -1) {
                close (manager->priv->inhibit_lid_switch_fd);
                manager->priv->inhibit_lid_switch_fd = -1;
                manager->priv->inhibit_lid_switch_taken = FALSE;
        }
        if (manager->priv->inhibit_suspend_fd != -1) {
                close (manager->priv->inhibit_suspend_fd);
                manager->priv->inhibit_suspend_fd = -1;
                manager->priv->inhibit_suspend_taken = FALSE;
        }

        if (manager->priv->logind_proxy != NULL) {
                g_object_unref (manager->priv->logind_proxy);
                manager->priv->logind_proxy = NULL;
        }

        g_free (manager->priv->backlight_helper_preference_args);
        manager->priv->backlight_helper_preference_args = NULL;

        if (manager->priv->x11_screen != NULL) {
                g_object_unref (manager->priv->x11_screen);
                manager->priv->x11_screen = NULL;
        }

        g_ptr_array_unref (manager->priv->devices_array);
        manager->priv->devices_array = NULL;

        if (manager->priv->phone != NULL) {
                g_object_unref (manager->priv->phone);
                manager->priv->phone = NULL;
        }

        if (manager->priv->device_composite != NULL) {
                g_object_unref (manager->priv->device_composite);
                manager->priv->device_composite = NULL;
        }

        if (manager->priv->previous_icon != NULL) {
                g_object_unref (manager->priv->previous_icon);
                manager->priv->previous_icon = NULL;
        }

        g_free (manager->priv->previous_summary);
        manager->priv->previous_summary = NULL;

        if (manager->priv->session_proxy != NULL) {
                g_object_unref (manager->priv->session_proxy);
                manager->priv->session_proxy = NULL;
        }

        if (manager->priv->session_presence_proxy != NULL) {
                g_object_unref (manager->priv->session_presence_proxy);
                manager->priv->session_presence_proxy = NULL;
        }

        if (manager->priv->critical_alert_timeout_id > 0) {
                g_source_remove (manager->priv->critical_alert_timeout_id);
                manager->priv->critical_alert_timeout_id = 0;
        }
        g_signal_handlers_disconnect_by_func (manager->priv->idletime,
                                              idle_idletime_reset_cb,
                                              manager);
        g_signal_handlers_disconnect_by_func (manager->priv->idletime,
                                              idle_idletime_alarm_expired_cb,
                                              manager);

        if (manager->priv->idletime != NULL) {
                g_object_unref (manager->priv->idletime);
                manager->priv->idletime = NULL;
        }

        if (manager->priv->status_icon != NULL) {
                g_object_unref (manager->priv->status_icon);
                manager->priv->status_icon = NULL;
        }

        if (manager->priv->xscreensaver_watchdog_timer_id > 0) {
                g_source_remove (manager->priv->xscreensaver_watchdog_timer_id);
                manager->priv->xscreensaver_watchdog_timer_id = 0;
        }

        g_clear_object (&manager->priv->power_iface);
        g_clear_object (&manager->priv->screen_iface);
        g_clear_object (&manager->priv->keyboard_iface);
}

static void
csd_power_manager_init (CsdPowerManager *manager)
{
        manager->priv = CSD_POWER_MANAGER_GET_PRIVATE (manager);
        manager->priv->inhibit_lid_switch_fd = -1;
        manager->priv->inhibit_suspend_fd = -1;
}

static void
csd_power_manager_finalize (GObject *object)
{
        CsdPowerManager *manager;

        manager = CSD_POWER_MANAGER (object);

        g_return_if_fail (manager->priv != NULL);

        if (manager->priv->p_name_id != 0)
                g_bus_unown_name (manager->priv->p_name_id);

        if (manager->priv->s_name_id != 0)
                g_bus_unown_name (manager->priv->s_name_id);

        if (manager->priv->k_name_id != 0)
                g_bus_unown_name (manager->priv->k_name_id);

        G_OBJECT_CLASS (csd_power_manager_parent_class)->finalize (object);
}

#if !UP_CHECK_VERSION(0,99,0)
#define UP_DEVICE_LEVEL_NONE 1
#endif

static GVariant *
device_to_variant_blob (UpDevice *device)
{
        const gchar *object_path, *vendor, *model;
        gchar *device_icon;
        gdouble percentage;
        GIcon *icon;
        guint64 time_empty, time_full;
        guint64 time_state = 0;
        GVariant *value;
        UpDeviceKind kind;
        UpDeviceState state;
        gint battery_level;

        icon = gpm_upower_get_device_icon (device, TRUE);
        device_icon = g_icon_to_string (icon);
        g_object_get (device,
                      "vendor", &vendor,
                      "model", &model,
                      "kind", &kind,
                      "percentage", &percentage,
                      "state", &state,
                      "time-to-empty", &time_empty,
                      "time-to-full", &time_full,
                      NULL);

        /* upower < 0.99.5 compatibility */
        if (g_object_class_find_property (G_OBJECT_GET_CLASS (device), "battery-level")) {
                g_object_get (device,
                              "battery-level", &battery_level,
                              NULL);
        } else {
                battery_level = UP_DEVICE_LEVEL_NONE;
        }

        /* only return time for these simple states */
        if (state == UP_DEVICE_STATE_DISCHARGING)
                time_state = time_empty;
        else if (state == UP_DEVICE_STATE_CHARGING)
                time_state = time_full;

        /* get an object path, even for the composite device */
        object_path = up_device_get_object_path (device);
        if (object_path == NULL)
                object_path = CSD_POWER_DBUS_PATH;

        /* format complex object */
        value = g_variant_new ("(sssusduut)",
                               object_path,
                               vendor,
                               model,
                               kind,
                               device_icon,
                               percentage,
                               state,
                               battery_level,
                               time_state);
        g_free (device_icon);
        g_object_unref (icon);
        return value;
}

/* returns new level */
static void
handle_method_call_keyboard (CsdPowerManager *manager,
                             const gchar *method_name,
                             GVariant *parameters,
                             GDBusMethodInvocation *invocation)
{
        gint step;
        gint value = -1;
        gboolean ret;
        guint percentage;
        GError *error = NULL;

        if (g_strcmp0 (method_name, "GetPercentage") == 0) {
                g_debug ("keyboard get percentage");
                ret = upower_kbd_get_percentage (manager, &error);
                value = manager->priv->kbd_brightness_now;

        } else if (g_strcmp0 (method_name, "SetPercentage") == 0) {
                g_debug ("keyboard set percentage");

                guint value_tmp;
                g_variant_get (parameters, "(u)", &percentage);
                value_tmp = PERCENTAGE_TO_ABS (0, manager->priv->kbd_brightness_max, percentage);

                ret = upower_kbd_set_brightness (manager, value_tmp, &error);
                if (ret)
                        value = value_tmp;

        } else if (g_strcmp0 (method_name, "StepUp") == 0) {
                g_debug ("keyboard step up");
                step = BRIGHTNESS_STEP_AMOUNT (manager->priv->kbd_brightness_max);
                value = MIN (manager->priv->kbd_brightness_now + step,
                             manager->priv->kbd_brightness_max);
                ret = upower_kbd_set_brightness (manager, value, &error);

        } else if (g_strcmp0 (method_name, "StepDown") == 0) {
                g_debug ("keyboard step down");
                step = BRIGHTNESS_STEP_AMOUNT (manager->priv->kbd_brightness_max);
                value = MAX (manager->priv->kbd_brightness_now - step, 0);
                ret = upower_kbd_set_brightness (manager, value, &error);
        } else if (g_strcmp0 (method_name, "GetStep") == 0) {
                g_debug ("keyboard get step");
                value = BRIGHTNESS_STEP_AMOUNT (manager->priv->kbd_brightness_max);
                ret = (value > 0);
        } else if (g_strcmp0 (method_name, "Toggle") == 0) {
                ret = upower_kbd_toggle (manager, &error);
                value = manager->priv->kbd_brightness_now;
        } else {
                g_assert_not_reached ();
        }

        /* return value */
        if (!ret) {
                g_dbus_method_invocation_return_gerror (invocation,
                                                        error);
                g_error_free (error);
        } else {
                percentage = ABS_TO_PERCENTAGE (0,
                                                manager->priv->kbd_brightness_max,
                                                value);
                g_dbus_method_invocation_return_value (invocation,
                                                       g_variant_new ("(u)", percentage));
        }
}

static void
handle_method_call_screen (CsdPowerManager *manager,
                           const gchar *method_name,
                           GVariant *parameters,
                           GDBusMethodInvocation *invocation)
{
        gboolean ret = FALSE;
        gint value = -1;
        guint value_tmp;
        GError *error = NULL;

        if ((g_strcmp0 (method_name, "GetPercentage") == 0) || (g_strcmp0 (method_name, "SetPercentage") == 0)) {
                if (g_strcmp0 (method_name, "GetPercentage") == 0) {
                        g_debug ("screen get percentage");
                        value = backlight_get_percentage (manager, &error);

                } else if (g_strcmp0 (method_name, "SetPercentage") == 0) {
                        g_debug ("screen set percentage");
                        g_variant_get (parameters, "(u)", &value_tmp);
                        ret = backlight_set_percentage (manager, value_tmp, TRUE, &error);
                        if (ret)
                                value = value_tmp;
                }

                /* return value */
                if (value < 0) {
                        g_dbus_method_invocation_return_gerror (invocation,
                                                                error);
                        g_error_free (error);
                } else {
                        g_dbus_method_invocation_return_value (invocation,
                                                               g_variant_new ("(u)",
                                                                              value));
                }
        } else if ((g_strcmp0 (method_name, "StepUp") == 0) || (g_strcmp0 (method_name, "StepDown") == 0)) {
                if (g_strcmp0 (method_name, "StepUp") == 0) {
                        g_debug ("screen step up");
                        value = backlight_step_up (manager, &error);
                } else if (g_strcmp0 (method_name, "StepDown") == 0) {
                        g_debug ("screen step down");
                        value = backlight_step_down (manager, &error);
                }

                /* return value */
                if (value < 0) {
                        g_dbus_method_invocation_return_gerror (invocation,
                                                                error);
                        g_error_free (error);
                } else {
                    // Gdk is not trustworthy for getting a monitor index - they could
                    // be out of order from muffin's opinion, so send the output's
                    // position and let Cinnamon tell us which monitor to react on.
                    gint x, y;

                    x = y = 0;
                    backlight_get_output_id (manager, &x, &y);
                        g_dbus_method_invocation_return_value (invocation,
                                                               g_variant_new ("(uii)",
                                                                              value, x, y));
                }
        } else {
                g_assert_not_reached ();
        }
}

static gboolean
screen_iface_method_cb (CsdScreen              *object,
                        GDBusMethodInvocation *invocation,
                        gpointer               user_data)
{
    CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);

    handle_method_call_screen (manager,
                               g_dbus_method_invocation_get_method_name (invocation),
                               g_dbus_method_invocation_get_parameters (invocation),
                               invocation);

    return TRUE;
}

static gboolean
screen_iface_set_method_cb (CsdScreen              *object,
                            GDBusMethodInvocation *invocation,
                            guint                  percent,
                            gpointer               user_data)
{
    CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);

    handle_method_call_screen (manager,
                               g_dbus_method_invocation_get_method_name (invocation),
                               g_dbus_method_invocation_get_parameters (invocation),
                               invocation);

    return TRUE;
}

static gboolean
keyboard_iface_method_cb (CsdScreen              *object,
                           GDBusMethodInvocation *invocation,
                           gpointer               user_data)
{
    CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);

    handle_method_call_keyboard (manager,
                                 g_dbus_method_invocation_get_method_name (invocation),
                                 g_dbus_method_invocation_get_parameters (invocation),
                                 invocation);

    return TRUE;
}

static gboolean
keyboard_iface_set_method_cb (CsdScreen              *object,
                              GDBusMethodInvocation *invocation,
                              guint                  percent,
                              gpointer               user_data)
{
    CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);

    handle_method_call_keyboard (manager,
                                 g_dbus_method_invocation_get_method_name (invocation),
                                 g_dbus_method_invocation_get_parameters (invocation),
                                 invocation);

    return TRUE;
}

static gboolean
power_iface_handle_get_primary_device (CsdPower              *object,
                                       GDBusMethodInvocation *invocation,
                                       gpointer               user_data)
{
        CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);
        UpDevice *device;
        GVariant *tuple = NULL;
        GVariant *value = NULL;

        g_debug ("Handling Power interface method GetPrimaryDevice");

        /* get the virtual device */
        device = engine_get_primary_device (manager);
        if (device == NULL) {
                g_dbus_method_invocation_return_dbus_error (invocation,
                                                            "org.cinnamon.SettingsDaemon.Power.Failed",
                                                            "There is no primary device.");
                return TRUE;
        }

        /* return the value */
        value = device_to_variant_blob (device);
        tuple = g_variant_new_tuple (&value, 1);
        g_dbus_method_invocation_return_value (invocation, tuple);
        g_object_unref (device);

        return TRUE;
}

static gboolean
power_iface_handle_get_devices (CsdPower              *object,
                                GDBusMethodInvocation *invocation,
                                gpointer               user_data)
{
        CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);
        UpDevice *device;
        GPtrArray *array;
        guint i;
        GVariantBuilder *builder;
        GVariant *tuple = NULL;
        GVariant *value = NULL;

        g_debug ("Handling Power interface method GetDevices");

        /* create builder */
        builder = g_variant_builder_new (G_VARIANT_TYPE("a(sssusduut)"));

        /* add each tuple to the array */
        array = manager->priv->devices_array;
        for (i=0; i<array->len; i++) {
                device = g_ptr_array_index (array, i);
                value = device_to_variant_blob (device);
                g_variant_builder_add_value (builder, value);
        }

        /* return the value */
        value = g_variant_builder_end (builder);
        tuple = g_variant_new_tuple (&value, 1);
        g_dbus_method_invocation_return_value (invocation, tuple);
        g_variant_builder_unref (builder);

        return TRUE;
}

static void
power_name_acquired (GDBusConnection *connection,
                     const gchar     *name,
                     gpointer         user_data)
{
        CsdPower *iface;
        CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);

        iface = csd_power_skeleton_new ();

        g_signal_connect (iface,
                          "handle-get-primary-device",
                          G_CALLBACK (power_iface_handle_get_primary_device),
                          manager);

        g_signal_connect (iface,
                          "handle-get-devices",
                          G_CALLBACK (power_iface_handle_get_devices),
                          manager);

        g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (iface),
                                          connection,
                                          CSD_POWER_DBUS_PATH,
                                          NULL);

        manager->priv->power_iface = iface;

        engine_recalculate_state (manager);
}

static void
screen_name_acquired (GDBusConnection *connection,
                     const gchar     *name,
                     gpointer         user_data)
{
        CsdScreen *iface;
        CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);

        iface = csd_screen_skeleton_new ();

        g_signal_connect (iface,
                          "handle-get-percentage",
                          G_CALLBACK (screen_iface_method_cb),
                          manager);

        g_signal_connect (iface,
                          "handle-set-percentage",
                          G_CALLBACK (screen_iface_set_method_cb),
                          manager);

        g_signal_connect (iface,
                          "handle-step-down",
                          G_CALLBACK (screen_iface_method_cb),
                          manager);

        g_signal_connect (iface,
                          "handle-step-up",
                          G_CALLBACK (screen_iface_method_cb),
                          manager);

        g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (iface),
                                          connection,
                                          CSD_POWER_DBUS_PATH,
                                          NULL);

        manager->priv->screen_iface = iface;

        backlight_emit_changed (manager);
}

static void
keyboard_name_acquired (GDBusConnection *connection,
                        const gchar     *name,
                        gpointer         user_data)
{
        CsdKeyboard *iface;
        CsdPowerManager *manager = CSD_POWER_MANAGER (user_data);

        iface = csd_keyboard_skeleton_new ();

        g_signal_connect (iface,
                          "handle-get-percentage",
                          G_CALLBACK (keyboard_iface_method_cb),
                          manager);

        g_signal_connect (iface,
                          "handle-set-percentage",
                          G_CALLBACK (keyboard_iface_set_method_cb),
                          manager);

        g_signal_connect (iface,
                          "handle-step-down",
                          G_CALLBACK (keyboard_iface_method_cb),
                          manager);

        g_signal_connect (iface,
                          "handle-step-up",
                          G_CALLBACK (keyboard_iface_method_cb),
                          manager);

        g_signal_connect (iface,
                          "handle-get-step",
                          G_CALLBACK (keyboard_iface_method_cb),
                          manager);

        g_signal_connect (iface,
                          "handle-toggle",
                          G_CALLBACK (keyboard_iface_method_cb),
                          manager);

        g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (iface),
                                          connection,
                                          CSD_POWER_DBUS_PATH,
                                          NULL);

        manager->priv->keyboard_iface = iface;

        upower_kbd_emit_changed (manager);
}

static void
on_bus_gotten (GObject             *source_object,
               GAsyncResult        *res,
               CsdPowerManager     *manager)
{
        GDBusConnection *connection;
        GError *error = NULL;

        if (manager->priv->bus_cancellable == NULL ||
            g_cancellable_is_cancelled (manager->priv->bus_cancellable)) {
                g_warning ("Operation has been cancelled, so not retrieving session bus");
                return;
        }

        connection = g_bus_get_finish (res, &error);
        if (connection == NULL) {
                g_warning ("Could not get session bus: %s", error->message);
                g_error_free (error);
                return;
        }

        manager->priv->connection = connection;

        manager->priv->p_name_id = g_bus_own_name_on_connection (connection,
                                                                 CSD_POWER_DBUS_INTERFACE,
                                                                 G_BUS_NAME_OWNER_FLAGS_NONE,
                                                                 power_name_acquired,
                                                                 NULL,
                                                                 manager,
                                                                 NULL);
        manager->priv->s_name_id = g_bus_own_name_on_connection (connection,
                                                                 CSD_POWER_DBUS_INTERFACE_SCREEN,
                                                                 G_BUS_NAME_OWNER_FLAGS_NONE,
                                                                 screen_name_acquired,
                                                                 NULL,
                                                                 manager,
                                                                 NULL);
        manager->priv->k_name_id = g_bus_own_name_on_connection (connection,
                                                                 CSD_POWER_DBUS_INTERFACE_KEYBOARD,
                                                                 G_BUS_NAME_OWNER_FLAGS_NONE,
                                                                 keyboard_name_acquired,
                                                                 NULL,
                                                                 manager,
                                                                 NULL);
}

static void
register_manager_dbus (CsdPowerManager *manager)
{
        manager->priv->bus_cancellable = g_cancellable_new ();

        g_bus_get (G_BUS_TYPE_SESSION,
                   manager->priv->bus_cancellable,
                   (GAsyncReadyCallback) on_bus_gotten,
                   manager);
}

CsdPowerManager *
csd_power_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CSD_TYPE_POWER_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
                register_manager_dbus (manager_object);
        }
        return CSD_POWER_MANAGER (manager_object);
}

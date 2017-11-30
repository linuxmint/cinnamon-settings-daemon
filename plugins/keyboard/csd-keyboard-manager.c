/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright © 2001 Ximian, Inc.
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Written by Sergey V. Oudaltsov <svu@users.sourceforge.net>
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

#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include <libxklavier/xklavier.h>
#include <libgnomekbd/gkbd-status.h>
#include <libgnomekbd/gkbd-keyboard-drawing.h>
#include <libgnomekbd/gkbd-desktop-config.h>
#include <libgnomekbd/gkbd-keyboard-config.h>
#include <libgnomekbd/gkbd-util.h>

#include "cinnamon-settings-profile.h"
#include "csd-keyboard-manager.h"
#include "csd-input-helper.h"
#include "csd-enums.h"
#include "delayed-dialog.h"

#define CSD_KEYBOARD_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CSD_TYPE_KEYBOARD_MANAGER, CsdKeyboardManagerPrivate))

#define CSD_KEYBOARD_DIR "org.cinnamon.settings-daemon.peripherals.keyboard"

#define KEY_REPEAT         "repeat"
#define KEY_CLICK          "click"
#define KEY_INTERVAL       "repeat-interval"
#define KEY_DELAY          "delay"
#define KEY_CLICK_VOLUME   "click-volume"
#define KEY_NUMLOCK_STATE  "numlock-state"

#define KEY_BELL_VOLUME    "bell-volume"
#define KEY_BELL_PITCH     "bell-pitch"
#define KEY_BELL_DURATION  "bell-duration"
#define KEY_BELL_MODE      "bell-mode"

struct CsdKeyboardManagerPrivate
{
	guint      start_idle_id;
        GSettings *settings;
        gboolean   have_xkb;
        gint       xkb_event_base;
        CsdNumLockState old_state;
        GdkDeviceManager *device_manager;
        guint device_added_id;
        guint device_removed_id;
        /* XKB */
	XklEngine *xkl_engine;
	XklConfigRegistry *xkl_registry;

	GkbdDesktopConfig current_config;
	GkbdKeyboardConfig current_kbd_config;

	GkbdKeyboardConfig initial_sys_kbd_config;
	GSettings *settings_desktop;
	GSettings *settings_keyboard;

	GtkStatusIcon *icon;
};

static void     csd_keyboard_manager_finalize    (GObject                 *object);

G_DEFINE_TYPE (CsdKeyboardManager, csd_keyboard_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static gboolean try_activating_xkb_config_if_new (CsdKeyboardManager *manager,
						  GkbdKeyboardConfig *current_sys_kbd_config);
static gboolean filter_xkb_config (CsdKeyboardManager *manager);

void csd_keyboard_xkb_init (CsdKeyboardManager *manager);

typedef void (*PostActivationCallback) (void *userData);

void
csd_keyboard_xkb_set_post_activation_callback (PostActivationCallback fun,
                                               void                  *userData);

static PostActivationCallback pa_callback = NULL;
static void *pa_callback_user_data = NULL;

static void
activation_error (void)
{
	char const *vendor;
	GtkWidget *dialog;

	vendor =
	    ServerVendor (GDK_DISPLAY_XDISPLAY
			  (gdk_display_get_default ()));

	/* VNC viewers will not work, do not barrage them with warnings */
	if (NULL != vendor && NULL != strstr (vendor, "VNC"))
		return;

	dialog = gtk_message_dialog_new_with_markup (NULL,
						     0,
						     GTK_MESSAGE_ERROR,
						     GTK_BUTTONS_CLOSE,
						     _
						     ("Error activating XKB configuration.\n"
						      "There can be various reasons for that.\n\n"
						      "If you report this situation as a bug, include the results of\n"
						      " <b>%s</b>\n"
						      " <b>%s</b>\n"
						      " <b>%s</b>\n"
						      " <b>%s</b>"),
						     "xprop -root | grep XKB",
						     "gsettings get org.gnome.libgnomekbd.keyboard model",
						     "gsettings get org.gnome.libgnomekbd.keyboard layouts",
						     "gsettings get org.gnome.libgnomekbd.keyboard options");
	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	csd_delayed_show_dialog (dialog);
}

static gboolean
ensure_xkl_registry (CsdKeyboardManager *manager)
{
	if (!manager->priv->xkl_registry) {
		manager->priv->xkl_registry =
		    xkl_config_registry_get_instance (manager->priv->xkl_engine);
		/* load all materials, unconditionally! */
		if (!xkl_config_registry_load (manager->priv->xkl_registry, TRUE)) {
			g_object_unref (manager->priv->xkl_registry);
			manager->priv->xkl_registry = NULL;
			return FALSE;
		}
	}

	return TRUE;
}

static void
apply_desktop_settings (CsdKeyboardManager *manager)
{
	if (manager->priv->xkl_engine == NULL)
		return;

	csd_keyboard_manager_apply_settings (manager);
	gkbd_desktop_config_load (&manager->priv->current_config);
	/* again, probably it would be nice to compare things
	   before activating them */
	gkbd_desktop_config_activate (&manager->priv->current_config);
}

static void
popup_menu_launch_capplet ()
{
	GAppInfo *info;
	GdkAppLaunchContext *ctx;
	GError *error = NULL;

	info = g_app_info_create_from_commandline
	    ("cinnamon-settings region", NULL, 0, &error);

        if (info == NULL) {
                if (error) {
                        g_warning ("cinnamon-settings region failed: %s\n",
                                   error->message);
                        g_error_free (error);
                }
                return;
        }

	ctx = gdk_display_get_app_launch_context
	        (gdk_display_get_default ());

	if (g_app_info_launch (info,
	                       NULL,
                               G_APP_LAUNCH_CONTEXT (ctx),
                               &error) == FALSE) {
		g_warning ("Could not execute keyboard properties capplet: [%s]\n",
			   error->message);
		g_error_free (error);
	}

	g_object_unref (info);
	g_object_unref (ctx);
}

static void
popup_menu_show_layout (CsdKeyboardManager *manager)
{
        XklState *xkl_state;
	char *command;

	xkl_state = xkl_engine_get_current_state (manager->priv->xkl_engine);
	if (xkl_state->group < 0)
		return;

	command = g_strdup_printf ("gkbd-keyboard-display -g %d", xkl_state->group+1);
	g_spawn_command_line_async (command, NULL);
	g_free (command);
}

static void
popup_menu_set_group (gint group_number, gboolean only_menu)
{

	XklEngine *engine = gkbd_status_get_xkl_engine ();

	XklState *st = xkl_engine_get_current_state(engine);
	Window cur;
	st->group = group_number;
	xkl_engine_allow_one_switch_to_secondary_group (engine);
	cur = xkl_engine_get_current_window (engine);
	if (cur != (Window) NULL) {
		g_debug ("Enforcing the state %d for window %lx\n",
			   st->group, cur);

		xkl_engine_save_state (engine,
				       xkl_engine_get_current_window
				       (engine), st);
/*    XSetInputFocus( GDK_DISPLAY(), cur, RevertToNone, CurrentTime );*/
	} else {
		g_debug ("??? Enforcing the state %d for unknown window\n",
			 st->group);
		/* strange situation - bad things can happen */
	}
        if (!only_menu)
        	xkl_engine_lock_group (engine, st->group);
}

static void
popup_menu_set_group_cb (GtkMenuItem * item, gpointer param)
{
	gint group_number = GPOINTER_TO_INT (param);

	popup_menu_set_group(group_number, FALSE);
}


static GtkMenu *
create_status_menu (CsdKeyboardManager *manager)
{
	GtkMenu *popup_menu = GTK_MENU (gtk_menu_new ());
	int i = 0;

	GtkMenu *groups_menu = GTK_MENU (gtk_menu_new ());
	gchar **current_name = gkbd_status_get_group_names ();

	GtkWidget *item = gtk_menu_item_new_with_mnemonic (_("_Layouts"));
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item),
				   GTK_WIDGET (groups_menu));

	item = gtk_menu_item_new_with_mnemonic (_("Show _Keyboard Layout..."));
	gtk_widget_show (item);
	g_signal_connect (item, "activate", popup_menu_show_layout, manager);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);

	/* translators note:
	 * This is the name of the cinnamon-settings "region" panel */
	item = gtk_menu_item_new_with_mnemonic (_("Region and Language Settings"));
	gtk_widget_show (item);
	g_signal_connect (item, "activate", popup_menu_launch_capplet, NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), item);

	for (i = 0; current_name && *current_name; i++, current_name++) {

		gchar *image_file = gkbd_status_get_image_filename (i);

		if (image_file == NULL) {
			item =
			    gtk_menu_item_new_with_label (*current_name);
		} else {
			GdkPixbuf *pixbuf =
			    gdk_pixbuf_new_from_file_at_size (image_file,
							      24, 24,
							      NULL);
			GtkWidget *img =
			    gtk_image_new_from_pixbuf (pixbuf);
			item =
			    gtk_image_menu_item_new_with_label
			    (*current_name);
			gtk_widget_show (img);
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM
						       (item), img);
			gtk_image_menu_item_set_always_show_image
			    (GTK_IMAGE_MENU_ITEM (item), TRUE);
			g_free (image_file);
		}
		gtk_widget_show (item);
		gtk_menu_shell_append (GTK_MENU_SHELL (groups_menu), item);
		g_signal_connect (item, "activate",
				  G_CALLBACK (popup_menu_set_group_cb),
				  GINT_TO_POINTER (i));
	}

	return popup_menu;
}

static gboolean
try_activating_xkb_config_if_new (CsdKeyboardManager *manager,
                                  GkbdKeyboardConfig *
				  current_sys_kbd_config)
{
	/* Activate - only if different! */
	if (!gkbd_keyboard_config_equals
	    (&manager->priv->current_kbd_config, current_sys_kbd_config)) {
		if (gkbd_keyboard_config_activate (&manager->priv->current_kbd_config)) {
			if (pa_callback != NULL) {
				(*pa_callback) (pa_callback_user_data);
				return TRUE;
			}
		} else {
			return FALSE;
		}
	}
	return TRUE;
}

static gboolean
filter_xkb_config (CsdKeyboardManager *manager)
{
	XklConfigItem *item;
	gchar *lname;
	gchar *vname;
	gchar **lv;
	gboolean any_change = FALSE;

	g_debug ("Filtering configuration against the registry\n");
	if (!ensure_xkl_registry (manager))
		return FALSE;

	lv = manager->priv->current_kbd_config.layouts_variants;
	item = xkl_config_item_new ();
	while (*lv) {
		g_debug ("Checking [%s]\n", *lv);
		if (gkbd_keyboard_config_split_items (*lv, &lname, &vname)) {
			gboolean should_be_dropped = FALSE;
			g_snprintf (item->name, sizeof (item->name), "%s",
				    lname);
			if (!xkl_config_registry_find_layout
			    (manager->priv->xkl_registry, item)) {
				g_debug ("Bad layout [%s]\n",
					 lname);
				should_be_dropped = TRUE;
			} else if (vname) {
				g_snprintf (item->name,
					    sizeof (item->name), "%s",
					    vname);
				if (!xkl_config_registry_find_variant
				    (manager->priv->xkl_registry, lname, item)) {
					g_debug ("Bad variant [%s(%s)]\n",
						 lname, vname);
					should_be_dropped = TRUE;
				}
			}
			if (should_be_dropped) {
				gkbd_strv_behead (lv);
				any_change = TRUE;
				continue;
			}
		}
		lv++;
	}
	g_object_unref (item);
	return any_change;
}

static void
apply_xkb_settings (CsdKeyboardManager *manager)
{
	GkbdKeyboardConfig current_sys_kbd_config;

	if (manager->priv->xkl_engine == NULL)
		return;

	gkbd_keyboard_config_init (&current_sys_kbd_config, manager->priv->xkl_engine);

	gkbd_keyboard_config_load (&manager->priv->current_kbd_config,
				   &manager->priv->initial_sys_kbd_config);

	gkbd_keyboard_config_load_from_x_current (&current_sys_kbd_config,
						  NULL);

	if (!try_activating_xkb_config_if_new (manager, &current_sys_kbd_config)) {
		if (filter_xkb_config (manager)) {
			if (!try_activating_xkb_config_if_new
			    (manager,&current_sys_kbd_config)) {
				g_warning
				    ("Could not activate the filtered XKB configuration");
				activation_error ();
			}
		} else {
			g_warning
			    ("Could not activate the XKB configuration");
			activation_error ();
		}
	} else
		g_debug ("Actual KBD configuration was not changed: redundant notification\n");

	gkbd_keyboard_config_term (&current_sys_kbd_config);
}

void
csd_keyboard_xkb_set_post_activation_callback (PostActivationCallback fun,
					       void *user_data)
{
	pa_callback = fun;
	pa_callback_user_data = user_data;
}

void
csd_keyboard_xkb_init (CsdKeyboardManager * manager)
{
	Display *dpy =
	    GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	cinnamon_settings_profile_start (NULL);

	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   DATADIR G_DIR_SEPARATOR_S
					   "icons");

	cinnamon_settings_profile_start ("xkl_engine_get_instance");
	manager->priv->xkl_engine = xkl_engine_get_instance (dpy);
	cinnamon_settings_profile_end ("xkl_engine_get_instance");
	if (manager->priv->xkl_engine) {

		gkbd_desktop_config_init (&manager->priv->current_config, manager->priv->xkl_engine);
		gkbd_keyboard_config_init (&manager->priv->current_kbd_config,
					   manager->priv->xkl_engine);
		xkl_engine_backup_names_prop (manager->priv->xkl_engine);
	        gkbd_keyboard_config_init (&manager->priv->initial_sys_kbd_config,
	                                   manager->priv->xkl_engine);
	        gkbd_keyboard_config_load_from_x_initial (&manager->priv->initial_sys_kbd_config,
						          NULL);
		manager->priv->settings_desktop = g_settings_new (GKBD_DESKTOP_SCHEMA);
		manager->priv->settings_keyboard = g_settings_new (GKBD_KEYBOARD_SCHEMA);
		g_signal_connect (manager->priv->settings_desktop, "changed",
				  (GCallback) apply_desktop_settings,
				  manager);
		g_signal_connect (manager->priv->settings_keyboard, "changed",
				  (GCallback) apply_xkb_settings, manager);

		cinnamon_settings_profile_start ("xkl_engine_start_listen");
		xkl_engine_start_listen (manager->priv->xkl_engine,
					 XKLL_MANAGE_LAYOUTS |
					 XKLL_MANAGE_WINDOW_STATES);
		cinnamon_settings_profile_end ("xkl_engine_start_listen");

		cinnamon_settings_profile_start ("apply_desktop_settings");
		apply_desktop_settings (manager);
		cinnamon_settings_profile_end ("apply_desktop_settings");
		cinnamon_settings_profile_start ("apply_xkb_settings");
		apply_xkb_settings (manager);
		cinnamon_settings_profile_end ("apply_xkb_settings");
	}

	cinnamon_settings_profile_end (NULL);
}

static gboolean
xkb_set_keyboard_autorepeat_rate (guint delay, guint interval)
{
        return XkbSetAutoRepeatRate (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                     XkbUseCoreKbd,
                                     delay,
                                     interval);
}

static gboolean
check_xkb_extension (CsdKeyboardManager *manager)
{
         Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
         int opcode, error_base, major, minor;

         manager->priv->have_xkb = XkbQueryExtension (dpy,
                                       &opcode,
                                       &manager->priv->xkb_event_base,
                                       &error_base,
                                       &major,
                                       &minor)
                 && XkbUseExtension (dpy, &major, &minor);

         return manager->priv->have_xkb;
}

static void
numlock_xkb_init (CsdKeyboardManager *manager)
{
        Display *dpy;

        dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());

        XkbSelectEventDetails (dpy,
                               XkbUseCoreKbd,
                               XkbStateNotifyMask,
                               XkbModifierLockMask,
                               XkbModifierLockMask);
}

static unsigned
numlock_NumLock_modifier_mask (void)
{
        Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
        return XkbKeysymToModifiers (dpy, XK_Num_Lock);
}

static void
numlock_set_xkb_state (CsdNumLockState new_state)
{
        unsigned int num_mask;
        Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
        if (new_state != CSD_NUM_LOCK_STATE_ON && new_state != CSD_NUM_LOCK_STATE_OFF)
                return;
        num_mask = numlock_NumLock_modifier_mask ();
        XkbLockModifiers (dpy, XkbUseCoreKbd, num_mask, new_state == CSD_NUM_LOCK_STATE_ON ? num_mask : 0);
}

static GdkFilterReturn
xkb_events_filter (GdkXEvent *xev_,
                   GdkEvent  *gdkev_,
                   gpointer   user_data)
{
        XEvent *xev = (XEvent *) xev_;
	XkbEvent *xkbev = (XkbEvent *) xev;
        CsdKeyboardManager *manager = (CsdKeyboardManager *) user_data;
/* libxklavier's events first */
	if (manager->priv->xkl_engine != NULL)
		xkl_engine_filter_events (manager->priv->xkl_engine, xev);

	/* Then XKB specific events */
        if (xev->type != manager->priv->xkb_event_base)
		return GDK_FILTER_CONTINUE;

	if (xkbev->any.xkb_type != XkbStateNotify)
		return GDK_FILTER_CONTINUE;

	if (xkbev->state.changed & XkbModifierLockMask) {
		unsigned num_mask = numlock_NumLock_modifier_mask ();
		unsigned locked_mods = xkbev->state.locked_mods;
		CsdNumLockState numlock_state;

		numlock_state = (num_mask & locked_mods) ? CSD_NUM_LOCK_STATE_ON : CSD_NUM_LOCK_STATE_OFF;

		if (numlock_state != manager->priv->old_state) {
			g_settings_set_enum (manager->priv->settings,
					     KEY_NUMLOCK_STATE,
					     numlock_state);
			manager->priv->old_state = numlock_state;
		}
	}

        return GDK_FILTER_CONTINUE;
}

static void
install_xkb_filter (CsdKeyboardManager *manager)
{
        gdk_window_add_filter (NULL,
                               xkb_events_filter,
                               manager);
}

static void
remove_xkb_filter (CsdKeyboardManager *manager)
{
        gdk_window_remove_filter (NULL,
                                  xkb_events_filter,
                                  manager);
}

static void
apply_settings (GSettings          *settings,
                const char         *key,
                CsdKeyboardManager *manager)
{
        XKeyboardControl kbdcontrol;
        gboolean         repeat;
        gboolean         click;
        guint            interval;
        guint            delay;
        int              click_volume;
        int              bell_volume;
        int              bell_pitch;
        int              bell_duration;
        CsdBellMode      bell_mode;
        gboolean         rnumlock;

        if (g_strcmp0 (key, KEY_NUMLOCK_STATE) == 0)
                return;

        repeat        = g_settings_get_boolean  (settings, KEY_REPEAT);
        click         = g_settings_get_boolean  (settings, KEY_CLICK);
        interval      = g_settings_get_uint  (settings, KEY_INTERVAL);
        delay         = g_settings_get_uint  (settings, KEY_DELAY);
        click_volume  = g_settings_get_int   (settings, KEY_CLICK_VOLUME);
        bell_pitch    = g_settings_get_int   (settings, KEY_BELL_PITCH);
        bell_duration = g_settings_get_int   (settings, KEY_BELL_DURATION);

        bell_mode = g_settings_get_enum (settings, KEY_BELL_MODE);
        bell_volume   = (bell_mode == CSD_BELL_MODE_ON) ? 50 : 0;

        gdk_error_trap_push ();
        if (repeat) {
                gboolean rate_set = FALSE;

                XAutoRepeatOn (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
                /* Use XKB in preference */
                rate_set = xkb_set_keyboard_autorepeat_rate (delay, interval);

                if (!rate_set)
                        g_warning ("Neither XKeyboard not Xfree86's keyboard extensions are available,\n"
                                   "no way to support keyboard autorepeat rate settings");
        } else {
                XAutoRepeatOff (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
        }

        /* as percentage from 0..100 inclusive */
        if (click_volume < 0) {
                click_volume = 0;
        } else if (click_volume > 100) {
                click_volume = 100;
        }
        kbdcontrol.key_click_percent = click ? click_volume : 0;
        kbdcontrol.bell_percent = bell_volume;
        kbdcontrol.bell_pitch = bell_pitch;
        kbdcontrol.bell_duration = bell_duration;
        XChangeKeyboardControl (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                KBKeyClickPercent | KBBellPercent | KBBellPitch | KBBellDuration,
                                &kbdcontrol);

	if (g_strcmp0 (key, "remember-numlock-state") == 0 || key == NULL) {
		rnumlock      = g_settings_get_boolean  (settings, "remember-numlock-state");

		manager->priv->old_state = g_settings_get_enum (manager->priv->settings, KEY_NUMLOCK_STATE);

		if (manager->priv->have_xkb && rnumlock)
			numlock_set_xkb_state (manager->priv->old_state);
	}

        XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), FALSE);
        gdk_error_trap_pop_ignored ();
}

void
csd_keyboard_manager_apply_settings (CsdKeyboardManager *manager)
{
        apply_settings (manager->priv->settings, NULL, manager);
}

static void
device_added_cb (GdkDeviceManager   *device_manager,
                 GdkDevice          *device,
                 CsdKeyboardManager *manager)
{
        GdkInputSource source;

        source = gdk_device_get_source (device);
        if (source == GDK_SOURCE_KEYBOARD) {
                apply_desktop_settings (manager);
                apply_xkb_settings (manager);
                run_custom_command (device, COMMAND_DEVICE_ADDED);
        }
}

static void
device_removed_cb (GdkDeviceManager   *device_manager,
                   GdkDevice          *device,
                   CsdKeyboardManager *manager)
{
         GdkInputSource source;

        source = gdk_device_get_source (device);
        if (source == GDK_SOURCE_KEYBOARD) {
                run_custom_command (device, COMMAND_DEVICE_REMOVED);
        }
}

static void
set_devicepresence_handler (CsdKeyboardManager *manager)
{
        GdkDeviceManager *device_manager;

        device_manager = gdk_display_get_device_manager (gdk_display_get_default ());

        manager->priv->device_added_id = g_signal_connect (G_OBJECT (device_manager), "device-added",
                                                           G_CALLBACK (device_added_cb), manager);
        manager->priv->device_removed_id = g_signal_connect (G_OBJECT (device_manager), "device-removed",
                                                             G_CALLBACK (device_removed_cb), manager);
        manager->priv->device_manager = device_manager;
}


static gboolean
start_keyboard_idle_cb (CsdKeyboardManager *manager)
{
        cinnamon_settings_profile_start (NULL);

        g_debug ("Starting keyboard manager");

        check_xkb_extension (manager);
        manager->priv->settings = g_settings_new (CSD_KEYBOARD_DIR);

        if (manager->priv->have_xkb) {
                csd_keyboard_xkb_init (manager);
                numlock_xkb_init (manager);
        }

        set_devicepresence_handler (manager);

        /* apply current settings before we install the callback */
        csd_keyboard_manager_apply_settings (manager);

        g_signal_connect (G_OBJECT (manager->priv->settings), "changed",
                          G_CALLBACK (apply_settings), manager);

        if (manager->priv->have_xkb)
		install_xkb_filter (manager);

        cinnamon_settings_profile_end (NULL);

        manager->priv->start_idle_id = 0;

        return FALSE;
}

gboolean
csd_keyboard_manager_start (CsdKeyboardManager *manager,
                            GError            **error)
{
        cinnamon_settings_profile_start (NULL);

        manager->priv->start_idle_id = g_idle_add ((GSourceFunc) start_keyboard_idle_cb, manager);

        cinnamon_settings_profile_end (NULL);

        return TRUE;
}

void
csd_keyboard_manager_stop (CsdKeyboardManager *manager)
{
        CsdKeyboardManagerPrivate *p = manager->priv;

        g_debug ("Stopping keyboard manager");

        if (p->settings != NULL) {
                g_object_unref (p->settings);
                p->settings = NULL;
        }
        if (p->settings_desktop != NULL) {
		g_object_unref (p->settings_desktop);
		p->settings_desktop = NULL;
	}
	if (p->settings_keyboard != NULL) {
		g_object_unref (p->settings_keyboard);
		p->settings_keyboard = NULL;
	}
        if (p->device_manager != NULL) {
                g_signal_handler_disconnect (p->device_manager, p->device_added_id);
                g_signal_handler_disconnect (p->device_manager, p->device_removed_id);
                p->device_manager = NULL;
        }

        if (p->have_xkb)
		remove_xkb_filter (manager);
	if (p->xkl_registry != NULL) {
		g_object_unref (p->xkl_registry);
		p->xkl_registry = NULL;
	}

	if (p->xkl_engine != NULL) {
		xkl_engine_stop_listen (p->xkl_engine,
					XKLL_MANAGE_LAYOUTS | XKLL_MANAGE_WINDOW_STATES);
		g_object_unref (p->xkl_engine);
		p->xkl_engine = NULL;
	}
}

static GObject *
csd_keyboard_manager_constructor (GType                  type,
                                  guint                  n_construct_properties,
                                  GObjectConstructParam *construct_properties)
{
        CsdKeyboardManager      *keyboard_manager;

        keyboard_manager = CSD_KEYBOARD_MANAGER (G_OBJECT_CLASS (csd_keyboard_manager_parent_class)->constructor (type,
                                                                                                      n_construct_properties,
                                                                                                      construct_properties));

        return G_OBJECT (keyboard_manager);
}

static void
csd_keyboard_manager_class_init (CsdKeyboardManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = csd_keyboard_manager_constructor;
        object_class->finalize = csd_keyboard_manager_finalize;

        g_type_class_add_private (klass, sizeof (CsdKeyboardManagerPrivate));
}

static void
csd_keyboard_manager_init (CsdKeyboardManager *manager)
{
        manager->priv = CSD_KEYBOARD_MANAGER_GET_PRIVATE (manager);
}

static void
csd_keyboard_manager_finalize (GObject *object)
{
        CsdKeyboardManager *keyboard_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_KEYBOARD_MANAGER (object));

        keyboard_manager = CSD_KEYBOARD_MANAGER (object);

        g_return_if_fail (keyboard_manager->priv != NULL);

        if (keyboard_manager->priv->start_idle_id != 0) {
                g_source_remove (keyboard_manager->priv->start_idle_id);
                keyboard_manager->priv->start_idle_id = 0;
        }

        G_OBJECT_CLASS (csd_keyboard_manager_parent_class)->finalize (object);
}

CsdKeyboardManager *
csd_keyboard_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CSD_TYPE_KEYBOARD_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return CSD_KEYBOARD_MANAGER (manager_object);
}

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

#include "cinnamon-settings-profile.h"
#include "csd-keyboard-manager.h"
#include "csd-enums.h"

#include "csd-keyboard-xkb.h"
#include "migrate-settings.h"

#define CSD_KEYBOARD_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CSD_TYPE_KEYBOARD_MANAGER, CsdKeyboardManagerPrivate))

#ifndef HOST_NAME_MAX
#  define HOST_NAME_MAX 255
#endif

#define CSD_KEYBOARD_DIR "org.cinnamon.settings-daemon.peripherals.keyboard"

#define KEY_CLICK          "click"
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
};

static void     csd_keyboard_manager_finalize    (GObject                 *object);

G_DEFINE_TYPE (CsdKeyboardManager, csd_keyboard_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static void
numlock_xkb_init (CsdKeyboardManager *manager)
{
        Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
        gboolean have_xkb;
        int opcode, error_base, major, minor;

        have_xkb = XkbQueryExtension (dpy,
                                      &opcode,
                                      &manager->priv->xkb_event_base,
                                      &error_base,
                                      &major,
                                      &minor)
                && XkbUseExtension (dpy, &major, &minor);

        if (have_xkb) {
                XkbSelectEventDetails (dpy,
                                       XkbUseCoreKbd,
                                       XkbStateNotifyMask,
                                       XkbModifierLockMask,
                                       XkbModifierLockMask);
        } else {
                g_warning ("XKB extension not available");
        }

        manager->priv->have_xkb = have_xkb;
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
numlock_xkb_callback (GdkXEvent *xev_,
                      GdkEvent  *gdkev_,
                      gpointer   user_data)
{
        XEvent *xev = (XEvent *) xev_;
	XkbEvent *xkbev = (XkbEvent *) xev;
        CsdKeyboardManager *manager = (CsdKeyboardManager *) user_data;

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
numlock_install_xkb_callback (CsdKeyboardManager *manager)
{
        if (!manager->priv->have_xkb)
                return;

        gdk_window_add_filter (NULL,
                               numlock_xkb_callback,
                               manager);
}

static guint
_csd_settings_get_uint (GSettings  *settings,
			const char *key)
{
	guint value;

	g_settings_get (settings, key, "u", &value);
	return value;
}

static void
apply_settings (GSettings          *settings,
                const char         *key,
                CsdKeyboardManager *manager)
{
        XKeyboardControl kbdcontrol;
        gboolean         click;
        int              click_volume;
        int              bell_volume;
        int              bell_pitch;
        int              bell_duration;
        CsdBellMode      bell_mode;
        gboolean         rnumlock;

        if (g_strcmp0 (key, KEY_NUMLOCK_STATE) == 0)
                return;

        click         = g_settings_get_boolean  (settings, KEY_CLICK);
        click_volume  = g_settings_get_int   (settings, KEY_CLICK_VOLUME);
        bell_pitch    = g_settings_get_int   (settings, KEY_BELL_PITCH);
        bell_duration = g_settings_get_int   (settings, KEY_BELL_DURATION);

        bell_mode = g_settings_get_enum (settings, KEY_BELL_MODE);
        bell_volume   = (bell_mode == CSD_BELL_MODE_ON) ? 50 : 0;

        gdk_x11_display_error_trap_push (gdk_display_get_default ());

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
        gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());
}

void
csd_keyboard_manager_apply_settings (CsdKeyboardManager *manager)
{
        apply_settings (manager->priv->settings, NULL, manager);
}

static gboolean
start_keyboard_idle_cb (CsdKeyboardManager *manager)
{
        cinnamon_settings_profile_start (NULL);

        g_debug ("Starting keyboard manager");

        manager->priv->have_xkb = 0;
        manager->priv->settings = g_settings_new (CSD_KEYBOARD_DIR);

        /* Essential - xkb initialization should happen before */
        csd_keyboard_xkb_init (manager);

        numlock_xkb_init (manager);

        /* apply current settings before we install the callback */
        csd_keyboard_manager_apply_settings (manager);

        g_signal_connect (G_OBJECT (manager->priv->settings), "changed",
                          G_CALLBACK (apply_settings), manager);

        numlock_install_xkb_callback (manager);

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

        if (p->have_xkb) {
                gdk_window_remove_filter (NULL,
                                          numlock_xkb_callback,
                                          manager);
        }

        csd_keyboard_xkb_shutdown ();
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

static void
migrate_keyboard_settings (void)
{
        CsdSettingsMigrateEntry entries[] = {
                { "repeat",          "repeat",          NULL },
                { "repeat-interval", "repeat-interval", NULL },
                { "delay",           "delay",           NULL }
        };

        csd_settings_migrate_check ("org.cinnamon.settings-daemon.peripherals.keyboard.deprecated",
                                    "/org/cinnamon/settings-daemon/peripherals/keyboard/",
                                    "org.cinnamon.desktop.peripherals.keyboard",
                                    "/org/cinnamon/desktop/peripherals/keyboard/",
                                    entries, G_N_ELEMENTS (entries));
}

CsdKeyboardManager *
csd_keyboard_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                migrate_keyboard_settings ();
                manager_object = g_object_new (CSD_TYPE_KEYBOARD_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return CSD_KEYBOARD_MANAGER (manager_object);
}

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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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

#include "cinnamon-settings-profile.h"
#include "csd-keyboard-manager.h"
#include "csd-input-helper.h"
#include "csd-enums.h"
#include "migrate-settings.h"

#define CSD_KEYBOARD_DIR "org.cinnamon.settings-daemon.peripherals.keyboard"

#define KEY_CLICK          "click"
#define KEY_CLICK_VOLUME   "click-volume"

#define KEY_BELL_VOLUME    "bell-volume"
#define KEY_BELL_PITCH     "bell-pitch"
#define KEY_BELL_DURATION  "bell-duration"
#define KEY_BELL_MODE      "bell-mode"
#define KEY_BELL_CUSTOM_FILE "bell-custom-file"

#define CINNAMON_DESKTOP_INTERFACE_DIR "org.cinnamon.desktop.interface"

#define KEY_GTK_IM_MODULE    "gtk-im-module"
#define GTK_IM_MODULE_SIMPLE "gtk-im-context-simple"
#define GTK_IM_MODULE_IBUS   "ibus"

#define CINNAMON_DESKTOP_INPUT_SOURCES_DIR "org.gnome.desktop.input-sources"

#define KEY_INPUT_SOURCES        "sources"
#define KEY_KEYBOARD_OPTIONS     "xkb-options"

#define INPUT_SOURCE_TYPE_XKB  "xkb"
#define INPUT_SOURCE_TYPE_IBUS "ibus"

#define DEFAULT_LAYOUT "us"

#define CINNAMON_A11Y_APPLICATIONS_INTERFACE_DIR "org.cinnamon.desktop.a11y.applications"
#define KEY_OSK_ENABLED "screen-keyboard-enabled"

struct _CsdKeyboardManager
{
        GObject    parent;

        guint      start_idle_id;
        GSettings *settings;
        GSettings *input_sources_settings;
        GSettings *a11y_settings;
        GDBusProxy *localed;
        GCancellable *cancellable;

        GdkDeviceManager *device_manager;
        guint device_added_id;
        guint device_removed_id;
};

static void     csd_keyboard_manager_class_init  (CsdKeyboardManagerClass *klass);
static void     csd_keyboard_manager_init        (CsdKeyboardManager      *keyboard_manager);
static void     csd_keyboard_manager_finalize    (GObject                 *object);

static void     update_gtk_im_module (CsdKeyboardManager *manager);

G_DEFINE_TYPE (CsdKeyboardManager, csd_keyboard_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static gboolean
session_is_wayland (void)
{
    static gboolean session_is_wayland = FALSE;
    static gsize once_init = 0;

    if (g_once_init_enter (&once_init)) {
        const gchar *env = g_getenv ("XDG_SESSION_TYPE");
        if (env && g_strcmp0 (env, "wayland") == 0) {
            session_is_wayland = TRUE;
        }

        g_debug ("Session is Wayland? %d", session_is_wayland);

        g_once_init_leave (&once_init, 1);
    }

    return session_is_wayland;
}

static void
init_builder_with_sources (GVariantBuilder *builder,
                           GSettings       *settings)
{
        const gchar *type;
        const gchar *id;
        GVariantIter iter;
        GVariant *sources;

        sources = g_settings_get_value (settings, KEY_INPUT_SOURCES);

        g_variant_builder_init (builder, G_VARIANT_TYPE ("a(ss)"));

        g_variant_iter_init (&iter, sources);
        while (g_variant_iter_next (&iter, "(&s&s)", &type, &id))
                g_variant_builder_add (builder, "(ss)", type, id);

        g_variant_unref (sources);
}

static void
apply_bell (CsdKeyboardManager *manager)
{
    GSettings       *settings;
        XKeyboardControl kbdcontrol;
        gboolean         click;
        int              bell_volume;
        int              bell_pitch;
        int              bell_duration;
        CsdBellMode      bell_mode;
        int              click_volume;

        if (session_is_wayland ())
                return;

        g_debug ("Applying the bell settings");
        settings      = manager->settings;
        click         = g_settings_get_boolean  (settings, KEY_CLICK);
        click_volume  = g_settings_get_int   (settings, KEY_CLICK_VOLUME);

        bell_pitch    = g_settings_get_int   (settings, KEY_BELL_PITCH);
        bell_duration = g_settings_get_int   (settings, KEY_BELL_DURATION);

        bell_mode = g_settings_get_enum (settings, KEY_BELL_MODE);
        bell_volume   = (bell_mode == CSD_BELL_MODE_ON) ? 50 : 0;

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

        gdk_error_trap_push ();
        XChangeKeyboardControl (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                KBKeyClickPercent | KBBellPercent | KBBellPitch | KBBellDuration,
                                &kbdcontrol);

        XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), FALSE);
        gdk_error_trap_pop_ignored ();
}

static void
apply_all_settings (CsdKeyboardManager *manager)
{
    apply_bell (manager);
}

static void
settings_changed (GSettings          *settings,
                  const char         *key,
                  CsdKeyboardManager *manager)
{
    if (g_strcmp0 (key, KEY_CLICK) == 0||
        g_strcmp0 (key, KEY_CLICK_VOLUME) == 0 ||
        g_strcmp0 (key, KEY_BELL_PITCH) == 0 ||
        g_strcmp0 (key, KEY_BELL_DURATION) == 0 ||
        g_strcmp0 (key, KEY_BELL_MODE) == 0) {
        g_debug ("Bell setting '%s' changed, applying bell settings", key);
        apply_bell (manager);
    } else if (g_strcmp0 (key, KEY_BELL_CUSTOM_FILE) == 0){
        g_debug ("Ignoring '%s' setting change", KEY_BELL_CUSTOM_FILE);
    } else {
        g_warning ("Unhandled settings change, key '%s'", key);
    }

}

static void
device_added_cb (GdkDeviceManager   *device_manager,
                 GdkDevice          *device,
                 CsdKeyboardManager *manager)
{
        GdkInputSource source;

        source = gdk_device_get_source (device);
        if (source == GDK_SOURCE_TOUCHSCREEN) {
                update_gtk_im_module (manager);
        }
}

static void
device_removed_cb (GdkDeviceManager   *device_manager,
                   GdkDevice          *device,
                   CsdKeyboardManager *manager)
{
        GdkInputSource source;

        source = gdk_device_get_source (device);
        if (source == GDK_SOURCE_TOUCHSCREEN)
                update_gtk_im_module (manager);
}

static void
set_devicepresence_handler (CsdKeyboardManager *manager)
{
        GdkDeviceManager *device_manager;

        if (session_is_wayland ())
                return;

        device_manager = gdk_display_get_device_manager (gdk_display_get_default ());

        manager->device_added_id = g_signal_connect (G_OBJECT (device_manager), "device-added",
                                                           G_CALLBACK (device_added_cb), manager);
        manager->device_removed_id = g_signal_connect (G_OBJECT (device_manager), "device-removed",
                                                             G_CALLBACK (device_removed_cb), manager);
        manager->device_manager = device_manager;
}

static gboolean
need_ibus (GVariant *sources)
{
        GVariantIter iter;
        const gchar *type;

        g_variant_iter_init (&iter, sources);
        while (g_variant_iter_next (&iter, "(&s&s)", &type, NULL))
                if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
                        return TRUE;

        return FALSE;
}

static gboolean
need_osk (CsdKeyboardManager *manager)
{
        gboolean has_touchscreen = FALSE;
        GList *devices;
        GdkSeat *seat;

        if (g_settings_get_boolean (manager->a11y_settings,
                                    KEY_OSK_ENABLED))
                return TRUE;

        seat = gdk_display_get_default_seat (gdk_display_get_default ());
        devices = gdk_seat_get_slaves (seat, GDK_SEAT_CAPABILITY_TOUCH);

        has_touchscreen = devices != NULL;

        g_list_free (devices);

        return has_touchscreen;
}

static void
set_gtk_im_module (CsdKeyboardManager *manager,
                   GSettings          *settings,
                   GVariant           *sources)
{
        const gchar *new_module;
        gchar *current_module;

        if (need_ibus (sources) || need_osk (manager))
                new_module = GTK_IM_MODULE_IBUS;
        else
                new_module = GTK_IM_MODULE_SIMPLE;

        current_module = g_settings_get_string (settings, KEY_GTK_IM_MODULE);
        if (!g_str_equal (current_module, new_module))
                g_settings_set_string (settings, KEY_GTK_IM_MODULE, new_module);
        g_free (current_module);
}

static void
update_gtk_im_module (CsdKeyboardManager *manager)
{
        GSettings *interface_settings;
        GVariant *sources;

        /* Gtk+ uses the IM module advertised in XSETTINGS so, if we
         * have IBus input sources, we want it to load that
         * module. Otherwise we can use the default "simple" module
         * which is builtin gtk+
         */
        interface_settings = g_settings_new (CINNAMON_DESKTOP_INTERFACE_DIR);
        sources = g_settings_get_value (manager->input_sources_settings,
                                        KEY_INPUT_SOURCES);
        set_gtk_im_module (manager, interface_settings, sources);
        g_object_unref (interface_settings);
        g_variant_unref (sources);
}

static void
get_sources_from_xkb_config (CsdKeyboardManager *manager)
{
        GVariantBuilder builder;
        GVariant *v;
        gint i, n;
        gchar **layouts = NULL;
        gchar **variants = NULL;

        v = g_dbus_proxy_get_cached_property (manager->localed, "X11Layout");
        if (v) {
                const gchar *s = g_variant_get_string (v, NULL);
                if (*s)
                        layouts = g_strsplit (s, ",", -1);
                g_variant_unref (v);
        }

        init_builder_with_sources (&builder, manager->input_sources_settings);

        if (!layouts) {
                g_variant_builder_add (&builder, "(ss)", INPUT_SOURCE_TYPE_XKB, DEFAULT_LAYOUT);
                goto out;
    }

        v = g_dbus_proxy_get_cached_property (manager->localed, "X11Variant");
        if (v) {
                const gchar *s = g_variant_get_string (v, NULL);
                if (*s)
                        variants = g_strsplit (s, ",", -1);
                g_variant_unref (v);
        }

        if (variants && variants[0])
                n = MIN (g_strv_length (layouts), g_strv_length (variants));
        else
                n = g_strv_length (layouts);

        for (i = 0; i < n && layouts[i][0]; ++i) {
                gchar *id;

                if (variants && variants[i] && variants[i][0])
                        id = g_strdup_printf ("%s+%s", layouts[i], variants[i]);
                else
                        id = g_strdup (layouts[i]);

                g_variant_builder_add (&builder, "(ss)", INPUT_SOURCE_TYPE_XKB, id);
                g_free (id);
        }

out:
        g_settings_set_value (manager->input_sources_settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));

        g_strfreev (layouts);
        g_strfreev (variants);
}

static void
get_options_from_xkb_config (CsdKeyboardManager *manager)
{
        GVariant *v;
        gchar **options = NULL;

        v = g_dbus_proxy_get_cached_property (manager->localed, "X11Options");
        if (v) {
                const gchar *s = g_variant_get_string (v, NULL);
                if (*s)
                        options = g_strsplit (s, ",", -1);
                g_variant_unref (v);
        }

        if (!options)
                return;

        g_settings_set_strv (manager->input_sources_settings, KEY_KEYBOARD_OPTIONS, (const gchar * const*) options);

        g_strfreev (options);
}

static void
maybe_create_initial_settings (CsdKeyboardManager *manager)
{
        GSettings *settings;
        GVariant *sources;
        gchar **options;

        settings = manager->input_sources_settings;

        /* if we still don't have anything do some educated guesses */
        sources = g_settings_get_value (settings, KEY_INPUT_SOURCES);
        if (g_variant_n_children (sources) < 1)
                get_sources_from_xkb_config (manager);
        g_variant_unref (sources);

        options = g_settings_get_strv (settings, KEY_KEYBOARD_OPTIONS);
        if (g_strv_length (options) < 1)
                get_options_from_xkb_config (manager);
        g_strfreev (options);
}

static void
localed_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        CsdKeyboardManager *manager = data;
        GDBusProxy *proxy;
        GError *error = NULL;

        proxy = g_dbus_proxy_new_finish (res, &error);
        if (!proxy) {
                if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                        g_error_free (error);
                        return;
                }
                g_warning ("Failed to contact localed: %s", error->message);
                g_error_free (error);
        }

        manager->localed = proxy;
        maybe_create_initial_settings (manager);
}

static gboolean
start_keyboard_idle_cb (CsdKeyboardManager *manager)
{
        cinnamon_settings_profile_start (NULL);

        g_debug ("Starting keyboard manager");

        manager->settings = g_settings_new (CSD_KEYBOARD_DIR);

        set_devicepresence_handler (manager);

        manager->input_sources_settings = g_settings_new (CINNAMON_DESKTOP_INPUT_SOURCES_DIR);
        g_signal_connect_swapped (manager->input_sources_settings,
                                  "changed::" KEY_INPUT_SOURCES,
                                  G_CALLBACK (update_gtk_im_module), manager);

        manager->a11y_settings = g_settings_new (CINNAMON_A11Y_APPLICATIONS_INTERFACE_DIR);
        g_signal_connect_swapped (manager->a11y_settings,
                                  "changed::" KEY_OSK_ENABLED,
                                  G_CALLBACK (update_gtk_im_module), manager);
        update_gtk_im_module (manager);

        manager->cancellable = g_cancellable_new ();

        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.freedesktop.locale1",
                                  "/org/freedesktop/locale1",
                                  "org.freedesktop.locale1",
                                  manager->cancellable,
                                  localed_proxy_ready,
                                  manager);

        if (!session_is_wayland ()) {
                /* apply current settings before we install the callback */
                g_debug ("Started the keyboard plugin, applying all settings");
                apply_all_settings (manager);

                g_signal_connect (G_OBJECT (manager->settings), "changed",
                                  G_CALLBACK (settings_changed), manager);
        }

        cinnamon_settings_profile_end (NULL);

        manager->start_idle_id = 0;

        return FALSE;
}

gboolean
csd_keyboard_manager_start (CsdKeyboardManager *manager,
                            GError            **error)
{
        cinnamon_settings_profile_start (NULL);

        manager->start_idle_id = g_idle_add ((GSourceFunc) start_keyboard_idle_cb, manager);
        g_source_set_name_by_id (manager->start_idle_id, "[gnome-settings-daemon] start_keyboard_idle_cb");

        cinnamon_settings_profile_end (NULL);

        return TRUE;
}

void
csd_keyboard_manager_stop (CsdKeyboardManager *manager)
{
        g_debug ("Stopping keyboard manager");

        g_cancellable_cancel (manager->cancellable);
        g_clear_object (&manager->cancellable);

        g_clear_object (&manager->settings);
        g_clear_object (&manager->input_sources_settings);
        g_clear_object (&manager->a11y_settings);
        g_clear_object (&manager->localed);

        if (manager->device_manager != NULL) {
                g_signal_handler_disconnect (manager->device_manager, manager->device_added_id);
                g_signal_handler_disconnect (manager->device_manager, manager->device_removed_id);
                manager->device_manager = NULL;
        }
}

static void
csd_keyboard_manager_class_init (CsdKeyboardManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = csd_keyboard_manager_finalize;
}

static void
csd_keyboard_manager_init (CsdKeyboardManager *manager)
{
}

static void
csd_keyboard_manager_finalize (GObject *object)
{
        CsdKeyboardManager *keyboard_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_KEYBOARD_MANAGER (object));

        keyboard_manager = CSD_KEYBOARD_MANAGER (object);

        g_return_if_fail (keyboard_manager != NULL);

        csd_keyboard_manager_stop (keyboard_manager);

        if (keyboard_manager->start_idle_id != 0)
                g_source_remove (keyboard_manager->start_idle_id);

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

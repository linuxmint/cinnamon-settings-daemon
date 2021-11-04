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

#include <act/act.h>

#include "cinnamon-settings-profile.h"
#include "csd-keyboard-manager.h"
#include "csd-input-helper.h"
#include "csd-enums.h"

#define CSD_KEYBOARD_DIR "org.cinnamon.settings-daemon.peripherals.keyboard"

#define KEY_CLICK          "click"
#define KEY_CLICK_VOLUME   "click-volume"

#define KEY_BELL_VOLUME    "bell-volume"
#define KEY_BELL_PITCH     "bell-pitch"
#define KEY_BELL_DURATION  "bell-duration"
#define KEY_BELL_MODE      "bell-mode"
#define KEY_BELL_CUSTOM_FILE "bell-custom-file"

#define GNOME_DESKTOP_INTERFACE_DIR "org.cinnamon.desktop.interface"

#define KEY_GTK_IM_MODULE    "gtk-im-module"
#define GTK_IM_MODULE_SIMPLE "gtk-im-context-simple"
#define GTK_IM_MODULE_IBUS   "ibus"

#define GNOME_DESKTOP_INPUT_SOURCES_DIR "org.cinnamon.desktop.input-sources"

#define KEY_INPUT_SOURCES        "sources"
#define KEY_KEYBOARD_OPTIONS     "xkb-options"

#define INPUT_SOURCE_TYPE_XKB  "xkb"
#define INPUT_SOURCE_TYPE_IBUS "ibus"

#define DEFAULT_LAYOUT "us"

#define GNOME_A11Y_APPLICATIONS_INTERFACE_DIR "org.cinnamon.desktop.a11y.applications"
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

static gboolean
schema_is_installed (const char *schema)
{
        GSettingsSchemaSource *source = NULL;
        gchar **non_relocatable = NULL;
        gchar **relocatable = NULL;
        gboolean installed = FALSE;

        source = g_settings_schema_source_get_default ();
        if (!source)
                return FALSE;

        g_settings_schema_source_list_schemas (source, TRUE, &non_relocatable, &relocatable);

        if (g_strv_contains ((const gchar * const *)non_relocatable, schema) ||
            g_strv_contains ((const gchar * const *)relocatable, schema))
                installed = TRUE;

        g_strfreev (non_relocatable);
        g_strfreev (relocatable);
        return installed;
}

static const gchar *
engine_from_locale (void)
{
        const gchar *locale;
        const gchar *locale_engine[][2] = {
                { "as_IN", "m17n:as:phonetic" },
                { "bn_IN", "m17n:bn:inscript" },
                { "gu_IN", "m17n:gu:inscript" },
                { "hi_IN", "m17n:hi:inscript" },
                { "ja_JP", "mozc-jp" },
                { "kn_IN", "m17n:kn:kgp" },
                { "ko_KR", "hangul" },
                { "mai_IN", "m17n:mai:inscript" },
                { "ml_IN", "m17n:ml:inscript" },
                { "mr_IN", "m17n:mr:inscript" },
                { "or_IN", "m17n:or:inscript" },
                { "pa_IN", "m17n:pa:inscript" },
                { "sd_IN", "m17n:sd:inscript" },
                { "ta_IN", "m17n:ta:tamil99" },
                { "te_IN", "m17n:te:inscript" },
                { "zh_CN", "libpinyin" },
                { "zh_HK", "cangjie3" },
                { "zh_TW", "chewing" },
        };
        gint i;

        locale = setlocale (LC_CTYPE, NULL);
        if (!locale)
                return NULL;

        for (i = 0; i < G_N_ELEMENTS (locale_engine); ++i)
                if (g_str_has_prefix (locale, locale_engine[i][0]))
                        return locale_engine[i][1];

        return NULL;
}

static void
add_ibus_sources_from_locale (GSettings *settings)
{
        const gchar *locale_engine;
        GVariantBuilder builder;

        locale_engine = engine_from_locale ();
        if (!locale_engine)
                return;

        init_builder_with_sources (&builder, settings);
        g_variant_builder_add (&builder, "(ss)", INPUT_SOURCE_TYPE_IBUS, locale_engine);
        g_settings_set_value (settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));
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
user_notify_is_loaded_cb (GObject    *object,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
        ActUser *user = ACT_USER (object);
        GSettings *settings = user_data;

        if (act_user_is_loaded (user)) {
                GVariant *sources;
                GVariantIter iter;
                const gchar *type;
                const gchar *name;
                GVariantBuilder builder;

                g_signal_handlers_disconnect_by_data (user, user_data);

                sources = g_settings_get_value (settings, KEY_INPUT_SOURCES);

                g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{ss}"));

                g_variant_iter_init (&iter, sources);
                while (g_variant_iter_next (&iter, "(&s&s)", &type, &name)) {
                        g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{ss}"));
                        g_variant_builder_add (&builder, "{ss}", type, name);
                        g_variant_builder_close (&builder);
                }

                g_variant_unref (sources);

                sources = g_variant_ref_sink (g_variant_builder_end (&builder));
                act_user_set_input_sources (user, sources);
                g_variant_unref (sources);
        }
}

static void
manager_notify_is_loaded_cb (GObject    *object,
                             GParamSpec *pspec,
                             gpointer    user_data)
{
        ActUserManager *manager = ACT_USER_MANAGER (object);

        gboolean loaded;
        g_object_get (manager, "is-loaded", &loaded, NULL);

        if (loaded) {
                ActUser *user;

                g_signal_handlers_disconnect_by_data (manager, user_data);

                user = act_user_manager_get_user (manager, g_get_user_name ());

                if (act_user_is_loaded (user))
                        user_notify_is_loaded_cb (G_OBJECT (user), NULL, user_data);
                else
                        g_signal_connect (user, "notify::is-loaded",
                                          user_notify_is_loaded_cb, user_data);
        }
}


static void
update_gtk_im_module (CsdKeyboardManager *manager)
{
        GSettings *interface_settings;
        GVariant *sources;
        ActUserManager *user_manager;
        gboolean user_manager_loaded;
        /* Gtk+ uses the IM module advertised in XSETTINGS so, if we
         * have IBus input sources, we want it to load that
         * module. Otherwise we can use the default "simple" module
         * which is builtin gtk+
         */
        interface_settings = g_settings_new (GNOME_DESKTOP_INTERFACE_DIR);
        sources = g_settings_get_value (manager->input_sources_settings,
                                        KEY_INPUT_SOURCES);
        set_gtk_im_module (manager, interface_settings, sources);
        g_object_unref (interface_settings);
        g_variant_unref (sources);

        user_manager = act_user_manager_get_default ();
        g_object_get (user_manager, "is-loaded", &user_manager_loaded, NULL);
        if (user_manager_loaded)
                manager_notify_is_loaded_cb (G_OBJECT (user_manager),
                                             NULL,
                                             manager->input_sources_settings);
        else
                g_signal_connect (user_manager,
                                  "notify::is-loaded",
                                  G_CALLBACK (manager_notify_is_loaded_cb),
                                  manager->input_sources_settings);
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
convert_libgnomekbd_options (GSettings *settings)
{
        GPtrArray *opt_array;
        GSettings *libgnomekbd_settings;
        gchar **options, **o;

        if (!schema_is_installed ("org.gnome.libgnomekbd.keyboard"))
                return;

        opt_array = g_ptr_array_new_with_free_func (g_free);

        libgnomekbd_settings = g_settings_new ("org.gnome.libgnomekbd.keyboard");
        options = g_settings_get_strv (libgnomekbd_settings, "options");

        for (o = options; *o; ++o) {
                gchar **strv;

                strv = g_strsplit (*o, "\t", 2);
                if (strv[0] && strv[1])
                        g_ptr_array_add (opt_array, g_strdup (strv[1]));
                g_strfreev (strv);
        }
        g_ptr_array_add (opt_array, NULL);

        g_settings_set_strv (settings, KEY_KEYBOARD_OPTIONS, (const gchar * const*) opt_array->pdata);

        g_strfreev (options);
        g_object_unref (libgnomekbd_settings);
        g_ptr_array_free (opt_array, TRUE);
}

static void
convert_libgnomekbd_layouts (GSettings *settings)
{
        GVariantBuilder builder;
        GSettings *libgnomekbd_settings;
        gchar **layouts, **l;

        if (!schema_is_installed ("org.gnome.libgnomekbd.keyboard"))
                return;

        init_builder_with_sources (&builder, settings);

        libgnomekbd_settings = g_settings_new ("org.gnome.libgnomekbd.keyboard");
        layouts = g_settings_get_strv (libgnomekbd_settings, "layouts");

        for (l = layouts; *l; ++l) {
                gchar *id;
                gchar **strv;

                strv = g_strsplit (*l, "\t", 2);
                if (strv[0] && !strv[1])
                        id = g_strdup (strv[0]);
                else if (strv[0] && strv[1])
                        id = g_strdup_printf ("%s+%s", strv[0], strv[1]);
                else
                        id = NULL;

                if (id)
                        g_variant_builder_add (&builder, "(ss)", INPUT_SOURCE_TYPE_XKB, id);

                g_free (id);
                g_strfreev (strv);
        }

        g_settings_set_value (settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));

        g_strfreev (layouts);
        g_object_unref (libgnomekbd_settings);
}

static gboolean
maybe_convert_old_settings (GSettings *settings)
{
        GVariant *sources;
        gchar **options;
        gchar *stamp_dir_path = NULL;
        gchar *stamp_file_path = NULL;
        GError *error = NULL;
        gboolean is_first_run = FALSE;

        stamp_dir_path = g_build_filename (g_get_user_data_dir (), PACKAGE_NAME, NULL);
        if (g_mkdir_with_parents (stamp_dir_path, 0755)) {
                g_warning ("Failed to create directory %s: %s", stamp_dir_path, g_strerror (errno));
                goto out;
        }

        stamp_file_path = g_build_filename (stamp_dir_path, "input-sources-converted", NULL);
        if (g_file_test (stamp_file_path, G_FILE_TEST_EXISTS))
                goto out;

        is_first_run = TRUE;

        sources = g_settings_get_value (settings, KEY_INPUT_SOURCES);
        if (g_variant_n_children (sources) < 1) {
                convert_libgnomekbd_layouts (settings);
        }
        g_variant_unref (sources);

        options = g_settings_get_strv (settings, KEY_KEYBOARD_OPTIONS);
        if (g_strv_length (options) < 1)
                convert_libgnomekbd_options (settings);
        g_strfreev (options);

        if (!g_file_set_contents (stamp_file_path, "", 0, &error)) {
                g_warning ("%s", error->message);
                g_error_free (error);
        }
out:
        g_free (stamp_file_path);
        g_free (stamp_dir_path);

        return is_first_run;
}

static void
maybe_create_initial_settings (CsdKeyboardManager *manager)
{
        GSettings *settings;
        GVariant *sources;
        gchar **options;

        settings = manager->input_sources_settings;

        if (g_getenv ("RUNNING_UNDER_GDM"))
                return;

        gboolean is_first_run = maybe_convert_old_settings (settings);

        /* if we still don't have anything do some educated guesses */
        sources = g_settings_get_value (settings, KEY_INPUT_SOURCES);
        if (g_variant_n_children (sources) < 1)
                get_sources_from_xkb_config (manager);
        g_variant_unref (sources);

        if (is_first_run)
                add_ibus_sources_from_locale (settings);

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

        manager->input_sources_settings = g_settings_new (GNOME_DESKTOP_INPUT_SOURCES_DIR);
        g_signal_connect_swapped (manager->input_sources_settings,
                                  "changed::" KEY_INPUT_SOURCES,
                                  G_CALLBACK (update_gtk_im_module), manager);

        manager->a11y_settings = g_settings_new (GNOME_A11Y_APPLICATIONS_INTERFACE_DIR);
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

        /* apply current settings before we install the callback */
        g_debug ("Started the keyboard plugin, applying all settings");
        apply_all_settings (manager);

        g_signal_connect (G_OBJECT (manager->settings), "changed",
                          G_CALLBACK (settings_changed), manager);

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

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gio/gio.h>

#include "gsd-xsettings-gtk.h"

#define XSETTINGS_PLUGIN_SCHEMA "org.gnome.settings-daemon.plugins.xsettings"

#define GTK_MODULES_DISABLED_KEY "disabled-gtk-modules"
#define GTK_MODULES_ENABLED_KEY  "enabled-gtk-modules"

enum {
        PROP_0,
        PROP_GTK_MODULES
};

struct GsdXSettingsGtkPrivate {
        char              *modules;
        GHashTable        *dir_modules;

        GSettings         *settings;

        guint64            dir_mtime;
        GFileMonitor      *monitor;
        GList             *cond_settings;
};

#define GSD_XSETTINGS_GTK_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), GSD_TYPE_XSETTINGS_GTK, GsdXSettingsGtkPrivate))

G_DEFINE_TYPE(GsdXSettingsGtk, gsd_xsettings_gtk, G_TYPE_OBJECT)

static void update_gtk_modules (GsdXSettingsGtk *gtk);

static void
empty_cond_settings_list (GsdXSettingsGtk *gtk)
{
        if (gtk->priv->cond_settings == NULL)
                return;

        /* Empty the list of settings */
        g_list_foreach (gtk->priv->cond_settings, (GFunc) g_object_unref, NULL);
        g_list_free (gtk->priv->cond_settings);
        gtk->priv->cond_settings = NULL;
}

static void
cond_setting_changed (GSettings       *settings,
                      const char      *key,
                      GsdXSettingsGtk *gtk)
{
        gboolean enabled;
        const char *module_name;

        module_name = g_object_get_data (G_OBJECT (settings), "module-name");

        enabled = g_settings_get_boolean (settings, key);
        if (enabled != FALSE) {
                if (gtk->priv->dir_modules == NULL)
                        gtk->priv->dir_modules = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
                g_hash_table_insert (gtk->priv->dir_modules, g_strdup (module_name), NULL);
        } else if (gtk->priv->dir_modules != NULL) {
                g_hash_table_remove (gtk->priv->dir_modules, module_name);
        }

        update_gtk_modules (gtk);
}

static char *
process_desktop_file (const char      *path,
                      GsdXSettingsGtk *gtk)
{
        GKeyFile *keyfile;
        char *retval;
        char *module_name;

        retval = NULL;

        if (g_str_has_suffix (path, ".desktop") == FALSE &&
            g_str_has_suffix (path, ".gtk-module") == FALSE)
                return retval;

        keyfile = g_key_file_new ();
        if (g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, NULL) == FALSE)
                goto bail;

        if (g_key_file_has_group (keyfile, "GTK Module") == FALSE)
                goto bail;

        module_name = g_key_file_get_string (keyfile, "GTK Module", "X-GTK-Module-Name", NULL);
        if (module_name == NULL)
                goto bail;

        if (g_key_file_has_key (keyfile, "GTK Module", "X-GTK-Module-Enabled-Schema", NULL) != FALSE) {
                char *schema;
                char *key;
                gboolean enabled;
                GSettings *settings;
                char *signal;

                schema = g_key_file_get_string (keyfile, "GTK Module", "X-GTK-Module-Enabled-Schema", NULL);
                key = g_key_file_get_string (keyfile, "GTK Module", "X-GTK-Module-Enabled-Key", NULL);

                settings = g_settings_new (schema);
                enabled = g_settings_get_boolean (settings, key);

                gtk->priv->cond_settings = g_list_prepend (gtk->priv->cond_settings, settings);

                g_object_set_data_full (G_OBJECT (settings), "module-name", g_strdup (module_name), (GDestroyNotify) g_free);

                signal = g_strdup_printf ("changed::%s", key);
                g_signal_connect_object (G_OBJECT (settings), signal, G_CALLBACK (cond_setting_changed), gtk, 0);
                g_free (signal);
                g_free (schema);
                g_free (key);

                if (enabled != FALSE)
                        retval = g_strdup (module_name);
        } else {
                retval = g_strdup (module_name);
        }

	g_free (module_name);

bail:
        g_key_file_free (keyfile);
        return retval;
}

static void
get_gtk_modules_from_dir (GsdXSettingsGtk *gtk)
{
        GFile *file;
        GFileInfo *info;
        GHashTable *ht;

        file = g_file_new_for_path (GTK_MODULES_DIRECTORY);
        info = g_file_query_info (file,
                                  G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                  G_FILE_QUERY_INFO_NONE,
                                  NULL,
                                  NULL);
        if (info != NULL) {
                guint64 dir_mtime;

                dir_mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
                if (gtk->priv->dir_mtime == 0 ||
                    dir_mtime > gtk->priv->dir_mtime) {
                        GDir *dir;
                        const char *name;

                        empty_cond_settings_list (gtk);

                        gtk->priv->dir_mtime = dir_mtime;

                        if (gtk->priv->dir_modules != NULL) {
                                g_hash_table_destroy (gtk->priv->dir_modules);
                                gtk->priv->dir_modules = NULL;
                        }

                        dir = g_dir_open (GTK_MODULES_DIRECTORY, 0, NULL);
                        if (dir == NULL)
                                goto bail;

                        ht = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

                        while ((name = g_dir_read_name (dir)) != NULL) {
                                char *path;
                                char *module;

                                path = g_build_filename (GTK_MODULES_DIRECTORY, name, NULL);
                                module = process_desktop_file (path, gtk);
                                if (module != NULL)
                                        g_hash_table_insert (ht, module, NULL);
                                g_free (path);
                        }
                        g_dir_close (dir);

                        gtk->priv->dir_modules = ht;
                }
                g_object_unref (info);
        } else {
                empty_cond_settings_list (gtk);
        }

bail:
        g_object_unref (file);
}

static void
stringify_gtk_modules (gpointer key,
                       gpointer value,
                       GString *str)
{
        if (str->len != 0)
                g_string_append_c (str, ':');
        g_string_append (str, key);
}

static void
update_gtk_modules (GsdXSettingsGtk *gtk)
{
        char **enabled, **disabled;
        GHashTable *ht;
        guint i;
        GString *str;
        char *modules;

        enabled = g_settings_get_strv (gtk->priv->settings, GTK_MODULES_ENABLED_KEY);
        disabled = g_settings_get_strv (gtk->priv->settings, GTK_MODULES_DISABLED_KEY);

        ht = g_hash_table_new (g_str_hash, g_str_equal);

        if (gtk->priv->dir_modules != NULL) {
                GList *list, *l;

                list = g_hash_table_get_keys (gtk->priv->dir_modules);
                for (l = list; l != NULL; l = l->next) {
                        g_hash_table_insert (ht, l->data, NULL);
                }
                g_list_free (list);
        }

        for (i = 0; enabled[i] != NULL; i++)
                g_hash_table_insert (ht, enabled[i], NULL);

        for (i = 0; disabled[i] != NULL; i++)
                g_hash_table_remove (ht, disabled[i]);

        str = g_string_new (NULL);
        g_hash_table_foreach (ht, (GHFunc) stringify_gtk_modules, str);
        g_hash_table_destroy (ht);

        modules = g_string_free (str, FALSE);

        if (modules == NULL ||
            gtk->priv->modules == NULL ||
            g_str_equal (modules, gtk->priv->modules) == FALSE) {
                g_free (gtk->priv->modules);
                gtk->priv->modules = modules;
                g_object_notify (G_OBJECT (gtk), "gtk-modules");
        } else {
                g_free (modules);
        }

	g_strfreev (enabled);
	g_strfreev (disabled);
}

static void
gtk_modules_dir_changed_cb (GFileMonitor     *monitor,
                            GFile            *file,
                            GFile            *other_file,
                            GFileMonitorEvent event_type,
                            GsdXSettingsGtk  *gtk)
{
        get_gtk_modules_from_dir (gtk);
        update_gtk_modules (gtk);
}

static void
gsd_xsettings_gtk_init (GsdXSettingsGtk *gtk)
{
        GFile *file;

        gtk->priv = GSD_XSETTINGS_GTK_GET_PRIVATE (gtk);

        g_debug ("GsdXSettingsGtk initializing");

        gtk->priv->settings = g_settings_new (XSETTINGS_PLUGIN_SCHEMA);

        get_gtk_modules_from_dir (gtk);

        file = g_file_new_for_path (GTK_MODULES_DIRECTORY);
        gtk->priv->monitor = g_file_monitor (file,
                                             G_FILE_MONITOR_NONE,
                                             NULL,
                                             NULL);
        g_signal_connect (G_OBJECT (gtk->priv->monitor), "changed",
                          G_CALLBACK (gtk_modules_dir_changed_cb), gtk);
        g_object_unref (file);

        update_gtk_modules (gtk);
}

static void
gsd_xsettings_gtk_finalize (GObject *object)
{
        GsdXSettingsGtk *gtk;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_XSETTINGS_GTK (object));

        g_debug ("GsdXSettingsGtk finalizing");

        gtk = GSD_XSETTINGS_GTK (object);

        g_return_if_fail (gtk->priv != NULL);

        g_free (gtk->priv->modules);
        gtk->priv->modules = NULL;

        if (gtk->priv->dir_modules != NULL) {
                g_hash_table_destroy (gtk->priv->dir_modules);
                gtk->priv->dir_modules = NULL;
        }

        g_object_unref (gtk->priv->settings);

        if (gtk->priv->monitor != NULL)
                g_object_unref (gtk->priv->monitor);

        empty_cond_settings_list (gtk);

        G_OBJECT_CLASS (gsd_xsettings_gtk_parent_class)->finalize (object);
}

static void
gsd_xsettings_gtk_get_property (GObject        *object,
                                guint           prop_id,
                                GValue         *value,
                                GParamSpec     *pspec)
{
        GsdXSettingsGtk *self;

        self = GSD_XSETTINGS_GTK (object);

        switch (prop_id) {
        case PROP_GTK_MODULES:
                g_value_set_string (value, self->priv->modules);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gsd_xsettings_gtk_class_init (GsdXSettingsGtkClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = gsd_xsettings_gtk_get_property;
        object_class->finalize = gsd_xsettings_gtk_finalize;

        g_object_class_install_property (object_class, PROP_GTK_MODULES,
                                         g_param_spec_string ("gtk-modules", NULL, NULL,
                                                              NULL, G_PARAM_READABLE));

        g_type_class_add_private (klass, sizeof (GsdXSettingsGtkPrivate));
}

GsdXSettingsGtk *
gsd_xsettings_gtk_new (void)
{
        return GSD_XSETTINGS_GTK (g_object_new (GSD_TYPE_XSETTINGS_GTK, NULL));
}

const char *
gsd_xsettings_gtk_get_modules (GsdXSettingsGtk *gtk)
{
        return gtk->priv->modules;
}

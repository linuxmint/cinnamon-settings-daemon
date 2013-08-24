/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gio/gio.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libcinnamon-desktop/gnome-pnp-ids.h>

#include "cinnamon-settings-plugin-info.h"
#include "cinnamon-settings-manager.h"
#include "cinnamon-settings-profile.h"

#define CSD_MANAGER_DBUS_PATH "/org/cinnamon/SettingsDaemon"
#define CSD_MANAGER_DBUS_NAME "org.cinnamon.SettingsDaemon"

#define DEFAULT_SETTINGS_PREFIX "org.cinnamon.settings-daemon"

#define PLUGIN_EXT ".cinnamon-settings-plugin"

#define CINNAMON_SETTINGS_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CINNAMON_TYPE_SETTINGS_MANAGER, CinnamonSettingsManagerPrivate))

static const gchar introspection_xml[] =
"<node name='/org/cinnamon/SettingsDaemon'>"
"  <interface name='org.cinnamon.SettingsDaemon'>"
"    <annotation name='org.freedesktop.DBus.GLib.CSymbol' value='cinnamon_settings_manager'/>"
"    <signal name='PluginActivated'>"
"      <arg name='name' type='s'/>"
"    </signal>"
"    <signal name='PluginDeactivated'>"
"      <arg name='name' type='s'/>"
"    </signal>"
"  </interface>"
"</node>";

struct CinnamonSettingsManagerPrivate
{
        guint                       owner_id;
        GDBusNodeInfo              *introspection_data;
        GDBusConnection            *connection;
        GSettings                  *settings;
        GnomePnpIds                *pnp_ids;
        GSList                     *plugins;
};

static void     cinnamon_settings_manager_class_init  (CinnamonSettingsManagerClass *klass);
static void     cinnamon_settings_manager_init        (CinnamonSettingsManager      *settings_manager);
static void     cinnamon_settings_manager_finalize    (GObject                   *object);

G_DEFINE_TYPE (CinnamonSettingsManager, cinnamon_settings_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

GQuark
cinnamon_settings_manager_error_quark (void)
{
        static GQuark ret = 0;
        if (ret == 0) {
                ret = g_quark_from_static_string ("cinnamon_settings_manager_error");
        }

        return ret;
}

static void
maybe_activate_plugin (CinnamonSettingsPluginInfo *info, gpointer user_data)
{
        if (cinnamon_settings_plugin_info_get_enabled (info)) {
                gboolean res;
                res = cinnamon_settings_plugin_info_activate (info);
                if (res) {
                        g_debug ("Plugin %s: active", cinnamon_settings_plugin_info_get_location (info));
                } else {
                        g_debug ("Plugin %s: activation failed", cinnamon_settings_plugin_info_get_location (info));
                }
        } else {
                g_debug ("Plugin %s: inactive", cinnamon_settings_plugin_info_get_location (info));
        }
}

static gint
compare_location (CinnamonSettingsPluginInfo *a,
                  CinnamonSettingsPluginInfo *b)
{
        const char *loc_a;
        const char *loc_b;

        loc_a = cinnamon_settings_plugin_info_get_location (a);
        loc_b = cinnamon_settings_plugin_info_get_location (b);

        if (loc_a == NULL || loc_b == NULL) {
                return -1;
        }

        return strcmp (loc_a, loc_b);
}

static int
compare_priority (CinnamonSettingsPluginInfo *a,
                  CinnamonSettingsPluginInfo *b)
{
        int prio_a;
        int prio_b;

        prio_a = cinnamon_settings_plugin_info_get_priority (a);
        prio_b = cinnamon_settings_plugin_info_get_priority (b);

        return prio_a - prio_b;
}

static void
emit_signal (CinnamonSettingsManager    *manager,
             const char              *signal,
             const char              *name)
{
        GError *error = NULL;

        /* FIXME: maybe we should queue those up until the D-Bus
         * connection is available... */
        if (manager->priv->connection == NULL)
                return;

        if (g_dbus_connection_emit_signal (manager->priv->connection,
                                           NULL,
                                           CSD_MANAGER_DBUS_PATH,
                                           CSD_MANAGER_DBUS_NAME,
                                           "PluginActivated",
                                           g_variant_new ("(s)", name),
                                           &error) == FALSE) {
                g_debug ("Error emitting signal: %s", error->message);
                g_error_free (error);
        }

}

static void
on_plugin_activated (CinnamonSettingsPluginInfo *info,
                     CinnamonSettingsManager    *manager)
{
        const char *name;

        name = cinnamon_settings_plugin_info_get_location (info);
        g_debug ("CinnamonSettingsManager: emitting plugin-activated %s", name);
        emit_signal (manager, "PluginActivated", name);
}

static void
on_plugin_deactivated (CinnamonSettingsPluginInfo *info,
                       CinnamonSettingsManager    *manager)
{
        const char *name;

        name = cinnamon_settings_plugin_info_get_location (info);
        g_debug ("CinnamonSettingsManager: emitting plugin-deactivated %s", name);
        emit_signal (manager, "PluginDeactivated", name);
}

static gboolean
contained (const char * const *items,
           const char         *item)
{
        while (*items) {
                if (g_strcmp0 (*items++, item) == 0) {
                        return TRUE;
                }
        }

        return FALSE;
}

static gboolean
is_schema (const char *schema)
{
        return contained (g_settings_list_schemas (), schema);
}


static void
_load_file (CinnamonSettingsManager *manager,
            const char           *filename)
{
        CinnamonSettingsPluginInfo *info;
        char                    *key_name;
        GSList                  *l;

        g_debug ("Loading plugin: %s", filename);
        cinnamon_settings_profile_start ("%s", filename);

        info = cinnamon_settings_plugin_info_new_from_file (filename);
        if (info == NULL) {
                goto out;
        }

        l = g_slist_find_custom (manager->priv->plugins,
                                 info,
                                 (GCompareFunc) compare_location);
        if (l != NULL) {
                goto out;
        }

        key_name = g_strdup_printf ("%s.plugins.%s",
                                    DEFAULT_SETTINGS_PREFIX,
                                    cinnamon_settings_plugin_info_get_location (info));

        /* Ignore unknown schemas or else we'll assert */
        if (is_schema (key_name)) {
                manager->priv->plugins = g_slist_prepend (manager->priv->plugins,
                                                          g_object_ref (info));

                g_signal_connect (info, "activated",
                                  G_CALLBACK (on_plugin_activated), manager);
                g_signal_connect (info, "deactivated",
                                  G_CALLBACK (on_plugin_deactivated), manager);

                cinnamon_settings_plugin_info_set_settings_prefix (info, key_name);
        } else {
                g_warning ("Ignoring unknown module '%s'", key_name);
        }

        /* Priority is set in the call above */
        g_free (key_name);

 out:
        if (info != NULL) {
                g_object_unref (info);
        }

        cinnamon_settings_profile_end ("%s", filename);
}

static void
_load_dir (CinnamonSettingsManager *manager,
           const char           *path)
{
        GError     *error;
        GDir       *d;
        const char *name;

        g_debug ("Loading settings plugins from dir: %s", path);
        cinnamon_settings_profile_start (NULL);

        error = NULL;
        d = g_dir_open (path, 0, &error);
        if (d == NULL) {
                g_warning ("%s", error->message);
                g_error_free (error);
                return;
        }

        while ((name = g_dir_read_name (d))) {
                char *filename;

                if (!g_str_has_suffix (name, PLUGIN_EXT)) {
                        continue;
                }

                filename = g_build_filename (path, name, NULL);
                if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
                        _load_file (manager, filename);
                }
                g_free (filename);
        }

        g_dir_close (d);

        cinnamon_settings_profile_end (NULL);
}

static void
_load_all (CinnamonSettingsManager *manager)
{
        cinnamon_settings_profile_start (NULL);

        /* load system plugins */
        _load_dir (manager, CINNAMON_SETTINGS_PLUGINDIR G_DIR_SEPARATOR_S);

        manager->priv->plugins = g_slist_sort (manager->priv->plugins, (GCompareFunc) compare_priority);
        g_slist_foreach (manager->priv->plugins, (GFunc) maybe_activate_plugin, NULL);
        cinnamon_settings_profile_end (NULL);
}

static void
_unload_plugin (CinnamonSettingsPluginInfo *info, gpointer user_data)
{
        if (cinnamon_settings_plugin_info_get_enabled (info)) {
                cinnamon_settings_plugin_info_deactivate (info);
        }
        g_object_unref (info);
}

static void
_unload_all (CinnamonSettingsManager *manager)
{
         g_slist_foreach (manager->priv->plugins, (GFunc) _unload_plugin, NULL);
         g_slist_free (manager->priv->plugins);
         manager->priv->plugins = NULL;
}

static void
on_bus_gotten (GObject             *source_object,
               GAsyncResult        *res,
               CinnamonSettingsManager *manager)
{
        GDBusConnection *connection;
        GError *error = NULL;

        connection = g_bus_get_finish (res, &error);
        if (connection == NULL) {
                g_warning ("Could not get session bus: %s", error->message);
                g_error_free (error);
                return;
        }
        manager->priv->connection = connection;

        g_dbus_connection_register_object (connection,
                                           CSD_MANAGER_DBUS_PATH,
                                           manager->priv->introspection_data->interfaces[0],
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL);
}

static void
register_manager (CinnamonSettingsManager *manager)
{
        manager->priv->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
        g_assert (manager->priv->introspection_data != NULL);

        g_bus_get (G_BUS_TYPE_SESSION,
                   NULL,
                   (GAsyncReadyCallback) on_bus_gotten,
                   manager);
}

gboolean
cinnamon_settings_manager_start (CinnamonSettingsManager *manager,
                              GError              **error)
{
        gboolean ret;

        g_debug ("Starting settings manager");

        ret = FALSE;

        cinnamon_settings_profile_start (NULL);

        if (!g_module_supported ()) {
                g_warning ("cinnamon-settings-daemon is not able to initialize the plugins.");
                g_set_error (error,
                             CINNAMON_SETTINGS_MANAGER_ERROR,
                             CINNAMON_SETTINGS_MANAGER_ERROR_GENERAL,
                             "Plugins not supported");

                goto out;
        }

        g_debug ("loading PNPIDs");
        manager->priv->pnp_ids = gnome_pnp_ids_new ();

        cinnamon_settings_profile_start ("initializing plugins");
        manager->priv->settings = g_settings_new (DEFAULT_SETTINGS_PREFIX ".plugins");

        _load_all (manager);
        cinnamon_settings_profile_end ("initializing plugins");

        ret = TRUE;
 out:
        cinnamon_settings_profile_end (NULL);

        return ret;
}

void
cinnamon_settings_manager_stop (CinnamonSettingsManager *manager)
{
        g_debug ("Stopping settings manager");

        _unload_all (manager);

        if (manager->priv->owner_id > 0) {
                g_bus_unown_name (manager->priv->owner_id);
                manager->priv->owner_id = 0;
        }

        if (manager->priv->settings != NULL) {
                g_object_unref (manager->priv->settings);
                manager->priv->settings = NULL;
        }

        if (manager->priv->pnp_ids != NULL) {
                g_object_unref (manager->priv->pnp_ids);
                manager->priv->pnp_ids = NULL;
        }
}

static void
cinnamon_settings_manager_dispose (GObject *object)
{
        CinnamonSettingsManager *manager;

        manager = CINNAMON_SETTINGS_MANAGER (object);

        cinnamon_settings_manager_stop (manager);

        G_OBJECT_CLASS (cinnamon_settings_manager_parent_class)->dispose (object);
}

static void
cinnamon_settings_manager_class_init (CinnamonSettingsManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = cinnamon_settings_manager_dispose;
        object_class->finalize = cinnamon_settings_manager_finalize;

        g_type_class_add_private (klass, sizeof (CinnamonSettingsManagerPrivate));
}

static void
cinnamon_settings_manager_init (CinnamonSettingsManager *manager)
{

        manager->priv = CINNAMON_SETTINGS_MANAGER_GET_PRIVATE (manager);
}

static void
cinnamon_settings_manager_finalize (GObject *object)
{
        CinnamonSettingsManager *manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CINNAMON_IS_SETTINGS_MANAGER (object));

        manager = CINNAMON_SETTINGS_MANAGER (object);

        g_return_if_fail (manager->priv != NULL);

        G_OBJECT_CLASS (cinnamon_settings_manager_parent_class)->finalize (object);
}

CinnamonSettingsManager *
cinnamon_settings_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CINNAMON_TYPE_SETTINGS_MANAGER,
                                               NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
                register_manager (manager_object);
        }

        return CINNAMON_SETTINGS_MANAGER (manager_object);
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Bastien Nocera <hadess@hadess.net>
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

#include <locale.h>
#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <glib/gstdio.h>

#ifdef HAVE_NETWORK_MANAGER
#include <nm-client.h>
#include <nm-device.h>
#include <nm-remote-settings.h>
#endif /* HAVE_NETWORK_MANAGER */

#include "cinnamon-settings-plugin.h"
#include "cinnamon-settings-profile.h"
#include "csd-sharing-manager.h"
#include "csd-sharing-enums.h"

#define CSD_SHARING_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CSD_TYPE_SHARING_MANAGER, CsdSharingManagerPrivate))
#define GSD_DBUS_NAME         "org.gnome.SettingsDaemon"
#define GSD_DBUS_PATH "/org/gnome/SettingsDaemon"

typedef struct {
        const char  *name;
        GSettings   *settings;
        gboolean     started;
        GSubprocess *process;
} ServiceInfo;

struct CsdSharingManagerPrivate
{
        GDBusNodeInfo           *introspection_data;
        guint                    name_id;
        GDBusConnection         *connection;

        GCancellable            *cancellable;
#ifdef HAVE_NETWORK_MANAGER
        NMClient                *client;
        NMRemoteSettings        *remote_settings;
#endif /* HAVE_NETWORK_MANAGER */

        GHashTable              *services;

        char                    *current_network;
        char                    *current_network_name;
        char                    *carrier_type;
        CsdSharingStatus         sharing_status;
};

#define CSD_SHARING_DBUS_NAME GSD_DBUS_NAME ".Sharing"
#define CSD_SHARING_DBUS_PATH GSD_DBUS_PATH "/Sharing"

static const gchar introspection_xml[] =
"<node>"
"  <interface name='org.gnome.SettingsDaemon.Sharing'>"
"    <annotation name='org.freedesktop.DBus.GLib.CSymbol' value='gsd_sharing_manager'/>"
"    <property name='CurrentNetwork' type='s' access='read'/>"
"    <property name='CurrentNetworkName' type='s' access='read'/>"
"    <property name='CarrierType' type='s' access='read'/>"
"    <property name='SharingStatus' type='u' access='read'/>"
"    <method name='EnableService'>"
"      <arg name='service-name' direction='in' type='s'/>"
"    </method>"
"    <method name='DisableService'>"
"      <arg name='service-name' direction='in' type='s'/>"
"      <arg name='network' direction='in' type='s'/>"
"    </method>"
"    <method name='ListNetworks'>"
"      <arg name='service-name' direction='in' type='s'/>"
"      <arg name='networks' direction='out' type='a(sss)'/>"
"    </method>"
"  </interface>"
"</node>";

static void     csd_sharing_manager_class_init  (CsdSharingManagerClass *klass);
static void     csd_sharing_manager_init        (CsdSharingManager      *manager);
static void     csd_sharing_manager_finalize    (GObject                *object);

G_DEFINE_TYPE (CsdSharingManager, csd_sharing_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static const char * const services[] = {
        "rygel",
        "vino-server",
        "gnome-user-share-webdav"
};

static void
csd_sharing_manager_start_service (CsdSharingManager *manager,
                                   ServiceInfo       *service)
{
        GDesktopAppInfo *app;
        const char *exec;
        char *desktop, **argvp;
        GError *error = NULL;

        if (service->started)
                return;
        g_debug ("About to start %s", service->name);

        desktop = g_strdup_printf ("%s.desktop", service->name);
        app = g_desktop_app_info_new (desktop);
        g_free (desktop);

        if (!app) {
                g_warning ("Could not find desktop file for service '%s'", service->name);
                return;
        }

        exec = g_app_info_get_commandline (G_APP_INFO (app));

        if (!g_shell_parse_argv (exec, NULL, &argvp, &error)) {
                g_warning ("Could not parse command-line '%s': %s", exec, error->message);
                g_error_free (error);
                g_object_unref (app);
                return;
        }

        service->process = g_subprocess_newv ((const gchar * const*) argvp, G_SUBPROCESS_FLAGS_NONE, &error);

        if (!service->process) {
                g_warning ("Could not start command-line '%s': %s", exec, error->message);
                g_error_free (error);
                service->started = FALSE;
        } else {
                service->started = TRUE;
        }

        g_strfreev (argvp);
        g_object_unref (app);
}

#ifdef HAVE_NETWORK_MANAGER
static gboolean
service_is_enabled_on_current_connection (CsdSharingManager *manager,
                                          ServiceInfo       *service)
{
        char **connections;
        int j;
        gboolean ret;
        connections = g_settings_get_strv (service->settings, "enabled-connections");
        ret = FALSE;
        for (j = 0; connections[j] != NULL; j++) {
                if (g_strcmp0 (connections[j], manager->priv->current_network) == 0) {
                        ret = TRUE;
                        break;
                }
        }

        g_strfreev (connections);
        return ret;
}
#else
static gboolean
service_is_enabled_on_current_connection (CsdSharingManager *manager,
                                          ServiceInfo       *service)
{
        return FALSE;
}
#endif /* HAVE_NETWORK_MANAGER */

static void
csd_sharing_manager_stop_service (CsdSharingManager *manager,
                                  ServiceInfo       *service)
{
        if (!service->started ||
            service->process == NULL) {
                    return;
        }

        g_debug ("About to stop %s", service->name);

        g_subprocess_send_signal (service->process, SIGTERM);
        g_clear_object (&service->process);
        service->started = FALSE;
}

static void
csd_sharing_manager_sync_services (CsdSharingManager *manager)
{
        GList *services, *l;

        services = g_hash_table_get_values (manager->priv->services);

        for (l = services; l != NULL; l = l->next) {
                ServiceInfo *service = l->data;
                gboolean should_be_started = FALSE;

                if (manager->priv->sharing_status == CSD_SHARING_STATUS_AVAILABLE &&
                    service_is_enabled_on_current_connection (manager, service))
                        should_be_started = TRUE;

                if (service->started != should_be_started) {
                        if (service->started)
                                csd_sharing_manager_stop_service (manager, service);
                        else
                                csd_sharing_manager_start_service (manager, service);
                }
        }
        g_list_free (services);
}

#ifdef HAVE_NETWORK_MANAGER
static void
properties_changed (CsdSharingManager *manager)
{
        GVariantBuilder props_builder;
        GVariant *props_changed = NULL;

        /* not yet connected to the session bus */
        if (manager->priv->connection == NULL)
                return;

        g_variant_builder_init (&props_builder, G_VARIANT_TYPE ("a{sv}"));

        g_variant_builder_add (&props_builder, "{sv}", "CurrentNetwork",
                               g_variant_new_string (manager->priv->current_network));
        g_variant_builder_add (&props_builder, "{sv}", "CurrentNetworkName",
                               g_variant_new_string (manager->priv->current_network_name));
        g_variant_builder_add (&props_builder, "{sv}", "CarrierType",
                               g_variant_new_string (manager->priv->carrier_type));
        g_variant_builder_add (&props_builder, "{sv}", "SharingStatus",
                               g_variant_new_uint32 (manager->priv->sharing_status));

        props_changed = g_variant_new ("(s@a{sv}@as)", CSD_SHARING_DBUS_NAME,
                                       g_variant_builder_end (&props_builder),
                                       g_variant_new_strv (NULL, 0));

        g_dbus_connection_emit_signal (manager->priv->connection,
                                       NULL,
                                       CSD_SHARING_DBUS_PATH,
                                       "org.freedesktop.DBus.Properties",
                                       "PropertiesChanged",
                                       props_changed, NULL);
}

static char **
get_connections_for_service (CsdSharingManager *manager,
                             const char        *service_name)
{
        ServiceInfo *service;

        service = g_hash_table_lookup (manager->priv->services, service_name);
        return g_settings_get_strv (service->settings, "enabled-connections");
}
#else
static char **
get_connections_for_service (CsdSharingManager *manager,
                             const char        *service_name)
{
        const char * const * connections [] = { NULL };
        return g_strdupv ((char **) connections);
}
#endif /* HAVE_NETWORK_MANAGER */

static gboolean
check_service (CsdSharingManager  *manager,
               const char         *service_name,
               GError            **error)
{
        if (g_hash_table_lookup (manager->priv->services, service_name))
                return TRUE;

        g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                     "Invalid service name '%s'", service_name);
        return FALSE;
}

static gboolean
csd_sharing_manager_enable_service (CsdSharingManager  *manager,
                                    const char         *service_name,
                                    GError            **error)
{
        ServiceInfo *service;
        char **connections;
        GPtrArray *array;
        guint i;

        if (!check_service (manager, service_name, error))
                return FALSE;

        if (manager->priv->sharing_status != CSD_SHARING_STATUS_AVAILABLE) {
                g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                             "Sharing cannot be enabled on this network, status is '%d'", manager->priv->sharing_status);
                return FALSE;
        }

        service = g_hash_table_lookup (manager->priv->services, service_name);
        connections = g_settings_get_strv (service->settings, "enabled-connections");
        array = g_ptr_array_new ();
        for (i = 0; connections[i] != NULL; i++) {
                if (g_strcmp0 (connections[i], manager->priv->current_network) == 0)
                        goto bail;
                g_ptr_array_add (array, connections[i]);
        }
        g_ptr_array_add (array, manager->priv->current_network);
        g_ptr_array_add (array, NULL);

        g_settings_set_strv (service->settings, "enabled-connections", (const gchar *const *) array->pdata);

bail:

        csd_sharing_manager_start_service (manager, service);

        g_ptr_array_unref (array);
        g_strfreev (connections);

        return TRUE;
}

static gboolean
csd_sharing_manager_disable_service (CsdSharingManager  *manager,
                                     const char         *service_name,
                                     const char         *network_name,
                                     GError            **error)
{
        ServiceInfo *service;
        char **connections;
        GPtrArray *array;
        guint i;

        if (!check_service (manager, service_name, error))
                return FALSE;

        service = g_hash_table_lookup (manager->priv->services, service_name);
        connections = g_settings_get_strv (service->settings, "enabled-connections");
        array = g_ptr_array_new ();
        for (i = 0; connections[i] != NULL; i++) {
                if (g_strcmp0 (connections[i], network_name) != 0)
                        g_ptr_array_add (array, connections[i]);
        }
        g_ptr_array_add (array, NULL);

        g_settings_set_strv (service->settings, "enabled-connections", (const gchar *const *) array->pdata);
        g_ptr_array_unref (array);
        g_strfreev (connections);

        if (g_str_equal (network_name, manager->priv->current_network))
                csd_sharing_manager_stop_service (manager, service);

        return TRUE;
}

#ifdef HAVE_NETWORK_MANAGER
static const char *
get_type_and_name_for_connection_uuid (CsdSharingManager *manager,
                                       const char        *uuid,
                                       const char       **name)
{
        NMRemoteConnection *conn;
        const char *type;

        if (!manager->priv->remote_settings)
                return NULL;

        conn = nm_remote_settings_get_connection_by_uuid (manager->priv->remote_settings, uuid);
        if (!conn)
                return NULL;
        type = nm_connection_get_connection_type (NM_CONNECTION (conn));
        *name = nm_connection_get_id (NM_CONNECTION (conn));

        return type;
}
#else
static const char *
get_type_and_name_for_connection_uuid (CsdSharingManager *manager,
                                       const char        *id,
                                       const char       **name)
{
        return NULL;
}
#endif /* HAVE_NETWORK_MANAGER */

#ifdef HAVE_NETWORK_MANAGER
static gboolean
connection_is_low_security (CsdSharingManager *manager,
                            const char        *uuid)
{
        NMRemoteConnection *conn;

        if (!manager->priv->remote_settings)
                return TRUE;

        conn = nm_remote_settings_get_connection_by_uuid (manager->priv->remote_settings, uuid);
        if (!conn)
                return TRUE;

        /* Disable sharing on open Wi-Fi
         * XXX: Also do this for WEP networks? */
        return (nm_connection_get_setting_wireless_security (NM_CONNECTION (conn)) == NULL);
}
#endif /* HAVE_NETWORK_MANAGER */

static GVariant *
csd_sharing_manager_list_networks (CsdSharingManager  *manager,
                                   const char         *service_name,
                                   GError            **error)
{
        char **connections;
        GVariantBuilder builder;
        guint i;

        if (!check_service (manager, service_name, error))
                return NULL;

#ifdef HAVE_NETWORK_MANAGER
        if (!manager->priv->remote_settings) {
                g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Not ready yet");
                return NULL;
        }
#endif /* HAVE_NETWORK_MANAGER */

        connections = get_connections_for_service (manager, service_name);

        g_variant_builder_init (&builder, G_VARIANT_TYPE ("(a(sss))"));
        g_variant_builder_open (&builder, G_VARIANT_TYPE ("a(sss)"));

        for (i = 0; connections[i] != NULL; i++) {
                const char *type, *name;

                type = get_type_and_name_for_connection_uuid (manager, connections[i], &name);
                if (!type)
                        continue;

                g_variant_builder_add (&builder, "(sss)", connections[i], name, type);
        }
        g_strfreev (connections);

        g_variant_builder_close (&builder);

        return g_variant_builder_end (&builder);
}

static GVariant *
handle_get_property (GDBusConnection *connection,
                     const gchar     *sender,
                     const gchar     *object_path,
                     const gchar     *interface_name,
                     const gchar     *property_name,
                     GError         **error,
                     gpointer         user_data)
{
        CsdSharingManager *manager = CSD_SHARING_MANAGER (user_data);

        /* Check session pointer as a proxy for whether the manager is in the
           start or stop state */
        if (manager->priv->connection == NULL)
                return NULL;

        if (g_strcmp0 (property_name, "CurrentNetwork") == 0) {
                return g_variant_new_string (manager->priv->current_network);
        }

        if (g_strcmp0 (property_name, "CurrentNetworkName") == 0) {
                return g_variant_new_string (manager->priv->current_network_name);
        }

        if (g_strcmp0 (property_name, "CurrentNetworkName") == 0) {
                return g_variant_new_string (manager->priv->current_network_name);
        }

        if (g_strcmp0 (property_name, "CarrierType") == 0) {
                return g_variant_new_string (manager->priv->carrier_type);
        }

        if (g_strcmp0 (property_name, "SharingStatus") == 0) {
                return g_variant_new_uint32 (manager->priv->sharing_status);
        }

        return NULL;
}

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
        CsdSharingManager *manager = (CsdSharingManager *) user_data;

        g_debug ("Calling method '%s' for sharing", method_name);

        /* Check session pointer as a proxy for whether the manager is in the
           start or stop state */
        if (manager->priv->connection == NULL)
                return;

        if (g_strcmp0 (method_name, "EnableService") == 0) {
                const char *service;
                GError *error = NULL;

                g_variant_get (parameters, "(&s)", &service);
                if (!csd_sharing_manager_enable_service (manager, service, &error))
                        g_dbus_method_invocation_take_error (invocation, error);
                else
                        g_dbus_method_invocation_return_value (invocation, NULL);
        } else if (g_strcmp0 (method_name, "DisableService") == 0) {
                const char *service;
                const char *network_name;
                GError *error = NULL;

                g_variant_get (parameters, "(&s&s)", &service, &network_name);
                if (!csd_sharing_manager_disable_service (manager, service, network_name, &error))
                        g_dbus_method_invocation_take_error (invocation, error);
                else
                        g_dbus_method_invocation_return_value (invocation, NULL);
        } else if (g_strcmp0 (method_name, "ListNetworks") == 0) {
                const char *service;
                GError *error = NULL;
                GVariant *variant;

                g_variant_get (parameters, "(&s)", &service);
                variant = csd_sharing_manager_list_networks (manager, service, &error);
                if (!variant)
                        g_dbus_method_invocation_take_error (invocation, error);
                else
                        g_dbus_method_invocation_return_value (invocation, variant);
        }
}

static const GDBusInterfaceVTable interface_vtable =
{
        handle_method_call,
        handle_get_property,
        NULL
};

static void
on_bus_gotten (GObject               *source_object,
               GAsyncResult          *res,
               CsdSharingManager     *manager)
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
                                           CSD_SHARING_DBUS_PATH,
                                           manager->priv->introspection_data->interfaces[0],
                                           &interface_vtable,
                                           manager,
                                           NULL,
                                           NULL);

        manager->priv->name_id = g_bus_own_name_on_connection (connection,
                                                               CSD_SHARING_DBUS_NAME,
                                                               G_BUS_NAME_OWNER_FLAGS_NONE,
                                                               NULL,
                                                               NULL,
                                                               NULL,
                                                               NULL);
}

#ifdef HAVE_NETWORK_MANAGER
static void
primary_connection_changed (GObject    *gobject,
                            GParamSpec *pspec,
                            gpointer    user_data)
{
        CsdSharingManager *manager = user_data;
        NMActiveConnection *a_con;

        a_con = nm_client_get_primary_connection (manager->priv->client);

        g_clear_pointer (&manager->priv->current_network, g_free);
        g_clear_pointer (&manager->priv->current_network_name, g_free);
        g_clear_pointer (&manager->priv->carrier_type, g_free);

        if (a_con) {
                manager->priv->current_network = g_strdup (nm_active_connection_get_uuid (a_con));
                manager->priv->current_network_name = g_strdup (nm_active_connection_get_id (a_con));
                manager->priv->carrier_type = g_strdup (nm_active_connection_get_connection_type (a_con));
                if (manager->priv->carrier_type == NULL)
                        manager->priv->carrier_type = g_strdup ("");
        } else {
                manager->priv->current_network = g_strdup ("");
                manager->priv->current_network_name = g_strdup ("");
                manager->priv->carrier_type = g_strdup ("");
        }

        if (!a_con) {
                manager->priv->sharing_status = CSD_SHARING_STATUS_OFFLINE;
        } else if (*(manager->priv->carrier_type) == '\0') {
                /* Missing carrier type information? */
                manager->priv->sharing_status = CSD_SHARING_STATUS_OFFLINE;
        } else if (g_str_equal (manager->priv->carrier_type, "bluetooth") ||
                   g_str_equal (manager->priv->carrier_type, "gsm") ||
                   g_str_equal (manager->priv->carrier_type, "cdma")) {
                manager->priv->sharing_status = CSD_SHARING_STATUS_DISABLED_MOBILE_BROADBAND;
        } else if (g_str_equal (manager->priv->carrier_type, "802-11-wireless")) {
                if (connection_is_low_security (manager, manager->priv->current_network))
                        manager->priv->sharing_status = CSD_SHARING_STATUS_DISABLED_LOW_SECURITY;
                else
                        manager->priv->sharing_status = CSD_SHARING_STATUS_AVAILABLE;
        } else {
                manager->priv->sharing_status = CSD_SHARING_STATUS_AVAILABLE;
        }

        g_debug ("current network: %s", manager->priv->current_network);
        g_debug ("current network name: %s", manager->priv->current_network_name);
        g_debug ("conn type: %s", manager->priv->carrier_type);
        g_debug ("status: %d", manager->priv->sharing_status);

        properties_changed (manager);
        csd_sharing_manager_sync_services (manager);
}

static void
nm_client_ready (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
        CsdSharingManager *manager = user_data;
        GError *error = NULL;
        NMClient *client;

        client = nm_client_new_finish (res, &error);
        if (!client) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Couldn't get NMClient: %s", error->message);
                g_error_free (error);
                return;
        }
        manager->priv->client = client;

        g_signal_connect (G_OBJECT (client), "notify::primary-connection",
                          G_CALLBACK (primary_connection_changed), manager);

        primary_connection_changed (NULL, NULL, manager);
}

static void
remote_settings_ready_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
        GError *error = NULL;
        CsdSharingManager *manager = user_data;
        NMRemoteSettings *remote_settings;

        remote_settings = nm_remote_settings_new_finish (res, &error);
        if (!remote_settings) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Couldn't get remote settings: %s", error->message);
                g_error_free (error);
                return;
        }

        manager->priv->remote_settings = remote_settings;
}
#else
static void
set_properties (CsdSharingManager *manager)
{
                manager->priv->current_network = g_strdup ("");
                manager->priv->carrier_type = g_strdup ("");
                manager->priv->sharing_status = CSD_SHARING_STATUS_OFFLINE;
}
#endif /* HAVE_NETWORK_MANAGER */

gboolean
csd_sharing_manager_start (CsdSharingManager *manager,
                           GError           **error)
{
        g_debug ("Starting sharing manager");
        cinnamon_settings_profile_start (NULL);

        manager->priv->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
        g_assert (manager->priv->introspection_data != NULL);

        manager->priv->cancellable = g_cancellable_new ();
#ifdef HAVE_NETWORK_MANAGER
        nm_client_new_async (manager->priv->cancellable, nm_client_ready, manager);
        nm_remote_settings_new_async (NULL, manager->priv->cancellable, remote_settings_ready_cb, manager);
#else
        set_properties (manager);
#endif /* HAVE_NETWORK_MANAGER */

        /* Start process of owning a D-Bus name */
        g_bus_get (G_BUS_TYPE_SESSION,
                   manager->priv->cancellable,
                   (GAsyncReadyCallback) on_bus_gotten,
                   manager);

        cinnamon_settings_profile_end (NULL);
        return TRUE;
}

void
csd_sharing_manager_stop (CsdSharingManager *manager)
{
        g_debug ("Stopping sharing manager");

        manager->priv->sharing_status = CSD_SHARING_STATUS_OFFLINE;
        csd_sharing_manager_sync_services (manager);

        if (manager->priv->cancellable) {
                g_cancellable_cancel (manager->priv->cancellable);
                g_clear_object (&manager->priv->cancellable);
        }

#ifdef HAVE_NETWORK_MANAGER
        g_clear_object (&manager->priv->client);
        g_clear_object (&manager->priv->remote_settings);
#endif /* HAVE_NETWORK_MANAGER */

        if (manager->priv->name_id != 0) {
                g_bus_unown_name (manager->priv->name_id);
                manager->priv->name_id = 0;
        }

        g_clear_pointer (&manager->priv->introspection_data, g_dbus_node_info_unref);
        g_clear_object (&manager->priv->connection);

        g_clear_pointer (&manager->priv->current_network, g_free);
        g_clear_pointer (&manager->priv->current_network_name, g_free);
        g_clear_pointer (&manager->priv->carrier_type, g_free);
}

static void
csd_sharing_manager_class_init (CsdSharingManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = csd_sharing_manager_finalize;

        g_type_class_add_private (klass, sizeof (CsdSharingManagerPrivate));
}

static void
service_free (gpointer pointer)
{
        ServiceInfo *service = pointer;

        g_clear_object (&service->settings);
        csd_sharing_manager_stop_service (NULL, service);
        g_free (service);
}

static void
csd_sharing_manager_init (CsdSharingManager *manager)
{
        guint i;

        manager->priv = CSD_SHARING_MANAGER_GET_PRIVATE (manager);
        manager->priv->services = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, service_free);

        /* Default state */
        manager->priv->current_network = g_strdup ("");
        manager->priv->current_network_name = g_strdup ("");
        manager->priv->carrier_type = g_strdup ("");
        manager->priv->sharing_status = CSD_SHARING_STATUS_OFFLINE;

        for (i = 0; i < G_N_ELEMENTS (services); i++) {
                ServiceInfo *service;
                char *path;

                service = g_new0 (ServiceInfo, 1);
                service->name = services[i];
                path = g_strdup_printf ("/org/cinnamon/settings-daemon/plugins/sharing/%s/", services[i]);
                service->settings = g_settings_new_with_path ("org.cinnamon.settings-daemon.plugins.sharing.service", path);
                g_free (path);

                g_hash_table_insert (manager->priv->services, (gpointer) services[i], service);
        }
}

static void
csd_sharing_manager_finalize (GObject *object)
{
        CsdSharingManager *manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_SHARING_MANAGER (object));

        manager = CSD_SHARING_MANAGER (object);

        g_return_if_fail (manager->priv != NULL);

        csd_sharing_manager_stop (manager);

        g_hash_table_unref (manager->priv->services);

        G_OBJECT_CLASS (csd_sharing_manager_parent_class)->finalize (object);
}

CsdSharingManager *
csd_sharing_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CSD_TYPE_SHARING_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return CSD_SHARING_MANAGER (manager_object);
}

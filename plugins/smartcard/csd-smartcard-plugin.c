/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "cinnamon-settings-plugin.h"
#include "csd-smartcard-plugin.h"
#include "csd-smartcard-manager.h"

struct CsdSmartcardPluginPrivate {
        CsdSmartcardManager *manager;
        GDBusConnection     *bus_connection;

        guint32              is_active : 1;
};

typedef enum
{
    CSD_SMARTCARD_REMOVE_ACTION_NONE,
    CSD_SMARTCARD_REMOVE_ACTION_LOCK_SCREEN,
    CSD_SMARTCARD_REMOVE_ACTION_FORCE_LOGOUT,
} CsdSmartcardRemoveAction;

#define SCREENSAVER_DBUS_NAME      "org.cinnamon.ScreenSaver"
#define SCREENSAVER_DBUS_PATH      "/"
#define SCREENSAVER_DBUS_INTERFACE "org.cinnamon.ScreenSaver"

#define SM_DBUS_NAME      "org.gnome.SessionManager"
#define SM_DBUS_PATH      "/org/gnome/SessionManager"
#define SM_DBUS_INTERFACE "org.gnome.SessionManager"
#define SM_LOGOUT_MODE_FORCE 2

#define KEY_REMOVE_ACTION "removal-action"

#define CSD_SMARTCARD_PLUGIN_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), CSD_TYPE_SMARTCARD_PLUGIN, CsdSmartcardPluginPrivate))

CINNAMON_SETTINGS_PLUGIN_REGISTER (CsdSmartcardPlugin, csd_smartcard_plugin);

static void
simulate_user_activity (CsdSmartcardPlugin *plugin)
{
        GDBusProxy *screensaver_proxy;

        g_debug ("CsdSmartcardPlugin telling screensaver about smart card insertion");
        screensaver_proxy = g_dbus_proxy_new_sync (plugin->priv->bus_connection,
                                                   0, NULL,
                                                   SCREENSAVER_DBUS_NAME,
                                                   SCREENSAVER_DBUS_PATH,
                                                   SCREENSAVER_DBUS_INTERFACE,
                                                   NULL, NULL);

        g_dbus_proxy_call (screensaver_proxy,
                           "SimulateUserActivity",
                           NULL, G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);

        g_object_unref (screensaver_proxy);
}

static void
lock_screen (CsdSmartcardPlugin *plugin)
{
        GDBusProxy *screensaver_proxy;

        g_debug ("CsdSmartcardPlugin telling screensaver to lock screen");
        screensaver_proxy = g_dbus_proxy_new_sync (plugin->priv->bus_connection,
                                                   0, NULL,
                                                   SCREENSAVER_DBUS_NAME,
                                                   SCREENSAVER_DBUS_PATH,
                                                   SCREENSAVER_DBUS_INTERFACE,
                                                   NULL, NULL);

        g_dbus_proxy_call (screensaver_proxy,
                           "Lock",
                           NULL, G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);

        g_object_unref (screensaver_proxy);
}

static void
force_logout (CsdSmartcardPlugin *plugin)
{
        GDBusProxy *sm_proxy;
        GError     *error;
        GVariant   *res;

        g_debug ("CsdSmartcardPlugin telling session manager to force logout");
        sm_proxy = g_dbus_proxy_new_sync (plugin->priv->bus_connection,
                                          0, NULL,
                                          SM_DBUS_NAME,
                                          SM_DBUS_PATH,
                                          SM_DBUS_INTERFACE,
                                          NULL, NULL);

        error = NULL;
        res = g_dbus_proxy_call_sync (sm_proxy,
                                      "Logout",
                                      g_variant_new ("(i)", SM_LOGOUT_MODE_FORCE),
                                      G_DBUS_CALL_FLAGS_NONE,
                                      -1, NULL, &error);

        if (! res) {
                g_warning ("CsdSmartcardPlugin Unable to force logout: %s", error->message);
                g_error_free (error);
        } else
                g_variant_unref (res);

        g_object_unref (sm_proxy);
}

static void
csd_smartcard_plugin_init (CsdSmartcardPlugin *plugin)
{
        plugin->priv = CSD_SMARTCARD_PLUGIN_GET_PRIVATE (plugin);

        g_debug ("CsdSmartcardPlugin initializing");

        plugin->priv->manager = csd_smartcard_manager_new (NULL);
}

static void
csd_smartcard_plugin_finalize (GObject *object)
{
        CsdSmartcardPlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_SMARTCARD_PLUGIN (object));

        g_debug ("CsdSmartcardPlugin finalizing");

        plugin = CSD_SMARTCARD_PLUGIN (object);

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL) {
                g_object_unref (plugin->priv->manager);
        }

        G_OBJECT_CLASS (csd_smartcard_plugin_parent_class)->finalize (object);
}

static void
smartcard_inserted_cb (CsdSmartcardManager *card_monitor,
                       CsdSmartcard        *card,
                       CsdSmartcardPlugin  *plugin)
{
        char *name;

        name = csd_smartcard_get_name (card);
        g_debug ("CsdSmartcardPlugin smart card '%s' inserted", name);
        g_free (name);

        simulate_user_activity (plugin);
}

static gboolean
user_logged_in_with_smartcard (void)
{
    return g_getenv ("PKCS11_LOGIN_TOKEN_NAME") != NULL;
}

static CsdSmartcardRemoveAction
get_configured_remove_action (CsdSmartcardPlugin *plugin)
{
        GSettings *settings;
        char *remove_action_string;
        CsdSmartcardRemoveAction remove_action;

        settings = g_settings_new ("org.cinnamon.settings-daemon.peripherals.smartcard");
        remove_action_string = g_settings_get_string (settings, KEY_REMOVE_ACTION);

        if (remove_action_string == NULL) {
                g_warning ("CsdSmartcardPlugin unable to get smartcard remove action");
                remove_action = CSD_SMARTCARD_REMOVE_ACTION_NONE;
        } else if (strcmp (remove_action_string, "none") == 0) {
                remove_action = CSD_SMARTCARD_REMOVE_ACTION_NONE;
        } else if (strcmp (remove_action_string, "lock_screen") == 0) {
                remove_action = CSD_SMARTCARD_REMOVE_ACTION_LOCK_SCREEN;
        } else if (strcmp (remove_action_string, "force_logout") == 0) {
                remove_action = CSD_SMARTCARD_REMOVE_ACTION_FORCE_LOGOUT;
        } else {
                g_warning ("CsdSmartcardPlugin unknown smartcard remove action");
                remove_action = CSD_SMARTCARD_REMOVE_ACTION_NONE;
        }

        g_object_unref (settings);

        return remove_action;
}

static void
process_smartcard_removal (CsdSmartcardPlugin *plugin)
{
        CsdSmartcardRemoveAction remove_action;

        g_debug ("CsdSmartcardPlugin processing smartcard removal");
        remove_action = get_configured_remove_action (plugin);

        switch (remove_action)
        {
            case CSD_SMARTCARD_REMOVE_ACTION_NONE:
                return;
            case CSD_SMARTCARD_REMOVE_ACTION_LOCK_SCREEN:
                lock_screen (plugin);
                break;
            case CSD_SMARTCARD_REMOVE_ACTION_FORCE_LOGOUT:
                force_logout (plugin);
                break;
        }
}

static void
smartcard_removed_cb (CsdSmartcardManager *card_monitor,
                      CsdSmartcard        *card,
                      CsdSmartcardPlugin  *plugin)
{

        char *name;

        name = csd_smartcard_get_name (card);
        g_debug ("CsdSmartcardPlugin smart card '%s' removed", name);
        g_free (name);

        if (!csd_smartcard_is_login_card (card)) {
                g_debug ("CsdSmartcardPlugin removed smart card was not used to login");
                return;
        }

        process_smartcard_removal (plugin);
}

static void
impl_activate (CinnamonSettingsPlugin *plugin)
{
        GError *error;
        CsdSmartcardPlugin *smartcard_plugin = CSD_SMARTCARD_PLUGIN (plugin);

        if (smartcard_plugin->priv->is_active) {
                g_debug ("CsdSmartcardPlugin Not activating smartcard plugin, because it's "
                         "already active");
                return;
        }

        if (!user_logged_in_with_smartcard ()) {
                g_debug ("CsdSmartcardPlugin Not activating smartcard plugin, because user didn't use "
                         " smartcard to log in");
                smartcard_plugin->priv->is_active = FALSE;
                return;
        }

        g_debug ("CsdSmartcardPlugin Activating smartcard plugin");

        error = NULL;
        smartcard_plugin->priv->bus_connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

        if (smartcard_plugin->priv->bus_connection == NULL) {
                g_warning ("CsdSmartcardPlugin Unable to connect to session bus: %s", error->message);
                return;
        }

        if (!csd_smartcard_manager_start (smartcard_plugin->priv->manager, &error)) {
                g_warning ("CsdSmartcardPlugin Unable to start smartcard manager: %s", error->message);
                g_error_free (error);
        }

        g_signal_connect (smartcard_plugin->priv->manager,
                          "smartcard-removed",
                          G_CALLBACK (smartcard_removed_cb), smartcard_plugin);

        g_signal_connect (smartcard_plugin->priv->manager,
                          "smartcard-inserted",
                          G_CALLBACK (smartcard_inserted_cb), smartcard_plugin);

        if (!csd_smartcard_manager_login_card_is_inserted (smartcard_plugin->priv->manager)) {
                g_debug ("CsdSmartcardPlugin processing smartcard removal immediately user logged in with smartcard "
                         "and it's not inserted");
                process_smartcard_removal (smartcard_plugin);
        }

        smartcard_plugin->priv->is_active = TRUE;
}

static void
impl_deactivate (CinnamonSettingsPlugin *plugin)
{
        CsdSmartcardPlugin *smartcard_plugin = CSD_SMARTCARD_PLUGIN (plugin);

        if (!smartcard_plugin->priv->is_active) {
                g_debug ("CsdSmartcardPlugin Not deactivating smartcard plugin, "
                         "because it's already inactive");
                return;
        }

        g_debug ("CsdSmartcardPlugin Deactivating smartcard plugin");

        csd_smartcard_manager_stop (smartcard_plugin->priv->manager);

        g_signal_handlers_disconnect_by_func (smartcard_plugin->priv->manager,
                                              smartcard_removed_cb, smartcard_plugin);

        g_signal_handlers_disconnect_by_func (smartcard_plugin->priv->manager,
                                              smartcard_inserted_cb, smartcard_plugin);
        smartcard_plugin->priv->bus_connection = NULL;
        smartcard_plugin->priv->is_active = FALSE;
}

static void
csd_smartcard_plugin_class_init (CsdSmartcardPluginClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        CinnamonSettingsPluginClass *plugin_class = CINNAMON_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = csd_smartcard_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;

        g_type_class_add_private (klass, sizeof (CsdSmartcardPluginPrivate));
}

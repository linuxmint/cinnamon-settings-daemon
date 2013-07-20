/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Red Hat, Inc.
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

#include "cinnamon-settings-plugin.h"
#include "csd-print-notifications-plugin.h"
#include "csd-print-notifications-manager.h"

struct CsdPrintNotificationsPluginPrivate {
        CsdPrintNotificationsManager *manager;
};

#define CSD_PRINT_NOTIFICATIONS_PLUGIN_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), CSD_TYPE_PRINT_NOTIFICATIONS_PLUGIN, CsdPrintNotificationsPluginPrivate))

CINNAMON_SETTINGS_PLUGIN_REGISTER (CsdPrintNotificationsPlugin, csd_print_notifications_plugin)

static void
csd_print_notifications_plugin_init (CsdPrintNotificationsPlugin *plugin)
{
        if (g_strcmp0 (g_getenv ("XDG_CURRENT_DESKTOP"), "Unity") == 0) {
            plugin->priv = NULL;
            g_debug ("CsdPrintNotificationsPlugin: Disabling for Unity, using system-config-printer");
            return;
        }

        plugin->priv = CSD_PRINT_NOTIFICATIONS_PLUGIN_GET_PRIVATE (plugin);

        plugin->priv->manager = csd_print_notifications_manager_new ();
}

static void
csd_print_notifications_plugin_finalize (GObject *object)
{
        CsdPrintNotificationsPlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_PRINT_NOTIFICATIONS_PLUGIN (object));

        g_debug ("CsdPrintNotificationsPlugin finalizing");

        plugin = CSD_PRINT_NOTIFICATIONS_PLUGIN (object);

        if (g_strcmp0 (g_getenv ("XDG_CURRENT_DESKTOP"), "Unity") == 0) {
            G_OBJECT_CLASS (csd_print_notifications_plugin_parent_class)->finalize (object);
            return;
        }

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL) {
                g_object_unref (plugin->priv->manager);
        }

        G_OBJECT_CLASS (csd_print_notifications_plugin_parent_class)->finalize (object);
}

static void
impl_activate (CinnamonSettingsPlugin *plugin)
{
        gboolean res;
        GError  *error;

        if (CSD_PRINT_NOTIFICATIONS_PLUGIN (plugin)->priv == NULL) {
            g_debug ("Not activating disabled print-notifications plugin");
            return;
        }

        g_debug ("Activating print-notifications plugin");

        error = NULL;
        res = csd_print_notifications_manager_start (CSD_PRINT_NOTIFICATIONS_PLUGIN (plugin)->priv->manager, &error);
        if (! res) {
                g_warning ("Unable to start print-notifications manager: %s", error->message);
                g_error_free (error);
        }
}

static void
impl_deactivate (CinnamonSettingsPlugin *plugin)
{
        if (CSD_PRINT_NOTIFICATIONS_PLUGIN (plugin)->priv == NULL) {
            g_debug ("Not deactivating disabled print-notifications plugin");
            return;
        }

        g_debug ("Deactivating print_notifications plugin");
        csd_print_notifications_manager_stop (CSD_PRINT_NOTIFICATIONS_PLUGIN (plugin)->priv->manager);
}

static void
csd_print_notifications_plugin_class_init (CsdPrintNotificationsPluginClass *klass)
{
        GObjectClass             *object_class = G_OBJECT_CLASS (klass);
        CinnamonSettingsPluginClass *plugin_class = CINNAMON_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = csd_print_notifications_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;

        g_type_class_add_private (klass, sizeof (CsdPrintNotificationsPluginPrivate));
}

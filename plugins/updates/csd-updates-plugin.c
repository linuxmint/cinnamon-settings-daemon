/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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
#include "csd-updates-plugin.h"
#include "csd-updates-manager.h"

struct CsdUpdatesPluginPrivate {
        CsdUpdatesManager *manager;
};

#define CSD_UPDATES_PLUGIN_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), CSD_TYPE_UPDATES_PLUGIN, CsdUpdatesPluginPrivate))

CINNAMON_SETTINGS_PLUGIN_REGISTER (CsdUpdatesPlugin, csd_updates_plugin)

static void
csd_updates_plugin_init (CsdUpdatesPlugin *plugin)
{
        plugin->priv = CSD_UPDATES_PLUGIN_GET_PRIVATE (plugin);

        g_debug ("CsdUpdatesPlugin initializing");

        plugin->priv->manager = csd_updates_manager_new ();
}

static void
csd_updates_plugin_finalize (GObject *object)
{
        CsdUpdatesPlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_UPDATES_PLUGIN (object));

        g_debug ("CsdUpdatesPlugin finalizing");

        plugin = CSD_UPDATES_PLUGIN (object);

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL)
                g_object_unref (plugin->priv->manager);

        G_OBJECT_CLASS (csd_updates_plugin_parent_class)->finalize (object);
}

static void
impl_activate (CinnamonSettingsPlugin *plugin)
{
        GError *error = NULL;

        g_debug ("Activating updates plugin");

        if (!csd_updates_manager_start (CSD_UPDATES_PLUGIN (plugin)->priv->manager, &error)) {
                g_warning ("Unable to start updates manager: %s", error->message);
                g_error_free (error);
        }
}

static void
impl_deactivate (CinnamonSettingsPlugin *plugin)
{
        g_debug ("Deactivating updates plugin");
        csd_updates_manager_stop (CSD_UPDATES_PLUGIN (plugin)->priv->manager);
}

static void
csd_updates_plugin_class_init (CsdUpdatesPluginClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        CinnamonSettingsPluginClass *plugin_class = CINNAMON_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = csd_updates_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;

        g_type_class_add_private (klass, sizeof (CsdUpdatesPluginPrivate));
}

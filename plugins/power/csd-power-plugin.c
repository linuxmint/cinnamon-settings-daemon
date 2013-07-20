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
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include "cinnamon-settings-plugin.h"
#include "csd-power-plugin.h"
#include "csd-power-manager.h"

struct CsdPowerPluginPrivate {
        CsdPowerManager *manager;
};

#define CSD_POWER_PLUGIN_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), CSD_TYPE_POWER_PLUGIN, CsdPowerPluginPrivate))

CINNAMON_SETTINGS_PLUGIN_REGISTER (CsdPowerPlugin, csd_power_plugin)

static void
csd_power_plugin_init (CsdPowerPlugin *plugin)
{
        plugin->priv = CSD_POWER_PLUGIN_GET_PRIVATE (plugin);

        g_debug ("CsdPowerPlugin initializing");

        plugin->priv->manager = csd_power_manager_new ();
}

static void
csd_power_plugin_finalize (GObject *object)
{
        CsdPowerPlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_POWER_PLUGIN (object));

        g_debug ("CsdPowerPlugin finalizing");

        plugin = CSD_POWER_PLUGIN (object);

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL) {
                g_object_unref (plugin->priv->manager);
        }

        G_OBJECT_CLASS (csd_power_plugin_parent_class)->finalize (object);
}

static void
impl_activate (CinnamonSettingsPlugin *plugin)
{
        gboolean res;
        GError  *error;

        g_debug ("Activating power plugin");

        error = NULL;
        res = csd_power_manager_start (CSD_POWER_PLUGIN (plugin)->priv->manager, &error);
        if (! res) {
                g_warning ("Unable to start power manager: %s", error->message);
                g_error_free (error);
        }
}

static void
impl_deactivate (CinnamonSettingsPlugin *plugin)
{
        g_debug ("Deactivating power plugin");
        csd_power_manager_stop (CSD_POWER_PLUGIN (plugin)->priv->manager);
}

static void
csd_power_plugin_class_init (CsdPowerPluginClass *klass)
{
        GObjectClass           *object_class = G_OBJECT_CLASS (klass);
        CinnamonSettingsPluginClass *plugin_class = CINNAMON_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = csd_power_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;

        g_type_class_add_private (klass, sizeof (CsdPowerPluginPrivate));
}

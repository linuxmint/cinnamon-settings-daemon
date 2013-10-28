/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Tomas Bzatek <tbzatek@redhat.com>
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include "cinnamon-settings-plugin.h"
#include "csd-automount-plugin.h"
#include "csd-automount-manager.h"

struct CsdAutomountPluginPrivate {
        CsdAutomountManager *manager;
};

#define CSD_AUTOMOUNT_PLUGIN_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), CSD_TYPE_AUTOMOUNT_PLUGIN, CsdAutomountPluginPrivate))

CINNAMON_SETTINGS_PLUGIN_REGISTER (CsdAutomountPlugin, csd_automount_plugin)

static void
csd_automount_plugin_init (CsdAutomountPlugin *plugin)
{
        plugin->priv = CSD_AUTOMOUNT_PLUGIN_GET_PRIVATE (plugin);

        g_debug ("Automount plugin initializing");

        plugin->priv->manager = csd_automount_manager_new ();
}

static void
csd_automount_plugin_finalize (GObject *object)
{
        CsdAutomountPlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_AUTOMOUNT_PLUGIN (object));

        g_debug ("Automount plugin finalizing");

        plugin = CSD_AUTOMOUNT_PLUGIN (object);

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL) {
                g_object_unref (plugin->priv->manager);
        }

        G_OBJECT_CLASS (csd_automount_plugin_parent_class)->finalize (object);
}

static void
impl_activate (CinnamonSettingsPlugin *plugin)
{
        gboolean res;
        GError  *error;

        g_debug ("Activating automount plugin");

        error = NULL;
        res = csd_automount_manager_start (CSD_AUTOMOUNT_PLUGIN (plugin)->priv->manager, &error);
        if (! res) {
                g_warning ("Unable to start automount manager: %s", error->message);
                g_error_free (error);
        }
}

static void
impl_deactivate (CinnamonSettingsPlugin *plugin)
{
        g_debug ("Deactivating automount plugin");
        csd_automount_manager_stop (CSD_AUTOMOUNT_PLUGIN (plugin)->priv->manager);
}

static void
csd_automount_plugin_class_init (CsdAutomountPluginClass *klass)
{
        GObjectClass             *object_class = G_OBJECT_CLASS (klass);
        CinnamonSettingsPluginClass *plugin_class = CINNAMON_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = csd_automount_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;

        g_type_class_add_private (klass, sizeof (CsdAutomountPluginPrivate));
}


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include "gnome-settings-plugin.h"
#include "gsd-orientation-plugin.h"
#include "gsd-orientation-manager.h"

struct GsdOrientationPluginPrivate {
        GsdOrientationManager *manager;
};

#define GSD_ORIENTATION_PLUGIN_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), GSD_TYPE_ORIENTATION_PLUGIN, GsdOrientationPluginPrivate))

GNOME_SETTINGS_PLUGIN_REGISTER (GsdOrientationPlugin, gsd_orientation_plugin)

static void
gsd_orientation_plugin_init (GsdOrientationPlugin *plugin)
{
        plugin->priv = GSD_ORIENTATION_PLUGIN_GET_PRIVATE (plugin);

        g_debug ("GsdOrientationPlugin initializing");

        plugin->priv->manager = gsd_orientation_manager_new ();
}

static void
gsd_orientation_plugin_finalize (GObject *object)
{
        GsdOrientationPlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_ORIENTATION_PLUGIN (object));

        g_debug ("GsdOrientationPlugin finalizing");

        plugin = GSD_ORIENTATION_PLUGIN (object);

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL) {
                g_object_unref (plugin->priv->manager);
        }

        G_OBJECT_CLASS (gsd_orientation_plugin_parent_class)->finalize (object);
}

static void
impl_activate (GnomeSettingsPlugin *plugin)
{
        gboolean res;
        GError  *error;

        g_debug ("Activating orientation plugin");

        error = NULL;
        res = gsd_orientation_manager_start (GSD_ORIENTATION_PLUGIN (plugin)->priv->manager, &error);
        if (! res) {
                g_warning ("Unable to start orientation manager: %s", error->message);
                g_error_free (error);
        }
}

static void
impl_deactivate (GnomeSettingsPlugin *plugin)
{
        g_debug ("Deactivating orientation plugin");
        gsd_orientation_manager_stop (GSD_ORIENTATION_PLUGIN (plugin)->priv->manager);
}

static void
gsd_orientation_plugin_class_init (GsdOrientationPluginClass *klass)
{
        GObjectClass           *object_class = G_OBJECT_CLASS (klass);
        GnomeSettingsPluginClass *plugin_class = GNOME_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = gsd_orientation_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;

        g_type_class_add_private (klass, sizeof (GsdOrientationPluginPrivate));
}

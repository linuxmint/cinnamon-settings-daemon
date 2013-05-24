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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include "gnome-settings-plugin.h"
#include "gsd-updates-plugin.h"
#include "gsd-updates-manager.h"

struct GsdUpdatesPluginPrivate {
        GsdUpdatesManager *manager;
};

#define GSD_UPDATES_PLUGIN_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), GSD_TYPE_UPDATES_PLUGIN, GsdUpdatesPluginPrivate))

GNOME_SETTINGS_PLUGIN_REGISTER (GsdUpdatesPlugin, gsd_updates_plugin)

static void
gsd_updates_plugin_init (GsdUpdatesPlugin *plugin)
{
        plugin->priv = GSD_UPDATES_PLUGIN_GET_PRIVATE (plugin);

        g_debug ("GsdUpdatesPlugin initializing");

        plugin->priv->manager = gsd_updates_manager_new ();
}

static void
gsd_updates_plugin_finalize (GObject *object)
{
        GsdUpdatesPlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_UPDATES_PLUGIN (object));

        g_debug ("GsdUpdatesPlugin finalizing");

        plugin = GSD_UPDATES_PLUGIN (object);

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL)
                g_object_unref (plugin->priv->manager);

        G_OBJECT_CLASS (gsd_updates_plugin_parent_class)->finalize (object);
}

static void
impl_activate (GnomeSettingsPlugin *plugin)
{
        GError *error = NULL;

        g_debug ("Activating updates plugin");

        if (!gsd_updates_manager_start (GSD_UPDATES_PLUGIN (plugin)->priv->manager, &error)) {
                g_warning ("Unable to start updates manager: %s", error->message);
                g_error_free (error);
        }
}

static void
impl_deactivate (GnomeSettingsPlugin *plugin)
{
        g_debug ("Deactivating updates plugin");
        gsd_updates_manager_stop (GSD_UPDATES_PLUGIN (plugin)->priv->manager);
}

static void
gsd_updates_plugin_class_init (GsdUpdatesPluginClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GnomeSettingsPluginClass *plugin_class = GNOME_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = gsd_updates_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;

        g_type_class_add_private (klass, sizeof (GsdUpdatesPluginPrivate));
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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
#include "gsd-color-plugin.h"
#include "gsd-color-manager.h"

struct GsdColorPluginPrivate {
        GsdColorManager *manager;
};

#define GSD_COLOR_PLUGIN_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), GSD_TYPE_COLOR_PLUGIN, GsdColorPluginPrivate))

GNOME_SETTINGS_PLUGIN_REGISTER (GsdColorPlugin, gsd_color_plugin)

static void
gsd_color_plugin_init (GsdColorPlugin *plugin)
{
        plugin->priv = GSD_COLOR_PLUGIN_GET_PRIVATE (plugin);

        g_debug ("GsdColorPlugin initializing");

        plugin->priv->manager = gsd_color_manager_new ();
}

static void
gsd_color_plugin_finalize (GObject *object)
{
        GsdColorPlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_COLOR_PLUGIN (object));

        g_debug ("GsdColorPlugin finalizing");

        plugin = GSD_COLOR_PLUGIN (object);

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL) {
                g_object_unref (plugin->priv->manager);
        }

        G_OBJECT_CLASS (gsd_color_plugin_parent_class)->finalize (object);
}

static void
impl_activate (GnomeSettingsPlugin *plugin)
{
        gboolean res;
        GError  *error;

        g_debug ("Activating color plugin");

        error = NULL;
        res = gsd_color_manager_start (GSD_COLOR_PLUGIN (plugin)->priv->manager, &error);
        if (! res) {
                g_warning ("Unable to start color manager: %s", error->message);
                g_error_free (error);
        }
}

static void
impl_deactivate (GnomeSettingsPlugin *plugin)
{
        g_debug ("Deactivating color plugin");
        gsd_color_manager_stop (GSD_COLOR_PLUGIN (plugin)->priv->manager);
}

static void
gsd_color_plugin_class_init (GsdColorPluginClass *klass)
{
        GObjectClass           *object_class = G_OBJECT_CLASS (klass);
        GnomeSettingsPluginClass *plugin_class = GNOME_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = gsd_color_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;

        g_type_class_add_private (klass, sizeof (GsdColorPluginPrivate));
}

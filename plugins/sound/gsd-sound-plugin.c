/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Lennart Poettering <lennart@poettering.net>
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

#include "cinnamon-settings-plugin.h"
#include "csd-sound-plugin.h"
#include "csd-sound-manager.h"

struct CsdSoundPluginPrivate {
        CsdSoundManager *manager;
};

#define CSD_SOUND_PLUGIN_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), CSD_TYPE_SOUND_PLUGIN, CsdSoundPluginPrivate))

CINNAMON_SETTINGS_PLUGIN_REGISTER (CsdSoundPlugin, csd_sound_plugin)

static void
csd_sound_plugin_init (CsdSoundPlugin *plugin)
{
        plugin->priv = CSD_SOUND_PLUGIN_GET_PRIVATE (plugin);

        g_debug ("CsdSoundPlugin initializing");

        plugin->priv->manager = csd_sound_manager_new ();
}

static void
csd_sound_plugin_finalize (GObject *object)
{
        CsdSoundPlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_SOUND_PLUGIN (object));

        g_debug ("CsdSoundPlugin finalizing");

        plugin = CSD_SOUND_PLUGIN (object);

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL)
                g_object_unref (plugin->priv->manager);

        G_OBJECT_CLASS (csd_sound_plugin_parent_class)->finalize (object);
}

static void
impl_activate (CinnamonSettingsSettingsPlugin *plugin)
{
        GError *error = NULL;

        g_debug ("Activating sound plugin");

        if (!csd_sound_manager_start (CSD_SOUND_PLUGIN (plugin)->priv->manager, &error)) {
                g_warning ("Unable to start sound manager: %s", error->message);
                g_error_free (error);
        }
}

static void
impl_deactivate (CinnamonSettingsSettingsPlugin *plugin)
{
        g_debug ("Deactivating sound plugin");
        csd_sound_manager_stop (CSD_SOUND_PLUGIN (plugin)->priv->manager);
}

static void
csd_sound_plugin_class_init (CsdSoundPluginClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        CinnamonSettingsSettingsPluginClass *plugin_class = CINNAMON_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = csd_sound_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;

        g_type_class_add_private (klass, sizeof (CsdSoundPluginPrivate));
}

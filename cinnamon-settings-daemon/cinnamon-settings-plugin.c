/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 * Copyright (C) 2007      William Jon McCann <mccann@jhu.edu>
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
 * Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#include "config.h"

#include "cinnamon-settings-plugin.h"

G_DEFINE_TYPE (CinnamonSettingsPlugin, cinnamon_settings_plugin, G_TYPE_OBJECT)

static void
dummy (CinnamonSettingsPlugin *plugin)
{
        /* Empty */
}

static void
cinnamon_settings_plugin_class_init (CinnamonSettingsPluginClass *klass)
{
        klass->activate = dummy;
        klass->deactivate = dummy;
}

static void
cinnamon_settings_plugin_init (CinnamonSettingsPlugin *plugin)
{
        /* Empty */
}

void
cinnamon_settings_plugin_activate (CinnamonSettingsPlugin *plugin)
{
        g_return_if_fail (CINNAMON_IS_SETTINGS_PLUGIN (plugin));

        CINNAMON_SETTINGS_PLUGIN_GET_CLASS (plugin)->activate (plugin);
}

void
cinnamon_settings_plugin_deactivate  (CinnamonSettingsPlugin *plugin)
{
        g_return_if_fail (CINNAMON_IS_SETTINGS_PLUGIN (plugin));

        CINNAMON_SETTINGS_PLUGIN_GET_CLASS (plugin)->deactivate (plugin);
}

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

#ifndef __CINNAMON_XSETTINGS_PLUGIN_H__
#define __CINNAMON_XSETTINGS_PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include "cinnamon-settings-plugin.h"

G_BEGIN_DECLS

#define CINNAMON_TYPE_XSETTINGS_PLUGIN                (cinnamon_xsettings_plugin_get_type ())
#define CINNAMON_XSETTINGS_PLUGIN(o)                  (G_TYPE_CHECK_INSTANCE_CAST ((o), CINNAMON_TYPE_XSETTINGS_PLUGIN, CinnamonSettingsXSettingsPlugin))
#define CINNAMON_XSETTINGS_PLUGIN_CLASS(k)            (G_TYPE_CHECK_CLASS_CAST((k), CINNAMON_TYPE_XSETTINGS_PLUGIN, CinnamonSettingsXSettingsPluginClass))
#define CINNAMON_IS_XSETTINGS_PLUGIN(o)               (G_TYPE_CHECK_INSTANCE_TYPE ((o), CINNAMON_TYPE_XSETTINGS_PLUGIN))
#define CINNAMON_IS_XSETTINGS_PLUGIN_CLASS(k)         (G_TYPE_CHECK_CLASS_TYPE ((k), CINNAMON_TYPE_XSETTINGS_PLUGIN))
#define CINNAMON_XSETTINGS_PLUGIN_GET_CLASS(o)        (G_TYPE_INSTANCE_GET_CLASS ((o), CINNAMON_TYPE_XSETTINGS_PLUGIN, CinnamonSettingsXSettingsPluginClass))

typedef struct CinnamonSettingsXSettingsPluginPrivate CinnamonSettingsXSettingsPluginPrivate;

typedef struct
{
        CinnamonSettingsPlugin          parent;
        CinnamonSettingsXSettingsPluginPrivate *priv;
} CinnamonSettingsXSettingsPlugin;

typedef struct
{
        CinnamonSettingsPluginClass parent_class;
} CinnamonSettingsXSettingsPluginClass;

GType   cinnamon_xsettings_plugin_get_type            (void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT GType register_cinnamon_settings_plugin (GTypeModule *module);

G_END_DECLS

#endif /* __CINNAMON_XSETTINGS_PLUGIN_H__ */

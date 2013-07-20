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

#ifndef __CSD_KEYBOARD_PLUGIN_H__
#define __CSD_KEYBOARD_PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include "cinnamon-settings-plugin.h"

G_BEGIN_DECLS

#define CSD_TYPE_KEYBOARD_PLUGIN                (csd_keyboard_plugin_get_type ())
#define CSD_KEYBOARD_PLUGIN(o)                  (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_KEYBOARD_PLUGIN, CsdKeyboardPlugin))
#define CSD_KEYBOARD_PLUGIN_CLASS(k)            (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_KEYBOARD_PLUGIN, CsdKeyboardPluginClass))
#define CSD_IS_KEYBOARD_PLUGIN(o)               (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_KEYBOARD_PLUGIN))
#define CSD_IS_KEYBOARD_PLUGIN_CLASS(k)         (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_KEYBOARD_PLUGIN))
#define CSD_KEYBOARD_PLUGIN_GET_CLASS(o)        (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_KEYBOARD_PLUGIN, CsdKeyboardPluginClass))

typedef struct CsdKeyboardPluginPrivate CsdKeyboardPluginPrivate;

typedef struct
{
        CinnamonSettingsPlugin    parent;
        CsdKeyboardPluginPrivate *priv;
} CsdKeyboardPlugin;

typedef struct
{
        CinnamonSettingsPluginClass parent_class;
} CsdKeyboardPluginClass;

GType   csd_keyboard_plugin_get_type            (void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT GType register_cinnamon_settings_plugin (GTypeModule *module);

G_END_DECLS

#endif /* __CSD_KEYBOARD_PLUGIN_H__ */

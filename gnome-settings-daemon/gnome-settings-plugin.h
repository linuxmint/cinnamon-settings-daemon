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
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GNOME_SETTINGS_PLUGIN_H__
#define __GNOME_SETTINGS_PLUGIN_H__

#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS
#define GNOME_TYPE_SETTINGS_PLUGIN              (gnome_settings_plugin_get_type())
#define GNOME_SETTINGS_PLUGIN(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_TYPE_SETTINGS_PLUGIN, GnomeSettingsPlugin))
#define GNOME_SETTINGS_PLUGIN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),  GNOME_TYPE_SETTINGS_PLUGIN, GnomeSettingsPluginClass))
#define GNOME_IS_SETTINGS_PLUGIN(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_TYPE_SETTINGS_PLUGIN))
#define GNOME_IS_SETTINGS_PLUGIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_TYPE_SETTINGS_PLUGIN))
#define GNOME_SETTINGS_PLUGIN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),  GNOME_TYPE_SETTINGS_PLUGIN, GnomeSettingsPluginClass))

typedef struct
{
        GObject parent;
} GnomeSettingsPlugin;

typedef struct
{
        GObjectClass parent_class;

        /* Virtual public methods */
        void            (*activate)                     (GnomeSettingsPlugin *plugin);
        void            (*deactivate)                   (GnomeSettingsPlugin *plugin);
} GnomeSettingsPluginClass;

GType            gnome_settings_plugin_get_type           (void) G_GNUC_CONST;

void             gnome_settings_plugin_activate           (GnomeSettingsPlugin *plugin);
void             gnome_settings_plugin_deactivate         (GnomeSettingsPlugin *plugin);

/*
 * Utility macro used to register plugins
 *
 * use: GNOME_SETTINGS_PLUGIN_REGISTER (PluginName, plugin_name)
 */
#define GNOME_SETTINGS_PLUGIN_REGISTER(PluginName, plugin_name)                \
        G_DEFINE_DYNAMIC_TYPE (PluginName,                                     \
                               plugin_name,                                    \
                               GNOME_TYPE_SETTINGS_PLUGIN)                     \
                                                                               \
G_MODULE_EXPORT GType                                                          \
register_gnome_settings_plugin (GTypeModule *type_module)                      \
{                                                                              \
        plugin_name##_register_type (type_module);                             \
                                                                               \
        return plugin_name##_get_type();                                       \
}                                                                              \
                                                                               \
static void                                                                    \
plugin_name##_class_finalize (PluginName##Class *plugin_name##_class)          \
{                                                                              \
}                                                                              \

G_END_DECLS

#endif  /* __GNOME_SETTINGS_PLUGIN_H__ */

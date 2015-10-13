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

#ifndef __CINNAMON_SETTINGS_PLUGIN_H__
#define __CINNAMON_SETTINGS_PLUGIN_H__

#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS
#define CINNAMON_TYPE_SETTINGS_PLUGIN              (cinnamon_settings_plugin_get_type())
#define CINNAMON_SETTINGS_PLUGIN(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), CINNAMON_TYPE_SETTINGS_PLUGIN, CinnamonSettingsPlugin))
#define CINNAMON_SETTINGS_PLUGIN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),  CINNAMON_TYPE_SETTINGS_PLUGIN, CinnamonSettingsPluginClass))
#define CINNAMON_IS_SETTINGS_PLUGIN(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), CINNAMON_TYPE_SETTINGS_PLUGIN))
#define CINNAMON_IS_SETTINGS_PLUGIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CINNAMON_TYPE_SETTINGS_PLUGIN))
#define CINNAMON_SETTINGS_PLUGIN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),  CINNAMON_TYPE_SETTINGS_PLUGIN, CinnamonSettingsPluginClass))

typedef struct
{
        GObject parent;
} CinnamonSettingsPlugin;

typedef struct
{
        GObjectClass parent_class;

        /* Virtual public methods */
        void            (*activate)                     (CinnamonSettingsPlugin *plugin);
        void            (*deactivate)                   (CinnamonSettingsPlugin *plugin);
} CinnamonSettingsPluginClass;

GType            cinnamon_settings_plugin_get_type           (void) G_GNUC_CONST;

void             cinnamon_settings_plugin_activate           (CinnamonSettingsPlugin *plugin);
void             cinnamon_settings_plugin_deactivate         (CinnamonSettingsPlugin *plugin);

/*
 * Utility macro used to register plugins
 *
 * use: CINNAMON_SETTINGS_PLUGIN_REGISTER (PluginName, plugin_name)
 */
#define CINNAMON_SETTINGS_PLUGIN_REGISTER(PluginName, plugin_name)                \
        G_DEFINE_DYNAMIC_TYPE (PluginName,                                     \
                               plugin_name,                                    \
                               CINNAMON_TYPE_SETTINGS_PLUGIN)                     \
                                                                               \
G_MODULE_EXPORT GType                                                          \
register_cinnamon_settings_plugin (GTypeModule *type_module)                      \
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


/*
 * Utility macro used to register plugins
 *
 * use: NEW_CINNAMON_SETTINGS_PLUGIN_REGISTER (PluginName, plugin_name)
 */

 /* TODO: make all plugins use this, not just wacom */

#define NEW_CINNAMON_SETTINGS_PLUGIN_REGISTER(PluginName, plugin_name)         \
typedef struct {                                                               \
        PluginName##Manager *manager;                                          \
} PluginName##PluginPrivate;                                                   \
typedef struct {                                                               \
    CinnamonSettingsPlugin    parent;                                         \
    PluginName##PluginPrivate *priv;                                       \
} PluginName##Plugin;                                                          \
typedef struct {                                                               \
    CinnamonSettingsPluginClass parent_class;                                 \
} PluginName##PluginClass;                                                     \
GType plugin_name##_plugin_get_type (void) G_GNUC_CONST;                       \
G_MODULE_EXPORT GType register_cinnamon_settings_plugin (GTypeModule *module);    \
                                                                               \
        G_DEFINE_DYNAMIC_TYPE (PluginName##Plugin,                             \
                               plugin_name##_plugin,                           \
                               CINNAMON_TYPE_SETTINGS_PLUGIN)                     \
                                                                               \
G_MODULE_EXPORT GType                                                          \
register_cinnamon_settings_plugin (GTypeModule *type_module)                      \
{                                                                              \
        plugin_name##_plugin_register_type (type_module);                      \
                                                                               \
        return plugin_name##_plugin_get_type();                                \
}                                                                              \
                                                                               \
static void                                                                    \
plugin_name##_plugin_class_finalize (PluginName##PluginClass * plugin_name##_class) \
{                                                                              \
}                                                                              \
                                                                               \
static void                                                                    \
plugin_name##_plugin_init (PluginName##Plugin *plugin)                         \
{                                                                              \
        plugin->priv = G_TYPE_INSTANCE_GET_PRIVATE ((plugin),                  \
                plugin_name##_plugin_get_type(), PluginName##PluginPrivate);   \
        g_debug (#PluginName " initializing");                                 \
        plugin->priv->manager = plugin_name##_manager_new ();                  \
}                                                                              \
                                                                               \
static void                                                                    \
plugin_name##_plugin_finalize (GObject *object)                                \
{                                                                              \
        PluginName##Plugin *plugin;                                            \
        g_return_if_fail (object != NULL);                                     \
        g_return_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (object, plugin_name##_plugin_get_type())); \
        g_debug ("PluginName## finalizing");                                   \
        plugin = G_TYPE_CHECK_INSTANCE_CAST ((object), plugin_name##_plugin_get_type(), PluginName##Plugin); \
        g_return_if_fail (plugin->priv != NULL);                               \
        if (plugin->priv->manager != NULL)                                     \
                g_object_unref (plugin->priv->manager);                        \
        G_OBJECT_CLASS (plugin_name##_plugin_parent_class)->finalize (object);        \
}                                                                              \
                                                                               \
static void                                                                    \
impl_activate (CinnamonSettingsPlugin *plugin)                                    \
{                                                                              \
        GError *error = NULL;                                                  \
        PluginName##Plugin *plugin_cast;                                       \
        g_debug ("Activating %s plugin", G_STRINGIFY(plugin_name));            \
        plugin_cast = G_TYPE_CHECK_INSTANCE_CAST ((plugin), plugin_name##_plugin_get_type(), PluginName##Plugin); \
        if (!plugin_name##_manager_start (plugin_cast->priv->manager, &error)) { \
                g_warning ("Unable to start %s manager: %s", G_STRINGIFY(plugin_name), error ? error->message : "No reason"); \
                g_clear_error (&error);                                        \
        }                                                                      \
}                                                                              \
                                                                               \
static void                                                                    \
impl_deactivate (CinnamonSettingsPlugin *plugin)                                  \
{                                                                              \
        PluginName##Plugin *plugin_cast;                                       \
        plugin_cast = G_TYPE_CHECK_INSTANCE_CAST ((plugin), plugin_name##_plugin_get_type(), PluginName##Plugin); \
        g_debug ("Deactivating %s plugin", G_STRINGIFY (plugin_name));         \
        plugin_name##_manager_stop (plugin_cast->priv->manager); \
}                                                                              \
                                                                               \
static void                                                                    \
plugin_name##_plugin_class_init (PluginName##PluginClass *klass)               \
{                                                                              \
        GObjectClass           *object_class = G_OBJECT_CLASS (klass);         \
        CinnamonSettingsPluginClass *plugin_class = CINNAMON_SETTINGS_PLUGIN_CLASS (klass); \
                                                                               \
        object_class->finalize = plugin_name##_plugin_finalize;                \
        plugin_class->activate = impl_activate;                                \
        plugin_class->deactivate = impl_deactivate;                            \
        g_type_class_add_private (klass, sizeof (PluginName##PluginPrivate));  \
}

G_END_DECLS

#endif  /* __CINNAMON_SETTINGS_PLUGIN_H__ */

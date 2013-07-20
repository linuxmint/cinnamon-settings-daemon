/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
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

#ifndef __CINNAMON_SETTINGS_PLUGIN_INFO_H__
#define __CINNAMON_SETTINGS_PLUGIN_INFO_H__

#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS
#define CINNAMON_TYPE_SETTINGS_PLUGIN_INFO              (cinnamon_settings_plugin_info_get_type())
#define CINNAMON_SETTINGS_PLUGIN_INFO(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), CINNAMON_TYPE_SETTINGS_PLUGIN_INFO, CinnamonSettingsPluginInfo))
#define CINNAMON_SETTINGS_PLUGIN_INFO_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),  CINNAMON_TYPE_SETTINGS_PLUGIN_INFO, CinnamonSettingsPluginInfoClass))
#define CINNAMON_IS_SETTINGS_PLUGIN_INFO(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), CINNAMON_TYPE_SETTINGS_PLUGIN_INFO))
#define CINNAMON_IS_SETTINGS_PLUGIN_INFO_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), CINNAMON_TYPE_SETTINGS_PLUGIN_INFO))
#define CINNAMON_SETTINGS_PLUGIN_INFO_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),  CINNAMON_TYPE_SETTINGS_PLUGIN_INFO, CinnamonSettingsPluginInfoClass))

typedef struct CinnamonSettingsPluginInfoPrivate CinnamonSettingsPluginInfoPrivate;

typedef struct
{
        GObject                         parent;
        CinnamonSettingsPluginInfoPrivate *priv;
} CinnamonSettingsPluginInfo;

typedef struct
{
        GObjectClass parent_class;

        void          (* activated)         (CinnamonSettingsPluginInfo *info);
        void          (* deactivated)       (CinnamonSettingsPluginInfo *info);
} CinnamonSettingsPluginInfoClass;

GType            cinnamon_settings_plugin_info_get_type           (void) G_GNUC_CONST;

CinnamonSettingsPluginInfo *cinnamon_settings_plugin_info_new_from_file (const char *filename);

void             cinnamon_settings_plugin_info_set_settings_prefix (CinnamonSettingsPluginInfo *info, const char *settings_prefix);
gboolean         cinnamon_settings_plugin_info_activate        (CinnamonSettingsPluginInfo *info);
gboolean         cinnamon_settings_plugin_info_deactivate      (CinnamonSettingsPluginInfo *info);

gboolean         cinnamon_settings_plugin_info_is_active       (CinnamonSettingsPluginInfo *info);
gboolean         cinnamon_settings_plugin_info_get_enabled     (CinnamonSettingsPluginInfo *info);
gboolean         cinnamon_settings_plugin_info_is_available    (CinnamonSettingsPluginInfo *info);

const char      *cinnamon_settings_plugin_info_get_name        (CinnamonSettingsPluginInfo *info);
const char      *cinnamon_settings_plugin_info_get_description (CinnamonSettingsPluginInfo *info);
const char     **cinnamon_settings_plugin_info_get_authors     (CinnamonSettingsPluginInfo *info);
const char      *cinnamon_settings_plugin_info_get_website     (CinnamonSettingsPluginInfo *info);
const char      *cinnamon_settings_plugin_info_get_copyright   (CinnamonSettingsPluginInfo *info);
const char      *cinnamon_settings_plugin_info_get_location    (CinnamonSettingsPluginInfo *info);
int              cinnamon_settings_plugin_info_get_priority    (CinnamonSettingsPluginInfo *info);

void             cinnamon_settings_plugin_info_set_priority    (CinnamonSettingsPluginInfo *info,
                                                             int                      priority);

G_END_DECLS

#endif  /* __CINNAMON_SETTINGS_PLUGIN_INFO_H__ */

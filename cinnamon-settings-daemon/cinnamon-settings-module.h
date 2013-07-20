/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 - Paolo Maggi
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

#ifndef CINNAMON_SETTINGS_MODULE_H
#define CINNAMON_SETTINGS_MODULE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CINNAMON_TYPE_SETTINGS_MODULE               (cinnamon_settings_module_get_type ())
#define CINNAMON_SETTINGS_MODULE(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), CINNAMON_TYPE_SETTINGS_MODULE, CinnamonSettingsModule))
#define CINNAMON_SETTINGS_MODULE_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), CINNAMON_TYPE_SETTINGS_MODULE, CinnamonSettingsModuleClass))
#define CINNAMON_IS_SETTINGS_MODULE(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CINNAMON_TYPE_SETTINGS_MODULE))
#define CINNAMON_IS_SETTINGS_MODULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((obj), CINNAMON_TYPE_SETTINGS_MODULE))
#define CINNAMON_SETTINGS_MODULE_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS((obj), CINNAMON_TYPE_SETTINGS_MODULE, CinnamonSettingsModuleClass))

typedef struct _CinnamonSettingsModule CinnamonSettingsModule;

GType                    cinnamon_settings_module_get_type          (void) G_GNUC_CONST;

CinnamonSettingsModule     *cinnamon_settings_module_new               (const gchar *path);

const char              *cinnamon_settings_module_get_path          (CinnamonSettingsModule *module);

GObject                 *cinnamon_settings_module_new_object        (CinnamonSettingsModule *module);

G_END_DECLS

#endif

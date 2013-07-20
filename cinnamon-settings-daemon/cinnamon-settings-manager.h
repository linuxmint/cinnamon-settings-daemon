/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 */

#ifndef __CINNAMON_SETTINGS_MANAGER_H
#define __CINNAMON_SETTINGS_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CINNAMON_TYPE_SETTINGS_MANAGER         (cinnamon_settings_manager_get_type ())
#define CINNAMON_SETTINGS_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CINNAMON_TYPE_SETTINGS_MANAGER, CinnamonSettingsManager))
#define CINNAMON_SETTINGS_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CINNAMON_TYPE_SETTINGS_MANAGER, CinnamonSettingsManagerClass))
#define CINNAMON_IS_SETTINGS_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CINNAMON_TYPE_SETTINGS_MANAGER))
#define CINNAMON_IS_SETTINGS_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CINNAMON_TYPE_SETTINGS_MANAGER))
#define CINNAMON_SETTINGS_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CINNAMON_TYPE_SETTINGS_MANAGER, CinnamonSettingsManagerClass))

typedef struct CinnamonSettingsManagerPrivate CinnamonSettingsManagerPrivate;

typedef struct
{
        GObject                      parent;
        CinnamonSettingsManagerPrivate *priv;
} CinnamonSettingsManager;

typedef struct
{
        GObjectClass   parent_class;

        void          (* plugin_activated)         (CinnamonSettingsManager *manager,
                                                    const char           *name);
        void          (* plugin_deactivated)       (CinnamonSettingsManager *manager,
                                                    const char           *name);
} CinnamonSettingsManagerClass;

typedef enum
{
        CINNAMON_SETTINGS_MANAGER_ERROR_GENERAL
} CinnamonSettingsManagerError;

#define CINNAMON_SETTINGS_MANAGER_ERROR cinnamon_settings_manager_error_quark ()

GQuark                 cinnamon_settings_manager_error_quark         (void);
GType                  cinnamon_settings_manager_get_type   (void);

CinnamonSettingsManager * cinnamon_settings_manager_new        (void);
gboolean               cinnamon_settings_manager_start      (CinnamonSettingsManager *manager,
                                                          GError              **error);
void                   cinnamon_settings_manager_stop       (CinnamonSettingsManager *manager);

G_END_DECLS

#endif /* __CINNAMON_SETTINGS_MANAGER_H */

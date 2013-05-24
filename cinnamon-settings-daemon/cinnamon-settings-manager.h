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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __GNOME_SETTINGS_MANAGER_H
#define __GNOME_SETTINGS_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_SETTINGS_MANAGER         (gnome_settings_manager_get_type ())
#define GNOME_SETTINGS_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_SETTINGS_MANAGER, GnomeSettingsManager))
#define GNOME_SETTINGS_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GNOME_TYPE_SETTINGS_MANAGER, GnomeSettingsManagerClass))
#define GNOME_IS_SETTINGS_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_SETTINGS_MANAGER))
#define GNOME_IS_SETTINGS_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_SETTINGS_MANAGER))
#define GNOME_SETTINGS_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_SETTINGS_MANAGER, GnomeSettingsManagerClass))

typedef struct GnomeSettingsManagerPrivate GnomeSettingsManagerPrivate;

typedef struct
{
        GObject                      parent;
        GnomeSettingsManagerPrivate *priv;
} GnomeSettingsManager;

typedef struct
{
        GObjectClass   parent_class;

        void          (* plugin_activated)         (GnomeSettingsManager *manager,
                                                    const char           *name);
        void          (* plugin_deactivated)       (GnomeSettingsManager *manager,
                                                    const char           *name);
} GnomeSettingsManagerClass;

typedef enum
{
        GNOME_SETTINGS_MANAGER_ERROR_GENERAL
} GnomeSettingsManagerError;

#define GNOME_SETTINGS_MANAGER_ERROR gnome_settings_manager_error_quark ()

GQuark                 gnome_settings_manager_error_quark         (void);
GType                  gnome_settings_manager_get_type   (void);

GnomeSettingsManager * gnome_settings_manager_new        (void);
gboolean               gnome_settings_manager_start      (GnomeSettingsManager *manager,
                                                          GError              **error);
void                   gnome_settings_manager_stop       (GnomeSettingsManager *manager);

G_END_DECLS

#endif /* __GNOME_SETTINGS_MANAGER_H */

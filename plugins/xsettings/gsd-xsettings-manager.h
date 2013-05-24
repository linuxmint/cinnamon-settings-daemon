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

#ifndef __GNOME_XSETTINGS_MANAGER_H
#define __GNOME_XSETTINGS_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_XSETTINGS_MANAGER         (gnome_xsettings_manager_get_type ())
#define GNOME_XSETTINGS_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_XSETTINGS_MANAGER, GnomeXSettingsManager))
#define GNOME_XSETTINGS_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GNOME_TYPE_XSETTINGS_MANAGER, GnomeXSettingsManagerClass))
#define GNOME_IS_XSETTINGS_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_XSETTINGS_MANAGER))
#define GNOME_IS_XSETTINGS_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_XSETTINGS_MANAGER))
#define GNOME_XSETTINGS_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_XSETTINGS_MANAGER, GnomeXSettingsManagerClass))

typedef struct GnomeXSettingsManagerPrivate GnomeXSettingsManagerPrivate;

typedef struct
{
        GObject                     parent;
        GnomeXSettingsManagerPrivate *priv;
} GnomeXSettingsManager;

typedef struct
{
        GObjectClass   parent_class;
} GnomeXSettingsManagerClass;

GType                   gnome_xsettings_manager_get_type            (void);

GnomeXSettingsManager * gnome_xsettings_manager_new                 (void);
gboolean                gnome_xsettings_manager_start               (GnomeXSettingsManager *manager,
                                                                     GError               **error);
void                    gnome_xsettings_manager_stop                (GnomeXSettingsManager *manager);

G_END_DECLS

#endif /* __GNOME_XSETTINGS_MANAGER_H */

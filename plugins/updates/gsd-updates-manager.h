/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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

#ifndef __GSD_UPDATES_MANAGER_H
#define __GSD_UPDATES_MANAGER_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GSD_TYPE_UPDATES_MANAGER         (gsd_updates_manager_get_type ())
#define GSD_UPDATES_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_UPDATES_MANAGER, GsdUpdatesManager))
#define GSD_UPDATES_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GSD_TYPE_UPDATES_MANAGER, GsdUpdatesManagerClass))
#define GSD_IS_UPDATES_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_UPDATES_MANAGER))
#define GSD_IS_UPDATES_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_UPDATES_MANAGER))
#define GSD_UPDATES_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_UPDATES_MANAGER, GsdUpdatesManagerClass))

typedef struct GsdUpdatesManagerPrivate GsdUpdatesManagerPrivate;

typedef struct
{
        GObject parent;
        GsdUpdatesManagerPrivate *priv;
} GsdUpdatesManager;

typedef struct
{
        GObjectClass parent_class;
} GsdUpdatesManagerClass;

GType gsd_updates_manager_get_type (void) G_GNUC_CONST;

GsdUpdatesManager *gsd_updates_manager_new (void);
gboolean gsd_updates_manager_start (GsdUpdatesManager *manager, GError **error);
void gsd_updates_manager_stop (GsdUpdatesManager *manager);

G_END_DECLS

#endif /* __GSD_UPDATES_MANAGER_H */

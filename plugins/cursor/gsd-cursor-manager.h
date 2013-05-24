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

#ifndef __GSD_CURSOR_MANAGER_H
#define __GSD_CURSOR_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GSD_TYPE_CURSOR_MANAGER         (gsd_cursor_manager_get_type ())
#define GSD_CURSOR_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_CURSOR_MANAGER, GsdCursorManager))
#define GSD_CURSOR_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GSD_TYPE_CURSOR_MANAGER, GsdCursorManagerClass))
#define GSD_IS_CURSOR_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_CURSOR_MANAGER))
#define GSD_IS_CURSOR_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_CURSOR_MANAGER))
#define GSD_CURSOR_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_CURSOR_MANAGER, GsdCursorManagerClass))

typedef struct GsdCursorManagerPrivate GsdCursorManagerPrivate;

typedef struct
{
        GObject                     parent;
        GsdCursorManagerPrivate *priv;
} GsdCursorManager;

typedef struct
{
        GObjectClass   parent_class;
} GsdCursorManagerClass;

GType                   gsd_cursor_manager_get_type            (void);

GsdCursorManager *       gsd_cursor_manager_new                 (void);
gboolean                gsd_cursor_manager_start               (GsdCursorManager *manager,
                                                               GError         **error);
void                    gsd_cursor_manager_stop                (GsdCursorManager *manager);

G_END_DECLS

#endif /* __GSD_CURSOR_MANAGER_H */

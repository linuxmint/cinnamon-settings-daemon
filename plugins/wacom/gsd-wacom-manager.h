/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2010 Red Hat, Inc.
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

#ifndef __GSD_WACOM_MANAGER_H
#define __GSD_WACOM_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GSD_TYPE_WACOM_MANAGER         (gsd_wacom_manager_get_type ())
#define GSD_WACOM_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_WACOM_MANAGER, GsdWacomManager))
#define GSD_WACOM_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GSD_TYPE_WACOM_MANAGER, GsdWacomManagerClass))
#define GSD_IS_WACOM_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_WACOM_MANAGER))
#define GSD_IS_WACOM_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_WACOM_MANAGER))
#define GSD_WACOM_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_WACOM_MANAGER, GsdWacomManagerClass))

typedef struct GsdWacomManagerPrivate GsdWacomManagerPrivate;

typedef struct
{
        GObject                     parent;
        GsdWacomManagerPrivate *priv;
} GsdWacomManager;

typedef struct
{
        GObjectClass   parent_class;
} GsdWacomManagerClass;

GType                   gsd_wacom_manager_get_type            (void);

GsdWacomManager *       gsd_wacom_manager_new                 (void);
gboolean                gsd_wacom_manager_start               (GsdWacomManager *manager,
                                                               GError         **error);
void                    gsd_wacom_manager_stop                (GsdWacomManager *manager);

G_END_DECLS

#endif /* __GSD_WACOM_MANAGER_H */

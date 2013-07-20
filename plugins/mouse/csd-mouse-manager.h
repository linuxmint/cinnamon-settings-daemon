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

#ifndef __CSD_MOUSE_MANAGER_H
#define __CSD_MOUSE_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CSD_TYPE_MOUSE_MANAGER         (csd_mouse_manager_get_type ())
#define CSD_MOUSE_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_MOUSE_MANAGER, CsdMouseManager))
#define CSD_MOUSE_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_MOUSE_MANAGER, CsdMouseManagerClass))
#define CSD_IS_MOUSE_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_MOUSE_MANAGER))
#define CSD_IS_MOUSE_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_MOUSE_MANAGER))
#define CSD_MOUSE_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_MOUSE_MANAGER, CsdMouseManagerClass))

typedef struct CsdMouseManagerPrivate CsdMouseManagerPrivate;

typedef struct
{
        GObject                     parent;
        CsdMouseManagerPrivate *priv;
} CsdMouseManager;

typedef struct
{
        GObjectClass   parent_class;
} CsdMouseManagerClass;

GType                   csd_mouse_manager_get_type            (void);

CsdMouseManager *       csd_mouse_manager_new                 (void);
gboolean                csd_mouse_manager_start               (CsdMouseManager *manager,
                                                               GError         **error);
void                    csd_mouse_manager_stop                (CsdMouseManager *manager);

G_END_DECLS

#endif /* __CSD_MOUSE_MANAGER_H */

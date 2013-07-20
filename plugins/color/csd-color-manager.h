/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 */

#ifndef __CSD_COLOR_MANAGER_H
#define __CSD_COLOR_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CSD_TYPE_COLOR_MANAGER         (csd_color_manager_get_type ())
#define CSD_COLOR_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_COLOR_MANAGER, CsdColorManager))
#define CSD_COLOR_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_COLOR_MANAGER, CsdColorManagerClass))
#define CSD_IS_COLOR_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_COLOR_MANAGER))
#define CSD_IS_COLOR_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_COLOR_MANAGER))
#define CSD_COLOR_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_COLOR_MANAGER, CsdColorManagerClass))
#define CSD_COLOR_MANAGER_ERROR        (csd_color_manager_error_quark ())

typedef struct CsdColorManagerPrivate CsdColorManagerPrivate;

typedef struct
{
        GObject                     parent;
        CsdColorManagerPrivate *priv;
} CsdColorManager;

typedef struct
{
        GObjectClass   parent_class;
} CsdColorManagerClass;

enum
{
        CSD_COLOR_MANAGER_ERROR_FAILED
};

GType                   csd_color_manager_get_type            (void);
GQuark                  csd_color_manager_error_quark         (void);

CsdColorManager *       csd_color_manager_new                 (void);
gboolean                csd_color_manager_start               (CsdColorManager *manager,
                                                               GError         **error);
void                    csd_color_manager_stop                (CsdColorManager *manager);

G_END_DECLS

#endif /* __CSD_COLOR_MANAGER_H */

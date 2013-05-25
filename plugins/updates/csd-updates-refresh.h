/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __CSD_UPDATES_REFRESH_H
#define __CSD_UPDATES_REFRESH_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CSD_TYPE_UPDATES_REFRESH        (csd_updates_refresh_get_type ())
#define CSD_UPDATES_REFRESH(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_UPDATES_REFRESH, CsdUpdatesRefresh))
#define CSD_UPDATES_REFRESH_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_UPDATES_REFRESH, CsdUpdatesRefreshClass))
#define CSD_IS_UPDATES_REFRESH(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_UPDATES_REFRESH))

typedef struct CsdUpdatesRefreshPrivate CsdUpdatesRefreshPrivate;

typedef struct
{
         GObject                         parent;
         CsdUpdatesRefreshPrivate       *priv;
} CsdUpdatesRefresh;

typedef struct
{
        GObjectClass    parent_class;
} CsdUpdatesRefreshClass;

GType                    csd_updates_refresh_get_type           (void);
CsdUpdatesRefresh       *csd_updates_refresh_new                (void);

G_END_DECLS

#endif /* __CSD_UPDATES_REFRESH_H */

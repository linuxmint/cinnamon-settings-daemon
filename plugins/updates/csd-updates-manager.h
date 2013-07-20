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
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 */

#ifndef __CSD_UPDATES_MANAGER_H
#define __CSD_UPDATES_MANAGER_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CSD_TYPE_UPDATES_MANAGER         (csd_updates_manager_get_type ())
#define CSD_UPDATES_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_UPDATES_MANAGER, CsdUpdatesManager))
#define CSD_UPDATES_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), CSD_TYPE_UPDATES_MANAGER, CsdUpdatesManagerClass))
#define CSD_IS_UPDATES_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_UPDATES_MANAGER))
#define CSD_IS_UPDATES_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_UPDATES_MANAGER))
#define CSD_UPDATES_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_UPDATES_MANAGER, CsdUpdatesManagerClass))

typedef struct CsdUpdatesManagerPrivate CsdUpdatesManagerPrivate;

typedef struct
{
        GObject parent;
        CsdUpdatesManagerPrivate *priv;
} CsdUpdatesManager;

typedef struct
{
        GObjectClass parent_class;
} CsdUpdatesManagerClass;

GType csd_updates_manager_get_type (void) G_GNUC_CONST;

CsdUpdatesManager *csd_updates_manager_new (void);
gboolean csd_updates_manager_start (CsdUpdatesManager *manager, GError **error);
void csd_updates_manager_stop (CsdUpdatesManager *manager);

G_END_DECLS

#endif /* __CSD_UPDATES_MANAGER_H */

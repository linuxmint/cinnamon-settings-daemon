/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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

#ifndef __GCM_DMI_H
#define __GCM_DMI_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GCM_TYPE_DMI            (gcm_dmi_get_type ())
#define GCM_DMI(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_DMI, GcmDmi))
#define GCM_DMI_CLASS(k)        (G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_DMI, GcmDmiClass))
#define GCM_IS_DMI(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_DMI))
#define GCM_IS_DMI_CLASS(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_DMI))
#define GCM_DMI_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_DMI, GcmDmiClass))

typedef struct _GcmDmiPrivate   GcmDmiPrivate;
typedef struct _GcmDmi          GcmDmi;
typedef struct _GcmDmiClass     GcmDmiClass;

struct _GcmDmi
{
         GObject                 parent;
         GcmDmiPrivate          *priv;
};

struct _GcmDmiClass
{
        GObjectClass    parent_class;
};

GType            gcm_dmi_get_type                       (void);
GcmDmi          *gcm_dmi_new                            (void);
const gchar     *gcm_dmi_get_name                       (GcmDmi         *dmi);
const gchar     *gcm_dmi_get_version                    (GcmDmi         *dmi);
const gchar     *gcm_dmi_get_vendor                     (GcmDmi         *dmi);

G_END_DECLS

#endif /* __GCM_DMI_H */


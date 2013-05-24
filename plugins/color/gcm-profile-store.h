/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2011 Richard Hughes <richard@hughsie.com>
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

#ifndef __GCM_PROFILE_STORE_H
#define __GCM_PROFILE_STORE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GCM_TYPE_PROFILE_STORE          (gcm_profile_store_get_type ())
#define GCM_PROFILE_STORE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_PROFILE_STORE, GcmProfileStore))
#define GCM_PROFILE_STORE_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_PROFILE_STORE, GcmProfileStoreClass))
#define GCM_IS_PROFILE_STORE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_PROFILE_STORE))
#define GCM_IS_PROFILE_STORE_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_PROFILE_STORE))
#define GCM_PROFILE_STORE_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_PROFILE_STORE, GcmProfileStoreClass))

typedef struct _GcmProfileStorePrivate  GcmProfileStorePrivate;
typedef struct _GcmProfileStore         GcmProfileStore;
typedef struct _GcmProfileStoreClass    GcmProfileStoreClass;

struct _GcmProfileStore
{
         GObject                         parent;
         GcmProfileStorePrivate         *priv;
};

struct _GcmProfileStoreClass
{
        GObjectClass    parent_class;
        void            (* added)                       (const gchar            *filename);
        void            (* removed)                     (const gchar            *filename);
};

GType            gcm_profile_store_get_type             (void);
GcmProfileStore *gcm_profile_store_new                  (void);
gboolean         gcm_profile_store_search               (GcmProfileStore        *profile_store);

G_END_DECLS

#endif /* __GCM_PROFILE_STORE_H */

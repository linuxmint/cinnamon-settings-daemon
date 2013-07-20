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

#include "config.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "cinnamon-settings-profile.h"
#include "csd-dummy-manager.h"

#define CSD_DUMMY_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CSD_TYPE_DUMMY_MANAGER, CsdDummyManagerPrivate))

struct CsdDummyManagerPrivate
{
        gboolean padding;
};

enum {
        PROP_0,
};

static void     csd_dummy_manager_class_init  (CsdDummyManagerClass *klass);
static void     csd_dummy_manager_init        (CsdDummyManager      *dummy_manager);
static void     csd_dummy_manager_finalize    (GObject             *object);

G_DEFINE_TYPE (CsdDummyManager, csd_dummy_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

gboolean
csd_dummy_manager_start (CsdDummyManager *manager,
                               GError               **error)
{
        g_debug ("Starting dummy manager");
        cinnamon_settings_profile_start (NULL);
        cinnamon_settings_profile_end (NULL);
        return TRUE;
}

void
csd_dummy_manager_stop (CsdDummyManager *manager)
{
        g_debug ("Stopping dummy manager");
}

static void
csd_dummy_manager_set_property (GObject        *object,
                               guint           prop_id,
                               const GValue   *value,
                               GParamSpec     *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
csd_dummy_manager_get_property (GObject        *object,
                               guint           prop_id,
                               GValue         *value,
                               GParamSpec     *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static GObject *
csd_dummy_manager_constructor (GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_properties)
{
        CsdDummyManager      *dummy_manager;

        dummy_manager = CSD_DUMMY_MANAGER (G_OBJECT_CLASS (csd_dummy_manager_parent_class)->constructor (type,
                                                                                                      n_construct_properties,
                                                                                                      construct_properties));

        return G_OBJECT (dummy_manager);
}

static void
csd_dummy_manager_dispose (GObject *object)
{
        G_OBJECT_CLASS (csd_dummy_manager_parent_class)->dispose (object);
}

static void
csd_dummy_manager_class_init (CsdDummyManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = csd_dummy_manager_get_property;
        object_class->set_property = csd_dummy_manager_set_property;
        object_class->constructor = csd_dummy_manager_constructor;
        object_class->dispose = csd_dummy_manager_dispose;
        object_class->finalize = csd_dummy_manager_finalize;

        g_type_class_add_private (klass, sizeof (CsdDummyManagerPrivate));
}

static void
csd_dummy_manager_init (CsdDummyManager *manager)
{
        manager->priv = CSD_DUMMY_MANAGER_GET_PRIVATE (manager);

}

static void
csd_dummy_manager_finalize (GObject *object)
{
        CsdDummyManager *dummy_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_DUMMY_MANAGER (object));

        dummy_manager = CSD_DUMMY_MANAGER (object);

        g_return_if_fail (dummy_manager->priv != NULL);

        G_OBJECT_CLASS (csd_dummy_manager_parent_class)->finalize (object);
}

CsdDummyManager *
csd_dummy_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CSD_TYPE_DUMMY_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return CSD_DUMMY_MANAGER (manager_object);
}

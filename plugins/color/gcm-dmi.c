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

#include "config.h"

#include <glib-object.h>
#include <math.h>
#include <string.h>
#include <gio/gio.h>
#include <stdlib.h>

#include "gcm-dmi.h"

static void     gcm_dmi_finalize        (GObject     *object);

#define GCM_DMI_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_DMI, GcmDmiPrivate))

struct _GcmDmiPrivate
{
        gchar                           *name;
        gchar                           *version;
        gchar                           *vendor;
};

static gpointer gcm_dmi_object = NULL;

G_DEFINE_TYPE (GcmDmi, gcm_dmi, G_TYPE_OBJECT)

static gchar *
gcm_dmi_get_from_filename (const gchar *filename)
{
        gboolean ret;
        GError *error = NULL;
        gchar *data = NULL;

        /* get the contents */
        ret = g_file_get_contents (filename, &data, NULL, &error);
        if (!ret) {
		if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
			g_warning ("failed to get contents of %s: %s", filename, error->message);
                g_error_free (error);
        }

        /* process the random chars and trailing spaces */
        if (data != NULL) {
                g_strdelimit (data, "\t_", ' ');
                g_strdelimit (data, "\n\r", '\0');
                g_strchomp (data);
        }

        /* don't return an empty string */
        if (data != NULL && data[0] == '\0') {
                g_free (data);
                data = NULL;
        }

        return data;
}

static gchar *
gcm_dmi_get_from_filenames (const gchar * const * filenames)
{
        guint i;
        gchar *tmp = NULL;

        /* try each one in preference order */
        for (i = 0; filenames[i] != NULL; i++) {
                tmp = gcm_dmi_get_from_filename (filenames[i]);
                if (tmp != NULL)
                        break;
        }
        return tmp;
}

const gchar *
gcm_dmi_get_name (GcmDmi *dmi)
{
        g_return_val_if_fail (GCM_IS_DMI (dmi), NULL);
        return dmi->priv->name;
}

const gchar *
gcm_dmi_get_version (GcmDmi *dmi)
{
        g_return_val_if_fail (GCM_IS_DMI (dmi), NULL);
        return dmi->priv->version;
}

const gchar *
gcm_dmi_get_vendor (GcmDmi *dmi)
{
        g_return_val_if_fail (GCM_IS_DMI (dmi), NULL);
        return dmi->priv->vendor;
}

static void
gcm_dmi_class_init (GcmDmiClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gcm_dmi_finalize;
        g_type_class_add_private (klass, sizeof (GcmDmiPrivate));
}

static void
gcm_dmi_init (GcmDmi *dmi)
{
#if defined(__linux__)
        const gchar *sysfs_name[] = {
                "/sys/class/dmi/id/product_name",
                "/sys/class/dmi/id/board_name",
                NULL};
        const gchar *sysfs_version[] = {
                "/sys/class/dmi/id/product_version",
                "/sys/class/dmi/id/chassis_version",
                "/sys/class/dmi/id/board_version",
                NULL};
        const gchar *sysfs_vendor[] = {
                "/sys/class/dmi/id/sys_vendor",
                "/sys/class/dmi/id/chassis_vendor",
                "/sys/class/dmi/id/board_vendor",
                NULL};
#else
#warning Please add dmi support for your OS
        const gchar *sysfs_name[] = { NULL };
        const gchar *sysfs_version[] = { NULL };
        const gchar *sysfs_vendor[] = { NULL };
#endif

        dmi->priv = GCM_DMI_GET_PRIVATE (dmi);

        /* get all the possible data now */
        dmi->priv->name = gcm_dmi_get_from_filenames (sysfs_name);
        dmi->priv->version = gcm_dmi_get_from_filenames (sysfs_version);
        dmi->priv->vendor = gcm_dmi_get_from_filenames (sysfs_vendor);
}

static void
gcm_dmi_finalize (GObject *object)
{
        GcmDmi *dmi = GCM_DMI (object);

        g_free (dmi->priv->name);
        g_free (dmi->priv->version);
        g_free (dmi->priv->vendor);

        G_OBJECT_CLASS (gcm_dmi_parent_class)->finalize (object);
}

GcmDmi *
gcm_dmi_new (void)
{
        if (gcm_dmi_object != NULL) {
                g_object_ref (gcm_dmi_object);
        } else {
                gcm_dmi_object = g_object_new (GCM_TYPE_DMI, NULL);
                g_object_add_weak_pointer (gcm_dmi_object, &gcm_dmi_object);
        }
        return GCM_DMI (gcm_dmi_object);
}

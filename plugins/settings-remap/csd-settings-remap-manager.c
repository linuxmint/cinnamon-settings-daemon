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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "cinnamon-settings-profile.h"
#include "csd-settings-remap-manager.h"

typedef struct
{
    GHashTable            *settings_table;
    GSettingsSchemaSource *source;
} CsdSettingsRemapManagerPrivate;

struct  _CsdSettingsRemapManager
{
    GObject                        parent;
    CsdSettingsRemapManagerPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (CsdSettingsRemapManager, csd_settings_remap_manager, G_TYPE_OBJECT)

static void     csd_settings_remap_manager_class_init  (CsdSettingsRemapManagerClass *klass);
static void     csd_settings_remap_manager_init        (CsdSettingsRemapManager      *settings_remap_manager);
static void     csd_settings_remap_manager_finalize    (GObject                     *object);

static gpointer manager_object = NULL;

typedef struct {
    const gchar *gnome_schema;
    const gchar *cinnamon_schema;
} SchemaMap;

static SchemaMap schemas[] = {
    { "org.gnome.desktop.interface", "org.cinnamon.desktop.interface" },
    { "org.gnome.desktop.peripherals.mouse", "org.cinnamon.desktop.peripherals.mouse" },
    { "org.gnome.desktop.sound", "org.cinnamon.desktop.sound" },
    { "org.gnome.desktop.privacy", "org.cinnamon.desktop.privacy" },
    { "org.gnome.desktop.wm.preferences", "org.cinnamon.desktop.wm.preferences" },
    { "org.gnome.settings-daemon.plugins.xsettings", "org.cinnamon.settings-daemon.plugins.xsettings" },
    { "org.gnome.desktop.a11y", "org.cinnamon.desktop.a11y.keyboard" },
    { "org.gnome.desktop.input-sources", "org.cinnamon.desktop.input-sources" },
};

static gboolean
was_handled_specially (CsdSettingsRemapManager *manager,
                       GSettings               *cinnamon_settings,
                       GSettings               *gnome_settings,
                       const gchar             *key)
{
    // ...
    return FALSE;
}

static void
on_settings_changed (GSettings   *cinnamon_settings,
                     const gchar *key,
                     gpointer     user_data)
{
    CsdSettingsRemapManager *manager = CSD_SETTINGS_REMAP_MANAGER (user_data);
    GSettings *gnome_settings;
    GSettingsSchema *gnome_schema;

    gnome_settings = g_hash_table_lookup (manager->priv->settings_table, cinnamon_settings);

    gnome_schema = g_settings_schema_source_lookup (manager->priv->source,
                                                    (gchar *) g_object_get_data (G_OBJECT (gnome_settings), "schema"),
                                                    FALSE);

    if (gnome_schema == NULL)
    {
        g_warning ("Schema not found during settings change - '%s'",
                   (gchar *) g_object_get_data (G_OBJECT (gnome_settings), "schema"));
        return;
    }

    if (!was_handled_specially (manager, cinnamon_settings, gnome_settings, key))
    {
        if (g_settings_schema_has_key (gnome_schema, key))
        {
            GVariant *value = g_settings_get_value (cinnamon_settings, key);
            g_settings_set_value (gnome_settings, key, value);
            g_variant_unref (value);
        }
    }

    g_settings_schema_unref (gnome_schema);
}

static void
sync_to_gnome (CsdSettingsRemapManager *manager,
               GSettings               *cinnamon_settings)
{
    GSettingsSchema *cinnamon_schema;
    gchar **cinnamon_keys;
    gint i;

    cinnamon_schema = g_settings_schema_source_lookup (manager->priv->source,
                                                       (gchar *) g_object_get_data (G_OBJECT (cinnamon_settings), "schema"),
                                                       FALSE);

    if (cinnamon_schema == NULL)
    {
        return;
    }

    cinnamon_keys = g_settings_schema_list_keys (cinnamon_schema);
    for (i = 0; i < g_strv_length (cinnamon_keys); i++)
    {
        on_settings_changed (cinnamon_settings, cinnamon_keys[i], manager);
    }

    g_strfreev (cinnamon_keys);
    g_settings_schema_unref (cinnamon_schema);
}

static gboolean
schema_exists (CsdSettingsRemapManager  *manager, 
               const gchar              *schema_id)
{
    GSettingsSchema *schema;

    schema = g_settings_schema_source_lookup (manager->priv->source, schema_id, FALSE);

    if (schema == NULL)
    {
        return FALSE;
    }

    g_settings_schema_unref (schema);
    return TRUE;
}

gboolean
csd_settings_remap_manager_start (CsdSettingsRemapManager *manager,
                                 GError                **error)
{
    gint i;

    g_debug ("Starting settings_remap manager");
    cinnamon_settings_profile_start (NULL);

    manager->priv->settings_table = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, g_object_unref);
    manager->priv->source = g_settings_schema_source_get_default ();

    for (i = 0; i < G_N_ELEMENTS (schemas); i++)
    {
        GSettings *gnome_settings;
        GSettings *cinnamon_settings;

        if (!schema_exists (manager, schemas[i].gnome_schema))
        {
            g_warning ("Skipping schema '%s' - doesn't exist", schemas[i].gnome_schema);
            continue;
        }

        gnome_settings = g_settings_new (schemas[i].gnome_schema);
        g_object_set_data_full (G_OBJECT (gnome_settings), "schema", g_strdup (schemas[i].gnome_schema), g_free);

        cinnamon_settings = g_settings_new (schemas[i].cinnamon_schema);
        g_object_set_data_full (G_OBJECT (cinnamon_settings), "schema", g_strdup (schemas[i].cinnamon_schema), g_free);
        g_signal_connect (cinnamon_settings, "changed", G_CALLBACK (on_settings_changed), manager);

        g_hash_table_insert (manager->priv->settings_table, cinnamon_settings, gnome_settings);

        sync_to_gnome (manager, cinnamon_settings);
    }

    cinnamon_settings_profile_end (NULL);
    return TRUE;
}

void
csd_settings_remap_manager_stop (CsdSettingsRemapManager *manager)
{
    g_clear_pointer (&manager->priv->settings_table, g_hash_table_destroy); 

    g_debug ("Stopping settings_remap manager");
}

static void
csd_settings_remap_manager_class_init (CsdSettingsRemapManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = csd_settings_remap_manager_finalize;
}

static void
csd_settings_remap_manager_init (CsdSettingsRemapManager *manager)
{
        manager->priv = csd_settings_remap_manager_get_instance_private (manager);

        manager->priv->settings_table = NULL;
}

static void
csd_settings_remap_manager_finalize (GObject *object)
{
        CsdSettingsRemapManager *settings_remap_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_SETTINGS_REMAP_MANAGER (object));

        settings_remap_manager = CSD_SETTINGS_REMAP_MANAGER (object);

        g_return_if_fail (settings_remap_manager->priv != NULL);

        G_OBJECT_CLASS (csd_settings_remap_manager_parent_class)->finalize (object);
}

CsdSettingsRemapManager *
csd_settings_remap_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CSD_TYPE_SETTINGS_REMAP_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return CSD_SETTINGS_REMAP_MANAGER (manager_object);
}

/*
 * Copyright (C) 2008 Michael J. Chudobiak <mjc@avtechpulse.com>
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

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <string.h>

#include "cinnamon-settings-profile.h"
#include "csd-housekeeping-manager.h"
#include "csd-disk-space.h"


/* General */
#define INTERVAL_ONCE_A_DAY 24*60*60
#define INTERVAL_TWO_MINUTES 2*60

/* Thumbnail cleaner */
#define THUMB_PREFIX "org.gnome.desktop.thumbnail-cache"

#define THUMB_AGE_KEY "maximum-age"
#define THUMB_SIZE_KEY "maximum-size"

struct CsdHousekeepingManagerPrivate {
        GSettings *settings;
        guint long_term_cb;
        guint short_term_cb;
};


#define CSD_HOUSEKEEPING_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CSD_TYPE_HOUSEKEEPING_MANAGER, CsdHousekeepingManagerPrivate))

static void     csd_housekeeping_manager_class_init  (CsdHousekeepingManagerClass *klass);
static void     csd_housekeeping_manager_init        (CsdHousekeepingManager      *housekeeping_manager);

G_DEFINE_TYPE (CsdHousekeepingManager, csd_housekeeping_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;


typedef struct {
        glong now;
        glong max_age;
        goffset total_size;
        goffset max_size;
} PurgeData;


typedef struct {
        time_t  mtime;
        char   *path;
        glong   size;
} ThumbData;


static void
thumb_data_free (gpointer data)
{
        ThumbData *info = data;

        if (info) {
                g_free (info->path);
                g_free (info);
        }
}

static GList *
read_dir_for_purge (const char *path, GList *files)
{
        GFile           *read_path;
        GFileEnumerator *enum_dir;

        read_path = g_file_new_for_path (path);
        enum_dir = g_file_enumerate_children (read_path,
                                              G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                              G_FILE_ATTRIBUTE_TIME_MODIFIED ","
                                              G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                              G_FILE_QUERY_INFO_NONE,
                                              NULL,
                                              NULL);

        if (enum_dir != NULL) {
                GFileInfo *info;
                while ((info = g_file_enumerator_next_file (enum_dir, NULL, NULL)) != NULL) {
                        const char *name;
                        name = g_file_info_get_name (info);

                        if (strlen (name) == 36 && strcmp (name + 32, ".png") == 0) {
                                ThumbData *td;
                                GFile     *entry;
                                char      *entry_path;
                                GTimeVal   mod_time;

                                entry = g_file_get_child (read_path, name);
                                entry_path = g_file_get_path (entry);
                                g_object_unref (entry);

                                g_file_info_get_modification_time (info, &mod_time);

                                td = g_new0 (ThumbData, 1);
                                td->path = entry_path;
                                td->mtime = mod_time.tv_sec;
                                td->size = g_file_info_get_size (info);

                                files = g_list_prepend (files, td);
                        }
                        g_object_unref (info);
                }
                g_object_unref (enum_dir);
        }
        g_object_unref (read_path);

        return files;
}

static void
purge_old_thumbnails (ThumbData *info, PurgeData *purge_data)
{
        if ((purge_data->now - info->mtime) > purge_data->max_age) {
                g_unlink (info->path);
                info->size = 0;
        } else {
                purge_data->total_size += info->size;
        }
}

static int
sort_file_mtime (ThumbData *file1, ThumbData *file2)
{
        return file1->mtime - file2->mtime;
}

static char **
get_thumbnail_dirs (void)
{
        GPtrArray *array;
        char *path;

        array = g_ptr_array_new ();

        /* check new XDG cache */
        path = g_build_filename (g_get_user_cache_dir (),
                                 "thumbnails",
                                 "normal",
                                 NULL);
        g_ptr_array_add (array, path);

        path = g_build_filename (g_get_user_cache_dir (),
                                 "thumbnails",
                                 "large",
                                 NULL);
        g_ptr_array_add (array, path);

        path = g_build_filename (g_get_user_cache_dir (),
                                 "thumbnails",
                                 "fail",
                                 "gnome-thumbnail-factory",
                                 NULL);
        g_ptr_array_add (array, path);

        /* cleanup obsolete locations too */
        path = g_build_filename (g_get_home_dir (),
                                 ".thumbnails",
                                 "normal",
                                 NULL);
        g_ptr_array_add (array, path);

        path = g_build_filename (g_get_home_dir (),
                                 ".thumbnails",
                                 "large",
                                 NULL);
        g_ptr_array_add (array, path);

        path = g_build_filename (g_get_home_dir (),
                                 ".thumbnails",
                                 "fail",
                                 "gnome-thumbnail-factory",
                                 NULL);
        g_ptr_array_add (array, path);

        g_ptr_array_add (array, NULL);

        return (char **) g_ptr_array_free (array, FALSE);
}

static void
purge_thumbnail_cache (CsdHousekeepingManager *manager)
{

        char     **paths;
        GList     *files;
        PurgeData  purge_data;
        GTimeVal   current_time;
        guint      i;

        g_debug ("housekeeping: checking thumbnail cache size and freshness");

        paths = get_thumbnail_dirs ();
        files = NULL;
        for (i = 0; paths[i] != NULL; i++)
                files = read_dir_for_purge (paths[i], files);
        g_strfreev (paths);

        g_get_current_time (&current_time);

        purge_data.now = current_time.tv_sec;
        purge_data.max_age = g_settings_get_int (manager->priv->settings, THUMB_AGE_KEY) * 24 * 60 * 60;
        purge_data.max_size = g_settings_get_int (manager->priv->settings, THUMB_SIZE_KEY) * 1024 * 1024;
        purge_data.total_size = 0;

        if (purge_data.max_age >= 0)
                g_list_foreach (files, (GFunc) purge_old_thumbnails, &purge_data);

        if ((purge_data.total_size > purge_data.max_size) && (purge_data.max_size >= 0)) {
                GList *scan;
                files = g_list_sort (files, (GCompareFunc) sort_file_mtime);
                for (scan = files; scan && (purge_data.total_size > purge_data.max_size); scan = scan->next) {
                        ThumbData *info = scan->data;
                        g_unlink (info->path);
                        purge_data.total_size -= info->size;
                }
        }

        g_list_foreach (files, (GFunc) thumb_data_free, NULL);
        g_list_free (files);
}

static gboolean
do_cleanup (CsdHousekeepingManager *manager)
{
        purge_thumbnail_cache (manager);
        return TRUE;
}

static gboolean
do_cleanup_once (CsdHousekeepingManager *manager)
{
        do_cleanup (manager);
        manager->priv->short_term_cb = 0;
        return FALSE;
}

static void
do_cleanup_soon (CsdHousekeepingManager *manager)
{
        if (manager->priv->short_term_cb == 0) {
                g_debug ("housekeeping: will tidy up in 2 minutes");
                manager->priv->short_term_cb = g_timeout_add_seconds (INTERVAL_TWO_MINUTES,
                                               (GSourceFunc) do_cleanup_once,
                                               manager);
        }
}

static void
settings_changed_callback (GSettings              *settings,
                           const char             *key,
                           CsdHousekeepingManager *manager)
{
        do_cleanup_soon (manager);
}

gboolean
csd_housekeeping_manager_start (CsdHousekeepingManager *manager,
                                GError                **error)
{
        g_debug ("Starting housekeeping manager");
        cinnamon_settings_profile_start (NULL);

        csd_ldsm_setup (FALSE);

        manager->priv->settings = g_settings_new (THUMB_PREFIX);
        g_signal_connect (G_OBJECT (manager->priv->settings), "changed",
                          G_CALLBACK (settings_changed_callback), manager);

        /* Clean once, a few minutes after start-up */
        do_cleanup_soon (manager);

        /* Clean periodically, on a daily basis. */
        manager->priv->long_term_cb = g_timeout_add_seconds (INTERVAL_ONCE_A_DAY,
                                      (GSourceFunc) do_cleanup,
                                      manager);

        cinnamon_settings_profile_end (NULL);

        return TRUE;
}

void
csd_housekeeping_manager_stop (CsdHousekeepingManager *manager)
{
        CsdHousekeepingManagerPrivate *p = manager->priv;

        g_debug ("Stopping housekeeping manager");

        if (p->short_term_cb) {
                g_source_remove (p->short_term_cb);
                p->short_term_cb = 0;
        }

        if (p->long_term_cb) {
                g_source_remove (p->long_term_cb);
                p->long_term_cb = 0;

                /* Do a clean-up on shutdown if and only if the size or age
                   limits have been set to paranoid levels (zero) */
                if ((g_settings_get_int (p->settings, THUMB_AGE_KEY) == 0) ||
                    (g_settings_get_int (p->settings, THUMB_SIZE_KEY) == 0)) {
                        do_cleanup (manager);
                }

                g_object_unref (p->settings);
                p->settings = NULL;
        }

        csd_ldsm_clean ();
}

static void
csd_housekeeping_manager_class_init (CsdHousekeepingManagerClass *klass)
{
        g_type_class_add_private (klass, sizeof (CsdHousekeepingManagerPrivate));
}

static void
csd_housekeeping_manager_init (CsdHousekeepingManager *manager)
{
        manager->priv = CSD_HOUSEKEEPING_MANAGER_GET_PRIVATE (manager);
}

CsdHousekeepingManager *
csd_housekeeping_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CSD_TYPE_HOUSEKEEPING_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return CSD_HOUSEKEEPING_MANAGER (manager_object);
}

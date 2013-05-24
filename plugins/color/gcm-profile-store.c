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
#include <gio/gio.h>

#include "gcm-profile-store.h"

static void     gcm_profile_store_finalize      (GObject     *object);

#define GCM_PROFILE_STORE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_PROFILE_STORE, GcmProfileStorePrivate))

struct _GcmProfileStorePrivate
{
        GPtrArray                       *filename_array;
        GPtrArray                       *directory_array;
        GCancellable                    *cancellable;
};

enum {
        SIGNAL_ADDED,
        SIGNAL_REMOVED,
        SIGNAL_CHANGED,
        SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (GcmProfileStore, gcm_profile_store, G_TYPE_OBJECT)

static void gcm_profile_store_search_path (GcmProfileStore *profile_store, const gchar *path, guint depth);
static void gcm_profile_store_process_child (GcmProfileStore *profile_store, const gchar *path, GFileInfo *info);

#define GCM_PROFILE_STORE_MAX_RECURSION_LEVELS          2

typedef struct {
        gchar           *path;
        GFileMonitor    *monitor;
        guint            depth;
} GcmProfileStoreDirHelper;

static void
gcm_profile_store_helper_free (GcmProfileStoreDirHelper *helper)
{
        g_free (helper->path);
        if (helper->monitor != NULL)
                g_object_unref (helper->monitor);
        g_free (helper);
}

static const gchar *
gcm_profile_store_find_filename (GcmProfileStore *profile_store, const gchar *filename)
{
        const gchar *tmp;
        guint i;
        GPtrArray *array = profile_store->priv->filename_array;

        for (i=0; i<array->len; i++) {
                tmp = g_ptr_array_index (array, i);
                if (g_strcmp0 (filename, tmp) == 0)
                        return tmp;
        }
        return NULL;
}

static GcmProfileStoreDirHelper *
gcm_profile_store_find_directory (GcmProfileStore *profile_store, const gchar *path)
{
        GcmProfileStoreDirHelper *tmp;
        guint i;
        GPtrArray *array = profile_store->priv->directory_array;

        for (i=0; i<array->len; i++) {
                tmp = g_ptr_array_index (array, i);
                if (g_strcmp0 (path, tmp->path) == 0)
                        return tmp;
        }
        return NULL;
}

static gboolean
gcm_profile_store_remove_profile (GcmProfileStore *profile_store,
                                  const gchar *filename)
{
        gboolean ret = FALSE;
        const gchar *tmp;
        gchar *filename_dup = NULL;

        GcmProfileStorePrivate *priv = profile_store->priv;

        /* find exact pointer */
        tmp = gcm_profile_store_find_filename (profile_store, filename);
        if (tmp == NULL)
                goto out;

        /* dup so we can emit the signal */
        filename_dup = g_strdup (tmp);
        ret = g_ptr_array_remove (priv->filename_array, (gpointer)tmp);
        if (!ret) {
                g_warning ("failed to remove %s", filename);
                goto out;
        }

        /* emit a signal */
        g_debug ("emit removed: %s", filename_dup);
        g_signal_emit (profile_store, signals[SIGNAL_REMOVED], 0, filename_dup);
out:
        g_free (filename_dup);
        return ret;
}

static void
gcm_profile_store_add_profile (GcmProfileStore *profile_store, const gchar *filename)
{
        GcmProfileStorePrivate *priv = profile_store->priv;

        /* add to list */
        g_ptr_array_add (priv->filename_array, g_strdup (filename));

        /* emit a signal */
        g_debug ("emit add: %s", filename);
        g_signal_emit (profile_store, signals[SIGNAL_ADDED], 0, filename);
}

static void
gcm_profile_store_created_query_info_cb (GObject *source_object,
                                         GAsyncResult *res,
                                         gpointer user_data)
{
        GFileInfo *info;
        GError *error = NULL;
        gchar *path;
        GFile *file = G_FILE (source_object);
        GFile *parent;
        GcmProfileStore *profile_store = GCM_PROFILE_STORE (user_data);

        info = g_file_query_info_finish (file, res, &error);
        if (info == NULL) {
                g_warning ("failed to get info about deleted file: %s",
                           error->message);
                g_error_free (error);
                return;
        }
        parent = g_file_get_parent (file);
        path = g_file_get_path (parent);
        gcm_profile_store_process_child (profile_store,
                                         path,
                                         info);
        g_free (path);
        g_object_unref (info);
        g_object_unref (parent);
}

static void
gcm_profile_store_remove_from_prefix (GcmProfileStore *profile_store,
                                      const gchar *prefix)
{
        guint i;
        const gchar *path;
        GcmProfileStorePrivate *priv = profile_store->priv;

        for (i = 0; i < priv->filename_array->len; i++) {
                path = g_ptr_array_index (priv->filename_array, i);
                if (g_str_has_prefix (path, prefix)) {
                        g_debug ("auto-removed %s as path removed", path);
                        gcm_profile_store_remove_profile (profile_store, path);
                }
        }
}

static void
gcm_profile_store_file_monitor_changed_cb (GFileMonitor *monitor,
                                           GFile *file,
                                           GFile *other_file,
                                           GFileMonitorEvent event_type,
                                           GcmProfileStore *profile_store)
{
        gchar *path = NULL;
        gchar *parent_path = NULL;
        const gchar *tmp;
        GcmProfileStoreDirHelper *helper;

        /* profile was deleted */
        if (event_type == G_FILE_MONITOR_EVENT_DELETED) {

                /* we can either have two things here, a directory or a
                 * file. We can't call g_file_query_info_async() as the
                 * inode doesn't exist anymore */
                path = g_file_get_path (file);
                tmp = gcm_profile_store_find_filename (profile_store, path);
                if (tmp != NULL) {
                        /* is a file */
                        gcm_profile_store_remove_profile (profile_store, path);
                        goto out;
                }

                /* is a directory, urgh. Remove all profiles there. */
                gcm_profile_store_remove_from_prefix (profile_store, path);
                helper = gcm_profile_store_find_directory (profile_store, path);
                if (helper != NULL) {
                        g_ptr_array_remove (profile_store->priv->directory_array,
                                            helper);
                }
                goto out;
        }

        /* only care about created objects */
        if (event_type == G_FILE_MONITOR_EVENT_CREATED) {
                g_file_query_info_async (file,
                                         G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                         G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                         G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                         G_PRIORITY_LOW,
                                         NULL,
                                         gcm_profile_store_created_query_info_cb,
                                         profile_store);
                goto out;
        }
out:
        g_free (path);
        g_free (parent_path);
}

static void
gcm_profile_store_process_child (GcmProfileStore *profile_store,
                                 const gchar *path,
                                 GFileInfo *info)
{
        gchar *full_path = NULL;
        const gchar *name;
        GcmProfileStoreDirHelper *helper;

        /* check we're not in a loop */
        helper = gcm_profile_store_find_directory (profile_store, path);
        if (helper == NULL)
                goto out;
        if (helper->depth > GCM_PROFILE_STORE_MAX_RECURSION_LEVELS) {
                g_warning ("recursing more than %i levels deep is insane",
                           GCM_PROFILE_STORE_MAX_RECURSION_LEVELS);
                goto out;
        }

        /* make the compete path */
        name = g_file_info_get_name (info);
        full_path = g_build_filename (path, name, NULL);

        /* if a directory */
        if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
                gcm_profile_store_search_path (profile_store,
                                               full_path,
                                               helper->depth + 1);
                goto out;
        }

        /* ignore temp files */
        if (g_strrstr (full_path, ".goutputstream") != NULL) {
                g_debug ("ignoring gvfs temporary file");
                goto out;
        }

        /* is a file */
        gcm_profile_store_add_profile (profile_store, full_path);
out:
        g_free (full_path);
}

static void
gcm_profile_store_next_files_cb (GObject *source_object,
                                 GAsyncResult *res,
                                 gpointer user_data)
{
        GList *files;
        GList *f;
        GError *error = NULL;
        GFileInfo *info;
        GFile *file;
        gchar *path;
        GFileEnumerator *enumerator = G_FILE_ENUMERATOR (source_object);
        GcmProfileStore *profile_store = GCM_PROFILE_STORE (user_data);

        files = g_file_enumerator_next_files_finish (enumerator,
                                                     res,
                                                     &error);
        if (files == NULL) {
                /* special value, meaning "no more files to process" */
                return;
        }
        if (error != NULL) {
                g_warning ("failed to get data about enumerated directory: %s",
                           error->message);
                g_error_free (error);
                return;
        }

        /* get each file */
        file = g_file_enumerator_get_container (enumerator);
        path = g_file_get_path (file);
        for (f = files; f != NULL; f = f->next) {
                info = G_FILE_INFO (f->data);
                gcm_profile_store_process_child (profile_store, path, info);
        }

        /* continue to get the rest of the data in chunks */
        g_file_enumerator_next_files_async  (enumerator,
                                             5,
                                             G_PRIORITY_LOW,
                                             profile_store->priv->cancellable,
                                             gcm_profile_store_next_files_cb,
                                             user_data);

        g_free (path);
        g_list_foreach (files, (GFunc) g_object_unref, NULL);
        g_list_free (files);
}

static void
gcm_profile_store_enumerate_children_cb (GObject *source_object,
                                         GAsyncResult *res,
                                         gpointer user_data)
{
        GError *error = NULL;
        GFileEnumerator *enumerator;
        GcmProfileStore *profile_store = GCM_PROFILE_STORE (user_data);

        enumerator = g_file_enumerate_children_finish (G_FILE (source_object),
                                                       res,
                                                       &error);
        if (enumerator == NULL) {
                GcmProfileStoreDirHelper *helper;
                gchar *path = NULL;

                path = g_file_get_path (G_FILE (source_object));
                if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
                        g_debug ("failed to enumerate directory %s: %s",
                                 path, error->message);
                else
                        g_warning ("failed to enumerate directory %s: %s",
                                   path, error->message);
                helper = gcm_profile_store_find_directory (profile_store, path);
                if (helper)
                        g_ptr_array_remove (profile_store->priv->directory_array, helper);
                g_error_free (error);
                g_free (path);
                return;
        }

        /* get the first chunk of data */
        g_file_enumerator_next_files_async (enumerator,
                                            5,
                                            G_PRIORITY_LOW,
                                            profile_store->priv->cancellable,
                                            gcm_profile_store_next_files_cb,
                                            user_data);
        g_object_unref (enumerator);
}

static void
gcm_profile_store_search_path (GcmProfileStore *profile_store, const gchar *path, guint depth)
{
        GFile *file = NULL;
        GError *error = NULL;
        GcmProfileStoreDirHelper *helper;

        file = g_file_new_for_path (path);

        /* add an inotify watch if not already added */
        helper = gcm_profile_store_find_directory (profile_store, path);
        if (helper == NULL) {
                helper = g_new0 (GcmProfileStoreDirHelper, 1);
                helper->depth = depth;
                helper->path = g_strdup (path);
                helper->monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, &error);
                if (helper->monitor == NULL) {
                        g_debug ("failed to monitor path: %s", error->message);
                        g_error_free (error);
                        gcm_profile_store_helper_free (helper);
                        goto out;
                }
                g_signal_connect (helper->monitor, "changed",
                                  G_CALLBACK(gcm_profile_store_file_monitor_changed_cb),
                                  profile_store);
                g_ptr_array_add (profile_store->priv->directory_array, helper);
        }

        /* get contents of directory */
        g_file_enumerate_children_async (file,
                                         G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                         G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                         G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                         G_PRIORITY_LOW,
                                         profile_store->priv->cancellable,
                                         gcm_profile_store_enumerate_children_cb,
                                         profile_store);
out:
        g_object_unref (file);
}

static gboolean
gcm_profile_store_mkdir_with_parents (const gchar *filename,
                                      GCancellable *cancellable,
                                      GError **error)
{
        gboolean ret;
        GFile *file;

        /* ensure destination exists */
        file = g_file_new_for_path (filename);
        ret = g_file_make_directory_with_parents (file, cancellable, error);
        g_object_unref (file);

        return ret;
}

gboolean
gcm_profile_store_search (GcmProfileStore *profile_store)
{
        gchar *path;
        gboolean ret;
        GError *error = NULL;

        /* get Linux per-user profiles */
        path = g_build_filename (g_get_user_data_dir (), "icc", NULL);
        ret = gcm_profile_store_mkdir_with_parents (path,
                                                    profile_store->priv->cancellable,
                                                    &error);
        if (!ret &&
            !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS)) {
                g_warning ("failed to create directory on startup: %s", error->message);
        } else {
                gcm_profile_store_search_path (profile_store, path, 0);
        }
        g_free (path);
        g_clear_error (&error);

        /* get per-user profiles from obsolete location */
        path = g_build_filename (g_get_home_dir (), ".color", "icc", NULL);
        gcm_profile_store_search_path (profile_store, path, 0);
        g_free (path);
        return TRUE;
}

static void
gcm_profile_store_class_init (GcmProfileStoreClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gcm_profile_store_finalize;

        signals[SIGNAL_ADDED] =
                g_signal_new ("added",
                              G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GcmProfileStoreClass, added),
                              NULL, NULL, g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE, 1, G_TYPE_STRING);
        signals[SIGNAL_REMOVED] =
                g_signal_new ("removed",
                              G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GcmProfileStoreClass, removed),
                              NULL, NULL, g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE, 1, G_TYPE_STRING);

        g_type_class_add_private (klass, sizeof (GcmProfileStorePrivate));
}

static void
gcm_profile_store_init (GcmProfileStore *profile_store)
{
        profile_store->priv = GCM_PROFILE_STORE_GET_PRIVATE (profile_store);
        profile_store->priv->cancellable = g_cancellable_new ();
        profile_store->priv->filename_array = g_ptr_array_new_with_free_func (g_free);
        profile_store->priv->directory_array = g_ptr_array_new_with_free_func ((GDestroyNotify) gcm_profile_store_helper_free);
}

static void
gcm_profile_store_finalize (GObject *object)
{
        GcmProfileStore *profile_store = GCM_PROFILE_STORE (object);
        GcmProfileStorePrivate *priv = profile_store->priv;

        g_cancellable_cancel (profile_store->priv->cancellable);
        g_object_unref (profile_store->priv->cancellable);
        g_ptr_array_unref (priv->filename_array);
        g_ptr_array_unref (priv->directory_array);

        G_OBJECT_CLASS (gcm_profile_store_parent_class)->finalize (object);
}

GcmProfileStore *
gcm_profile_store_new (void)
{
        GcmProfileStore *profile_store;
        profile_store = g_object_new (GCM_TYPE_PROFILE_STORE, NULL);
        return GCM_PROFILE_STORE (profile_store);
}


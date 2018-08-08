/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * vim: set et sw=8 ts=8:
 *
 * Copyright (c) 2008, Novell, Inc.
 *
 * Authors: Vincent Untz <vuntz@gnome.org>
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

#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gio/gunixmounts.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libnotify/notify.h>

#include "csd-disk-space.h"
#include "csd-ldsm-dialog.h"
#include "csd-disk-space-helper.h"

#define GIGABYTE                   1024 * 1024 * 1024

#define CHECK_EVERY_X_SECONDS      60

#define DISK_SPACE_ANALYZER        "baobab"

#define SETTINGS_HOUSEKEEPING_DIR     "org.cinnamon.settings-daemon.plugins.housekeeping"
#define SETTINGS_FREE_PC_NOTIFY_KEY   "free-percent-notify"
#define SETTINGS_FREE_PC_NOTIFY_AGAIN_KEY "free-percent-notify-again"
#define SETTINGS_FREE_SIZE_NO_NOTIFY  "free-size-gb-no-notify"
#define SETTINGS_MIN_NOTIFY_PERIOD    "min-notify-period"
#define SETTINGS_IGNORE_PATHS         "ignore-paths"

typedef struct
{
        GUnixMountEntry *mount;
        struct statvfs buf;
        time_t notify_time;
} LdsmMountInfo;

static GHashTable        *ldsm_notified_hash = NULL;
static unsigned int       ldsm_timeout_id = 0;
static GUnixMountMonitor *ldsm_monitor = NULL;
static double             free_percent_notify = 0.05;
static double             free_percent_notify_again = 0.01;
static unsigned int       free_size_gb_no_notify = 2;
static unsigned int       min_notify_period = 10;
static GSList            *ignore_paths = NULL;
static GSettings         *settings = NULL;
static CsdLdsmDialog     *dialog = NULL;
static NotifyNotification *notification = NULL;

static guint64           *time_read;

static gchar*
ldsm_get_fs_id_for_path (const gchar *path)
{
        GFile *file;
        GFileInfo *fileinfo;
        gchar *attr_id_fs;

        file = g_file_new_for_path (path);
        fileinfo = g_file_query_info (file, G_FILE_ATTRIBUTE_ID_FILESYSTEM, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
        if (fileinfo) {
                attr_id_fs = g_strdup (g_file_info_get_attribute_string (fileinfo, G_FILE_ATTRIBUTE_ID_FILESYSTEM));
                g_object_unref (fileinfo);
        } else {
                attr_id_fs = NULL;
        }

        g_object_unref (file);

        return attr_id_fs;
}

static gboolean
ldsm_mount_has_trash (LdsmMountInfo *mount)
{
        const gchar *user_data_dir;
        gchar *user_data_attr_id_fs;
        gchar *path_attr_id_fs;
        gboolean mount_uses_user_trash = FALSE;
        gchar *trash_files_dir;
        gboolean has_trash = FALSE;
        GDir *dir;
        const gchar *path;

        user_data_dir = g_get_user_data_dir ();
        user_data_attr_id_fs = ldsm_get_fs_id_for_path (user_data_dir);

        path = g_unix_mount_get_mount_path (mount->mount);
        path_attr_id_fs = ldsm_get_fs_id_for_path (path);

        if (g_strcmp0 (user_data_attr_id_fs, path_attr_id_fs) == 0) {
                /* The volume that is low on space is on the same volume as our home
                 * directory. This means the trash is at $XDG_DATA_HOME/Trash,
                 * not at the root of the volume which is full.
                 */
                mount_uses_user_trash = TRUE;
        }

        g_free (user_data_attr_id_fs);
        g_free (path_attr_id_fs);

        /* I can't think of a better way to find out if a volume has any trash. Any suggestions? */
        if (mount_uses_user_trash) {
                trash_files_dir = g_build_filename (g_get_user_data_dir (), "Trash", "files", NULL);
        } else {
                gchar *uid;

                uid = g_strdup_printf ("%d", getuid ());
                trash_files_dir = g_build_filename (path, ".Trash", uid, "files", NULL);
                if (!g_file_test (trash_files_dir, G_FILE_TEST_IS_DIR)) {
                        gchar *trash_dir;

                        g_free (trash_files_dir);
                        trash_dir = g_strdup_printf (".Trash-%s", uid);
                        trash_files_dir = g_build_filename (path, trash_dir, "files", NULL);
                        g_free (trash_dir);
                        if (!g_file_test (trash_files_dir, G_FILE_TEST_IS_DIR)) {
                                g_free (trash_files_dir);
                                g_free (uid);
                                return has_trash;
                        }
                }
                g_free (uid);
        }

        dir = g_dir_open (trash_files_dir, 0, NULL);
        if (dir) {
                if (g_dir_read_name (dir))
                        has_trash = TRUE;
                g_dir_close (dir);
        }

        g_free (trash_files_dir);

        return has_trash;
}

static void
ldsm_analyze_path (const gchar *path)
{
        const gchar *argv[] = { DISK_SPACE_ANALYZER, path, NULL };

        g_spawn_async (NULL, (gchar **) argv, NULL, G_SPAWN_SEARCH_PATH,
                        NULL, NULL, NULL, NULL);
}

static gboolean
server_has_actions (void)
{
        gboolean has;
        GList   *caps;
        GList   *l;

        caps = notify_get_server_caps ();
        if (caps == NULL) {
                fprintf (stderr, "Failed to receive server caps.\n");
                return FALSE;
        }

        l = g_list_find_custom (caps, "actions", (GCompareFunc)strcmp);
        has = l != NULL;

        g_list_foreach (caps, (GFunc) g_free, NULL);
        g_list_free (caps);

        return has;
}

static void
ignore_callback (NotifyNotification *n,
                 const char         *action)
{
        g_assert (action != NULL);
        g_assert (strcmp (action, "ignore") == 0);

        /* Do nothing */

        notify_notification_close (n, NULL);
}

static void
examine_callback (NotifyNotification *n,
                  const char         *action,
                  const char         *path)
{
        g_assert (action != NULL);
        g_assert (strcmp (action, "examine") == 0);

        ldsm_analyze_path (path);

        notify_notification_close (n, NULL);
}

static void
nemo_empty_trash_cb (GObject *object,
                         GAsyncResult *res,
                         gpointer _unused)
{
        GDBusProxy *proxy = G_DBUS_PROXY (object);
        GError *error = NULL;

        g_dbus_proxy_call_finish (proxy, res, &error);

        if (error != NULL) {
                g_warning ("Unable to call EmptyTrash() on the Nemo DBus interface: %s",
                           error->message);
                g_error_free (error);
        }

        /* clean up the proxy object */
        g_object_unref (proxy);
}

static void
nemo_proxy_ready_cb (GObject *object,
                         GAsyncResult *res,
                         gpointer _unused)
{
        GDBusProxy *proxy = NULL;
        GError *error = NULL;

        proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

        if (proxy == NULL) {
                g_warning ("Unable to create a proxy object for the Nemo DBus interface: %s",
                           error->message);
                g_error_free (error);

                return;
        }

        g_dbus_proxy_call (proxy,
                           "EmptyTrash",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           nemo_empty_trash_cb,
                           NULL);
}

void
csd_ldsm_show_empty_trash (void)
{
        /* prepare the Nemo proxy object */
        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.Nemo",
                                  "/org/Nemo",
                                  "org.Nemo.FileOperations",
                                  NULL,
                                  nemo_proxy_ready_cb,
                                  NULL);
}

static void
empty_trash_callback (NotifyNotification *n,
                      const char         *action)
{
        g_assert (action != NULL);
        g_assert (strcmp (action, "empty-trash") == 0);

        csd_ldsm_show_empty_trash ();

        notify_notification_close (n, NULL);
}

static void
on_notification_closed (NotifyNotification *n)
{
        g_object_unref (notification);
        notification = NULL;
}

static gboolean
ldsm_notify_for_mount (LdsmMountInfo *mount,
                       gboolean       multiple_volumes,
                       gboolean       other_usable_volumes)
{
        gchar  *name, *program;
        gint64 free_space;
        gint response;
        gboolean has_trash;
        gboolean has_disk_analyzer;
        gboolean retval = TRUE;
        gchar *path;

        /* Don't show a notice if one is already displayed */
        if (dialog != NULL || notification != NULL)
                return retval;

        name = g_unix_mount_guess_name (mount->mount);
        free_space = (gint64) mount->buf.f_frsize * (gint64) mount->buf.f_bavail;
        has_trash = ldsm_mount_has_trash (mount);
        path = g_strdup (g_unix_mount_get_mount_path (mount->mount));

        program = g_find_program_in_path (DISK_SPACE_ANALYZER);
        has_disk_analyzer = (program != NULL);
        g_free (program);

        if (server_has_actions ()) {
                char *free_space_str;
                char *summary;
                char *body;

                free_space_str = g_format_size (free_space);

                if (multiple_volumes) {
                        summary = g_strdup_printf (_("Low Disk Space on \"%s\""), name);
                        if (has_trash) {
                                body = g_strdup_printf (_("The volume \"%s\" has only %s disk space remaining.  You may free up some space by emptying the trash."),
                                                        name,
                                                        free_space_str);
                        } else {
                                body = g_strdup_printf (_("The volume \"%s\" has only %s disk space remaining."),
                                                        name,
                                                        free_space_str);
                        }
                } else {
                        summary = g_strdup (_("Low Disk Space"));
                        if (has_trash) {
                                body = g_strdup_printf (_("This computer has only %s disk space remaining.  You may free up some space by emptying the trash."),
                                                        free_space_str);
                        } else {
                                body = g_strdup_printf (_("This computer has only %s disk space remaining."),
                                                        free_space_str);
                        }
                }
                g_free (free_space_str);

                notification = notify_notification_new (summary, body, "drive-harddisk-symbolic");
                g_free (summary);
                g_free (body);

                g_signal_connect (notification,
                                  "closed",
                                  G_CALLBACK (on_notification_closed),
                                  NULL);

                notify_notification_set_app_name (notification, _("Disk space"));
                notify_notification_set_hint (notification, "transient", g_variant_new_boolean (TRUE));
                notify_notification_set_urgency (notification, NOTIFY_URGENCY_CRITICAL);
                notify_notification_set_timeout (notification, NOTIFY_EXPIRES_DEFAULT);
                if (has_disk_analyzer) {
                        notify_notification_add_action (notification,
                                                        "examine",
                                                        _("Examine"),
                                                        (NotifyActionCallback) examine_callback,
                                                        g_strdup (path),
                                                        g_free);
                }
                if (has_trash) {
                        notify_notification_add_action (notification,
                                                        "empty-trash",
                                                        _("Empty Trash"),
                                                        (NotifyActionCallback) empty_trash_callback,
                                                        NULL,
                                                        NULL);
                }
                notify_notification_add_action (notification,
                                                "ignore",
                                                _("Ignore"),
                                                (NotifyActionCallback) ignore_callback,
                                                NULL,
                                                NULL);
                notify_notification_set_category (notification, "device");

                if (!notify_notification_show (notification, NULL)) {
                        g_warning ("failed to send disk space notification\n");
                }

        } else {
                dialog = csd_ldsm_dialog_new (other_usable_volumes,
                                              multiple_volumes,
                                              has_disk_analyzer,
                                              has_trash,
                                              free_space,
                                              name,
                                              path);

                g_object_ref (G_OBJECT (dialog));
                response = gtk_dialog_run (GTK_DIALOG (dialog));

                gtk_widget_destroy (GTK_WIDGET (dialog));
                dialog = NULL;

                switch (response) {
                case GTK_RESPONSE_CANCEL:
                        retval = FALSE;
                        break;
                case CSD_LDSM_DIALOG_RESPONSE_ANALYZE:
                        retval = FALSE;
                        ldsm_analyze_path (path);
                        break;
                case CSD_LDSM_DIALOG_RESPONSE_EMPTY_TRASH:
                        retval = TRUE;
                        csd_ldsm_show_empty_trash ();
                        break;
                case GTK_RESPONSE_NONE:
                case GTK_RESPONSE_DELETE_EVENT:
                        retval = TRUE;
                        break;
                default:
                        g_assert_not_reached ();
                }
        }

        g_free (name);
        g_free (path);

        return retval;
}

static gboolean
ldsm_mount_has_space (LdsmMountInfo *mount)
{
        gdouble free_space;

        free_space = (double) mount->buf.f_bavail / (double) mount->buf.f_blocks;
        /* enough free space, nothing to do */
        if (free_space > free_percent_notify)
                return TRUE;

        if (((gint64) mount->buf.f_frsize * (gint64) mount->buf.f_bavail) > ((gint64) free_size_gb_no_notify * GIGABYTE))
                return TRUE;

        /* If we got here, then this volume is low on space */
        return FALSE;
}

static gboolean
ldsm_mount_is_virtual (LdsmMountInfo *mount)
{
        if (mount->buf.f_blocks == 0) {
                /* Filesystems with zero blocks are virtual */
                return TRUE;
        }

        return FALSE;
}

static gint
ldsm_ignore_path_compare (gconstpointer a,
                          gconstpointer b)
{
        return g_strcmp0 ((const gchar *)a, (const gchar *)b);
}

static gboolean
ldsm_mount_is_user_ignore (const gchar *path)
{
        if (g_slist_find_custom (ignore_paths, path, (GCompareFunc) ldsm_ignore_path_compare) != NULL)
                return TRUE;
        else
                return FALSE;
}


static void
ldsm_free_mount_info (gpointer data)
{
        LdsmMountInfo *mount = data;

        g_return_if_fail (mount != NULL);

        g_unix_mount_free (mount->mount);
        g_free (mount);
}

static void
ldsm_maybe_warn_mounts (GList *mounts,
                        gboolean multiple_volumes,
                        gboolean other_usable_volumes)
{
        GList *l;
        gboolean done = FALSE;

        for (l = mounts; l != NULL; l = l->next) {
                LdsmMountInfo *mount_info = l->data;
                LdsmMountInfo *previous_mount_info;
                gdouble free_space;
                gdouble previous_free_space;
                time_t curr_time;
                const gchar *path;
                gboolean show_notify;

                if (done) {
                        /* Don't show any more dialogs if the user took action with the last one. The user action
                         * might free up space on multiple volumes, making the next dialog redundant.
                         */
                        ldsm_free_mount_info (mount_info);
                        continue;
                }

                path = g_unix_mount_get_mount_path (mount_info->mount);

                previous_mount_info = g_hash_table_lookup (ldsm_notified_hash, path);
                if (previous_mount_info != NULL)
                        previous_free_space = (gdouble) previous_mount_info->buf.f_bavail / (gdouble) previous_mount_info->buf.f_blocks;

                free_space = (gdouble) mount_info->buf.f_bavail / (gdouble) mount_info->buf.f_blocks;

                if (previous_mount_info == NULL) {
                        /* We haven't notified for this mount yet */
                        show_notify = TRUE;
                        mount_info->notify_time = time (NULL);
                        g_hash_table_replace (ldsm_notified_hash, g_strdup (path), mount_info);
                } else if ((previous_free_space - free_space) > free_percent_notify_again) {
                        /* We've notified for this mount before and free space has decreased sufficiently since last time to notify again */
                        curr_time = time (NULL);
                        if (difftime (curr_time, previous_mount_info->notify_time) > (gdouble)(min_notify_period * 60)) {
                                show_notify = TRUE;
                                mount_info->notify_time = curr_time;
                        } else {
                                /* It's too soon to show the dialog again. However, we still replace the LdsmMountInfo
                                 * struct in the hash table, but give it the notfiy time from the previous dialog.
                                 * This will stop the notification from reappearing unnecessarily as soon as the timeout expires.
                                 */
                                show_notify = FALSE;
                                mount_info->notify_time = previous_mount_info->notify_time;
                        }
                        g_hash_table_replace (ldsm_notified_hash, g_strdup (path), mount_info);
                } else {
                        /* We've notified for this mount before, but the free space hasn't decreased sufficiently to notify again */
                        ldsm_free_mount_info (mount_info);
                        show_notify = FALSE;
                }

                if (show_notify) {
                        if (ldsm_notify_for_mount (mount_info, multiple_volumes, other_usable_volumes))
                                done = TRUE;
                }
        }
}

static gboolean
ldsm_check_all_mounts (gpointer data)
{
        GList *mounts;
        GList *l;
        GList *check_mounts = NULL;
        GList *full_mounts = NULL;
        guint number_of_mounts;
        guint number_of_full_mounts;
        gboolean multiple_volumes = FALSE;
        gboolean other_usable_volumes = FALSE;

        /* We iterate through the static mounts in /etc/fstab first, seeing if
         * they're mounted by checking if the GUnixMountPoint has a corresponding GUnixMountEntry.
         * Iterating through the static mounts means we automatically ignore dynamically mounted media.
         */
        mounts = g_unix_mount_points_get (time_read);

        for (l = mounts; l != NULL; l = l->next) {
                GUnixMountPoint *mount_point = l->data;
                GUnixMountEntry *mount;
                LdsmMountInfo *mount_info;
                const gchar *path;

                path = g_unix_mount_point_get_mount_path (mount_point);
                mount = g_unix_mount_at (path, time_read);
                g_unix_mount_point_free (mount_point);
                if (mount == NULL) {
                        /* The GUnixMountPoint is not mounted */
                        continue;
                }

                mount_info = g_new0 (LdsmMountInfo, 1);
                mount_info->mount = mount;

                path = g_unix_mount_get_mount_path (mount);

                if (g_unix_mount_is_readonly (mount)) {
                        ldsm_free_mount_info (mount_info);
                        continue;
                }

                if (ldsm_mount_is_user_ignore (path)) {
                        ldsm_free_mount_info (mount_info);
                        continue;
                }

                if (csd_should_ignore_unix_mount (mount)) {
                        ldsm_free_mount_info (mount_info);
                        continue;
                }

                if (statvfs (path, &mount_info->buf) != 0) {
                        ldsm_free_mount_info (mount_info);
                        continue;
                }

                if (ldsm_mount_is_virtual (mount_info)) {
                        ldsm_free_mount_info (mount_info);
                        continue;
                }

                check_mounts = g_list_prepend (check_mounts, mount_info);
        }

        g_list_free (mounts);

        number_of_mounts = g_list_length (check_mounts);
        if (number_of_mounts > 1)
                multiple_volumes = TRUE;

        for (l = check_mounts; l != NULL; l = l->next) {
                LdsmMountInfo *mount_info = l->data;

                if (!ldsm_mount_has_space (mount_info)) {
                        full_mounts = g_list_prepend (full_mounts, mount_info);
                } else {
                        g_hash_table_remove (ldsm_notified_hash, g_unix_mount_get_mount_path (mount_info->mount));
                        ldsm_free_mount_info (mount_info);
                }
        }

        number_of_full_mounts = g_list_length (full_mounts);
        if (number_of_mounts > number_of_full_mounts)
                other_usable_volumes = TRUE;

        ldsm_maybe_warn_mounts (full_mounts, multiple_volumes,
                                other_usable_volumes);

        g_list_free (check_mounts);
        g_list_free (full_mounts);

        return TRUE;
}

static gboolean
ldsm_is_hash_item_not_in_mounts (gpointer key,
                                 gpointer value,
                                 gpointer user_data)
{
        GList *l;

        for (l = (GList *) user_data; l != NULL; l = l->next) {
                GUnixMountEntry *mount = l->data;
                const char *path;

                path = g_unix_mount_get_mount_path (mount);

                if (strcmp (path, key) == 0)
                        return FALSE;
        }

        return TRUE;
}

static void
ldsm_mounts_changed (GObject  *monitor,
                     gpointer  data)
{
        GList *mounts;

        /* remove the saved data for mounts that got removed */
        mounts = g_unix_mounts_get (time_read);
        g_hash_table_foreach_remove (ldsm_notified_hash,
                                     ldsm_is_hash_item_not_in_mounts, mounts);
        g_list_free_full (mounts, (GDestroyNotify) g_unix_mount_free);

        /* check the status now, for the new mounts */
        ldsm_check_all_mounts (NULL);

        /* and reset the timeout */
        if (ldsm_timeout_id) {
            g_source_remove (ldsm_timeout_id);
            ldsm_timeout_id = 0;
        }
        ldsm_timeout_id = g_timeout_add_seconds (CHECK_EVERY_X_SECONDS,
                                                 ldsm_check_all_mounts, NULL);
}

static gboolean
ldsm_is_hash_item_in_ignore_paths (gpointer key,
                                   gpointer value,
                                   gpointer user_data)
{
        return ldsm_mount_is_user_ignore (key);
}

static void
csd_ldsm_get_config (void)
{
        gchar **settings_list;

        free_percent_notify = g_settings_get_double (settings, SETTINGS_FREE_PC_NOTIFY_KEY);
        if (free_percent_notify >= 1 || free_percent_notify < 0) {
                g_warning ("Invalid configuration of free_percent_notify: %f\n" \
                           "Using sensible default", free_percent_notify);
                free_percent_notify = 0.05;
        }

        free_percent_notify_again = g_settings_get_double (settings, SETTINGS_FREE_PC_NOTIFY_AGAIN_KEY);
        if (free_percent_notify_again >= 1 || free_percent_notify_again < 0) {
                g_warning ("Invalid configuration of free_percent_notify_again: %f\n" \
                           "Using sensible default\n", free_percent_notify_again);
                free_percent_notify_again = 0.01;
        }

        free_size_gb_no_notify = g_settings_get_int (settings, SETTINGS_FREE_SIZE_NO_NOTIFY);
        min_notify_period = g_settings_get_int (settings, SETTINGS_MIN_NOTIFY_PERIOD);

        if (ignore_paths != NULL) {
                g_slist_foreach (ignore_paths, (GFunc) g_free, NULL);
                g_clear_pointer (&ignore_paths, g_slist_free);
        }

        settings_list = g_settings_get_strv (settings, SETTINGS_IGNORE_PATHS);
        if (settings_list != NULL) {
                guint i;

                for (i = 0; settings_list[i] != NULL; i++)
                        ignore_paths = g_slist_prepend (ignore_paths,
                                                        g_strdup (settings_list[i]));

                /* Make sure we don't leave stale entries in ldsm_notified_hash */
                g_hash_table_foreach_remove (ldsm_notified_hash,
                                             ldsm_is_hash_item_in_ignore_paths, NULL);

                g_strfreev (settings_list);
        }
}

static void
csd_ldsm_update_config (GSettings *settings,
                        const gchar *key,
                        gpointer user_data)
{
        csd_ldsm_get_config ();
}

void
csd_ldsm_setup (gboolean check_now)
{
        if (ldsm_notified_hash || ldsm_timeout_id || ldsm_monitor) {
                g_warning ("Low disk space monitor already initialized.");
                return;
        }

        ldsm_notified_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                    g_free,
                                                    ldsm_free_mount_info);

        settings = g_settings_new (SETTINGS_HOUSEKEEPING_DIR);
        csd_ldsm_get_config ();
        g_signal_connect (G_OBJECT (settings), "changed",
                          G_CALLBACK (csd_ldsm_update_config), NULL);

        ldsm_monitor = g_unix_mount_monitor_get ();
        g_signal_connect (ldsm_monitor, "mounts-changed",
                          G_CALLBACK (ldsm_mounts_changed), NULL);

        if (check_now)
                ldsm_check_all_mounts (NULL);

        ldsm_timeout_id = g_timeout_add_seconds (CHECK_EVERY_X_SECONDS,
                                                 ldsm_check_all_mounts, NULL);
}

void
csd_ldsm_clean (void)
{
        if (ldsm_timeout_id) {
            g_source_remove (ldsm_timeout_id);
            ldsm_timeout_id = 0;
        }

        g_clear_pointer (&ldsm_notified_hash, g_hash_table_destroy);
        g_clear_object (&ldsm_monitor);
        g_clear_object (&settings);
        g_clear_object (&dialog);
        if (notification != NULL)
                notify_notification_close (notification, NULL);
        g_slist_free_full (ignore_paths, g_free);
        ignore_paths = NULL;
}


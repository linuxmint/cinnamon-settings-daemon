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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <packagekit-glib2/packagekit.h>
#include <libnotify/notify.h>
#include <gdesktop-enums.h>

#include "gsd-enums.h"
#include "gsd-updates-manager.h"
#include "gsd-updates-firmware.h"
#include "gsd-updates-refresh.h"
#include "gsd-updates-common.h"
#include "gnome-settings-profile.h"

#define GSD_UPDATES_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_UPDATES_MANAGER, GsdUpdatesManagerPrivate))

#define MAX_FAILED_GET_UPDATES              10 /* the maximum number of tries */
#define GSD_UPDATES_ICON_NORMAL             "software-update-available-symbolic"
#define GSD_UPDATES_ICON_URGENT             "software-update-urgent-symbolic"

struct GsdUpdatesManagerPrivate
{
        GCancellable            *cancellable;
        GsdUpdatesRefresh       *refresh;
        GsdUpdatesFirmware      *firmware;
        GSettings               *settings_proxy;
        GSettings               *settings_ftp;
        GSettings               *settings_gsd;
        GSettings               *settings_http;
        guint                    number_updates_critical_last_shown;
        guint                    offline_update_id;
        PkError                 *offline_update_error;
        NotifyNotification      *notification_updates;
        PkControl               *control;
        PkTask                  *task;
        guint                    inhibit_cookie;
        GDBusProxy              *proxy_session;
        guint                    update_viewer_watcher_id;
        GVolumeMonitor          *volume_monitor;
        guint                    failed_get_updates_count;
        gboolean                 pending_updates;
        GDBusConnection         *connection;
        guint                    owner_id;
        GDBusNodeInfo           *introspection;
        GPtrArray               *update_packages;
};

static void gsd_updates_manager_class_init (GsdUpdatesManagerClass *klass);
static void gsd_updates_manager_init (GsdUpdatesManager *updates_manager);
static void gsd_updates_manager_finalize (GObject *object);
static void emit_changed (GsdUpdatesManager *manager);

G_DEFINE_TYPE (GsdUpdatesManager, gsd_updates_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static void
child_exit_cb (GPid pid, gint status, gpointer user_data)
{
        g_spawn_close_pid (pid);
}

static void
clear_offline_updates_message (void)
{
        gboolean ret;
        GError *error = NULL;
        gchar *argv[3];
        GPid pid;

        argv[0] = BINDIR "/pkexec";
        argv[1] = LIBEXECDIR "/pk-clear-offline-update";
        argv[2] = NULL;
        ret = g_spawn_async (NULL,
                             argv,
                             NULL,
                             G_SPAWN_DO_NOT_REAP_CHILD,
                             NULL,
                             NULL,
                             &pid,
                             &error);
        if (!ret) {
                g_warning ("Failure clearing offline update message: %s",
                           error->message);
                g_error_free (error);
                return;
        }
        g_child_watch_add (pid, child_exit_cb, NULL);
}

static void
show_offline_updates_error (GsdUpdatesManager *manager)
{
        const gchar *title;
        gboolean show_geeky = FALSE;
        GString *msg;
        GtkWidget *dialog;

        /* TRANSLATORS: this is when the offline update failed */
        title = _("Failed To Update");
        msg = g_string_new ("");
        switch (pk_error_get_code (manager->priv->offline_update_error)) {
        case PK_ERROR_ENUM_UNFINISHED_TRANSACTION:
                /* TRANSLATORS: the transaction could not be completed
                 * as a previous transaction was unfinished */
                g_string_append (msg, _("A previous update was unfinished."));
                show_geeky = TRUE;
                break;
        case PK_ERROR_ENUM_PACKAGE_DOWNLOAD_FAILED:
        case PK_ERROR_ENUM_NO_CACHE:
        case PK_ERROR_ENUM_NO_NETWORK:
        case PK_ERROR_ENUM_NO_MORE_MIRRORS_TO_TRY:
        case PK_ERROR_ENUM_CANNOT_FETCH_SOURCES:
                /* TRANSLATORS: the package manager needed to download
                 * something with no network available */
                g_string_append (msg, _("Network access was required but not available."));
                break;
        case PK_ERROR_ENUM_BAD_GPG_SIGNATURE:
        case PK_ERROR_ENUM_CANNOT_UPDATE_REPO_UNSIGNED:
        case PK_ERROR_ENUM_GPG_FAILURE:
        case PK_ERROR_ENUM_MISSING_GPG_SIGNATURE:
        case PK_ERROR_ENUM_PACKAGE_CORRUPT:
                /* TRANSLATORS: if the package is not signed correctly
                 *  */
                g_string_append (msg, _("An update was not signed in the correct way."));
                show_geeky = TRUE;
                break;
        case PK_ERROR_ENUM_DEP_RESOLUTION_FAILED:
        case PK_ERROR_ENUM_FILE_CONFLICTS:
        case PK_ERROR_ENUM_INCOMPATIBLE_ARCHITECTURE:
        case PK_ERROR_ENUM_PACKAGE_CONFLICTS:
                /* TRANSLATORS: the transaction failed in a way the user
                 * probably cannot comprehend. Package management systems
                 * really are teh suck.*/
                g_string_append (msg, _("The update could not be completed."));
                show_geeky = TRUE;
                break;
        case PK_ERROR_ENUM_TRANSACTION_CANCELLED:
                /* TRANSLATORS: the user aborted the update manually */
                g_string_append (msg, _("The update was cancelled."));
                break;
        case PK_ERROR_ENUM_NO_PACKAGES_TO_UPDATE:
        case PK_ERROR_ENUM_UPDATE_NOT_FOUND:
                /* TRANSLATORS: the user must have updated manually after
                 * the updates were prepared */
                g_string_append (msg, _("An offline update was requested but no packages required updating."));
                break;
        case PK_ERROR_ENUM_NO_SPACE_ON_DEVICE:
                /* TRANSLATORS: we ran out of disk space */
                g_string_append (msg, _("No space was left on the drive."));
                break;
        case PK_ERROR_ENUM_PACKAGE_FAILED_TO_BUILD:
        case PK_ERROR_ENUM_PACKAGE_FAILED_TO_INSTALL:
        case PK_ERROR_ENUM_PACKAGE_FAILED_TO_REMOVE:
                /* TRANSLATORS: the update process failed in a general
                 * way, usually this message will come from source distros
                 * like gentoo */
                g_string_append (msg, _("An update failed to install correctly."));
                show_geeky = TRUE;
                break;
        default:
                /* TRANSLATORS: We didn't handle the error type */
                g_string_append (msg, _("The offline update failed in an unexpected way."));
                show_geeky = TRUE;
                break;
        }
        if (show_geeky) {
                g_string_append_printf (msg, "\n%s\n\n%s",
                                        /* TRANSLATORS: these are geeky messages from the
                                         * package manager no mortal is supposed to understand,
                                         * but google might know what they mean */
                                        _("Detailed errors from the package manager follow:"),
                                        pk_error_get_details (manager->priv->offline_update_error));
        }
        dialog = gtk_message_dialog_new (NULL,
                                         0,
                                         GTK_MESSAGE_INFO,
                                         GTK_BUTTONS_CLOSE,
                                         "%s", title);
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  "%s", msg->str);
        g_signal_connect_swapped (dialog, "response",
                                  G_CALLBACK (gtk_widget_destroy),
                                  dialog);
        gtk_widget_show (dialog);
        clear_offline_updates_message ();
        g_string_free (msg, TRUE);
}

static void
libnotify_action_cb (NotifyNotification *notification,
                     gchar *action,
                     gpointer user_data)
{
        gboolean ret;
        GError *error = NULL;
        GsdUpdatesManager *manager = GSD_UPDATES_MANAGER (user_data);

        notify_notification_close (notification, NULL);
        if (g_strcmp0 (action, "distro-upgrade-info") == 0) {
                ret = g_spawn_command_line_async (DATADIR "/PackageKit/pk-upgrade-distro.sh",
                                                  &error);
                if (!ret) {
                        g_warning ("Failure launching pk-upgrade-distro.sh: %s",
                                   error->message);
                        g_error_free (error);
                }
                goto out;
        }
        if (g_strcmp0 (action, "show-update-viewer") == 0) {
                ret = g_spawn_command_line_async (BINDIR "/gpk-update-viewer",
                                                  &error);
                if (!ret) {
                        g_warning ("Failure launching update viewer: %s",
                                   error->message);
                        g_error_free (error);
                }
                goto out;
        }
        if (g_strcmp0 (action, "clear-offline-updates") == 0) {
                clear_offline_updates_message ();
                goto out;
        }
        if (g_strcmp0 (action, "error-offline-updates") == 0) {
                show_offline_updates_error (manager);
                goto out;
        }
        if (g_strcmp0 (action, "cancel") == 0) {
                /* try to cancel */
                g_cancellable_cancel (manager->priv->cancellable);
                goto out;
        }
        g_warning ("unknown action id: %s", action);
out:
        return;
}

static void
on_notification_closed (NotifyNotification *notification, gpointer data)
{
        g_object_unref (notification);
}

static void
get_distro_upgrades_finished_cb (GObject *object,
                                 GAsyncResult *res,
                                 GsdUpdatesManager *manager)
{
        const gchar *title;
        gboolean ret;
        gchar *name = NULL;
        GError *error = NULL;
        GPtrArray *array = NULL;
        GString *string = NULL;
        guint i;
        NotifyNotification *notification;
        PkClient *client = PK_CLIENT(object);
        PkDistroUpgrade *item;
        PkError *error_code = NULL;
        PkResults *results;
        PkUpdateStateEnum state;

        /* get the results */
        results = pk_client_generic_finish (PK_CLIENT(client), res, &error);
        if (results == NULL) {
                if (error->domain != PK_CLIENT_ERROR ||
                    error->code != PK_CLIENT_ERROR_NOT_SUPPORTED) {
                        g_warning ("failed to get upgrades: %s",
                                   error->message);
                }
                g_error_free (error);
                goto out;
        }

        /* check error code */
        error_code = pk_results_get_error_code (results);
        if (error_code != NULL) {
                g_warning ("failed to get upgrades: %s, %s",
                           pk_error_enum_to_string (pk_error_get_code (error_code)),
                           pk_error_get_details (error_code));
                goto out;
        }

        /* process results */
        array = pk_results_get_distro_upgrade_array (results);

        /* any updates? */
        if (array->len == 0) {
                g_debug ("no upgrades");
                goto out;
        }

        /* do we do the notification? */
        ret = g_settings_get_boolean (manager->priv->settings_gsd,
                                      GSD_SETTINGS_NOTIFY_DISTRO_UPGRADES);
        if (!ret) {
                g_debug ("ignoring due to GSettings");
                goto out;
        }

        /* find the upgrade string */
        string = g_string_new ("");
        for (i=0; i < array->len; i++) {
                item = (PkDistroUpgrade *) g_ptr_array_index (array, i);
                g_object_get (item,
                              "name", &name,
                              "state", &state,
                              NULL);
                g_string_append_printf (string, "%s (%s)\n",
                                        name,
                                        pk_distro_upgrade_enum_to_string (state));
                g_free (name);
        }
        if (string->len != 0)
                g_string_set_size (string, string->len-1);

        /* TRANSLATORS: a distro update is available, e.g. Fedora 8 to Fedora 9 */
        title = _("Distribution upgrades available");
        notification = notify_notification_new (title,
                                                string->str,
                                                GSD_UPDATES_ICON_NORMAL);
        notify_notification_set_app_name (notification, _("Software Updates"));
        notify_notification_set_timeout (notification, NOTIFY_EXPIRES_NEVER);
        notify_notification_set_urgency (notification, NOTIFY_URGENCY_NORMAL);
        notify_notification_add_action (notification, "distro-upgrade-info",
                                        /* TRANSLATORS: provides more information about the upgrade */
                                        _("More information"),
                                        libnotify_action_cb,
                                        manager, NULL);
        g_signal_connect (notification, "closed",
                          G_CALLBACK (on_notification_closed), NULL);
        ret = notify_notification_show (notification, &error);
        if (!ret) {
                g_warning ("error: %s", error->message);
                g_error_free (error);
        }
out:
        if (error_code != NULL)
                g_object_unref (error_code);
        if (array != NULL)
                g_ptr_array_unref (array);
        if (string != NULL)
                g_string_free (string, TRUE);
        if (results != NULL)
                g_object_unref (results);
}

static void
due_get_upgrades_cb (GsdUpdatesRefresh *refresh, GsdUpdatesManager *manager)
{
        /* optimize the amount of downloaded data by setting the cache age */
        pk_client_set_cache_age (PK_CLIENT(manager->priv->task),
                                 g_settings_get_int (manager->priv->settings_gsd,
                                                     GSD_SETTINGS_FREQUENCY_GET_UPGRADES));

        /* get new distro upgrades list */
        pk_client_get_distro_upgrades_async (PK_CLIENT(manager->priv->task),
                                             NULL,
                                             NULL, NULL,
                                             (GAsyncReadyCallback) get_distro_upgrades_finished_cb,
                                             manager);
}

static void
refresh_cache_finished_cb (GObject *object, GAsyncResult *res, GsdUpdatesManager *manager)
{
        PkClient *client = PK_CLIENT(object);
        PkResults *results;
        GError *error = NULL;
        PkError *error_code = NULL;

        /* get the results */
        results = pk_client_generic_finish (PK_CLIENT(client), res, &error);
        if (results == NULL) {
                g_warning ("failed to refresh the cache: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* check error code */
        error_code = pk_results_get_error_code (results);
        if (error_code != NULL) {
                g_warning ("failed to refresh the cache: %s, %s",
                           pk_error_enum_to_string (pk_error_get_code (error_code)),
                           pk_error_get_details (error_code));
                goto out;
        }
out:
        if (error_code != NULL)
                g_object_unref (error_code);
        if (results != NULL)
                g_object_unref (results);
}

static void
due_refresh_cache_cb (GsdUpdatesRefresh *refresh, GsdUpdatesManager *manager)
{
        /* optimize the amount of downloaded data by setting the cache age */
        pk_client_set_cache_age (PK_CLIENT(manager->priv->task),
                                 g_settings_get_int (manager->priv->settings_gsd,
                                                     GSD_SETTINGS_FREQUENCY_REFRESH_CACHE));

        pk_client_refresh_cache_async (PK_CLIENT(manager->priv->task),
                                       TRUE,
                                       NULL,
                                       NULL, NULL,
                                       (GAsyncReadyCallback) refresh_cache_finished_cb,
                                       manager);
}

static void
notify_critical_updates (GsdUpdatesManager *manager, GPtrArray *array)
{
        const gchar *message;
        const gchar *title;
        gboolean ret;
        GError *error = NULL;
        NotifyNotification *notification;

        /* if the number of critical updates is the same as the last notification,
         * then skip the notifcation as we don't want to bombard the user every hour */
        if (array->len == manager->priv->number_updates_critical_last_shown) {
                g_debug ("ignoring as user ignored last warning");
                return;
        }

        /* save for comparison later */
        manager->priv->number_updates_critical_last_shown = array->len;

        /* TRANSLATORS: title in the libnotify popup */
        title = ngettext ("Update", "Updates", array->len);

        /* TRANSLATORS: message when there are security updates */
        message = ngettext ("An important software update is available",
                            "Important software updates are available", array->len);

        /* close any existing notification */
        if (manager->priv->notification_updates != NULL) {
                notify_notification_close (manager->priv->notification_updates, NULL);
                manager->priv->notification_updates = NULL;
        }

        /* do the bubble */
        g_debug ("title=%s, message=%s", title, message);
        notification = notify_notification_new (title,
                                                message,
                                                GSD_UPDATES_ICON_URGENT);
        notify_notification_set_app_name (notification, _("Software Updates"));
        notify_notification_set_timeout (notification, 15000);
        notify_notification_set_urgency (notification, NOTIFY_URGENCY_CRITICAL);
        notify_notification_add_action (notification, "show-update-viewer",
                                        /* TRANSLATORS: button: open the update viewer to install updates*/
                                        _("Install updates"), libnotify_action_cb, manager, NULL);
        g_signal_connect (notification, "closed",
                          G_CALLBACK (on_notification_closed), NULL);
        ret = notify_notification_show (notification, &error);
        if (!ret) {
                g_warning ("error: %s", error->message);
                g_error_free (error);
        }
        /* track so we can prevent doubled notifications */
        manager->priv->notification_updates = notification;
}

static void
notify_normal_updates_maybe (GsdUpdatesManager *manager, GPtrArray *array)
{
        const gchar *message;
        const gchar *title;
        gboolean ret;
        GError *error = NULL;
        guint64 time_last_notify;
        guint64 time_now;
        guint freq_updates_notify;
        NotifyNotification *notification;

        /* find out if enough time has passed since the last notification */
        time_now = g_get_real_time () / 1000000;
        freq_updates_notify = g_settings_get_int (manager->priv->settings_gsd,
                                                  GSD_SETTINGS_FREQUENCY_UPDATES_NOTIFICATION);
        g_settings_get (manager->priv->settings_gsd,
                        GSD_SETTINGS_LAST_UPDATES_NOTIFICATION,
                        "t", &time_last_notify);
        if (time_last_notify > 0 &&
            (guint64) freq_updates_notify > time_now - time_last_notify) {
                g_debug ("not showing non-critical notification as already shown %i hours ago",
                        (guint) (time_now - time_last_notify) / (60 * 60));
                return;
        }

        /* TRANSLATORS: title in the libnotify popup */
        title = ngettext ("Update", "Updates", array->len);

        /* TRANSLATORS: message when there are non-security updates */
        message = ngettext ("A software update is available.",
                            "Software updates are available.", array->len);

        /* close any existing notification */
        if (manager->priv->notification_updates != NULL) {
                notify_notification_close (manager->priv->notification_updates, NULL);
                manager->priv->notification_updates = NULL;
        }

        /* do the bubble */
        g_debug ("title=%s, message=%s", title, message);
        notification = notify_notification_new (title,
                                                message,
                                                GSD_UPDATES_ICON_NORMAL);
        notify_notification_set_app_name (notification, _("Software Updates"));
        notify_notification_set_timeout (notification, 15000);
        notify_notification_set_urgency (notification, NOTIFY_URGENCY_NORMAL);
        notify_notification_add_action (notification, "show-update-viewer",
                                        /* TRANSLATORS: button: open the update viewer to install updates*/
                                        _("Install updates"), libnotify_action_cb, manager, NULL);
        g_signal_connect (notification, "closed",
                          G_CALLBACK (on_notification_closed), NULL);
        ret = notify_notification_show (notification, &error);
        if (!ret) {
                g_warning ("error: %s", error->message);
                g_error_free (error);
        }

        /* reset notification time */
        g_settings_set (manager->priv->settings_gsd,
                        GSD_SETTINGS_LAST_UPDATES_NOTIFICATION,
                        "t", time_now);

        /* track so we can prevent doubled notifications */
        manager->priv->notification_updates = notification;
}

static void
notify_failed_get_updates_maybe (GsdUpdatesManager *manager)
{
        const gchar *button;
        const gchar *message;
        const gchar *title;
        gboolean ret;
        GError *error = NULL;
        NotifyNotification *notification;

        /* give the user a break */
        if (manager->priv->failed_get_updates_count++ < MAX_FAILED_GET_UPDATES) {
                g_debug ("failed GetUpdates, but will retry %i more times before notification",
                         MAX_FAILED_GET_UPDATES - manager->priv->failed_get_updates_count);
                goto out;
        }

        /* TRANSLATORS: the updates mechanism */
        title = _("Updates");

        /* TRANSLATORS: we failed to get the updates multiple times,
         * and now we need to inform the user that something might be wrong */
        message = _("Unable to access software updates");

        /* TRANSLATORS: try again, this time launching the update viewer */
        button = _("Try again");

        notification = notify_notification_new (title,
                                                message,
                                                GSD_UPDATES_ICON_NORMAL);
        notify_notification_set_app_name (notification, _("Software Updates"));
        notify_notification_set_timeout (notification, 120*1000);
        notify_notification_set_urgency (notification, NOTIFY_URGENCY_NORMAL);
        notify_notification_add_action (notification, "show-update-viewer",
                                        button,
                                        libnotify_action_cb,
                                        manager, NULL);
        g_signal_connect (notification, "closed",
                          G_CALLBACK (on_notification_closed), NULL);
        ret = notify_notification_show (notification, &error);
        if (!ret) {
                g_warning ("failed to show notification: %s",
                           error->message);
                g_error_free (error);
        }
out:
        /* reset, even if the message failed */
        manager->priv->failed_get_updates_count = 0;
}

static void
check_updates_for_importance (GsdUpdatesManager *manager)
{
        guint i;
        PkPackage *pkg;
        GPtrArray *important_array;

        /* check each package */
        important_array = g_ptr_array_new ();
        for (i = 0; i < manager->priv->update_packages->len; i++) {
                pkg = g_ptr_array_index (manager->priv->update_packages, i);
                if (pk_package_get_info (pkg) == PK_INFO_ENUM_SECURITY ||
                    pk_package_get_info (pkg) == PK_INFO_ENUM_IMPORTANT)
                        g_ptr_array_add (important_array, pkg);
        }
        if (important_array->len > 0) {
                notify_critical_updates (manager,
                                         important_array);
        } else {
                notify_normal_updates_maybe (manager,
                                             manager->priv->update_packages);
        }
        g_ptr_array_unref (important_array);
}

static void
package_download_finished_cb (GObject *object,
                              GAsyncResult *res,
                              GsdUpdatesManager *manager)
{
        PkClient *client = PK_CLIENT(object);
        PkResults *results;
        GError *error = NULL;
        PkError *error_code = NULL;

        /* get the results */
        results = pk_client_generic_finish (PK_CLIENT(client), res, &error);
        if (results == NULL) {
                g_warning ("failed to download: %s",
                           error->message);
                g_error_free (error);
                notify_failed_get_updates_maybe (manager);
                goto out;
        }

        /* check error code */
        error_code = pk_results_get_error_code (results);
        if (error_code != NULL) {
                g_warning ("failed to download: %s, %s",
                           pk_error_enum_to_string (pk_error_get_code (error_code)),
                           pk_error_get_details (error_code));
                switch (pk_error_get_code (error_code)) {
                case PK_ERROR_ENUM_CANCELLED_PRIORITY:
                case PK_ERROR_ENUM_TRANSACTION_CANCELLED:
                        g_debug ("ignoring error");
                        break;
                default:
                        notify_failed_get_updates_maybe (manager);
                        break;
                }
                goto out;
        }

        /* check to see if should notify */
        check_updates_for_importance (manager);

        /* we succeeded, so allow the shell to query us */
        manager->priv->pending_updates = TRUE;
        emit_changed (manager);
out:
        if (error_code != NULL)
                g_object_unref (error_code);
        if (results != NULL)
                g_object_unref (results);
}

static void
auto_download_updates (GsdUpdatesManager *manager)
{
        gchar **package_ids;
        guint i;
        PkPackage *pkg;

        /* download each package */
        package_ids = g_new0 (gchar *, manager->priv->update_packages->len + 1);
        for (i = 0; i < manager->priv->update_packages->len; i++) {
                pkg = g_ptr_array_index (manager->priv->update_packages, i);
                package_ids[i] = g_strdup (pk_package_get_id (pkg));
        }

#if PK_CHECK_VERSION(0,8,1)
        /* we've set only-download in PkTask */
        pk_task_update_packages_async (manager->priv->task,
                                       package_ids,
                                       manager->priv->cancellable,
                                       NULL, NULL,
                                       (GAsyncReadyCallback) package_download_finished_cb,
                                       manager);
#else
        /* download them all */
        pk_client_download_packages_async (PK_CLIENT(manager->priv->task),
                                           package_ids,
                                           NULL, /* this means system cache */
                                           manager->priv->cancellable,
                                           NULL, NULL,
                                           (GAsyncReadyCallback) package_download_finished_cb,
                                           manager);
#endif
        g_strfreev (package_ids);
}

static void
get_updates_finished_cb (GObject *object,
                         GAsyncResult *res,
                         GsdUpdatesManager *manager)
{
        PkClient *client = PK_CLIENT(object);
        PkResults *results;
        GError *error = NULL;
        gboolean ret;
        PkError *error_code = NULL;

        /* get the results */
        results = pk_client_generic_finish (PK_CLIENT(client), res, &error);
        if (results == NULL) {
                g_warning ("failed to get updates: %s",
                           error->message);
                g_error_free (error);
                notify_failed_get_updates_maybe (manager);
                goto out;
        }

        /* check error code */
        error_code = pk_results_get_error_code (results);
        if (error_code != NULL) {
                g_warning ("failed to get updates: %s, %s",
                           pk_error_enum_to_string (pk_error_get_code (error_code)),
                           pk_error_get_details (error_code));
                switch (pk_error_get_code (error_code)) {
                case PK_ERROR_ENUM_CANCELLED_PRIORITY:
                case PK_ERROR_ENUM_TRANSACTION_CANCELLED:
                        g_debug ("ignoring error");
                        break;
                default:
                        notify_failed_get_updates_maybe (manager);
                        break;
                }
                goto out;
        }

        /* we succeeded, so clear the count */
        manager->priv->failed_get_updates_count = 0;

        /* so we can download or check for important & security updates */
        if (manager->priv->update_packages != NULL)
                g_ptr_array_unref (manager->priv->update_packages);
        manager->priv->update_packages = pk_results_get_package_array (results);

        /* we have no updates */
        if (manager->priv->update_packages->len == 0) {
                g_debug ("no updates");
                goto out;
        }

        /* should we auto-download the updates? */
        ret = g_settings_get_boolean (manager->priv->settings_gsd,
                                      GSD_SETTINGS_AUTO_DOWNLOAD_UPDATES);
        if (ret) {
                auto_download_updates (manager);
                goto out;
        }

        /* just check to see if should notify */
        check_updates_for_importance (manager);
out:
        if (error_code != NULL)
                g_object_unref (error_code);
        if (results != NULL)
                g_object_unref (results);
}

static void
query_updates (GsdUpdatesManager *manager)
{
        /* optimize the amount of downloaded data by setting the cache age */
        pk_client_set_cache_age (PK_CLIENT(manager->priv->task),
                                 g_settings_get_int (manager->priv->settings_gsd,
                                                     GSD_SETTINGS_FREQUENCY_GET_UPDATES));

        /* get new update list */
        pk_client_get_updates_async (PK_CLIENT(manager->priv->task),
                                     pk_bitfield_value (PK_FILTER_ENUM_NONE),
                                     manager->priv->cancellable,
                                     NULL, NULL,
                                     (GAsyncReadyCallback) get_updates_finished_cb,
                                     manager);
}

static void
due_get_updates_cb (GsdUpdatesRefresh *refresh, GsdUpdatesManager *manager)
{
        query_updates (manager);
}

static gchar *
get_proxy_http (GsdUpdatesManager *manager)
{
        gboolean ret;
        gchar *host = NULL;
        gchar *password = NULL;
        gchar *proxy = NULL;
        gchar *username = NULL;
        GString *string = NULL;
        guint port;
        GDesktopProxyMode proxy_mode;

        proxy_mode = g_settings_get_enum (manager->priv->settings_proxy, "mode");
        if (proxy_mode != G_DESKTOP_PROXY_MODE_MANUAL)
                goto out;

        host = g_settings_get_string (manager->priv->settings_http,
                                      "host");
        if (host == NULL)
                goto out;
        port = g_settings_get_int (manager->priv->settings_http,
                                   "port");

        /* use an HTTP auth string? */
        ret = g_settings_get_boolean (manager->priv->settings_http,
                                      "use-authentication");
        if (ret) {
                username = g_settings_get_string (manager->priv->settings_http,
                                                  "authentication-user");
                password = g_settings_get_string (manager->priv->settings_http,
                                                  "authentication-password");
        }

        /* make PackageKit proxy string */
        string = g_string_new (host);
        if (port > 0)
                g_string_append_printf (string, ":%i", port);
        if (username != NULL && password != NULL)
                g_string_append_printf (string, "@%s:%s", username, password);
        else if (username != NULL)
                g_string_append_printf (string, "@%s", username);
        else if (password != NULL)
                g_string_append_printf (string, "@:%s", password);
        proxy = g_string_free (string, FALSE);
out:
        g_free (host);
        g_free (username);
        g_free (password);
        return proxy;
}

static gchar *
get_proxy_ftp (GsdUpdatesManager *manager)
{
        gchar *host = NULL;
        gchar *proxy = NULL;
        GString *string = NULL;
        guint port;
        GDesktopProxyMode proxy_mode;

        proxy_mode = g_settings_get_enum (manager->priv->settings_proxy, "mode");
        if (proxy_mode != G_DESKTOP_PROXY_MODE_MANUAL)
                goto out;

        host = g_settings_get_string (manager->priv->settings_ftp,
                                      "host");
        if (host == NULL)
                goto out;
        port = g_settings_get_int (manager->priv->settings_ftp,
                                   "port");
        if (port == 0)
                goto out;

        /* make PackageKit proxy string */
        string = g_string_new (host);
        if (port > 0)
                g_string_append_printf (string, ":%i", port);
        proxy = g_string_free (string, FALSE);
out:
        g_free (host);
        return proxy;
}


static void
set_proxy_cb (GObject *object, GAsyncResult *res, gpointer user_data)
{
        gboolean ret;
        GError *error = NULL;
        PkControl *control = PK_CONTROL (object);

        /* get the result */
        ret = pk_control_set_proxy_finish (control, res, &error);
        if (!ret) {
                g_warning ("failed to set proxies: %s", error->message);
                g_error_free (error);
        }
}

static void
reload_proxy_settings (GsdUpdatesManager *manager)
{
        gchar *proxy_http;
        gchar *proxy_ftp;

        proxy_http = get_proxy_http (manager);
        proxy_ftp = get_proxy_ftp (manager);

        /* send to daemon */
        pk_control_set_proxy_async (manager->priv->control,
                                    proxy_http,
                                    proxy_ftp,
                                    NULL,
                                    set_proxy_cb,
                                    manager);

        g_free (proxy_http);
        g_free (proxy_ftp);
}

static void
settings_changed_cb (GSettings         *settings,
                     const char        *key,
                     GsdUpdatesManager *manager)
{
        reload_proxy_settings (manager);
}

static void
settings_gsd_changed_cb (GSettings         *settings,
                         const char        *key,
                         GsdUpdatesManager *manager)
{
}

static void
session_inhibit (GsdUpdatesManager *manager)
{
        const gchar *reason;
        GError *error = NULL;
        GVariant *retval = NULL;

        /* state invalid somehow */
        if (manager->priv->inhibit_cookie != 0) {
                g_warning ("already locked");
                goto out;
        }

        /* TRANSLATORS: the reason why we've inhibited it */
        reason = _("A transaction that cannot be interrupted is running");
        retval = g_dbus_proxy_call_sync (manager->priv->proxy_session,
                                         "Inhibit",
                                         g_variant_new ("(susu)",
                                                        "gnome-settings-daemon", /* app-id */
                                                        0, /* xid */
                                                        reason, /* reason */
                                                        4 /* flags */),
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         manager->priv->cancellable,
                                         &error);
        if (retval == NULL) {
                g_warning ("failed to inhibit gnome-session: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* get cookie */
        g_variant_get (retval, "(u)",
                       &manager->priv->inhibit_cookie);
out:
        if (retval != NULL)
                g_variant_unref (retval);
}

static void
session_uninhibit (GsdUpdatesManager *manager)
{
        GError *error = NULL;
        GVariant *retval = NULL;

        /* state invalid somehow */
        if (manager->priv->inhibit_cookie == 0) {
                g_warning ("not locked");
                goto out;
        }
        retval = g_dbus_proxy_call_sync (manager->priv->proxy_session,
                                         "Uninhibit",
                                         g_variant_new ("(u)",
                                                        manager->priv->inhibit_cookie),
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         manager->priv->cancellable,
                                         &error);
        if (retval == NULL) {
                g_warning ("failed to uninhibit gnome-session: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }
out:
        manager->priv->inhibit_cookie = 0;
        if (retval != NULL)
                g_variant_unref (retval);
}

static void
notify_locked_cb (PkControl *control,
                  GParamSpec *pspec,
                  GsdUpdatesManager *manager)
{
        gboolean locked;

        g_object_get (control, "locked", &locked, NULL);

        /* TODO: locked is a bit harsh, we can probably still allow
         * reboot when packages are downloading or the transaction is
         * depsolving */
        if (locked) {
                session_inhibit (manager);
        } else {
                session_uninhibit (manager);
        }
}

static void
update_viewer_appeared_cb (GDBusConnection *connection,
                           const gchar *name,
                           const gchar *name_owner,
                           gpointer user_data)
{
        GsdUpdatesManager *manager = GSD_UPDATES_MANAGER (user_data);

        /* close any existing notification */
        if (manager->priv->notification_updates != NULL) {
                g_debug ("update viewer on the bus, clearing bubble");
                notify_notification_close (manager->priv->notification_updates, NULL);
                manager->priv->notification_updates = NULL;
        }
}

static gboolean
file_exists_in_root (const gchar *root, const gchar *filename)
{
        gboolean ret = FALSE;
        GFile *source;
        gchar *source_path;

        source_path = g_build_filename (root, filename, NULL);
        source = g_file_new_for_path (source_path);

        /* ignore virtual mountpoints */
        if (!g_file_is_native (source))
                goto out;

        /* an interesting file exists */
        ret = g_file_query_exists (source, NULL);
        g_debug ("checking for %s: %s", source_path, ret ? "yes" : "no");
        if (!ret)
                goto out;
out:
        g_free (source_path);
        g_object_unref (source);
        return ret;
}

static void
mount_added_cb (GVolumeMonitor *volume_monitor,
                GMount *mount,
                GsdUpdatesManager *manager)
{
        gboolean ret = FALSE;
        gchar **filenames = NULL;
        gchar *media_repo_filenames;
        gchar *root_path;
        GFile *root;
        guint i;

        /* check if any installed media is an install disk */
        root = g_mount_get_root (mount);
        root_path = g_file_get_path (root);

        /* use settings */
        media_repo_filenames = g_settings_get_string (manager->priv->settings_gsd,
                                                      GSD_SETTINGS_MEDIA_REPO_FILENAMES);
        if (media_repo_filenames == NULL) {
                g_warning ("failed to get media repo filenames");
                goto out;
        }

        /* search each possible filename */
        filenames = g_strsplit (media_repo_filenames, ",", -1);
        for (i=0; filenames[i] != NULL; i++) {
                ret = file_exists_in_root (root_path, filenames[i]);
                if (ret)
                        break;
        }

        /* do an updates check with the new media */
        if (ret)
                query_updates (manager);
out:
        g_strfreev (filenames);
        g_free (media_repo_filenames);
        g_free (root_path);
        g_object_unref (root);
}

static GVariant *
handle_get_property (GDBusConnection *connection_, const gchar *sender,
                     const gchar *object_path, const gchar *interface_name,
                     const gchar *property_name, GError **error,
                     gpointer user_data)
{
        GVariant *retval = NULL;
        GsdUpdatesManager *manager = GSD_UPDATES_MANAGER(user_data);

        if (g_strcmp0 (property_name, "PendingUpdates") == 0) {
                retval = g_variant_new_boolean (manager->priv->pending_updates);
        }

        return retval;
}

static void
emit_changed (GsdUpdatesManager *manager)
{
        gboolean ret;
        GError *error = NULL;

        /* check we are connected */
        if (manager->priv->connection == NULL)
                return;

        /* just emit signal */
        ret = g_dbus_connection_emit_signal (manager->priv->connection,
                                             NULL,
                                             "/",
                                             "org.gnome.SettingsDaemonUpdates",
                                             "Changed",
                                             NULL,
                                             &error);
        if (!ret) {
                g_warning ("failed to emit signal: %s", error->message);
                g_error_free (error);
        }
}

static const GDBusInterfaceVTable interface_vtable =
{
        NULL, /* MethodCall */
        handle_get_property, /* GetProperty */
        NULL, /* SetProperty */
};

static void
on_bus_gotten (GObject *source_object,
               GAsyncResult *res,
               GsdUpdatesManager *manager)
{
        GDBusConnection *connection;
        GError *error = NULL;

        connection = g_bus_get_finish (res, &error);
        if (connection == NULL) {
                g_warning ("Could not get session bus: %s",
                           error->message);
                g_error_free (error);
                return;
        }
        manager->priv->connection = connection;

        g_dbus_connection_register_object (connection,
                                           "/",
                                           manager->priv->introspection->interfaces[0],
                                           &interface_vtable,
                                           manager,
                                           NULL,
                                           NULL);
}

#define PK_OFFLINE_UPDATE_RESULTS_GROUP		"PackageKit Offline Update Results"
#define PK_OFFLINE_UPDATE_RESULTS_FILENAME	"/var/lib/PackageKit/offline-update-competed"

static gboolean
check_offline_update_cb (gpointer user_data)
{
        const gchar *message;
        const gchar *title;
        gboolean ret;
        gboolean success;
        gchar *error_code = NULL;
        gchar *error_details = NULL;
        gchar *packages = NULL;
        GError *error = NULL;
        GKeyFile *key_file = NULL;
        GsdUpdatesManager *manager = (GsdUpdatesManager *) user_data;
        guint i;
        guint num_packages = 1;
        NotifyNotification *notification;
        PkErrorEnum error_enum = PK_ERROR_ENUM_UNKNOWN;

        /* was any offline update attempted */
        if (!g_file_test (PK_OFFLINE_UPDATE_RESULTS_FILENAME, G_FILE_TEST_EXISTS))
                goto out;

        /* open the file and see what happened */
        key_file = g_key_file_new ();
        ret = g_key_file_load_from_file (key_file,
                                         PK_OFFLINE_UPDATE_RESULTS_FILENAME,
                                         G_KEY_FILE_NONE,
                                         &error);
        if (!ret) {
                g_warning ("failed to open %s: %s",
                           PK_OFFLINE_UPDATE_RESULTS_FILENAME,
                           error->message);
                g_error_free (error);
                goto out;
        }
        success = g_key_file_get_boolean (key_file,
                                          PK_OFFLINE_UPDATE_RESULTS_GROUP,
                                          "Success",
                                          NULL);
        if (success) {
                packages = g_key_file_get_string (key_file,
                                                  PK_OFFLINE_UPDATE_RESULTS_GROUP,
                                                  "Packages",
                                                  NULL);
                if (packages == NULL) {
                        g_warning ("No 'Packages' in %s",
                                   PK_OFFLINE_UPDATE_RESULTS_FILENAME);
                        goto out;
                }

                /* count the packages for translators */
                for (i = 0; packages[i] != '\0'; i++) {
                        if (packages[i] == ',')
                                num_packages++;
                }

                /* TRANSLATORS: title in the libnotify popup */
                title = ngettext ("Software Update Installed",
                                  "Software Updates Installed",
                                  num_packages);

                /* TRANSLATORS: message when we've done offline updates */
                message = ngettext ("An important OS update has been installed.",
                                    "Important OS updates have been installed.",
                                    num_packages);

                /* no need to keep the file around anymore */
                clear_offline_updates_message ();
        } else {
                /* get error details */
                manager->priv->offline_update_error = pk_error_new ();

                error_code = g_key_file_get_string (key_file,
                                                    PK_OFFLINE_UPDATE_RESULTS_GROUP,
                                                    "ErrorCode",
                                                    NULL);
                if (error_code != NULL)
                        error_enum = pk_error_enum_from_string (error_code);
                error_details = g_key_file_get_string (key_file,
                                                       PK_OFFLINE_UPDATE_RESULTS_GROUP,
                                                       "ErrorDetails",
                                                       NULL);
                g_object_set (manager->priv->offline_update_error,
                              "code", error_enum,
                              "details", error_details,
                              NULL);

                /* TRANSLATORS: title in the libnotify popup */
                title = _("Software Updates Failed");

                /* TRANSLATORS: message when we've not done offline updates */
                message = _("An important OS update failed to be installed.");
        }

        /* do the bubble */
        g_debug ("title=%s, message=%s", title, message);
        notification = notify_notification_new (title,
                                                message,
                                                GSD_UPDATES_ICON_URGENT);
        notify_notification_set_app_name (notification, _("Software Updates"));
        notify_notification_set_timeout (notification, -1);
        notify_notification_set_urgency (notification, NOTIFY_URGENCY_NORMAL);
        if (success) {
#if 0
                notify_notification_add_action (notification, "review-offline-updates",
                                                /* TRANSLATORS: button: review the offline update changes */
                                                _("Review"), libnotify_action_cb, manager, NULL);
#endif
        } else {
                notify_notification_add_action (notification, "error-offline-updates",
                                                /* TRANSLATORS: button: review the offline update changes */
                                                _("Show details"), libnotify_action_cb, manager, NULL);
        }
        notify_notification_add_action (notification, "clear-offline-updates",
                                        /* TRANSLATORS: button: clear notification */
                                        _("OK"), libnotify_action_cb, manager, NULL);
        g_signal_connect (notification, "closed",
                          G_CALLBACK (on_notification_closed), NULL);
        ret = notify_notification_show (notification, &error);
        if (!ret) {
                g_warning ("error: %s", error->message);
                g_error_free (error);
        }
out:
        g_free (packages);
        g_free (error_code);
        g_free (error_details);
        if (key_file != NULL)
                g_key_file_free (key_file);
        manager->priv->offline_update_id = 0;
        return FALSE;
}

gboolean
gsd_updates_manager_start (GsdUpdatesManager *manager,
                           GError **error)
{
        gboolean ret = FALSE;
        gchar *introspection_data = NULL;
        GFile *file = NULL;

        g_debug ("Starting updates manager");

        /* use PackageKit */
        manager->priv->cancellable = g_cancellable_new ();
        manager->priv->control = pk_control_new ();
        g_signal_connect (manager->priv->control, "notify::locked",
                          G_CALLBACK (notify_locked_cb), manager);
        manager->priv->task = pk_task_new ();
        g_object_set (manager->priv->task,
                      "background", TRUE,
                      "interactive", FALSE,
#if PK_CHECK_VERSION(0,8,1)
                      "only-download", TRUE,
#endif
                      NULL);

        /* watch UDev for missing firmware */
        manager->priv->firmware = gsd_updates_firmware_new ();

        /* get automatic callbacks about when we should check for
         * updates, refresh-caches and upgrades */
        manager->priv->refresh = gsd_updates_refresh_new ();
        g_signal_connect (manager->priv->refresh, "get-upgrades",
                          G_CALLBACK (due_get_upgrades_cb), manager);
        g_signal_connect (manager->priv->refresh, "refresh-cache",
                          G_CALLBACK (due_refresh_cache_cb), manager);
        g_signal_connect (manager->priv->refresh, "get-updates",
                          G_CALLBACK (due_get_updates_cb), manager);

        /* get proxy settings */
        manager->priv->settings_proxy = g_settings_new ("org.gnome.system.proxy");
        g_signal_connect (manager->priv->settings_proxy, "changed",
                          G_CALLBACK (settings_changed_cb), manager);

        /* get http settings */
        manager->priv->settings_http = g_settings_new ("org.gnome.system.proxy.http");
        g_signal_connect (manager->priv->settings_http, "changed",
                          G_CALLBACK (settings_changed_cb), manager);

        /* get ftp settings */
        manager->priv->settings_ftp = g_settings_new ("org.gnome.system.proxy.ftp");
        g_signal_connect (manager->priv->settings_ftp, "changed",
                          G_CALLBACK (settings_changed_cb), manager);

        /* get ftp settings */
        manager->priv->settings_gsd = g_settings_new ("org.gnome.settings-daemon.plugins.updates");
        g_signal_connect (manager->priv->settings_gsd, "changed",
                          G_CALLBACK (settings_gsd_changed_cb), manager);

        /* use gnome-session for the idle detection */
        manager->priv->proxy_session =
                g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                               G_DBUS_PROXY_FLAGS_NONE,
                                               NULL, /* GDBusInterfaceInfo */
                                               "org.gnome.SessionManager",
                                               "/org/gnome/SessionManager",
                                               "org.gnome.SessionManager",
                                               manager->priv->cancellable,
                                               error);
        if (manager->priv->proxy_session == NULL)
                goto out;

        /* if the update viewer is started, then hide the notification */
        manager->priv->update_viewer_watcher_id =
                g_bus_watch_name (G_BUS_TYPE_SESSION,
                                  "org.freedesktop.PackageKit.UpdateViewer",
                                  G_BUS_NAME_WATCHER_FLAGS_NONE,
                                  update_viewer_appeared_cb,
                                  NULL,
                                  manager,
                                  NULL);

        /* get a volume monitor so we can watch media */
        manager->priv->volume_monitor = g_volume_monitor_get ();
        g_signal_connect (manager->priv->volume_monitor, "mount-added",
                          G_CALLBACK (mount_added_cb), manager);

        /* coldplug */
        reload_proxy_settings (manager);

        /* load introspection from file */
        file = g_file_new_for_path (DATADIR "/dbus-1/interfaces/org.gnome.SettingsDaemonUpdates.xml");
        ret = g_file_load_contents (file, NULL, &introspection_data, NULL, NULL, error);
        if (!ret)
                goto out;

        /* build introspection from XML */
        manager->priv->introspection = g_dbus_node_info_new_for_xml (introspection_data, error);
        if (manager->priv->introspection == NULL)
                goto out;

        /* export the object */
        g_bus_get (G_BUS_TYPE_SESSION,
                   NULL,
                   (GAsyncReadyCallback) on_bus_gotten,
                   manager);

        /* check for offline update */
        manager->priv->offline_update_id =
                g_timeout_add_seconds (5,
                                       check_offline_update_cb,
                                       manager);

        /* success */
        ret = TRUE;
        g_debug ("Started updates manager");
out:
        g_free (introspection_data);
        return ret;
}

void
gsd_updates_manager_stop (GsdUpdatesManager *manager)
{
        g_debug ("Stopping updates manager");

        g_clear_object (&manager->priv->settings_proxy);
        g_clear_object (&manager->priv->settings_http);
        g_clear_object (&manager->priv->settings_ftp);
        g_clear_object (&manager->priv->settings_gsd);
        g_clear_object (&manager->priv->control);
        g_clear_object (&manager->priv->task);
        g_clear_object (&manager->priv->refresh);
        g_clear_object (&manager->priv->firmware);
        g_clear_object (&manager->priv->proxy_session);
        g_clear_object (&manager->priv->volume_monitor);
        g_clear_object (&manager->priv->cancellable);
        g_clear_pointer (&manager->priv->introspection, g_dbus_node_info_unref);
        if (manager->priv->update_viewer_watcher_id != 0) {
                g_bus_unwatch_name (manager->priv->update_viewer_watcher_id);
                manager->priv->update_viewer_watcher_id = 0;
        }
        if (manager->priv->owner_id > 0) {
                g_bus_unown_name (manager->priv->owner_id);
                manager->priv->owner_id = 0;
        }
        if (manager->priv->offline_update_id) {
                g_source_remove (manager->priv->offline_update_id);
                manager->priv->offline_update_id = 0;
        }
        if (manager->priv->update_packages != NULL) {
                g_ptr_array_unref (manager->priv->update_packages);
                manager->priv->update_packages = NULL;
        }
        g_clear_object (&manager->priv->offline_update_error);
}

static GObject *
gsd_updates_manager_constructor (
                GType type,
                guint n_construct_properties,
                GObjectConstructParam *construct_properties)
{
        GsdUpdatesManager *m;

        m = GSD_UPDATES_MANAGER (G_OBJECT_CLASS (gsd_updates_manager_parent_class)->constructor (
                                                           type,
                                                           n_construct_properties,
                                                           construct_properties));

        return G_OBJECT (m);
}

static void
gsd_updates_manager_dispose (GObject *object)
{
        GsdUpdatesManager *manager;

        manager = GSD_UPDATES_MANAGER (object);

        gsd_updates_manager_stop (manager);

        G_OBJECT_CLASS (gsd_updates_manager_parent_class)->dispose (object);
}

static void
gsd_updates_manager_class_init (GsdUpdatesManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = gsd_updates_manager_constructor;
        object_class->dispose = gsd_updates_manager_dispose;
        object_class->finalize = gsd_updates_manager_finalize;

        g_type_class_add_private (klass, sizeof (GsdUpdatesManagerPrivate));
}

static void
gsd_updates_manager_init (GsdUpdatesManager *manager)
{
        manager->priv = GSD_UPDATES_MANAGER_GET_PRIVATE (manager);
}

static void
gsd_updates_manager_finalize (GObject *object)
{
        GsdUpdatesManager *updates_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_UPDATES_MANAGER (object));

        updates_manager = GSD_UPDATES_MANAGER (object);

        g_return_if_fail (updates_manager->priv);

        G_OBJECT_CLASS (gsd_updates_manager_parent_class)->finalize (object);
}

GsdUpdatesManager *
gsd_updates_manager_new (void)
{
        if (manager_object) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (GSD_TYPE_UPDATES_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object, (gpointer *) &manager_object);
        }

        return GSD_UPDATES_MANAGER (manager_object);
}

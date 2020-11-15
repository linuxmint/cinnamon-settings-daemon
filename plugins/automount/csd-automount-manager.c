/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 *
 * Author: Tomas Bzatek <tbzatek@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "cinnamon-settings-profile.h"
#include "cinnamon-settings-session.h"
#include "csd-automount-manager.h"
#include "csd-autorun.h"

#define CSD_AUTOMOUNT_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CSD_TYPE_AUTOMOUNT_MANAGER, CsdAutomountManagerPrivate))

struct CsdAutomountManagerPrivate
{
        GSettings   *settings;
        GSettings   *settings_screensaver;

	GVolumeMonitor *volume_monitor;
	unsigned int automount_idle_id;

        CinnamonSettingsSession *session;
        gboolean session_is_active;
        gboolean screensaver_active;
        guint ss_watch_id;
        GDBusProxy *ss_proxy;

        GList *volume_queue;
};


G_DEFINE_TYPE (CsdAutomountManager, csd_automount_manager, G_TYPE_OBJECT)

static GtkDialog *
show_error_dialog (const char *primary_text,
		   const char *secondary_text)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (NULL,
					 0,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK,
					 "%s", "");

	g_object_set (dialog,
		      "text", primary_text,
		      "secondary-text", secondary_text,
		      NULL);

	gtk_widget_show (GTK_WIDGET (dialog));

	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	return GTK_DIALOG (dialog);
}

static void
startup_volume_mount_cb (GObject *source_object,
			 GAsyncResult *res,
			 gpointer user_data)
{
	g_volume_mount_finish (G_VOLUME (source_object), res, NULL);
}

static void
automount_all_volumes (CsdAutomountManager *manager)
{
	GList *volumes, *l;
	GMount *mount;
	GVolume *volume;

	if (g_settings_get_boolean (manager->priv->settings, "automount")) {
		/* automount all mountable volumes at start-up */
		volumes = g_volume_monitor_get_volumes (manager->priv->volume_monitor);
		for (l = volumes; l != NULL; l = l->next) {
			volume = l->data;

			if (!g_volume_should_automount (volume) ||
			    !g_volume_can_mount (volume)) {
				continue;
			}

			mount = g_volume_get_mount (volume);
			if (mount != NULL) {
				g_object_unref (mount);
				continue;
			}

			/* pass NULL as GMountOperation to avoid user interaction */
			g_volume_mount (volume, 0, NULL, NULL, startup_volume_mount_cb, NULL);
		}
		g_list_free_full (volumes, g_object_unref);
	}
}

static gboolean
automount_all_volumes_idle_cb (gpointer data)
{
	CsdAutomountManager *manager = CSD_AUTOMOUNT_MANAGER (data);

	automount_all_volumes (manager);

	manager->priv->automount_idle_id = 0;
	return FALSE;
}

static void
volume_mount_cb (GObject *source_object,
		 GAsyncResult *res,
		 gpointer user_data)
{
	GMountOperation *mount_op = user_data;
	GError *error;
	char *primary;
	char *name;

	error = NULL;
	csd_allow_autorun_for_volume_finish (G_VOLUME (source_object));
	if (!g_volume_mount_finish (G_VOLUME (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED && error->code != G_IO_ERROR_ALREADY_MOUNTED) {
			name = g_volume_get_name (G_VOLUME (source_object));
			primary = g_strdup_printf (_("Unable to mount %s"), name);
			g_free (name);
			show_error_dialog (primary,
				           error->message);
			g_free (primary);
		}
		g_error_free (error);
	}

	g_object_unref (mount_op);
}

static void
do_mount_volume (GVolume *volume)
{
	GMountOperation *mount_op;

	mount_op = gtk_mount_operation_new (NULL);
	g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);

	csd_allow_autorun_for_volume (volume);
	g_volume_mount (volume, 0, mount_op, NULL, volume_mount_cb, mount_op);
}

static void
check_volume_queue (CsdAutomountManager *manager)
{
        GList *l;
        GVolume *volume;

        if (manager->priv->screensaver_active)
                return;

        l = manager->priv->volume_queue;

        while (l != NULL) {
                volume = l->data;

                do_mount_volume (volume);
                manager->priv->volume_queue =
                        g_list_remove (manager->priv->volume_queue, volume);

                g_object_unref (volume);
                l = l->next;
        }

        manager->priv->volume_queue = NULL;
}

static void
check_screen_lock_and_mount (CsdAutomountManager *manager,
                             GVolume *volume)
{
        if (!manager->priv->session_is_active)
                return;

        if (manager->priv->screensaver_active) {
                /* queue the volume, to mount it after the screensaver state changed */
                g_debug ("Queuing volume %p", volume);
                manager->priv->volume_queue = g_list_prepend (manager->priv->volume_queue,
                                                              g_object_ref (volume));
        } else {
                /* mount it immediately */
                do_mount_volume (volume);
        }
}

static void
volume_removed_callback (GVolumeMonitor *monitor,
                         GVolume *volume,
                         CsdAutomountManager *manager)
{
        g_debug ("Volume %p removed, removing from the queue", volume);

        /* clear it from the queue, if present */
        manager->priv->volume_queue =
                g_list_remove (manager->priv->volume_queue, volume);
}

static void
volume_added_callback (GVolumeMonitor *monitor,
		       GVolume *volume,
		       CsdAutomountManager *manager)
{
	if (g_settings_get_boolean (manager->priv->settings, "automount") &&
	    g_volume_should_automount (volume) &&
	    g_volume_can_mount (volume)) {
                check_screen_lock_and_mount (manager, volume);
	} else {
		/* Allow csd_autorun() to run. When the mount is later
		 * added programmatically (i.e. for a blank CD),
		 * csd_autorun() will be called by mount_added_callback(). */
		csd_allow_autorun_for_volume (volume);
		csd_allow_autorun_for_volume_finish (volume);
	}
}

static void
autorun_show_window (GMount *mount, gpointer user_data)
{
	GFile *location;
        char *uri;
        GError *error;
	char *primary;
	char *name;

	location = g_mount_get_root (mount);
  uri = g_file_get_uri (location);

  error = NULL;
	/* use default folder handler */
  g_debug("Opening %s", uri);

  if (g_str_has_prefix (uri, "afc://")) {
    // AFC (Apple File Conduit, which runs on iPhone and other Apple devices) doesn't always work well
    // Observed on an iOS 4 device it would work the first time the device was connected, and then indefinitely hang after that
    // Even a simple 'ls /run/user/$USER/gvfs' would hang forever
    // It is unacceptable for CSD to hang, so we're treating AFC differently (asynchronously)
    g_debug("AFC protocol detected, opening asynchronously!");
    char * command = g_strdup_printf("timeout 10s xdg-open %s", uri);
    g_debug("Executing command '%s'", command);
    system(command);
    g_debug("Command was executed, moving on..");
    g_free(command);
  }
  else {
    if (! gtk_show_uri (NULL, uri, GDK_CURRENT_TIME, &error)) {
  		name = g_mount_get_name (mount);
  		primary = g_strdup_printf (_("Unable to open a folder for %s"), name);
  		g_free (name);
  		show_error_dialog (primary, error->message);
  		g_free (primary);
  		g_error_free (error);
    }
  }

  g_free (uri);
	g_object_unref (location);
}

static void
mount_added_callback (GVolumeMonitor *monitor,
		      GMount *mount,
		      CsdAutomountManager *manager)
{
        /* don't autorun if the session is not active */
        if (!manager->priv->session_is_active) {
                return;
        }

	csd_autorun (mount, manager->priv->settings, autorun_show_window, manager);
}


static void
session_state_changed (CinnamonSettingsSession *session, GParamSpec *pspec, gpointer user_data)
{
        CsdAutomountManager *manager = user_data;
        CsdAutomountManagerPrivate *p = manager->priv;

        if (cinnamon_settings_session_get_state (session) == CINNAMON_SETTINGS_SESSION_STATE_ACTIVE) {
                p->session_is_active = TRUE;
        }
        else {
                p->session_is_active = FALSE;
        }

        if (!p->session_is_active) {
                if (p->volume_queue != NULL) {
                        g_list_free_full (p->volume_queue, g_object_unref);
                        p->volume_queue = NULL;
                }
        }
}

static void
do_initialize_session (CsdAutomountManager *manager)
{
        manager->priv->session = cinnamon_settings_session_new ();
        g_signal_connect (manager->priv->session, "notify::state",
                          G_CALLBACK (session_state_changed), manager);
        session_state_changed (manager->priv->session, NULL, manager);
}

#define SCREENSAVER_NAME "org.cinnamon.ScreenSaver"
#define SCREENSAVER_PATH "/org/cinnamon/ScreenSaver"
#define SCREENSAVER_INTERFACE "org.cinnamon.ScreenSaver"

static void
screensaver_signal_callback (GDBusProxy *proxy,
                             const gchar *sender_name,
                             const gchar *signal_name,
                             GVariant *parameters,
                             gpointer user_data)
{
        CsdAutomountManager *manager = user_data;

        if (g_strcmp0 (signal_name, "ActiveChanged") == 0) {
                g_variant_get (parameters, "(b)", &manager->priv->screensaver_active);
                g_debug ("Screensaver active changed to %d", manager->priv->screensaver_active);

                check_volume_queue (manager);
        }
}

static void
screensaver_get_active_ready_cb (GObject *source,
                                 GAsyncResult *res,
                                 gpointer user_data)
{
        CsdAutomountManager *manager = user_data;
        GDBusProxy *proxy = manager->priv->ss_proxy;
        GVariant *result;
        GError *error = NULL;

        result = g_dbus_proxy_call_finish (proxy,
                                           res,
                                           &error);

        if (error != NULL) {
                g_warning ("Can't call GetActive() on the ScreenSaver object: %s",
                           error->message);
                g_error_free (error);

                return;
        }

        g_variant_get (result, "(b)", &manager->priv->screensaver_active);
        g_variant_unref (result);

        g_debug ("Screensaver GetActive() returned %d", manager->priv->screensaver_active);
}

static void
screensaver_proxy_ready_cb (GObject *source,
                            GAsyncResult *res,
                            gpointer user_data)
{
        CsdAutomountManager *manager = user_data;
        GError *error = NULL;
        GDBusProxy *ss_proxy;
        
        ss_proxy = g_dbus_proxy_new_finish (res, &error);

        if (error != NULL) {
                g_warning ("Can't get proxy for the ScreenSaver object: %s",
                           error->message);
                g_error_free (error);

                return;
        }

        g_debug ("ScreenSaver proxy ready");

        manager->priv->ss_proxy = ss_proxy;

        g_signal_connect (ss_proxy, "g-signal",
                          G_CALLBACK (screensaver_signal_callback), manager);

        g_dbus_proxy_call (ss_proxy,
                           "GetActive",
                           NULL,
                           G_DBUS_CALL_FLAGS_NO_AUTO_START,
                           -1,
                           NULL,
                           screensaver_get_active_ready_cb,
                           manager);
}

static void
screensaver_appeared_callback (GDBusConnection *connection,
                               const gchar *name,
                               const gchar *name_owner,
                               gpointer user_data)
{
        CsdAutomountManager *manager = user_data;

        g_debug ("ScreenSaver name appeared");

        manager->priv->screensaver_active = FALSE;

        g_dbus_proxy_new (connection,
                          G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                          NULL,
                          name,
                          SCREENSAVER_PATH,
                          SCREENSAVER_INTERFACE,
                          NULL,
                          screensaver_proxy_ready_cb,
                          manager);
}

static void
screensaver_vanished_callback (GDBusConnection *connection,
                               const gchar *name,
                               gpointer user_data)
{
        CsdAutomountManager *manager = user_data;

        g_debug ("ScreenSaver name vanished");

        manager->priv->screensaver_active = FALSE;

        if (manager->priv->ss_proxy != NULL) {
                g_object_unref (manager->priv->ss_proxy);
                manager->priv->ss_proxy = NULL;
        }

        /* in this case force a clear of the volume queue, without
         * mounting them.
         */
        if (manager->priv->volume_queue != NULL) {
                g_list_free_full (manager->priv->volume_queue, g_object_unref);
                manager->priv->volume_queue = NULL;
        }
}

static void
do_initialize_screensaver (CsdAutomountManager *manager)
{
        CsdAutomountManagerPrivate *p = manager->priv;

        p->ss_watch_id =
                g_bus_watch_name (G_BUS_TYPE_SESSION,
                                  SCREENSAVER_NAME,
                                  G_BUS_NAME_WATCHER_FLAGS_NONE,
                                  screensaver_appeared_callback,
                                  screensaver_vanished_callback,
                                  manager,
                                  NULL);
}

static void
setup_automounter (CsdAutomountManager *manager)
{
        do_initialize_session (manager);

        gchar *custom_saver = g_settings_get_string (manager->priv->settings_screensaver,
                                                     "custom-screensaver-command");

        /* if we fail to get the gsettings entry, or if the user did not select
         * a custom screen saver, default to cinnamon-screensaver */
        if (NULL == custom_saver || g_strcmp0 (custom_saver, "") == 0)
                do_initialize_screensaver (manager);
        g_free (custom_saver);
        
	manager->priv->volume_monitor = g_volume_monitor_get ();
	g_signal_connect_object (manager->priv->volume_monitor, "mount-added",
				 G_CALLBACK (mount_added_callback), manager, 0);
	g_signal_connect_object (manager->priv->volume_monitor, "volume-added",
				 G_CALLBACK (volume_added_callback), manager, 0);
	g_signal_connect_object (manager->priv->volume_monitor, "volume-removed",
				 G_CALLBACK (volume_removed_callback), manager, 0);

	manager->priv->automount_idle_id =
		g_idle_add_full (G_PRIORITY_LOW,
				 automount_all_volumes_idle_cb,
				 manager, NULL);
}

gboolean
csd_automount_manager_start (CsdAutomountManager *manager,
                                       GError              **error)
{
        g_debug ("Starting automounting manager");
        cinnamon_settings_profile_start (NULL);

        manager->priv->settings = g_settings_new ("org.cinnamon.desktop.media-handling");
        manager->priv->settings_screensaver = g_settings_new ("org.cinnamon.desktop.screensaver");
        setup_automounter (manager);

        cinnamon_settings_profile_end (NULL);

        return TRUE;
}

void
csd_automount_manager_stop (CsdAutomountManager *manager)
{
        CsdAutomountManagerPrivate *p = manager->priv;

        g_debug ("Stopping automounting manager");

        if (p->session != NULL) {
                g_object_unref (p->session);
                p->session = NULL;
        }

        if (p->volume_monitor != NULL) {
                g_object_unref (p->volume_monitor);
                p->volume_monitor = NULL;
        }

        if (p->settings != NULL) {
                g_object_unref (p->settings);
                p->settings = NULL;
        }

        if (p->settings_screensaver != NULL) {
                g_object_unref (p->settings_screensaver);
                p->settings_screensaver = NULL;
        }

        if (p->ss_proxy != NULL) {
                g_object_unref (p->ss_proxy);
                p->ss_proxy = NULL;
        }

        g_bus_unwatch_name (p->ss_watch_id);

        if (p->volume_queue != NULL) {
                g_list_free_full (p->volume_queue, g_object_unref);
                p->volume_queue = NULL;
        }

        if (p->automount_idle_id != 0) {
                g_source_remove (p->automount_idle_id);
                p->automount_idle_id = 0;
        }
}

static void
csd_automount_manager_class_init (CsdAutomountManagerClass *klass)
{
        g_type_class_add_private (klass, sizeof (CsdAutomountManagerPrivate));
}

static void
csd_automount_manager_init (CsdAutomountManager *manager)
{
        manager->priv = CSD_AUTOMOUNT_MANAGER_GET_PRIVATE (manager);
}

CsdAutomountManager *
csd_automount_manager_new (void)
{
        return CSD_AUTOMOUNT_MANAGER (g_object_new (CSD_TYPE_AUTOMOUNT_MANAGER, NULL));
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2001-2003 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2006-2007 William Jon McCann <mccann@jhu.edu>
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
#include <math.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

#ifdef HAVE_GUDEV
#include <gudev/gudev.h>
#endif

#include "cinnamon-settings-profile.h"
#include "csd-marshal.h"
#include "csd-media-keys-manager.h"

#include "shortcuts-list.h"
#include "csd-osd-window.h"
#include "csd-input-helper.h"
#include "csd-power-helper.h"
#include "csd-enums.h"

#include <canberra.h>
#include <pulse/pulseaudio.h>
#include "gvc-mixer-control.h"

#include <libnotify/notify.h>

/* For media keys, we need to keep using org.gnome because
   that's what apps are looking for */
#define GNOME_DBUS_PATH "/org/gnome/SettingsDaemon"
#define GNOME_DBUS_NAME "org.gnome.SettingsDaemon"
#define CSD_MEDIA_KEYS_DBUS_PATH GNOME_DBUS_PATH "/MediaKeys"
#define CSD_MEDIA_KEYS_DBUS_NAME GNOME_DBUS_NAME ".MediaKeys"

#define CINNAMON_DBUS_PATH "/org/cinnamon/SettingsDaemon"
#define CINNAMON_DBUS_NAME "org.cinnamon.SettingsDaemon"

#define CINNAMON_KEYBINDINGS_PATH CINNAMON_DBUS_PATH "/KeybindingHandler"
#define CINNAMON_KEYBINDINGS_NAME CINNAMON_DBUS_NAME ".KeybindingHandler"

#define GNOME_SESSION_DBUS_NAME "org.gnome.SessionManager"
#define GNOME_SESSION_DBUS_PATH "/org/gnome/SessionManager"
#define GNOME_SESSION_DBUS_INTERFACE "org.gnome.SessionManager"

#define GNOME_KEYRING_DBUS_NAME "org.gnome.keyring"
#define GNOME_KEYRING_DBUS_PATH "/org/gnome/keyring/daemon"
#define GNOME_KEYRING_DBUS_INTERFACE "org.gnome.keyring.Daemon"


static const gchar introspection_xml[] =
"<node>"
"  <interface name='org.gnome.SettingsDaemon.MediaKeys'>"
"    <annotation name='org.freedesktop.DBus.GLib.CSymbol' value='csd_media_keys_manager'/>"
"    <method name='GrabMediaPlayerKeys'>"
"      <arg name='application' direction='in' type='s'/>"
"      <arg name='time' direction='in' type='u'/>"
"    </method>"
"    <method name='ReleaseMediaPlayerKeys'>"
"      <arg name='application' direction='in' type='s'/>"
"    </method>"
"    <signal name='MediaPlayerKeyPressed'>"
"      <arg name='application' type='s'/>"
"      <arg name='key' type='s'/>"
"    </signal>"
"  </interface>"
"</node>";

static const gchar kb_introspection_xml[] =
"<node>"
"  <interface name='org.cinnamon.SettingsDaemon.KeybindingHandler'>"
"    <annotation name='org.freedesktop.DBus.GLib.CSymbol' value='csd_media_keys_manager'/>"
"    <method name='HandleKeybinding'>"
"      <arg name='type' direction='in' type='u'/>"
"    </method>"
"  </interface>"
"</node>";

#define SETTINGS_INTERFACE_DIR "org.cinnamon.desktop.interface"
#define SETTINGS_POWER_DIR "org.cinnamon.settings-daemon.plugins.power"
#define SETTINGS_XSETTINGS_DIR "org.cinnamon.settings-daemon.plugins.xsettings"
#define SETTINGS_TOUCHPAD_DIR "org.cinnamon.settings-daemon.peripherals.touchpad"
#define TOUCHPAD_ENABLED_KEY "touchpad-enabled"
#define HIGH_CONTRAST "HighContrast"

#define VOLUME_STEP 6           /* percents for one volume button press */
#define MAX_VOLUME 65536.0

#define CSD_MEDIA_KEYS_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CSD_TYPE_MEDIA_KEYS_MANAGER, CsdMediaKeysManagerPrivate))

typedef struct {
        char   *application;
        char   *name;
        guint32 time;
        guint   watch_id;
} MediaPlayer;

typedef struct {
        MediaKeyType key_type;
        const char *settings_key;
        const char *hard_coded;
        char *custom_path;
        char *custom_command;
        Key *key;
} MediaKey;

struct CsdMediaKeysManagerPrivate
{
        /* Volume bits */
        GvcMixerControl *volume;
        GvcMixerStream  *stream;
        ca_context      *ca;
        GtkSettings     *gtksettings;
#ifdef HAVE_GUDEV
        GHashTable      *streams; /* key = X device ID, value = stream id */
        GUdevClient     *udev_client;
#endif /* HAVE_GUDEV */

        GtkWidget       *dialog;
        GSettings       *settings;

        GPtrArray       *keys;

        /* HighContrast theme settings */
        GSettings       *interface_settings;
        char            *icon_theme;
        char            *gtk_theme;

        /* Power stuff */
        GSettings       *power_settings;
        GDBusProxy      *upower_proxy;
        GDBusProxy      *power_screen_proxy;
        GDBusProxy      *power_keyboard_proxy;

        /* Multihead stuff */
        GdkScreen       *current_screen;
        GSList          *screens;
        int              opcode;

        GList           *media_players;

        GDBusNodeInfo   *introspection_data;
        GDBusNodeInfo   *kb_introspection_data;
        GDBusConnection *connection;
        GCancellable    *bus_cancellable;
        GDBusProxy      *xrandr_proxy;
        GCancellable    *cancellable;

        guint            start_idle_id;

        /* Ubuntu notifications */
        NotifyNotification *volume_notification;
        NotifyNotification *brightness_notification;
        NotifyNotification *kb_backlight_notification;
};

static void     csd_media_keys_manager_class_init  (CsdMediaKeysManagerClass *klass);
static void     csd_media_keys_manager_init        (CsdMediaKeysManager      *media_keys_manager);
static void     csd_media_keys_manager_finalize    (GObject                  *object);
static void     register_manager                   (CsdMediaKeysManager      *manager);
static gboolean do_action (CsdMediaKeysManager *manager,
                           guint                deviceid,
                           MediaKeyType         type,
                           gint64               timestamp);

G_DEFINE_TYPE (CsdMediaKeysManager, csd_media_keys_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

#define NOTIFY_CAP_PRIVATE_SYNCHRONOUS "x-canonical-private-synchronous"
#define NOTIFY_CAP_PRIVATE_ICON_ONLY "x-canonical-private-icon-only"
#define NOTIFY_HINT_TRUE "true"

typedef struct {
        CsdMediaKeysManager *manager;
        MediaKeyType type;
        guint old_percentage;

} CsdBrightnessActionData;

static const char *volume_icons[] = {
        "notification-audio-volume-muted",
        "notification-audio-volume-low",
        "notification-audio-volume-medium",
        "notification-audio-volume-high",
        NULL
};

static const char *brightness_icons[] = {
        "notification-display-brightness-off",
	"notification-display-brightness-low",
	"notification-display-brightness-medium",
	"notification-display-brightness-high",
	"notification-display-brightness-full",
        NULL
};

static const char *kb_backlight_icons[] = {
        "notification-keyboard-brightness-off",
        "notification-keyboard-brightness-low",
        "notification-keyboard-brightness-medium",
        "notification-keyboard-brightness-high",
        "notification-keyboard-brightness-full",
        NULL
};

static const char *
calculate_icon_name (gint value, const char **icon_names)
{
        value = CLAMP (value, 0, 100);
        gint length = g_strv_length (icon_names);
        gint s = (length - 1) * value / 100 + 1;
        s = CLAMP (s, 1, length - 1);

        return icon_names[s];
}

static void
init_screens (CsdMediaKeysManager *manager)
{
        GdkDisplay *display;
        int i;

        display = gdk_display_get_default ();
        for (i = 0; i < gdk_display_get_n_screens (display); i++) {
                GdkScreen *screen;

                screen = gdk_display_get_screen (display, i);
                if (screen == NULL) {
                        continue;
                }
                manager->priv->screens = g_slist_append (manager->priv->screens, screen);
        }

        manager->priv->current_screen = manager->priv->screens->data;
}

static void
media_key_free (MediaKey *key)
{
        if (key == NULL)
                return;
        g_free (key->custom_path);
        g_free (key->custom_command);
        free_key (key->key);
        g_free (key);
}

static char *
get_term_command (CsdMediaKeysManager *manager)
{
        char *cmd_term, *cmd_args;;
        char *cmd = NULL;
        GSettings *settings;

        settings = g_settings_new ("org.cinnamon.desktop.default-applications.terminal");
        cmd_term = g_settings_get_string (settings, "exec");
        if (cmd_term[0] == '\0')
                cmd_term = g_strdup ("gnome-terminal");

        cmd_args = g_settings_get_string (settings, "exec-arg");
        if (strcmp (cmd_term, "") != 0) {
                cmd = g_strdup_printf ("%s %s -e", cmd_term, cmd_args);
        } else {
                cmd = g_strdup_printf ("%s -e", cmd_term);
        }

        g_free (cmd_args);
        g_free (cmd_term);
        g_object_unref (settings);

        return cmd;
}

static char **
get_keyring_env (CsdMediaKeysManager *manager)
{
	GError *error = NULL;
	GVariant *variant, *item;
	GVariantIter *iter;
	char **envp;

	variant = g_dbus_connection_call_sync (manager->priv->connection,
					       GNOME_KEYRING_DBUS_NAME,
					       GNOME_KEYRING_DBUS_PATH,
					       GNOME_KEYRING_DBUS_INTERFACE,
					       "GetEnvironment",
					       NULL,
					       NULL,
					       G_DBUS_CALL_FLAGS_NONE,
					       -1,
					       NULL,
					       &error);
	if (variant == NULL) {
		g_warning ("Failed to call GetEnvironment on keyring daemon: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	envp = g_get_environ ();

	g_variant_get (variant, "(a{ss})", &iter);

	while ((item = g_variant_iter_next_value (iter))) {
		char *key;
		char *value;

		g_variant_get (item,
			       "{ss}",
			       &key,
			       &value);

		envp = g_environ_setenv (envp, key, value, TRUE);

		g_variant_unref (item);
		g_free (key);
		g_free (value);
	}

	g_variant_iter_free (iter);
	g_variant_unref (variant);

	return envp;
}

static void
execute (CsdMediaKeysManager *manager,
         char                *cmd,
         gboolean             need_term)
{
        gboolean retval;
        char   **argv;
        int      argc;
        char    *exec;
        char    *term = NULL;
        GError  *error = NULL;

        retval = FALSE;

        if (need_term)
                term = get_term_command (manager);

        if (term) {
                exec = g_strdup_printf ("%s %s", term, cmd);
                g_free (term);
        } else {
                exec = g_strdup (cmd);
        }

        if (g_shell_parse_argv (exec, &argc, &argv, NULL)) {
		char   **envp;

		envp = get_keyring_env (manager);

                retval = g_spawn_async (g_get_home_dir (),
                                        argv,
                                        envp,
                                        G_SPAWN_SEARCH_PATH,
                                        NULL,
                                        NULL,
                                        NULL,
                                        &error);

                g_strfreev (argv);
                g_strfreev (envp);
        }

        if (retval == FALSE) {
                g_warning ("Couldn't execute command: %s: %s", exec, error->message);
                g_error_free (error);
        }
        g_free (exec);
}

static void
dialog_init (CsdMediaKeysManager *manager)
{
        if (manager->priv->dialog != NULL
            && !csd_osd_window_is_valid (CSD_OSD_WINDOW (manager->priv->dialog))) {
                gtk_widget_destroy (manager->priv->dialog);
                manager->priv->dialog = NULL;
        }

        if (manager->priv->dialog == NULL) {
                manager->priv->dialog = csd_osd_window_new ();
        }
}

static void
print_key_parse_error (MediaKey      *key,
		       const char    *str)
{
	if (str == NULL || *str == '\0')
		return;
	if (key->settings_key != NULL)
		g_debug ("Unable to parse key '%s' for GSettings entry '%s'", str, key->settings_key);
	else
		g_debug ("Unable to parse hard-coded key '%s'", key->hard_coded);
}

static char *
get_key_string (CsdMediaKeysManager *manager,
		MediaKey            *key)
{
	if (key->settings_key != NULL)
		return g_settings_get_string (manager->priv->settings, key->settings_key);
	else if (key->hard_coded != NULL)
		return g_strdup (key->hard_coded);
    else
		g_assert_not_reached ();
}

static gboolean
grab_media_key (MediaKey            *key,
		CsdMediaKeysManager *manager)
{
	char *tmp;
	gboolean need_flush;

	need_flush = FALSE;

	if (key->key != NULL) {
		need_flush = TRUE;
		ungrab_key_unsafe (key->key, manager->priv->screens);
	}

	free_key (key->key);
	key->key = NULL;

	tmp = get_key_string (manager, key);

	key->key = parse_key (tmp);
	if (key->key == NULL) {
		print_key_parse_error (key, tmp);
		g_free (tmp);
		return need_flush;
	}

	grab_key_unsafe (key->key, CSD_KEYGRAB_NORMAL, manager->priv->screens);

	g_free (tmp);

	return TRUE;
}

 static void
gsettings_changed_cb (GSettings           *settings,
                      const gchar         *settings_key,
                      CsdMediaKeysManager *manager)
{
        int      i;
        gboolean need_flush = TRUE;

        gdk_error_trap_push ();

        /* Find the key that was modified */
        for (i = 0; i < manager->priv->keys->len; i++) {
                MediaKey *key;

                key = g_ptr_array_index (manager->priv->keys, i);

                /* Skip over hard-coded and GConf keys */
                if (key->settings_key == NULL)
                        continue;
                if (strcmp (settings_key, key->settings_key) == 0) {
                        if (grab_media_key (key, manager))
                                need_flush = TRUE;
                        break;
                }
        }

        if (need_flush)
                gdk_flush ();
        if (gdk_error_trap_pop ())
                g_warning ("Grab failed for some keys, another application may already have access the them.");
}

static void
add_key (CsdMediaKeysManager *manager, guint i)
{
	MediaKey *key;

	key = g_new0 (MediaKey, 1);
	key->key_type = media_keys[i].key_type;
	key->settings_key = media_keys[i].settings_key;
	key->hard_coded = media_keys[i].hard_coded;

	g_ptr_array_add (manager->priv->keys, key);

	grab_media_key (key, manager);
}

static void
init_kbd (CsdMediaKeysManager *manager)
{
        int i;

        cinnamon_settings_profile_start (NULL);

        gdk_error_trap_push ();

        manager->priv->keys = g_ptr_array_new_with_free_func ((GDestroyNotify) media_key_free);

        /* Media keys
         * Add hard-coded shortcuts first so that they can't be preempted */
        for (i = 0; i < G_N_ELEMENTS (media_keys); i++) {
                if (media_keys[i].hard_coded)
                        add_key (manager, i);
        }
        for (i = 0; i < G_N_ELEMENTS (media_keys); i++) {
                if (media_keys[i].hard_coded == NULL)
                        add_key (manager, i);
        }

        gdk_flush ();
        if (gdk_error_trap_pop ())
                g_warning ("Grab failed for some keys, another application may already have access the them.");

        cinnamon_settings_profile_end (NULL);
}

static void
dialog_show (CsdMediaKeysManager *manager)
{
        int            orig_w;
        int            orig_h;
        int            screen_w;
        int            screen_h;
        int            x;
        int            y;
        GdkRectangle   geometry;
        int            monitor;

        gtk_window_set_screen (GTK_WINDOW (manager->priv->dialog),
                               manager->priv->current_screen);

        /*
         * get the window size
         * if the window hasn't been mapped, it doesn't necessarily
         * know its true size, yet, so we need to jump through hoops
         */
        gtk_window_get_default_size (GTK_WINDOW (manager->priv->dialog), &orig_w, &orig_h);

        monitor = gdk_screen_get_primary_monitor (manager->priv->current_screen);

        gdk_screen_get_monitor_geometry (manager->priv->current_screen,
                                         monitor,
                                         &geometry);

        screen_w = geometry.width;
        screen_h = geometry.height;

        x = ((screen_w - orig_w) / 2) + geometry.x;
        y = geometry.y + (screen_h / 2) + (screen_h / 2 - orig_h) / 2;

        gtk_window_move (GTK_WINDOW (manager->priv->dialog), x, y);

        gtk_widget_show (manager->priv->dialog);

        gdk_display_sync (gdk_screen_get_display (manager->priv->current_screen));
}

static void
launch_app (GAppInfo *app_info,
	    gint64    timestamp)
{
	GError *error = NULL;
        GdkAppLaunchContext *launch_context;

        /* setup the launch context so the startup notification is correct */
        launch_context = gdk_display_get_app_launch_context (gdk_display_get_default ());
        gdk_app_launch_context_set_timestamp (launch_context, timestamp);

	if (!g_app_info_launch (app_info, NULL, G_APP_LAUNCH_CONTEXT (launch_context), &error)) {
		g_warning ("Could not launch '%s': %s",
			   g_app_info_get_commandline (app_info),
			   error->message);
		g_error_free (error);
	}
        g_object_unref (launch_context);
}

static void
do_url_action (CsdMediaKeysManager *manager,
               const char          *scheme,
               gint64               timestamp)
{
        GAppInfo *app_info;

        app_info = g_app_info_get_default_for_uri_scheme (scheme);
        if (app_info != NULL) {
                launch_app (app_info, timestamp);
                g_object_unref (app_info);
        } else {
                g_warning ("Could not find default application for '%s' scheme", scheme);
	}
}

static void
do_media_action (CsdMediaKeysManager *manager,
		 gint64               timestamp)
{
        GAppInfo *app_info;

        app_info = g_app_info_get_default_for_type ("audio/x-vorbis+ogg", FALSE);
        if (app_info != NULL) {
                launch_app (app_info, timestamp);
                g_object_unref (app_info);
        } else {
                g_warning ("Could not find default application for '%s' mime-type", "audio/x-vorbis+ogg");
        }
}

static void
do_terminal_action (CsdMediaKeysManager *manager)
{
        GSettings *settings;
        char *term;

        settings = g_settings_new ("org.cinnamon.desktop.default-applications.terminal");
        term = g_settings_get_string (settings, "exec");

        if (term)
        execute (manager, term, FALSE);

        g_free (term);
        g_object_unref (settings);
}

static void
cinnamon_session_shutdown (CsdMediaKeysManager *manager)
{
	GError *error = NULL;
	GVariant *variant;

	/* Shouldn't happen, but you never know */
	if (manager->priv->connection == NULL) {
		execute (manager, "cinnamon-session-quit --logout", FALSE);
		return;
	}

	variant = g_dbus_connection_call_sync (manager->priv->connection,
					       GNOME_SESSION_DBUS_NAME,
					       GNOME_SESSION_DBUS_PATH,
					       GNOME_SESSION_DBUS_INTERFACE,
					       "Shutdown",
					       NULL,
					       NULL,
					       G_DBUS_CALL_FLAGS_NONE,
					       -1,
					       NULL,
					       &error);
	if (variant == NULL) {
		g_warning ("Failed to call Shutdown on session manager: %s", error->message);
		g_error_free (error);
		return;
	}
	g_variant_unref (variant);
}

static void
do_logout_action (CsdMediaKeysManager *manager)
{
        execute (manager, "cinnamon-session-quit --logout", FALSE);
}

static void
do_eject_action_cb (GDrive              *drive,
                    GAsyncResult        *res,
                    CsdMediaKeysManager *manager)
{
        g_drive_eject_with_operation_finish (drive, res, NULL);
}

#define NO_SCORE 0
#define SCORE_CAN_EJECT 50
#define SCORE_HAS_MEDIA 100
static void
do_eject_action (CsdMediaKeysManager *manager)
{
        GList *drives, *l;
        GDrive *fav_drive;
        guint score;
        GVolumeMonitor *volume_monitor;

        volume_monitor = g_volume_monitor_get ();


        /* Find the best drive to eject */
        fav_drive = NULL;
        score = NO_SCORE;
        drives = g_volume_monitor_get_connected_drives (volume_monitor);
        for (l = drives; l != NULL; l = l->next) {
                GDrive *drive = l->data;

                if (g_drive_can_eject (drive) == FALSE)
                        continue;
                if (g_drive_is_media_removable (drive) == FALSE)
                        continue;
                if (score < SCORE_CAN_EJECT) {
                        fav_drive = drive;
                        score = SCORE_CAN_EJECT;
                }
                if (g_drive_has_media (drive) == FALSE)
                        continue;
                if (score < SCORE_HAS_MEDIA) {
                        fav_drive = drive;
                        score = SCORE_HAS_MEDIA;
                        break;
                }
        }

        /* Show the dialogue */
        dialog_init (manager);
        csd_osd_window_set_action_custom (CSD_OSD_WINDOW (manager->priv->dialog),
                                                 "media-eject-symbolic",
                                                 FALSE);
        dialog_show (manager);

        /* Clean up the drive selection and exit if no suitable
         * drives are found */
        if (fav_drive != NULL)
                fav_drive = g_object_ref (fav_drive);

        g_list_foreach (drives, (GFunc) g_object_unref, NULL);
        if (fav_drive == NULL)
                return;

        /* Eject! */
        g_drive_eject_with_operation (fav_drive, G_MOUNT_UNMOUNT_FORCE,
                                      NULL, NULL,
                                      (GAsyncReadyCallback) do_eject_action_cb,
                                      manager);
        g_object_unref (fav_drive);
        g_object_unref (volume_monitor);
}

static void
do_home_key_action (CsdMediaKeysManager *manager,
		    gint64               timestamp)
{
	GFile *file;
	GError *error = NULL;
	char *uri;

	file = g_file_new_for_path (g_get_home_dir ());
	uri = g_file_get_uri (file);
	g_object_unref (file);

	if (gtk_show_uri (NULL, uri, timestamp, &error) == FALSE) {
		g_warning ("Failed to launch '%s': %s", uri, error->message);
		g_error_free (error);
	}
	g_free (uri);
}

static void
do_execute_desktop (CsdMediaKeysManager *manager,
		    const char          *desktop,
		    gint64               timestamp)
{
        GDesktopAppInfo *app_info;

        app_info = g_desktop_app_info_new (desktop);
        if (app_info != NULL) {
                launch_app (G_APP_INFO (app_info), timestamp);
                g_object_unref (app_info);
        } else {
                g_warning ("Could not find application '%s'", desktop);
	}
}

static void
do_touchpad_osd_action (CsdMediaKeysManager *manager, gboolean state)
{
    dialog_init (manager);
    csd_osd_window_set_action_custom (CSD_OSD_WINDOW (manager->priv->dialog),
                                            state ? "input-touchpad-symbolic" : "touchpad-disabled-symbolic",
                                            FALSE);
    dialog_show (manager);
}

static void
do_touchpad_action (CsdMediaKeysManager *manager)
{
        GSettings *settings;
        gboolean state;

        if (touchpad_is_present () == FALSE) {
                do_touchpad_osd_action (manager, FALSE);
                return;
        }

        settings = g_settings_new (SETTINGS_TOUCHPAD_DIR);
        state = g_settings_get_boolean (settings, TOUCHPAD_ENABLED_KEY);

        do_touchpad_osd_action (manager, !state);

        g_settings_set_boolean (settings, TOUCHPAD_ENABLED_KEY, !state);
        g_object_unref (settings);
}

static void
update_dialog (CsdMediaKeysManager *manager,
               GvcMixerStream      *stream,
               gint                 vol,
               gboolean             muted,
               gboolean             sound_changed,
               gboolean             quiet)
{    
        vol = CLAMP (vol, 0, 100);

        dialog_init (manager);
        csd_osd_window_set_volume_muted (CSD_OSD_WINDOW (manager->priv->dialog),
                                                muted);
        csd_osd_window_set_volume_level (CSD_OSD_WINDOW (manager->priv->dialog), vol);
        csd_osd_window_set_action (CSD_OSD_WINDOW (manager->priv->dialog),
                                          CSD_OSD_WINDOW_ACTION_VOLUME);
        dialog_show (manager);

done:
        if (quiet == FALSE && sound_changed != FALSE && muted == FALSE) {                            
                GSettings *settings = g_settings_new ("org.cinnamon.desktop.sound");
                gboolean enabled = g_settings_get_boolean (settings, "volume-sound-enabled");
                char *sound = g_settings_get_string (settings, "volume-sound-file");                
                if (enabled) {
                    ca_context_change_device (manager->priv->ca, gvc_mixer_stream_get_name (stream));
                    ca_context_play (manager->priv->ca, 1, CA_PROP_MEDIA_FILENAME, sound, NULL);
                }
                g_free(sound);
                g_object_unref (settings);
        }
}

#ifdef HAVE_GUDEV
/* PulseAudio gives us /devices/... paths, when udev
 * expects /sys/devices/... paths. */
static GUdevDevice *
get_udev_device_for_sysfs_path (CsdMediaKeysManager *manager,
				const char *sysfs_path)
{
	char *path;
	GUdevDevice *dev;

	path = g_strdup_printf ("/sys%s", sysfs_path);
	dev = g_udev_client_query_by_sysfs_path (manager->priv->udev_client, path);
	g_free (path);

	return dev;
}

static GvcMixerStream *
get_stream_for_device_id (CsdMediaKeysManager *manager,
			  guint                deviceid)
{
	char *devnode;
	gpointer id_ptr;
	GvcMixerStream *res;
	GUdevDevice *dev, *parent;
	GSList *sinks, *l;

	id_ptr = g_hash_table_lookup (manager->priv->streams, GUINT_TO_POINTER (deviceid));
	if (id_ptr != NULL) {
		if (GPOINTER_TO_UINT (id_ptr) == (guint) -1)
			return NULL;
		else
			return gvc_mixer_control_lookup_stream_id (manager->priv->volume, GPOINTER_TO_UINT (id_ptr));
	}

	devnode = xdevice_get_device_node (deviceid);
	if (devnode == NULL) {
		g_debug ("Could not find device node for XInput device %d", deviceid);
		return NULL;
	}

	dev = g_udev_client_query_by_device_file (manager->priv->udev_client, devnode);
	if (dev == NULL) {
		g_debug ("Could not find udev device for device path '%s'", devnode);
		g_free (devnode);
		return NULL;
	}
	g_free (devnode);

	if (g_strcmp0 (g_udev_device_get_property (dev, "ID_BUS"), "usb") != 0) {
		g_debug ("Not handling XInput device %d, not USB", deviceid);
		g_hash_table_insert (manager->priv->streams,
				     GUINT_TO_POINTER (deviceid),
				     GUINT_TO_POINTER ((guint) -1));
		g_object_unref (dev);
		return NULL;
	}

	parent = g_udev_device_get_parent_with_subsystem (dev, "usb", "usb_device");
	if (parent == NULL) {
		g_warning ("No USB device parent for XInput device %d even though it's USB", deviceid);
		g_object_unref (dev);
		return NULL;
	}

	res = NULL;
	sinks = gvc_mixer_control_get_sinks (manager->priv->volume);
	for (l = sinks; l; l = l->next) {
		GvcMixerStream *stream = l->data;
		const char *sysfs_path;
		GUdevDevice *sink_dev, *sink_parent;

		sysfs_path = gvc_mixer_stream_get_sysfs_path (stream);
		sink_dev = get_udev_device_for_sysfs_path (manager, sysfs_path);
		if (sink_dev == NULL)
			continue;
		sink_parent = g_udev_device_get_parent_with_subsystem (sink_dev, "usb", "usb_device");
		g_object_unref (sink_dev);
		if (sink_parent == NULL)
			continue;

		if (g_strcmp0 (g_udev_device_get_sysfs_path (sink_parent),
			       g_udev_device_get_sysfs_path (parent)) == 0) {
			res = stream;
		}
		g_object_unref (sink_parent);
		if (res != NULL)
			break;
	}

	if (res)
		g_hash_table_insert (manager->priv->streams,
				     GUINT_TO_POINTER (deviceid),
				     GUINT_TO_POINTER (gvc_mixer_stream_get_id (res)));
	else
		g_hash_table_insert (manager->priv->streams,
				     GUINT_TO_POINTER (deviceid),
				     GUINT_TO_POINTER ((guint) -1));

	return res;
}
#endif /* HAVE_GUDEV */

static void
do_sound_action (CsdMediaKeysManager *manager,
		 guint                deviceid,
                 int                  type,
                 gboolean             quiet)
{
	GvcMixerStream *stream;
        gboolean old_muted, new_muted;
        guint old_vol, new_vol, norm_vol_step, osd_vol;
        gboolean sound_changed;

        /* Find the stream that corresponds to the device, if any */
#ifdef HAVE_GUDEV
        stream = get_stream_for_device_id (manager, deviceid);
        if (stream == NULL)
#endif /* HAVE_GUDEV */
                stream = manager->priv->stream;
        if (stream == NULL)
                return;

        norm_vol_step = PA_VOLUME_NORM * VOLUME_STEP / 100;

        /* FIXME: this is racy */
        new_vol = old_vol = gvc_mixer_stream_get_volume (stream);
        new_muted = old_muted = gvc_mixer_stream_get_is_muted (stream);
        sound_changed = FALSE;

        switch (type) {
        case MUTE_KEY:
                new_muted = !old_muted;
                break;
        case VOLUME_DOWN_KEY:
                if (old_vol <= norm_vol_step) {
                        new_vol = 0;
                        new_muted = TRUE;
                } else {
                        new_vol = old_vol - norm_vol_step;
                }
                break;
        case VOLUME_UP_KEY:
                new_muted = FALSE;
                /* When coming out of mute only increase the volume if it was 0 */
                if (!old_muted || old_vol == 0)
                        new_vol = MIN (old_vol + norm_vol_step, MAX_VOLUME);
                break;
        }

        if (old_muted != new_muted) {
                gvc_mixer_stream_change_is_muted (stream, new_muted);
                sound_changed = TRUE;
        }

        if (old_vol != new_vol) {
                if (gvc_mixer_stream_set_volume (stream, new_vol) != FALSE) {
                        gvc_mixer_stream_push_volume (stream);
                        sound_changed = TRUE;
                }
        }

        if (type == VOLUME_DOWN_KEY && old_vol == 0 && old_muted)
                osd_vol = -1;
        else if (type == VOLUME_UP_KEY && old_vol == PA_VOLUME_NORM && !old_muted)
                osd_vol = 101;
        else if (!new_muted)
                osd_vol = (int) (100 * (double) new_vol / PA_VOLUME_NORM);
        else
                osd_vol = 0;
        update_dialog (manager, stream, osd_vol, new_muted, sound_changed, quiet);
}

// static void
// sound_theme_changed (GtkSettings         *settings,
//                      GParamSpec          *pspec,
//                      CsdMediaKeysManager *manager)
// {
//         char *theme_name;

//         g_object_get (G_OBJECT (manager->priv->gtksettings), "gtk-sound-theme-name", &theme_name, NULL);
//         if (theme_name)
//                 ca_context_change_props (manager->priv->ca, CA_PROP_CANBERRA_XDG_THEME_NAME, theme_name, NULL);
//         g_free (theme_name);
// }

static void
update_default_sink (CsdMediaKeysManager *manager)
{
        GvcMixerStream *stream;

        stream = gvc_mixer_control_get_default_sink (manager->priv->volume);
        if (stream == manager->priv->stream)
                return;

        if (manager->priv->stream != NULL) {
                g_object_unref (manager->priv->stream);
                manager->priv->stream = NULL;
        }

        if (stream != NULL) {
                manager->priv->stream = g_object_ref (stream);
        } else {
                g_warning ("Unable to get default sink");
        }
}

static void
on_control_state_changed (GvcMixerControl     *control,
                          GvcMixerControlState new_state,
                          CsdMediaKeysManager *manager)
{
        update_default_sink (manager);
}

static void
on_control_default_sink_changed (GvcMixerControl     *control,
                                 guint                id,
                                 CsdMediaKeysManager *manager)
{
        update_default_sink (manager);
}

#ifdef HAVE_GUDEV
static gboolean
remove_stream (gpointer key,
	       gpointer value,
	       gpointer id)
{
	if (GPOINTER_TO_UINT (value) == GPOINTER_TO_UINT (id))
		return TRUE;
	return FALSE;
}
#endif /* HAVE_GUDEV */

static void
on_control_stream_removed (GvcMixerControl     *control,
                           guint                id,
                           CsdMediaKeysManager *manager)
{
        if (manager->priv->stream != NULL) {
		if (gvc_mixer_stream_get_id (manager->priv->stream) == id) {
	                g_object_unref (manager->priv->stream);
			manager->priv->stream = NULL;
		}
        }

#ifdef HAVE_GUDEV
	g_hash_table_foreach_remove (manager->priv->streams, (GHRFunc) remove_stream, GUINT_TO_POINTER (id));
#endif
}

static void
free_media_player (MediaPlayer *player)
{
        if (player->watch_id > 0) {
                g_bus_unwatch_name (player->watch_id);
                player->watch_id = 0;
        }
        g_free (player->application);
        g_free (player->name);
        g_free (player);
}

static gint
find_by_application (gconstpointer a,
                     gconstpointer b)
{
        return strcmp (((MediaPlayer *)a)->application, b);
}

static gint
find_by_name (gconstpointer a,
              gconstpointer b)
{
        return strcmp (((MediaPlayer *)a)->name, b);
}

static gint
find_by_time (gconstpointer a,
              gconstpointer b)
{
        return ((MediaPlayer *)a)->time < ((MediaPlayer *)b)->time;
}

static void
name_vanished_handler (GDBusConnection     *connection,
                       const gchar         *name,
                       CsdMediaKeysManager *manager)
{
        GList *iter;

        iter = g_list_find_custom (manager->priv->media_players,
                                   name,
                                   find_by_name);

        if (iter != NULL) {
                MediaPlayer *player;

                player = iter->data;
                g_debug ("Deregistering vanished %s (name: %s)", player->application, player->name);
                free_media_player (player);
                manager->priv->media_players = g_list_delete_link (manager->priv->media_players, iter);
        }
}

/*
 * Register a new media player. Most applications will want to call
 * this with time = GDK_CURRENT_TIME. This way, the last registered
 * player will receive media events. In some cases, applications
 * may want to register with a lower priority (usually 1), to grab
 * events only nobody is interested.
 */
static void
csd_media_keys_manager_grab_media_player_keys (CsdMediaKeysManager *manager,
                                               const char          *application,
                                               const char          *name,
                                               guint32              time)
{
        GList       *iter;
        MediaPlayer *media_player;
        guint        watch_id;

        if (time == GDK_CURRENT_TIME) {
                GTimeVal tv;

                g_get_current_time (&tv);
                time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        }

        iter = g_list_find_custom (manager->priv->media_players,
                                   application,
                                   find_by_application);

        if (iter != NULL) {
                if (((MediaPlayer *)iter->data)->time < time) {
                        MediaPlayer *player = iter->data;
                        free_media_player (player);
                        manager->priv->media_players = g_list_delete_link (manager->priv->media_players, iter);
                } else {
                        return;
                }
        }

        watch_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                     name,
                                     G_BUS_NAME_WATCHER_FLAGS_NONE,
                                     NULL,
                                     (GBusNameVanishedCallback) name_vanished_handler,
                                     manager,
                                     NULL);

        g_debug ("Registering %s at %u", application, time);
        media_player = g_new0 (MediaPlayer, 1);
        media_player->application = g_strdup (application);
        media_player->name = g_strdup (name);
        media_player->time = time;
        media_player->watch_id = watch_id;

        manager->priv->media_players = g_list_insert_sorted (manager->priv->media_players,
                                                             media_player,
                                                             find_by_time);
}

static void
csd_media_keys_manager_release_media_player_keys (CsdMediaKeysManager *manager,
                                                  const char          *application,
                                                  const char          *name)
{
        GList *iter = NULL;

        g_return_if_fail (application != NULL || name != NULL);

        if (application != NULL) {
                iter = g_list_find_custom (manager->priv->media_players,
                                           application,
                                           find_by_application);
        }

        if (iter == NULL && name != NULL) {
                iter = g_list_find_custom (manager->priv->media_players,
                                           name,
                                           find_by_name);
        }

        if (iter != NULL) {
                MediaPlayer *player;

                player = iter->data;
                g_debug ("Deregistering %s (name: %s)", application, player->name);
                free_media_player (player);
                manager->priv->media_players = g_list_delete_link (manager->priv->media_players, iter);
        }
}

static gboolean
csd_media_player_key_pressed (CsdMediaKeysManager *manager,
                              const char          *key)
{
        const char  *application;
        gboolean     have_listeners;
        GError      *error = NULL;
        MediaPlayer *player;

        g_return_val_if_fail (key != NULL, FALSE);

        g_debug ("Media key '%s' pressed", key);

        have_listeners = (manager->priv->media_players != NULL);

        if (!have_listeners) {
                /* Popup a dialog with an (/) icon */
                dialog_init (manager);
                csd_osd_window_set_action_custom (CSD_OSD_WINDOW (manager->priv->dialog),
                                                         "action-unavailable-symbolic",
                                                         FALSE);
                dialog_show (manager);
                return TRUE;
        }

        player = manager->priv->media_players->data;
        application = player->application;

        if (g_dbus_connection_emit_signal (manager->priv->connection,
                                           player->name,
                                           CSD_MEDIA_KEYS_DBUS_PATH,
                                           CSD_MEDIA_KEYS_DBUS_NAME,
                                           "MediaPlayerKeyPressed",
                                           g_variant_new ("(ss)", application ? application : "", key),
                                           &error) == FALSE) {
                g_debug ("Error emitting signal: %s", error->message);
                g_error_free (error);
        }

        return !have_listeners;
}

static void
csd_media_keys_manager_handle_cinnamon_keybinding (CsdMediaKeysManager *manager,
                                                   guint                deviceid,
                                                   MediaKeyType         type,
                                                   gint64               timestamp)
{
    do_action (manager, NULL, type, timestamp);
}

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
        CsdMediaKeysManager *manager = (CsdMediaKeysManager *) user_data;

        g_debug ("Calling method '%s' for media-keys", method_name);

        if (g_strcmp0 (method_name, "ReleaseMediaPlayerKeys") == 0) {
                const char *app_name;

                g_variant_get (parameters, "(&s)", &app_name);
                csd_media_keys_manager_release_media_player_keys (manager, app_name, sender);
                g_dbus_method_invocation_return_value (invocation, NULL);
        } else if (g_strcmp0 (method_name, "GrabMediaPlayerKeys") == 0) {
                const char *app_name;
                guint32 time;

                g_variant_get (parameters, "(&su)", &app_name, &time);
                csd_media_keys_manager_grab_media_player_keys (manager, app_name, sender, time);
                g_dbus_method_invocation_return_value (invocation, NULL);
        } else if (g_strcmp0 (method_name, "HandleKeybinding") == 0) {
                MediaKeyType action;
                g_variant_get (parameters, "(u)", &action);
                csd_media_keys_manager_handle_cinnamon_keybinding (manager, NULL, action, CurrentTime);
                g_dbus_method_invocation_return_value (invocation, NULL);
        }
}

static const GDBusInterfaceVTable interface_vtable =
{
        handle_method_call,
        NULL, /* Get Property */
        NULL, /* Set Property */
};

static gboolean
do_multimedia_player_action (CsdMediaKeysManager *manager,
                             const char          *icon,
                             const char          *key)
{
        return csd_media_player_key_pressed (manager, key);
}

static void
on_xrandr_action_call_finished (GObject             *source_object,
                                GAsyncResult        *res,
                                CsdMediaKeysManager *manager)
{
        GError *error = NULL;
        GVariant *variant;
        char *action;

        action = g_object_get_data (G_OBJECT (source_object),
                                    "csd-media-keys-manager-xrandr-action");

        variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);

        g_object_unref (manager->priv->cancellable);
        manager->priv->cancellable = NULL;

        if (error != NULL) {
                g_warning ("Unable to call '%s': %s", action, error->message);
                g_error_free (error);
        } else {
                g_variant_unref (variant);
        }

        g_free (action);
}

static void
do_xrandr_action (CsdMediaKeysManager *manager,
                  const char          *action,
                  gint64               timestamp)
{
        CsdMediaKeysManagerPrivate *priv = manager->priv;

        if (priv->connection == NULL || priv->xrandr_proxy == NULL) {
                g_warning ("No existing D-Bus connection trying to handle XRANDR keys");
                return;
        }

        if (priv->cancellable != NULL) {
                g_debug ("xrandr action already in flight");
                return;
        }

        priv->cancellable = g_cancellable_new ();

        g_object_set_data (G_OBJECT (priv->xrandr_proxy),
                           "csd-media-keys-manager-xrandr-action",
                           g_strdup (action));

        g_dbus_proxy_call (priv->xrandr_proxy,
                           action,
                           g_variant_new ("(x)", timestamp),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           priv->cancellable,
                           (GAsyncReadyCallback) on_xrandr_action_call_finished,
                           manager);
}

static gboolean
do_video_out_action (CsdMediaKeysManager *manager,
                     gint64               timestamp)
{
        do_xrandr_action (manager, "VideoModeSwitch", timestamp);
        return FALSE;
}

static gboolean
do_video_rotate_action (CsdMediaKeysManager *manager,
                        gint64               timestamp)
{
        do_xrandr_action (manager, "Rotate", timestamp);
        return FALSE;
}

static void
do_toggle_accessibility_key (const char *key)
{
        GSettings *settings;
        gboolean state;

        settings = g_settings_new ("org.cinnamon.desktop.a11y.applications");
        state = g_settings_get_boolean (settings, key);
        g_settings_set_boolean (settings, key, !state);
        g_object_unref (settings);
}

static void
do_magnifier_action (CsdMediaKeysManager *manager)
{
        do_toggle_accessibility_key ("screen-magnifier-enabled");
}

static void
do_screenreader_action (CsdMediaKeysManager *manager)
{
        do_toggle_accessibility_key ("screen-reader-enabled");
}

static void
do_on_screen_keyboard_action (CsdMediaKeysManager *manager)
{
        do_toggle_accessibility_key ("screen-keyboard-enabled");
}

static void
do_text_size_action (CsdMediaKeysManager *manager,
		     MediaKeyType         type)
{
	gdouble factor, best, distance;
	guint i;

	/* Same values used in the Seeing tab of the Universal Access panel */
	static gdouble factors[] = {
		0.75,
		1.0,
		1.25,
		1.5
	};

	/* Figure out the current DPI scaling factor */
	factor = g_settings_get_double (manager->priv->interface_settings, "text-scaling-factor");
	factor += (type == INCREASE_TEXT_KEY ? 0.25 : -0.25);

	/* Try to find a matching value */
	distance = 1e6;
	best = 1.0;
	for (i = 0; i < G_N_ELEMENTS(factors); i++) {
		gdouble d;
		d = fabs (factor - factors[i]);
		if (d < distance) {
			best = factors[i];
			distance = d;
		}
	}

	if (best == 1.0)
		g_settings_reset (manager->priv->interface_settings, "text-scaling-factor");
	else
		g_settings_set_double (manager->priv->interface_settings, "text-scaling-factor", best);
}

static void
do_magnifier_zoom_action (CsdMediaKeysManager *manager,
			  MediaKeyType         type)
{
	GSettings *settings;
	gdouble offset, value;

	if (type == MAGNIFIER_ZOOM_IN_KEY)
		offset = 1.0;
	else
		offset = -1.0;

	settings = g_settings_new ("org.cinnamon.desktop.a11y.magnifier");
	value = g_settings_get_double (settings, "mag-factor");
	value += offset;
	value = roundl (value);
	g_settings_set_double (settings, "mag-factor", value);
	g_object_unref (settings);
}

static void
do_toggle_contrast_action (CsdMediaKeysManager *manager)
{
	gboolean high_contrast;
	char *theme;

	/* Are we using HighContrast now? */
	theme = g_settings_get_string (manager->priv->interface_settings, "gtk-theme");
	high_contrast = g_str_equal (theme, HIGH_CONTRAST);
	g_free (theme);

	if (high_contrast != FALSE) {
		if (manager->priv->gtk_theme == NULL)
			g_settings_reset (manager->priv->interface_settings, "gtk-theme");
		else
			g_settings_set (manager->priv->interface_settings, "gtk-theme", manager->priv->gtk_theme);
		g_settings_set (manager->priv->interface_settings, "icon-theme", manager->priv->icon_theme);
	} else {
		g_settings_set (manager->priv->interface_settings, "gtk-theme", HIGH_CONTRAST);
		g_settings_set (manager->priv->interface_settings, "icon-theme", HIGH_CONTRAST);
	}
}

static void
do_config_power_action (CsdMediaKeysManager *manager,
                        const gchar *config_key)
{
        CsdPowerActionType action_type;

        action_type = g_settings_get_enum (manager->priv->power_settings,
                                           config_key);
        switch (action_type) {
        case CSD_POWER_ACTION_SUSPEND:
                csd_power_suspend (manager->priv->upower_proxy);
                break;
        case CSD_POWER_ACTION_INTERACTIVE:
                cinnamon_session_shutdown (manager);
                break;
        case CSD_POWER_ACTION_SHUTDOWN:
                execute (manager, "cinnamon-session-quit --power-off --no-prompt", FALSE);
                break;
        case CSD_POWER_ACTION_HIBERNATE:
                csd_power_hibernate (manager->priv->upower_proxy);
                break;
        case CSD_POWER_ACTION_BLANK:
                execute (manager, "cinnamon-screensaver-command --lock", FALSE);
                break;
        case CSD_POWER_ACTION_NOTHING:
                /* these actions cannot be handled by media-keys and
                 * are not used in this context */
                break;
        }
}

static void
update_screen_cb (GObject             *source_object,
                  GAsyncResult        *res,
                  gpointer             user_data)
{
        GError *error = NULL;
        guint percentage;
        GVariant *new_percentage;
        CsdBrightnessActionData *data = (CsdBrightnessActionData *) user_data;
        CsdMediaKeysManager *manager = data->manager;

        new_percentage = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                                   res, &error);
        if (new_percentage == NULL) {
                g_warning ("Failed to set new screen percentage: %s",
                           error->message);
                g_error_free (error);
                g_free (data);
                return;
        }

        /* update the dialog with the new value */
        g_variant_get (new_percentage, "(u)", &percentage);
        guint osd_percentage;

        if (data->old_percentage == 100 && data->type == SCREEN_BRIGHTNESS_UP_KEY)
                osd_percentage = 101;
        else if (data->old_percentage == 0 && data->type == SCREEN_BRIGHTNESS_DOWN_KEY)
                osd_percentage = -1;
        else
                osd_percentage = CLAMP (percentage, 0, 100);

        dialog_init (manager);
        csd_osd_window_set_action_custom (CSD_OSD_WINDOW (manager->priv->dialog),
                                                 "display-brightness-symbolic",
                                                 TRUE);
        csd_osd_window_set_volume_level (CSD_OSD_WINDOW (manager->priv->dialog),
                                                percentage);
        dialog_show (manager);

        g_free (data);
        g_variant_unref (new_percentage);
}

static void
do_screen_brightness_action_real (GObject       *source_object,
                                  GAsyncResult  *res,
                                  gpointer       user_data)
{
        CsdBrightnessActionData *data = (CsdBrightnessActionData *) user_data;
        CsdMediaKeysManager *manager = data->manager;
        GError *error = NULL;

        GVariant *old_percentage = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                                             res, &error);
        if (old_percentage == NULL) {
                g_warning ("Failed to get old screen percentage: %s", error->message);
                g_error_free (error);
                g_free (data);
                return;
        }

        g_variant_get (old_percentage, "(u)", &data->old_percentage);

        /* call into the power plugin */
        g_dbus_proxy_call (manager->priv->power_screen_proxy,
                           data->type == SCREEN_BRIGHTNESS_UP_KEY ? "StepUp" : "StepDown",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           update_screen_cb,
                           data);

        g_variant_unref (old_percentage);
}

static void
do_screen_brightness_action (CsdMediaKeysManager *manager,
                             MediaKeyType type)
{
        if (manager->priv->connection == NULL ||
            manager->priv->power_screen_proxy == NULL) {
                g_warning ("No existing D-Bus connection trying to handle power keys");
                return;
        }

        CsdBrightnessActionData *data = g_new0 (CsdBrightnessActionData, 1);
        data->manager = manager;
        data->type = type;

        g_dbus_proxy_call (manager->priv->power_screen_proxy,
                           "GetPercentage",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           do_screen_brightness_action_real,
                           data);
}

static void
update_keyboard_cb (GObject             *source_object,
                    GAsyncResult        *res,
                    gpointer             user_data)
{
        GError *error = NULL;
        guint percentage;
        GVariant *new_percentage;
        CsdMediaKeysManager *manager = CSD_MEDIA_KEYS_MANAGER (user_data);

        new_percentage = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                                   res, &error);
        if (new_percentage == NULL) {
                g_warning ("Failed to set new keyboard percentage: %s",
                           error->message);
                g_error_free (error);
                return;
        }

        /* update the dialog with the new value */
        g_variant_get (new_percentage, "(u)", &percentage);

        /* FIXME: No overshoot effect for keyboard, as the power plugin has no interface
         *        to get the old brightness */
        dialog_init (manager);
        csd_osd_window_set_action_custom (CSD_OSD_WINDOW (manager->priv->dialog),
                                                 "keyboard-brightness-symbolic",
                                                 TRUE);
        csd_osd_window_set_volume_level (CSD_OSD_WINDOW (manager->priv->dialog),
                                                percentage);
        dialog_show (manager);

        g_variant_unref (new_percentage);
}

static void
do_keyboard_brightness_action (CsdMediaKeysManager *manager,
                               MediaKeyType type)
{
        const char *cmd;

        if (manager->priv->connection == NULL ||
            manager->priv->power_keyboard_proxy == NULL) {
                g_warning ("No existing D-Bus connection trying to handle power keys");
                return;
        }

        switch (type) {
        case KEYBOARD_BRIGHTNESS_UP_KEY:
                cmd = "StepUp";
                break;
        case KEYBOARD_BRIGHTNESS_DOWN_KEY:
                cmd = "StepDown";
                break;
        case KEYBOARD_BRIGHTNESS_TOGGLE_KEY:
                cmd = "Toggle";
                break;
        default:
                g_assert_not_reached ();
        }

        /* call into the power plugin */
        g_dbus_proxy_call (manager->priv->power_keyboard_proxy,
                           cmd,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           update_keyboard_cb,
                           manager);
}

static gboolean
do_action (CsdMediaKeysManager *manager,
           guint                deviceid,
           MediaKeyType         type,
           gint64               timestamp)
{
        char *cmd;

        g_debug ("Launching action for key type '%d' (on device id %d)", type, deviceid);

        switch (type) {
        case TOUCHPAD_KEY:
                do_touchpad_action (manager);
                break;
        case TOUCHPAD_ON_KEY:
                do_touchpad_osd_action (manager, TRUE);
                break;
        case TOUCHPAD_OFF_KEY:
                do_touchpad_osd_action (manager, FALSE);
                break;
        case MUTE_KEY:
        case VOLUME_DOWN_KEY:
        case VOLUME_UP_KEY:
                do_sound_action (manager, deviceid, type, FALSE);
                break;
        case MUTE_QUIET_KEY:
                do_sound_action (manager, deviceid, MUTE_KEY, TRUE);
                break;
        case VOLUME_DOWN_QUIET_KEY:
                do_sound_action (manager, deviceid, VOLUME_DOWN_KEY, TRUE);
                break;
        case VOLUME_UP_QUIET_KEY:
                do_sound_action (manager, deviceid, VOLUME_UP_KEY, TRUE);
                break;
        case LOGOUT_KEY:
                do_logout_action (manager);
                break;
        case EJECT_KEY:
                do_eject_action (manager);
                break;
        case HOME_KEY:
                do_home_key_action (manager, timestamp);
                break;
        case SEARCH_KEY:
                cmd = NULL;
                if ((cmd = g_find_program_in_path ("tracker-search-tool")))
                        do_execute_desktop (manager, "tracker-needle.desktop", timestamp);
                else
                        do_execute_desktop (manager, "gnome-search-tool.desktop", timestamp);
                g_free (cmd);
                break;
        case EMAIL_KEY:
                do_url_action (manager, "mailto", timestamp);
                break;
        case SCREENSAVER_KEY:
                execute (manager, "cinnamon-screensaver-command --lock", FALSE);
                break;
        case HELP_KEY:
                do_url_action (manager, "ghelp", timestamp);
                break;
        case SCREENSHOT_KEY:
                execute (manager, "gnome-screenshot", FALSE);
                break;
        case WINDOW_SCREENSHOT_KEY:
                execute (manager, "gnome-screenshot --window", FALSE);
                break;
        case AREA_SCREENSHOT_KEY:
                execute (manager, "gnome-screenshot --area", FALSE);
                break;
        case SCREENSHOT_CLIP_KEY:
                execute (manager, "gnome-screenshot --clipboard", FALSE);
                break;
        case WINDOW_SCREENSHOT_CLIP_KEY:
                execute (manager, "gnome-screenshot --window --clipboard", FALSE);
                break;
        case AREA_SCREENSHOT_CLIP_KEY:
                execute (manager, "gnome-screenshot --area --clipboard", FALSE);
                break;
        case TERMINAL_KEY:
                do_terminal_action (manager);
                break;
        case WWW_KEY:
                do_url_action (manager, "http", timestamp);
                break;
        case MEDIA_KEY:
                do_media_action (manager, timestamp);
                break;
        case CALCULATOR_KEY:
                do_execute_desktop (manager, "gcalctool.desktop", timestamp);
                break;
        case PLAY_KEY:
                return do_multimedia_player_action (manager, NULL, "Play");
        case PAUSE_KEY:
                return do_multimedia_player_action (manager, NULL, "Pause");
        case STOP_KEY:
                return do_multimedia_player_action (manager, NULL, "Stop");
        case PREVIOUS_KEY:
                return do_multimedia_player_action (manager, NULL, "Previous");
        case NEXT_KEY:
                return do_multimedia_player_action (manager, NULL, "Next");
        case REWIND_KEY:
                return do_multimedia_player_action (manager, NULL, "Rewind");
        case FORWARD_KEY:
                return do_multimedia_player_action (manager, NULL, "FastForward");
        case REPEAT_KEY:
                return do_multimedia_player_action (manager, NULL, "Repeat");
        case RANDOM_KEY:
                return do_multimedia_player_action (manager, NULL, "Shuffle");
        case VIDEO_OUT_KEY:
                do_video_out_action (manager, timestamp);
                break;
        case ROTATE_VIDEO_KEY:
                do_video_rotate_action (manager, timestamp);
                break;
        case MAGNIFIER_KEY:
                do_magnifier_action (manager);
                break;
        case SCREENREADER_KEY:
                do_screenreader_action (manager);
                break;
        case ON_SCREEN_KEYBOARD_KEY:
                do_on_screen_keyboard_action (manager);
                break;
	case INCREASE_TEXT_KEY:
	case DECREASE_TEXT_KEY:
		do_text_size_action (manager, type);
		break;
	case MAGNIFIER_ZOOM_IN_KEY:
	case MAGNIFIER_ZOOM_OUT_KEY:
		do_magnifier_zoom_action (manager, type);
		break;
	case TOGGLE_CONTRAST_KEY:
		do_toggle_contrast_action (manager);
		break;
        case POWER_KEY:
                do_config_power_action (manager, "button-power");
                break;
        case SLEEP_KEY:
                do_config_power_action (manager, "button-sleep");
                break;
        case SUSPEND_KEY:
                do_config_power_action (manager, "button-suspend");
                break;
        case HIBERNATE_KEY:
                do_config_power_action (manager, "button-hibernate");
                break;
        case SCREEN_BRIGHTNESS_UP_KEY:
        case SCREEN_BRIGHTNESS_DOWN_KEY:
                do_screen_brightness_action (manager, type);
                break;
        case KEYBOARD_BRIGHTNESS_UP_KEY:
        case KEYBOARD_BRIGHTNESS_DOWN_KEY:
        case KEYBOARD_BRIGHTNESS_TOGGLE_KEY:
                do_keyboard_brightness_action (manager, type);
                break;
        case BATTERY_KEY:
                do_execute_desktop (manager, "gnome-power-statistics.desktop", timestamp);
                break;
        /* Note, no default so compiler catches missing keys */
        case CUSTOM_KEY:
                g_assert_not_reached ();
        }

        return FALSE;
}

static GdkScreen *
get_screen_from_root (CsdMediaKeysManager *manager,
                      Window               root)
{
        GSList    *l;

        /* Look for which screen we're receiving events */
        for (l = manager->priv->screens; l != NULL; l = l->next) {
                GdkScreen *screen = (GdkScreen *) l->data;
                GdkWindow *window = gdk_screen_get_root_window (screen);

                if (GDK_WINDOW_XID (window) == root)
                        return screen;
        }

        return NULL;
}

static GdkFilterReturn
filter_key_events (XEvent              *xevent,
                   GdkEvent            *event,
                   CsdMediaKeysManager *manager)
{
	XIEvent             *xiev;
	XIDeviceEvent       *xev;
	XGenericEventCookie *cookie;
        guint                i;
	guint                deviceid;

        /* verify we have a key event */
	if (xevent->type != GenericEvent)
		return GDK_FILTER_CONTINUE;
	cookie = &xevent->xcookie;
	if (cookie->extension != manager->priv->opcode)
		return GDK_FILTER_CONTINUE;

	xiev = (XIEvent *) xevent->xcookie.data;

	if (xiev->evtype != XI_KeyPress &&
	    xiev->evtype != XI_KeyRelease)
		return GDK_FILTER_CONTINUE;

	xev = (XIDeviceEvent *) xiev;

	deviceid = xev->sourceid;

        for (i = 0; i < manager->priv->keys->len; i++) {
                MediaKey *key;

                key = g_ptr_array_index (manager->priv->keys, i);

                if (match_xi2_key (key->key, xev)) {
                        switch (key->key_type) {
                        case VOLUME_DOWN_KEY:
                        case VOLUME_UP_KEY:
                        case VOLUME_DOWN_QUIET_KEY:
                        case VOLUME_UP_QUIET_KEY:
                        case SCREEN_BRIGHTNESS_UP_KEY:
                        case SCREEN_BRIGHTNESS_DOWN_KEY:
                        case KEYBOARD_BRIGHTNESS_UP_KEY:
                        case KEYBOARD_BRIGHTNESS_DOWN_KEY:
                                /* auto-repeatable keys */
                                if (xiev->evtype != XI_KeyPress)
                                        return GDK_FILTER_CONTINUE;
                                break;
                        default:
                                if (xiev->evtype != XI_KeyRelease) {
                                        return GDK_FILTER_CONTINUE;
                                }
                        }

                        manager->priv->current_screen = get_screen_from_root (manager, xev->root);

                        if (do_action (manager, deviceid, key->key_type, xev->time) == FALSE) {
                                return GDK_FILTER_REMOVE;
                        } else {
                                return GDK_FILTER_CONTINUE;
                        }
                }
        }

        return GDK_FILTER_CONTINUE;
}

static void
update_theme_settings (GSettings           *settings,
		       const char          *key,
		       CsdMediaKeysManager *manager)
{
	char *theme;

	theme = g_settings_get_string (manager->priv->interface_settings, key);
	if (g_str_equal (theme, HIGH_CONTRAST)) {
		g_free (theme);
	} else {
		if (g_str_equal (key, "gtk-theme")) {
			g_free (manager->priv->gtk_theme);
			manager->priv->gtk_theme = theme;
		} else {
			g_free (manager->priv->icon_theme);
			manager->priv->icon_theme = theme;
		}
	}
}

static gboolean
start_media_keys_idle_cb (CsdMediaKeysManager *manager)
{
        GSList *l;
        //char *theme_name;

        g_debug ("Starting media_keys manager");
        cinnamon_settings_profile_start (NULL);


        gvc_mixer_control_open (manager->priv->volume);

        manager->priv->settings = g_settings_new (SETTINGS_BINDING_DIR);
        g_signal_connect (G_OBJECT (manager->priv->settings), "changed",
                          G_CALLBACK (gsettings_changed_cb), manager);

        /* Sound events */
        ca_context_create (&manager->priv->ca);
        ca_context_set_driver (manager->priv->ca, "pulse");
        ca_context_change_props (manager->priv->ca, 0,
                                 CA_PROP_APPLICATION_ID, "org.gnome.VolumeControl",
                                 NULL);
        manager->priv->gtksettings = gtk_settings_get_for_screen (gdk_screen_get_default ());
        //g_object_get (G_OBJECT (manager->priv->gtksettings), "gtk-sound-theme-name", &theme_name, NULL);
        //if (theme_name)
        //        ca_context_change_props (manager->priv->ca, CA_PROP_CANBERRA_XDG_THEME_NAME, theme_name, NULL);
        //g_free (theme_name);
        //g_signal_connect (manager->priv->gtksettings, "notify::gtk-sound-theme-name",
        //                  G_CALLBACK (sound_theme_changed), manager);

        /* for the power plugin interface code */
        manager->priv->power_settings = g_settings_new (SETTINGS_POWER_DIR);

        /* Logic from http://git.gnome.org/browse/gnome-shell/tree/js/ui/status/accessibility.js#n163 */
        manager->priv->interface_settings = g_settings_new (SETTINGS_INTERFACE_DIR);
        g_signal_connect (G_OBJECT (manager->priv->interface_settings), "changed::gtk-theme",
			  G_CALLBACK (update_theme_settings), manager);
        g_signal_connect (G_OBJECT (manager->priv->interface_settings), "changed::icon-theme",
			  G_CALLBACK (update_theme_settings), manager);
	manager->priv->gtk_theme = g_settings_get_string (manager->priv->interface_settings, "gtk-theme");
	if (g_str_equal (manager->priv->gtk_theme, HIGH_CONTRAST)) {
		g_free (manager->priv->gtk_theme);
		manager->priv->gtk_theme = NULL;
	}
	manager->priv->icon_theme = g_settings_get_string (manager->priv->interface_settings, "icon-theme");

        init_screens (manager);
        init_kbd (manager);

        /* Start filtering the events */
        for (l = manager->priv->screens; l != NULL; l = l->next) {
                cinnamon_settings_profile_start ("gdk_window_add_filter");

                g_debug ("adding key filter for screen: %d",
                         gdk_screen_get_number (l->data));

                gdk_window_add_filter (gdk_screen_get_root_window (l->data),
                                       (GdkFilterFunc) filter_key_events,
                                       manager);
                cinnamon_settings_profile_end ("gdk_window_add_filter");
        }

        GSettings *settings = g_settings_new ("org.cinnamon.sounds");
        gboolean enabled = g_settings_get_boolean(settings, "login-enabled");
        gchar *sound = g_settings_get_string (settings, "login-file");
        if (enabled) {
            ca_context_play (manager->priv->ca, 1, CA_PROP_MEDIA_FILENAME, sound, NULL);
        }
        g_free(sound);
        g_object_unref (settings);

        cinnamon_settings_profile_end (NULL);

        manager->priv->start_idle_id = 0;

        return FALSE;
}

gboolean
csd_media_keys_manager_start (CsdMediaKeysManager *manager,
                              GError             **error)
{
        const char * const subsystems[] = { "input", "usb", "sound", NULL };

        cinnamon_settings_profile_start (NULL);

        if (supports_xinput2_devices (&manager->priv->opcode) == FALSE) {
                g_debug ("No Xinput2 support, disabling plugin");
                return TRUE;
        }

#ifdef HAVE_GUDEV
        manager->priv->streams = g_hash_table_new (g_direct_hash, g_direct_equal);
        manager->priv->udev_client = g_udev_client_new (subsystems);
#endif

        /* initialise Volume handler
         *
         * We do this one here to force checking gstreamer cache, etc.
         * The rest (grabbing and setting the keys) can happen in an
         * idle.
         */
        cinnamon_settings_profile_start ("gvc_mixer_control_new");

        manager->priv->volume = gvc_mixer_control_new ("Cinnamon Volume Control Media Keys");

        g_signal_connect (manager->priv->volume,
                          "state-changed",
                          G_CALLBACK (on_control_state_changed),
                          manager);
        g_signal_connect (manager->priv->volume,
                          "default-sink-changed",
                          G_CALLBACK (on_control_default_sink_changed),
                          manager);
        g_signal_connect (manager->priv->volume,
                          "stream-removed",
                          G_CALLBACK (on_control_stream_removed),
                          manager);

        cinnamon_settings_profile_end ("gvc_mixer_control_new");

        manager->priv->start_idle_id = g_idle_add ((GSourceFunc) start_media_keys_idle_cb, manager);

        register_manager (manager_object);

        cinnamon_settings_profile_end (NULL);

        return TRUE;
}

void
csd_media_keys_manager_stop (CsdMediaKeysManager *manager)
{
        CsdMediaKeysManagerPrivate *priv = manager->priv;
        GSList *ls;
        GList *l;
        int i;

        g_debug ("Stopping media_keys manager");

        if (priv->bus_cancellable != NULL) {
                g_cancellable_cancel (priv->bus_cancellable);
                g_object_unref (priv->bus_cancellable);
                priv->bus_cancellable = NULL;
        }

        for (ls = priv->screens; ls != NULL; ls = ls->next) {
                gdk_window_remove_filter (gdk_screen_get_root_window (ls->data),
                                          (GdkFilterFunc) filter_key_events,
                                          manager);
        }

        if (manager->priv->gtksettings != NULL) {
                //g_signal_handlers_disconnect_by_func (manager->priv->gtksettings, sound_theme_changed, manager);
                manager->priv->gtksettings = NULL;
        }

        if (manager->priv->ca) {
                ca_context_destroy (manager->priv->ca);
                manager->priv->ca = NULL;
        }

#ifdef HAVE_GUDEV
        if (priv->streams) {
                g_hash_table_destroy (priv->streams);
                priv->streams = NULL;
        }
        if (priv->udev_client) {
                g_object_unref (priv->udev_client);
                priv->udev_client = NULL;
        }
#endif /* HAVE_GUDEV */

        if (priv->settings) {
                g_object_unref (priv->settings);
                priv->settings = NULL;
        }

        if (priv->power_settings) {
                g_object_unref (priv->power_settings);
                priv->power_settings = NULL;
        }

        if (priv->power_screen_proxy) {
                g_object_unref (priv->power_screen_proxy);
                priv->power_screen_proxy = NULL;
        }

        if (priv->power_keyboard_proxy) {
                g_object_unref (priv->power_keyboard_proxy);
                priv->power_keyboard_proxy = NULL;
        }

        if (priv->upower_proxy) {
                g_object_unref (priv->upower_proxy);
                priv->upower_proxy = NULL;
        }

        if (priv->cancellable != NULL) {
                g_cancellable_cancel (priv->cancellable);
                g_object_unref (priv->cancellable);
                priv->cancellable = NULL;
        }

        if (priv->introspection_data) {
                g_dbus_node_info_unref (priv->introspection_data);
                priv->introspection_data = NULL;
        }

        if (priv->kb_introspection_data) {
                g_dbus_node_info_unref (priv->kb_introspection_data);
                priv->kb_introspection_data = NULL;
        }

        if (priv->connection != NULL) {
                g_object_unref (priv->connection);
                priv->connection = NULL;
        }

        if (priv->volume_notification != NULL) {
                notify_notification_close (priv->volume_notification, NULL);
                g_object_unref (priv->volume_notification);
                priv->volume_notification = NULL;
        }

        if (priv->brightness_notification != NULL) {
                notify_notification_close (priv->brightness_notification, NULL);
                g_object_unref (priv->brightness_notification);
                priv->brightness_notification = NULL;
        }

        if (priv->kb_backlight_notification != NULL) {
                notify_notification_close (priv->kb_backlight_notification, NULL);
                g_object_unref (priv->kb_backlight_notification);
                priv->kb_backlight_notification = NULL;
        }

        if (priv->keys != NULL) {
                gdk_error_trap_push ();
                for (i = 0; i < priv->keys->len; ++i) {
                        MediaKey *key;

                        key = g_ptr_array_index (manager->priv->keys, i);

                        if (key->key)
                                ungrab_key_unsafe (key->key, priv->screens);
                }
                g_ptr_array_free (priv->keys, TRUE);
                priv->keys = NULL;

                gdk_flush ();
                gdk_error_trap_pop_ignored ();
        }

        if (priv->screens != NULL) {
                g_slist_free (priv->screens);
                priv->screens = NULL;
        }

        if (priv->stream) {
                g_object_unref (priv->stream);
                priv->stream = NULL;
        }

        if (priv->volume) {
                g_object_unref (priv->volume);
                priv->volume = NULL;
        }

        if (priv->dialog != NULL) {
                gtk_widget_destroy (priv->dialog);
                priv->dialog = NULL;
        }

        if (priv->media_players != NULL) {
                for (l = priv->media_players; l; l = l->next) {
                        MediaPlayer *mp = l->data;
                        g_free (mp->application);
                        g_free (mp);
                }
                g_list_free (priv->media_players);
                priv->media_players = NULL;
        }
}

static GObject *
csd_media_keys_manager_constructor (GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_properties)
{
        CsdMediaKeysManager      *media_keys_manager;

        media_keys_manager = CSD_MEDIA_KEYS_MANAGER (G_OBJECT_CLASS (csd_media_keys_manager_parent_class)->constructor (type,
                                                                                                      n_construct_properties,
                                                                                                      construct_properties));

        return G_OBJECT (media_keys_manager);
}

static void
csd_media_keys_manager_class_init (CsdMediaKeysManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = csd_media_keys_manager_constructor;
        object_class->finalize = csd_media_keys_manager_finalize;

        g_type_class_add_private (klass, sizeof (CsdMediaKeysManagerPrivate));
}

static void
csd_media_keys_manager_init (CsdMediaKeysManager *manager)
{
        manager->priv = CSD_MEDIA_KEYS_MANAGER_GET_PRIVATE (manager);
}

static void
csd_media_keys_manager_finalize (GObject *object)
{
        CsdMediaKeysManager *media_keys_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_MEDIA_KEYS_MANAGER (object));

        media_keys_manager = CSD_MEDIA_KEYS_MANAGER (object);

        g_return_if_fail (media_keys_manager->priv != NULL);

        if (media_keys_manager->priv->start_idle_id != 0)
                g_source_remove (media_keys_manager->priv->start_idle_id);

        G_OBJECT_CLASS (csd_media_keys_manager_parent_class)->finalize (object);
}

static void
xrandr_ready_cb (GObject             *source_object,
                 GAsyncResult        *res,
                 CsdMediaKeysManager *manager)
{
        GError *error = NULL;

        manager->priv->xrandr_proxy = g_dbus_proxy_new_finish (res, &error);
        if (manager->priv->xrandr_proxy == NULL) {
                g_warning ("Failed to get proxy for XRandR operations: %s", error->message);
                g_error_free (error);
        }
}

static void
upower_ready_cb (GObject             *source_object,
                 GAsyncResult        *res,
                 CsdMediaKeysManager *manager)
{
        GError *error = NULL;

        manager->priv->upower_proxy = g_dbus_proxy_new_finish (res, &error);
        if (manager->priv->upower_proxy == NULL) {
                g_warning ("Failed to get proxy for upower: %s",
                           error->message);
                g_error_free (error);
        }
}

static void
power_screen_ready_cb (GObject             *source_object,
                       GAsyncResult        *res,
                       CsdMediaKeysManager *manager)
{
        GError *error = NULL;

        manager->priv->power_screen_proxy = g_dbus_proxy_new_finish (res, &error);
        if (manager->priv->power_screen_proxy == NULL) {
                g_warning ("Failed to get proxy for power (screen): %s",
                           error->message);
                g_error_free (error);
        }
}

static void
power_keyboard_ready_cb (GObject             *source_object,
                         GAsyncResult        *res,
                         CsdMediaKeysManager *manager)
{
        GError *error = NULL;

        manager->priv->power_keyboard_proxy = g_dbus_proxy_new_finish (res, &error);
        if (manager->priv->power_keyboard_proxy == NULL) {
                g_warning ("Failed to get proxy for power (keyboard): %s",
                           error->message);
                g_error_free (error);
        }
}

static void
on_bus_gotten (GObject             *source_object,
               GAsyncResult        *res,
               CsdMediaKeysManager *manager)
{
        GDBusConnection *connection;
        GError *error = NULL;

        if (manager->priv->bus_cancellable == NULL ||
            g_cancellable_is_cancelled (manager->priv->bus_cancellable)) {
                g_warning ("Operation has been cancelled, so not retrieving session bus");
                return;
        }

        connection = g_bus_get_finish (res, &error);
        if (connection == NULL) {
                g_warning ("Could not get session bus: %s", error->message);
                g_error_free (error);
                return;
        }
        manager->priv->connection = connection;

        g_dbus_connection_register_object (connection,
                                           CSD_MEDIA_KEYS_DBUS_PATH,
                                           manager->priv->introspection_data->interfaces[0],
                                           &interface_vtable,
                                           manager,
                                           NULL,
                                           NULL);

        g_dbus_connection_register_object (connection,
                                           CINNAMON_KEYBINDINGS_PATH,
                                           manager->priv->kb_introspection_data->interfaces[0],
                                           &interface_vtable,
                                           manager,
                                           NULL,
                                           NULL);

        g_dbus_proxy_new (manager->priv->connection,
                          G_DBUS_PROXY_FLAGS_NONE,
                          NULL,
                          "org.cinnamon.SettingsDaemon",
                          "/org/cinnamon/SettingsDaemon/XRANDR",
                          "org.cinnamon.SettingsDaemon.XRANDR_2",
                          NULL,
                          (GAsyncReadyCallback) xrandr_ready_cb,
                          manager);

        g_dbus_proxy_new (manager->priv->connection,
                          G_DBUS_PROXY_FLAGS_NONE,
                          NULL,
                          "org.cinnamon.SettingsDaemon",
                          "/org/cinnamon/SettingsDaemon/Power",
                          "org.cinnamon.SettingsDaemon.Power.Screen",
                          NULL,
                          (GAsyncReadyCallback) power_screen_ready_cb,
                          manager);

        g_dbus_proxy_new (manager->priv->connection,
                          G_DBUS_PROXY_FLAGS_NONE,
                          NULL,
                          "org.cinnamon.SettingsDaemon",
                          "/org/cinnamon/SettingsDaemon/Power",
                          "org.cinnamon.SettingsDaemon.Power.Keyboard",
                          NULL,
                          (GAsyncReadyCallback) power_keyboard_ready_cb,
                          manager);
}

static void
register_manager (CsdMediaKeysManager *manager)
{
        manager->priv->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
        manager->priv->kb_introspection_data = g_dbus_node_info_new_for_xml (kb_introspection_xml, NULL);
        manager->priv->bus_cancellable = g_cancellable_new ();
        g_assert (manager->priv->introspection_data != NULL);
        g_assert (manager->priv->kb_introspection_data != NULL);

        g_bus_get (G_BUS_TYPE_SESSION,
                   manager->priv->bus_cancellable,
                   (GAsyncReadyCallback) on_bus_gotten,
                   manager);

        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.freedesktop.UPower",
                                  "/org/freedesktop/UPower",
                                  "org.freedesktop.UPower",
                                  NULL,
                                  (GAsyncReadyCallback) upower_ready_cb,
                                  manager);
}

CsdMediaKeysManager *
csd_media_keys_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CSD_TYPE_MEDIA_KEYS_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return CSD_MEDIA_KEYS_MANAGER (manager_object);
}

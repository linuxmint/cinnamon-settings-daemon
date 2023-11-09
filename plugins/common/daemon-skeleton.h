/**
 * Create a test app for your plugin quickly.
 *
 * #define NEW csd_media_keys_manager_new
 * #define START csd_media_keys_manager_start
 * #define MANAGER CsdMediaKeysManager
 * #include "csd-media-keys-manager.h"
 *
 * #include "daemon-skeleton.h"
 */

#include "config.h"

#include <stdlib.h>
#include <signal.h>

#include <glib/gi18n.h>
#include <glib-unix.h>
#include <libnotify/notify.h>

#ifndef PLUGIN_NAME
#error Include PLUGIN_CFLAGS in the daemon s CFLAGS
#endif /* !PLUGIN_NAME */

#ifndef INIT_LIBNOTIFY
#define INIT_LIBNOTIFY FALSE
#endif

#define GNOME_SESSION_DBUS_NAME           "org.gnome.SessionManager"
#define GNOME_SESSION_DBUS_PATH           "/org/gnome/SessionManager"
#define GNOME_SESSION_CLIENT_PRIVATE_NAME "org.gnome.SessionManager.ClientPrivate"

static MANAGER *manager = NULL;
static int timeout = -1;
static gboolean verbose = FALSE;

static GOptionEntry entries[] = {
         { "exit-time", 0, 0, G_OPTION_ARG_INT, &timeout, "Exit after n seconds time", NULL },
         { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Verbose", NULL },
         {NULL}
};

static void
respond_to_end_session (GDBusProxy *proxy)
{
        /* we must answer with "EndSessionResponse" */
        g_dbus_proxy_call (proxy, "EndSessionResponse",
                           g_variant_new ("(bs)", TRUE, ""),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);
}

static void
client_proxy_signal_cb (GDBusProxy *proxy,
                        gchar *sender_name,
                        gchar *signal_name,
                        GVariant *parameters,
                        gpointer user_data)
{
        if (g_strcmp0 (signal_name, "QueryEndSession") == 0) {
                g_debug ("Got QueryEndSession signal");
                respond_to_end_session (proxy);
        } else if (g_strcmp0 (signal_name, "EndSession") == 0) {
                g_debug ("Got EndSession signal");
                respond_to_end_session (proxy);
        } else if (g_strcmp0 (signal_name, "Stop") == 0) {
                g_debug ("Got Stop signal");
                g_main_loop_quit ((GMainLoop *) user_data);
        }
}

static void
on_client_registered (GObject             *source_object,
                      GAsyncResult        *res,
                      GMainLoop           *loop)
{
        GVariant *variant;
        GDBusProxy *client_proxy;
        GError *error = NULL;
        gchar *object_path = NULL;

        variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (!variant) {
            if (error != NULL) {
                g_warning ("Unable to register client: %s", error->message);
                g_error_free (error);
            }

            return;
        }

        g_variant_get (variant, "(o)", &object_path);

        g_debug ("Registered client at path %s", object_path);

        client_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION, 0, NULL,
                                                      GNOME_SESSION_DBUS_NAME,
                                                      object_path,
                                                      GNOME_SESSION_CLIENT_PRIVATE_NAME,
                                                      NULL,
                                                      &error);
        if (!client_proxy) {
            if (error != NULL) {
                g_warning ("Unable to get the session client proxy: %s", error->message);
                g_error_free (error);
            }

            return;
        }

        g_signal_connect (client_proxy, "g-signal",
                          G_CALLBACK (client_proxy_signal_cb), loop);

        g_free (object_path);
        g_variant_unref (variant);
}

static void
register_with_cinnamon_session (GMainLoop *loop)
{
    const char *startup_id;
    GError *error = NULL;
    GDBusProxy *proxy;

    GDBusConnection *bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
    if (!bus) {
        g_debug ("Could not connect to the dbus session bus");
        return;
    }

    proxy = g_dbus_proxy_new_sync (bus,
                                   G_DBUS_PROXY_FLAGS_NONE,
                                   NULL,
                                   GNOME_SESSION_DBUS_NAME,
                                   GNOME_SESSION_DBUS_PATH,
                                   GNOME_SESSION_DBUS_NAME,
                                   NULL,
                                   &error);
    g_object_unref (bus);

    if (proxy == NULL) {
        if (error != NULL) {
            g_debug ("Could not connect to the Session manager: %s", error->message);
            g_error_free (error);
        }

        return;
    }

   startup_id = g_getenv ("DESKTOP_AUTOSTART_ID");
   g_dbus_proxy_call (proxy,
              "RegisterClient",
              g_variant_new ("(ss)", PLUGIN_NAME, startup_id ? startup_id : ""),
              G_DBUS_CALL_FLAGS_NONE,
              -1,
              NULL,
              (GAsyncReadyCallback) on_client_registered,
              loop);
}

static gboolean
handle_signal (gpointer user_data)
{
    GMainLoop *loop = (GMainLoop *) user_data;

    g_debug ("Got SIGTERM - shutting down");

    if (g_main_loop_is_running (loop))
        g_main_loop_quit (loop);

    return G_SOURCE_REMOVE;
}

int
main (int argc, char **argv)
{
        GError  *error;
        gboolean started;
        GOptionContext *context;
        GMainLoop *loop;


        bindtextdomain (GETTEXT_PACKAGE, CINNAMON_SETTINGS_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

        /* Work around https://bugzilla.gnome.org/show_bug.cgi?id=674885 */
        g_type_ensure (G_TYPE_DBUS_CONNECTION);
        g_type_ensure (G_TYPE_DBUS_PROXY);

        if (INIT_LIBNOTIFY) {
            notify_init ("cinnamon-settings-daemon");
        }

        error = NULL;


        context = g_option_context_new (NULL);
        g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                fprintf (stderr, "%s\n", error->message);
                g_error_free (error);
                exit (1);
        }
        g_option_context_free (context);

        loop = g_main_loop_new (NULL, FALSE);

        g_unix_signal_add (SIGTERM, (GSourceFunc) handle_signal, loop);

        if (verbose) {
                g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
                setlinebuf (stdout);
        }

        if (timeout > 0) {
                guint id;
                id = g_timeout_add_seconds (timeout, (GSourceFunc) g_main_loop_quit, loop);
                g_source_set_name_by_id (id, "[cinnamon-settings-daemon] g_main_loop_quit");
        }

        manager = NEW ();

        error = NULL;

        if (REGISTER_BEFORE_STARTING) {
          register_with_cinnamon_session (loop);
          started = START (manager, &error);
        }
        else {
          started = START (manager, &error);
          register_with_cinnamon_session (loop);
        }

        if (!started) {
            if (error != NULL) {
                fprintf (stderr, "[cinnamon-settings-daemon-%s] Failed to start: %s\n", PLUGIN_NAME, error->message);
                g_error_free (error);
            }

            exit (1);
        }

        g_main_loop_run (loop);

        STOP (manager);
        g_object_unref (manager);

        return 0;
}

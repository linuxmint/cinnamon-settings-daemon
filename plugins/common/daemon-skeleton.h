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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libnotify/notify.h>

#ifndef PLUGIN_NAME
#error Include PLUGIN_CFLAGS in the daemon s CFLAGS
#endif /* !PLUGIN_NAME */

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
                /* Ideally, we would call gtk_main_quit (); here.
                   Instead, do nothing.
                   cinnamon-session-manager doesn't wait in this STOP PHASE.
                   so we're not delaying the logout/shutdown sequence.
                   Also, if another process is lagging and delaying previous
                   phases, we don't want to lose CSD in the background.
                   We want CSD plugins to run until the very very end of the session.
                */
        }
}

static void
on_client_registered (GObject             *source_object,
                      GAsyncResult        *res,
                      gpointer             user_data)
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
                          G_CALLBACK (client_proxy_signal_cb), NULL);

        g_free (object_path);
        g_variant_unref (variant);
}

static void
register_with_cinnamon_session (void)
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
              NULL);
}

int
main (int argc, char **argv)
{
        GError  *error;
        gboolean started;

        bindtextdomain (GETTEXT_PACKAGE, CINNAMON_SETTINGS_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

        /* Work around https://bugzilla.gnome.org/show_bug.cgi?id=674885 */
        g_type_ensure (G_TYPE_DBUS_CONNECTION);
        g_type_ensure (G_TYPE_DBUS_PROXY);

        gdk_set_allowed_backends ("x11");

        notify_init ("cinnamon-settings-daemon");

        if (FORCE_GDK_SCALE) {
          g_setenv ("GDK_SCALE", "1", TRUE);
        }

        error = NULL;
        if (! gtk_init_with_args (&argc, &argv, PLUGIN_NAME, entries, NULL, &error)) {
            if (error != NULL) {
                fprintf (stderr, "%s\n", error->message);
                g_error_free (error);
            }

            exit (1);
        }

        if (FORCE_GDK_SCALE) {
          g_unsetenv ("GDK_SCALE");
        }

        if (verbose)
                g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

        if (timeout > 0) {
                guint id;
                id = g_timeout_add_seconds (timeout, (GSourceFunc) gtk_main_quit, NULL);
                g_source_set_name_by_id (id, "[cinnamon-settings-daemon] gtk_main_quit");
        }

        manager = NEW ();

        error = NULL;

        if (REGISTER_BEFORE_STARTING) {
          register_with_cinnamon_session ();
          started = START (manager, &error);
        }
        else {
          started = START (manager, &error);
          register_with_cinnamon_session ();
        }

        if (!started) {
            if (error != NULL) {
                fprintf (stderr, "[cinnamon-settings-daemon-%s] Failed to start: %s\n", PLUGIN_NAME, error->message);
                g_error_free (error);
            }

            exit (1);
        }

        gtk_main ();

        STOP (manager);
        g_object_unref (manager);

        return 0;
}

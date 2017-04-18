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

static MANAGER *manager = NULL;
static int timeout = -1;
static gboolean verbose = FALSE;

static GOptionEntry entries[] = {
         { "exit-time", 0, 0, G_OPTION_ARG_INT, &timeout, "Exit after n seconds time", NULL },
         { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "Verbose", NULL },
         {NULL}
};

int
main (int argc, char **argv)
{
        GError  *error;

        bindtextdomain (GETTEXT_PACKAGE, CINNAMON_SETTINGS_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);
        notify_init ("cinnamon-settings-daemon");

        error = NULL;
        if (! gtk_init_with_args (&argc, &argv, PLUGIN_NAME, entries, NULL, &error)) {
                fprintf (stderr, "%s\n", error->message);
                g_error_free (error);
                exit (1);
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
        START (manager, &error);

        gtk_main ();

        STOP (manager);
        g_object_unref (manager);

        return 0;
}

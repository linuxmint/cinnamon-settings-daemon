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

#ifndef SCHEMA_NAME
#define SCHEMA_NAME PLUGIN_NAME
#endif

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

static gboolean
has_settings (void)
{
	const gchar * const * list;
	guint i;

	list = g_settings_list_schemas ();
	for (i = 0; list[i] != NULL; i++) {
		if (g_str_equal (list[i], "org.cinnamon.settings-daemon.plugins." SCHEMA_NAME))
			return TRUE;
	}
	return FALSE;
}

int
main (int argc, char **argv)
{
        GError  *error;
        GSettings *settings;

        bindtextdomain (GETTEXT_PACKAGE, CINNAMON_SETTINGS_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);
        notify_init ("cinnamon-settings-daemon");

        error = NULL;
        if (! gtk_init_with_args (&argc, &argv, SCHEMA_NAME, entries, NULL, &error)) {
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

	if (has_settings () == FALSE) {
		fprintf (stderr, "The schemas for plugin '%s' aren't available, check your installation.\n", SCHEMA_NAME);
		exit (1);
	}

        settings = g_settings_new ("org.cinnamon.settings-daemon.plugins." SCHEMA_NAME);
        if (g_settings_get_boolean (settings, "active") != FALSE) {
		fprintf (stderr, "Plugin '%s' is not disabled. You need to disable it before launching the test application.\n", SCHEMA_NAME);
		fprintf (stderr, "To deactivate:\n");
		fprintf (stderr, "\tgsettings set org.cinnamon.settings-daemon.plugins." SCHEMA_NAME " active false\n");
		fprintf (stderr, "To reactivate:\n");
		fprintf (stderr, "\tgsettings set org.cinnamon.settings-daemon.plugins." SCHEMA_NAME " active true\n");
		exit (1);
	}

        manager = NEW ();

        error = NULL;
        START (manager, &error);

        gtk_main ();

        STOP (manager);
        g_object_unref (manager);

        return 0;
}

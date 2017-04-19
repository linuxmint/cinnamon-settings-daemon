/* Test for system timezone handling
 *
 * Copyright (C) 2008-2010 Novell, Inc.
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 */

#include <glib.h>
#include "system-timezone.h"

static void
timezone_print (void)
{
	SystemTimezone *systz;

	systz = system_timezone_new ();
        g_print ("Current timezone: %s\n", system_timezone_get (systz));
	g_object_unref (systz);
}

static int
timezone_set (const char *new_tz)
{
        GError *error;

        error = NULL;
        if (!system_timezone_set (new_tz, &error)) {
                g_printerr ("%s\n", error->message);
                g_error_free (error);
                return 1;
        }

	return 0;
}

int
main (int    argc,
      char **argv)
{
	int      retval;

	gboolean  get = FALSE;
	char     *tz_set = NULL;

	GError         *error;
	GOptionContext *context;
        GOptionEntry options[] = {
                { "get", 'g', 0, G_OPTION_ARG_NONE, &get, "Get the current timezone", NULL },
                { "set", 's', 0, G_OPTION_ARG_STRING, &tz_set, "Set the timezone to TIMEZONE", "TIMEZONE" },
                { NULL, 0, 0, 0, NULL, NULL, NULL }
        };

	retval = 0;


	context = g_option_context_new ("");
	g_option_context_add_main_entries (context, options, NULL);

	error = NULL;
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr ("%s\n", error->message);
		g_error_free (error);
		g_option_context_free (context);

		return 1;
	}

	g_option_context_free (context);

	if (get || (!tz_set))
		timezone_print ();
	else if (tz_set)
		retval = timezone_set (tz_set);
	else
		g_assert_not_reached ();

        return retval;
}

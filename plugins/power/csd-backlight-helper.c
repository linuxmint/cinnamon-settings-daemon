/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

#include <unistd.h>
#include <glib-object.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <gudev/gudev.h>
#include <gio/gio.h>

#define CSD_BACKLIGHT_HELPER_EXIT_CODE_SUCCESS			0
#define CSD_BACKLIGHT_HELPER_EXIT_CODE_FAILED			1
#define CSD_BACKLIGHT_HELPER_EXIT_CODE_ARGUMENTS_INVALID	3
#define CSD_BACKLIGHT_HELPER_EXIT_CODE_INVALID_USER		4
#define CSD_BACKLIGHT_HELPER_EXIT_CODE_NO_DEVICES		5

#define CSD_POWER_SETTINGS_SCHEMA	"org.cinnamon.settings-daemon.plugins.power"

static gchar *
csd_backlight_helper_get_type (GList *devices, const gchar *type)
{
	const gchar *type_tmp;
	GList *d;

	for (d = devices; d != NULL; d = d->next) {
		type_tmp = g_udev_device_get_sysfs_attr (d->data, "type");
		if (g_strcmp0 (type_tmp, type) == 0)
			return g_strdup (g_udev_device_get_sysfs_path (d->data));
	}
	return NULL;
}

static gchar *
csd_backlight_helper_get_best_backlight (gchar** preference_list)
{
	gchar *path = NULL;
	GList *devices;
	GUdevClient *client;

	client = g_udev_client_new (NULL);
	devices = g_udev_client_query_by_subsystem (client, "backlight");
	if (devices == NULL)
		goto out;

	/* setup our gsettings interface */
	if (preference_list == NULL || preference_list[0] == NULL) {
		g_print("%s\n%s\n",
		"Warning: no backlight sources have been configured.",
		"Check " CSD_POWER_SETTINGS_SCHEMA " to configure some.");
		goto out;
	}

	int i = 0;
	for (i=0; preference_list[i] != NULL; i++) {
		path = csd_backlight_helper_get_type 
			(devices, preference_list[i]);
		if (path != NULL)
			goto out;
	}

out:
	g_object_unref (client);
	g_list_foreach (devices, (GFunc) g_object_unref, NULL);
	g_list_free (devices);
	return path;
}

static gboolean
csd_backlight_helper_write (const gchar *filename, gint value, GError **error)
{
	gchar *text = NULL;
	gint retval;
	gint length;
	gint fd = -1;
	gboolean ret = TRUE;

	fd = open (filename, O_WRONLY);
	if (fd < 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to open filename: %s", filename);
		goto out;
	}

	/* convert to text */
	text = g_strdup_printf ("%i", value);
	length = strlen (text);

	/* write to device file */
	retval = write (fd, text, length);
	if (retval != length) {
		ret = FALSE;
		g_set_error (error, 1, 0, "writing '%s' to %s failed", text, filename);
		goto out;
	}
out:
	if (fd >= 0)
		close (fd);
	g_free (text);
	return ret;
}

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	gint uid;
	gint euid;
	guint retval = 0;
	GError *error = NULL;
	gboolean ret = FALSE;
	gint set_brightness = -1;
	gboolean get_brightness = FALSE;
	gboolean get_max_brightness = FALSE;
	gchar *filename = NULL;
	gchar *filename_file = NULL;
	gchar *contents = NULL;
	gchar** backlight_preference_order = NULL;

	const GOptionEntry options[] = {
		{ "set-brightness", '\0', 0, G_OPTION_ARG_INT, &set_brightness,
		   /* command line argument */
		  "Set the current brightness", NULL },
		{ "get-brightness", '\0', 0, G_OPTION_ARG_NONE, &get_brightness,
		   /* command line argument */
		  "Get the current brightness", NULL },
		{ "get-max-brightness", '\0', 0, G_OPTION_ARG_NONE, &get_max_brightness,
		   /* command line argument */
		  "Get the number of brightness levels supported", NULL },
        { "backlight-preference", 'b', 0, G_OPTION_ARG_STRING_ARRAY,
          &backlight_preference_order,
		   /* command line argument */
          "Set a backlight control search preference", NULL },
		{ NULL}
	};


	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, "Cinnamon Settings Daemon Backlight Helper");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

#ifndef __linux__
	/* the g-s-d plugin should only call this helper on linux */
	g_critical ("Attempting to call gsb-backlight-helper on non-Linux");
	g_assert_not_reached ();
#endif

	/* no input */
	if (set_brightness == -1 && !get_brightness && !get_max_brightness) {
		g_print ("%s\n", "No valid option was specified");
		retval = CSD_BACKLIGHT_HELPER_EXIT_CODE_ARGUMENTS_INVALID;
		goto out;
	}

	/* find device */
	filename = csd_backlight_helper_get_best_backlight
	                (backlight_preference_order);
	if (filename == NULL) {
		retval = CSD_BACKLIGHT_HELPER_EXIT_CODE_NO_DEVICES;
		g_print ("%s: %s\n",
			 "Could not get or set the value of the backlight",
			 "No backlight devices present");
		goto out;
	}

	/* GetBrightness */
	if (get_brightness) {
		filename_file = g_build_filename (filename, "brightness", NULL);
		ret = g_file_get_contents (filename_file, &contents, NULL, &error);
		if (!ret) {
			g_print ("%s: %s\n",
				 "Could not get the value of the backlight",
				 error->message);
			g_error_free (error);
			retval = CSD_BACKLIGHT_HELPER_EXIT_CODE_ARGUMENTS_INVALID;
			goto out;
		}

		/* just print the contents to stdout */
		g_print ("%s", contents);
		retval = CSD_BACKLIGHT_HELPER_EXIT_CODE_SUCCESS;
		goto out;
	}

	/* GetSteps */
	if (get_max_brightness) {
		filename_file = g_build_filename (filename, "max_brightness", NULL);
		ret = g_file_get_contents (filename_file, &contents, NULL, &error);
		if (!ret) {
			g_print ("%s: %s\n",
				 "Could not get the maximum value of the backlight",
				 error->message);
			g_error_free (error);
			retval = CSD_BACKLIGHT_HELPER_EXIT_CODE_ARGUMENTS_INVALID;
			goto out;
		}

		/* just print the contents to stdout */
		g_print ("%s", contents);
		retval = CSD_BACKLIGHT_HELPER_EXIT_CODE_SUCCESS;
		goto out;
	}

	/* check calling UID */
	uid = getuid ();
	euid = geteuid ();
	if (uid != 0 || euid != 0) {
		g_print ("%s\n",
			 "This program can only be used by the root user");
		retval = CSD_BACKLIGHT_HELPER_EXIT_CODE_ARGUMENTS_INVALID;
		goto out;
	}

	/* SetBrightness */
	if (set_brightness != -1) {
		filename_file = g_build_filename (filename, "brightness", NULL);
		ret = csd_backlight_helper_write (filename_file, set_brightness, &error);
		if (!ret) {
			g_print ("%s: %s\n",
				 "Could not set the value of the backlight",
				 error->message);
			g_error_free (error);
			retval = CSD_BACKLIGHT_HELPER_EXIT_CODE_ARGUMENTS_INVALID;
			goto out;
		}
	}

	/* success */
	retval = CSD_BACKLIGHT_HELPER_EXIT_CODE_SUCCESS;
out:
	g_free (filename);
	g_free (filename_file);
	g_free (contents);
	return retval;
}


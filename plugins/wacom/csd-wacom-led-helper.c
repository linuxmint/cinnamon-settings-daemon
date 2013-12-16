/*
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2012      Bastien Nocera <hadess@hadess.net>
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

#include <glib.h>
#include <locale.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <gudev/gudev.h>

static gboolean
csd_wacom_led_helper_write (const gchar *filename, gint value, GError **error)
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

static char *
get_led_sysfs_path (GUdevDevice *device,
		    int          group_num)
{
	char *status;
	char *filename;

	status = g_strdup_printf ("status_led%d_select", group_num);
	filename = g_build_filename (g_udev_device_get_sysfs_path (device), "wacom_led", status, NULL);
	g_free (status);

	return filename;
}

static char *path = NULL;
static int group_num = -1;
static int led_num = -1;

const GOptionEntry options[] = {
	{ "path", '\0', 0, G_OPTION_ARG_FILENAME, &path, "Device path for the Wacom device", NULL },
	{ "group", '\0', 0, G_OPTION_ARG_INT, &group_num, "Which LED group to set", NULL },
	{ "led", '\0', 0, G_OPTION_ARG_INT, &led_num, "Which LED to set", NULL },
	{ NULL}
};


int main (int argc, char **argv)
{
	GOptionContext *context;
	GUdevClient *client;
	GUdevDevice *device, *parent;
	int uid, euid;
	char *filename;
	GError *error = NULL;
        const char * const subsystems[] = { "input", NULL };

	/* get calling process */
	uid = getuid ();
	euid = geteuid ();
	if (uid != 0 || euid != 0) {
		g_print ("This program can only be used by the root user\n");
		return 1;
	}

	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, "Cinnamon Settings Daemon Digitizer LED Helper");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);

	if (path == NULL ||
	    group_num < 0 ||
	    led_num < 0) {
		char *txt;

		txt = g_option_context_get_help (context, FALSE, NULL);
		g_print ("%s", txt);
		g_free (txt);

		g_option_context_free (context);

		return 1;
	}
	g_option_context_free (context);

	client = g_udev_client_new (subsystems);
	device = g_udev_client_query_by_device_file (client, path);
	if (device == NULL) {
		g_debug ("Could not find device '%s' in udev database", path);
		goto bail;
	}

	if (g_udev_device_get_property_as_boolean (device, "ID_INPUT_TABLET") == FALSE &&
	    g_udev_device_get_property_as_boolean (device, "ID_INPUT_TOUCHPAD") == FALSE) {
		g_debug ("Device '%s' is not a Wacom tablet", path);
		goto bail;
	}

	if (g_strcmp0 (g_udev_device_get_property (device, "ID_BUS"), "usb") != 0) {
		/* FIXME handle Bluetooth LEDs too */
		g_debug ("Non-USB LEDs setting is not supported");
		goto bail;
	}

	parent = g_udev_device_get_parent_with_subsystem (device, "usb", "usb_interface");
	if (parent == NULL) {
		g_debug ("Could not find parent USB device for '%s'", path);
		goto bail;
	}
	g_object_unref (device);
	device = parent;

	filename = get_led_sysfs_path (device, group_num);
	if (csd_wacom_led_helper_write (filename, led_num, &error) == FALSE) {
		g_debug ("Could not set LED status for '%s': %s", path, error->message);
		g_error_free (error);
		g_free (filename);
		goto bail;
	}
	g_free (filename);

	g_debug ("Successfully set LED status for '%s', group %d to %d",
		 path, group_num, led_num);

	g_object_unref (device);
	g_object_unref (client);

	return 0;

bail:
	if (device != NULL)
		g_object_unref (device);
	if (client != NULL)
		g_object_unref (client);
	return 1;
}

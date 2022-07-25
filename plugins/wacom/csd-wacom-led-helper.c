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

#define LED_BRIGHTNESS 127 /* maximum brightness accepted by led on wacom Intuos4 connected over Bluetooth */

static int
csd_wacom_led_helper_write (const gchar *filename, gint value, GError **error)
{
	gchar *text = NULL;
	gint retval;
	gint length;
	gint fd = -1;
	int ret = 1;

	fd = open (filename, O_WRONLY);
	if (fd < 0) {
		g_set_error (error, 1, 0, "failed to open filename: %s", filename);
		goto out;
	}

	/* convert to text */
	text = g_strdup_printf ("%i", value);
	length = strlen (text);

	/* write to device file */
	retval = write (fd, text, length);
	if (retval != length) {
		g_set_error (error, 1, 0, "writing '%s' to %s failed", text, filename);
		goto out;
	} else
		ret = 0;
out:
	if (fd >= 0)
		close (fd);
	g_free (text);
	return ret;
}

static char *
get_led_sys_path (GUdevClient *client,
		  GUdevDevice *device,
		  int          group_num,
		  int          led_num,
		  gboolean     usb,
		  int         *write_value)
{
	GUdevDevice *parent;
	char *status = NULL;
	char *filename = NULL;
	GUdevDevice *hid_dev;
	const char *dev_uniq;
	GList *hid_list;
	GList *element;
	const char *dev_hid_uniq;

	/* check for new unified hid implementation first */
	parent = g_udev_device_get_parent_with_subsystem (device, "hid", NULL);
	if (parent) {
		status = g_strdup_printf ("status_led%d_select", group_num);
		filename = g_build_filename (g_udev_device_get_sysfs_path (parent), "wacom_led", status, NULL);
		g_free (status);
		g_object_unref (parent);
		if(g_file_test (filename, G_FILE_TEST_EXISTS)) {
			*write_value = led_num;
			return filename;
		}
		g_clear_pointer (&filename, g_free);
	}

	/* old kernel */
	if (usb) {
		parent = g_udev_device_get_parent_with_subsystem (device, "usb", "usb_interface");
		if (!parent)
			goto no_parent;
		status = g_strdup_printf ("status_led%d_select", group_num);
		filename = g_build_filename (g_udev_device_get_sysfs_path (parent), "wacom_led", status, NULL);
		g_free (status);

		*write_value = led_num;
	 } else {
		parent = g_udev_device_get_parent_with_subsystem (device, "input", NULL);
		if (!parent)
			goto no_parent;
		dev_uniq = g_udev_device_get_property (parent, "UNIQ");

		hid_list =  g_udev_client_query_by_subsystem (client, "hid");
		element = g_list_first(hid_list);
		while (element) {
			hid_dev = (GUdevDevice*)element->data;
			dev_hid_uniq = g_udev_device_get_property (hid_dev, "HID_UNIQ");
			if (g_strrstr (dev_uniq, dev_hid_uniq)){
				status = g_strdup_printf ("/leds/%s:selector:%i/brightness", g_udev_device_get_name (hid_dev), led_num);
				filename = g_build_filename (g_udev_device_get_sysfs_path (hid_dev), status, NULL);
				g_free (status);
				break;
			}
			element = g_list_next(element);
		}
		g_list_free_full(hid_list, g_object_unref);

		*write_value = LED_BRIGHTNESS;
	}

	g_object_unref (parent);

	return filename;

no_parent:
	g_debug ("Could not find proper parent device for '%s'",
		 g_udev_device_get_device_file (device));

	return NULL;
}

int main (int argc, char **argv)
{
	GOptionContext *context;
	GUdevClient *client;
	GUdevDevice *device;
	int uid, euid;
	char *filename;
	gboolean usb;
	int value;
	GError *error = NULL;
        const char * const subsystems[] = { "hid", "input", NULL };
        int ret = 1;

	char *path = NULL;
	int group_num = -1;
	int led_num = -1;

	const GOptionEntry options[] = {
		{ "path", '\0', 0, G_OPTION_ARG_FILENAME, &path, "Device path for the Wacom device", NULL },
		{ "group", '\0', 0, G_OPTION_ARG_INT, &group_num, "Which LED group to set", NULL },
		{ "led", '\0', 0, G_OPTION_ARG_INT, &led_num, "Which LED to set", NULL },
		{ NULL}
	};

	/* get calling process */
	uid = getuid ();
	euid = geteuid ();
	if (uid != 0 || euid != 0) {
		g_print ("This program can only be used by the root user\n");
		return 1;
	}

	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, "GNOME Settings Daemon Wacom LED Helper");
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
		goto out;
	}

	if (g_udev_device_get_property_as_boolean (device, "ID_INPUT_TABLET") == FALSE &&
	    g_udev_device_get_property_as_boolean (device, "ID_INPUT_TOUCHPAD") == FALSE) {
		g_debug ("Device '%s' is not a Wacom tablet", path);
		goto out;
	}

	if (g_strcmp0 (g_udev_device_get_property (device, "ID_BUS"), "usb") != 0)
		usb = FALSE;
	else
		usb = TRUE;

	filename = get_led_sys_path (client, device, group_num, led_num, usb, &value);
	if (!filename)
		goto out;

	if (csd_wacom_led_helper_write (filename, value, &error)) {
		g_debug ("Could not set LED status for '%s': %s", path, error->message);
		g_error_free (error);
		g_free (filename);
		goto out;
	}
	g_free (filename);

	g_debug ("Successfully set LED status for '%s', group %d to %d",
		 path, group_num, led_num);

	ret = 0;

out:
	g_free (path);
	g_clear_object (&device);
	g_clear_object (&client);

	return ret;
}

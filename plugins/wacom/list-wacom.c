/*
 * Copyright (C) 2011 Red Hat, Inc.
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
 * Author: Bastien Nocera <hadess@hadess.net>
 *
 */

#include "config.h"

#include <gtk/gtk.h>

#include "csd-wacom-device.h"

static gboolean fake_devices = FALSE;
static gboolean monitor_styli = FALSE;
static gboolean option_debug = FALSE;

static char *
get_loc (GSettings *settings)
{
	char *path, *schema, *ret;

	g_return_val_if_fail (G_IS_SETTINGS (settings), NULL);

	g_object_get (G_OBJECT (settings),
		      "path", &path,
		      "schema-id", &schema,
		      NULL);
	ret = g_strdup_printf ("schema-id: %s (path: %s)", schema, path);
	g_free (schema);
	g_free (path);

	return ret;
}

static const char *
stylus_type_to_string (CsdWacomStylusType type)
{
	switch (type) {
	case WACOM_STYLUS_TYPE_UNKNOWN:
		return "Unknown";
	case WACOM_STYLUS_TYPE_GENERAL:
		return "General";
	case WACOM_STYLUS_TYPE_INKING:
		return "Inking";
	case WACOM_STYLUS_TYPE_AIRBRUSH:
		return "Airbrush";
	case WACOM_STYLUS_TYPE_CLASSIC:
		return "Classic";
	case WACOM_STYLUS_TYPE_MARKER:
		return "Marker";
	case WACOM_STYLUS_TYPE_STROKE:
		return "Stroke";
	case WACOM_STYLUS_TYPE_PUCK:
		return "Puck";
	default:
		g_assert_not_reached ();
	}
	return NULL;
}

static const char *
button_type_to_string (CsdWacomTabletButtonType type)
{
	switch (type) {
	case WACOM_TABLET_BUTTON_TYPE_NORMAL:
		return "normal";
	case WACOM_TABLET_BUTTON_TYPE_STRIP:
		return "touch-strip";
	case WACOM_TABLET_BUTTON_TYPE_RING:
		return "touch-ring";
	case WACOM_TABLET_BUTTON_TYPE_HARDCODED:
		return "hard-coded";
	default:
		g_assert_not_reached ();
	}
}

#define BOOL_AS_STR(x) (x ? "yes" : "no")

static void
print_stylus (CsdWacomStylus *stylus,
	      gboolean        is_current)
{
	CsdWacomDevice *device;
	char *loc;

	device = csd_wacom_stylus_get_device (stylus);

	g_print ("\t%sStylus: '%s' (Type: %s, ID: 0x%x)\n",
		 is_current ? "*** " : "",
		 csd_wacom_stylus_get_name (stylus),
		 stylus_type_to_string (csd_wacom_stylus_get_stylus_type (stylus)),
		 csd_wacom_stylus_get_id (stylus));

	loc = get_loc (csd_wacom_stylus_get_settings (stylus));
	g_print ("\t\tSettings: %s\n", loc);
	g_free (loc);

	g_print ("\t\tIcon name: %s\n", csd_wacom_stylus_get_icon_name (stylus));

	if (csd_wacom_device_get_device_type (device) == WACOM_TYPE_STYLUS) {
		int num_buttons;
		char *buttons;

		g_print ("\t\tHas Eraser: %s\n", BOOL_AS_STR(csd_wacom_stylus_get_has_eraser (stylus)));

		num_buttons = csd_wacom_stylus_get_num_buttons (stylus);
		if (num_buttons < 0)
			num_buttons = 2;
		if (num_buttons > 0)
			buttons = g_strdup_printf ("%d buttons", num_buttons);
		else
			buttons = g_strdup ("no button");
		g_print ("\t\tButtons: %s\n", buttons);
		g_free (buttons);
	}
}

static void
print_buttons (CsdWacomDevice *device)
{
	GList *buttons, *l;

	buttons = csd_wacom_device_get_buttons (device);
	if (buttons == NULL)
		return;

	for (l = buttons; l != NULL; l = l->next) {
		CsdWacomTabletButton *button = l->data;

		g_print ("\tButton: %s (%s)\n", button->name, button->id);
		g_print ("\t\tType: %s\n", button_type_to_string (button->type));
		if (button->group_id > 0) {
			g_print ("\t\tGroup: %d", button->group_id);
			if (button->idx >= 0)
				g_print (" Index: %d\n", button->idx);
			else
				g_print ("\n");
		}
		if (button->settings) {
			char *loc;
			loc = get_loc (button->settings);
			g_print ("\t\tSettings: %s\n", loc);
			g_free (loc);
		}
	}
	g_list_free (buttons);
}

static void
last_stylus_changed (CsdWacomDevice  *device,
		     GParamSpec      *pspec,
		     gpointer         user_data)
{
	CsdWacomStylus *stylus;

	g_object_get (device, "last-stylus", &stylus, NULL);

	g_print ("Stylus changed for device '%s'\n",
		 csd_wacom_device_get_tool_name (device));

	print_stylus (stylus, TRUE);
}

static void
list_devices (GList *devices)
{
	GList *l;

	for (l = devices; l ; l = l->next) {
		CsdWacomDevice *device;
		CsdWacomDeviceType type;
		char *loc;

		device = l->data;

		g_signal_connect (G_OBJECT (device), "notify::last-stylus",
				  G_CALLBACK (last_stylus_changed), NULL);

		g_print ("Device '%s' (type: %s)\n",
			 csd_wacom_device_get_name (device),
			 csd_wacom_device_type_to_string (csd_wacom_device_get_device_type (device)));
		g_print ("\tReversible: %s\n", BOOL_AS_STR (csd_wacom_device_reversible (device)));
		g_print ("\tScreen Tablet: %s\n", BOOL_AS_STR (csd_wacom_device_is_screen_tablet (device)));
		g_print ("\tIntegrated Device: %s\n", BOOL_AS_STR (csd_wacom_device_is_isd (device)));
		g_print ("\tUnknown (fallback) device: %s\n", BOOL_AS_STR(csd_wacom_device_is_fallback (device)));

		loc = get_loc (csd_wacom_device_get_settings (device));
		g_print ("\tGeneric settings: %s\n", loc);
		g_free (loc);

		type = csd_wacom_device_get_device_type (device);
		if (type == WACOM_TYPE_STYLUS ||
		    type == WACOM_TYPE_ERASER) {
			GList *styli, *j;
			CsdWacomStylus *current_stylus;

			g_object_get (device, "last-stylus", &current_stylus, NULL);

			styli = csd_wacom_device_list_styli (device);
			for (j = styli; j; j = j->next) {
				CsdWacomStylus *stylus;

				stylus = j->data;
				print_stylus (stylus, current_stylus == stylus);
			}
			g_list_free (styli);
		}

		print_buttons (device);

		if (monitor_styli == FALSE)
			g_object_unref (device);
	}
	g_list_free (devices);
}

static void
list_actual_devices (void)
{
	GdkDeviceManager *mgr;
	GList *list, *l, *devices;

	mgr = gdk_display_get_device_manager (gdk_display_get_default ());

	list = gdk_device_manager_list_devices (mgr, GDK_DEVICE_TYPE_SLAVE);
	devices = NULL;
	for (l = list; l ; l = l->next) {
		CsdWacomDevice *device;

		device = csd_wacom_device_new (l->data);
		if (csd_wacom_device_get_device_type (device) == WACOM_TYPE_INVALID) {
			g_object_unref (device);
			continue;
		}

		devices = g_list_prepend (devices, device);
	}
	g_list_free (list);

	list_devices (devices);
}

static void
list_fake_devices (void)
{
	GList *devices;

	devices = csd_wacom_device_create_fake_cintiq ();
	list_devices (devices);

	devices = csd_wacom_device_create_fake_bt ();
	list_devices (devices);

	devices = csd_wacom_device_create_fake_x201 ();
	list_devices (devices);

	devices = csd_wacom_device_create_fake_intuos4 ();
	list_devices (devices);
}

int main (int argc, char **argv)
{
	GError *error = NULL;
	GOptionContext *context;
	const GOptionEntry entries[] = {
		{ "fake", 'f', 0, G_OPTION_ARG_NONE, &fake_devices, "Output fake devices", NULL },
		{ "monitor", 'm', 0, G_OPTION_ARG_NONE, &monitor_styli, "Monitor changing styli", NULL },
		{ "debug", 'd', 0, G_OPTION_ARG_NONE, &option_debug, "Debug output", NULL },
		{ NULL }
	};

	gtk_init (&argc, &argv);

	context = g_option_context_new ("- test parser functions");
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

	if (g_option_context_parse (context, &argc, &argv, &error) == FALSE) {
		g_print ("Option parsing failed: %s\n", error->message);
		g_option_context_free (context);
		return 1;
	}

	if (option_debug)
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

	if (fake_devices == FALSE)
		list_actual_devices ();
	else
		list_fake_devices ();

	if (monitor_styli)
		gtk_main ();

	g_option_context_free (context);

	return 0;
}

/*
 * Copyright (C) 2012 Red Hat, Inc.
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
 * Author: Olivier Fourdan <ofourdan@redhat.com>
 *
 */

#include "config.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <glib/gi18n.h>
#include "csd-wacom-osd-window.h"

static gboolean option_debug = FALSE;

static CsdWacomDevice *
search_pad_device (void)
{
	GdkDeviceManager *mgr;
	GList *list, *l;

	mgr = gdk_display_get_device_manager (gdk_display_get_default ());
	list = gdk_device_manager_list_devices (mgr, GDK_DEVICE_TYPE_SLAVE);
	for (l = list; l ; l = l->next) {
		CsdWacomDevice *device;

		device = csd_wacom_device_new (l->data);
		if (csd_wacom_device_get_device_type (device) == WACOM_TYPE_PAD)
			return (device);
		g_object_unref (device);
	}
	g_list_free (list);

	return NULL;
}

static CsdWacomDevice *
create_fake_device (const char *tablet)
{
	CsdWacomDevice *device;
	gchar *tool;

	tool = g_strdup_printf ("%s pad", tablet);
	device = csd_wacom_device_create_fake (WACOM_TYPE_PAD, tablet, tool);
	g_free (tool);

	return device;
}

static gboolean
on_key_release_event(GtkWidget   *widget,
                     GdkEventKey *event,
                     gpointer     data)
{
	gtk_main_quit();

	return FALSE;
}

int main(int argc, char** argv)
{
	GtkWidget *widget;
	GError *error = NULL;
	GOptionContext *context;
	CsdWacomDevice *device = NULL;
	gchar *message;
	gchar *tablet = NULL;
	const GOptionEntry entries[] = {
		{ "tablet", 't', 0, G_OPTION_ARG_STRING, &tablet, "Name of the tablet to show", "<string>"},
		{ "debug", 'd', 0, G_OPTION_ARG_NONE, &option_debug, "Debug output", NULL },
		{ NULL }
	};

	gtk_init (&argc, &argv);

	context = g_option_context_new ("- test functions");
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_set_help_enabled (context, TRUE);
	if (g_option_context_parse (context, &argc, &argv, &error) == FALSE) {
		g_print ("%s\n", error->message);
		g_option_context_free (context);
		return 1;
	}

	g_option_context_free (context);

	if (option_debug)
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

	if (tablet)
		device = create_fake_device (tablet);
	else
		device = search_pad_device ();

	if (device == NULL) {
		g_print ("No pad device found, consider using --tablet\n");
		return 1;
	}

	if (csd_wacom_device_get_layout_path (device) == NULL) {
		g_print ("This device has not layout available in libwacom\n");
		return 1;
	}

	message = g_strdup_printf ("<big><b>%s</b></big>\n<i>(Press a key to exit)</i>",
	                           csd_wacom_device_get_name (device));
	widget = csd_wacom_osd_window_new (device, message);
	g_free (message);

	g_signal_connect (widget, "key-release-event",
			  G_CALLBACK(on_key_release_event), NULL);
	g_signal_connect (widget, "delete-event",
			  G_CALLBACK (gtk_main_quit), NULL);
	g_signal_connect (widget, "unmap",
			  G_CALLBACK (gtk_main_quit), NULL);

	gtk_widget_show (widget);
	gtk_main ();

	g_free (tablet);

	if (device) {
		g_object_unref (device);
	}

	return 0;
}

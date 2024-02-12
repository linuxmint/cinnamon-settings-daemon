/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Bastien Nocera <hadess@hadess.net>
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
 */

#include "config.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <sys/types.h>
#include <X11/Xatom.h>

#include "csd-input-helper.h"

static void
print_disabled_devices (void)
{
	GList *devices, *l;
	GdkDeviceManager *manager;

	manager = gdk_display_get_device_manager (gdk_display_get_default ());

	devices = get_disabled_devices (manager);
	g_print ("Disabled devices:\t\t\t ");
	if (devices == NULL) {
		g_print ("no\n");
		return;
	}

	for (l = devices; l != NULL; l = l->next) {
		g_print ("%d ", GPOINTER_TO_INT (l->data));
	}
	g_list_free (devices);
	g_print ("\n");
}

int main (int argc, char **argv)
{
	gboolean supports_xinput;
	gboolean has_touchpad, has_touchscreen;
        XDeviceInfo *device_info;
        gint n_devices, opcode, i;

	gtk_init (&argc, &argv);

	supports_xinput = supports_xinput_devices ();
	if (supports_xinput) {
		g_print ("Supports XInput:\t\t\t yes\n");
	} else {
		g_print ("Supports XInput:\t\t\t no\n");
		return 0;
	}
	supports_xinput = supports_xinput2_devices (&opcode);
	if (supports_xinput) {
		g_print ("Supports XInput2:\t\t\t yes (opcode: %d)\n", opcode);
	} else {
		g_print ("Supports XInput2:\t\t\t no\n");
		return 0;
	}

	has_touchpad = touchpad_is_present ();
	g_print ("Has touchpad:\t\t\t\t %s\n", has_touchpad ? "yes" : "no");

	has_touchscreen = touchscreen_is_present ();
	g_print ("Has touchscreen:\t\t\t %s\n", has_touchscreen ? "yes" : "no");

        device_info = XListInputDevices (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), &n_devices);
        if (device_info == NULL) {
		g_warning ("Has no input devices");
                return 1;
	}

	print_disabled_devices ();

        for (i = 0; i < n_devices; i++) {
                XDevice *device;

		if (device_info_is_touchscreen (&device_info[i])) {
			g_print ("Device %d is touchscreen:\t\t %s\n", (int) device_info[i].id, "yes");
			continue;
		}

                gdk_x11_display_error_trap_push (gdk_display_get_default ());
                device = XOpenDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device_info[i].id);
                if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) || (device == NULL))
                        continue;

                if (device_is_touchpad (device))
			g_print ("Device %d is touchpad:\t\t %s\n", (int) device_info[i].id, "yes");
		else {
			int tool_id;

			tool_id = xdevice_get_last_tool_id (device_info[i].id);
			if (tool_id >= 0x0)
				g_print ("Device %d is touchpad/touchscreen:\t %s (tool ID: 0x%x)\n", (int) device_info[i].id, "no", tool_id);
			else
				g_print ("Device %d is touchpad/touchscreen:\t %s\n", (int) device_info[i].id, "no");
		}

                xdevice_close (device);
        }
        XFreeDeviceList (device_info);

	return 0;
}

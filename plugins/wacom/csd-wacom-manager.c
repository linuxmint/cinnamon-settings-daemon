/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2010 Red Hat, Inc.
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

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <locale.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <Xwacom.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libcinnamon-desktop/gnome-rr.h>

#include "csd-enums.h"
#include "csd-input-helper.h"
#include "csd-keygrab.h"
#include "cinnamon-settings-profile.h"
#include "csd-wacom-manager.h"
#include "csd-wacom-device.h"
#include "csd-wacom-osd-window.h"

#define CSD_WACOM_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CSD_TYPE_WACOM_MANAGER, CsdWacomManagerPrivate))

#define KEY_ROTATION            "rotation"
#define KEY_TOUCH               "touch"
#define KEY_TPCBUTTON           "tablet-pc-button"
#define KEY_IS_ABSOLUTE         "is-absolute"
#define KEY_AREA                "area"
#define KEY_DISPLAY             "display"
#define KEY_KEEP_ASPECT         "keep-aspect"
#define KEY_KEEP_ROTATION       "keep-rotation"

/* Stylus and Eraser settings */
#define KEY_BUTTON_MAPPING      "buttonmapping"
#define KEY_PRESSURETHRESHOLD   "pressurethreshold"
#define KEY_PRESSURECURVE       "pressurecurve"

/* Button settings */
#define KEY_ACTION_TYPE         "action-type"
#define KEY_CUSTOM_ACTION       "custom-action"
#define KEY_CUSTOM_ELEVATOR_ACTION "custom-elevator-action"

/* See "Wacom Pressure Threshold" */
#define DEFAULT_PRESSURE_THRESHOLD 27

struct CsdWacomManagerPrivate
{
        guint start_idle_id;
        GdkDeviceManager *device_manager;
        guint device_added_id;
        guint device_removed_id;
        GHashTable *devices; /* key = GdkDevice, value = CsdWacomDevice */
        GList *rr_screens;

        /* button capture */
        GSList *screens;
        int      opcode;

        /* Help OSD window */
        GtkWidget *osd_window;
};

static void     csd_wacom_manager_class_init  (CsdWacomManagerClass *klass);
static void     csd_wacom_manager_init        (CsdWacomManager      *wacom_manager);
static void     csd_wacom_manager_finalize    (GObject              *object);

G_DEFINE_TYPE (CsdWacomManager, csd_wacom_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static GObject *
csd_wacom_manager_constructor (GType                     type,
                               guint                      n_construct_properties,
                               GObjectConstructParam     *construct_properties)
{
        CsdWacomManager      *wacom_manager;

        wacom_manager = CSD_WACOM_MANAGER (G_OBJECT_CLASS (csd_wacom_manager_parent_class)->constructor (type,
                                                                                                         n_construct_properties,
                                                                                                         construct_properties));

        return G_OBJECT (wacom_manager);
}

static void
csd_wacom_manager_dispose (GObject *object)
{
        G_OBJECT_CLASS (csd_wacom_manager_parent_class)->dispose (object);
}

static void
csd_wacom_manager_class_init (CsdWacomManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = csd_wacom_manager_constructor;
        object_class->dispose = csd_wacom_manager_dispose;
        object_class->finalize = csd_wacom_manager_finalize;

        g_type_class_add_private (klass, sizeof (CsdWacomManagerPrivate));
}

static int
get_device_id (CsdWacomDevice *device)
{
	GdkDevice *gdk_device;
	int id;

	g_object_get (device, "gdk-device", &gdk_device, NULL);
	if (gdk_device == NULL)
		return -1;
	g_object_get (gdk_device, "device-id", &id, NULL);
	return id;
}

static XDevice *
open_device (CsdWacomDevice *device)
{
	XDevice *xdev;
	int id;

	id = get_device_id (device);
	if (id < 0)
		return NULL;

	gdk_x11_display_error_trap_push (gdk_display_get_default ());
	xdev = XOpenDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), id);
	if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()) || (xdev == NULL))
		return NULL;

	return xdev;
}


static void
wacom_set_property (CsdWacomDevice *device,
		    PropertyHelper *property)
{
	XDevice *xdev;

	xdev = open_device (device);
	device_set_property (xdev, csd_wacom_device_get_tool_name (device), property);
	xdevice_close (xdev);
}

static void
set_rotation (CsdWacomDevice *device,
	      CsdWacomRotation rotation)
{
        gchar rot = rotation;
        PropertyHelper property = {
                .name = "Wacom Rotation",
                .nitems = 1,
                .format = 8,
                .type   = XA_INTEGER,
                .data.c = &rot,
        };

        wacom_set_property (device, &property);
}

static void
set_pressurecurve (CsdWacomDevice *device,
                   GVariant       *value)
{
        PropertyHelper property = {
                .name = "Wacom Pressurecurve",
                .nitems = 4,
                .type   = XA_INTEGER,
                .format = 32,
        };
        gsize nvalues;

        property.data.i = g_variant_get_fixed_array (value, &nvalues, sizeof (gint32));

        if (nvalues != 4) {
                g_error ("Pressurecurve requires 4 values.");
                return;
        }

        wacom_set_property (device, &property);
        g_variant_unref (value);
}

/* Area handling. Each area is defined as top x/y, bottom x/y and limits the
 * usable area of the physical device to the given area (in device coords)
 */
static void
set_area (CsdWacomDevice  *device,
          GVariant        *value)
{
        PropertyHelper property = {
                .name = "Wacom Tablet Area",
                .nitems = 4,
                .type   = XA_INTEGER,
                .format = 32,
        };
        gsize nvalues;

        property.data.i = g_variant_get_fixed_array (value, &nvalues, sizeof (gint32));

        if (nvalues != 4) {
                g_error ("Area configuration requires 4 values.");
                return;
        }

        wacom_set_property (device, &property);
        g_variant_unref (value);
}

/* Returns the rotation to apply a device relative to the current rotation of the output */
static CsdWacomRotation
get_relative_rotation (CsdWacomRotation device_rotation,
                       CsdWacomRotation output_rotation)
{
	CsdWacomRotation rotations[] = { CSD_WACOM_ROTATION_HALF,
	                                 CSD_WACOM_ROTATION_CW,
	                                 CSD_WACOM_ROTATION_NONE,
	                                 CSD_WACOM_ROTATION_CCW };
	guint i;

	if (device_rotation == output_rotation)
		return CSD_WACOM_ROTATION_NONE;

	if (output_rotation == CSD_WACOM_ROTATION_NONE)
		return device_rotation;

	for (i = 0; i < G_N_ELEMENTS (rotations); i++){
		if (device_rotation == rotations[i])
			break;
	}

	if (output_rotation == CSD_WACOM_ROTATION_HALF)
		return rotations[(i + G_N_ELEMENTS (rotations) - 2) % G_N_ELEMENTS (rotations)];

	if (output_rotation == CSD_WACOM_ROTATION_CW)
		return rotations[(i + G_N_ELEMENTS (rotations) - 1) % G_N_ELEMENTS (rotations)];

	if (output_rotation == CSD_WACOM_ROTATION_CCW)
		return rotations[(i + 1) % G_N_ELEMENTS (rotations)];

	/* fallback */
	return CSD_WACOM_ROTATION_NONE;
}

static void
set_display (CsdWacomDevice  *device,
             GVariant        *value)
{
        CsdWacomRotation  device_rotation;
        CsdWacomRotation  output_rotation;
        GSettings        *settings;
        float matrix[NUM_ELEMS_MATRIX];
        PropertyHelper property = {
                .name   = "Coordinate Transformation Matrix",
                .nitems = NUM_ELEMS_MATRIX,
                .format = 32,
                .type   = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "FLOAT", True),
        };

        csd_wacom_device_get_display_matrix (device, matrix);

        property.data.i = (gint*)(&matrix);
        g_debug ("Applying matrix to device...");
        wacom_set_property (device, &property);

        /* Apply display rotation to device */
        settings = csd_wacom_device_get_settings (device);
        if (g_settings_get_boolean (settings, KEY_KEEP_ROTATION)) {
            device_rotation = g_settings_get_enum (settings, KEY_ROTATION);
            output_rotation = csd_wacom_device_get_display_rotation (device);
            set_rotation (device, get_relative_rotation (device_rotation, output_rotation));
        }

        g_variant_unref (value);
}

static void
set_absolute (CsdWacomDevice  *device,
              gint             is_absolute)
{
	XDevice *xdev;

	xdev = open_device (device);
	gdk_x11_display_error_trap_push (gdk_display_get_default ());
	XSetDeviceMode (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), xdev, is_absolute ? Absolute : Relative);
	if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()))
		g_error ("Failed to set mode \"%s\" for \"%s\".",
			 is_absolute ? "Absolute" : "Relative", csd_wacom_device_get_tool_name (device));
	xdevice_close (xdev);
}

static void
compute_aspect_area (gint monitor,
                     gint *area,
                     CsdWacomRotation rotation)
{
	gint width  = area[2] - area[0];
	gint height = area[3] - area[1];
	GdkScreen *screen;
	GdkRectangle monitor_geometry;
	float aspect;

	screen = gdk_screen_get_default ();
	if (monitor < 0) {
		monitor_geometry.width = gdk_screen_get_width (screen);
		monitor_geometry.height = gdk_screen_get_height (screen);
	} else {
		gdk_screen_get_monitor_geometry (screen, monitor, &monitor_geometry);
	}

	if (rotation == CSD_WACOM_ROTATION_CW || rotation == CSD_WACOM_ROTATION_CCW)
		aspect = (float) monitor_geometry.height / (float) monitor_geometry.width;
	else
		aspect = (float) monitor_geometry.width / (float) monitor_geometry.height;

	if ((float) width / (float) height > aspect)
		width = height * aspect;
	else
		height = width / aspect;

	switch (rotation)
	{
		case CSD_WACOM_ROTATION_NONE:
			area[2] = area[0] + width;
			area[3] = area[1] + height;
			break;
		case CSD_WACOM_ROTATION_CW:
			area[0] = area[2] - width;
			area[3] = area[1] + height;
			break;
		case CSD_WACOM_ROTATION_HALF:
			area[0] = area[2] - width;
			area[1] = area[3] - height;
			break;
		case CSD_WACOM_ROTATION_CCW:
			area[2] = area[0] + width;
			area[1] = area[3] - height;
			break;
		default:
			break;
	}
}

static void
set_keep_aspect (CsdWacomDevice *device,
                 gboolean        keep_aspect)
{
        GVariant *values[4], *variant;
	guint i;

	gint *area;
	gint monitor = CSD_WACOM_SET_ALL_MONITORS;
	CsdWacomRotation rotation;
	GSettings *settings;

        settings = csd_wacom_device_get_settings (device);

        /* Set area to default values for the device */
	for (i = 0; i < G_N_ELEMENTS (values); i++)
		values[i] = g_variant_new_int32 (-1);
	variant = g_variant_new_array (G_VARIANT_TYPE_INT32, values, G_N_ELEMENTS (values));

        /* If keep_aspect is not set, just reset the area to default and let
         * gsettings notification call set_area() for us...
         */
	if (!keep_aspect) {
		g_settings_set_value (settings, KEY_AREA, variant);
		return;
        }

        /* Reset the device area to get the default area */
	set_area (device, variant);

	/* Get current rotation */
	rotation = g_settings_get_enum (settings, KEY_ROTATION);

	/* Get current area */
	area = csd_wacom_device_get_area (device);
	if (!area) {
		g_warning("Device area not available.\n");
		return;
	}

	/* Get corresponding monitor size */
	monitor = csd_wacom_device_get_display_monitor (device);

	/* Adjust area to match the monitor aspect ratio */
	g_debug ("Initial device area: (%d,%d) (%d,%d)", area[0], area[1], area[2], area[3]);
	compute_aspect_area (monitor, area, rotation);
	g_debug ("Adjusted device area: (%d,%d) (%d,%d)", area[0], area[1], area[2], area[3]);

	for (i = 0; i < G_N_ELEMENTS (values); i++)
		values[i] = g_variant_new_int32 (area[i]);
	variant = g_variant_new_array (G_VARIANT_TYPE_INT32, values, G_N_ELEMENTS (values));
	g_settings_set_value (settings, KEY_AREA, variant);

	g_free (area);
}


static void
set_device_buttonmap (CsdWacomDevice *device,
                      GVariant       *value)
{
	XDevice *xdev;
	gsize nmap;
	const gint *intmap;
	unsigned char *map;
	int i, j, rc;

	xdev = open_device (device);

	intmap = g_variant_get_fixed_array (value, &nmap, sizeof (gint32));
	map = g_new0 (unsigned char, nmap);
	for (i = 0; i < nmap; i++)
		map[i] = intmap[i];
        g_variant_unref (value);

	gdk_x11_display_error_trap_push (gdk_display_get_default ());

	/* X refuses to change the mapping while buttons are engaged,
	 * so if this is the case we'll retry a few times
	 */
	for (j = 0;
	     j < 20 && (rc = XSetDeviceButtonMapping (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), xdev, map, nmap)) == MappingBusy;
	     ++j) {
		g_usleep (300);
	}

	if ((gdk_x11_display_error_trap_pop (gdk_display_get_default ()) && rc != MappingSuccess) ||
	    rc != MappingSuccess)
		g_warning ("Error in setting button mapping for \"%s\"", csd_wacom_device_get_tool_name (device));

	g_free (map);

	xdevice_close (xdev);
}

static void
set_touch (CsdWacomDevice *device,
	   gboolean        touch)
{
        gchar data = touch;
        PropertyHelper property = {
                .name = "Wacom Enable Touch",
                .nitems = 1,
                .format = 8,
                .type   = XA_INTEGER,
                .data.c = &data,
        };

        wacom_set_property (device, &property);
}

static void
set_tpcbutton (CsdWacomDevice *device,
	       gboolean        tpcbutton)
{
        /* Wacom's TPCButton option which this setting emulates is to enable
         * Tablet PC stylus behaviour when on. The property "Hover Click"
         * works the other way round, i.e. if Hover Click is enabled this
         * is the equivalent of TPC behaviour disabled. */
        gchar data = tpcbutton ? 0 : 1;
        PropertyHelper property = {
                .name = "Wacom Hover Click",
                .nitems = 1,
                .format = 8,
                .type   = XA_INTEGER,
                .data.c = &data,
        };

        wacom_set_property (device, &property);
}

static void
set_pressurethreshold (CsdWacomDevice *device,
                       gint            threshold)
{
        PropertyHelper property = {
                .name = "Wacom Pressure Threshold",
                .nitems = 1,
                .format = 32,
                .type   = XA_INTEGER,
                .data.i = &threshold,
        };

        wacom_set_property (device, &property);
}

static void
apply_stylus_settings (CsdWacomDevice *device)
{
	GSettings *stylus_settings;
	CsdWacomStylus *stylus;
	int threshold;

	g_object_get (device, "last-stylus", &stylus, NULL);
	if (stylus == NULL) {
		g_warning ("Last stylus is not set");
		return;
	}

	g_debug ("Applying setting for stylus '%s' on device '%s'",
		 csd_wacom_stylus_get_name (stylus),
		 csd_wacom_device_get_name (device));

	stylus_settings = csd_wacom_stylus_get_settings (stylus);
	set_pressurecurve (device, g_settings_get_value (stylus_settings, KEY_PRESSURECURVE));
	set_device_buttonmap (device, g_settings_get_value (stylus_settings, KEY_BUTTON_MAPPING));

	threshold = g_settings_get_int (stylus_settings, KEY_PRESSURETHRESHOLD);
	if (threshold == -1)
		threshold = DEFAULT_PRESSURE_THRESHOLD;
	set_pressurethreshold (device, threshold);
}

static void
set_led (CsdWacomDevice       *device,
	 CsdWacomTabletButton *button,
	 int                   index)
{
	GError *error = NULL;
	const char *path;
	char *command;
	gint status_led;
	gboolean ret;

#ifndef HAVE_GUDEV
	/* Not implemented on non-Linux systems */
	return;
#endif
	g_return_if_fail (index >= 1);

	path = csd_wacom_device_get_path (device);
	status_led = button->status_led;

	if (status_led == CSD_WACOM_NO_LED) {
		g_debug ("Ignoring unhandled group ID %d for device %s",
		         button->group_id, csd_wacom_device_get_name (device));
		return;
	}
	g_debug ("Switching group ID %d to index %d for device %s", button->group_id, index, path);

	command = g_strdup_printf ("pkexec " LIBEXECDIR "/csd-wacom-led-helper --path %s --group %d --led %d",
				   path, status_led, index - 1);
	ret = g_spawn_command_line_sync (command,
					 NULL,
					 NULL,
					 NULL,
					 &error);

	if (ret == FALSE) {
		g_debug ("Failed to launch '%s': %s", command, error->message);
		g_error_free (error);
	}

	g_free (command);
}

struct DefaultButtons {
	const char *button;
	int         num;
};

struct DefaultButtons def_touchrings_buttons[] = {
	/* Touchrings */
	{ "AbsWheelUp", 90 },
	{ "AbsWheelDown", 91 },
	{ "RelWheelUp", 90 },
	{ "RelWheelDown", 91 },
	{ "AbsWheel2Up", 92 },
	{ "AbsWheel2Down", 93 },
	{ NULL, 0 }
};

struct DefaultButtons def_touchstrip_buttons[] = {
	/* Touchstrips */
	{ "StripLeftUp", 94 },
	{ "StripLeftDown", 95 },
	{ "StripRightUp", 96 },
	{ "StripRightDown", 97 },
	{ NULL, 0 }
};

static void
reset_touch_buttons (XDevice               *xdev,
		     struct DefaultButtons *buttons,
		     const char            *device_property)
{
	Atom actions[6];
	Atom action_prop;
	guint i;

	/* Create a device property with the action for button i */
	for (i = 0; buttons[i].button != NULL; i++)
	{
		char *propname;
		glong action[2]; /* press + release */
		Atom prop;
		int mapped_button = buttons[i].num;

		action[0] = AC_BUTTON | AC_KEYBTNPRESS | mapped_button;
		action[1] = AC_BUTTON | mapped_button;

		propname = g_strdup_printf ("Button %s action", buttons[i].button);
		prop = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), propname, False);
		g_free (propname);
		XChangeDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), xdev,
				       prop, XA_INTEGER, 32, PropModeReplace,
				       (const guchar *) &action, 2);

		/* prop now contains press + release for the mapped button */
		actions[i] = prop;
	}

	/* Now set the actual action property to contain references to the various
	 * actions */
	action_prop = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device_property, True);
	XChangeDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), xdev,
			       action_prop, XA_ATOM, 32, PropModeReplace,
			       (const guchar *) actions, i);
}

static void
reset_pad_buttons (CsdWacomDevice *device)
{
	XDevice *xdev;
	int nmap;
	unsigned char *map;
	int i, j, rc;
	GList *buttons, *l;

	/* Normal buttons */
	xdev = open_device (device);

	gdk_x11_display_error_trap_push (gdk_display_get_default ());

	nmap = 256;
	map = g_new0 (unsigned char, nmap);
	for (i = 0; i < nmap && i < sizeof (map); i++)
		map[i] = i + 1;

	/* X refuses to change the mapping while buttons are engaged,
	 * so if this is the case we'll retry a few times */
	for (j = 0;
	     j < 20 && (rc = XSetDeviceButtonMapping (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), xdev, map, nmap)) == MappingBusy;
	     ++j) {
		g_usleep (300);
	}

	if ((gdk_x11_display_error_trap_pop (gdk_display_get_default ()) && rc != MappingSuccess) ||
	    rc != MappingSuccess)
		g_warning ("Error in resetting button mapping for \"%s\" (rc=%d)", csd_wacom_device_get_tool_name (device), rc);

	g_free (map);

	gdk_x11_display_error_trap_push (gdk_display_get_default ());
	reset_touch_buttons (xdev, def_touchrings_buttons, "Wacom Wheel Buttons");
	reset_touch_buttons (xdev, def_touchstrip_buttons, "Wacom Strip Buttons");
	gdk_x11_display_error_trap_pop_ignored (gdk_display_get_default ());

	xdevice_close (xdev);

	/* Reset all the LEDs */
	buttons = csd_wacom_device_get_buttons (device);
	for (l = buttons; l != NULL; l = l->next) {
		CsdWacomTabletButton *button = l->data;
                if (button->type == WACOM_TABLET_BUTTON_TYPE_HARDCODED &&
                    button->status_led != CSD_WACOM_NO_LED) {
                        set_led (device, button, 1);
                }
        }
        g_list_free (buttons);
}

static void
set_wacom_settings (CsdWacomManager *manager,
		    CsdWacomDevice  *device)
{
	CsdWacomDeviceType type;
	GSettings *settings;

	g_debug ("Applying settings for device '%s' (type: %s)",
		 csd_wacom_device_get_tool_name (device),
		 csd_wacom_device_type_to_string (csd_wacom_device_get_device_type (device)));

	settings = csd_wacom_device_get_settings (device);
        set_rotation (device, g_settings_get_enum (settings, KEY_ROTATION));
        set_touch (device, g_settings_get_boolean (settings, KEY_TOUCH));

        type = csd_wacom_device_get_device_type (device);

	if (type == WACOM_TYPE_TOUCH &&
	    csd_wacom_device_is_screen_tablet (device) == FALSE) {
		set_absolute (device, FALSE);
		return;
	}

	if (type == WACOM_TYPE_CURSOR) {
		GVariant *values[4], *variant;
		guint i;

		set_absolute (device, FALSE);

		for (i = 0; i < G_N_ELEMENTS (values); i++)
			values[i] = g_variant_new_int32 (-1);

		variant = g_variant_new_array (G_VARIANT_TYPE_INT32, values, G_N_ELEMENTS (values));
		set_area (device, variant);
		return;
	}

	if (type == WACOM_TYPE_PAD) {
		int id;

		id = get_device_id (device);
		reset_pad_buttons (device);
		grab_button (id, TRUE, manager->priv->screens);
		return;
	}

	if (type == WACOM_TYPE_STYLUS)
		set_tpcbutton (device, g_settings_get_boolean (settings, KEY_TPCBUTTON));

	set_absolute (device, g_settings_get_boolean (settings, KEY_IS_ABSOLUTE));

	/* Ignore touch devices as they do not share the same range of values for area */
	if (type != WACOM_TYPE_TOUCH) {
		if (csd_wacom_device_is_screen_tablet (device) == FALSE)
			set_keep_aspect (device, g_settings_get_boolean (settings, KEY_KEEP_ASPECT));
		set_area (device, g_settings_get_value (settings, KEY_AREA));
	}
	set_display (device, g_settings_get_value (settings, KEY_DISPLAY));

        /* only pen and eraser have pressure threshold and curve settings */
        if (type == WACOM_TYPE_STYLUS ||
	    type == WACOM_TYPE_ERASER) {
		apply_stylus_settings (device);
	}
}

static void
wacom_settings_changed (GSettings      *settings,
			gchar          *key,
			CsdWacomDevice *device)
{
	CsdWacomDeviceType type;

	type = csd_wacom_device_get_device_type (device);

	if (g_str_equal (key, KEY_ROTATION)) {
	        if (type != WACOM_TYPE_PAD)
		        set_rotation (device, g_settings_get_enum (settings, key));
	} else if (g_str_equal (key, KEY_TOUCH)) {
		set_touch (device, g_settings_get_boolean (settings, key));
	} else if (g_str_equal (key, KEY_TPCBUTTON)) {
		set_tpcbutton (device, g_settings_get_boolean (settings, key));
	} else if (g_str_equal (key, KEY_IS_ABSOLUTE)) {
		if (type != WACOM_TYPE_CURSOR &&
		    type != WACOM_TYPE_PAD &&
		    type != WACOM_TYPE_TOUCH)
			set_absolute (device, g_settings_get_boolean (settings, key));
	} else if (g_str_equal (key, KEY_AREA)) {
		if (type != WACOM_TYPE_CURSOR &&
		    type != WACOM_TYPE_PAD &&
		    type != WACOM_TYPE_TOUCH)
			set_area (device, g_settings_get_value (settings, key));
	} else if (g_str_equal (key, KEY_DISPLAY)) {
		if (type != WACOM_TYPE_CURSOR &&
		    type != WACOM_TYPE_PAD)
			set_display (device, g_settings_get_value (settings, key));
	} else if (g_str_equal (key, KEY_KEEP_ASPECT)) {
		if (type != WACOM_TYPE_CURSOR &&
		    type != WACOM_TYPE_PAD &&
		    type != WACOM_TYPE_TOUCH &&
		    !csd_wacom_device_is_screen_tablet (device))
			set_keep_aspect (device, g_settings_get_boolean (settings, key));
	} else {
		g_warning ("Unhandled tablet-wide setting '%s' changed", key);
	}
}

static void
stylus_settings_changed (GSettings      *settings,
			 gchar          *key,
			 CsdWacomStylus *stylus)
{
	CsdWacomDevice *device;
	CsdWacomStylus *last_stylus;

	device = csd_wacom_stylus_get_device (stylus);

	g_object_get (device, "last-stylus", &last_stylus, NULL);
	if (last_stylus != stylus) {
		g_debug ("Not applying changed settings because '%s' is the current stylus, not '%s'",
			 last_stylus ? csd_wacom_stylus_get_name (last_stylus) : "NONE",
			 csd_wacom_stylus_get_name (stylus));
		return;
	}

	if (g_str_equal (key, KEY_PRESSURECURVE)) {
		set_pressurecurve (device, g_settings_get_value (settings, key));
	} else if (g_str_equal (key, KEY_PRESSURETHRESHOLD)) {
		int threshold;

		threshold = g_settings_get_int (settings, KEY_PRESSURETHRESHOLD);
		if (threshold == -1)
			threshold = DEFAULT_PRESSURE_THRESHOLD;
		set_pressurethreshold (device, threshold);
	} else if (g_str_equal (key, KEY_BUTTON_MAPPING)) {
		set_device_buttonmap (device, g_settings_get_value (settings, key));
	}  else {
		g_warning ("Unhandled stylus setting '%s' changed", key);
	}
}

static void
last_stylus_changed (CsdWacomDevice  *device,
		     GParamSpec      *pspec,
		     CsdWacomManager *manager)
{
	g_debug ("Stylus for device '%s' changed, applying settings",
		 csd_wacom_device_get_name (device));
	apply_stylus_settings (device);
}

static void
osd_window_destroy (CsdWacomManager *manager)
{
	g_return_if_fail (manager != NULL);

        g_clear_pointer (&manager->priv->osd_window, gtk_widget_destroy);
}

static gboolean
osd_window_on_key_release_event (GtkWidget   *widget,
                                 GdkEventKey *event,
                                 CsdWacomManager *manager)
{

	if (event->type != GDK_KEY_RELEASE)
		return FALSE;
	if (event->keyval != GDK_KEY_Escape)
		return FALSE;

	osd_window_destroy (manager);

	return FALSE;
}

static gboolean
osd_window_on_focus_out_event (GtkWidget *widget,
                               GdkEvent  *event,
                               CsdWacomManager *manager)
{
	/* If the OSD window loses focus, hide it */
	osd_window_destroy (manager);

	return FALSE;
}

static gboolean
osd_window_toggle_visibility (CsdWacomManager *manager,
                              CsdWacomDevice  *device)
{
	GtkWidget *widget;
        const gchar *layout_path;

	if (manager->priv->osd_window) {
		osd_window_destroy (manager);
		return FALSE;
	}

        layout_path = csd_wacom_device_get_layout_path (device);
        if (layout_path == NULL) {
                g_warning ("Cannot display the on-screen help window as the tablet "
                           "definition for %s is missing the layout\n"
                           "Please consider contributing the layout for your "
                           "tablet to libwacom at linuxwacom-devel@lists.sourceforge.net\n",
                           csd_wacom_device_get_name (device));
		return FALSE;
        }

        if (g_file_test (layout_path, G_FILE_TEST_EXISTS) == FALSE) {
                g_warning ("Cannot display the on-screen help window as the "
                           "layout file %s cannot be found on disk\n"
                           "Please check your libwacom installation\n",
                           layout_path);
		return FALSE;
        }

	widget = csd_wacom_osd_window_new (device, NULL);

	/* Connect some signals to the OSD window */
	g_signal_connect (widget, "key-release-event",
			  G_CALLBACK(osd_window_on_key_release_event), manager);
	g_signal_connect (widget, "focus-out-event",
			  G_CALLBACK(osd_window_on_focus_out_event), manager);
	g_object_add_weak_pointer (G_OBJECT (widget), (gpointer *) &manager->priv->osd_window);

	gtk_window_present (GTK_WINDOW(widget));
	manager->priv->osd_window = widget;

	return TRUE;
}

static gboolean
osd_window_update_viewable (CsdWacomManager      *manager,
                            CsdWacomTabletButton *button,
                            GtkDirectionType      dir,
                            XIEvent              *xiev)
{
	if (manager->priv->osd_window == NULL)
		return FALSE;

	csd_wacom_osd_window_set_active (CSD_WACOM_OSD_WINDOW (manager->priv->osd_window),
	                                 button,
	                                 dir,
	                                 xiev->evtype == XI_ButtonPress);

	return TRUE;
}

static void
device_added_cb (GdkDeviceManager *device_manager,
                 GdkDevice        *gdk_device,
                 CsdWacomManager  *manager)
{
	CsdWacomDevice *device;
	GSettings *settings;

	device = csd_wacom_device_new (gdk_device);
	if (csd_wacom_device_get_device_type (device) == WACOM_TYPE_INVALID) {
		g_object_unref (device);
		return;
	}
	g_debug ("Adding device '%s' (type: '%s') to known devices list",
		 csd_wacom_device_get_tool_name (device),
		 csd_wacom_device_type_to_string (csd_wacom_device_get_device_type (device)));
	g_hash_table_insert (manager->priv->devices, (gpointer) gdk_device, device);

	settings = csd_wacom_device_get_settings (device);
	g_signal_connect (G_OBJECT (settings), "changed",
			  G_CALLBACK (wacom_settings_changed), device);

	if (csd_wacom_device_get_device_type (device) == WACOM_TYPE_STYLUS ||
	    csd_wacom_device_get_device_type (device) == WACOM_TYPE_ERASER) {
		GList *styli, *l;

		styli = csd_wacom_device_list_styli (device);

		for (l = styli ; l ; l = l->next) {
			settings = csd_wacom_stylus_get_settings (l->data);
			g_signal_connect (G_OBJECT (settings), "changed",
					  G_CALLBACK (stylus_settings_changed), l->data);
		}

		g_list_free (styli);

		g_signal_connect (G_OBJECT (device), "notify::last-stylus",
				  G_CALLBACK (last_stylus_changed), manager);
	}

        set_wacom_settings (manager, device);
}

static void
device_removed_cb (GdkDeviceManager *device_manager,
                   GdkDevice        *gdk_device,
                   CsdWacomManager  *manager)
{
	g_debug ("Removing device '%s' from known devices list",
		 gdk_device_get_name (gdk_device));
	g_hash_table_remove (manager->priv->devices, gdk_device);

	/* Enable this chunk of code if you want to valgrind
	 * test-wacom. It will exit when there are no Wacom devices left */
#if 0
	if (g_hash_table_size (manager->priv->devices) == 0)
		gtk_main_quit ();
#endif
}

static CsdWacomDevice *
device_id_to_device (CsdWacomManager *manager,
		     int              deviceid)
{
	GList *devices, *l;
	CsdWacomDevice *ret;

	ret = NULL;
	devices = g_hash_table_get_keys (manager->priv->devices);

	for (l = devices; l != NULL; l = l->next) {
		GdkDevice *device = l->data;
		int id;

		g_object_get (device, "device-id", &id, NULL);
		if (id == deviceid) {
			ret = g_hash_table_lookup (manager->priv->devices, device);
			break;
		}
	}

	g_list_free (devices);
	return ret;
}

struct {
	guint mask;
	KeySym keysym;
} mods_keysyms[] = {
	{ GDK_MOD1_MASK, XK_Alt_L },
	{ GDK_SHIFT_MASK, XK_Shift_L },
	{ GDK_CONTROL_MASK, XK_Control_L },
};

static void
send_modifiers (Display *display,
		guint mask,
		gboolean is_press)
{
	guint i;

	if (mask == 0)
		return;

	for (i = 0; i < G_N_ELEMENTS(mods_keysyms); i++) {
		if (mask & mods_keysyms[i].mask) {
			guint keycode;

			keycode = XKeysymToKeycode (display, mods_keysyms[i].keysym);
			XTestFakeKeyEvent (display, keycode,
					   is_press ? True : False, 0);
		}
	}
}

static char *
get_elevator_shortcut_string (GSettings        *settings,
			      GtkDirectionType  dir)
{
	char **strv, *str;

	strv = g_settings_get_strv (settings, KEY_CUSTOM_ELEVATOR_ACTION);
	if (strv == NULL)
		return NULL;

	if (g_strv_length (strv) >= 1 && dir == GTK_DIR_UP)
		str = g_strdup (strv[0]);
	else if (g_strv_length (strv) >= 2 && dir == GTK_DIR_DOWN)
		str = g_strdup (strv[1]);
	else
		str = NULL;

	g_strfreev (strv);

	return str;
}

static void
generate_key (CsdWacomTabletButton *wbutton,
	      int                   group,
	      Display              *display,
	      GtkDirectionType      dir,
	      gboolean              is_press)
{
	char                 *str;
	guint                 keyval;
	guint                *keycodes;
	guint                 keycode;
	guint                 mods;
	GdkKeymapKey         *keys;
	int                   n_keys;
	guint                 i;

	if (wbutton->type == WACOM_TABLET_BUTTON_TYPE_STRIP ||
	    wbutton->type == WACOM_TABLET_BUTTON_TYPE_RING)
		str = get_elevator_shortcut_string (wbutton->settings, dir);
	else
		str = g_settings_get_string (wbutton->settings, KEY_CUSTOM_ACTION);

	if (str == NULL)
		return;

	gtk_accelerator_parse_with_keycode (str, &keyval, &keycodes, &mods);
	if (keycodes == NULL) {
		g_warning ("Failed to find a keycode for shortcut '%s'", str);
		g_free (str);
		return;
	}
	g_free (keycodes);

	/* Now look for our own keycode, in the group as us */
	if (!gdk_keymap_get_entries_for_keyval (gdk_keymap_get_default (), keyval, &keys, &n_keys)) {
		g_warning ("Failed to find a keycode for keyval '%s' (0x%x)", gdk_keyval_name (keyval), keyval);
		g_free (str);
		return;
	}

	keycode = 0;
	for (i = 0; i < n_keys; i++) {
		if (keys[i].group != group)
			continue;
		if (keys[i].level > 0)
			continue;
		keycode = keys[i].keycode;
	}
	/* Couldn't find it in the current group? Look in group 0 */
	if (keycode == 0) {
		for (i = 0; i < n_keys; i++) {
			if (keys[i].group > 0)
				continue;
			keycode = keys[i].keycode;
		}
	}
	g_free (keys);

	if (keycode == 0) {
		g_warning ("Not emitting '%s' (keyval: %d, keycode: %d mods: 0x%x), invalid keycode",
			   str, keyval, keycode, mods);
		g_free (str);
		return;
	} else {
		g_debug ("Emitting '%s' (keyval: %d, keycode: %d mods: 0x%x)",
			 str, keyval, keycode, mods);
	}

	/* And send out the keys! */
	gdk_x11_display_error_trap_push (gdk_display_get_default ());
	if (is_press)
		send_modifiers (display, mods, TRUE);
	XTestFakeKeyEvent (display, keycode,
			   is_press ? True : False, 0);
	if (is_press == FALSE)
		send_modifiers (display, mods, FALSE);
	if (gdk_x11_display_error_trap_pop (gdk_display_get_default ()))
		g_warning ("Failed to generate fake key event '%s'", str);

	g_free (str);
}

static void
switch_monitor (CsdWacomDevice *device)
{
	gint current_monitor, n_monitors;

	/* We don't; do that for screen tablets, sorry... */
	if (csd_wacom_device_is_screen_tablet (device))
		return;

	n_monitors = gdk_screen_get_n_monitors (gdk_screen_get_default ());

	/* There's no point in switching if there just one monitor */
	if (n_monitors < 2)
		return;

	current_monitor = csd_wacom_device_get_display_monitor (device);

	/* Select next monitor */
	current_monitor++;

	if (current_monitor >= n_monitors)
		current_monitor = CSD_WACOM_SET_ALL_MONITORS;

	csd_wacom_device_set_display (device, current_monitor);
}

static const char*
get_direction_name (CsdWacomTabletButtonType type,
                    GtkDirectionType         dir)
{
        if (type == WACOM_TABLET_BUTTON_TYPE_RING)
                return (dir == GTK_DIR_UP ? " 'CCW'" : " 'CW'");

        if (type == WACOM_TABLET_BUTTON_TYPE_STRIP)
                return (dir == GTK_DIR_UP ? " 'up'" : " 'down'");

        return "";
}

static GdkFilterReturn
filter_button_events (XEvent          *xevent,
                      GdkEvent        *event,
                      CsdWacomManager *manager)
{
	XIEvent             *xiev;
	XIDeviceEvent       *xev;
	XGenericEventCookie *cookie;
	guint                deviceid;
	CsdWacomDevice      *device;
	int                  button;
	CsdWacomTabletButton *wbutton;
	GtkDirectionType      dir;
	gboolean emulate;

        /* verify we have a key event */
	if (xevent->type != GenericEvent)
		return GDK_FILTER_CONTINUE;
	cookie = &xevent->xcookie;
	if (cookie->extension != manager->priv->opcode)
		return GDK_FILTER_CONTINUE;

	xiev = (XIEvent *) xevent->xcookie.data;

	if (xiev->evtype != XI_ButtonRelease &&
	    xiev->evtype != XI_ButtonPress)
		return GDK_FILTER_CONTINUE;

	xev = (XIDeviceEvent *) xiev;

	deviceid = xev->sourceid;
	device = device_id_to_device (manager, deviceid);
	if (csd_wacom_device_get_device_type (device) != WACOM_TYPE_PAD)
		return GDK_FILTER_CONTINUE;

	if ((manager->priv->osd_window != NULL) &&
	    (device != csd_wacom_osd_window_get_device (CSD_WACOM_OSD_WINDOW(manager->priv->osd_window))))
		/* This is a button event from another device while showing OSD window */
		osd_window_destroy (manager);

	button = xev->detail;

	wbutton = csd_wacom_device_get_button (device, button, &dir);
	if (wbutton == NULL) {
		g_warning ("Could not find matching button for '%d' on '%s'",
			   button, csd_wacom_device_get_name (device));
		return GDK_FILTER_CONTINUE;
	}

	g_debug ("Received event button %s '%s'%s ('%d') on device '%s' ('%d')",
		 xiev->evtype == XI_ButtonPress ? "press" : "release",
		 wbutton->id,
		 get_direction_name (wbutton->type, dir),
		 button,
		 csd_wacom_device_get_name (device),
		 deviceid);

	if (wbutton->type == WACOM_TABLET_BUTTON_TYPE_HARDCODED) {
		int new_mode;

		/* We switch modes on key press */
		if (xiev->evtype == XI_ButtonRelease) {
			osd_window_update_viewable (manager, wbutton, dir, xiev);
			return GDK_FILTER_REMOVE;
                }
		new_mode = csd_wacom_device_set_next_mode (device, wbutton);
                if (manager->priv->osd_window != NULL) {
			csd_wacom_osd_window_set_mode (CSD_WACOM_OSD_WINDOW(manager->priv->osd_window), wbutton->group_id, new_mode);
			osd_window_update_viewable (manager, wbutton, dir, xiev);
                }
		set_led (device, wbutton, new_mode);
		return GDK_FILTER_REMOVE;
	}

	/* Update OSD window if shown */
	emulate = osd_window_update_viewable (manager, wbutton, dir, xiev);

	/* Nothing to do */
	if (g_settings_get_enum (wbutton->settings, KEY_ACTION_TYPE) == CSD_WACOM_ACTION_TYPE_NONE)
		return GDK_FILTER_REMOVE;

	/* Show OSD window when requested */
	if (g_settings_get_enum (wbutton->settings, KEY_ACTION_TYPE) == CSD_WACOM_ACTION_TYPE_HELP) {
		if (xiev->evtype == XI_ButtonRelease)
			osd_window_toggle_visibility (manager, device);
		return GDK_FILTER_REMOVE;
	}

	if (emulate)
		return GDK_FILTER_REMOVE;

	/* Switch monitor */
	if (g_settings_get_enum (wbutton->settings, KEY_ACTION_TYPE) == CSD_WACOM_ACTION_TYPE_SWITCH_MONITOR) {
		if (xiev->evtype == XI_ButtonRelease)
			switch_monitor (device);
		return GDK_FILTER_REMOVE;
	}

	/* Send a key combination out */
	generate_key (wbutton, xev->group.effective, xev->display, dir, xiev->evtype == XI_ButtonPress ? True : False);

	return GDK_FILTER_REMOVE;
}

static void
set_devicepresence_handler (CsdWacomManager *manager)
{
        GdkDeviceManager *device_manager;

        device_manager = gdk_display_get_device_manager (gdk_display_get_default ());
        if (device_manager == NULL)
                return;

        manager->priv->device_added_id = g_signal_connect (G_OBJECT (device_manager), "device-added",
                                                           G_CALLBACK (device_added_cb), manager);
        manager->priv->device_removed_id = g_signal_connect (G_OBJECT (device_manager), "device-removed",
                                                             G_CALLBACK (device_removed_cb), manager);
        manager->priv->device_manager = device_manager;
}

static void
csd_wacom_manager_init (CsdWacomManager *manager)
{
        manager->priv = CSD_WACOM_MANAGER_GET_PRIVATE (manager);
}

static gboolean
csd_wacom_manager_idle_cb (CsdWacomManager *manager)
{
	GList *devices, *l;
	GSList *ls;

        cinnamon_settings_profile_start (NULL);

        manager->priv->devices = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);

        set_devicepresence_handler (manager);

        devices = gdk_device_manager_list_devices (manager->priv->device_manager, GDK_DEVICE_TYPE_SLAVE);
        for (l = devices; l ; l = l->next)
		device_added_cb (manager->priv->device_manager, l->data, manager);
        g_list_free (devices);

        /* Start filtering the button events */
        for (ls = manager->priv->screens; ls != NULL; ls = ls->next) {
                gdk_window_add_filter (gdk_screen_get_root_window (ls->data),
                                       (GdkFilterFunc) filter_button_events,
                                       manager);
        }

        cinnamon_settings_profile_end (NULL);

        manager->priv->start_idle_id = 0;

        return FALSE;
}

/*
 * The monitors-changed signal is emitted when the number, size or
 * position of the monitors attached to the screen change.
 */
static void
on_screen_changed_cb (GnomeRRScreen *rr_screen,
		      CsdWacomManager *manager)
{
	GList *devices, *l;

        /*
         * A ::changed signal may be received at startup before
         * the devices get added, in this case simply ignore the
         * notification
         */
        if (manager->priv->devices == NULL)
                return;

        g_debug ("Screen configuration changed");
	devices = g_hash_table_get_values (manager->priv->devices);
	for (l = devices; l != NULL; l = l->next) {
		CsdWacomDevice *device = l->data;
		CsdWacomDeviceType type;
		GSettings *settings;

		type = csd_wacom_device_get_device_type (device);
		if (type == WACOM_TYPE_CURSOR || type == WACOM_TYPE_PAD)
			continue;

		settings = csd_wacom_device_get_settings (device);
		/* Ignore touch devices as they do not share the same range of values for area */
		if (type != WACOM_TYPE_TOUCH) {
			if (csd_wacom_device_is_screen_tablet (device) == FALSE)
				set_keep_aspect (device, g_settings_get_boolean (settings, KEY_KEEP_ASPECT));
			set_area (device, g_settings_get_value (settings, KEY_AREA));
		}
		set_display (device, g_settings_get_value (settings, KEY_DISPLAY));
	}
	g_list_free (devices);
}

static void
init_screens (CsdWacomManager *manager)
{
        GdkDisplay *display;
        int i;

        display = gdk_display_get_default ();
        for (i = 0; i < gdk_display_get_n_screens (display); i++) {
                GError *error = NULL;
                GdkScreen *screen;
                GnomeRRScreen *rr_screen;

                screen = gdk_display_get_screen (display, i);
                if (screen == NULL) {
                        continue;
                }
                manager->priv->screens = g_slist_append (manager->priv->screens, screen);

		/*
		 * We also keep a list of GnomeRRScreen to monitor changes such as rotation
		 * which are not reported by Gdk's "monitors-changed" callback
		 */
		rr_screen = gnome_rr_screen_new (screen, &error);
		if (rr_screen == NULL) {
			g_warning ("Failed to create GnomeRRScreen: %s", error->message);
			g_error_free (error);
			continue;
		}
		manager->priv->rr_screens = g_list_prepend (manager->priv->rr_screens, rr_screen);
		g_signal_connect (rr_screen,
				  "changed",
				  G_CALLBACK (on_screen_changed_cb),
			          manager);
        }
}

gboolean
csd_wacom_manager_start (CsdWacomManager *manager,
                         GError         **error)
{
        cinnamon_settings_profile_start (NULL);

        if (supports_xinput2_devices (&manager->priv->opcode) == FALSE) {
                g_debug ("No Xinput2 support, disabling plugin");
                return TRUE;
        }

        if (supports_xtest () == FALSE) {
                g_debug ("No XTest extension support, disabling plugin");
                return TRUE;
        }

        init_screens (manager);

        manager->priv->start_idle_id = g_idle_add ((GSourceFunc) csd_wacom_manager_idle_cb, manager);

        cinnamon_settings_profile_end (NULL);

        return TRUE;
}

void
csd_wacom_manager_stop (CsdWacomManager *manager)
{
        CsdWacomManagerPrivate *p = manager->priv;
        GSList *ls;
        GList *l;

        g_debug ("Stopping wacom manager");

        if (p->device_manager != NULL) {
                GList *devices;

                g_signal_handler_disconnect (p->device_manager, p->device_added_id);
                g_signal_handler_disconnect (p->device_manager, p->device_removed_id);

                devices = gdk_device_manager_list_devices (p->device_manager, GDK_DEVICE_TYPE_SLAVE);
                for (l = devices; l != NULL; l = l->next) {
                        CsdWacomDeviceType type;

                        type = csd_wacom_device_get_device_type (l->data);
                        if (type == WACOM_TYPE_PAD) {
                                int id;

                                id = get_device_id (l->data);
                                grab_button (id, FALSE, manager->priv->screens);
                        }
                }
                g_list_free (devices);

                p->device_manager = NULL;
        }

        for (ls = p->screens; ls != NULL; ls = ls->next) {
                gdk_window_remove_filter (gdk_screen_get_root_window (ls->data),
                                          (GdkFilterFunc) filter_button_events,
                                          manager);
        }

	for (l = p->rr_screens; l != NULL; l = l->next)
		g_signal_handlers_disconnect_by_func (l->data, on_screen_changed_cb, manager);

        g_clear_pointer (&p->osd_window, gtk_widget_destroy);
}

static void
csd_wacom_manager_finalize (GObject *object)
{
        CsdWacomManager *wacom_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_WACOM_MANAGER (object));

        wacom_manager = CSD_WACOM_MANAGER (object);

        g_return_if_fail (wacom_manager->priv != NULL);

        if (wacom_manager->priv->devices) {
                g_hash_table_destroy (wacom_manager->priv->devices);
                wacom_manager->priv->devices = NULL;
        }

        if (wacom_manager->priv->screens != NULL) {
                g_slist_free (wacom_manager->priv->screens);
                wacom_manager->priv->screens = NULL;
        }

	if (wacom_manager->priv->rr_screens != NULL) {
		g_list_free_full (wacom_manager->priv->rr_screens, g_object_unref);
		wacom_manager->priv->rr_screens = NULL;
	}

        if (wacom_manager->priv->start_idle_id != 0) {
            g_source_remove (wacom_manager->priv->start_idle_id);
            wacom_manager->priv->start_idle_id = 0;
        }

        G_OBJECT_CLASS (csd_wacom_manager_parent_class)->finalize (object);
}

CsdWacomManager *
csd_wacom_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CSD_TYPE_WACOM_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return CSD_WACOM_MANAGER (manager_object);
}

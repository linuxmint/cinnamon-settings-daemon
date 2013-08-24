/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2010,2011 Red Hat, Inc.
 *
 * Author: Bastien Nocera <hadess@hadess.net>
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

#include <fcntl.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gudev/gudev.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libcinnamon-desktop/gnome-rr.h>

#include "csd-input-helper.h"
#include "cinnamon-settings-profile.h"
#include "csd-orientation-manager.h"

typedef enum {
        ORIENTATION_UNDEFINED,
        ORIENTATION_NORMAL,
        ORIENTATION_BOTTOM_UP,
        ORIENTATION_LEFT_UP,
        ORIENTATION_RIGHT_UP
} OrientationUp;

#define CSD_ORIENTATION_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CSD_TYPE_ORIENTATION_MANAGER, CsdOrientationManagerPrivate))

struct CsdOrientationManagerPrivate
{
        guint start_idle_id;

        /* Accelerometer */
        char *sysfs_path;
        OrientationUp prev_orientation;

        /* DBus */
        GDBusNodeInfo   *introspection_data;
        GDBusConnection *connection;
        GDBusProxy      *xrandr_proxy;
        GCancellable    *cancellable;

        /* Notifications */
        GUdevClient *client;
        GSettings *settings;
        gboolean orientation_lock;
};

#define CONF_SCHEMA "org.cinnamon.settings-daemon.peripherals.touchscreen"
#define ORIENTATION_LOCK_KEY "orientation-lock"

#define CSD_DBUS_PATH "/org/cinnamon/SettingsDaemon"
#define CSD_ORIENTATION_DBUS_PATH CSD_DBUS_PATH "/Orientation"

static const gchar introspection_xml[] =
"<node>"
"  <interface name='org.cinnamon.SettingsDaemon.Orientation'>"
"    <annotation name='org.freedesktop.DBus.GLib.CSymbol' value='csd_orientation_manager'/>"
"  </interface>"
"</node>";

static void     csd_orientation_manager_class_init  (CsdOrientationManagerClass *klass);
static void     csd_orientation_manager_init        (CsdOrientationManager      *orientation_manager);
static void     csd_orientation_manager_finalize    (GObject                    *object);

G_DEFINE_TYPE (CsdOrientationManager, csd_orientation_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

#define MPU_THRESHOLD 12000
#define MPU_POLL_INTERVAL 1

static gboolean is_mpu6050 = FALSE;
static char *mpu6050_accel_x = NULL;
static char *mpu6050_accel_y = NULL;
static gboolean mpu_timer(CsdOrientationManager *manager);

static GObject *
csd_orientation_manager_constructor (GType                     type,
                               guint                      n_construct_properties,
                               GObjectConstructParam     *construct_properties)
{
        CsdOrientationManager      *orientation_manager;

        orientation_manager = CSD_ORIENTATION_MANAGER (G_OBJECT_CLASS (csd_orientation_manager_parent_class)->constructor (type,
                                                                                                         n_construct_properties,
                                                                                                         construct_properties));

        return G_OBJECT (orientation_manager);
}

static void
csd_orientation_manager_dispose (GObject *object)
{
        G_OBJECT_CLASS (csd_orientation_manager_parent_class)->dispose (object);
}

static void
csd_orientation_manager_class_init (CsdOrientationManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = csd_orientation_manager_constructor;
        object_class->dispose = csd_orientation_manager_dispose;
        object_class->finalize = csd_orientation_manager_finalize;

        g_type_class_add_private (klass, sizeof (CsdOrientationManagerPrivate));
}

static void
csd_orientation_manager_init (CsdOrientationManager *manager)
{
        manager->priv = CSD_ORIENTATION_MANAGER_GET_PRIVATE (manager);
        manager->priv->prev_orientation = ORIENTATION_UNDEFINED;
}

static GnomeRRRotation
orientation_to_rotation (OrientationUp    orientation)
{
        switch (orientation) {
        case ORIENTATION_NORMAL:
                return GNOME_RR_ROTATION_0;
        case ORIENTATION_BOTTOM_UP:
                return GNOME_RR_ROTATION_180;
        case ORIENTATION_LEFT_UP:
                return GNOME_RR_ROTATION_90;
        case ORIENTATION_RIGHT_UP:
                return GNOME_RR_ROTATION_270;
        default:
                g_assert_not_reached ();
        }
}

static OrientationUp
orientation_from_string (const char *orientation)
{
        if (g_strcmp0 (orientation, "normal") == 0)
                return ORIENTATION_NORMAL;
        if (g_strcmp0 (orientation, "bottom-up") == 0)
                return ORIENTATION_BOTTOM_UP;
        if (g_strcmp0 (orientation, "left-up") == 0)
                return ORIENTATION_LEFT_UP;
        if (g_strcmp0 (orientation, "right-up") == 0)
                return ORIENTATION_RIGHT_UP;

        return ORIENTATION_UNDEFINED;
}

static const char *
orientation_to_string (OrientationUp o)
{
        switch (o) {
        case ORIENTATION_UNDEFINED:
                return "undefined";
        case ORIENTATION_NORMAL:
                return "normal";
        case ORIENTATION_BOTTOM_UP:
                return "bottom-up";
        case ORIENTATION_LEFT_UP:
                return "left-up";
        case ORIENTATION_RIGHT_UP:
                return "right-up";
        default:
                g_assert_not_reached ();
        }
}

static OrientationUp
get_orientation_from_device (GUdevDevice *dev)
{
        const char *value;

        value = g_udev_device_get_property (dev, "ID_INPUT_ACCELEROMETER_ORIENTATION");
        if (value == NULL) {
                g_debug ("Couldn't find orientation for accelerometer %s",
                         g_udev_device_get_sysfs_path (dev));
                return ORIENTATION_UNDEFINED;
        }
        g_debug ("Found orientation '%s' for accelerometer %s",
                 value, g_udev_device_get_sysfs_path (dev));

        return orientation_from_string (value);
}

static void
on_xrandr_action_call_finished (GObject               *source_object,
                                GAsyncResult          *res,
                                CsdOrientationManager *manager)
{
        GError *error = NULL;
        GVariant *variant;

        variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);

        g_object_unref (manager->priv->cancellable);
        manager->priv->cancellable = NULL;

        if (error != NULL) {
                g_warning ("Unable to call 'RotateTo': %s", error->message);
                g_error_free (error);
        } else {
                g_variant_unref (variant);
        }
}

static void
do_xrandr_action (CsdOrientationManager *manager,
                  GnomeRRRotation        rotation)
{
        CsdOrientationManagerPrivate *priv = manager->priv;
        GTimeVal tv;
        gint64 timestamp;

        if (priv->connection == NULL || priv->xrandr_proxy == NULL) {
                g_warning ("No existing D-Bus connection trying to handle XRANDR keys");
                return;
        }

        if (priv->cancellable != NULL) {
                g_debug ("xrandr action already in flight");
                return;
        }

        g_get_current_time (&tv);
        timestamp = tv.tv_sec * 1000 + tv.tv_usec / 1000;

        priv->cancellable = g_cancellable_new ();

        g_dbus_proxy_call (priv->xrandr_proxy,
                           "RotateTo",
                           g_variant_new ("(ix)", rotation, timestamp),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           priv->cancellable,
                           (GAsyncReadyCallback) on_xrandr_action_call_finished,
                           manager);
}

static void
do_rotation (CsdOrientationManager *manager)
{
        GnomeRRRotation rotation;

        if (manager->priv->orientation_lock) {
                g_debug ("Orientation changed, but we are locked");
                return;
        }
        if (manager->priv->prev_orientation == ORIENTATION_UNDEFINED) {
                g_debug ("Not trying to rotate, orientation is undefined");
                return;
        }

        rotation = orientation_to_rotation (manager->priv->prev_orientation);

        do_xrandr_action (manager, rotation);
}

static void
client_uevent_cb (GUdevClient           *client,
                  gchar                 *action,
                  GUdevDevice           *device,
                  CsdOrientationManager *manager)
{
        const char *sysfs_path;
        OrientationUp orientation;

        sysfs_path = g_udev_device_get_sysfs_path (device);
        g_debug ("Received uevent '%s' from '%s'", action, sysfs_path);

        if (manager->priv->orientation_lock)
                return;

        if (g_str_equal (action, "change") == FALSE)
                return;

        if (g_strcmp0 (manager->priv->sysfs_path, sysfs_path) != 0)
                return;

        g_debug ("Received an event from the accelerometer");

        orientation = get_orientation_from_device (device);
        if (orientation != manager->priv->prev_orientation) {
                manager->priv->prev_orientation = orientation;
                g_debug ("Orientation changed to '%s', switching screen rotation",
                         orientation_to_string (manager->priv->prev_orientation));

                do_rotation (manager);
        }
}

static void
orientation_lock_changed_cb (GSettings             *settings,
                             gchar                 *key,
                             CsdOrientationManager *manager)
{
        gboolean new;

        new = g_settings_get_boolean (settings, key);
        if (new == manager->priv->orientation_lock)
                return;

        manager->priv->orientation_lock = new;
	
        if (new == FALSE) {
                if (is_mpu6050) {
                        g_timeout_add_seconds(MPU_POLL_INTERVAL, (GSourceFunc) mpu_timer, manager);
                }
                /* Handle the rotations that could have occurred while
                 * we were locked */
                do_rotation (manager);
        }
}

static void
xrandr_ready_cb (GObject               *source_object,
                 GAsyncResult          *res,
                 CsdOrientationManager *manager)
{
        GError *error = NULL;

        manager->priv->xrandr_proxy = g_dbus_proxy_new_finish (res, &error);
        if (manager->priv->xrandr_proxy == NULL) {
                g_warning ("Failed to get proxy for XRandR operations: %s", error->message);
                g_error_free (error);
        }
}

static void
on_bus_gotten (GObject               *source_object,
               GAsyncResult          *res,
               CsdOrientationManager *manager)
{
        GDBusConnection *connection;
        GError *error = NULL;

        connection = g_bus_get_finish (res, &error);
        if (connection == NULL) {
                g_warning ("Could not get session bus: %s", error->message);
                g_error_free (error);
                return;
        }
        manager->priv->connection = connection;

        g_dbus_connection_register_object (connection,
                                           CSD_ORIENTATION_DBUS_PATH,
                                           manager->priv->introspection_data->interfaces[0],
                                           NULL,
                                           NULL,
                                           NULL,
                                           NULL);

        g_dbus_proxy_new (manager->priv->connection,
                          G_DBUS_PROXY_FLAGS_NONE,
                          NULL,
                          "org.cinnamon.SettingsDaemon",
                          "/org/cinnamon/SettingsDaemon/XRANDR",
                          "org.cinnamon.SettingsDaemon.XRANDR_2",
                          NULL,
                          (GAsyncReadyCallback) xrandr_ready_cb,
                          manager);
}

static GUdevDevice *
get_accelerometer (GUdevClient *client)
{
        GList *list, *listiio, *l;
        GUdevDevice *ret, *parent;

        /* Look for a device with the ID_INPUT_ACCELEROMETER=1 property */
        ret = NULL;
        list = g_udev_client_query_by_subsystem (client, "input");
        listiio = g_udev_client_query_by_subsystem (client, "iio");
        list = g_list_concat(list, listiio);
        for (l = list; l != NULL; l = l->next) {
                GUdevDevice *dev;

                dev = l->data;
                if (g_udev_device_get_property_as_boolean (dev, "ID_INPUT_ACCELEROMETER")) {
                        ret = dev;
                        continue;
                }
                g_object_unref (dev);
        }
        g_list_free (list);

        if (ret == NULL)
                return NULL;

        /* Now walk up to the parent */
        parent = g_udev_device_get_parent (ret);
        if (parent == NULL)
                return ret;

        if (g_udev_device_get_property_as_boolean (parent, "ID_INPUT_ACCELEROMETER")) {
                g_object_unref (ret);
                ret = parent;
        } else {
                g_object_unref (parent);
        }

        return ret;
}

static int read_sysfs_attr_as_int(const char *filename) {
	int i, c;
	char buf[40];
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		return 0;
	c = read(fd, buf, 40);
	if (c < 0)
		return 0;
	close(fd);
	sscanf(buf, "%d", &i);
	
	return i;
}

static gboolean mpu_timer(CsdOrientationManager *manager) {
	int x, y;
	static gboolean first = TRUE;
	OrientationUp orientation = manager->priv->prev_orientation;

        if (manager->priv->xrandr_proxy == NULL)
                return TRUE;

	x = read_sysfs_attr_as_int(mpu6050_accel_x);
	y = read_sysfs_attr_as_int(mpu6050_accel_y);

	if (x > MPU_THRESHOLD)
		orientation = ORIENTATION_NORMAL;
	if (x < -MPU_THRESHOLD)
		orientation = ORIENTATION_BOTTOM_UP;
	if (y > MPU_THRESHOLD)
		orientation = ORIENTATION_RIGHT_UP;
	if (y < -MPU_THRESHOLD)
		orientation = ORIENTATION_LEFT_UP;

        if (orientation != manager->priv->prev_orientation || first) {
                first = FALSE;
                manager->priv->prev_orientation = orientation;
                g_debug ("Orientation changed to '%s', switching screen rotation",
                         orientation_to_string (manager->priv->prev_orientation));

                do_rotation (manager);
        }

        return !manager->priv->orientation_lock;
}

static gboolean
csd_orientation_manager_idle_cb (CsdOrientationManager *manager)
{
        const char * const subsystems[] = { "input", NULL };
        GUdevDevice *dev;

        cinnamon_settings_profile_start (NULL);

        manager->priv->settings = g_settings_new (CONF_SCHEMA);
        manager->priv->orientation_lock = g_settings_get_boolean (manager->priv->settings, ORIENTATION_LOCK_KEY);
        g_signal_connect (G_OBJECT (manager->priv->settings), "changed::orientation-lock",
                          G_CALLBACK (orientation_lock_changed_cb), manager);

        manager->priv->client = g_udev_client_new (subsystems);
        dev = get_accelerometer (manager->priv->client);
        if (dev == NULL) {
                g_debug ("Did not find an accelerometer");
                cinnamon_settings_profile_end (NULL);
                return FALSE;
        }
        manager->priv->sysfs_path = g_strdup (g_udev_device_get_sysfs_path (dev));
        g_debug ("Found accelerometer at sysfs path '%s'", manager->priv->sysfs_path);

        manager->priv->prev_orientation = get_orientation_from_device (dev);

        /* Poll the sysfs attributes exposed by MPU6050 as it is not an uevent based input driver */
        if (g_strcmp0 (g_udev_device_get_sysfs_attr (dev, "name"), "mpu6050") == 0) {
                manager->priv->prev_orientation = ORIENTATION_NORMAL;
                g_timeout_add_seconds(MPU_POLL_INTERVAL, (GSourceFunc) mpu_timer, manager);
                mpu6050_accel_x = g_build_filename(manager->priv->sysfs_path, "in_accel_x_raw", NULL);
                mpu6050_accel_y = g_build_filename(manager->priv->sysfs_path, "in_accel_y_raw", NULL);
                is_mpu6050 = TRUE;
        }

        g_object_unref (dev);

        /* Start process of owning a D-Bus name */
        g_bus_get (G_BUS_TYPE_SESSION,
                   NULL,
                   (GAsyncReadyCallback) on_bus_gotten,
                   manager);

        g_signal_connect (G_OBJECT (manager->priv->client), "uevent",
                          G_CALLBACK (client_uevent_cb), manager);

        cinnamon_settings_profile_end (NULL);

        return FALSE;
}

gboolean
csd_orientation_manager_start (CsdOrientationManager *manager,
                         GError         **error)
{
        cinnamon_settings_profile_start (NULL);

        manager->priv->start_idle_id = g_idle_add ((GSourceFunc) csd_orientation_manager_idle_cb, manager);

        manager->priv->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
        g_assert (manager->priv->introspection_data != NULL);

        cinnamon_settings_profile_end (NULL);

        return TRUE;
}

void
csd_orientation_manager_stop (CsdOrientationManager *manager)
{
        CsdOrientationManagerPrivate *p = manager->priv;

        g_debug ("Stopping orientation manager");

        if (p->settings) {
                g_object_unref (p->settings);
                p->settings = NULL;
        }

        if (p->sysfs_path) {
                g_free (p->sysfs_path);
                p->sysfs_path = NULL;
        }

        if (p->introspection_data) {
                g_dbus_node_info_unref (p->introspection_data);
                p->introspection_data = NULL;
        }

        if (p->client) {
                g_object_unref (p->client);
                p->client = NULL;
        }
}

static void
csd_orientation_manager_finalize (GObject *object)
{
        CsdOrientationManager *orientation_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_ORIENTATION_MANAGER (object));

        orientation_manager = CSD_ORIENTATION_MANAGER (object);

        g_return_if_fail (orientation_manager->priv != NULL);

        if (orientation_manager->priv->start_idle_id != 0)
                g_source_remove (orientation_manager->priv->start_idle_id);

        G_OBJECT_CLASS (csd_orientation_manager_parent_class)->finalize (object);
}

CsdOrientationManager *
csd_orientation_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CSD_TYPE_ORIENTATION_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return CSD_ORIENTATION_MANAGER (manager_object);
}

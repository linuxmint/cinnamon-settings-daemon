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

        /* Accelerometer */
        guint          watch_id;
        GDBusProxy    *iio_proxy;
        gboolean       has_accel;
        OrientationUp  prev_orientation;

        /* DBus */
        GDBusProxy      *xrandr_proxy;
        GCancellable    *cancellable;

        /* Notifications */
        GSettings *settings;
        gboolean orientation_lock;
};

#define CONF_SCHEMA "org.cinnamon.settings-daemon.peripherals.touchscreen"
#define ORIENTATION_LOCK_KEY "orientation-lock"

static void     csd_orientation_manager_finalize    (GObject                    *object);

G_DEFINE_TYPE (CsdOrientationManager, csd_orientation_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

#define MPU_THRESHOLD 12000
#define MPU_POLL_INTERVAL 1

static gboolean is_mpu6050 = FALSE;
static char *mpu6050_accel_x = NULL;
static char *mpu6050_accel_y = NULL;
static gboolean mpu_timer(CsdOrientationManager *manager);

static void
csd_orientation_manager_class_init (CsdOrientationManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

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
get_orientation_from_device (CsdOrientationManager *manager)
{
        GVariant *v;
        OrientationUp o;

        v = g_dbus_proxy_get_cached_property (manager->priv->iio_proxy, "AccelerometerOrientation");
        if (v == NULL) {
                g_debug ("Couldn't find orientation for accelerometer");
                return ORIENTATION_UNDEFINED;
        }
        g_debug ("Found orientation '%s' for accelerometer", g_variant_get_string (v, NULL));

        o = orientation_from_string (g_variant_get_string (v, NULL));
        g_variant_unref (v);
        return o;
}

static void
on_xrandr_action_call_finished (GObject               *source_object,
                                GAsyncResult          *res,
                                CsdOrientationManager *manager)
{
        GError *error = NULL;
        GVariant *variant;

        variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);

        g_clear_object (&manager->priv->cancellable);

        if (variant == NULL) {
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

        if (priv->xrandr_proxy == NULL) {
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
orientation_lock_changed_cb (GSettings             *settings,
                             gchar                 *key,
                             CsdOrientationManager *manager)
{
        gboolean new;

        new = g_settings_get_boolean (settings, ORIENTATION_LOCK_KEY);
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
properties_changed (GDBusProxy *proxy,
                    GVariant   *changed_properties,
                    GStrv       invalidated_properties,
                    gpointer    user_data)
{
        CsdOrientationManager *manager = user_data;
        CsdOrientationManagerPrivate *p = manager->priv;
        GVariant *v;
        GVariantDict dict;

        if (manager->priv->xrandr_proxy == NULL)
                return;

        if (changed_properties)
                g_variant_dict_init (&dict, changed_properties);

        if (changed_properties == NULL ||
            g_variant_dict_contains (&dict, "HasAccelerometer")) {
                v = g_dbus_proxy_get_cached_property (p->iio_proxy, "HasAccelerometer");
                if (v == NULL) {
                        g_debug ("Couldn't fetch HasAccelerometer property");
                        return;
                }
                p->has_accel = g_variant_get_boolean (v);
                if (!p->has_accel)
                        p->prev_orientation = ORIENTATION_UNDEFINED;
                g_variant_unref (v);
        }

        if (changed_properties == NULL ||
            g_variant_dict_contains (&dict, "AccelerometerOrientation")) {
                if (p->has_accel) {
                        OrientationUp orientation;

                        orientation = get_orientation_from_device (manager);
                        if (orientation != p->prev_orientation) {
                                p->prev_orientation = orientation;
                                g_debug ("Orientation changed to '%s', switching screen rotation",
                                         orientation_to_string (p->prev_orientation));

                                do_rotation (manager);
                        }
                }
        }
}

static void
xrandr_ready_cb (GObject               *source_object,
                 GAsyncResult          *res,
                 CsdOrientationManager *manager)
{
        CsdOrientationManagerPrivate *p = manager->priv;
        GError *error = NULL;

        manager->priv->xrandr_proxy = g_dbus_proxy_new_finish (res, &error);
        if (manager->priv->xrandr_proxy == NULL) {
                g_warning ("Failed to get proxy for XRandR operations: %s", error->message);
                g_error_free (error);
        }

        if (p->iio_proxy == NULL)
                return;

        properties_changed (manager->priv->iio_proxy, NULL, NULL, manager);
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

static void
iio_sensor_appeared_cb (GDBusConnection *connection,
                        const gchar     *name,
                        const gchar     *name_owner,
                        gpointer         user_data)
{
        CsdOrientationManager *manager = user_data;
        CsdOrientationManagerPrivate *p = manager->priv;
        GError *error = NULL;

        p->iio_proxy = g_dbus_proxy_new_sync (connection,
                                              G_DBUS_PROXY_FLAGS_NONE,
                                              NULL,
                                              "net.hadess.SensorProxy",
                                              "/net/hadess/SensorProxy",
                                              "net.hadess.SensorProxy",
                                              NULL,
                                              &error);

        if (p->iio_proxy == NULL) {
                g_warning ("Failed to access net.hadess.SensorProxy after it appeared");
                return;
        }

        g_dbus_proxy_call_sync (p->iio_proxy,
                                "ClaimAccelerometer",
                                NULL,
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                NULL, NULL);

        g_signal_connect (G_OBJECT (manager->priv->iio_proxy), "g-properties-changed",
                          G_CALLBACK (properties_changed), manager);

        properties_changed (manager->priv->iio_proxy, NULL, NULL, manager);
}

static void
iio_sensor_vanished_cb (GDBusConnection *connection,
                        const gchar     *name,
                        gpointer         user_data)
{
        CsdOrientationManager *manager = user_data;

        g_clear_object (&manager->priv->iio_proxy);
        manager->priv->has_accel = FALSE;
        manager->priv->prev_orientation = ORIENTATION_UNDEFINED;
}

gboolean
csd_orientation_manager_start (CsdOrientationManager *manager,
                         GError         **error)
{
        cinnamon_settings_profile_start (NULL);

        manager->priv->settings = g_settings_new (CONF_SCHEMA);
        g_signal_connect (G_OBJECT (manager->priv->settings), "changed::" ORIENTATION_LOCK_KEY,
                          G_CALLBACK (orientation_lock_changed_cb), manager);
        manager->priv->orientation_lock = g_settings_get_boolean (manager->priv->settings, ORIENTATION_LOCK_KEY);

        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.cinnamon.SettingsDaemon.XRANDR_2",
                                  "/org/cinnamon/SettingsDaemon/XRANDR",
                                  "org.cinnamon.SettingsDaemon.XRANDR_2",
                                  NULL,
                                  (GAsyncReadyCallback) xrandr_ready_cb,
                                  manager);

        manager->priv->watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                                    "net.hadess.SensorProxy",
                                                    G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                    iio_sensor_appeared_cb,
                                                    iio_sensor_vanished_cb,
                                                    manager,
                                                    NULL);

        cinnamon_settings_profile_end (NULL);

        return TRUE;
}

void
csd_orientation_manager_stop (CsdOrientationManager *manager)
{
        CsdOrientationManagerPrivate *p = manager->priv;

        g_debug ("Stopping orientation manager");

        if (p->watch_id > 0) {
                g_bus_unwatch_name (p->watch_id);
                p->watch_id = 0;
        }

        if (p->iio_proxy) {
                g_dbus_proxy_call_sync (p->iio_proxy,
                                        "ReleaseAccelerometer",
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL, NULL);
                g_clear_object (&p->iio_proxy);
        }

        g_clear_object (&p->xrandr_proxy);
        g_clear_object (&p->settings);
        p->has_accel = FALSE;

        if (p->cancellable) {
                g_cancellable_cancel (p->cancellable);
                g_clear_object (&p->cancellable);
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

        csd_orientation_manager_stop (orientation_manager);

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

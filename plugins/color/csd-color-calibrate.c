/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2011-2013 Richard Hughes <richard@hughsie.com>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <glib/gi18n.h>
#include <colord.h>
#include <libnotify/notify.h>
#include <canberra-gtk.h>

#include "csd-color-calibrate.h"

#define CCM_SESSION_NOTIFY_TIMEOUT                      30000 /* ms */
#define CCM_SETTINGS_RECALIBRATE_PRINTER_THRESHOLD      "recalibrate-printer-threshold"
#define CCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD      "recalibrate-display-threshold"

struct _CsdColorCalibrate
{
        GObject          parent;

        CdClient        *client;
        GSettings       *settings;
};

static void     csd_color_calibrate_class_init  (CsdColorCalibrateClass *klass);
static void     csd_color_calibrate_init        (CsdColorCalibrate      *color_calibrate);
static void     csd_color_calibrate_finalize    (GObject             *object);

G_DEFINE_TYPE (CsdColorCalibrate, csd_color_calibrate, G_TYPE_OBJECT)

typedef struct {
        CsdColorCalibrate       *calibrate;
        CdProfile               *profile;
        CdDevice                *device;
        guint32                  output_id;
} CcmSessionAsyncHelper;

static void
ccm_session_async_helper_free (CcmSessionAsyncHelper *helper)
{
        if (helper->calibrate != NULL)
                g_object_unref (helper->calibrate);
        if (helper->profile != NULL)
                g_object_unref (helper->profile);
        if (helper->device != NULL)
                g_object_unref (helper->device);
        g_free (helper);
}

static void
ccm_session_exec_control_center (CsdColorCalibrate *calibrate)
{
        gboolean ret;
        GError *error = NULL;
        GAppInfo *app_info;
        GdkAppLaunchContext *launch_context;

        /* setup the launch context so the startup notification is correct */
        launch_context = gdk_display_get_app_launch_context (gdk_display_get_default ());
        app_info = g_app_info_create_from_commandline (BINDIR "/gnome-control-center color",
                                                       "gnome-control-center",
                                                       G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION,
                                                       &error);
        if (app_info == NULL) {
                g_warning ("failed to create application info: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* launch gnome-control-center */
        ret = g_app_info_launch (app_info,
                                 NULL,
                                 G_APP_LAUNCH_CONTEXT (launch_context),
                                 &error);
        if (!ret) {
                g_warning ("failed to launch gnome-control-center: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }
out:
        g_object_unref (launch_context);
        if (app_info != NULL)
                g_object_unref (app_info);
}

static void
ccm_session_notify_cb (NotifyNotification *notification,
                       gchar *action,
                       gpointer user_data)
{
        CsdColorCalibrate *calibrate = CSD_COLOR_CALIBRATE (user_data);

        if (g_strcmp0 (action, "recalibrate") == 0) {
                notify_notification_close (notification, NULL);
                ccm_session_exec_control_center (calibrate);
        }
}

static void
closed_cb (NotifyNotification *notification, gpointer data)
{
        g_object_unref (notification);
}

static gboolean
ccm_session_notify_recalibrate (CsdColorCalibrate *calibrate,
                                const gchar *title,
                                const gchar *message,
                                CdDeviceKind kind)
{
        gboolean ret;
        GError *error = NULL;
        NotifyNotification *notification;

        /* show a bubble */
        notification = notify_notification_new (title, message, "preferences-color");
        notify_notification_set_timeout (notification, CCM_SESSION_NOTIFY_TIMEOUT);
        notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);
        notify_notification_set_app_name (notification, _("Color"));
        notify_notification_set_hint_string (notification, "desktop-entry", "gnome-color-panel");

        notify_notification_add_action (notification,
                                        "recalibrate",
                                        /* TRANSLATORS: button: this is to open CCM */
                                        _("Recalibrate now"),
                                        ccm_session_notify_cb,
                                        calibrate, NULL);

        g_signal_connect (notification, "closed", G_CALLBACK (closed_cb), NULL);
        ret = notify_notification_show (notification, &error);
        if (!ret) {
                g_warning ("failed to show notification: %s",
                           error->message);
                g_error_free (error);
        }
        return ret;
}

static gchar *
ccm_session_device_get_title (CdDevice *device)
{
        const gchar *vendor;
        const gchar *model;

        model = cd_device_get_model (device);
        vendor = cd_device_get_vendor (device);
        if (model != NULL && vendor != NULL)
                return g_strdup_printf ("%s - %s", vendor, model);
        if (vendor != NULL)
                return g_strdup (vendor);
        if (model != NULL)
                return g_strdup (model);
        return g_strdup (cd_device_get_id (device));
}

static void
ccm_session_notify_device (CsdColorCalibrate *calibrate, CdDevice *device)
{
        CdDeviceKind kind;
        const gchar *title;
        gchar *device_title = NULL;
        gchar *message;
        guint threshold;
        glong since;

        /* TRANSLATORS: this is when the device has not been recalibrated in a while */
        title = _("Recalibration required");
        device_title = ccm_session_device_get_title (device);

        /* check we care */
        kind = cd_device_get_kind (device);
        if (kind == CD_DEVICE_KIND_DISPLAY) {

                /* get from GSettings */
                threshold = g_settings_get_uint (calibrate->settings,
                                                 CCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD);

                /* TRANSLATORS: this is when the display has not been recalibrated in a while */
                message = g_strdup_printf (_("The display “%s” should be recalibrated soon."),
                                           device_title);
        } else {

                /* get from GSettings */
                threshold = g_settings_get_uint (calibrate->settings,
                                                 CCM_SETTINGS_RECALIBRATE_PRINTER_THRESHOLD);

                /* TRANSLATORS: this is when the printer has not been recalibrated in a while */
                message = g_strdup_printf (_("The printer “%s” should be recalibrated soon."),
                                           device_title);
        }

        /* check if we need to notify */
        since = (g_get_real_time () - cd_device_get_modified (device)) / G_USEC_PER_SEC;
        if (threshold > since)
                ccm_session_notify_recalibrate (calibrate, title, message, kind);
        g_free (device_title);
        g_free (message);
}

static void
ccm_session_profile_connect_cb (GObject *object,
                                GAsyncResult *res,
                                gpointer user_data)
{
        const gchar *filename;
        gboolean ret;
        gchar *basename = NULL;
        const gchar *data_source;
        GError *error = NULL;
        CdProfile *profile = CD_PROFILE (object);
        CcmSessionAsyncHelper *helper = (CcmSessionAsyncHelper *) user_data;
        CsdColorCalibrate *calibrate = CSD_COLOR_CALIBRATE (helper->calibrate);

        ret = cd_profile_connect_finish (profile,
                                         res,
                                         &error);
        if (!ret) {
                g_warning ("failed to connect to profile: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* ensure it's a profile generated by us */
        data_source = cd_profile_get_metadata_item (profile,
                                                    CD_PROFILE_METADATA_DATA_SOURCE);
        if (data_source == NULL) {

                /* existing profiles from gnome-color-calibrate < 3.1
                 * won't have the extra metadata values added */
                filename = cd_profile_get_filename (profile);
                if (filename == NULL)
                        goto out;
                basename = g_path_get_basename (filename);
                if (!g_str_has_prefix (basename, "CCM")) {
                        g_debug ("not a CCM profile for %s: %s",
                                 cd_device_get_id (helper->device), filename);
                        goto out;
                }

        /* ensure it's been created from a calibration, rather than from
         * auto-EDID */
        } else if (g_strcmp0 (data_source,
                   CD_PROFILE_METADATA_DATA_SOURCE_CALIB) != 0) {
                g_debug ("not a calib profile for %s",
                         cd_device_get_id (helper->device));
                goto out;
        }

        /* handle device */
        ccm_session_notify_device (calibrate, helper->device);
out:
        ccm_session_async_helper_free (helper);
        g_free (basename);
}

static void
ccm_session_device_connect_cb (GObject *object,
                               GAsyncResult *res,
                               gpointer user_data)
{
        gboolean ret;
        GError *error = NULL;
        CdDeviceKind kind;
        CdProfile *profile = NULL;
        CdDevice *device = CD_DEVICE (object);
        CsdColorCalibrate *calibrate = CSD_COLOR_CALIBRATE (user_data);
        CcmSessionAsyncHelper *helper;

        ret = cd_device_connect_finish (device,
                                        res,
                                        &error);
        if (!ret) {
                g_warning ("failed to connect to device: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* check we care */
        kind = cd_device_get_kind (device);
        if (kind != CD_DEVICE_KIND_DISPLAY &&
            kind != CD_DEVICE_KIND_PRINTER)
                goto out;

        /* ensure we have a profile */
        profile = cd_device_get_default_profile (device);
        if (profile == NULL) {
                g_debug ("no profile set for %s", cd_device_get_id (device));
                goto out;
        }

        /* connect to the profile */
        helper = g_new0 (CcmSessionAsyncHelper, 1);
        helper->calibrate = g_object_ref (calibrate);
        helper->device = g_object_ref (device);
        cd_profile_connect (profile,
                            NULL,
                            ccm_session_profile_connect_cb,
                            helper);
out:
        if (profile != NULL)
                g_object_unref (profile);
}

static void
ccm_session_device_added_notify_cb (CdClient *client,
                                    CdDevice *device,
                                    CsdColorCalibrate *calibrate)
{
        /* connect to the device to get properties */
        cd_device_connect (device,
                           NULL,
                           ccm_session_device_connect_cb,
                           calibrate);
}

static void
ccm_session_sensor_added_cb (CdClient *client,
                             CdSensor *sensor,
                             CsdColorCalibrate *calibrate)
{
        ca_context_play (ca_gtk_context_get (), 0,
                         CA_PROP_EVENT_ID, "device-added",
                         /* TRANSLATORS: this is the application name */
                         CA_PROP_APPLICATION_NAME, _("GNOME Settings Daemon Color Plugin"),
                        /* TRANSLATORS: this is a sound description */
                         CA_PROP_EVENT_DESCRIPTION, _("Color calibration device added"), NULL);

        /* open up the color prefs window */
        ccm_session_exec_control_center (calibrate);
}

static void
ccm_session_sensor_removed_cb (CdClient *client,
                               CdSensor *sensor,
                               CsdColorCalibrate *calibrate)
{
        ca_context_play (ca_gtk_context_get (), 0,
                         CA_PROP_EVENT_ID, "device-removed",
                         /* TRANSLATORS: this is the application name */
                         CA_PROP_APPLICATION_NAME, _("GNOME Settings Daemon Color Plugin"),
                        /* TRANSLATORS: this is a sound description */
                         CA_PROP_EVENT_DESCRIPTION, _("Color calibration device removed"), NULL);
}

static void
csd_color_calibrate_class_init (CsdColorCalibrateClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = csd_color_calibrate_finalize;
}

static void
csd_color_calibrate_init (CsdColorCalibrate *calibrate)
{
        calibrate->settings = g_settings_new ("org.gnome.settings-daemon.plugins.color");
        calibrate->client = cd_client_new ();
        g_signal_connect (calibrate->client, "device-added",
                          G_CALLBACK (ccm_session_device_added_notify_cb),
                          calibrate);
        g_signal_connect (calibrate->client, "sensor-added",
                          G_CALLBACK (ccm_session_sensor_added_cb),
                          calibrate);
        g_signal_connect (calibrate->client, "sensor-removed",
                          G_CALLBACK (ccm_session_sensor_removed_cb),
                          calibrate);
}

static void
csd_color_calibrate_finalize (GObject *object)
{
        CsdColorCalibrate *calibrate;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_COLOR_CALIBRATE (object));

        calibrate = CSD_COLOR_CALIBRATE (object);

        g_clear_object (&calibrate->settings);
        g_clear_object (&calibrate->client);

        G_OBJECT_CLASS (csd_color_calibrate_parent_class)->finalize (object);
}

CsdColorCalibrate *
csd_color_calibrate_new (void)
{
        CsdColorCalibrate *calibrate;
        calibrate = g_object_new (CSD_TYPE_COLOR_CALIBRATE, NULL);
        return CSD_COLOR_CALIBRATE (calibrate);
}

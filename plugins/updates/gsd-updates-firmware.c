/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2012 Richard Hughes <richard@hughsie.com>
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libnotify/notify.h>
#include <packagekit-glib2/packagekit.h>
#ifdef HAVE_GUDEV
#include <gudev/gudev.h>
#endif

#include "gsd-updates-common.h"
#include "gsd-updates-firmware.h"

static void     gsd_updates_firmware_finalize   (GObject          *object);

#define GSD_UPDATES_FIRMWARE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_UPDATES_TYPE_FIRMWARE, GsdUpdatesFirmwarePrivate))
#define GSD_UPDATES_FIRMWARE_MISSING_DIR                "/run/udev/firmware-missing"
#define GSD_UPDATES_FIRMWARE_LOADING_DIR                "/lib/firmware"
#define GSD_UPDATES_FIRMWARE_LOGIN_DELAY                10 /* seconds */
#define GSD_UPDATES_FIRMWARE_PROCESS_DELAY              2 /* seconds */
#define GSD_UPDATES_FIRMWARE_INSERT_DELAY               2 /* seconds */
#define GSD_UPDATES_FIRMWARE_DEVICE_REBIND_PROGRAM      "/usr/sbin/pk-device-rebind"

struct GsdUpdatesFirmwarePrivate
{
        GSettings               *settings;
        GFileMonitor            *monitor;
        GPtrArray               *array_requested;
        PkTask                  *task;
        GPtrArray               *packages_found;
        guint                    timeout_id;
};

typedef enum {
        FIRMWARE_SUBSYSTEM_USB,
        FIRMWARE_SUBSYSTEM_PCI,
        FIRMWARE_SUBSYSTEM_UNKNOWN
} FirmwareSubsystem;

typedef struct {
        gchar                   *filename;
        gchar                   *sysfs_path;
        gchar                   *model;
        gchar                   *id;
        FirmwareSubsystem        subsystem;
} GsdUpdatesFirmwareRequest;

G_DEFINE_TYPE (GsdUpdatesFirmware, gsd_updates_firmware, G_TYPE_OBJECT)

static void install_package_ids (GsdUpdatesFirmware *firmware);
static void ignore_devices (GsdUpdatesFirmware *firmware);

static gboolean
subsystem_can_replug (FirmwareSubsystem subsystem)
{
        if (subsystem == FIRMWARE_SUBSYSTEM_USB)
                return TRUE;
        return FALSE;
}

static GsdUpdatesFirmwareRequest *
request_new (const gchar *filename, const gchar *sysfs_path)
{
        GsdUpdatesFirmwareRequest *req;
#ifdef HAVE_GUDEV
        GUdevDevice *device;
        GUdevClient *client;
        const gchar *subsystem;
        const gchar *model;
        const gchar *id_vendor;
        const gchar *id_product;
#endif

        req = g_new0 (GsdUpdatesFirmwareRequest, 1);
        req->filename = g_strdup (filename);
        req->sysfs_path = g_strdup (sysfs_path);
        req->subsystem = FIRMWARE_SUBSYSTEM_UNKNOWN;
#ifdef HAVE_GUDEV

        /* get all subsystems */
        client = g_udev_client_new (NULL);
        device = g_udev_client_query_by_sysfs_path (client, sysfs_path);
        if (device == NULL)
                goto out;

        /* find subsystem, which will affect if we have to replug, or reboot */
        subsystem = g_udev_device_get_subsystem (device);
        if (g_strcmp0 (subsystem, "usb") == 0) {
                req->subsystem = FIRMWARE_SUBSYSTEM_USB;
        } else if (g_strcmp0 (subsystem, "pci") == 0) {
                req->subsystem = FIRMWARE_SUBSYSTEM_PCI;
        } else {
                g_warning ("subsystem unrecognised: %s", subsystem);
        }

        /* get model, so we can show something sensible */
        model = g_udev_device_get_property (device, "ID_MODEL");
        if (model != NULL && model[0] != '\0') {
                req->model = g_strdup (model);
                /* replace invalid chars */
                g_strdelimit (req->model, "_", ' ');
        }

        /* create ID so we can ignore the specific device */
        id_vendor = g_udev_device_get_property (device, "ID_VENDOR");
        id_product = g_udev_device_get_property (device, "ID_MODEL_ID");
        req->id = g_strdup_printf ("%s_%s", id_vendor, id_product);
out:
        if (device != NULL)
                g_object_unref (device);
        g_object_unref (client);
#endif
        return req;
}

static void
request_free (GsdUpdatesFirmwareRequest *req)
{
        g_free (req->filename);
        g_free (req->model);
        g_free (req->sysfs_path);
        g_free (req->id);
        g_free (req);
}

static gboolean
device_rebind (GsdUpdatesFirmware *firmware)
{
        gboolean ret;
        gchar *argv[4];
        gchar *rebind_stderr = NULL;
        gchar *rebind_stdout = NULL;
        GError *error = NULL;
        gint exit_status = 0;
        guint i;
        GPtrArray *array;
        const GsdUpdatesFirmwareRequest *req;
        GString *string;

        string = g_string_new ("");

        /* make a string array of all the devices to replug */
        array = firmware->priv->array_requested;
        for (i=0; i<array->len; i++) {
                req = g_ptr_array_index (array, i);
                g_string_append_printf (string, "%s ", req->sysfs_path);
        }

        /* remove trailing space */
        if (string->len > 0)
                g_string_set_size (string, string->len-1);

        /* use PolicyKit to do this as root */
        argv[0] = BINDIR "/pkexec";
        argv[1] = GSD_UPDATES_FIRMWARE_DEVICE_REBIND_PROGRAM;
        argv[2] = string->str;
        argv[3] = NULL;
        ret = g_spawn_sync (NULL,
                            argv,
                            NULL,
                            0,
                            NULL, NULL,
                            &rebind_stdout,
                            &rebind_stderr,
                            &exit_status,
                            &error);
        if (!ret) {
                g_warning ("failed to spawn '%s': %s",
                           argv[1], error->message);
                g_error_free (error);
                goto out;
        }

        /* if we failed to rebind the device */
        if (exit_status != 0) {
                g_warning ("failed to rebind: %s, %s",
                           rebind_stdout, rebind_stderr);
                ret = FALSE;
                goto out;
        }
out:
        g_free (rebind_stdout);
        g_free (rebind_stderr);
        g_string_free (string, TRUE);
        return ret;
}
static void
libnotify_cb (NotifyNotification *notification, gchar *action, gpointer data)
{
        GsdUpdatesFirmware *firmware = GSD_UPDATES_FIRMWARE (data);

        if (g_strcmp0 (action, "install-firmware") == 0) {
                install_package_ids (firmware);
        } else if (g_strcmp0 (action, "ignore-devices") == 0) {
                ignore_devices (firmware);
        } else {
                g_warning ("unknown action id: %s", action);
        }
        notify_notification_close (notification, NULL);
}

static void
on_notification_closed (NotifyNotification *notification, gpointer data)
{
        g_object_unref (notification);
}

static void
require_restart (GsdUpdatesFirmware *firmware)
{
        const gchar *message;
        gboolean ret;
        GError *error = NULL;
        NotifyNotification *notification;

        /* TRANSLATORS: we need to restart so the new hardware can re-request the firmware */
        message = _("You will need to restart this computer before the hardware will work correctly.");

        /* TRANSLATORS: title of libnotify bubble */
        notification = notify_notification_new (_("Additional software was installed"), message, NULL);
        notify_notification_set_app_name (notification, _("Software Updates"));
        notify_notification_set_timeout (notification, NOTIFY_EXPIRES_NEVER);
        notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);
        g_signal_connect (notification, "closed",
                          G_CALLBACK (on_notification_closed), NULL);

        /* show the bubble */
        ret = notify_notification_show (notification, &error);
        if (!ret) {
                g_warning ("error: %s", error->message);
                g_error_free (error);
        }
}

static void
require_replug (GsdUpdatesFirmware *firmware)
{
        const gchar *message;
        gboolean ret;
        GError *error = NULL;
        NotifyNotification *notification;

        /* TRANSLATORS: we need to remove an replug so the new hardware can re-request the firmware */
        message = _("You will need to remove and then reinsert the hardware before it will work correctly.");

        /* TRANSLATORS: title of libnotify bubble */
        notification = notify_notification_new (_("Additional software was installed"), message, NULL);
        notify_notification_set_app_name (notification, _("Software Updates"));
        notify_notification_set_timeout (notification, NOTIFY_EXPIRES_NEVER);
        notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);
        g_signal_connect (notification, "closed",
                          G_CALLBACK (on_notification_closed), NULL);

        /* show the bubble */
        ret = notify_notification_show (notification, &error);
        if (!ret) {
                g_warning ("error: %s", error->message);
                g_error_free (error);
        }
}

static void
require_nothing (GsdUpdatesFirmware *firmware)
{
        const gchar *message;
        gboolean ret;
        GError *error = NULL;
        NotifyNotification *notification;

        /* TRANSLATORS: we need to remove an replug so the new hardware can re-request the firmware */
        message = _("Your hardware has been set up and is now ready to use.");

        /* TRANSLATORS: title of libnotify bubble */
        notification = notify_notification_new (_("Additional software was installed"), message, NULL);
        notify_notification_set_app_name (notification, _("Software Updates"));
        notify_notification_set_timeout (notification, NOTIFY_EXPIRES_NEVER);
        notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);
        g_signal_connect (notification, "closed",
                          G_CALLBACK (on_notification_closed), NULL);

        /* show the bubble */
        ret = notify_notification_show (notification, &error);
        if (!ret) {
                g_warning ("error: %s", error->message);
                g_error_free (error);
        }
}

static void
install_packages_cb (GObject *object,
                     GAsyncResult *res,
                     GsdUpdatesFirmware *firmware)
{
        PkClient *client = PK_CLIENT (object);
        GError *error = NULL;
        PkResults *results = NULL;
        GPtrArray *array = NULL;
        gboolean restart = FALSE;
        const GsdUpdatesFirmwareRequest *req;
        gboolean ret;
        guint i;
        PkError *error_code = NULL;

        /* get the results */
        results = pk_client_generic_finish (client, res, &error);
        if (results == NULL) {
                g_warning ("failed to install file: %s", error->message);
                g_error_free (error);
                goto out;
        }

        /* check error code */
        error_code = pk_results_get_error_code (results);
        if (error_code != NULL) {
                g_warning ("failed to install file: %s, %s",
                           pk_error_enum_to_string (pk_error_get_code (error_code)),
                           pk_error_get_details (error_code));
                goto out;
        }

        /* go through all the requests, and find the worst type */
        array = firmware->priv->array_requested;
        for (i=0; i<array->len; i++) {
                req = g_ptr_array_index (array, i);
                ret = subsystem_can_replug (req->subsystem);
                if (!ret) {
                        restart = TRUE;
                        break;
                }
        }

        /* can we just rebind the device */
        ret = g_file_test (GSD_UPDATES_FIRMWARE_DEVICE_REBIND_PROGRAM, G_FILE_TEST_EXISTS);
        if (ret) {
                ret = device_rebind (firmware);
                if (ret) {
                        require_nothing (firmware);
                        goto out;
                }
        } else {
                /* give the user the correct message */
                if (restart)
                        require_restart (firmware);
                else
                        require_replug (firmware);
        }

        /* clear array */
        g_ptr_array_set_size (firmware->priv->array_requested, 0);
out:
        if (error_code != NULL)
                g_object_unref (error_code);
        if (array != NULL)
                g_ptr_array_unref (array);
        if (results != NULL)
                g_object_unref (results);
}

static gchar **
package_array_to_strv (GPtrArray *array)
{
	PkPackage *item;
	gchar **results;
	guint i;

	results = g_new0 (gchar *, array->len+1);
	for (i=0; i<array->len; i++) {
		item = g_ptr_array_index (array, i);
		results[i] = g_strdup (pk_package_get_id (item));
	}
	return results;
}

static void
install_package_ids (GsdUpdatesFirmware *firmware)
{
        gchar **package_ids;

        /* install all of the firmware files */
        package_ids = package_array_to_strv (firmware->priv->packages_found);
        pk_client_install_packages_async (PK_CLIENT(firmware->priv->task),
                                          TRUE, package_ids,
                                          NULL,
                                          NULL, NULL,
                                          (GAsyncReadyCallback) install_packages_cb,
                                          firmware);
        g_strfreev (package_ids);
}

static void
ignore_devices (GsdUpdatesFirmware *firmware)
{
        gchar *existing = NULL;
        GsdUpdatesFirmwareRequest *req;
        GPtrArray *array;
        GString *string;
        guint i;

        /* get from settings */
        existing = g_settings_get_string (firmware->priv->settings,
                                          GSD_SETTINGS_IGNORED_DEVICES);

        /* get existing string */
        string = g_string_new (existing);
        if (string->len > 0)
                g_string_append (string, ",");

        /* add all listed devices */
        array = firmware->priv->array_requested;
        for (i=0; i<array->len; i++) {
                req = g_ptr_array_index (array, i);
                g_string_append_printf (string, "%s,", req->id);
        }

        /* remove final ',' */
        if (string->len > 2)
                g_string_set_size (string, string->len - 1);

        /* set new string */
        g_settings_set_string (firmware->priv->settings,
                               GSD_SETTINGS_IGNORED_DEVICES,
                               string->str);

        g_free (existing);
        g_string_free (string, TRUE);
}

static PkPackage *
check_available (GsdUpdatesFirmware *firmware, const gchar *filename)
{
        guint length = 0;
        GPtrArray *array = NULL;
        GError *error = NULL;
        PkPackage *item = NULL;
        PkBitfield filter;
        PkResults *results;
        gchar **values = NULL;
        PkError *error_code = NULL;

        /* search for newest not installed package */
        filter = pk_bitfield_from_enums (PK_FILTER_ENUM_NOT_INSTALLED,
                                         PK_FILTER_ENUM_NEWEST, -1);
        values = g_strsplit (filename, "&", -1);
        results = pk_client_search_files (PK_CLIENT(firmware->priv->task),
                                          filter,
                                          values,
                                          NULL,
                                          NULL, NULL,
                                          &error);
        if (results == NULL) {
                g_warning ("failed to search file %s: %s",
                           filename, error->message);
                g_error_free (error);
                goto out;
        }

        /* check error code */
        error_code = pk_results_get_error_code (results);
        if (error_code != NULL) {
                g_warning ("failed to search file: %s, %s",
                           pk_error_enum_to_string (pk_error_get_code (error_code)),
                           pk_error_get_details (error_code));
                goto out;
        }

        /* make sure we have one package */
        array = pk_results_get_package_array (results);
        if (array->len == 0)
                g_debug ("no package providing %s found", filename);
        else if (array->len != 1)
                g_warning ("not one package providing %s found (%i)", filename, length);
        else
                item = g_object_ref (g_ptr_array_index (array, 0));
out:
        g_strfreev (values);
        if (error_code != NULL)
                g_object_unref (error_code);
        if (array != NULL)
                g_ptr_array_unref (array);
        if (results != NULL)
                g_object_unref (results);
        return item;
}

static void
remove_duplicate (GPtrArray *array)
{
        guint i, j;
        const gchar *val;
        const gchar *val_tmp;

        /* remove each duplicate entry */
        for (i=0; i<array->len; i++) {
                val = g_ptr_array_index (array, i);
                for (j=i+1; j<array->len; j++) {
                        val_tmp = g_ptr_array_index (array, j);
                        if (g_strcmp0 (val_tmp, val) == 0)
                                g_ptr_array_remove_index_fast (array, j);
                }
        }
}

static gboolean
delay_timeout_cb (gpointer data)
{
        guint i;
        gboolean ret;
        GString *string;
        GsdUpdatesFirmware *firmware = GSD_UPDATES_FIRMWARE (data);
        NotifyNotification *notification;
        GPtrArray *array;
        GError *error = NULL;
        PkPackage *item = NULL;
        const GsdUpdatesFirmwareRequest *req;
        gboolean has_data = FALSE;

        /* message string */
        string = g_string_new ("");

        /* try to find each firmware file in an available package */
        array = firmware->priv->array_requested;
        for (i=0; i<array->len; i++) {
                req = g_ptr_array_index (array, i);
                /* save to new array if we found one package for this file */
                item = check_available (firmware, req->filename);
                if (item != NULL) {
                        g_ptr_array_add (firmware->priv->packages_found, item);
                        g_object_unref (item);
                }
        }

        /* nothing to do */
        if (firmware->priv->packages_found->len == 0) {
                g_debug ("no packages providing any of the missing firmware");
                goto out;
        }

        /* check we don't want the same package more than once */
        remove_duplicate (firmware->priv->packages_found);

        /* have we got any models to array */
        for (i=0; i<array->len; i++) {
                req = g_ptr_array_index (array, i);
                if (req->model != NULL) {
                        has_data = TRUE;
                        break;
                }
        }

        /* TRANSLATORS: we need another package to keep udev quiet */
        g_string_append (string, _("Additional firmware is required to make hardware in this computer function correctly."));

        /* sdd what information we have */
        if (has_data) {
                g_string_append (string, "\n");
                for (i=0; i<array->len; i++) {
                        req = g_ptr_array_index (array, i);
                        if (req->model != NULL)
                                g_string_append_printf (string, "\n• %s", req->model);
                }
                g_string_append (string, "\n");
        }

        /* TRANSLATORS: title of libnotify bubble */
        notification = notify_notification_new (_("Additional firmware required"), string->str, NULL);
        notify_notification_set_app_name (notification, _("Software Updates"));
        notify_notification_set_timeout (notification, NOTIFY_EXPIRES_NEVER);
        notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);
        notify_notification_add_action (notification, "install-firmware",
                                        /* TRANSLATORS: button label */
                                        _("Install firmware"), libnotify_cb, firmware, NULL);
        notify_notification_add_action (notification, "ignore-devices",
                                        /* TRANSLATORS: we should ignore this device and not ask anymore */
                                        _("Ignore devices"), libnotify_cb, firmware, NULL);
        g_signal_connect (notification, "closed",
                          G_CALLBACK (on_notification_closed), NULL);

        ret = notify_notification_show (notification, &error);
        if (!ret) {
                g_warning ("error: %s", error->message);
                g_error_free (error);
        }

out:
        g_string_free (string, TRUE);
        /* never repeat */
        return FALSE;
}

static void
remove_banned (GsdUpdatesFirmware *firmware, GPtrArray *array)
{
        gboolean ret;
        gchar **banned = NULL;
        gchar *banned_str;
        GsdUpdatesFirmwareRequest *req;
        guint i, j;

        /* get from settings */
        banned_str = g_settings_get_string (firmware->priv->settings,
                                            GSD_SETTINGS_BANNED_FIRMWARE);
        if (banned_str == NULL) {
                g_warning ("could not read banned list");
                goto out;
        }

        /* nothing in list, common case */
        if (banned_str[0] == '\0') {
                g_debug ("nothing in banned list");
                goto out;
        }

        /* split using "," */
        banned = g_strsplit (banned_str, ",", 0);

        /* remove any banned pattern matches */
        for (i=0; i<array->len; i++) {
                req = g_ptr_array_index (array, i);
                for (j=0; banned[j] != NULL; j++) {
                        ret = g_pattern_match_simple (banned[j], req->filename);
                        if (ret) {
                                g_debug ("match %s for %s, removing",
                                         banned[j], req->filename);
                                request_free (req);
                                g_ptr_array_remove_index_fast (array, i);
                                break;
                        }
                }
        }
out:
        g_free (banned_str);
        g_strfreev (banned);
}

static void
remove_ignored (GsdUpdatesFirmware *firmware, GPtrArray *array)
{
        gboolean ret;
        gchar **ignored = NULL;
        gchar *ignored_str;
        GsdUpdatesFirmwareRequest *req;
        guint i, j;

        /* get from settings */
        ignored_str = g_settings_get_string (firmware->priv->settings,
                                             GSD_SETTINGS_IGNORED_DEVICES);
        if (ignored_str == NULL) {
                g_warning ("could not read ignored list");
                goto out;
        }

        /* nothing in list, common case */
        if (ignored_str[0] == '\0') {
                g_debug ("nothing in ignored list");
                goto out;
        }

        /* split using "," */
        ignored = g_strsplit (ignored_str, ",", 0);

        /* remove any ignored pattern matches */
        for (i=0; i<array->len; i++) {
                req = g_ptr_array_index (array, i);
                if (req->id == NULL)
                        continue;
                for (j=0; ignored[j] != NULL; j++) {
                        ret = g_pattern_match_simple (ignored[j], req->id);
                        if (ret) {
                                g_debug ("match %s for %s, removing", ignored[j], req->id);
                                request_free (req);
                                g_ptr_array_remove_index_fast (array, i);
                                break;
                        }
                }
        }
out:
        g_free (ignored_str);
        g_strfreev (ignored);
}

static gchar *
udev_text_decode (const gchar *data)
{
        guint i;
        guint j;
        gchar *decode;

        decode = g_strdup (data);
        for (i = 0, j = 0; data[i] != '\0'; j++) {
                if (memcmp (&data[i], "\\x2f", 4) == 0) {
                        decode[j] = '/';
                        i += 4;
                } else if (memcmp (&data[i], "\\x5c", 4) == 0) {
                        decode[j] = '\\';
                        i += 4;
                } else {
                        decode[j] = data[i];
                        i++;
                }
        }
        decode[j] = '\0';
        return decode;
}

static gchar *
get_device (GsdUpdatesFirmware *firmware, const gchar *filename)
{
        GFile *file;
        GFileInfo *info;
        const gchar *symlink_path;
        gchar *syspath = NULL;
        GError *error = NULL;
        gchar *target = NULL;
        gchar *tmp;

        /* get the file data */
        file = g_file_new_for_path (filename);
        info = g_file_query_info (file,
                                  G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
                                  G_FILE_QUERY_INFO_NONE,
                                  NULL,
                                  &error);
        if (info == NULL) {
                g_warning ("Failed to get symlink: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* /devices/pci0000:00/0000:00:1d.0/usb5/5-2/firmware/5-2 */
        symlink_path = g_file_info_get_symlink_target (info);
        if (symlink_path == NULL) {
                g_warning ("failed to get symlink target");
                goto out;
        }

        /* prepend sys to make '/sys/devices/pci0000:00/0000:00:1d.0/usb5/5-2/firmware/5-2' */
        syspath = g_strconcat ("/sys", symlink_path, NULL);

        /* start with the longest, and try to find a sub-path that exists */
        tmp = &syspath[strlen (syspath)];
        while (tmp != NULL) {
                *tmp = '\0';
                g_debug ("testing %s", target);
                if (g_file_test (syspath, G_FILE_TEST_EXISTS)) {
                        target = g_strdup (syspath);
                        goto out;
                }
                tmp = g_strrstr (syspath, "/");
        }
out:
        if (info != NULL)
                g_object_unref (info);
        g_object_unref (file);
        g_free (syspath);
        return target;
}

static void
add_filename (GsdUpdatesFirmware *firmware, const gchar *filename_no_path)
{
        gboolean ret;
        gchar *filename_path = NULL;
        gchar *missing_path = NULL;
        gchar *sysfs_path = NULL;
        GsdUpdatesFirmwareRequest *req;
        GPtrArray *array;
        guint i;

        /* this is the file we want to load */
        filename_path = g_build_filename (GSD_UPDATES_FIRMWARE_LOADING_DIR,
                                          filename_no_path, NULL);

        /* file already exists */
        ret = g_file_test (filename_path, G_FILE_TEST_EXISTS);
        if (ret)
                goto out;

        /* this is the file that udev created for us */
        missing_path = g_build_filename (GSD_UPDATES_FIRMWARE_MISSING_DIR,
                                         filename_no_path, NULL);
        g_debug ("filename=%s -> %s", missing_path, filename_path);

        /* get symlink target */
        sysfs_path = get_device (firmware, missing_path);
        if (sysfs_path == NULL)
                goto out;

        /* find any previous requests with this path or firmware */
        array = firmware->priv->array_requested;
        for (i=0; i<array->len; i++) {
                req = g_ptr_array_index (array, i);
                if (g_strcmp0 (sysfs_path, req->sysfs_path) == 0) {
                        g_debug ("ignoring previous sysfs request for %s",
                                 sysfs_path);
                        goto out;
                }
                if (g_strcmp0 (filename_path, req->filename) == 0) {
                        g_debug ("ignoring previous filename request for %s",
                                 filename_path);
                        goto out;
                }
        }

        /* create new request object */
        req = request_new (filename_path, sysfs_path);
        g_ptr_array_add (firmware->priv->array_requested, req);
out:
        g_free (missing_path);
        g_free (filename_path);
        g_free (sysfs_path);
}

static void
scan_directory (GsdUpdatesFirmware *firmware)
{
        gboolean ret;
        GError *error = NULL;
        GDir *dir;
        const gchar *filename;
        gchar *filename_decoded;
        guint i;
        GPtrArray *array;
        const GsdUpdatesFirmwareRequest *req;
        guint scan_id = 0;

        /* should we check and show the user */
        ret = g_settings_get_boolean (firmware->priv->settings,
                                      GSD_SETTINGS_ENABLE_CHECK_FIRMWARE);
        if (!ret) {
                g_debug ("not showing thanks to GSettings");
                return;
        }

        /* open the directory of requests */
        dir = g_dir_open (GSD_UPDATES_FIRMWARE_MISSING_DIR, 0, &error);
        if (dir == NULL) {
                if (error->code != G_FILE_ERROR_NOENT) {
                        g_warning ("failed to open directory: %s",
                                   error->message);
                }
                g_error_free (error);
                return;
        }

        /* find all the firmware requests */
        filename = g_dir_read_name (dir);
        while (filename != NULL) {

                filename_decoded = udev_text_decode (filename);
                add_filename (firmware, filename_decoded);
                g_free (filename_decoded);

                /* next file */
                filename = g_dir_read_name (dir);
        }
        g_dir_close (dir);

        /* debugging */
        array = firmware->priv->array_requested;
        for (i=0; i<array->len; i++) {
                req = g_ptr_array_index (array, i);
                g_debug ("requested: %s", req->filename);
        }

        /* remove banned files */
        remove_banned (firmware, array);

        /* remove ignored devices */
        remove_ignored (firmware, array);

        /* debugging */
        array = firmware->priv->array_requested;
        for (i=0; i<array->len; i++) {
                req = g_ptr_array_index (array, i);
                g_debug ("searching for: %s", req->filename);
        }

        /* don't spam the user at startup, so wait a little delay */
        if (array->len > 0) {
                scan_id = g_timeout_add_seconds (GSD_UPDATES_FIRMWARE_PROCESS_DELAY,
                                                 delay_timeout_cb,
                                                 firmware);
                g_source_set_name_by_id (scan_id, "[GsdUpdatesFirmware] process");
        }
}

static gboolean
scan_directory_cb (GsdUpdatesFirmware *firmware)
{
        scan_directory (firmware);
        firmware->priv->timeout_id = 0;
        return FALSE;
}

static void
monitor_changed_cb (GFileMonitor *monitor,
                    GFile *file,
                    GFile *other_file,
                    GFileMonitorEvent event_type,
                    GsdUpdatesFirmware *firmware)
{
        if (firmware->priv->timeout_id > 0) {
                g_debug ("clearing timeout as device changed");
                g_source_remove (firmware->priv->timeout_id);
        }

        /* wait for the device to settle */
        firmware->priv->timeout_id =
                g_timeout_add_seconds (GSD_UPDATES_FIRMWARE_INSERT_DELAY,
                                       (GSourceFunc) scan_directory_cb,
                                       firmware);
        g_source_set_name_by_id (firmware->priv->timeout_id,
                                 "[GsdUpdatesFirmware] changed");
}

static void
gsd_updates_firmware_class_init (GsdUpdatesFirmwareClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gsd_updates_firmware_finalize;
        g_type_class_add_private (klass, sizeof (GsdUpdatesFirmwarePrivate));
}

static void
gsd_updates_firmware_init (GsdUpdatesFirmware *firmware)
{
        GFile *file;
        GError *error = NULL;

        firmware->priv = GSD_UPDATES_FIRMWARE_GET_PRIVATE (firmware);
        firmware->priv->timeout_id = 0;
        firmware->priv->packages_found = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
        firmware->priv->array_requested = g_ptr_array_new_with_free_func ((GDestroyNotify) request_free);
        firmware->priv->settings = g_settings_new (GSD_SETTINGS_SCHEMA);
        firmware->priv->task = pk_task_new ();
        g_object_set (firmware->priv->task,
                      "background", TRUE,
                      NULL);

        /* setup watch for new hardware */
        file = g_file_new_for_path (GSD_UPDATES_FIRMWARE_MISSING_DIR);
        firmware->priv->monitor = g_file_monitor (file,
                                                  G_FILE_MONITOR_NONE,
                                                  NULL,
                                                  &error);
        if (firmware->priv->monitor == NULL) {
                g_warning ("failed to setup monitor: %s", error->message);
                g_error_free (error);
                goto out;
        }

        /* limit to one per second */
        g_file_monitor_set_rate_limit (firmware->priv->monitor, 1000);

        /* get notified of changes */
        g_signal_connect (firmware->priv->monitor, "changed",
                          G_CALLBACK (monitor_changed_cb), firmware);
out:
        g_object_unref (file);
        firmware->priv->timeout_id =
                g_timeout_add_seconds (GSD_UPDATES_FIRMWARE_LOGIN_DELAY,
                                       (GSourceFunc) scan_directory_cb,
                                       firmware);
        g_source_set_name_by_id (firmware->priv->timeout_id,
                                 "[GsdUpdatesFirmware] login coldplug");
}

static void
gsd_updates_firmware_finalize (GObject *object)
{
        GsdUpdatesFirmware *firmware;

        g_return_if_fail (GSD_UPDATES_IS_FIRMWARE (object));

        firmware = GSD_UPDATES_FIRMWARE (object);

        g_return_if_fail (firmware->priv != NULL);
        g_ptr_array_unref (firmware->priv->array_requested);
        g_ptr_array_unref (firmware->priv->packages_found);
        g_object_unref (PK_CLIENT(firmware->priv->task));
        g_object_unref (firmware->priv->settings);
        if (firmware->priv->monitor != NULL)
                g_object_unref (firmware->priv->monitor);
        if (firmware->priv->timeout_id > 0)
                g_source_remove (firmware->priv->timeout_id);

        G_OBJECT_CLASS (gsd_updates_firmware_parent_class)->finalize (object);
}

GsdUpdatesFirmware *
gsd_updates_firmware_new (void)
{
        GsdUpdatesFirmware *firmware;
        firmware = g_object_new (GSD_UPDATES_TYPE_FIRMWARE, NULL);
        return GSD_UPDATES_FIRMWARE (firmware);
}

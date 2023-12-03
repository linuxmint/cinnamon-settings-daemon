/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 David Zeuthen <david@fubar.dk>
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/time.h>

#include <glib.h>
#include <glib-object.h>
#include "csd-exported-datetime.h"

#include <polkit/polkit.h>

#include "system-timezone.h"

#include "csd-datetime-mechanism.h"

/* NTP helper functions for various distributions */
#include "csd-datetime-mechanism-fedora.h"
#include "csd-datetime-mechanism-debian.h"
#include "csd-datetime-mechanism-suse.h"

struct _CsdDatetimeMechanism
{
        GObject        parent;
        CsdExportedDateTime *skeleton;
        PolkitAuthority *auth;
};

G_DEFINE_TYPE (CsdDatetimeMechanism, csd_datetime_mechanism, G_TYPE_OBJECT)

static gboolean _check_polkit_for_action (CsdDatetimeMechanism *mechanism, GDBusMethodInvocation *invocation);

#define IFACE "org.cinnamon.SettingsDaemon.DateTimeMechanism"

static const GDBusErrorEntry csd_datetime_mechanism_error_entries[] = {
        { CSD_DATETIME_MECHANISM_ERROR_GENERAL,                 IFACE ".GeneralError" },
        { CSD_DATETIME_MECHANISM_ERROR_NOT_PRIVILEGED,          IFACE ".NotPrivileged" },
        { CSD_DATETIME_MECHANISM_ERROR_INVALID_TIMEZONE_FILE,   IFACE ".InvalidTimezoneFile" }
};

GQuark
csd_datetime_mechanism_error_quark (void)
{
        static volatile gsize quark_volatile = 0;

        g_dbus_error_register_error_domain ("csd_datetime_mechanism_error",
                                            &quark_volatile,
                                            csd_datetime_mechanism_error_entries,
                                            G_N_ELEMENTS (csd_datetime_mechanism_error_entries));

        return quark_volatile;
}

static gboolean
do_exit (gpointer user_data)
{
        g_debug ("Exiting due to inactivity");
        g_main_loop_quit (loop);
        return FALSE;
}

static void
reset_killtimer (void)
{
        static guint timer_id = 0;

        if (timer_id > 0) {
                g_source_remove (timer_id);
                timer_id = 0;
        }

        g_debug ("Setting killtimer to 30 seconds...");
        timer_id = g_timeout_add_seconds (30, do_exit, NULL);
}

static gboolean
check_can_do (CsdDatetimeMechanism  *mechanism,
              const char            *action,
              GDBusMethodInvocation *invocation,
              gint                  *canval)
{
        const char *sender;
        PolkitSubject *subject;
        PolkitAuthorizationResult *result;
        GError *error;

        reset_killtimer ();

        /* Check that caller is privileged */
        sender = g_dbus_method_invocation_get_sender (invocation);
        subject = polkit_system_bus_name_new (sender);

        error = NULL;
        result = polkit_authority_check_authorization_sync (mechanism->auth,
                                                            subject,
                                                            action,
                                                            NULL,
                                                            0,
                                                            NULL,
                                                            &error);
        g_object_unref (subject);

        if (error) {
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                return FALSE;
        }

        if (polkit_authorization_result_get_is_authorized (result)) {
                *canval = 2;
        }
        else if (polkit_authorization_result_get_is_challenge (result)) {
                *canval = 1;
        }
        else {
                *canval = 0;
        }

        g_object_unref (result);

        return TRUE;
}

gboolean
handle_can_set_timezone (CsdExportedDateTime   *object,
                         GDBusMethodInvocation *invocation,
                         CsdDatetimeMechanism  *mechanism)
{
        gint canval = 0;

        reset_killtimer ();
        g_debug ("CanSetTimezone called");

        if (!check_can_do (mechanism,
                           "org.cinnamon.settingsdaemon.datetimemechanism.configure",
                           invocation,
                           &canval)) {
                return FALSE;
        }

        csd_exported_date_time_complete_can_set_timezone (object, invocation, canval);

        return TRUE;
}

static gboolean
check_tz_name (const char *tz,
               GError    **error)
{
        GFile *file;
        char *tz_path, *actual_path;
        gboolean retval;

        retval = TRUE;
        tz_path = g_build_filename (SYSTEM_ZONEINFODIR, tz, NULL);

        /* Get the actual resolved path */
        file = g_file_new_for_path (tz_path);
        actual_path = g_file_get_path (file);
        g_object_unref (file);

        /* The tz name passed had relative paths in it */
        if (g_strcmp0 (tz_path, actual_path) != 0) {
                g_set_error (error, CSD_DATETIME_MECHANISM_ERROR,
                             CSD_DATETIME_MECHANISM_ERROR_INVALID_TIMEZONE_FILE,
                             "Timezone file '%s' was invalid.",
                             tz);
                retval = FALSE;
        }

        g_free (tz_path);
        g_free (actual_path);

        return retval;
}

gboolean
handle_set_timezone (CsdExportedDateTime     *object,
                     GDBusMethodInvocation   *invocation,
                     const char              *tz,
                     CsdDatetimeMechanism    *mechanism)
{
        GError *error;

        reset_killtimer ();
        g_debug ("SetTimezone ('%s') called", tz);

        if (!_check_polkit_for_action (mechanism, invocation))
                return FALSE;

        error = NULL;

        if (!check_tz_name (tz, &error))
                return FALSE;

        if (!system_timezone_set (tz, &error)) {
                GError *error2;
                int     code;

                if (error->code == SYSTEM_TIMEZONE_ERROR_INVALID_TIMEZONE_FILE)
                        code = CSD_DATETIME_MECHANISM_ERROR_INVALID_TIMEZONE_FILE;
                else
                        code = CSD_DATETIME_MECHANISM_ERROR_GENERAL;

                error2 = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                      code, "%s", error->message);

                g_error_free (error);

                g_dbus_method_invocation_return_gerror (invocation, error2);
                g_error_free (error2);

                return FALSE;
        }

        csd_exported_date_time_complete_set_timezone (object, invocation);
        return TRUE;
}


gboolean
handle_get_timezone (CsdExportedDateTime    *object,
                     GDBusMethodInvocation  *invocation,
                     CsdDatetimeMechanism   *mechanism)
{
  gchar *timezone;

  reset_killtimer ();
  g_debug ("GetTimezone called");

  timezone = system_timezone_find ();

  csd_exported_date_time_complete_get_timezone (object, invocation, timezone);
  g_free (timezone);

  return TRUE;
}

static gboolean
_sync_hwclock (GDBusMethodInvocation *invocation)
{
        GError *error;

        error = NULL;

        if (g_file_test ("/sbin/hwclock",
                         G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_EXECUTABLE)) {
                int exit_status;
                if (!g_spawn_command_line_sync ("/sbin/hwclock --systohc", NULL, NULL, &exit_status, &error)) {
                        GError *error2;
                        error2 = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                              CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                              "Error spawning /sbin/hwclock: %s", error->message);
                        g_error_free (error);
                        g_dbus_method_invocation_return_gerror (invocation, error2);
                        g_error_free (error2);
                        return FALSE;
                }
                if (WEXITSTATUS (exit_status) != 0) {
                        error = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                             CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                             "/sbin/hwclock returned %d", exit_status);
                        g_dbus_method_invocation_return_gerror (invocation, error);
                        g_error_free (error);
                        return FALSE;
                }
        }

        return TRUE;
}

static gboolean
_set_date (CsdDatetimeMechanism  *mechanism,
           guint                  day,
           guint                  month,
           guint                  year,
           GDBusMethodInvocation *invocation)
{
        GDateTime *time;
        char *date_str, *time_str;
        char *date_cmd;
        int exit_status;
        GError *error;

        if (!_check_polkit_for_action (mechanism, invocation))
                return FALSE;

        date_str = g_strdup_printf ("%02d/%02d/%d", month, day, year);
        error = NULL;

        time = g_date_time_new_now_local ();
        time_str = g_date_time_format (time, "%R:%S");
        g_date_time_unref (time);

        date_cmd = g_strdup_printf ("/bin/date -s \"%s %s\" +\"%%D %%R:%%S\"", date_str, time_str);
        g_free (date_str);
        g_free (time_str);

        if (!g_spawn_command_line_sync (date_cmd, NULL, NULL, &exit_status, &error)) {
                GError *error2;
                error2 = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                      CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                      "Error spawning /bin/date: %s", error->message);
                g_error_free (error);
                g_dbus_method_invocation_return_gerror (invocation, error2);
                g_error_free (error2);
                g_free (date_cmd);
                return FALSE;
        }
        g_free (date_cmd);
        if (WEXITSTATUS (exit_status) != 0) {
                error = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                     CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                     "/bin/date returned %d", exit_status);
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                return FALSE;
        }

        if (!_sync_hwclock (invocation))
                return FALSE;

        return TRUE;
}

gboolean
handle_set_date (CsdExportedDateTime   *object,
                 GDBusMethodInvocation *invocation,
                 guint                  day,
                 guint                  month,
                 guint                  year,
                 CsdDatetimeMechanism  *mechanism)
{
        reset_killtimer ();
        g_debug ("SetDate (%d, %d, %d) called", day, month, year);

        if (!_set_date (mechanism, day, month, year, invocation)) {
                return FALSE;
        }

        csd_exported_date_time_complete_set_date (object, invocation);
        return TRUE;
}

static gboolean
_set_time (CsdDatetimeMechanism  *mechanism,
           const struct timeval  *tv,
           GDBusMethodInvocation *invocation)
{
        GError *error;

        if (!_check_polkit_for_action (mechanism, invocation))
                return FALSE;

        if (settimeofday (tv, NULL) != 0) {
                error = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                     CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                     "Error calling settimeofday({%ld,%ld}): %s",
                                     (gint64) tv->tv_sec, (gint64) tv->tv_usec,
                                     strerror (errno));
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                return FALSE;
        }

        if (!_sync_hwclock (invocation))
                return FALSE;

        return TRUE;
}

gboolean
handle_set_time (CsdExportedDateTime   *object,
                 GDBusMethodInvocation *invocation,
                 gint64                 seconds_since_epoch,
                 CsdDatetimeMechanism  *mechanism)
{
        struct timeval tv;

        reset_killtimer ();
        g_debug ("SetTime (%" G_GINT64_FORMAT ") called", seconds_since_epoch);

        tv.tv_sec = (time_t) seconds_since_epoch;
        tv.tv_usec = 0;

        if (!_set_time (mechanism, &tv, invocation)) {
                return FALSE;
        }

        csd_exported_date_time_complete_set_time (object, invocation);

        return TRUE;
}

gboolean
handle_can_set_time (CsdExportedDateTime   *object,
                     GDBusMethodInvocation *invocation,
                     CsdDatetimeMechanism  *mechanism)
{
        gint canval = 0;

        g_debug ("CanSetTime called");

        if (!check_can_do (mechanism,
                           "org.cinnamon.settingsdaemon.datetimemechanism.configure",
                           invocation,
                           &canval)) {
                return FALSE;
        }

        csd_exported_date_time_complete_can_set_time (object, invocation, canval);
        return TRUE;
}

gboolean
handle_adjust_time (CsdExportedDateTime   *object,
                    GDBusMethodInvocation *invocation,
                    gint64                 seconds_to_add,
                    CsdDatetimeMechanism  *mechanism)
{
        struct timeval tv;

        reset_killtimer ();
        g_debug ("AdjustTime (%" G_GINT64_FORMAT " ) called", seconds_to_add);

        if (gettimeofday (&tv, NULL) != 0) {
                GError *error;
                error = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                     CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                     "Error calling gettimeofday(): %s", strerror (errno));
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                return FALSE;
        }

        tv.tv_sec += (time_t) seconds_to_add;

        if (!_set_time (mechanism, &tv, invocation)) {
                return FALSE;
        }

        csd_exported_date_time_complete_adjust_time (object, invocation);

        return TRUE;
}

gboolean
handle_get_hardware_clock_using_utc (CsdExportedDateTime   *object,
                                     GDBusMethodInvocation *invocation,
                                     CsdDatetimeMechanism  *mechanism)
{
        char **lines;
        char *data;
        gsize len;
        GError *error;
        gboolean is_utc;

        reset_killtimer ();
        g_debug ("GetHardwareClockUsingUtc called");

        error = NULL;

        if (!g_file_get_contents ("/etc/adjtime", &data, &len, &error)) {
                GError *error2;
                error2 = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                      CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                      "Error reading /etc/adjtime file: %s", error->message);
                g_error_free (error);
                g_dbus_method_invocation_return_gerror (invocation, error2);
                g_error_free (error2);
                return FALSE;
        }

        lines = g_strsplit (data, "\n", 0);
        g_free (data);

        if (g_strv_length (lines) < 3) {
                error = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                     CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                     "Cannot parse /etc/adjtime");
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                g_strfreev (lines);
                return FALSE;
        }

        if (strcmp (lines[2], "UTC") == 0) {
                is_utc = TRUE;
        } else if (strcmp (lines[2], "LOCAL") == 0) {
                is_utc = FALSE;
        } else {
                error = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                     CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                     "Expected UTC or LOCAL at line 3 of /etc/adjtime; found '%s'", lines[2]);
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                g_strfreev (lines);
                return FALSE;
        }
        g_strfreev (lines);

        csd_exported_date_time_complete_get_hardware_clock_using_utc (object, invocation, is_utc);

        return TRUE;
}

gboolean
handle_set_hardware_clock_using_utc (CsdExportedDateTime   *object,
                                     GDBusMethodInvocation *invocation,
                                     gboolean               using_utc,
                                     CsdDatetimeMechanism  *mechanism)
{
        GError *error;

        error = NULL;

        reset_killtimer ();

        g_debug ("SetHardwareClockUsingUtc (%d) called", using_utc);

        if (!_check_polkit_for_action (mechanism, invocation))
                return FALSE;

        if (g_file_test ("/sbin/hwclock", 
                         G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_EXECUTABLE)) {
                int exit_status;
                char *cmd;
                cmd = g_strdup_printf ("/sbin/hwclock %s --systohc", using_utc ? "--utc" : "--localtime");
                if (!g_spawn_command_line_sync (cmd, NULL, NULL, &exit_status, &error)) {
                        GError *error2;
                        error2 = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                              CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                              "Error spawning /sbin/hwclock: %s", error->message);
                        g_error_free (error);
                        g_dbus_method_invocation_return_gerror (invocation, error2);
                        g_error_free (error2);
                        g_free (cmd);
                        return FALSE;
                }
                g_free (cmd);
                if (WEXITSTATUS (exit_status) != 0) {
                        error = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                             CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                             "/sbin/hwclock returned %d", exit_status);
                        g_dbus_method_invocation_return_gerror (invocation, error);
                        g_error_free (error);
                        return FALSE;
                }

                if (g_file_test ("/etc/redhat-release", G_FILE_TEST_EXISTS)) { /* Fedora */
                        if (!_update_etc_sysconfig_clock_fedora (invocation, "UTC=", using_utc ? "true" : "false"))
                                return FALSE;
                } else if (g_file_test ("/etc/SuSE-release", G_FILE_TEST_EXISTS)) { /* SUSE variant */
                        if (!_update_etc_sysconfig_clock_suse (invocation, "HWCLOCK=", using_utc ? "-u" : "--localtime"))
                                return FALSE;
                }
        }

        csd_exported_date_time_complete_set_hardware_clock_using_utc (object, invocation);

        return TRUE;
}

gboolean
handle_get_using_ntp  (CsdExportedDateTime   *object,
                       GDBusMethodInvocation *invocation,
                       CsdDatetimeMechanism  *mechanism)
{
        GError *error = NULL;
        gboolean ret;
        gboolean is_using_ntp = FALSE;
        gboolean can_use_ntp = FALSE;

        reset_killtimer ();

        g_debug ("GetUsingNtp called");

        if (g_file_test ("/etc/redhat-release", G_FILE_TEST_EXISTS)) /* Fedora */
                ret = _get_using_ntp_fedora (invocation, &can_use_ntp, &is_using_ntp);
        else if (g_file_test ("/usr/sbin/update-rc.d", G_FILE_TEST_EXISTS)) /* Debian */
                ret = _get_using_ntp_debian (invocation, &can_use_ntp, &is_using_ntp);
        else if (g_file_test ("/etc/SuSE-release", G_FILE_TEST_EXISTS)) /* SUSE variant */
                ret = _get_using_ntp_suse (invocation, &can_use_ntp, &is_using_ntp);
        else {
                error = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                     CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                     "Error enabling NTP: OS variant not supported");
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                return FALSE;
        }

        if (ret) {
                csd_exported_date_time_complete_get_using_ntp (object, invocation, can_use_ntp, is_using_ntp);
        }

        return ret;
}

gboolean
handle_set_using_ntp  (CsdExportedDateTime   *object,
                       GDBusMethodInvocation *invocation,
                       gboolean               using_ntp,
                       CsdDatetimeMechanism  *mechanism)
{
        GError *error;
        gboolean ret;

        reset_killtimer ();
        g_debug ("SetUsingNtp (%d) called", using_ntp);

        error = NULL;

        if (!_check_polkit_for_action (mechanism, invocation))
                return FALSE;

        if (g_file_test ("/etc/redhat-release", G_FILE_TEST_EXISTS)) /* Fedora */
                ret = _set_using_ntp_fedora (invocation, using_ntp);
        else if (g_file_test ("/usr/sbin/update-rc.d", G_FILE_TEST_EXISTS)) /* Debian */
                ret = _set_using_ntp_debian (invocation, using_ntp);
        else if (g_file_test ("/etc/SuSE-release", G_FILE_TEST_EXISTS)) /* SUSE variant */
                ret = _set_using_ntp_suse (invocation, using_ntp);
        else {
                error = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                     CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                     "Error enabling NTP: OS variant not supported");
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                return FALSE;
        }

        if (ret) {
            csd_exported_date_time_complete_set_using_ntp (object, invocation);
        }

        return ret;
}



gboolean
handle_can_set_using_ntp (CsdExportedDateTime   *object,
                          GDBusMethodInvocation *invocation,
                          CsdDatetimeMechanism  *mechanism)
{
        gint canval = 0;

        g_debug ("CanSetUsingNtp called");

        if (!check_can_do (mechanism,
                           "org.cinnamon.settingsdaemon.datetimemechanism.configure",
                           invocation,
                           &canval)) {
                return FALSE;
        }

        csd_exported_date_time_complete_can_set_using_ntp (object, invocation, canval);

        return TRUE;
}

static void
csd_datetime_mechanism_dispose (GObject *object)
{
        CsdDatetimeMechanism *mechanism;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_DATETIME_IS_MECHANISM (object));

        mechanism = CSD_DATETIME_MECHANISM (object);

        if (mechanism->skeleton != NULL) {
                g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (mechanism->skeleton));
                g_clear_object (&mechanism->skeleton);
        }

        g_clear_object (&mechanism->auth);

        G_OBJECT_CLASS (csd_datetime_mechanism_parent_class)->dispose (object);
}

static void
csd_datetime_mechanism_class_init (CsdDatetimeMechanismClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = csd_datetime_mechanism_dispose;
}

static void
csd_datetime_mechanism_init (CsdDatetimeMechanism *mechanism)
{
}

typedef struct
{
    const gchar  *signal_name;
    gpointer      callback;
} SkeletonSignal;

static SkeletonSignal skeleton_signals[] = {
    // signal name                                     callback
    { "handle-set-timezone",                           handle_set_timezone },
    { "handle-get-timezone",                           handle_get_timezone },
    { "handle-can-set-timezone",                       handle_can_set_timezone },
    { "handle-set-date",                               handle_set_date },
    { "handle-set-time",                               handle_set_time },
    { "handle-can-set-time",                           handle_can_set_time },
    { "handle-adjust-time",                            handle_adjust_time },
    { "handle-get-hardware-clock-using-utc",           handle_get_hardware_clock_using_utc },
    { "handle-set-hardware-clock-using-utc",           handle_set_hardware_clock_using_utc },
    { "handle-get-using-ntp",                          handle_get_using_ntp },
    { "handle-set-using-ntp",                          handle_set_using_ntp },
    { "handle-can-set-using-ntp",                      handle_can_set_using_ntp }
};

static gboolean
register_mechanism (CsdDatetimeMechanism *mechanism)
{
        GError *error = NULL;
        gint i;

        mechanism->auth = polkit_authority_get_sync (NULL, &error);
        if (mechanism->auth == NULL) {
                if (error != NULL) {
                        g_critical ("error getting system bus: %s", error->message);
                        g_error_free (error);
                }
                goto error;
        }

        mechanism->skeleton = csd_exported_date_time_skeleton_new ();

        g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (mechanism->skeleton),
                                          connection,
                                          "/org/cinnamon/SettingsDaemon/DateTimeMechanism",
                                          &error);

        if (error != NULL) {
                g_critical ("error exporting datetime interface: %s", error->message);
                g_error_free (error);
                goto error;
        }

        for (i = 0; i < G_N_ELEMENTS (skeleton_signals); i++) {
                SkeletonSignal sig = skeleton_signals[i];
                g_signal_connect (mechanism->skeleton,
                                  sig.signal_name,
                                  G_CALLBACK (sig.callback),
                                  mechanism);
        }

        reset_killtimer ();

        return TRUE;

error:
        return FALSE;
}


CsdDatetimeMechanism *
csd_datetime_mechanism_new (void)
{
        GObject *object;
        gboolean res;

        object = g_object_new (CSD_DATETIME_TYPE_MECHANISM, NULL);

        res = register_mechanism (CSD_DATETIME_MECHANISM (object));
        if (! res) {
                g_object_unref (object);
                return NULL;
        }

        return CSD_DATETIME_MECHANISM (object);
}

static gboolean
_check_polkit_for_action (CsdDatetimeMechanism *mechanism, GDBusMethodInvocation *invocation)
{
        const char *action = "org.cinnamon.settingsdaemon.datetimemechanism.configure";
        const char *sender;
        GError *error;
        PolkitSubject *subject;
        PolkitAuthorizationResult *result;

        error = NULL;

        /* Check that caller is privileged */
        sender = g_dbus_method_invocation_get_sender (invocation);
        subject = polkit_system_bus_name_new (sender);

        result = polkit_authority_check_authorization_sync (mechanism->auth,
                                                            subject,
                                                            action,
                                                            NULL,
                                                            POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
                                                            NULL, &error);
        g_object_unref (subject);

        if (error) {
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);

                return FALSE;
        }

        if (!polkit_authorization_result_get_is_authorized (result)) {
                error = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                     CSD_DATETIME_MECHANISM_ERROR_NOT_PRIVILEGED,
                                     "Not Authorized for action %s", action);
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                g_object_unref (result);

                return FALSE;
        }

        g_object_unref (result);

        return TRUE;
}

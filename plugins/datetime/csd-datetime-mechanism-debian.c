/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 David Zeuthen <david@fubar.dk>
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "csd-datetime-mechanism-debian.h"
#include "csd-datetime-mechanism.h"

static void
_get_using_ntpdate (gboolean *can_use, gboolean *is_using, GError ** error)
{
        if (!g_file_test ("/usr/sbin/ntpdate-debian", G_FILE_TEST_EXISTS))
                return;

        *can_use = TRUE;

        if (g_file_test ("/etc/network/if-up.d/ntpdate", G_FILE_TEST_EXISTS))
                *is_using = TRUE;
}

static void
_get_using_ntpd (gboolean *can_use, gboolean *is_using, GError ** error)
{
        int exit_status;
        GError *tmp_error = NULL;

        if (!g_file_test ("/usr/sbin/ntpd", G_FILE_TEST_EXISTS))
                return;

        *can_use = TRUE;

        if (!g_spawn_command_line_sync ("/usr/sbin/service ntp status",
                                        NULL, NULL, &exit_status, &tmp_error)) {
                if (error != NULL && *error == NULL) {
                        *error = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                              CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                              "Error spawning /usr/sbin/service: %s",
                                              tmp_error->message);
                }
                g_error_free (tmp_error);
                return;
        }

        if (exit_status == 0)
                *is_using = TRUE;
}

gboolean
_get_using_ntp_debian (GDBusMethodInvocation *invocation,
                       gboolean              *can_use_ntp,
                       gboolean              *is_using_ntp)
{
        GError *error = NULL;

        /* In Debian, ntpdate is used whenever the network comes up. So if
           either ntpdate or ntpd is installed and available, can_use is true.
           If either is active, is_using is true. */
        _get_using_ntpdate (can_use_ntp, is_using_ntp, &error);
        _get_using_ntpd (can_use_ntp, is_using_ntp, &error);

        if (error == NULL) {
                return TRUE;
        } else {
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                return FALSE;
        }
}

static void
_set_using_ntpdate (gboolean using_ntp, GError **error)
{
        const gchar *cmd = NULL;
        GError  *tmp_error = NULL;

        /* Debian uses an if-up.d script to sync network time when an interface
           comes up.  This is a separate mechanism from ntpd altogether. */

#define NTPDATE_ENABLED  "/etc/network/if-up.d/ntpdate"
#define NTPDATE_DISABLED "/etc/network/if-up.d/ntpdate.disabled"

        if (using_ntp && g_file_test (NTPDATE_DISABLED, G_FILE_TEST_EXISTS))
                cmd = "/bin/mv -f "NTPDATE_DISABLED" "NTPDATE_ENABLED;
        else if (!using_ntp && g_file_test (NTPDATE_ENABLED, G_FILE_TEST_EXISTS))
                cmd = "/bin/mv -f "NTPDATE_ENABLED" "NTPDATE_DISABLED;
        else
                 return;

        if (!g_spawn_command_line_sync (cmd, NULL, NULL, NULL, &tmp_error)) {
                if (error != NULL && *error == NULL) {
                        *error = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                              CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                              "Error spawning /bin/mv: %s",
                                              tmp_error->message);
                }
                g_error_free (tmp_error);
                return;
        }

        /* Kick start ntpdate to sync time immediately */
        if (using_ntp &&
            !g_spawn_command_line_sync ("/etc/network/if-up.d/ntpdate",
                                        NULL, NULL, NULL, &tmp_error)) {
                if (error != NULL && *error == NULL) {
                        *error = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                              CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                              "Error spawning /etc/network/if-up.d/ntpdate: %s",
                                              tmp_error->message);
                }
                g_error_free (tmp_error);
                return;
        }
}

static void
_set_using_ntpd (gboolean using_ntp, GError **error)
{
        GError *tmp_error = NULL;
        int exit_status;
        char *cmd;

        if (!g_file_test ("/usr/sbin/ntpd", G_FILE_TEST_EXISTS))
                return;

        cmd = g_strconcat ("/usr/sbin/update-rc.d ntp ", using_ntp ? "enable" : "disable", NULL);

        if (!g_spawn_command_line_sync (cmd, NULL, NULL, &exit_status, &tmp_error)) {
                if (error != NULL && *error == NULL) {
                        *error = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                              CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                              "Error spawning '%s': %s",
                                              cmd, tmp_error->message);
                }
                g_error_free (tmp_error);
                g_free (cmd);
                return;
        }

        g_free (cmd);

        cmd = g_strconcat ("/usr/sbin/service ntp ", using_ntp ? "restart" : "stop", NULL);;

        if (!g_spawn_command_line_sync (cmd, NULL, NULL, &exit_status, &tmp_error)) {
                if (error != NULL && *error == NULL) {
                        *error = g_error_new (CSD_DATETIME_MECHANISM_ERROR,
                                              CSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                              "Error spawning '%s': %s",
                                              cmd, tmp_error->message);
                }
                g_error_free (tmp_error);
                g_free (cmd);
                return;
        }

        g_free (cmd);
}

gboolean
_set_using_ntp_debian  (GDBusMethodInvocation *invocation,
                        gboolean               using_ntp)
{
        GError *error = NULL;

        /* In Debian, ntpdate and ntpd may be installed separately, so don't
           assume both are valid. */

        _set_using_ntpdate (using_ntp, &error);
        _set_using_ntpd (using_ntp, &error);

        if (error == NULL) {
                return TRUE;
        } else {
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                return FALSE;
        }
}


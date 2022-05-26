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
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "csd-datetime-mechanism.h"

#define BUS_NAME "org.cinnamon.SettingsDaemon.DateTimeMechanism"

GMainLoop             *loop;
GDBusConnection       *connection;
static CsdDatetimeMechanism  *mechanism;
static int                    ret;

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
        if (mechanism == NULL) {
                g_critical ("Couldn't acquire bus name");
                g_main_loop_quit (loop);
                return;
        }

        g_message ("Name lost or replaced - quitting");
        g_main_loop_quit (loop);
}

int
main (int argc, char **argv)
{
        GError *error;
        guint owner_id;

        ret = 1;
        error = NULL;
        connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);

        if (connection == NULL) {
            g_critical ("Couldn't connect to system bus: %s", error->message);
            g_error_free (error);
            goto out;
        }

        mechanism = csd_datetime_mechanism_new ();

        if (mechanism == NULL) {
            goto out;
        }

        owner_id = g_bus_own_name_on_connection (connection,
                                                 BUS_NAME,
                                                 G_BUS_NAME_OWNER_FLAGS_REPLACE | G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
                                                 on_name_acquired,
                                                 on_name_lost,
                                                 NULL, NULL);

        loop = g_main_loop_new (NULL, FALSE);
        g_main_loop_run (loop);

        g_object_unref (mechanism);

        g_bus_unown_name (owner_id);

        g_object_unref (connection);
        g_main_loop_unref (loop);
        ret = 0;

out:
        return ret;
}

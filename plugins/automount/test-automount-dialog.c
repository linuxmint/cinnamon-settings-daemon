/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * save-session.c - Small program to talk to session manager.

   Copyright (C) 1998 Tom Tromey
   Copyright (C) 2008 Red Hat, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA
   02110-1335, USA.
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib.h>

#include "csd-autorun.h"

static void
autorun_show_window (GMount *mount, gpointer user_data)
{
    gtk_main_quit ();
}

int
main (int argc, char *argv[])
{
        GVolumeMonitor *monitor;
        GError *error;
        GList *mounts;

        error = NULL;
        if (! gtk_init_with_args (&argc, &argv, NULL, NULL, NULL, &error)) {
                g_warning ("Unable to start: %s", error->message);
                g_error_free (error);
                exit (1);
        }

        if (argc != 2)
        {
            g_print ("Need one argument as content type\n");
            exit (1);
        }

        monitor = g_volume_monitor_get ();
        mounts = g_volume_monitor_get_mounts (monitor);

        if (mounts)
        {
            GMount *mount = G_MOUNT (mounts->data);

            csd_autorun_for_content_type (mount,
                                          argv[1],
                                          (CsdAutorunOpenWindow) autorun_show_window,
                                          NULL);
        }

        gtk_main ();

        gtk_main_quit ();
        return 0;
}

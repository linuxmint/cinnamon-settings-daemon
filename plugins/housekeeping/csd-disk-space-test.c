/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * vim: set et sw=8 ts=8:
 *
 * Copyright (c) 2008, Novell, Inc.
 *
 * Authors: Vincent Untz <vuntz@gnome.org>
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
#include <libnotify/notify.h>
#include "csd-disk-space.h"

int
main (int    argc,
      char **argv)
{
        GMainLoop *loop;

        gtk_init (&argc, &argv);
        notify_init ("csd-disk-space-test");

        loop = g_main_loop_new (NULL, FALSE);

        csd_ldsm_setup (TRUE);

        g_main_loop_run (loop);

        csd_ldsm_clean ();
        g_main_loop_unref (loop);

        return 0;
}


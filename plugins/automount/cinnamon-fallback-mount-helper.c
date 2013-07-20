/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 *
 * Author: Tomas Bzatek <tbzatek@redhat.com>
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <unistd.h>
#include <gtk/gtk.h>

#include "csd-automount-manager.h"

int
main (int argc,
      char **argv)
{
        GMainLoop *loop;
        CsdAutomountManager *manager;
        GError *error = NULL;

        g_type_init ();
        gtk_init (&argc, &argv);

        bindtextdomain (GETTEXT_PACKAGE, CINNAMON_SETTINGS_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

        loop = g_main_loop_new (NULL, FALSE);
        manager = csd_automount_manager_new ();

        csd_automount_manager_start (manager, &error);

        if (error != NULL) {
                g_printerr ("Unable to start the mount manager: %s",
                            error->message);

                g_error_free (error);
                _exit (1);
        }

        g_main_loop_run (loop);

        csd_automount_manager_stop (manager);
        g_main_loop_unref (loop);

        return 0;
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA
 * 02110-1335, USA.
 *
 *
 */

#include "config.h"

#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "csd-osd-window.h"

static gboolean
update_state (GtkWidget *window)
{
        static int count = 0;

        count++;

        switch (count) {
        case 1:
                csd_osd_window_set_volume_level (CSD_OSD_WINDOW (window), 50);
                csd_osd_window_set_action (CSD_OSD_WINDOW (window),
                                           CSD_OSD_WINDOW_ACTION_VOLUME);

                gtk_widget_show (window);
                break;
        case 2:
                csd_osd_window_set_volume_level (CSD_OSD_WINDOW (window), 100);
                csd_osd_window_set_action (CSD_OSD_WINDOW (window),
                                           CSD_OSD_WINDOW_ACTION_VOLUME);

                gtk_widget_show (window);
                break;
        case 3:
                csd_osd_window_set_volume_muted (CSD_OSD_WINDOW (window), TRUE);
                csd_osd_window_set_action (CSD_OSD_WINDOW (window),
                                           CSD_OSD_WINDOW_ACTION_VOLUME);

                gtk_widget_show (window);
                break;
        case 4:
                csd_osd_window_set_action_custom (CSD_OSD_WINDOW (window),
                                                  "media-eject-symbolic",
                                                  FALSE);

                gtk_widget_show (window);
                break;
        case 5:
                csd_osd_window_set_volume_level (CSD_OSD_WINDOW (window), 50);
                csd_osd_window_set_action_custom (CSD_OSD_WINDOW (window),
                                                  "display-brightness-symbolic",
                                                  TRUE);

                gtk_widget_show (window);
                break;
        case 6:
                csd_osd_window_set_volume_level (CSD_OSD_WINDOW (window), 50);
                csd_osd_window_set_action_custom (CSD_OSD_WINDOW (window),
                                                  "keyboard-brightness-symbolic",
                                                  TRUE);

                gtk_widget_show (window);
                break;
        case 7:
                csd_osd_window_set_action_custom (CSD_OSD_WINDOW (window),
                                                  "touchpad-disabled-symbolic",
                                                  TRUE);

                gtk_widget_show (window);
                break;
        case 8:
                csd_osd_window_set_action_custom (CSD_OSD_WINDOW (window),
                                                  "input-touchpad-symbolic",
                                                  TRUE);

                gtk_widget_show (window);
                break;
        default:
                gtk_main_quit ();
                break;
        }

        return TRUE;
}

static void
test_window (void)
{
        GtkWidget *window;

        window = csd_osd_window_new ();
        gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER_ALWAYS);

        csd_osd_window_set_volume_level (CSD_OSD_WINDOW (window), 0);
        csd_osd_window_set_action (CSD_OSD_WINDOW (window),
                                   CSD_OSD_WINDOW_ACTION_VOLUME);

        gtk_widget_show (window);

        g_timeout_add (3000, (GSourceFunc) update_state, window);
}

int
main (int    argc,
      char **argv)
{
        GError *error = NULL;

        bindtextdomain (GETTEXT_PACKAGE, CINNAMON_SETTINGS_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

        if (! gtk_init_with_args (&argc, &argv, NULL, NULL, NULL, &error)) {
                fprintf (stderr, "%s", error->message);
                g_error_free (error);
                exit (1);
        }

        gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                           DATADIR G_DIR_SEPARATOR_S "gnome-power-manager" G_DIR_SEPARATOR_S "icons");

        test_window ();

        gtk_main ();

        return 0;
}

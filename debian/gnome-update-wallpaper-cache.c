/*
 * Copyright (C) 2010 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License version 3.0 for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by Didier Roche <didrocks@ubuntu.com>
 * 
 * Bug: https://bugs.launchpad.net/bugs/530024
 */

#include <glib.h>
#include <gdk/gdk.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-bg.h>

static GOptionEntry entries[] =
{
  { NULL }
};

main (int argc, char *argv[])
{
    GOptionContext *context = NULL;
    GError         *error = NULL;

    GdkScreen *screen;
    GdkRectangle rect;
    GnomeBG *bg;
    GSettings *settings;
    GdkPixbuf *pixbuf;

    gdk_init (&argc, &argv);

    context = g_option_context_new ("- refresh wallpaper cache");
    g_option_context_add_main_entries (context, entries, NULL);
    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_printerr ("option parsing failed: %s\n", error->message);
        g_option_context_free(context);
        g_error_free (error);
        return (1);
    }
    if (context)
        g_option_context_free (context);

    /* cache only the first monitor */
    screen = gdk_screen_get_default ();
    gdk_screen_get_monitor_geometry (screen, 0, &rect);

    bg = gnome_bg_new ();
    settings = g_settings_new ("org.gnome.desktop.background");
    gnome_bg_load_from_preferences (bg, settings);

    pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, rect.width, rect.height);
    gnome_bg_draw (bg, pixbuf, screen, FALSE);

    g_object_unref (settings);

    return (0);
}

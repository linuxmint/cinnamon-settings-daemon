/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright © 2001 Ximian, Inc.
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright 2007 Red Hat, Inc.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-bg.h>
#include <X11/Xatom.h>

#include "gnome-settings-profile.h"
#include "gsd-background-manager.h"

#define GSD_BACKGROUND_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_BACKGROUND_MANAGER, GsdBackgroundManagerPrivate))

struct GsdBackgroundManagerPrivate
{
        GSettings   *settings;
        GnomeBG     *bg;

        GnomeBGCrossfade *fade;

        GDBusProxy  *proxy;
        guint        proxy_signal_id;
};

static void     gsd_background_manager_class_init  (GsdBackgroundManagerClass *klass);
static void     gsd_background_manager_init        (GsdBackgroundManager      *background_manager);
static void     gsd_background_manager_finalize    (GObject             *object);

static void setup_bg (GsdBackgroundManager *manager);
static void connect_screen_signals (GsdBackgroundManager *manager);

G_DEFINE_TYPE (GsdBackgroundManager, gsd_background_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static gboolean
dont_draw_background (GsdBackgroundManager *manager)
{
        return !g_settings_get_boolean (manager->priv->settings,
                                        "draw-background");
}

static gboolean
nautilus_is_drawing_background (GsdBackgroundManager *manager)
{
       Atom           window_id_atom;
       Window         nautilus_xid;
       Atom           actual_type;
       int            actual_format;
       unsigned long  nitems;
       unsigned long  bytes_after;
       unsigned char *data;
       Atom           wmclass_atom;
       gboolean       running;
       gint           error;
       gboolean       show_desktop_icons;

       show_desktop_icons = g_settings_get_boolean (manager->priv->settings,
                                                     "show-desktop-icons");
       if (! show_desktop_icons) {
               return FALSE;
       }

       window_id_atom = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                     "NAUTILUS_DESKTOP_WINDOW_ID", True);

       if (window_id_atom == None) {
               return FALSE;
       }

       XGetWindowProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                           GDK_ROOT_WINDOW (),
                           window_id_atom,
                           0,
                           1,
                           False,
                           XA_WINDOW,
                           &actual_type,
                           &actual_format,
                           &nitems,
                           &bytes_after,
                           &data);

       if (data != NULL) {
               nautilus_xid = *(Window *) data;
               XFree (data);
       } else {
               return FALSE;
       }

       if (actual_type != XA_WINDOW) {
               return FALSE;
       }
       if (actual_format != 32) {
               return FALSE;
       }

       wmclass_atom = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), "WM_CLASS", False);

       gdk_error_trap_push ();

       XGetWindowProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                           nautilus_xid,
                           wmclass_atom,
                           0,
                           24,
                           False,
                           XA_STRING,
                           &actual_type,
                           &actual_format,
                           &nitems,
                           &bytes_after,
                           &data);

       error = gdk_error_trap_pop ();

       if (error == BadWindow) {
               return FALSE;
       }

       if (actual_type == XA_STRING &&
           nitems == 24 &&
           bytes_after == 0 &&
           actual_format == 8 &&
           data != NULL &&
           !strcmp ((char *)data, "desktop_window") &&
           !strcmp ((char *)data + strlen ((char *)data) + 1, "Nautilus")) {
               running = TRUE;
       } else {
               running = FALSE;
       }

       if (data != NULL) {
               XFree (data);
       }

       return running;
}

static void
on_crossfade_finished (GsdBackgroundManager *manager)
{
        g_object_unref (manager->priv->fade);
        manager->priv->fade = NULL;
}

static void
draw_background (GsdBackgroundManager *manager,
                 gboolean              use_crossfade)
{
        GdkDisplay *display;
        int         n_screens;
        int         i;


        if (nautilus_is_drawing_background (manager) ||
            dont_draw_background (manager)) {
                return;
        }

        gnome_settings_profile_start (NULL);

        display = gdk_display_get_default ();
        n_screens = gdk_display_get_n_screens (display);

        for (i = 0; i < n_screens; ++i) {
                GdkScreen *screen;
                GdkWindow *root_window;
                cairo_surface_t *surface;

                screen = gdk_display_get_screen (display, i);

                root_window = gdk_screen_get_root_window (screen);

                surface = gnome_bg_create_surface (manager->priv->bg,
                                                   root_window,
                                                   gdk_screen_get_width (screen),
                                                   gdk_screen_get_height (screen),
                                                   TRUE);

                if (use_crossfade) {

                        if (manager->priv->fade != NULL) {
                                g_object_unref (manager->priv->fade);
                        }

                        manager->priv->fade = gnome_bg_set_surface_as_root_with_crossfade (screen, surface);
                        g_signal_connect_swapped (manager->priv->fade, "finished",
                                                  G_CALLBACK (on_crossfade_finished),
                                                  manager);
                } else {
                        gnome_bg_set_surface_as_root (screen, surface);
                }

                cairo_surface_destroy (surface);
        }

        gnome_settings_profile_end (NULL);
}

static void
on_bg_transitioned (GnomeBG              *bg,
                    GsdBackgroundManager *manager)
{
        draw_background (manager, FALSE);
}

static gboolean
settings_change_event_cb (GSettings            *settings,
                          gpointer              keys,
                          gint                  n_keys,
                          GsdBackgroundManager *manager)
{
        gnome_bg_load_from_preferences (manager->priv->bg,
                                        manager->priv->settings);
        return FALSE;
}

static void
on_screen_size_changed (GdkScreen            *screen,
                        GsdBackgroundManager *manager)
{
        draw_background (manager, FALSE);
}

static void
watch_bg_preferences (GsdBackgroundManager *manager)
{
        g_signal_connect (manager->priv->settings,
                          "change-event",
                          G_CALLBACK (settings_change_event_cb),
                          manager);
}

static void
on_bg_changed (GnomeBG              *bg,
               GsdBackgroundManager *manager)
{
        draw_background (manager, TRUE);
}

static void
setup_bg (GsdBackgroundManager *manager)
{
        g_return_if_fail (manager->priv->bg == NULL);

        manager->priv->bg = gnome_bg_new ();

        g_signal_connect (manager->priv->bg,
                          "changed",
                          G_CALLBACK (on_bg_changed),
                          manager);

        g_signal_connect (manager->priv->bg,
                          "transitioned",
                          G_CALLBACK (on_bg_transitioned),
                          manager);

        connect_screen_signals (manager);
        watch_bg_preferences (manager);
        gnome_bg_load_from_preferences (manager->priv->bg,
                                        manager->priv->settings);
}

static void
setup_bg_and_draw_background (GsdBackgroundManager *manager)
{
        setup_bg (manager);
        draw_background (manager, FALSE);
}

static void
disconnect_session_manager_listener (GsdBackgroundManager *manager)
{
        if (manager->priv->proxy && manager->priv->proxy_signal_id) {
                g_signal_handler_disconnect (manager->priv->proxy,
                                             manager->priv->proxy_signal_id);
                manager->priv->proxy_signal_id = 0;
        }
}

static void
on_session_manager_signal (GDBusProxy   *proxy,
                           const gchar  *sender_name,
                           const gchar  *signal_name,
                           GVariant     *parameters,
                           gpointer      user_data)
{
        GsdBackgroundManager *manager = GSD_BACKGROUND_MANAGER (user_data);

        if (g_strcmp0 (signal_name, "SessionRunning") == 0) {
                setup_bg_and_draw_background (manager);
                disconnect_session_manager_listener (manager);
        }
}

static void
draw_background_after_session_loads (GsdBackgroundManager *manager)
{
        GError *error = NULL;
        GDBusProxyFlags flags;

        flags = G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START;
        manager->priv->proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                              flags,
                                                              NULL, /* GDBusInterfaceInfo */
                                                              "org.gnome.SessionManager",
                                                              "/org/gnome/SessionManager",
                                                              "org.gnome.SessionManager",
                                                              NULL, /* GCancellable */
                                                              &error);
        if (manager->priv->proxy == NULL) {
                g_warning ("Could not listen to session manager: %s",
                           error->message);
                g_error_free (error);
                return;
        }

        manager->priv->proxy_signal_id = g_signal_connect (manager->priv->proxy,
                                                           "g-signal",
                                                           G_CALLBACK (on_session_manager_signal),
                                                           manager);
}


static void
disconnect_screen_signals (GsdBackgroundManager *manager)
{
        GdkDisplay *display;
        int         i;
        int         n_screens;

        display = gdk_display_get_default ();
        n_screens = gdk_display_get_n_screens (display);

        for (i = 0; i < n_screens; ++i) {
                GdkScreen *screen;
                screen = gdk_display_get_screen (display, i);
                g_signal_handlers_disconnect_by_func (screen,
                                                      G_CALLBACK (on_screen_size_changed),
                                                      manager);
        }
}

static void
connect_screen_signals (GsdBackgroundManager *manager)
{
        GdkDisplay *display;
        int         i;
        int         n_screens;

        display = gdk_display_get_default ();
        n_screens = gdk_display_get_n_screens (display);

        for (i = 0; i < n_screens; ++i) {
                GdkScreen *screen;
                screen = gdk_display_get_screen (display, i);
                g_signal_connect (screen,
                                  "monitors-changed",
                                  G_CALLBACK (on_screen_size_changed),
                                  manager);
                g_signal_connect (screen,
                                  "size-changed",
                                  G_CALLBACK (on_screen_size_changed),
                                  manager);
        }
}

static void
draw_background_changed (GSettings            *settings,
                         const char           *key,
                         GsdBackgroundManager *manager)
{
        if (dont_draw_background (manager) == FALSE)
                setup_bg_and_draw_background (manager);
}

gboolean
gsd_background_manager_start (GsdBackgroundManager *manager,
                              GError              **error)
{
        gboolean show_desktop_icons;

        g_debug ("Starting background manager");
        gnome_settings_profile_start (NULL);

        manager->priv->settings = g_settings_new ("org.gnome.desktop.background");
        g_signal_connect (manager->priv->settings, "changed::draw-background",
                          G_CALLBACK (draw_background_changed), manager);

        /* If this is set, nautilus will draw the background and is
	 * almost definitely in our session.  however, it may not be
	 * running yet (so is_nautilus_running() will fail).  so, on
	 * startup, just don't do anything if this key is set so we
	 * don't waste time setting the background only to have
	 * nautilus overwrite it.
	 */
        show_desktop_icons = g_settings_get_boolean (manager->priv->settings,
                                                     "show-desktop-icons");

        if (!show_desktop_icons) {
                setup_bg (manager);
        } else {
                draw_background_after_session_loads (manager);
        }

        gnome_settings_profile_end (NULL);

        return TRUE;
}

void
gsd_background_manager_stop (GsdBackgroundManager *manager)
{
        GsdBackgroundManagerPrivate *p = manager->priv;

        g_debug ("Stopping background manager");

        disconnect_screen_signals (manager);

        if (manager->priv->proxy) {
                disconnect_session_manager_listener (manager);
                g_object_unref (manager->priv->proxy);
        }

        g_signal_handlers_disconnect_by_func (manager->priv->settings,
                                              settings_change_event_cb,
                                              manager);

        if (p->settings != NULL) {
                g_object_unref (p->settings);
                p->settings = NULL;
        }

        if (p->bg != NULL) {
                g_object_unref (p->bg);
                p->bg = NULL;
        }
}

static GObject *
gsd_background_manager_constructor (GType                  type,
                                    guint                  n_construct_properties,
                                    GObjectConstructParam *construct_properties)
{
        GsdBackgroundManager      *background_manager;

        background_manager = GSD_BACKGROUND_MANAGER (G_OBJECT_CLASS (gsd_background_manager_parent_class)->constructor (type,
                                                                                                                        n_construct_properties,
                                                                                                                        construct_properties));

        return G_OBJECT (background_manager);
}

static void
gsd_background_manager_class_init (GsdBackgroundManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = gsd_background_manager_constructor;
        object_class->finalize = gsd_background_manager_finalize;

        g_type_class_add_private (klass, sizeof (GsdBackgroundManagerPrivate));
}

static void
gsd_background_manager_init (GsdBackgroundManager *manager)
{
        manager->priv = GSD_BACKGROUND_MANAGER_GET_PRIVATE (manager);
}

static void
gsd_background_manager_finalize (GObject *object)
{
        GsdBackgroundManager *background_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_BACKGROUND_MANAGER (object));

        background_manager = GSD_BACKGROUND_MANAGER (object);

        g_return_if_fail (background_manager->priv != NULL);

        G_OBJECT_CLASS (gsd_background_manager_parent_class)->finalize (object);
}

GsdBackgroundManager *
gsd_background_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (GSD_TYPE_BACKGROUND_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return GSD_BACKGROUND_MANAGER (manager_object);
}

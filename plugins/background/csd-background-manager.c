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
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
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
#include <libcinnamon-desktop/gnome-bg.h>
#include <X11/Xatom.h>

#include "cinnamon-settings-profile.h"
#include "csd-background-manager.h"

#define CSD_BACKGROUND_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CSD_TYPE_BACKGROUND_MANAGER, CsdBackgroundManagerPrivate))

struct CsdBackgroundManagerPrivate
{
        GSettings   *settings;
        GnomeBG     *bg;

        GnomeBGCrossfade *fade;

        guint        watch_id;
};

static void     csd_background_manager_class_init  (CsdBackgroundManagerClass *klass);
static void     csd_background_manager_init        (CsdBackgroundManager      *background_manager);
static void     csd_background_manager_finalize    (GObject             *object);

static void setup_bg (CsdBackgroundManager *manager);
static void connect_screen_signals (CsdBackgroundManager *manager);

G_DEFINE_TYPE (CsdBackgroundManager, csd_background_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static void
on_crossfade_finished (CsdBackgroundManager *manager)
{
        g_object_unref (manager->priv->fade);
        manager->priv->fade = NULL;
}

static void
draw_background (CsdBackgroundManager *manager,
                 gboolean              use_crossfade)
{
        GdkDisplay *display;
        int         n_screens;
        int         i;

        cinnamon_settings_profile_start (NULL);

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

        cinnamon_settings_profile_end (NULL);
}

static void
on_bg_transitioned (GnomeBG              *bg,
                    CsdBackgroundManager *manager)
{
        draw_background (manager, FALSE);
}

static gboolean
settings_change_event_cb (GSettings            *settings,
                          gpointer              keys,
                          gint                  n_keys,
                          CsdBackgroundManager *manager)
{
        gnome_bg_load_from_preferences (manager->priv->bg,
                                        manager->priv->settings);
        return FALSE;
}

static void
on_screen_size_changed (GdkScreen            *screen,
                        CsdBackgroundManager *manager)
{
        draw_background (manager, FALSE);
}

static void
watch_bg_preferences (CsdBackgroundManager *manager)
{
        g_signal_connect (manager->priv->settings,
                          "change-event",
                          G_CALLBACK (settings_change_event_cb),
                          manager);
}

static void
on_bg_changed (GnomeBG              *bg,
               CsdBackgroundManager *manager)
{
        draw_background (manager, TRUE);
}

static void
setup_bg (CsdBackgroundManager *manager)
{
        if (manager->priv->bg != NULL)
            return;

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
setup_bg_and_draw_background (CsdBackgroundManager *manager)
{
        setup_bg (manager);
        draw_background (manager, FALSE);
}

static void
on_dbus_appeared (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
    CsdBackgroundManager *manager = CSD_BACKGROUND_MANAGER (user_data);

    setup_bg_and_draw_background (manager);
}

static void
draw_background_after_cinnamon_loads (CsdBackgroundManager *manager)
{
        manager->priv->watch_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                                    "org.Cinnamon",
                                                    0,
                                                    on_dbus_appeared,
                                                    NULL,
                                                    manager,
                                                    NULL);
}

static void
disconnect_screen_signals (CsdBackgroundManager *manager)
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
connect_screen_signals (CsdBackgroundManager *manager)
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

gboolean
csd_background_manager_start (CsdBackgroundManager *manager,
                              GError              **error)
{
        gboolean show_desktop_icons;

        g_debug ("Starting background manager");
        cinnamon_settings_profile_start (NULL);

        manager->priv->settings = g_settings_new ("org.cinnamon.desktop.background");
        draw_background_after_cinnamon_loads (manager);

        cinnamon_settings_profile_end (NULL);

        return TRUE;
}

void
csd_background_manager_stop (CsdBackgroundManager *manager)
{
        CsdBackgroundManagerPrivate *p = manager->priv;

        g_debug ("Stopping background manager");

        disconnect_screen_signals (manager);

        g_signal_handlers_disconnect_by_func (manager->priv->settings,
                                              settings_change_event_cb,
                                              manager);

        if (manager->priv->watch_id > 0)
            g_bus_unwatch_name (manager->priv->watch_id);

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
csd_background_manager_constructor (GType                  type,
                                    guint                  n_construct_properties,
                                    GObjectConstructParam *construct_properties)
{
        CsdBackgroundManager      *background_manager;

        background_manager = CSD_BACKGROUND_MANAGER (G_OBJECT_CLASS (csd_background_manager_parent_class)->constructor (type,
                                                                                                                        n_construct_properties,
                                                                                                                        construct_properties));

        return G_OBJECT (background_manager);
}

static void
csd_background_manager_class_init (CsdBackgroundManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = csd_background_manager_constructor;
        object_class->finalize = csd_background_manager_finalize;

        g_type_class_add_private (klass, sizeof (CsdBackgroundManagerPrivate));
}

static void
csd_background_manager_init (CsdBackgroundManager *manager)
{
        manager->priv = CSD_BACKGROUND_MANAGER_GET_PRIVATE (manager);
}

static void
csd_background_manager_finalize (GObject *object)
{
        CsdBackgroundManager *background_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CSD_IS_BACKGROUND_MANAGER (object));

        background_manager = CSD_BACKGROUND_MANAGER (object);

        g_return_if_fail (background_manager->priv != NULL);

        G_OBJECT_CLASS (csd_background_manager_parent_class)->finalize (object);
}

CsdBackgroundManager *
csd_background_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (CSD_TYPE_BACKGROUND_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return CSD_BACKGROUND_MANAGER (manager_object);
}

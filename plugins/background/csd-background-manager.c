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
#include "monitor-background.h"

#define CSD_BACKGROUND_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CSD_TYPE_BACKGROUND_MANAGER, CsdBackgroundManagerPrivate))

struct CsdBackgroundManagerPrivate
{
        GSettings   *settings;
        GnomeBG     *bg;

        GnomeBGCrossfade *fade;

        GDBusProxy  *proxy;
        guint        proxy_signal_id;

        GPtrArray   *mbs;
};

static void     csd_background_manager_finalize    (GObject             *object);

static void setup_bg (CsdBackgroundManager *manager);
static void connect_screen_signals (CsdBackgroundManager *manager);

G_DEFINE_TYPE (CsdBackgroundManager, csd_background_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static void
draw_background_wayland_session (CsdBackgroundManager *manager)
{
    gint i;

    cinnamon_settings_profile_start (NULL);

    for (i = 0; i < manager->priv->mbs->len; i++)
    {
        GtkImage *image;

        MonitorBackground *mb = g_ptr_array_index (manager->priv->mbs, i);

        image = monitor_background_get_pending_image (mb);
        gnome_bg_create_and_set_gtk_image (manager->priv->bg, image, mb->width, mb->height);
        g_object_unref (image);

        monitor_background_show_next_image (mb);
    }

    cinnamon_settings_profile_end (NULL);
}

static void
draw_background_x11_session (CsdBackgroundManager *manager)
{
        GdkDisplay *display;

        cinnamon_settings_profile_start (NULL);

        display = gdk_display_get_default ();

        if (display)
        {
            GdkScreen *screen;
            GdkWindow *root_window;
            cairo_surface_t *surface;

            screen = gdk_display_get_screen (display, 0);

            root_window = gdk_screen_get_root_window (screen);

            surface = gnome_bg_create_surface (manager->priv->bg,
                                               root_window,
                                               gdk_screen_get_width (screen),
                                               gdk_screen_get_height (screen),
                                               TRUE);

            gnome_bg_set_surface_as_root (screen, surface);

            cairo_surface_destroy (surface);
        }

        cinnamon_settings_profile_end (NULL);
}

static gboolean
session_is_wayland (void)
{
    static gboolean session_is_wayland = FALSE;
    static gsize once_init = 0;

    if (g_once_init_enter (&once_init)) {
        const gchar *env = g_getenv ("XDG_SESSION_TYPE");
        if (env && g_strcmp0 (env, "wayland") == 0) {
            session_is_wayland = TRUE;
        }

        g_debug ("Session is Wayland? %d", session_is_wayland);

        g_once_init_leave (&once_init, 1);
    }

    return session_is_wayland;
}

static void
draw_background (CsdBackgroundManager *manager)
{
    if (session_is_wayland ()) {
        draw_background_wayland_session (manager);
    } else {
        draw_background_x11_session (manager);
    }
}

static void
on_bg_transitioned (GnomeBG              *bg,
                    CsdBackgroundManager *manager)
{
        draw_background (manager);
}

static gboolean
settings_change_event_cb (GSettings            *settings,
                          gpointer              keys,
                          gint                  n_keys,
                          CsdBackgroundManager *manager)
{
        gnome_bg_load_from_preferences (manager->priv->bg,
                                        manager->priv->settings);

        gnome_bg_set_accountsservice_background (gnome_bg_get_filename (manager->priv->bg));

        return FALSE;
}

static void
on_screen_size_changed (GdkScreen            *screen,
                        CsdBackgroundManager *manager)
{

        draw_background (manager);
}

static void
setup_monitors (CsdBackgroundManager *manager)
{
    GdkDisplay *display;
    gint i;

    display = gdk_display_get_default ();

    g_clear_pointer (&manager->priv->mbs, g_ptr_array_unref);
    manager->priv->mbs = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

    for (i = 0; i < gdk_display_get_n_monitors (display); i++)
    {
        GdkMonitor *monitor = gdk_display_get_monitor (display, i);
        MonitorBackground *mb = monitor_background_new (i, monitor);
        g_ptr_array_add (manager->priv->mbs, mb);
    }
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
    draw_background (manager);
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

        gnome_bg_set_accountsservice_background (gnome_bg_get_filename (manager->priv->bg));
}

static void
setup_bg_and_draw_background (CsdBackgroundManager *manager)
{
        setup_bg (manager);

        if (session_is_wayland ()) {
            setup_monitors (manager);
        }

        draw_background (manager);
}

static void
disconnect_session_manager_listener (CsdBackgroundManager *manager)
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
        CsdBackgroundManager *manager = CSD_BACKGROUND_MANAGER (user_data);

        if (g_strcmp0 (signal_name, "SessionRunning") == 0) {
                setup_bg_and_draw_background (manager);
                disconnect_session_manager_listener (manager);
        }
}

static void
draw_background_after_session_loads (CsdBackgroundManager *manager)
{
        GError *error = NULL;
        GDBusProxyFlags flags;
        GVariant *var = NULL;
        gboolean running = FALSE;

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

        var = g_dbus_proxy_call_sync (manager->priv->proxy,
                                      "IsSessionRunning",
                                      NULL,
                                      G_DBUS_CALL_FLAGS_NONE,
                                      -1,
                                      NULL,
                                      NULL);

        if (var != NULL) {
            g_variant_get (var, "(b)", &running);
            g_variant_unref (var);
        }

        if (running) {
            setup_bg_and_draw_background (manager);
        } else {
            manager->priv->proxy_signal_id = g_signal_connect (manager->priv->proxy,
                                                               "g-signal",
                                                               G_CALLBACK (on_session_manager_signal),
                                                               manager);
        }
}


static void
disconnect_screen_signals (CsdBackgroundManager *manager)
{
        GdkDisplay *display;

        display = gdk_display_get_default ();

        if (display)
        {
            GdkScreen *screen;

            screen = gdk_display_get_screen (display, 0);
            g_signal_handlers_disconnect_by_func (screen,
                                                  G_CALLBACK (on_screen_size_changed),
                                                  manager);
        }
}

static void
connect_screen_signals (CsdBackgroundManager *manager)
{
        GdkDisplay *display;

        display = gdk_display_get_default ();

        if (display)
        {
            GdkScreen *screen;

            screen = gdk_display_get_screen (display, 0);
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
        g_debug ("Starting background manager");
        cinnamon_settings_profile_start (NULL);

        manager->priv->settings = g_settings_new ("org.cinnamon.desktop.background");

        draw_background_after_session_loads (manager);

        cinnamon_settings_profile_end (NULL);

        return TRUE;
}

void
csd_background_manager_stop (CsdBackgroundManager *manager)
{
        CsdBackgroundManagerPrivate *p = manager->priv;

        g_debug ("Stopping background manager");

        disconnect_screen_signals (manager);

        if (manager->priv->proxy) {
                disconnect_session_manager_listener (manager);
                g_object_unref (manager->priv->proxy);
        }

        g_signal_handlers_disconnect_by_func (manager->priv->settings,
                                              settings_change_event_cb,
                                              manager);

        g_clear_pointer (&manager->priv->mbs, g_ptr_array_unref);
        g_clear_object (&p->settings);
        g_clear_object (&p->bg);
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

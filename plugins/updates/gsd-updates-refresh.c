/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <packagekit-glib2/packagekit.h>
#include <libupower-glib/upower.h>

#include "gsd-updates-common.h"
#include "gsd-updates-refresh.h"

static void     gsd_updates_refresh_finalize    (GObject            *object);

#define GSD_UPDATES_REFRESH_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_UPDATES_REFRESH, GsdUpdatesRefreshPrivate))

#define PERIODIC_CHECK_TIME     60*60   /* poke PackageKit every hour */
#define LOGIN_TIMEOUT           3       /* seconds */
#define SESSION_STARTUP_TIMEOUT 10      /* seconds */

enum {
        PRESENCE_STATUS_AVAILABLE = 0,
        PRESENCE_STATUS_INVISIBLE,
        PRESENCE_STATUS_BUSY,
        PRESENCE_STATUS_IDLE,
        PRESENCE_STATUS_UNKNOWN
};

/*
 * at startup, after a small delay, force a GetUpdates call
 * every hour (or any event) check:
   - if we are online, idle and on AC power, it's been more than a day
     since we refreshed then RefreshCache
   - if we are online and it's been longer than the timeout since
     getting the updates period then GetUpdates
*/

struct GsdUpdatesRefreshPrivate
{
        gboolean                 session_idle;
        gboolean                 on_battery;
        gboolean                 network_active;
        guint                    timeout_id;
        guint                    periodic_id;
        UpClient                *client;
        GSettings               *settings;
        GDBusProxy              *proxy_session;
        PkControl               *control;
};

enum {
        REFRESH_CACHE,
        GET_UPDATES,
        GET_UPGRADES,
        LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GsdUpdatesRefresh, gsd_updates_refresh, G_TYPE_OBJECT)

static void
gsd_updates_refresh_class_init (GsdUpdatesRefreshClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gsd_updates_refresh_finalize;
        g_type_class_add_private (klass, sizeof (GsdUpdatesRefreshPrivate));
        signals [REFRESH_CACHE] =
                g_signal_new ("refresh-cache",
                              G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                              0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        signals [GET_UPDATES] =
                g_signal_new ("get-updates",
                              G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                              0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        signals [GET_UPGRADES] =
                g_signal_new ("get-upgrades",
                              G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                              0, NULL, NULL, g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
}

static void
get_time_refresh_cache_cb (GObject *object,
                           GAsyncResult *res,
                           GsdUpdatesRefresh *refresh)
{
        PkControl *control = PK_CONTROL (object);
        GError *error = NULL;
        guint seconds;
        guint thresh;

        /* get the result */
        seconds = pk_control_get_time_since_action_finish (control, res, &error);
        if (seconds == 0) {
                g_warning ("failed to get time: %s", error->message);
                g_error_free (error);
                return;
        }

        /* have we passed the timout? */
        thresh = g_settings_get_int (refresh->priv->settings,
                                     GSD_SETTINGS_FREQUENCY_GET_UPDATES);
        if (seconds < thresh) {
                g_debug ("not before timeout, thresh=%u, now=%u", thresh, seconds);
                return;
        }

        /* send signal */
        g_debug ("emitting refresh-cache");
        g_signal_emit (refresh, signals [REFRESH_CACHE], 0);
}

static void
maybe_refresh_cache (GsdUpdatesRefresh *refresh)
{
        guint thresh;

        g_return_if_fail (GSD_IS_UPDATES_REFRESH (refresh));

        /* if we don't want to auto check for updates, don't do this either */
        thresh = g_settings_get_int (refresh->priv->settings,
                                     GSD_SETTINGS_FREQUENCY_GET_UPDATES);
        if (thresh == 0) {
                g_debug ("not when policy is set to never");
                return;
        }

        /* only do the refresh cache when the user is idle */
        if (!refresh->priv->session_idle) {
                g_debug ("not when session active");
                return;
        }

        /* get this each time, as it may have changed behind out back */
        thresh = g_settings_get_int (refresh->priv->settings,
                                     GSD_SETTINGS_FREQUENCY_REFRESH_CACHE);
        if (thresh == 0) {
                g_debug ("not when policy is set to never");
                return;
        }

        /* get the time since the last refresh */
        pk_control_get_time_since_action_async (refresh->priv->control,
                                                PK_ROLE_ENUM_REFRESH_CACHE,
                                                NULL,
                                                (GAsyncReadyCallback) get_time_refresh_cache_cb,
                                                refresh);
}

static void
get_time_get_updates_cb (GObject *object, GAsyncResult *res, GsdUpdatesRefresh *refresh)
{
        PkControl *control = PK_CONTROL (object);
        GError *error = NULL;
        guint seconds;
        guint thresh;

        /* get the result */
        seconds = pk_control_get_time_since_action_finish (control, res, &error);
        if (seconds == 0) {
                g_warning ("failed to get time: %s", error->message);
                g_error_free (error);
                return;
        }

        /* have we passed the timout? */
        thresh = g_settings_get_int (refresh->priv->settings,
                                     GSD_SETTINGS_FREQUENCY_GET_UPDATES);
        if (seconds < thresh) {
                g_debug ("not before timeout, thresh=%u, now=%u", thresh, seconds);
                return;
        }

        /* send signal */
        g_debug ("emitting get-updates");
        g_signal_emit (refresh, signals [GET_UPDATES], 0);
}

static void
maybe_get_updates (GsdUpdatesRefresh *refresh)
{
        guint thresh;

        g_return_if_fail (GSD_IS_UPDATES_REFRESH (refresh));

        /* if we don't want to auto check for updates, don't do this either */
        thresh = g_settings_get_int (refresh->priv->settings,
                                     GSD_SETTINGS_FREQUENCY_GET_UPDATES);
        if (thresh == 0) {
                g_debug ("not when policy is set to never");
                return;
        }

        /* get the time since the last refresh */
        pk_control_get_time_since_action_async (refresh->priv->control,
                                                PK_ROLE_ENUM_GET_UPDATES,
                                                NULL,
                                                (GAsyncReadyCallback) get_time_get_updates_cb,
                                                refresh);
}

static void
get_time_get_upgrades_cb (GObject *object,
                          GAsyncResult *res,
                          GsdUpdatesRefresh *refresh)
{
        PkControl *control = PK_CONTROL (object);
        GError *error = NULL;
        guint seconds;
        guint thresh;

        /* get the result */
        seconds = pk_control_get_time_since_action_finish (control, res, &error);
        if (seconds == 0) {
                g_warning ("failed to get time: %s", error->message);
                g_error_free (error);
                return;
        }

        /* have we passed the timout? */
        thresh = g_settings_get_int (refresh->priv->settings,
                                     GSD_SETTINGS_FREQUENCY_GET_UPDATES);
        if (seconds < thresh) {
                g_debug ("not before timeout, thresh=%u, now=%u",
                         thresh, seconds);
                return;
        }

        /* send signal */
        g_debug ("emitting get-upgrades");
        g_signal_emit (refresh, signals [GET_UPGRADES], 0);
}

static void
maybe_get_upgrades (GsdUpdatesRefresh *refresh)
{
        guint thresh;

        g_return_if_fail (GSD_IS_UPDATES_REFRESH (refresh));

        /* get this each time, as it may have changed behind out back */
        thresh = g_settings_get_int (refresh->priv->settings,
                                     GSD_SETTINGS_FREQUENCY_GET_UPGRADES);
        if (thresh == 0) {
                g_debug ("not when policy is set to never");
                return;
        }

        /* get the time since the last refresh */
        pk_control_get_time_since_action_async (refresh->priv->control,
                                                PK_ROLE_ENUM_GET_DISTRO_UPGRADES,
                                                NULL,
                                                (GAsyncReadyCallback) get_time_get_upgrades_cb,
                                                refresh);
}

static gboolean
change_state_cb (GsdUpdatesRefresh *refresh)
{
        /* check all actions */
        maybe_refresh_cache (refresh);
        maybe_get_updates (refresh);
        maybe_get_upgrades (refresh);
        return FALSE;
}

static gboolean
change_state (GsdUpdatesRefresh *refresh)
{
        gboolean ret;

        g_return_val_if_fail (GSD_IS_UPDATES_REFRESH (refresh), FALSE);

        /* no point continuing if we have no network */
        if (!refresh->priv->network_active) {
                g_debug ("not when no network");
                return FALSE;
        }

        /* not on battery unless overridden */
        ret = g_settings_get_boolean (refresh->priv->settings,
                                      GSD_SETTINGS_UPDATE_BATTERY);
        if (!ret && refresh->priv->on_battery) {
                g_debug ("not when on battery");
                return FALSE;
        }

        /* wait a little time for things to settle down */
        if (refresh->priv->timeout_id != 0)
                g_source_remove (refresh->priv->timeout_id);
        g_debug ("defering action for %i seconds",
                 SESSION_STARTUP_TIMEOUT);
        refresh->priv->timeout_id =
                g_timeout_add_seconds (SESSION_STARTUP_TIMEOUT,
                                       (GSourceFunc) change_state_cb,
                                       refresh);
        g_source_set_name_by_id (refresh->priv->timeout_id,
                                 "[GsdUpdatesRefresh] change-state");

        return TRUE;
}

static void
settings_key_changed_cb (GSettings *client,
                         const gchar *key,
                         GsdUpdatesRefresh *refresh)
{
        g_return_if_fail (GSD_IS_UPDATES_REFRESH (refresh));
        if (g_strcmp0 (key, GSD_SETTINGS_FREQUENCY_GET_UPDATES) == 0 ||
            g_strcmp0 (key, GSD_SETTINGS_FREQUENCY_GET_UPGRADES) == 0 ||
            g_strcmp0 (key, GSD_SETTINGS_FREQUENCY_REFRESH_CACHE) == 0 ||
            g_strcmp0 (key, GSD_SETTINGS_UPDATE_BATTERY) == 0)
                change_state (refresh);
}

static gboolean
convert_network_state (GsdUpdatesRefresh *refresh, PkNetworkEnum state)
{
        /* offline */
        if (state == PK_NETWORK_ENUM_OFFLINE)
                return FALSE;

        /* online */
        if (state == PK_NETWORK_ENUM_ONLINE ||
            state == PK_NETWORK_ENUM_WIFI ||
            state == PK_NETWORK_ENUM_WIRED)
                return TRUE;

        /* check policy */
        if (state == PK_NETWORK_ENUM_MOBILE)
                return g_settings_get_boolean (refresh->priv->settings,
                                               GSD_SETTINGS_CONNECTION_USE_MOBILE);

        /* not recognised */
        g_warning ("state unknown: %i", state);
        return TRUE;
}

static void
notify_network_state_cb (PkControl *control,
                         GParamSpec *pspec,
                         GsdUpdatesRefresh *refresh)
{
        PkNetworkEnum state;

        g_return_if_fail (GSD_IS_UPDATES_REFRESH (refresh));

        g_object_get (control, "network-state", &state, NULL);
        refresh->priv->network_active = convert_network_state (refresh, state);
        g_debug ("setting online %i", refresh->priv->network_active);
        if (refresh->priv->network_active)
                change_state (refresh);
}

static gboolean
periodic_timeout_cb (gpointer user_data)
{
        GsdUpdatesRefresh *refresh = GSD_UPDATES_REFRESH (user_data);

        g_return_val_if_fail (GSD_IS_UPDATES_REFRESH (refresh), FALSE);

        /* debug so we can catch polling */
        g_debug ("polling check");

        /* triggered once an hour */
        change_state (refresh);

        /* always return */
        return TRUE;
}

static void
gsd_updates_refresh_client_changed_cb (UpClient *client,
                                       GsdUpdatesRefresh *refresh)
{
        gboolean on_battery;

        g_return_if_fail (GSD_IS_UPDATES_REFRESH (refresh));

        /* get the on-battery state */
        on_battery = up_client_get_on_battery (refresh->priv->client);
        if (on_battery == refresh->priv->on_battery) {
                g_debug ("same state as before, ignoring");
                return;
        }

        /* save in local cache */
        g_debug ("setting on_battery %i", on_battery);
        refresh->priv->on_battery = on_battery;
        if (!on_battery)
                change_state (refresh);
}

static void
get_properties_cb (GObject *object,
                   GAsyncResult *res,
                   GsdUpdatesRefresh *refresh)
{
        PkNetworkEnum state;
        GError *error = NULL;
        PkControl *control = PK_CONTROL(object);
        gboolean ret;

        /* get the result */
        ret = pk_control_get_properties_finish (control, res, &error);
        if (!ret) {
                /* TRANSLATORS: backend is broken, and won't tell us what it supports */
                g_warning ("could not get properties");
                g_error_free (error);
                goto out;
        }

        /* get values */
        g_object_get (control,
                      "network-state", &state,
                      NULL);
        refresh->priv->network_active = convert_network_state (refresh, state);
out:
        return;
}

static void
session_presence_signal_cb (GDBusProxy *proxy,
                            gchar *sender_name,
                            gchar *signal_name,
                            GVariant *parameters,
                            GsdUpdatesRefresh *refresh)
{
        guint status;

        g_return_if_fail (GSD_IS_UPDATES_REFRESH (refresh));

        if (g_strcmp0 (signal_name, "StatusChanged") != 0)
                return;

        /* map status code into boolean */
        g_variant_get (parameters, "(u)", &status);
        refresh->priv->session_idle = (status == PRESENCE_STATUS_IDLE);
        g_debug ("setting is_idle %i",
                 refresh->priv->session_idle);
        if (refresh->priv->session_idle)
                change_state (refresh);

}

static void
gsd_updates_refresh_init (GsdUpdatesRefresh *refresh)
{
        GError *error = NULL;
        GVariant *status;
        guint status_code;

        refresh->priv = GSD_UPDATES_REFRESH_GET_PRIVATE (refresh);
        refresh->priv->on_battery = FALSE;
        refresh->priv->network_active = FALSE;
        refresh->priv->timeout_id = 0;
        refresh->priv->periodic_id = 0;

        /* we need to know the updates frequency */
        refresh->priv->settings = g_settings_new (GSD_SETTINGS_SCHEMA);
        g_signal_connect (refresh->priv->settings, "changed",
                          G_CALLBACK (settings_key_changed_cb), refresh);

        /* we need to query the last cache refresh time */
        refresh->priv->control = pk_control_new ();
        g_signal_connect (refresh->priv->control, "notify::network-state",
                          G_CALLBACK (notify_network_state_cb),
                          refresh);

        /* get network state */
        pk_control_get_properties_async (refresh->priv->control,
                                         NULL,
                                         (GAsyncReadyCallback) get_properties_cb,
                                         refresh);

        /* use a UpClient */
        refresh->priv->client = up_client_new ();
        g_signal_connect (refresh->priv->client, "changed",
                          G_CALLBACK (gsd_updates_refresh_client_changed_cb), refresh);

        /* get the battery state */
        refresh->priv->on_battery = up_client_get_on_battery (refresh->priv->client);
        g_debug ("setting on battery %i", refresh->priv->on_battery);

        /* use gnome-session for the idle detection */
        refresh->priv->proxy_session =
                g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                               G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                               NULL, /* GDBusInterfaceInfo */
                                               "org.gnome.SessionManager",
                                               "/org/gnome/SessionManager/Presence",
                                               "org.gnome.SessionManager.Presence",
                                               NULL, /* GCancellable */
                                               &error);
        if (refresh->priv->proxy_session == NULL) {
                g_warning ("Error creating proxy: %s",
                           error->message);
                g_error_free (error);
        } else {
                g_signal_connect (refresh->priv->proxy_session,
                                  "g-signal",
                                  G_CALLBACK (session_presence_signal_cb),
                                  refresh);
                status = g_dbus_proxy_get_cached_property (refresh->priv->proxy_session,
                                                           "status");
                if (status) {
                        g_variant_get (status, "u", &status_code);
                        refresh->priv->session_idle = (status_code == PRESENCE_STATUS_IDLE);
                        g_variant_unref (status);
                }
                else {
                        refresh->priv->session_idle = FALSE;
                }
        }

        /* we check this in case we miss one of the async signals */
        refresh->priv->periodic_id =
                g_timeout_add_seconds (PERIODIC_CHECK_TIME,
                                       periodic_timeout_cb, refresh);
        g_source_set_name_by_id (refresh->priv->periodic_id,
                                 "[GsdUpdatesRefresh] periodic check");

        /* check system state */
        change_state (refresh);
}

static void
gsd_updates_refresh_finalize (GObject *object)
{
        GsdUpdatesRefresh *refresh;

        g_return_if_fail (GSD_IS_UPDATES_REFRESH (object));

        refresh = GSD_UPDATES_REFRESH (object);
        g_return_if_fail (refresh->priv != NULL);

        if (refresh->priv->timeout_id != 0)
                g_source_remove (refresh->priv->timeout_id);
        if (refresh->priv->periodic_id != 0)
                g_source_remove (refresh->priv->periodic_id);

        g_signal_handlers_disconnect_by_data (refresh->priv->client, refresh);

        g_object_unref (refresh->priv->control);
        g_object_unref (refresh->priv->settings);
        g_object_unref (refresh->priv->client);
        if (refresh->priv->proxy_session != NULL)
                g_object_unref (refresh->priv->proxy_session);

        G_OBJECT_CLASS (gsd_updates_refresh_parent_class)->finalize (object);
}

GsdUpdatesRefresh *
gsd_updates_refresh_new (void)
{
        GsdUpdatesRefresh *refresh;
        refresh = g_object_new (GSD_TYPE_UPDATES_REFRESH, NULL);
        return GSD_UPDATES_REFRESH (refresh);
}


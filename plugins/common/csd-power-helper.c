/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Bastien Nocera <hadess@hadess.net>
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

#include <gio/gio.h>

#include "config.h"

#include "csd-power-helper.h"

#define LOGIND_DBUS_NAME                       "org.freedesktop.login1"
#define LOGIND_DBUS_PATH                       "/org/freedesktop/login1"
#define LOGIND_DBUS_INTERFACE                  "org.freedesktop.login1.Manager"

#define CONSOLEKIT_DBUS_NAME                    "org.freedesktop.ConsoleKit"
#define CONSOLEKIT_DBUS_PATH_MANAGER            "/org/freedesktop/ConsoleKit/Manager"
#define CONSOLEKIT_DBUS_INTERFACE_MANAGER       "org.freedesktop.ConsoleKit.Manager"

static void
logind_stop (void)
{
        GDBusConnection *bus;

        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
        g_dbus_connection_call (bus,
                                LOGIND_DBUS_NAME,
                                LOGIND_DBUS_PATH,
                                LOGIND_DBUS_INTERFACE,
                                "PowerOff",
                                g_variant_new ("(b)", FALSE),
                                NULL, 0, G_MAXINT, NULL, NULL, NULL);
        g_object_unref (bus);
}

static void
logind_suspend (void)
{
        GDBusConnection *bus;

        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
        g_dbus_connection_call (bus,
                                LOGIND_DBUS_NAME,
                                LOGIND_DBUS_PATH,
                                LOGIND_DBUS_INTERFACE,
                                "Suspend",
                                g_variant_new ("(b)", TRUE),
                                NULL, 0, G_MAXINT, NULL, NULL, NULL);
        g_object_unref (bus);
}

static void
logind_hybrid_suspend (void)
{
        GDBusConnection *bus;

        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
        g_dbus_connection_call (bus,
                                LOGIND_DBUS_NAME,
                                LOGIND_DBUS_PATH,
                                LOGIND_DBUS_INTERFACE,
                                "HybridSleep",
                                g_variant_new ("(b)", TRUE),
                                NULL, 0, G_MAXINT, NULL, NULL, NULL);
        g_object_unref (bus);
}

static gboolean
can_hybrid_sleep (void)
{
        GDBusConnection *bus;
        GVariant *res;
        gchar *rv;
        gboolean can_hybrid;
        GError *error = NULL;

        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
        res = g_dbus_connection_call_sync (bus,
                                           LOGIND_DBUS_NAME,
                                           LOGIND_DBUS_PATH,
                                           LOGIND_DBUS_INTERFACE,
                                           "CanHybridSleep",
                                           NULL,
                                           G_VARIANT_TYPE_TUPLE,
                                           0, G_MAXINT, NULL, &error);

        g_object_unref (bus);

        if (error) {
          g_warning ("Calling CanHybridSleep failed: %s", error->message);
          g_clear_error (&error);

          return FALSE;
        }

        g_variant_get (res, "(s)", &rv);
        g_variant_unref (res);

        can_hybrid = g_strcmp0 (rv, "yes") == 0 ||
                     g_strcmp0 (rv, "challenge") == 0;

        if (!can_hybrid) {
          g_warning ("logind does not support hybrid sleep");
        }

        g_free (rv);

        return can_hybrid;
}

static void
logind_hibernate (void)
{
        GDBusConnection *bus;

        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
        g_dbus_connection_call (bus,
                                LOGIND_DBUS_NAME,
                                LOGIND_DBUS_PATH,
                                LOGIND_DBUS_INTERFACE,
                                "Hibernate",
                                g_variant_new ("(b)", TRUE),
                                NULL, 0, G_MAXINT, NULL, NULL, NULL);
        g_object_unref (bus);
}

static void
consolekit_stop_cb (GObject *source_object,
                    GAsyncResult *res,
                    gpointer user_data)
{
        GVariant *result;
        GError *error = NULL;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                           res,
                                           &error);
        if (result == NULL) {
                g_warning ("couldn't stop using ConsoleKit: %s",
                           error->message);
                g_error_free (error);
        } else {
                g_variant_unref (result);
        }
}

static void
consolekit_stop (void)
{
        GError *error = NULL;
        GDBusProxy *proxy;

        /* power down the machine in a safe way */
        proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                               NULL,
                                               CONSOLEKIT_DBUS_NAME,
                                               CONSOLEKIT_DBUS_PATH_MANAGER,
                                               CONSOLEKIT_DBUS_INTERFACE_MANAGER,
                                               NULL, &error);
        if (proxy == NULL) {
                g_warning ("cannot connect to ConsoleKit: %s",
                           error->message);
                g_error_free (error);
                return;
        }
        g_dbus_proxy_call (proxy,
                           "Stop",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL,
                           consolekit_stop_cb, NULL);
        g_object_unref (proxy);
}

static void
consolekit_sleep_cb (GObject *source_object,
                     GAsyncResult *res,
                     gpointer user_data) 
{
        GVariant *result;
        GError *error = NULL;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                           res,
                                           &error);
        if (result == NULL) {
                g_warning ("couldn't sleep using ConsoleKit: %s",
                           error->message);
                g_error_free (error);
        } else {
                g_variant_unref (result);
        }
}

static void
consolekit_suspend (void)
{
        GError *error = NULL;
        GDBusProxy *proxy;
        
        proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                               NULL,
                                               CONSOLEKIT_DBUS_NAME,
                                               CONSOLEKIT_DBUS_PATH_MANAGER,
                                               CONSOLEKIT_DBUS_INTERFACE_MANAGER,
                                               NULL, &error);
        if (proxy == NULL) {
                g_warning ("cannot connect to ConsoleKit: %s",
                           error->message);
                g_error_free (error);
                return;
        }
        g_dbus_proxy_call (proxy,
                           "Suspend",
                           g_variant_new("(b)", TRUE),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL,
                           consolekit_sleep_cb, NULL);
        g_object_unref (proxy);
}

static void
consolekit_hibernate (void)
{
        GError *error = NULL;
        GDBusProxy *proxy;
        
        proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                               NULL,
                                               CONSOLEKIT_DBUS_NAME,
                                               CONSOLEKIT_DBUS_PATH_MANAGER,
                                               CONSOLEKIT_DBUS_INTERFACE_MANAGER,
                                               NULL, &error);
        if (proxy == NULL) {
                g_warning ("cannot connect to ConsoleKit: %s",
                           error->message);
                g_error_free (error);
                return;
        }
        g_dbus_proxy_call (proxy,
                           "Hibernate",
                           g_variant_new("(b)", TRUE),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL,
                           consolekit_sleep_cb, NULL);
        g_object_unref (proxy);
}

static void
consolekit_hybrid_suspend (void)
{
        GError *error = NULL;
        GDBusProxy *proxy;
        
        proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                               NULL,
                                               CONSOLEKIT_DBUS_NAME,
                                               CONSOLEKIT_DBUS_PATH_MANAGER,
                                               CONSOLEKIT_DBUS_INTERFACE_MANAGER,
                                               NULL, &error);
        if (proxy == NULL) {
                g_warning ("cannot connect to ConsoleKit: %s",
                           error->message);
                g_error_free (error);
                return;
        }
        g_dbus_proxy_call (proxy,
                           "HybridSleep",
                           g_variant_new("(b)", TRUE),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL,
                           consolekit_sleep_cb, NULL);
        g_object_unref (proxy);
}

void
csd_power_suspend (gboolean    use_logind,
                   gboolean    try_hybrid)
{
  if (use_logind) {
    if (try_hybrid && can_hybrid_sleep ()) {
      logind_hybrid_suspend ();
    }
    else {
      logind_suspend ();
    }
  }
  else {
    if (try_hybrid && can_hybrid_sleep ()) {
      consolekit_hybrid_suspend ();
    }
    else {
      consolekit_suspend ();
    }
  }
}

void
csd_power_poweroff (gboolean use_logind)
{
  if (use_logind) {
    logind_stop ();
  }
  else {
    consolekit_stop ();
  }
}

void
csd_power_hibernate (gboolean use_logind)
{
  if (use_logind) {
    logind_hibernate ();
  }
  else {
    consolekit_hibernate ();
  }
}

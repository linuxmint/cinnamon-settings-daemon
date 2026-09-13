/*
 * Copyright (C) 2022 Benjamin Berg <bberg@redhat.com>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <string.h>

#include "csd-systemd-notify.h"
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <libnotify/notify.h>

struct _CsdSystemdNotify {
        GObject parent;

        GDBusConnection *session;
        guint sub_service;
        guint sub_scope;
};

G_DEFINE_TYPE (CsdSystemdNotify, csd_systemd_notify, G_TYPE_OBJECT)

static void
notify_oom_kill (char *unit)
{
        g_autoptr(GDesktopAppInfo) app = NULL;
        g_autofree char *unit_copy = NULL;
        g_autofree char *app_id = NULL;
        g_autofree char *desktop_id = NULL;
        g_autofree char *summary = NULL;
        g_autofree char *message = NULL;
        NotifyNotification *notification = NULL;
        char *pos;

        unit_copy = g_strdup (unit);

        if (g_str_has_suffix (unit_copy, ".service")) {
                /* Find (first) @ character */
                pos = strchr (unit_copy, '@');
                if (pos)
                        *pos = '\0';
        } else if (g_str_has_suffix (unit_copy, ".scope")) {
                /* Find last - character */
                pos = strrchr (unit_copy, '-');
                if (pos)
                        *pos = '\0';
        } else {
                /* This cannot happen, because we only subscribe to the Scope
                 * and Service DBus interfaces.
                 */
                g_assert_not_reached ();
                return;
        }


        pos = strrchr (unit_copy, '-');
        if (pos) {
                pos += 1;

                app_id = g_strcompress (pos);
                desktop_id = g_strjoin (NULL, app_id, ".desktop", NULL);

                app = g_desktop_app_info_new (desktop_id);
        }

        if (app) {
                /* TRANSLATORS: %s is the application name. */
                summary = g_strdup_printf (_("%s Stopped"),
                                           g_app_info_get_name (G_APP_INFO (app)));
                /* TRANSLATORS: %s is the application name. */
                message = g_strdup_printf (_("Device memory is nearly full. %s was using a lot of memory and was forced to stop."),
                                           g_app_info_get_name (G_APP_INFO (app)));
        } else if (g_str_has_prefix (unit, "vte-spawn-")) {
                /* TRANSLATORS: A terminal tab/window was killed. */
                summary = g_strdup_printf (_("Virtual Terminal Stopped"));
                /* TRANSLATORS: A terminal tab/window was killed. */
                message = g_strdup_printf (_("Device memory is nearly full. Virtual terminal processes were using a lot of memory and were forced to stop."));
        } else {
                /* TRANSLATORS: We don't have a good description of what was killed. */
                summary = g_strdup_printf (_("Application Stopped"));
                /* TRANSLATORS: We don't have a good description of what was killed. */
                message = g_strdup_printf (_("Device memory is nearly full. An application was using a lot of memory and was forced to stop."));
        }

        notification = notify_notification_new (summary, message, NULL);

        if (app) {
                notify_notification_set_hint_string (notification, "desktop-entry", desktop_id);
                notify_notification_set_app_name (notification, g_app_info_get_name (G_APP_INFO (app)));
        }
        notify_notification_set_hint (notification, "image-path", g_variant_new_string ("dialog-warning-symbolic"));
        notify_notification_set_hint (notification, "transient", g_variant_new_boolean (TRUE));
        notify_notification_set_urgency (notification, NOTIFY_URGENCY_CRITICAL);
        notify_notification_set_timeout (notification, NOTIFY_EXPIRES_DEFAULT);

        notify_notification_show (notification, NULL);
        g_object_unref (notification);
}

/* Taken from hexdecoct.c in systemd, LGPL-2.1-or-later */
static int
unhexchar (char c)
{
        if (c >= '0' && c <= '9')
                return c - '0';

        if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;

        if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;

        return -EINVAL;
}


static char*
unescape_dbus_path (const char *path)
{
        g_autofree char *res = g_malloc (strlen (path) + 1);
        char *r;

        for (r = res; *path; path += 1, r += 1) {
                int c1, c2;
                if (*path != '_') {
                        *r = *path;
                        continue;
                }
                /* Read next two hex characters */
                path += 1;
                c1 = unhexchar (*path);
                if (c1 < 0)
                        return NULL;
                path += 1;
                c2 = unhexchar (*path);
                if (c2 < 0)
                        return NULL;

                *r = (c1 << 4) | c2;
        }
        *r = '\0';

        return g_steal_pointer (&res);
}

static void
on_unit_properties_changed (GDBusConnection *connection,
                            const char *sender_name,
                            const char *object_path,
                            const char *interface_name,
                            const char *signal_name,
                            GVariant *parameters,
                            gpointer user_data)
{
        g_autoptr(GVariant) dict = NULL;
        const char *result = NULL;
        const char *unit_escaped = NULL;
        g_autofree char *unit = NULL;

        g_assert (g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(sa{sv}as)")));

        dict = g_variant_get_child_value (parameters, 1);
        g_assert (dict);

        unit_escaped = strrchr (object_path, '/');
        g_assert (unit_escaped);
        unit_escaped += 1;

        unit = unescape_dbus_path (unit_escaped);
        g_assert (unit);

        if (g_variant_lookup (dict, "Result", "&s", &result)) {
                if (g_strcmp0 (result, "oom-kill") == 0)
                        notify_oom_kill (unit);
        }
}

static void
on_bus_gotten (GDBusConnection  *obj,
               GAsyncResult     *res,
               CsdSystemdNotify *self)
{
        g_autoptr(GError) error = NULL;
        GDBusConnection *con;

        con = g_bus_get_finish (res, &error);
        if (!con) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to get session bus: %s", error->message);
                return;
        }

        self->session = con;

        /* Subscribe to systemd events by calling Subscribe on
	 * org.freedesktop.systemd1.Manager.
	 */
        g_dbus_connection_call (self->session,
                                "org.freedesktop.systemd1",
                                "/org/freedesktop/systemd1",
                                "org.freedesktop.systemd1.Manager",
                                "Subscribe",
                                NULL,
                                G_VARIANT_TYPE ("()"),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                NULL,
                                NULL,
                                NULL);

        self->sub_service = g_dbus_connection_signal_subscribe (self->session,
                                                                "org.freedesktop.systemd1",
                                                                "org.freedesktop.DBus.Properties",
                                                                "PropertiesChanged",
                                                                NULL,
                                                                "org.freedesktop.systemd1.Service",
                                                                G_DBUS_SIGNAL_FLAGS_MATCH_ARG0_NAMESPACE,
                                                                on_unit_properties_changed,
                                                                self,
                                                                NULL);

        self->sub_scope = g_dbus_connection_signal_subscribe (self->session,
                                                              "org.freedesktop.systemd1",
                                                              "org.freedesktop.DBus.Properties",
                                                              "PropertiesChanged",
                                                              NULL,
                                                              "org.freedesktop.systemd1.Scope",
                                                              G_DBUS_SIGNAL_FLAGS_MATCH_ARG0_NAMESPACE,
                                                              on_unit_properties_changed,
                                                              self,
                                                              NULL);
}

static void
csd_systemd_notify_init (CsdSystemdNotify *self)
{
        g_bus_get (G_BUS_TYPE_SESSION, NULL, (GAsyncReadyCallback) on_bus_gotten, self);
}

static void
csd_systemd_notify_dispose (GObject *obj)
{
        CsdSystemdNotify *self = CSD_SYSTEMD_NOTIFY (obj);

        if (self->sub_service) {
                g_dbus_connection_signal_unsubscribe (self->session, self->sub_service);
                g_dbus_connection_signal_unsubscribe (self->session, self->sub_scope);
        }
        self->sub_service = 0;
        self->sub_scope = 0;
        g_clear_object (&self->session);

        G_OBJECT_CLASS (csd_systemd_notify_parent_class)->dispose (obj);
}

static void
csd_systemd_notify_class_init (CsdSystemdNotifyClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = csd_systemd_notify_dispose;
}


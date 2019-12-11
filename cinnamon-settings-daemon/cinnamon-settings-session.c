/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2011 Richard Hughes <richard@hughsie.com>
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

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>

#include "cinnamon-settings-session.h"

#ifdef HAVE_LOGIND
#include <systemd/sd-login.h>
#endif

typedef struct
{
  GSource source;
  GPollFD pollfd;
#ifdef HAVE_LOGIND
  sd_login_monitor *monitor;
#endif
} SdSource;

#ifdef HAVE_LOGIND
static gboolean
sd_source_prepare (GSource *source,
                   gint    *timeout)
{
  *timeout = -1;
  return FALSE;
}

static gboolean
sd_source_check (GSource *source)
{
  SdSource *sd_source = (SdSource *)source;

  return sd_source->pollfd.revents != 0;
}

static gboolean
sd_source_dispatch (GSource     *source,
                    GSourceFunc  callback,
                    gpointer     user_data)

{
  SdSource *sd_source = (SdSource *)source;
  gboolean ret;

  g_warn_if_fail (callback != NULL);

  ret = (*callback) (user_data);

  sd_login_monitor_flush (sd_source->monitor);
  return ret;
}

static void
sd_source_finalize (GSource *source)
{
  SdSource *sd_source = (SdSource*)source;

  sd_login_monitor_unref (sd_source->monitor);
}

static GSourceFuncs sd_source_funcs = {
  sd_source_prepare,
  sd_source_check,
  sd_source_dispatch,
  sd_source_finalize
};

static GSource *
sd_source_new (void)
{
  GSource *source;
  SdSource *sd_source;
  int ret;

  source = g_source_new (&sd_source_funcs, sizeof (SdSource));
  sd_source = (SdSource *)source;

  if ((ret = sd_login_monitor_new (NULL, &sd_source->monitor)) < 0)
    {
      g_printerr ("Error getting login monitor: %d", ret);
    }
  else
    {
      sd_source->pollfd.fd = sd_login_monitor_get_fd (sd_source->monitor);
      sd_source->pollfd.events = G_IO_IN;
      g_source_add_poll (source, &sd_source->pollfd);
    }

  return source;
}

#endif

static void     cinnamon_settings_session_finalize	(GObject		*object);

#define CINNAMON_SETTINGS_SESSION_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CINNAMON_TYPE_SETTINGS_SESSION, CinnamonSettingsSessionPrivate))

#define CONSOLEKIT_NAME			"org.freedesktop.ConsoleKit"
#define CONSOLEKIT_PATH			"/org/freedesktop/ConsoleKit"
#define CONSOLEKIT_INTERFACE		"org.freedesktop.ConsoleKit"

#define CONSOLEKIT_MANAGER_PATH	 	"/org/freedesktop/ConsoleKit/Manager"
#define CONSOLEKIT_MANAGER_INTERFACE    "org.freedesktop.ConsoleKit.Manager"
#define CONSOLEKIT_SEAT_INTERFACE       "org.freedesktop.ConsoleKit.Seat"
#define CONSOLEKIT_SESSION_INTERFACE    "org.freedesktop.ConsoleKit.Session"

struct CinnamonSettingsSessionPrivate
{
	GSource			*sd_source; // used in logind mode
	GDBusProxy		*proxy_session; // used in consolekit mode
	GCancellable	*cancellable; // used in consolekit mode
	gchar			*session_id;
	CinnamonSettingsSessionState state;
};

enum {
	PROP_0,
	PROP_STATE,
	PROP_LAST
};

G_DEFINE_TYPE (CinnamonSettingsSession, cinnamon_settings_session, G_TYPE_OBJECT)

CinnamonSettingsSessionState
cinnamon_settings_session_get_state (CinnamonSettingsSession *session)
{
	g_return_val_if_fail (CINNAMON_IS_SETTINGS_SESSION (session),
			      CINNAMON_SETTINGS_SESSION_STATE_UNKNOWN);
	return session->priv->state;
}

static void
cinnamon_settings_session_set_state (CinnamonSettingsSession *session,
				  gboolean active)
{
        CinnamonSettingsSessionState  state;

        state = active ? CINNAMON_SETTINGS_SESSION_STATE_ACTIVE
                       : CINNAMON_SETTINGS_SESSION_STATE_INACTIVE;
        if (session->priv->state != state) {
                session->priv->state = state;
                g_object_notify (G_OBJECT (session), "state");
        }
}

static void
cinnamon_settings_session_get_property (GObject *object,
				     guint prop_id,
				     GValue *value,
				     GParamSpec *pspec)
{
	CinnamonSettingsSession *session = CINNAMON_SETTINGS_SESSION (object);

	switch (prop_id) {
	case PROP_STATE:
		g_value_set_enum (value, session->priv->state);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

GType
cinnamon_settings_session_state_get_type (void)
{
	static GType etype = 0;
	if (etype == 0) {
		static const GEnumValue values[] = {
			{ CINNAMON_SETTINGS_SESSION_STATE_UNKNOWN,
			  "unknown", "Unknown" },
			{ CINNAMON_SETTINGS_SESSION_STATE_ACTIVE,
			  "active", "Active" },
			{ CINNAMON_SETTINGS_SESSION_STATE_INACTIVE,
			  "inactive", "Inactive" },
			{ 0, NULL, NULL }
			};
		etype = g_enum_register_static ("CinnamonSettingsSessionState", values);
	}
	return etype;
}

static void
cinnamon_settings_session_class_init (CinnamonSettingsSessionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = cinnamon_settings_session_get_property;
	object_class->finalize = cinnamon_settings_session_finalize;
	g_type_class_add_private (klass, sizeof (CinnamonSettingsSessionPrivate));

	g_object_class_install_property (object_class,
					 PROP_STATE,
					 g_param_spec_enum ("state",
							    "The session state",
							    NULL,
							    CINNAMON_TYPE_SETTINGS_SESSION_STATE,
							    CINNAMON_SETTINGS_SESSION_STATE_UNKNOWN,
							    G_PARAM_READABLE));
}

#ifdef HAVE_LOGIND

static gboolean
sessions_changed (gpointer user_data)
{
        CinnamonSettingsSession *session = user_data;
        gboolean active;

        active = sd_session_is_active (session->priv->session_id);
        cinnamon_settings_session_set_state (session, active);

        return TRUE;
}

#endif

static void
cinnamon_settings_session_proxy_signal_cb (GDBusProxy *proxy,
					const gchar *sender_name,
					const gchar *signal_name,
					GVariant *parameters,
					CinnamonSettingsSession *session)
{
	gboolean active;
	if (g_strcmp0 (signal_name, "ActiveChanged") == 0) {
		g_variant_get (parameters, "(b)", &active);
		g_debug ("emitting active: %i", active);
		cinnamon_settings_session_set_state (session, active);
	}
}

static void
is_active_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	gboolean active = FALSE;
	GError *error = NULL;
	GVariant *result;
	CinnamonSettingsSession *session = CINNAMON_SETTINGS_SESSION (user_data);

	/* is our session active */
	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_warning ("IsActive failed: %s", error->message);
		g_error_free (error);
		return;
	}
	g_variant_get (result, "(b)", &active);
	cinnamon_settings_session_set_state (session, active);

	/* watch for changes */
	g_signal_connect (session->priv->proxy_session, "g-signal",
			  G_CALLBACK (cinnamon_settings_session_proxy_signal_cb),
			  session);

	g_variant_unref (result);
}

static void
got_session_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GError *error = NULL;
	CinnamonSettingsSession *session = CINNAMON_SETTINGS_SESSION (user_data);

	/* connect to session */
	session->priv->proxy_session = g_dbus_proxy_new_for_bus_finish (res,
									&error);
	if (session->priv->proxy_session == NULL) {
		g_warning ("cannot connect to %s: %s",
			   session->priv->session_id,
			   error->message);
		g_error_free (error);
		return;
	}

	/* is our session active */
	g_dbus_proxy_call (session->priv->proxy_session,
			   "IsActive",
			   NULL,
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   session->priv->cancellable,
			   is_active_cb,
			   session);
}

static void
got_session_path_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GVariant *result;
	GError *error = NULL;
	CinnamonSettingsSession *session = CINNAMON_SETTINGS_SESSION (user_data);

	result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
					   res,
					   &error);
	if (result == NULL) {
		g_warning ("Failed to get session for pid: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	g_variant_get (result, "(o)", &session->priv->session_id);
	g_debug ("ConsoleKit session ID: %s", session->priv->session_id);

	/* connect to session */
	g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
				  NULL,
				  CONSOLEKIT_NAME,
				  session->priv->session_id,
				  CONSOLEKIT_SESSION_INTERFACE,
				  session->priv->cancellable,
				  got_session_proxy_cb,
				  session);
	g_variant_unref (result);
}

static void
got_manager_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	GDBusProxy *proxy_manager;
	GError *error = NULL;
	guint32 pid;
	CinnamonSettingsSession *session = CINNAMON_SETTINGS_SESSION (user_data);

	proxy_manager = g_dbus_proxy_new_for_bus_finish (res, &error);
	if (proxy_manager == NULL) {
		g_warning ("cannot connect to ConsoleKit: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	/* get the session we are running in */
	pid = getpid ();
	g_dbus_proxy_call (proxy_manager,
			   "GetSessionForUnixProcess",
			   g_variant_new ("(u)", pid),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1, session->priv->cancellable,
			   got_session_path_cb,
			   session);
	g_object_unref (proxy_manager);
}

static void
cinnamon_settings_session_init (CinnamonSettingsSession *session)
{
	session->priv = CINNAMON_SETTINGS_SESSION_GET_PRIVATE (session);

#ifdef HAVE_LOGIND
    if (access("/run/systemd/seats/", F_OK) == 0) {    // sd_booted ()
        sd_pid_get_session (getpid(), &session->priv->session_id);
        session->priv->sd_source = sd_source_new ();
        g_source_set_callback (session->priv->sd_source, sessions_changed, session, NULL);
        g_source_attach (session->priv->sd_source, NULL);
        sessions_changed (session);

        g_debug ("Using logind");
    }
    else
#endif /* HAVE_LOGIND */

    {
        g_debug ("Using consolekit");

		session->priv->cancellable = g_cancellable_new ();

		/* connect to ConsoleKit */
		g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
				  G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
				  NULL,
				  CONSOLEKIT_NAME,
				  CONSOLEKIT_MANAGER_PATH,
				  CONSOLEKIT_MANAGER_INTERFACE,
				  session->priv->cancellable,
				  got_manager_proxy_cb,
				  session);
	}

}

static void
cinnamon_settings_session_finalize (GObject *object)
{
	CinnamonSettingsSession *session;

	session = CINNAMON_SETTINGS_SESSION (object);

    g_free (session->priv->session_id);

    if (session->priv->sd_source != NULL) {
        g_source_destroy (session->priv->sd_source);
        g_source_unref (session->priv->sd_source);
    }

	g_cancellable_cancel (session->priv->cancellable);

	if (session->priv->proxy_session != NULL)
		g_object_unref (session->priv->proxy_session);
	g_object_unref (session->priv->cancellable);

	G_OBJECT_CLASS (cinnamon_settings_session_parent_class)->finalize (object);
}

CinnamonSettingsSession *
cinnamon_settings_session_new (void)
{
	CinnamonSettingsSession *session;
	session = g_object_new (CINNAMON_TYPE_SETTINGS_SESSION, NULL);
	return CINNAMON_SETTINGS_SESSION (session);
}

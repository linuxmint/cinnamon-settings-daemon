/*
 * Copyright © 2013 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses>
 *
 * Author: Michael Wood <michael.g.wood@intel.com>
 */

#include "mpris-controller.h"
#include "bus-watch-namespace.h"
#include <gio/gio.h>

G_DEFINE_TYPE (MprisController, mpris_controller, G_TYPE_OBJECT)

#define CONTROLLER_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MPRIS_TYPE_CONTROLLER, MprisControllerPrivate))

struct _MprisControllerPrivate
{
  GCancellable *cancellable;
  GDBusProxy *mpris_client_proxy;
  guint namespace_watcher_id;
  GSList *other_players;
  gboolean connecting;
};


static void
mpris_controller_dispose (GObject *object)
{
  MprisControllerPrivate *priv = MPRIS_CONTROLLER (object)->priv;

  g_clear_object (&priv->cancellable);
  g_clear_object (&priv->mpris_client_proxy);

  if (priv->namespace_watcher_id)
    {
      bus_unwatch_namespace (priv->namespace_watcher_id);
      priv->namespace_watcher_id = 0;
    }

  if (priv->other_players)
    {
      g_slist_free_full (priv->other_players, g_free);
      priv->other_players = NULL;
    }

  G_OBJECT_CLASS (mpris_controller_parent_class)->dispose (object);
}

static void
mpris_proxy_call_done (GObject      *object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GError *error = NULL;
  GVariant *ret;

  if (!(ret = g_dbus_proxy_call_finish (G_DBUS_PROXY (object), res, &error)))
    {
      g_warning ("Error calling method %s", error->message);
      g_clear_error (&error);
      return;
    }
  g_variant_unref (ret);
}

gboolean
mpris_controller_key (MprisController *self, const gchar *key)
{
  MprisControllerPrivate *priv = MPRIS_CONTROLLER (self)->priv;

  if (!priv->mpris_client_proxy)
    return FALSE;

  if (g_strcmp0 (key, "Play") == 0)
    key = "PlayPause";

  g_debug ("calling %s over dbus to mpris client %s",
           key, g_dbus_proxy_get_name (priv->mpris_client_proxy));
  g_dbus_proxy_call (priv->mpris_client_proxy,
                     key, NULL, 0, -1, priv->cancellable,
                     mpris_proxy_call_done,
                     NULL);
  return TRUE;
}

static void
mpris_proxy_ready_cb (GObject      *object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  MprisControllerPrivate *priv = MPRIS_CONTROLLER (user_data)->priv;
  GError *error = NULL;

  priv->mpris_client_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (!priv->mpris_client_proxy)
    g_warning ("Error connecting to mpris interface %s", error->message);

  priv->connecting = FALSE;

  g_clear_error (&error);
}

static void
start_mpris_proxy (MprisController *self, const gchar *name)
{
  MprisControllerPrivate *priv = MPRIS_CONTROLLER (self)->priv;

  g_debug ("Creating proxy for for %s", name);
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            0,
                            NULL,
                            name,
                            "/org/mpris/MediaPlayer2",
                            "org.mpris.MediaPlayer2.Player",
                            priv->cancellable,
                            mpris_proxy_ready_cb,
                            self);
  priv->connecting = TRUE;
}

static void
mpris_player_appeared (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
  MprisController *self = user_data;
  MprisControllerPrivate *priv = MPRIS_CONTROLLER (self)->priv;

  if (priv->mpris_client_proxy == NULL && !priv->connecting)
    start_mpris_proxy (self, name);
  else
    self->priv->other_players = g_slist_prepend (self->priv->other_players, g_strdup (name));
}

static void
mpris_player_vanished (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
  MprisController *self = user_data;
  MprisControllerPrivate *priv = MPRIS_CONTROLLER (self)->priv;

  if (priv->mpris_client_proxy &&
      g_strcmp0 (name, g_dbus_proxy_get_name (priv->mpris_client_proxy)) == 0)
    {
      g_clear_object (&priv->mpris_client_proxy);

      /* take the next one if there's one */
      if (self->priv->other_players && !priv->connecting)
        {
          GSList *first;
          gchar *name;

          first = self->priv->other_players;
          name = first->data;

          start_mpris_proxy (self, name);

          self->priv->other_players = self->priv->other_players->next;
          g_free (name);
          g_slist_free_1 (first);
        }
    }
}

static void
mpris_controller_constructed (GObject *object)
{
  MprisControllerPrivate *priv = MPRIS_CONTROLLER (object)->priv;

  priv->namespace_watcher_id = bus_watch_namespace (G_BUS_TYPE_SESSION,
                                                    "org.mpris.MediaPlayer2",
                                                    mpris_player_appeared,
                                                    mpris_player_vanished,
                                                    MPRIS_CONTROLLER (object),
                                                    NULL);
}

static void
mpris_controller_class_init (MprisControllerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MprisControllerPrivate));

  object_class->constructed = mpris_controller_constructed;
  object_class->dispose = mpris_controller_dispose;
}

static void
mpris_controller_init (MprisController *self)
{
  self->priv = CONTROLLER_PRIVATE (self);
}

MprisController *
mpris_controller_new (void)
{
  return g_object_new (MPRIS_TYPE_CONTROLLER, NULL);
}

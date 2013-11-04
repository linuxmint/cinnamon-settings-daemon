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

#ifndef __MPRIS_CONTROLLER_H__
#define __MPRIS_CONTROLLER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MPRIS_TYPE_CONTROLLER mpris_controller_get_type()
#define MPRIS_CONTROLLER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), MPRIS_TYPE_CONTROLLER, MprisController))
#define MPRIS_CONTROLLER_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass), MPRIS_TYPE_CONTROLLER, MprisControllerClass))
#define MPRIS_IS_CONTROLLER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MPRIS_TYPE_CONTROLLER))
#define MPRIS_IS_CONTROLLER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MPRIS_TYPE_CONTROLLER))
#define MPRIS_CONTROLLER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), MPRIS_TYPE_CONTROLLER, MprisControllerClass))

typedef struct _MprisController MprisController;
typedef struct _MprisControllerClass MprisControllerClass;
typedef struct _MprisControllerPrivate MprisControllerPrivate;

struct _MprisController
{
  GObject parent;

  MprisControllerPrivate *priv;
};

struct _MprisControllerClass
{
  GObjectClass parent_class;
};

GType mpris_controller_get_type (void) G_GNUC_CONST;

MprisController *mpris_controller_new (void);
gboolean         mpris_controller_key (MprisController *self, const gchar *key);

G_END_DECLS

#endif /* __MPRIS_CONTROLLER_H__ */

/*
 * Copyright (C) 2010 Canonical Ltd.
 * 
 * Authors: Jan Arne Petersen <jpetersen@openismus.com>
 * 
 * Based on gkbd-status.h by Sergey V. Udaltsov <svu@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef __GKBD_CONFIGURATION_H__
#define __GKBD_CONFIGURATION_H__

#include <glib-object.h>

#include <libxklavier/xklavier.h>

G_BEGIN_DECLS

typedef struct _GkbdConfiguration GkbdConfiguration;
typedef struct _GkbdConfigurationPrivate GkbdConfigurationPrivate;
typedef struct _GkbdConfigurationClass GkbdConfigurationClass;

#define GKBD_TYPE_CONFIGURATION           (gkbd_configuration_get_type ())
#define GKBD_CONFIGURATION(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GKBD_TYPE_CONFIGURATION, GkbdConfiguration))
#define GKBD_INDCATOR_CLASS(obj)          (G_TYPE_CHECK_CLASS_CAST ((obj), GKBD_TYPE_CONFIGURATION,  GkbdConfigurationClass))
#define GKBD_IS_CONFIGURATION(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GKBD_TYPE_CONFIGURATION))
#define GKBD_IS_CONFIGURATION_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE ((obj), GKBD_TYPE_CONFIGURATION))
#define GKBD_CONFIGURATION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GKBD_TYPE_CONFIGURATION, GkbdConfigurationClass))

struct _GkbdConfiguration {
	GObject parent;

	GkbdConfigurationPrivate *priv;
};

struct _GkbdConfigurationClass {
	GObjectClass parent_class;
};

extern GType gkbd_configuration_get_type (void);

extern GkbdConfiguration *gkbd_configuration_get (void);

extern XklEngine *gkbd_configuration_get_xkl_engine (GkbdConfiguration *configuration);

extern const char * const *gkbd_configuration_get_group_names (GkbdConfiguration *configuration);
extern const char * const *gkbd_configuration_get_short_group_names (GkbdConfiguration *configuration);

G_END_DECLS

#endif

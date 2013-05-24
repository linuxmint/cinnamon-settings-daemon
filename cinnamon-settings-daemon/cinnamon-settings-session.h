/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

#ifndef __CINNAMON_SETTINGS_SESSION_H
#define __CINNAMON_SETTINGS_SESSION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CINNAMON_TYPE_SETTINGS_SESSION		(cinnamon_settings_session_get_type ())
#define CINNAMON_SETTINGS_SESSION(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), CINNAMON_TYPE_SETTINGS_SESSION, CinnamonSettingsSession))
#define CINNAMON_SETTINGS_SESSION_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), CINNAMON_TYPE_SETTINGS_SESSION, CinnamonSettingsSessionClass))
#define CINNAMON_IS_SETTINGS_SESSION(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), CINNAMON_TYPE_SETTINGS_SESSION))
#define CINNAMON_IS_SETTINGS_SESSION_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), CINNAMON_TYPE_SETTINGS_SESSION))
#define CINNAMON_SETTINGS_SESSION_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), CINNAMON_TYPE_SETTINGS_SESSION, CinnamonSettingsSessionClass))
#define CINNAMON_TYPE_SETTINGS_SESSION_STATE	(cinnamon_settings_session_state_get_type())

typedef struct CinnamonSettingsSessionPrivate CinnamonSettingsSessionPrivate;

typedef struct
{
	 GObject			 parent;
	 CinnamonSettingsSessionPrivate	*priv;
} CinnamonSettingsSession;

typedef struct
{
	GObjectClass	parent_class;
} CinnamonSettingsSessionClass;

typedef enum {
	CINNAMON_SETTINGS_SESSION_STATE_UNKNOWN,
	CINNAMON_SETTINGS_SESSION_STATE_ACTIVE,
	CINNAMON_SETTINGS_SESSION_STATE_INACTIVE,
} CinnamonSettingsSessionState;

GType			 cinnamon_settings_session_get_type	(void);
GType			 cinnamon_settings_session_state_get_type	(void);
CinnamonSettingsSession	*cinnamon_settings_session_new		(void);
CinnamonSettingsSessionState cinnamon_settings_session_get_state	(CinnamonSettingsSession	*session);


G_END_DECLS

#endif /* __CINNAMON_SETTINGS_SESSION_H */


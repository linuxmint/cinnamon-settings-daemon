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

#ifndef __GNOME_SETTINGS_SESSION_H
#define __GNOME_SETTINGS_SESSION_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GNOME_TYPE_SETTINGS_SESSION		(gnome_settings_session_get_type ())
#define GNOME_SETTINGS_SESSION(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GNOME_TYPE_SETTINGS_SESSION, GnomeSettingsSession))
#define GNOME_SETTINGS_SESSION_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GNOME_TYPE_SETTINGS_SESSION, GnomeSettingsSessionClass))
#define GNOME_IS_SETTINGS_SESSION(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOME_TYPE_SETTINGS_SESSION))
#define GNOME_IS_SETTINGS_SESSION_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GNOME_TYPE_SETTINGS_SESSION))
#define GNOME_SETTINGS_SESSION_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GNOME_TYPE_SETTINGS_SESSION, GnomeSettingsSessionClass))
#define GNOME_TYPE_SETTINGS_SESSION_STATE	(gnome_settings_session_state_get_type())

typedef struct GnomeSettingsSessionPrivate GnomeSettingsSessionPrivate;

typedef struct
{
	 GObject			 parent;
	 GnomeSettingsSessionPrivate	*priv;
} GnomeSettingsSession;

typedef struct
{
	GObjectClass	parent_class;
} GnomeSettingsSessionClass;

typedef enum {
	GNOME_SETTINGS_SESSION_STATE_UNKNOWN,
	GNOME_SETTINGS_SESSION_STATE_ACTIVE,
	GNOME_SETTINGS_SESSION_STATE_INACTIVE,
} GnomeSettingsSessionState;

GType			 gnome_settings_session_get_type	(void);
GType			 gnome_settings_session_state_get_type	(void);
GnomeSettingsSession	*gnome_settings_session_new		(void);
GnomeSettingsSessionState gnome_settings_session_get_state	(GnomeSettingsSession	*session);


G_END_DECLS

#endif /* __GNOME_SETTINGS_SESSION_H */


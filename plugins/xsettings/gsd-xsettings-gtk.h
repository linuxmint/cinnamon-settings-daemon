/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __GSD_XSETTINGS_GTK_H__
#define __GSD_XSETTINGS_GTK_H__

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS

#define GSD_TYPE_XSETTINGS_GTK                (gsd_xsettings_gtk_get_type ())
#define GSD_XSETTINGS_GTK(o)                  (G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_XSETTINGS_GTK, GsdXSettingsGtk))
#define GSD_XSETTINGS_GTK_CLASS(k)            (G_TYPE_CHECK_CLASS_CAST((k), GSD_TYPE_XSETTINGS_GTK, GsdXSettingsGtkClass))
#define GSD_IS_XSETTINGS_GTK(o)               (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_XSETTINGS_GTK))
#define GSD_IS_XSETTINGS_GTK_CLASS(k)         (G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_XSETTINGS_GTK))
#define GSD_XSETTINGS_GTK_GET_CLASS(o)        (G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_XSETTINGS_GTK, GsdXSettingsGtkClass))

typedef struct GsdXSettingsGtkPrivate GsdXSettingsGtkPrivate;

typedef struct
{
        GObject                   parent;
        GsdXSettingsGtkPrivate *priv;
} GsdXSettingsGtk;

typedef struct
{
        GObjectClass parent_class;
} GsdXSettingsGtkClass;

GType   gsd_xsettings_gtk_get_type            (void) G_GNUC_CONST;

GsdXSettingsGtk *gsd_xsettings_gtk_new        (void);

const char * gsd_xsettings_gtk_get_modules (GsdXSettingsGtk *gtk);

G_END_DECLS

#endif /* __GSD_XSETTINGS_GTK_H__ */

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
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 */

#ifndef __CSD_XSETTINGS_GTK_H__
#define __CSD_XSETTINGS_GTK_H__

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS

#define CSD_TYPE_XSETTINGS_GTK                (csd_xsettings_gtk_get_type ())
#define CSD_XSETTINGS_GTK(o)                  (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_XSETTINGS_GTK, CsdXSettingsGtk))
#define CSD_XSETTINGS_GTK_CLASS(k)            (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_XSETTINGS_GTK, CsdXSettingsGtkClass))
#define CSD_IS_XSETTINGS_GTK(o)               (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_XSETTINGS_GTK))
#define CSD_IS_XSETTINGS_GTK_CLASS(k)         (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_XSETTINGS_GTK))
#define CSD_XSETTINGS_GTK_GET_CLASS(o)        (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_XSETTINGS_GTK, CsdXSettingsGtkClass))

typedef struct CsdXSettingsGtkPrivate CsdXSettingsGtkPrivate;

typedef struct
{
        GObject                   parent;
        CsdXSettingsGtkPrivate *priv;
} CsdXSettingsGtk;

typedef struct
{
        GObjectClass parent_class;
} CsdXSettingsGtkClass;

GType   csd_xsettings_gtk_get_type            (void) G_GNUC_CONST;

CsdXSettingsGtk *csd_xsettings_gtk_new        (void);

const char * csd_xsettings_gtk_get_modules (CsdXSettingsGtk *gtk);

G_END_DECLS

#endif /* __CSD_XSETTINGS_GTK_H__ */

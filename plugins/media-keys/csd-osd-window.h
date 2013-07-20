/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*-
 *
 * On-screen-display (OSD) window for cinnamon-settings-daemon's plugins
 *
 * Copyright (C) 2006 William Jon McCann <mccann@jhu.edu> 
 * Copyright (C) 2009 Novell, Inc
 *
 * Authors:
 *   William Jon McCann <mccann@jhu.edu>
 *   Federico Mena-Quintero <federico@novell.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 */

/* CsdOsdWindow is an "on-screen-display" window (OSD).  It is the cute,
 * semi-transparent, curved popup that appears when you press a hotkey global to
 * the desktop, such as to change the volume, switch your monitor's parameters,
 * etc.
 */

#ifndef CSD_OSD_WINDOW_H
#define CSD_OSD_WINDOW_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CSD_TYPE_OSD_WINDOW            (csd_osd_window_get_type ())
#define CSD_OSD_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj),  CSD_TYPE_OSD_WINDOW, CsdOsdWindow))
#define CSD_OSD_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),   CSD_TYPE_OSD_WINDOW, CsdOsdWindowClass))
#define CSD_IS_OSD_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj),  CSD_TYPE_OSD_WINDOW))
#define CSD_IS_OSD_WINDOW_CLASS(klass) (G_TYPE_INSTANCE_GET_CLASS ((klass), CSD_TYPE_OSD_WINDOW))
#define CSD_OSD_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CSD_TYPE_OSD_WINDOW, CsdOsdWindowClass))

typedef struct CsdOsdWindow                   CsdOsdWindow;
typedef struct CsdOsdWindowClass              CsdOsdWindowClass;
typedef struct CsdOsdWindowPrivate            CsdOsdWindowPrivate;

struct CsdOsdWindow {
        GtkWindow                   parent;

        CsdOsdWindowPrivate  *priv;
};

struct CsdOsdWindowClass {
        GtkWindowClass parent_class;
};

typedef enum {
        CSD_OSD_WINDOW_ACTION_VOLUME,
        CSD_OSD_WINDOW_ACTION_CUSTOM
} CsdOsdWindowAction;

GType                 csd_osd_window_get_type          (void);

GtkWidget *           csd_osd_window_new               (void);
gboolean              csd_osd_window_is_valid          (CsdOsdWindow       *window);
void                  csd_osd_window_set_action        (CsdOsdWindow       *window,
                                                        CsdOsdWindowAction  action);
void                  csd_osd_window_set_action_custom (CsdOsdWindow       *window,
                                                        const char         *icon_name,
                                                        gboolean            show_level);
void                  csd_osd_window_set_volume_muted  (CsdOsdWindow       *window,
                                                        gboolean            muted);
void                  csd_osd_window_set_volume_level  (CsdOsdWindow       *window,
                                                        int                 level);

G_END_DECLS

#endif

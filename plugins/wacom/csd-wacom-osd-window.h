/*
 * Copyright (C) 2012 Red Hat, Inc.
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
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 * Author: Olivier Fourdan <ofourdan@redhat.com>
 *
 */

#ifndef __CSD_WACOM_OSD_WINDOW_H
#define __CSD_WACOM_OSD_WINDOW_H

#include <gtk/gtk.h>
#include <glib-object.h>
#include "csd-wacom-device.h"

#define CSD_TYPE_WACOM_OSD_WINDOW         (csd_wacom_osd_window_get_type ())
#define CSD_WACOM_OSD_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_WACOM_OSD_WINDOW, CsdWacomOSDWindow))
#define CSD_WACOM_OSD_WINDOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_WACOM_OSD_WINDOW, CsdWacomOSDWindowClass))
#define CSD_IS_WACOM_OSD_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_WACOM_OSD_WINDOW))
#define CSD_IS_WACOM_OSD_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_WACOM_OSD_WINDOW))
#define CSD_WACOM_OSD_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_WACOM_OSD_WINDOW, CsdWacomOSDWindowClass))

typedef struct CsdWacomOSDWindowPrivate CsdWacomOSDWindowPrivate;

typedef struct
{
        GtkWindow                 window;
        CsdWacomOSDWindowPrivate *priv;
} CsdWacomOSDWindow;

typedef struct
{
        GtkWindowClass            parent_class;
} CsdWacomOSDWindowClass;

GType                     csd_wacom_osd_window_get_type        (void) G_GNUC_CONST;
CsdWacomDevice *          csd_wacom_osd_window_get_device      (CsdWacomOSDWindow        *osd_window);
void                      csd_wacom_osd_window_set_message     (CsdWacomOSDWindow        *osd_window,
                                                                const gchar              *str);
const char *              csd_wacom_osd_window_get_message     (CsdWacomOSDWindow        *osd_window);
void                      csd_wacom_osd_window_set_active      (CsdWacomOSDWindow        *osd_window,
                                                                CsdWacomTabletButton     *button,
                                                                GtkDirectionType          dir,
                                                                gboolean                  active);
void                      csd_wacom_osd_window_set_mode        (CsdWacomOSDWindow        *osd_window,
                                                                gint                      group_id,
                                                                gint                      mode);
GtkWidget *               csd_wacom_osd_window_new             (CsdWacomDevice           *pad,
                                                                const gchar              *message);

#endif /* __CSD_WACOM_OSD_WINDOW_H */

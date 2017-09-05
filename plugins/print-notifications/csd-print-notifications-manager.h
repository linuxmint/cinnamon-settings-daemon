/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __CSD_PRINT_NOTIFICATIONS_MANAGER_H
#define __CSD_PRINT_NOTIFICATIONS_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CSD_TYPE_PRINT_NOTIFICATIONS_MANAGER         (csd_print_notifications_manager_get_type ())
#define CSD_PRINT_NOTIFICATIONS_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_PRINT_NOTIFICATIONS_MANAGER, CsdPrintNotificationsManager))
#define CSD_PRINT_NOTIFICATIONS_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_PRINT_NOTIFICATIONS_MANAGER, CsdPrintNotificationsManagerClass))
#define CSD_IS_PRINT_NOTIFICATIONS_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_PRINT_NOTIFICATIONS_MANAGER))
#define CSD_IS_PRINT_NOTIFICATIONS_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_PRINT_NOTIFICATIONS_MANAGER))
#define CSD_PRINT_NOTIFICATIONS_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_PRINT_NOTIFICATIONS_MANAGER, CsdPrintNotificationsManagerClass))

typedef struct CsdPrintNotificationsManagerPrivate CsdPrintNotificationsManagerPrivate;

typedef struct
{
        GObject                              parent;
        CsdPrintNotificationsManagerPrivate *priv;
} CsdPrintNotificationsManager;

typedef struct
{
        GObjectClass   parent_class;
} CsdPrintNotificationsManagerClass;

GType                         csd_print_notifications_manager_get_type (void);

CsdPrintNotificationsManager *csd_print_notifications_manager_new      (void);
gboolean                      csd_print_notifications_manager_start    (CsdPrintNotificationsManager *manager,
                                                                        GError                      **error);
void                          csd_print_notifications_manager_stop     (CsdPrintNotificationsManager *manager);

G_END_DECLS

#endif /* __CSD_PRINT_NOTIFICATIONS_MANAGER_H */

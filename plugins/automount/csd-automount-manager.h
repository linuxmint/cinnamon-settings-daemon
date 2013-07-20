/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 *
 * Author: Tomas Bzatek <tbzatek@redhat.com>
 */

#ifndef __CSD_AUTOMOUNT_MANAGER_H
#define __CSD_AUTOMOUNT_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CSD_TYPE_AUTOMOUNT_MANAGER         (csd_automount_manager_get_type ())
#define CSD_AUTOMOUNT_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_AUTOMOUNT_MANAGER, CsdAutomountManager))
#define CSD_AUTOMOUNT_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_AUTOMOUNT_MANAGER, CsdAutomountManagerClass))
#define CSD_IS_AUTOMOUNT_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_AUTOMOUNT_MANAGER))
#define CSD_IS_AUTOMOUNT_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_AUTOMOUNT_MANAGER))
#define CSD_AUTOMOUNT_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_AUTOMOUNT_MANAGER, CsdAutomountManagerClass))

typedef struct CsdAutomountManagerPrivate CsdAutomountManagerPrivate;

typedef struct
{
        GObject                     parent;
        CsdAutomountManagerPrivate *priv;
} CsdAutomountManager;

typedef struct
{
        GObjectClass   parent_class;
} CsdAutomountManagerClass;

GType                   csd_automount_manager_get_type            (void);

CsdAutomountManager *   csd_automount_manager_new                 (void);
gboolean                csd_automount_manager_start               (CsdAutomountManager *manager,
                                                                   GError              **error);
void                    csd_automount_manager_stop                (CsdAutomountManager *manager);

G_END_DECLS

#endif /* __CSD_AUTOMOUNT_MANAGER_H */

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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 */

#ifndef __CSD_CLIPBOARD_MANAGER_H
#define __CSD_CLIPBOARD_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CSD_TYPE_CLIPBOARD_MANAGER         (csd_clipboard_manager_get_type ())
#define CSD_CLIPBOARD_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_CLIPBOARD_MANAGER, CsdClipboardManager))
#define CSD_CLIPBOARD_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_CLIPBOARD_MANAGER, CsdClipboardManagerClass))
#define CSD_IS_CLIPBOARD_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_CLIPBOARD_MANAGER))
#define CSD_IS_CLIPBOARD_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_CLIPBOARD_MANAGER))
#define CSD_CLIPBOARD_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_CLIPBOARD_MANAGER, CsdClipboardManagerClass))

typedef struct CsdClipboardManagerPrivate CsdClipboardManagerPrivate;

typedef struct
{
        GObject                     parent;
        CsdClipboardManagerPrivate *priv;
} CsdClipboardManager;

typedef struct
{
        GObjectClass   parent_class;
} CsdClipboardManagerClass;

GType                   csd_clipboard_manager_get_type            (void);

CsdClipboardManager *       csd_clipboard_manager_new                 (void);
gboolean                csd_clipboard_manager_start               (CsdClipboardManager *manager,
                                                               GError         **error);
void                    csd_clipboard_manager_stop                (CsdClipboardManager *manager);

G_END_DECLS

#endif /* __CSD_CLIPBOARD_MANAGER_H */

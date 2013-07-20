/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * csd-ldsm-dialog.c
 * Copyright (C) Chris Coulson 2009 <chrisccoulson@googlemail.com>
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
 */

#ifndef _CSD_LDSM_DIALOG_H_
#define _CSD_LDSM_DIALOG_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CSD_TYPE_LDSM_DIALOG             (csd_ldsm_dialog_get_type ())
#define CSD_LDSM_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CSD_TYPE_LDSM_DIALOG, CsdLdsmDialog))
#define CSD_LDSM_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CSD_TYPE_LDSM_DIALOG, CsdLdsmDialogClass))
#define CSD_IS_LDSM_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CSD_TYPE_LDSM_DIALOG))
#define CSD_IS_LDSM_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CSD_TYPE_LDSM_DIALOG))
#define CSD_LDSM_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CSD_TYPE_LDSM_DIALOG, CsdLdsmDialogClass))

enum
{
        CSD_LDSM_DIALOG_RESPONSE_EMPTY_TRASH = -20,
        CSD_LDSM_DIALOG_RESPONSE_ANALYZE = -21
};

typedef struct CsdLdsmDialogPrivate CsdLdsmDialogPrivate;
typedef struct _CsdLdsmDialogClass CsdLdsmDialogClass;
typedef struct _CsdLdsmDialog CsdLdsmDialog;

struct _CsdLdsmDialogClass
{
        GtkDialogClass parent_class;
};

struct _CsdLdsmDialog
{
        GtkDialog parent_instance;
        CsdLdsmDialogPrivate *priv;
};

GType csd_ldsm_dialog_get_type (void) G_GNUC_CONST;

CsdLdsmDialog * csd_ldsm_dialog_new (gboolean other_usable_partitions,
                                     gboolean other_partitions,
                                     gboolean display_baobab,
                                     gboolean display_empty_trash,
                                     gint64 space_remaining,
                                     const gchar *partition_name,
                                     const gchar *mount_path);

G_END_DECLS

#endif /* _CSD_LDSM_DIALOG_H_ */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
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

#ifndef __CSD_A11Y_PREFERENCES_DIALOG_H
#define __CSD_A11Y_PREFERENCES_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CSD_TYPE_A11Y_PREFERENCES_DIALOG         (csd_a11y_preferences_dialog_get_type ())
#define CSD_A11Y_PREFERENCES_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_A11Y_PREFERENCES_DIALOG, CsdA11yPreferencesDialog))
#define CSD_A11Y_PREFERENCES_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_A11Y_PREFERENCES_DIALOG, CsdA11yPreferencesDialogClass))
#define CSD_IS_A11Y_PREFERENCES_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_A11Y_PREFERENCES_DIALOG))
#define CSD_IS_A11Y_PREFERENCES_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_A11Y_PREFERENCES_DIALOG))
#define CSD_A11Y_PREFERENCES_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_A11Y_PREFERENCES_DIALOG, CsdA11yPreferencesDialogClass))

typedef struct CsdA11yPreferencesDialogPrivate CsdA11yPreferencesDialogPrivate;

typedef struct
{
        GtkDialog                        parent;
        CsdA11yPreferencesDialogPrivate *priv;
} CsdA11yPreferencesDialog;

typedef struct
{
        GtkDialogClass   parent_class;
} CsdA11yPreferencesDialogClass;

GType                  csd_a11y_preferences_dialog_get_type                   (void);

GtkWidget            * csd_a11y_preferences_dialog_new                        (void);

G_END_DECLS

#endif /* __CSD_A11Y_PREFERENCES_DIALOG_H */

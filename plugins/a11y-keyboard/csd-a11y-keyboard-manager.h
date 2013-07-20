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

#ifndef __CSD_A11Y_KEYBOARD_MANAGER_H
#define __CSD_A11Y_KEYBOARD_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CSD_TYPE_A11Y_KEYBOARD_MANAGER         (csd_a11y_keyboard_manager_get_type ())
#define CSD_A11Y_KEYBOARD_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_A11Y_KEYBOARD_MANAGER, CsdA11yKeyboardManager))
#define CSD_A11Y_KEYBOARD_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_A11Y_KEYBOARD_MANAGER, CsdA11yKeyboardManagerClass))
#define CSD_IS_A11Y_KEYBOARD_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_A11Y_KEYBOARD_MANAGER))
#define CSD_IS_A11Y_KEYBOARD_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_A11Y_KEYBOARD_MANAGER))
#define CSD_A11Y_KEYBOARD_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_A11Y_KEYBOARD_MANAGER, CsdA11yKeyboardManagerClass))

typedef struct CsdA11yKeyboardManagerPrivate CsdA11yKeyboardManagerPrivate;

typedef struct
{
        GObject                     parent;
        CsdA11yKeyboardManagerPrivate *priv;
} CsdA11yKeyboardManager;

typedef struct
{
        GObjectClass   parent_class;
} CsdA11yKeyboardManagerClass;

GType                   csd_a11y_keyboard_manager_get_type            (void);

CsdA11yKeyboardManager *csd_a11y_keyboard_manager_new                 (void);
gboolean                csd_a11y_keyboard_manager_start               (CsdA11yKeyboardManager *manager,
                                                                       GError                **error);
void                    csd_a11y_keyboard_manager_stop                (CsdA11yKeyboardManager *manager);

G_END_DECLS

#endif /* __CSD_A11Y_KEYBOARD_MANAGER_H */

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

#ifndef __CSD_KEYBOARD_MANAGER_H
#define __CSD_KEYBOARD_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CSD_TYPE_KEYBOARD_MANAGER         (csd_keyboard_manager_get_type ())

G_DECLARE_FINAL_TYPE (CsdKeyboardManager, csd_keyboard_manager, CSD, KEYBOARD_MANAGER, GObject)

CsdKeyboardManager *       csd_keyboard_manager_new                 (void);
gboolean                csd_keyboard_manager_start               (CsdKeyboardManager *manager,
                                                               GError         **error);
void                    csd_keyboard_manager_stop                (CsdKeyboardManager *manager);

G_END_DECLS

#endif /* __CSD_KEYBOARD_MANAGER_H */

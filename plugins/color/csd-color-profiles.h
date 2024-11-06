/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2011-2013 Richard Hughes <richard@hughsie.com>
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

#ifndef __CSD_COLOR_PROFILES_H
#define __CSD_COLOR_PROFILES_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CSD_TYPE_COLOR_PROFILES         (csd_color_profiles_get_type ())
G_DECLARE_FINAL_TYPE (CsdColorProfiles, csd_color_profiles, CSD, COLOR_PROFILES, GObject)

GQuark                  csd_color_profiles_error_quark          (void);

CsdColorProfiles *      csd_color_profiles_new                  (void);
gboolean                csd_color_profiles_start                (CsdColorProfiles *profiles,
                                                                 GError **error);
void                    csd_color_profiles_stop                 (CsdColorProfiles *profiles);

G_END_DECLS

#endif /* __CSD_COLOR_PROFILES_H */

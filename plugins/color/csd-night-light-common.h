/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
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

#ifndef __CSD_NIGHT_LIGHT_COMMON_H
#define __CSD_NIGHT_LIGHT_COMMON_H

#include <glib-object.h>

G_BEGIN_DECLS

gboolean csd_night_light_get_sunrise_sunset     (GDateTime      *dt,
                                                 gdouble         pos_lat,
                                                 gdouble         pos_long,
                                                 gdouble        *sunrise,
                                                 gdouble        *sunset);
gdouble  csd_night_light_frac_day_from_dt       (GDateTime      *dt);
gboolean csd_night_light_frac_day_is_between    (gdouble         value,
                                                 gdouble         start,
                                                 gdouble         end);

G_END_DECLS

#endif /* __CSD_NIGHT_LIGHT_COMMON_H */

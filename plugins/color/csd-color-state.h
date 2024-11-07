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

#ifndef __CSD_COLOR_STATE_H
#define __CSD_COLOR_STATE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CSD_TYPE_COLOR_STATE         (csd_color_state_get_type ())

G_DECLARE_FINAL_TYPE (CsdColorState, csd_color_state, CSD, COLOR_STATE, GObject)

#define CSD_COLOR_TEMPERATURE_MIN               1000    /* Kelvin */
#define CSD_COLOR_TEMPERATURE_DEFAULT           6500    /* Kelvin, is RGB [1.0,1.0,1.0] */
#define CSD_COLOR_TEMPERATURE_MAX               10000   /* Kelvin */

GQuark                  csd_color_state_error_quark     (void);

CsdColorState *         csd_color_state_new             (void);
void                    csd_color_state_start           (CsdColorState *state);
void                    csd_color_state_stop            (CsdColorState *state);
void                    csd_color_state_set_temperature (CsdColorState *state,
                                                         guint temperature);
guint                   csd_color_state_get_temperature (CsdColorState *state);

G_END_DECLS

#endif /* __CSD_COLOR_STATE_H */

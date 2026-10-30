/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __GPMCOMMON_H
#define __GPMCOMMON_H

#include <glib.h>
#include <libupower-glib/upower.h>

G_BEGIN_DECLS

/* 20 steps across the range, or one per level when there are fewer than 20 */
#define BRIGHTNESS_STEP_AMOUNT(max) ((max) < 20 ? 1 : (max) / 20)

/* both directions round to nearest so a percentage survives the round trip */
#define ABS_TO_PERCENTAGE(min, max, value) abs_to_percentage(min, max, value)
#define PERCENTAGE_TO_ABS(min, max, value) ((min) + ((((max) - (min)) * (value) + 50) / 100))

gint             abs_to_percentage                      (gint            min,
                                                         gint            max,
                                                         gint            value);
gchar           *gpm_get_timestring                     (guint           time);
GIcon           *gpm_upower_get_device_icon             (UpDevice       *device,
                                                         gboolean        use_symbolic);

G_END_DECLS

#endif  /* __GPMCOMMON_H */

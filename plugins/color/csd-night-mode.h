/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
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

#ifndef __CSD_NIGHT_MODE_H__
#define __CSD_NIGHT_MODE_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define CSD_TYPE_NIGHT_MODE (csd_night_mode_get_type ())
G_DECLARE_FINAL_TYPE (CsdNightMode, csd_night_mode, CSD, NIGHT_MODE, GObject)

CsdNightMode   *csd_night_mode_new                     (void);
gboolean        csd_night_mode_start                   (CsdNightMode  *self,
                                                        GError       **error);

gboolean         csd_night_mode_get_active             (CsdNightMode  *self);
gdouble          csd_night_mode_get_sunrise            (CsdNightMode  *self);
gdouble          csd_night_mode_get_sunset             (CsdNightMode  *self);
gdouble          csd_night_light_get_temperature       (CsdNightMode  *self);

gboolean         csd_night_mode_get_disabled_until_tmw (CsdNightMode  *self);
void             csd_night_mode_set_disabled_until_tmw (CsdNightMode  *self,
                                                        gboolean       value);

gboolean         csd_night_light_get_forced             (CsdNightMode *self);
void             csd_night_light_set_forced             (CsdNightMode *self,
                                                         gboolean      value);
gboolean         csd_night_theme_get_forced             (CsdNightMode *self);
void             csd_night_theme_set_forced             (CsdNightMode *self,
                                                         gboolean      value);

void             csd_night_mode_set_date_time_now       (CsdNightMode *self,
                                                         GDateTime    *datetime);
void             csd_night_light_set_smooth_enabled     (CsdNightMode *self,
                                                         gboolean      smooth_enabled);

G_END_DECLS

#endif

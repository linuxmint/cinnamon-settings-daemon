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

gchar           *gpm_get_timestring                     (guint           time);
const gchar     *gpm_device_to_localised_string         (UpDevice       *device);
const gchar     *gpm_device_kind_to_localised_string    (UpDeviceKind    kind,
                                                         guint           number);
const gchar     *gpm_device_kind_to_icon                (UpDeviceKind    kind);
const gchar     *gpm_device_technology_to_localised_string (UpDeviceTechnology technology_enum);
const gchar     *gpm_device_state_to_localised_string   (UpDeviceState   state);
GIcon           *gpm_upower_get_device_icon             (UpDevice       *device,
                                                         gboolean        use_symbolic);
gchar           *gpm_upower_get_device_summary          (UpDevice       *device);
gchar           *gpm_upower_get_device_description      (UpDevice       *device);

G_END_DECLS

#endif  /* __GPMCOMMON_H */

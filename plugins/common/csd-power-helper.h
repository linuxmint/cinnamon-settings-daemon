/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Bastien Nocera <hadess@hadess.net>
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

#ifndef __CSD_POWER_HELPER_H
#define __CSD_POWER_HELPER_H

#include <glib.h>

G_BEGIN_DECLS

#include <gio/gio.h>

void csd_power_suspend   (GDBusProxy *upower_proxy);
void csd_power_hibernate (GDBusProxy *upower_proxy);
void csd_power_poweroff  (void);

G_END_DECLS

#endif /* __CSD_POWER_HELPER_H */

/*
 * Copyright (C) 2022 Benjamin Berg <bberg@redhat.com>
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

#ifndef __CSD_SYSTEMD_NOTIFY_H
#define __CSD_SYSTEMD_NOTIFY_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CSD_TYPE_SYSTEMD_NOTIFY         (csd_systemd_notify_get_type ())

G_DECLARE_FINAL_TYPE (CsdSystemdNotify, csd_systemd_notify, CSD, SYSTEMD_NOTIFY, GObject)

G_END_DECLS

#endif /* __CSD_SYSTEMD_NOTIFY_H */

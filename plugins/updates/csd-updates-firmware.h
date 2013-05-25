/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2011 Richard Hughes <richard@hughsie.com>
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

#ifndef __CSD_UPDATES_FIRMWARE_H
#define __CSD_UPDATES_FIRMWARE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CSD_UPDATES_TYPE_FIRMWARE               (csd_updates_firmware_get_type ())
#define CSD_UPDATES_FIRMWARE(o)                 (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_UPDATES_TYPE_FIRMWARE, CsdUpdatesFirmware))
#define CSD_UPDATES_FIRMWARE_CLASS(k)           (G_TYPE_CHECK_CLASS_CAST((k), CSD_UPDATES_TYPE_FIRMWARE, CsdUpdatesFirmwareClass))
#define CSD_UPDATES_IS_FIRMWARE(o)              (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_UPDATES_TYPE_FIRMWARE))

typedef struct CsdUpdatesFirmwarePrivate CsdUpdatesFirmwarePrivate;

typedef struct
{
         GObject                         parent;
         CsdUpdatesFirmwarePrivate      *priv;
} CsdUpdatesFirmware;

typedef struct
{
        GObjectClass    parent_class;
} CsdUpdatesFirmwareClass;

GType                    csd_updates_firmware_get_type          (void);
CsdUpdatesFirmware      *csd_updates_firmware_new               (void);

G_END_DECLS

#endif /* __CSD_UPDATES_FIRMWARE_H */

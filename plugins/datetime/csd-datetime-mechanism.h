/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 David Zeuthen <david@fubar.dk>
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
 *
 */

#ifndef CSD_DATETIME_MECHANISM_H
#define CSD_DATETIME_MECHANISM_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define CSD_DATETIME_TYPE_MECHANISM         (csd_datetime_mechanism_get_type ())
G_DECLARE_FINAL_TYPE (CsdDatetimeMechanism, csd_datetime_mechanism, CSD_DATETIME, MECHANISM, GObject)

extern GMainLoop             *loop;
extern GDBusConnection       *connection;

typedef enum
{
        CSD_DATETIME_MECHANISM_ERROR_GENERAL,
        CSD_DATETIME_MECHANISM_ERROR_NOT_PRIVILEGED,
        CSD_DATETIME_MECHANISM_ERROR_INVALID_TIMEZONE_FILE,
        CSD_DATETIME_MECHANISM_NUM_ERRORS
} CsdDatetimeMechanismError;

#define CSD_DATETIME_MECHANISM_ERROR csd_datetime_mechanism_error_quark ()

GType csd_datetime_mechanism_error_get_type (void);
#define CSD_DATETIME_MECHANISM_TYPE_ERROR (csd_datetime_mechanism_error_get_type ())

GQuark                     csd_datetime_mechanism_error_quark         (void);
GType                      csd_datetime_mechanism_get_type            (void);
CsdDatetimeMechanism      *csd_datetime_mechanism_new                 (void);

G_END_DECLS

#endif /* CSD_DATETIME_MECHANISM_H */

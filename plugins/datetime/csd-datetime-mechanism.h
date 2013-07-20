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
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define CSD_DATETIME_TYPE_MECHANISM         (csd_datetime_mechanism_get_type ())
#define CSD_DATETIME_MECHANISM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_DATETIME_TYPE_MECHANISM, CsdDatetimeMechanism))
#define CSD_DATETIME_MECHANISM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_DATETIME_TYPE_MECHANISM, CsdDatetimeMechanismClass))
#define CSD_DATETIME_IS_MECHANISM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_DATETIME_TYPE_MECHANISM))
#define CSD_DATETIME_IS_MECHANISM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_DATETIME_TYPE_MECHANISM))
#define CSD_DATETIME_MECHANISM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_DATETIME_TYPE_MECHANISM, CsdDatetimeMechanismClass))

typedef struct CsdDatetimeMechanismPrivate CsdDatetimeMechanismPrivate;

typedef struct
{
        GObject        parent;
        CsdDatetimeMechanismPrivate *priv;
} CsdDatetimeMechanism;

typedef struct
{
        GObjectClass   parent_class;
} CsdDatetimeMechanismClass;

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

/* exported methods */
gboolean            csd_datetime_mechanism_get_timezone (CsdDatetimeMechanism   *mechanism,
                                                         DBusGMethodInvocation  *context);
gboolean            csd_datetime_mechanism_set_timezone (CsdDatetimeMechanism   *mechanism,
                                                         const char             *zone_file,
                                                         DBusGMethodInvocation  *context);

gboolean            csd_datetime_mechanism_can_set_timezone (CsdDatetimeMechanism  *mechanism,
                                                             DBusGMethodInvocation *context);

gboolean            csd_datetime_mechanism_set_time     (CsdDatetimeMechanism  *mechanism,
                                                         gint64                 seconds_since_epoch,
                                                         DBusGMethodInvocation *context);

gboolean             csd_datetime_mechanism_set_date     (CsdDatetimeMechanism  *mechanism,
                                                          guint                  day,
                                                          guint                  month,
                                                          guint                  year,
                                                          DBusGMethodInvocation *context);

gboolean            csd_datetime_mechanism_can_set_time (CsdDatetimeMechanism  *mechanism,
                                                         DBusGMethodInvocation *context);

gboolean            csd_datetime_mechanism_adjust_time  (CsdDatetimeMechanism  *mechanism,
                                                         gint64                 seconds_to_add,
                                                         DBusGMethodInvocation *context);

gboolean            csd_datetime_mechanism_get_hardware_clock_using_utc  (CsdDatetimeMechanism  *mechanism,
                                                                          DBusGMethodInvocation *context);

gboolean            csd_datetime_mechanism_set_hardware_clock_using_utc  (CsdDatetimeMechanism  *mechanism,
                                                                          gboolean               using_utc,
                                                                          DBusGMethodInvocation *context);

gboolean            csd_datetime_mechanism_get_using_ntp  (CsdDatetimeMechanism    *mechanism,
                                                           DBusGMethodInvocation   *context);

gboolean            csd_datetime_mechanism_set_using_ntp  (CsdDatetimeMechanism    *mechanism,
                                                           gboolean                 using_ntp,
                                                           DBusGMethodInvocation   *context);

gboolean            csd_datetime_mechanism_can_set_using_ntp (CsdDatetimeMechanism  *mechanism,
                                                              DBusGMethodInvocation *context);
G_END_DECLS

#endif /* CSD_DATETIME_MECHANISM_H */

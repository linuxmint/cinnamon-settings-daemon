/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
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

#ifndef __GPM_IDLETIME_H
#define __GPM_IDLETIME_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GPM_IDLETIME_TYPE               (gpm_idletime_get_type ())
#define GPM_IDLETIME(o)                 (G_TYPE_CHECK_INSTANCE_CAST ((o), GPM_IDLETIME_TYPE, GpmIdletime))
#define GPM_IDLETIME_CLASS(k)           (G_TYPE_CHECK_CLASS_CAST((k), GPM_IDLETIME_TYPE, GpmIdletimeClass))
#define GPM_IS_IDLETIME(o)              (G_TYPE_CHECK_INSTANCE_TYPE ((o), GPM_IDLETIME_TYPE))
#define GPM_IS_IDLETIME_CLASS(k)        (G_TYPE_CHECK_CLASS_TYPE ((k), GPM_IDLETIME_TYPE))
#define GPM_IDLETIME_GET_CLASS(o)       (G_TYPE_INSTANCE_GET_CLASS ((o), GPM_IDLETIME_TYPE, GpmIdletimeClass))

typedef struct GpmIdletimePrivate GpmIdletimePrivate;

typedef struct
{
        GObject                  parent;
        GpmIdletimePrivate      *priv;
} GpmIdletime;

typedef struct
{
        GObjectClass    parent_class;
        void            (* alarm_expired)               (GpmIdletime    *idletime,
                                                         guint           timer_id);
        void            (* reset)                       (GpmIdletime    *idletime);
} GpmIdletimeClass;

GType            gpm_idletime_get_type                  (void);
GpmIdletime     *gpm_idletime_new                       (void);

void             gpm_idletime_alarm_reset_all           (GpmIdletime    *idletime);
gboolean         gpm_idletime_alarm_set                 (GpmIdletime    *idletime,
                                                         guint           alarm_id,
                                                         guint           timeout);
gboolean         gpm_idletime_alarm_remove              (GpmIdletime    *idletime,
                                                         guint           alarm_id);
gint64           gpm_idletime_get_time                  (GpmIdletime    *idletime);

G_END_DECLS

#endif  /* __GPM_IDLETIME_H */

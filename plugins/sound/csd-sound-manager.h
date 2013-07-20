/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Lennart Poettering <lennart@poettering.net>
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

#ifndef __CSD_SOUND_MANAGER_H
#define __CSD_SOUND_MANAGER_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CSD_TYPE_SOUND_MANAGER         (csd_sound_manager_get_type ())
#define CSD_SOUND_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_SOUND_MANAGER, CsdSoundManager))
#define CSD_SOUND_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), CSD_TYPE_SOUND_MANAGER, CsdSoundManagerClass))
#define CSD_IS_SOUND_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_SOUND_MANAGER))
#define CSD_IS_SOUND_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_SOUND_MANAGER))
#define CSD_SOUND_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_SOUND_MANAGER, CsdSoundManagerClass))

typedef struct CsdSoundManagerPrivate CsdSoundManagerPrivate;

typedef struct
{
        GObject parent;
        CsdSoundManagerPrivate *priv;
} CsdSoundManager;

typedef struct
{
        GObjectClass parent_class;
} CsdSoundManagerClass;

GType csd_sound_manager_get_type (void) G_GNUC_CONST;

CsdSoundManager *csd_sound_manager_new (void);
gboolean csd_sound_manager_start (CsdSoundManager *manager, GError **error);
void csd_sound_manager_stop (CsdSoundManager *manager);

G_END_DECLS

#endif /* __CSD_SOUND_MANAGER_H */

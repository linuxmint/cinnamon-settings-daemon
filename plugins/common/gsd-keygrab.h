/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Jens Granseuer <jensgr@gmx.net>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GSD_COMMON_KEYGRAB_H
#define __GSD_COMMON_KEYGRAB_H

G_BEGIN_DECLS

#include <glib.h>
#include <X11/keysym.h>
#include <X11/extensions/XInput2.h>

typedef struct {
        guint keysym;
        guint state;
        guint *keycodes;
} Key;

typedef enum {
        GSD_KEYGRAB_NORMAL           = 0,
        GSD_KEYGRAB_ALLOW_UNMODIFIED = 1 << 0,
        GSD_KEYGRAB_SYNCHRONOUS      = 1 << 1
} GsdKeygrabFlags;

void	        grab_key_unsafe	(Key     *key,
				 GsdKeygrabFlags flags,
			         GSList  *screens);

void            ungrab_key_unsafe (Key     *key,
                                   GSList  *screens);

gboolean        match_xi2_key   (Key           *key,
                                 XIDeviceEvent *event);

gboolean        key_uses_keycode (const Key *key,
                                  guint keycode);

Key *           parse_key        (const char    *str);
void            free_key         (Key           *key);

void            grab_button      (int      deviceid,
                                  gboolean grab,
                                  GSList  *screens);

G_END_DECLS

#endif /* __GSD_COMMON_KEYGRAB_H */

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
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 */

#ifndef __CSD_COMMON_KEYGRAB_H
#define __CSD_COMMON_KEYGRAB_H

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
        CSD_KEYGRAB_NORMAL           = 0,
        CSD_KEYGRAB_ALLOW_UNMODIFIED = 1 << 0,
        CSD_KEYGRAB_SYNCHRONOUS      = 1 << 1
} CsdKeygrabFlags;

void	        grab_key_unsafe	(Key     *key,
				 CsdKeygrabFlags flags,
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

#endif /* __CSD_COMMON_KEYGRAB_H */

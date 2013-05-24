/*
 * gsd-automount.h:helpers for automounting hotplugged volumes 
 *
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: David Zeuthen <davidz@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */

/* TODO:
 *
 * - unmount all the media we've automounted on shutdown
 * - finish x-content / * types
 *  - finalize the semi-spec
 *  - add probing/sniffing code
 * - implement missing features
 *  - "Open Folder when mounted"
 *  - Autorun spec (e.g. $ROOT/.autostart)
 *
 */

#ifndef __GSD_AUTORUN_H__
#define __GSD_AUTORUN_H__

#include <gtk/gtk.h>
#include <gio/gio.h>

typedef void (*GsdAutorunOpenWindow) (GMount *mount,
				      gpointer user_data);

void gsd_autorun (GMount *mount,
		  GSettings *settings,
		  GsdAutorunOpenWindow open_window_func,
		  gpointer user_data);

void gsd_allow_autorun_for_volume (GVolume *volume);
void gsd_allow_autorun_for_volume_finish (GVolume *volume);

#endif /* __GSD_AUTORUN_H__ */

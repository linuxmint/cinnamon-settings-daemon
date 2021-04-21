/*
 * csd-automount.h:helpers for automounting hotplugged volumes 
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
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
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

#ifndef __CSD_AUTORUN_H__
#define __CSD_AUTORUN_H__

#include <gtk/gtk.h>
#include <gio/gio.h>

typedef void (*CsdAutorunOpenWindow) (GMount *mount,
				      gpointer user_data);

void csd_autorun (GMount *mount,
		  GSettings *settings,
		  CsdAutorunOpenWindow open_window_func,
		  gpointer user_data);

void csd_autorun_for_content_type (GMount               *mount,
                                   const gchar          *content_type,
                                   CsdAutorunOpenWindow  callback,
                                   gpointer              user_data);

void csd_allow_autorun_for_volume (GVolume *volume);
void csd_allow_autorun_for_volume_finish (GVolume *volume);

#endif /* __CSD_AUTORUN_H__ */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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

#ifndef __GSD_UPDATES_COMMON_H
#define __GSD_UPDATES_COMMON_H

G_BEGIN_DECLS

#define GSD_SETTINGS_BANNED_FIRMWARE                    "banned-firmware"
#define GSD_SETTINGS_CONNECTION_USE_MOBILE              "connection-use-mobile"
#define GSD_SETTINGS_ENABLE_CHECK_FIRMWARE              "enable-check-firmware"
#define GSD_SETTINGS_FREQUENCY_GET_UPDATES              "frequency-get-updates"
#define GSD_SETTINGS_FREQUENCY_GET_UPGRADES             "frequency-get-upgrades"
#define GSD_SETTINGS_FREQUENCY_REFRESH_CACHE            "frequency-refresh-cache"
#define GSD_SETTINGS_FREQUENCY_UPDATES_NOTIFICATION     "frequency-updates-notification"
#define GSD_SETTINGS_IGNORED_DEVICES                    "ignored-devices"
#define GSD_SETTINGS_LAST_UPDATES_NOTIFICATION          "last-updates-notification"
#define GSD_SETTINGS_MEDIA_REPO_FILENAMES               "media-repo-filenames"
#define GSD_SETTINGS_NOTIFY_DISTRO_UPGRADES             "notify-distro-upgrades"
#define GSD_SETTINGS_SCHEMA                             "org.gnome.settings-daemon.plugins.updates"
#define GSD_SETTINGS_UPDATE_BATTERY                     "update-battery"
#define GSD_SETTINGS_AUTO_DOWNLOAD_UPDATES              "auto-download-updates"

G_END_DECLS

#endif /* __GSD_UPDATES_COMMON_H */

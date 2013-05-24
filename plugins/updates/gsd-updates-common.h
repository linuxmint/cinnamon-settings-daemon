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

#ifndef __CSD_UPDATES_COMMON_H
#define __CSD_UPDATES_COMMON_H

G_BEGIN_DECLS

#define CSD_SETTINGS_BANNED_FIRMWARE                    "banned-firmware"
#define CSD_SETTINGS_CONNECTION_USE_MOBILE              "connection-use-mobile"
#define CSD_SETTINGS_ENABLE_CHECK_FIRMWARE              "enable-check-firmware"
#define CSD_SETTINGS_FREQUENCY_GET_UPDATES              "frequency-get-updates"
#define CSD_SETTINGS_FREQUENCY_GET_UPGRADES             "frequency-get-upgrades"
#define CSD_SETTINGS_FREQUENCY_REFRESH_CACHE            "frequency-refresh-cache"
#define CSD_SETTINGS_FREQUENCY_UPDATES_NOTIFICATION     "frequency-updates-notification"
#define CSD_SETTINGS_IGNORED_DEVICES                    "ignored-devices"
#define CSD_SETTINGS_LAST_UPDATES_NOTIFICATION          "last-updates-notification"
#define CSD_SETTINGS_MEDIA_REPO_FILENAMES               "media-repo-filenames"
#define CSD_SETTINGS_NOTIFY_DISTRO_UPGRADES             "notify-distro-upgrades"
#define CSD_SETTINGS_SCHEMA                             "org.gnome.settings-daemon.plugins.updates"
#define CSD_SETTINGS_UPDATE_BATTERY                     "update-battery"
#define CSD_SETTINGS_AUTO_DOWNLOAD_UPDATES              "auto-download-updates"

G_END_DECLS

#endif /* __CSD_UPDATES_COMMON_H */

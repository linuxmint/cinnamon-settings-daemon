/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Bastien Nocera <hadess@hadess.net>
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

#ifndef __CSD_SCREENSAVER_PROXY_MANAGER_H
#define __CSD_SCREENSAVER_PROXY_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CSD_TYPE_SCREENSAVER_PROXY_MANAGER         (csd_screensaver_proxy_manager_get_type ())
#define CSD_SCREENSAVER_PROXY_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CSD_TYPE_SCREENSAVER_PROXY_MANAGER, CsdScreensaverProxyManager))
#define CSD_SCREENSAVER_PROXY_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CSD_TYPE_SCREENSAVER_PROXY_MANAGER, CsdScreensaverProxyManagerClass))
#define CSD_IS_SCREENSAVER_PROXY_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CSD_TYPE_SCREENSAVER_PROXY_MANAGER))
#define CSD_IS_SCREENSAVER_PROXY_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CSD_TYPE_SCREENSAVER_PROXY_MANAGER))
#define CSD_SCREENSAVER_PROXY_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CSD_TYPE_SCREENSAVER_PROXY_MANAGER, CsdScreensaverProxyManagerClass))

typedef struct CsdScreensaverProxyManagerPrivate CsdScreensaverProxyManagerPrivate;

typedef struct
{
        GObject                     parent;
        CsdScreensaverProxyManagerPrivate *priv;
} CsdScreensaverProxyManager;

typedef struct
{
        GObjectClass   parent_class;
} CsdScreensaverProxyManagerClass;

GType                       csd_screensaver_proxy_manager_get_type            (void);

CsdScreensaverProxyManager *csd_screensaver_proxy_manager_new                 (void);
gboolean                    csd_screensaver_proxy_manager_start               (CsdScreensaverProxyManager  *manager,
                                                                               GError                     **error);
void                        csd_screensaver_proxy_manager_stop                (CsdScreensaverProxyManager  *manager);

G_END_DECLS

#endif /* __CSD_SCREENSAVER_PROXY_MANAGER_H */

/* csd-smartcard-manager.h - object for monitoring smartcard insertion and
 *                           removal events
 *
 * Copyright (C) 2006, 2009 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA
 * 02110-1335, USA.
 *
 * Written by: Ray Strode
 */
#ifndef CSD_SMARTCARD_MANAGER_H
#define CSD_SMARTCARD_MANAGER_H

#define CSD_SMARTCARD_ENABLE_INTERNAL_API
#include "csd-smartcard.h"

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS
#define CSD_TYPE_SMARTCARD_MANAGER            (csd_smartcard_manager_get_type ())
#define CSD_SMARTCARD_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CSD_TYPE_SMARTCARD_MANAGER, CsdSmartcardManager))
#define CSD_SMARTCARD_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CSD_TYPE_SMARTCARD_MANAGER, CsdSmartcardManagerClass))
#define CSD_IS_SMARTCARD_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SC_TYPE_SMARTCARD_MANAGER))
#define CSD_IS_SMARTCARD_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SC_TYPE_SMARTCARD_MANAGER))
#define CSD_SMARTCARD_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), CSD_TYPE_SMARTCARD_MANAGER, CsdSmartcardManagerClass))
#define CSD_SMARTCARD_MANAGER_ERROR           (csd_smartcard_manager_error_quark ())
typedef struct _CsdSmartcardManager CsdSmartcardManager;
typedef struct _CsdSmartcardManagerClass CsdSmartcardManagerClass;
typedef struct _CsdSmartcardManagerPrivate CsdSmartcardManagerPrivate;
typedef enum _CsdSmartcardManagerError CsdSmartcardManagerError;

struct _CsdSmartcardManager {
    GObject parent;

    /*< private > */
    CsdSmartcardManagerPrivate *priv;
};

struct _CsdSmartcardManagerClass {
        GObjectClass parent_class;

        /* Signals */
        void (*smartcard_inserted) (CsdSmartcardManager *manager,
                                    CsdSmartcard        *token);
        void (*smartcard_removed) (CsdSmartcardManager *manager,
                                   CsdSmartcard        *token);
        void (*error) (CsdSmartcardManager *manager,
                       GError              *error);
};

enum _CsdSmartcardManagerError {
    CSD_SMARTCARD_MANAGER_ERROR_GENERIC = 0,
    CSD_SMARTCARD_MANAGER_ERROR_WITH_NSS,
    CSD_SMARTCARD_MANAGER_ERROR_LOADING_DRIVER,
    CSD_SMARTCARD_MANAGER_ERROR_WATCHING_FOR_EVENTS,
    CSD_SMARTCARD_MANAGER_ERROR_REPORTING_EVENTS
};

GType csd_smartcard_manager_get_type (void) G_GNUC_CONST;
GQuark csd_smartcard_manager_error_quark (void) G_GNUC_CONST;

CsdSmartcardManager *csd_smartcard_manager_new_default (void);

CsdSmartcardManager *csd_smartcard_manager_new (const char *module);

gboolean csd_smartcard_manager_start (CsdSmartcardManager  *manager,
                                      GError              **error);

void csd_smartcard_manager_stop (CsdSmartcardManager *manager);

char *csd_smartcard_manager_get_module_path (CsdSmartcardManager *manager);
gboolean csd_smartcard_manager_login_card_is_inserted (CsdSmartcardManager *manager);

G_END_DECLS
#endif                                /* CSD_SMARTCARD_MANAGER_H */

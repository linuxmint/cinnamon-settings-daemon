/* securitycard.h - api for reading and writing data to a security card
 *
 * Copyright (C) 2006 Ray Strode
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
 */
#ifndef CSD_SMARTCARD_H
#define CSD_SMARTCARD_H

#include <glib.h>
#include <glib-object.h>

#include <secmod.h>

G_BEGIN_DECLS
#define CSD_TYPE_SMARTCARD            (csd_smartcard_get_type ())
#define CSD_SMARTCARD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CSD_TYPE_SMARTCARD, CsdSmartcard))
#define CSD_SMARTCARD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CSD_TYPE_SMARTCARD, CsdSmartcardClass))
#define CSD_IS_SMARTCARD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CSD_TYPE_SMARTCARD))
#define CSD_IS_SMARTCARD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CSD_TYPE_SMARTCARD))
#define CSD_SMARTCARD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), CSD_TYPE_SMARTCARD, CsdSmartcardClass))
#define CSD_SMARTCARD_ERROR           (csd_smartcard_error_quark ())
typedef struct _CsdSmartcardClass CsdSmartcardClass;
typedef struct _CsdSmartcard CsdSmartcard;
typedef struct _CsdSmartcardPrivate CsdSmartcardPrivate;
typedef enum _CsdSmartcardError CsdSmartcardError;
typedef enum _CsdSmartcardState CsdSmartcardState;

typedef struct _CsdSmartcardRequest CsdSmartcardRequest;

struct _CsdSmartcard {
    GObject parent;

    /*< private > */
    CsdSmartcardPrivate *priv;
};

struct _CsdSmartcardClass {
    GObjectClass parent_class;

    void (* inserted) (CsdSmartcard *card);
    void (* removed)  (CsdSmartcard *card);
};

enum _CsdSmartcardError {
    CSD_SMARTCARD_ERROR_GENERIC = 0,
};

enum _CsdSmartcardState {
    CSD_SMARTCARD_STATE_INSERTED = 0,
    CSD_SMARTCARD_STATE_REMOVED,
};

GType csd_smartcard_get_type (void) G_GNUC_CONST;
GQuark csd_smartcard_error_quark (void) G_GNUC_CONST;

CK_SLOT_ID csd_smartcard_get_slot_id (CsdSmartcard *card);
gint csd_smartcard_get_slot_series (CsdSmartcard *card);
CsdSmartcardState csd_smartcard_get_state (CsdSmartcard *card);

char *csd_smartcard_get_name (CsdSmartcard *card);
gboolean csd_smartcard_is_login_card (CsdSmartcard *card);

gboolean csd_smartcard_unlock (CsdSmartcard *card,
                               const char   *password);

/* don't under any circumstances call these functions */
#ifdef CSD_SMARTCARD_ENABLE_INTERNAL_API

CsdSmartcard *_csd_smartcard_new (SECMODModule *module,
                                  CK_SLOT_ID    slot_id,
                                  gint          slot_series);
CsdSmartcard *_csd_smartcard_new_from_name (SECMODModule *module,
                                            const char   *name);

void _csd_smartcard_set_state (CsdSmartcard      *card,
                               CsdSmartcardState  state);
#endif

G_END_DECLS
#endif                                /* CSD_SMARTCARD_H */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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

#ifndef __CCM_EDID_H
#define __CCM_EDID_H

#include <glib-object.h>
#include <colord.h>

G_BEGIN_DECLS

#define CCM_TYPE_EDID           (ccm_edid_get_type ())
G_DECLARE_FINAL_TYPE (CcmEdid, ccm_edid, CCM, EDID, GObject)

#define CCM_EDID_ERROR          (ccm_edid_error_quark ())
enum
{
        CCM_EDID_ERROR_FAILED_TO_PARSE
};

GQuark           ccm_edid_error_quark                   (void);
CcmEdid         *ccm_edid_new                           (void);
void             ccm_edid_reset                         (CcmEdid                *edid);
gboolean         ccm_edid_parse                         (CcmEdid                *edid,
                                                         const guint8           *data,
                                                         gsize                   length,
                                                         GError                 **error);
const gchar     *ccm_edid_get_monitor_name              (CcmEdid                *edid);
const gchar     *ccm_edid_get_vendor_name               (CcmEdid                *edid);
const gchar     *ccm_edid_get_serial_number             (CcmEdid                *edid);
const gchar     *ccm_edid_get_eisa_id                   (CcmEdid                *edid);
const gchar     *ccm_edid_get_checksum                  (CcmEdid                *edid);
const gchar     *ccm_edid_get_pnp_id                    (CcmEdid                *edid);
guint            ccm_edid_get_width                     (CcmEdid                *edid);
guint            ccm_edid_get_height                    (CcmEdid                *edid);
gfloat           ccm_edid_get_gamma                     (CcmEdid                *edid);
const CdColorYxy *ccm_edid_get_red                      (CcmEdid                *edid);
const CdColorYxy *ccm_edid_get_green                    (CcmEdid                *edid);
const CdColorYxy *ccm_edid_get_blue                     (CcmEdid                *edid);
const CdColorYxy *ccm_edid_get_white                    (CcmEdid                *edid);

G_END_DECLS

#endif /* __CCM_EDID_H */


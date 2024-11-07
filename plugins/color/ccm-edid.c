/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Soren Sandmann <sandmann@redhat.com>
 * Copyright (C) 2009-2011 Richard Hughes <richard@hughsie.com>
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

#include "config.h"

#include <glib-object.h>
#include <math.h>
#include <string.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <libcinnamon-desktop/gnome-pnp-ids.h>

#include "ccm-edid.h"

static void     ccm_edid_finalize       (GObject     *object);

struct _CcmEdid
{
        GObject                          parent;

        gchar                           *monitor_name;
        gchar                           *vendor_name;
        gchar                           *serial_number;
        gchar                           *eisa_id;
        gchar                           *checksum;
        gchar                           *pnp_id;
        guint                            width;
        guint                            height;
        gfloat                           gamma;
        CdColorYxy                      *red;
        CdColorYxy                      *green;
        CdColorYxy                      *blue;
        CdColorYxy                      *white;
        GnomePnpIds                     *pnp_ids;
};

G_DEFINE_TYPE (CcmEdid, ccm_edid, G_TYPE_OBJECT)

#define CCM_EDID_OFFSET_PNPID                           0x08
#define CCM_EDID_OFFSET_SERIAL                          0x0c
#define CCM_EDID_OFFSET_SIZE                            0x15
#define CCM_EDID_OFFSET_GAMMA                           0x17
#define CCM_EDID_OFFSET_DATA_BLOCKS                     0x36
#define CCM_EDID_OFFSET_LAST_BLOCK                      0x6c
#define CCM_EDID_OFFSET_EXTENSION_BLOCK_COUNT           0x7e

#define CCM_DESCRIPTOR_DISPLAY_PRODUCT_NAME             0xfc
#define CCM_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER    0xff
#define CCM_DESCRIPTOR_COLOR_MANAGEMENT_DATA            0xf9
#define CCM_DESCRIPTOR_ALPHANUMERIC_DATA_STRING         0xfe
#define CCM_DESCRIPTOR_COLOR_POINT                      0xfb

GQuark
ccm_edid_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("ccm_edid_error");
	return quark;
}

const gchar *
ccm_edid_get_monitor_name (CcmEdid *edid)
{
        g_return_val_if_fail (CCM_IS_EDID (edid), NULL);
        return edid->monitor_name;
}

const gchar *
ccm_edid_get_vendor_name (CcmEdid *edid)
{
        g_return_val_if_fail (CCM_IS_EDID (edid), NULL);

        if (edid->vendor_name == NULL)
                edid->vendor_name = gnome_pnp_ids_get_pnp_id (edid->pnp_ids, edid->pnp_id);
        return edid->vendor_name;
}

const gchar *
ccm_edid_get_serial_number (CcmEdid *edid)
{
        g_return_val_if_fail (CCM_IS_EDID (edid), NULL);
        return edid->serial_number;
}

const gchar *
ccm_edid_get_eisa_id (CcmEdid *edid)
{
        g_return_val_if_fail (CCM_IS_EDID (edid), NULL);
        return edid->eisa_id;
}

const gchar *
ccm_edid_get_checksum (CcmEdid *edid)
{
        g_return_val_if_fail (CCM_IS_EDID (edid), NULL);
        return edid->checksum;
}

const gchar *
ccm_edid_get_pnp_id (CcmEdid *edid)
{
        g_return_val_if_fail (CCM_IS_EDID (edid), NULL);
        return edid->pnp_id;
}

guint
ccm_edid_get_width (CcmEdid *edid)
{
        g_return_val_if_fail (CCM_IS_EDID (edid), 0);
        return edid->width;
}

guint
ccm_edid_get_height (CcmEdid *edid)
{
        g_return_val_if_fail (CCM_IS_EDID (edid), 0);
        return edid->height;
}

gfloat
ccm_edid_get_gamma (CcmEdid *edid)
{
        g_return_val_if_fail (CCM_IS_EDID (edid), 0.0f);
        return edid->gamma;
}

const CdColorYxy *
ccm_edid_get_red (CcmEdid *edid)
{
        g_return_val_if_fail (CCM_IS_EDID (edid), NULL);
        return edid->red;
}

const CdColorYxy *
ccm_edid_get_green (CcmEdid *edid)
{
        g_return_val_if_fail (CCM_IS_EDID (edid), NULL);
        return edid->green;
}

const CdColorYxy *
ccm_edid_get_blue (CcmEdid *edid)
{
        g_return_val_if_fail (CCM_IS_EDID (edid), NULL);
        return edid->blue;
}

const CdColorYxy *
ccm_edid_get_white (CcmEdid *edid)
{
        g_return_val_if_fail (CCM_IS_EDID (edid), NULL);
        return edid->white;
}

void
ccm_edid_reset (CcmEdid *edid)
{
        g_return_if_fail (CCM_IS_EDID (edid));

        /* free old data */
        g_free (edid->monitor_name);
        g_free (edid->vendor_name);
        g_free (edid->serial_number);
        g_free (edid->eisa_id);
        g_free (edid->checksum);

        /* do not deallocate, just blank */
        edid->pnp_id[0] = '\0';

        /* set to default values */
        edid->monitor_name = NULL;
        edid->vendor_name = NULL;
        edid->serial_number = NULL;
        edid->eisa_id = NULL;
        edid->checksum = NULL;
        edid->width = 0;
        edid->height = 0;
        edid->gamma = 0.0f;
}

static gint
ccm_edid_get_bit (gint in, gint bit)
{
        return (in & (1 << bit)) >> bit;
}

/**
 * ccm_edid_get_bits:
 **/
static gint
ccm_edid_get_bits (gint in, gint begin, gint end)
{
        gint mask = (1 << (end - begin + 1)) - 1;

        return (in >> begin) & mask;
}

/**
 * ccm_edid_decode_fraction:
 **/
static gdouble
ccm_edid_decode_fraction (gint high, gint low)
{
        gdouble result = 0.0;
        gint i;

        high = (high << 2) | low;
        for (i = 0; i < 10; ++i)
                result += ccm_edid_get_bit (high, i) * pow (2, i - 10);
        return result;
}

static gchar *
ccm_edid_parse_string (const guint8 *data)
{
        gchar *text;
        guint i;
        guint replaced = 0;

        /* this is always 13 bytes, but we can't guarantee it's null
         * terminated or not junk. */
        text = g_strndup ((const gchar *) data, 13);

        /* remove insane newline chars */
        g_strdelimit (text, "\n\r", '\0');

        /* remove spaces */
        g_strchomp (text);

        /* nothing left? */
        if (text[0] == '\0') {
                g_free (text);
                text = NULL;
                goto out;
        }

        /* ensure string is printable */
        for (i = 0; text[i] != '\0'; i++) {
                if (!g_ascii_isprint (text[i])) {
                        text[i] = '-';
                        replaced++;
                }
        }

        /* if the string is junk, ignore the string */
        if (replaced > 4) {
                g_free (text);
                text = NULL;
                goto out;
        }
out:
        return text;
}

gboolean
ccm_edid_parse (CcmEdid *edid, const guint8 *data, gsize length, GError **error)
{
        gboolean ret = TRUE;
        guint i;
        guint32 serial;
        gchar *tmp;

        /* check header */
        if (length < 128) {
                g_set_error_literal (error,
                                     CCM_EDID_ERROR,
                                     CCM_EDID_ERROR_FAILED_TO_PARSE,
                                     "EDID length is too small");
                ret = FALSE;
                goto out;
        }
        if (data[0] != 0x00 || data[1] != 0xff) {
                g_set_error_literal (error,
                                     CCM_EDID_ERROR,
                                     CCM_EDID_ERROR_FAILED_TO_PARSE,
                                     "Failed to parse EDID header");
                ret = FALSE;
                goto out;
        }

        /* free old data */
        ccm_edid_reset (edid);

        /* decode the PNP ID from three 5 bit words packed into 2 bytes
         * /--08--\/--09--\
         * 7654321076543210
         * |\---/\---/\---/
         * R  C1   C2   C3 */
        edid->pnp_id[0] = 'A' + ((data[CCM_EDID_OFFSET_PNPID+0] & 0x7c) / 4) - 1;
        edid->pnp_id[1] = 'A' + ((data[CCM_EDID_OFFSET_PNPID+0] & 0x3) * 8) + ((data[CCM_EDID_OFFSET_PNPID+1] & 0xe0) / 32) - 1;
        edid->pnp_id[2] = 'A' + (data[CCM_EDID_OFFSET_PNPID+1] & 0x1f) - 1;

        /* maybe there isn't a ASCII serial number descriptor, so use this instead */
        serial = (guint32) data[CCM_EDID_OFFSET_SERIAL+0];
        serial += (guint32) data[CCM_EDID_OFFSET_SERIAL+1] * 0x100;
        serial += (guint32) data[CCM_EDID_OFFSET_SERIAL+2] * 0x10000;
        serial += (guint32) data[CCM_EDID_OFFSET_SERIAL+3] * 0x1000000;
        if (serial > 0)
                edid->serial_number = g_strdup_printf ("%" G_GUINT32_FORMAT, serial);

        /* get the size */
        edid->width = data[CCM_EDID_OFFSET_SIZE+0];
        edid->height = data[CCM_EDID_OFFSET_SIZE+1];

        /* we don't care about aspect */
        if (edid->width == 0 || edid->height == 0) {
                edid->width = 0;
                edid->height = 0;
        }

        /* get gamma */
        if (data[CCM_EDID_OFFSET_GAMMA] == 0xff) {
                edid->gamma = 1.0f;
        } else {
                edid->gamma = ((gfloat) data[CCM_EDID_OFFSET_GAMMA] / 100) + 1;
        }

        /* get color red */
        edid->red->x = ccm_edid_decode_fraction (data[0x1b], ccm_edid_get_bits (data[0x19], 6, 7));
        edid->red->y = ccm_edid_decode_fraction (data[0x1c], ccm_edid_get_bits (data[0x19], 4, 5));

        /* get color green */
        edid->green->x = ccm_edid_decode_fraction (data[0x1d], ccm_edid_get_bits (data[0x19], 2, 3));
        edid->green->y = ccm_edid_decode_fraction (data[0x1e], ccm_edid_get_bits (data[0x19], 0, 1));

        /* get color blue */
        edid->blue->x = ccm_edid_decode_fraction (data[0x1f], ccm_edid_get_bits (data[0x1a], 6, 7));
        edid->blue->y = ccm_edid_decode_fraction (data[0x20], ccm_edid_get_bits (data[0x1a], 4, 5));

        /* get color white */
        edid->white->x = ccm_edid_decode_fraction (data[0x21], ccm_edid_get_bits (data[0x1a], 2, 3));
        edid->white->y = ccm_edid_decode_fraction (data[0x22], ccm_edid_get_bits (data[0x1a], 0, 1));

        /* parse EDID data */
        for (i = CCM_EDID_OFFSET_DATA_BLOCKS;
             i <= CCM_EDID_OFFSET_LAST_BLOCK;
             i += 18) {
                /* ignore pixel clock data */
                if (data[i] != 0)
                        continue;
                if (data[i+2] != 0)
                        continue;

                /* any useful blocks? */
                if (data[i+3] == CCM_DESCRIPTOR_DISPLAY_PRODUCT_NAME) {
                        tmp = ccm_edid_parse_string (&data[i+5]);
                        if (tmp != NULL) {
                                g_free (edid->monitor_name);
                                edid->monitor_name = tmp;
                        }
                } else if (data[i+3] == CCM_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER) {
                        tmp = ccm_edid_parse_string (&data[i+5]);
                        if (tmp != NULL) {
                                g_free (edid->serial_number);
                                edid->serial_number = tmp;
                        }
                } else if (data[i+3] == CCM_DESCRIPTOR_COLOR_MANAGEMENT_DATA) {
                        g_warning ("failing to parse color management data");
                } else if (data[i+3] == CCM_DESCRIPTOR_ALPHANUMERIC_DATA_STRING) {
                        tmp = ccm_edid_parse_string (&data[i+5]);
                        if (tmp != NULL) {
                                g_free (edid->eisa_id);
                                edid->eisa_id = tmp;
                        }
                } else if (data[i+3] == CCM_DESCRIPTOR_COLOR_POINT) {
                        if (data[i+3+9] != 0xff) {
                                /* extended EDID block(1) which contains
                                 * a better gamma value */
                                edid->gamma = ((gfloat) data[i+3+9] / 100) + 1;
                        }
                        if (data[i+3+14] != 0xff) {
                                /* extended EDID block(2) which contains
                                 * a better gamma value */
                                edid->gamma = ((gfloat) data[i+3+9] / 100) + 1;
                        }
                }
        }

        /* calculate checksum */
        edid->checksum = g_compute_checksum_for_data (G_CHECKSUM_MD5, data, length);
out:
        return ret;
}

static void
ccm_edid_class_init (CcmEdidClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = ccm_edid_finalize;
}

static void
ccm_edid_init (CcmEdid *edid)
{
        edid->pnp_ids = gnome_pnp_ids_new ();
        edid->pnp_id = g_new0 (gchar, 4);
        edid->red = cd_color_yxy_new ();
        edid->green = cd_color_yxy_new ();
        edid->blue = cd_color_yxy_new ();
        edid->white = cd_color_yxy_new ();
}

static void
ccm_edid_finalize (GObject *object)
{
        CcmEdid *edid = CCM_EDID (object);

        g_free (edid->monitor_name);
        g_free (edid->vendor_name);
        g_free (edid->serial_number);
        g_free (edid->eisa_id);
        g_free (edid->checksum);
        g_free (edid->pnp_id);
        cd_color_yxy_free (edid->white);
        cd_color_yxy_free (edid->red);
        cd_color_yxy_free (edid->green);
        cd_color_yxy_free (edid->blue);
        g_object_unref (edid->pnp_ids);

        G_OBJECT_CLASS (ccm_edid_parent_class)->finalize (object);
}

CcmEdid *
ccm_edid_new (void)
{
        CcmEdid *edid;
        edid = g_object_new (CCM_TYPE_EDID, NULL);
        return CCM_EDID (edid);
}


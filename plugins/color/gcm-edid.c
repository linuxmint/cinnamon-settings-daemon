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
#include <libgnome-desktop/gnome-pnp-ids.h>

#include "gcm-edid.h"

static void     gcm_edid_finalize       (GObject     *object);

#define GCM_EDID_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_EDID, GcmEdidPrivate))

struct _GcmEdidPrivate
{
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

G_DEFINE_TYPE (GcmEdid, gcm_edid, G_TYPE_OBJECT)

#define GCM_EDID_OFFSET_PNPID                           0x08
#define GCM_EDID_OFFSET_SERIAL                          0x0c
#define GCM_EDID_OFFSET_SIZE                            0x15
#define GCM_EDID_OFFSET_GAMMA                           0x17
#define GCM_EDID_OFFSET_DATA_BLOCKS                     0x36
#define GCM_EDID_OFFSET_LAST_BLOCK                      0x6c
#define GCM_EDID_OFFSET_EXTENSION_BLOCK_COUNT           0x7e

#define GCM_DESCRIPTOR_DISPLAY_PRODUCT_NAME             0xfc
#define GCM_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER    0xff
#define GCM_DESCRIPTOR_COLOR_MANAGEMENT_DATA            0xf9
#define GCM_DESCRIPTOR_ALPHANUMERIC_DATA_STRING         0xfe
#define GCM_DESCRIPTOR_COLOR_POINT                      0xfb

GQuark
gcm_edid_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("gcm_edid_error");
	return quark;
}

const gchar *
gcm_edid_get_monitor_name (GcmEdid *edid)
{
        g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
        return edid->priv->monitor_name;
}

const gchar *
gcm_edid_get_vendor_name (GcmEdid *edid)
{
        GcmEdidPrivate *priv = edid->priv;
        g_return_val_if_fail (GCM_IS_EDID (edid), NULL);

        if (priv->vendor_name == NULL)
                priv->vendor_name = gnome_pnp_ids_get_pnp_id (priv->pnp_ids, priv->pnp_id);
        return priv->vendor_name;
}

const gchar *
gcm_edid_get_serial_number (GcmEdid *edid)
{
        g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
        return edid->priv->serial_number;
}

const gchar *
gcm_edid_get_eisa_id (GcmEdid *edid)
{
        g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
        return edid->priv->eisa_id;
}

const gchar *
gcm_edid_get_checksum (GcmEdid *edid)
{
        g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
        return edid->priv->checksum;
}

const gchar *
gcm_edid_get_pnp_id (GcmEdid *edid)
{
        g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
        return edid->priv->pnp_id;
}

guint
gcm_edid_get_width (GcmEdid *edid)
{
        g_return_val_if_fail (GCM_IS_EDID (edid), 0);
        return edid->priv->width;
}

guint
gcm_edid_get_height (GcmEdid *edid)
{
        g_return_val_if_fail (GCM_IS_EDID (edid), 0);
        return edid->priv->height;
}

gfloat
gcm_edid_get_gamma (GcmEdid *edid)
{
        g_return_val_if_fail (GCM_IS_EDID (edid), 0.0f);
        return edid->priv->gamma;
}

const CdColorYxy *
gcm_edid_get_red (GcmEdid *edid)
{
        g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
        return edid->priv->red;
}

const CdColorYxy *
gcm_edid_get_green (GcmEdid *edid)
{
        g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
        return edid->priv->green;
}

const CdColorYxy *
gcm_edid_get_blue (GcmEdid *edid)
{
        g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
        return edid->priv->blue;
}

const CdColorYxy *
gcm_edid_get_white (GcmEdid *edid)
{
        g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
        return edid->priv->white;
}

void
gcm_edid_reset (GcmEdid *edid)
{
        GcmEdidPrivate *priv = edid->priv;

        g_return_if_fail (GCM_IS_EDID (edid));

        /* free old data */
        g_free (priv->monitor_name);
        g_free (priv->vendor_name);
        g_free (priv->serial_number);
        g_free (priv->eisa_id);
        g_free (priv->checksum);

        /* do not deallocate, just blank */
        priv->pnp_id[0] = '\0';

        /* set to default values */
        priv->monitor_name = NULL;
        priv->vendor_name = NULL;
        priv->serial_number = NULL;
        priv->eisa_id = NULL;
        priv->checksum = NULL;
        priv->width = 0;
        priv->height = 0;
        priv->gamma = 0.0f;
}

static gint
gcm_edid_get_bit (gint in, gint bit)
{
        return (in & (1 << bit)) >> bit;
}

/**
 * gcm_edid_get_bits:
 **/
static gint
gcm_edid_get_bits (gint in, gint begin, gint end)
{
        gint mask = (1 << (end - begin + 1)) - 1;

        return (in >> begin) & mask;
}

/**
 * gcm_edid_decode_fraction:
 **/
static gdouble
gcm_edid_decode_fraction (gint high, gint low)
{
        gdouble result = 0.0;
        gint i;

        high = (high << 2) | low;
        for (i = 0; i < 10; ++i)
                result += gcm_edid_get_bit (high, i) * pow (2, i - 10);
        return result;
}

static gchar *
gcm_edid_parse_string (const guint8 *data)
{
        gchar *text;
        guint i;
        guint replaced = 0;

        /* this is always 12 bytes, but we can't guarantee it's null
         * terminated or not junk. */
        text = g_strndup ((const gchar *) data, 12);

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
gcm_edid_parse (GcmEdid *edid, const guint8 *data, gsize length, GError **error)
{
        gboolean ret = TRUE;
        guint i;
        GcmEdidPrivate *priv = edid->priv;
        guint32 serial;
        gchar *tmp;

        /* check header */
        if (length < 128) {
                g_set_error_literal (error,
                                     GCM_EDID_ERROR,
                                     GCM_EDID_ERROR_FAILED_TO_PARSE,
                                     "EDID length is too small");
                ret = FALSE;
                goto out;
        }
        if (data[0] != 0x00 || data[1] != 0xff) {
                g_set_error_literal (error,
                                     GCM_EDID_ERROR,
                                     GCM_EDID_ERROR_FAILED_TO_PARSE,
                                     "Failed to parse EDID header");
                ret = FALSE;
                goto out;
        }

        /* free old data */
        gcm_edid_reset (edid);

        /* decode the PNP ID from three 5 bit words packed into 2 bytes
         * /--08--\/--09--\
         * 7654321076543210
         * |\---/\---/\---/
         * R  C1   C2   C3 */
        priv->pnp_id[0] = 'A' + ((data[GCM_EDID_OFFSET_PNPID+0] & 0x7c) / 4) - 1;
        priv->pnp_id[1] = 'A' + ((data[GCM_EDID_OFFSET_PNPID+0] & 0x3) * 8) + ((data[GCM_EDID_OFFSET_PNPID+1] & 0xe0) / 32) - 1;
        priv->pnp_id[2] = 'A' + (data[GCM_EDID_OFFSET_PNPID+1] & 0x1f) - 1;

        /* maybe there isn't a ASCII serial number descriptor, so use this instead */
        serial = (guint32) data[GCM_EDID_OFFSET_SERIAL+0];
        serial += (guint32) data[GCM_EDID_OFFSET_SERIAL+1] * 0x100;
        serial += (guint32) data[GCM_EDID_OFFSET_SERIAL+2] * 0x10000;
        serial += (guint32) data[GCM_EDID_OFFSET_SERIAL+3] * 0x1000000;
        if (serial > 0)
                priv->serial_number = g_strdup_printf ("%" G_GUINT32_FORMAT, serial);

        /* get the size */
        priv->width = data[GCM_EDID_OFFSET_SIZE+0];
        priv->height = data[GCM_EDID_OFFSET_SIZE+1];

        /* we don't care about aspect */
        if (priv->width == 0 || priv->height == 0) {
                priv->width = 0;
                priv->height = 0;
        }

        /* get gamma */
        if (data[GCM_EDID_OFFSET_GAMMA] == 0xff) {
                priv->gamma = 1.0f;
        } else {
                priv->gamma = ((gfloat) data[GCM_EDID_OFFSET_GAMMA] / 100) + 1;
        }

        /* get color red */
        priv->red->x = gcm_edid_decode_fraction (data[0x1b], gcm_edid_get_bits (data[0x19], 6, 7));
        priv->red->y = gcm_edid_decode_fraction (data[0x1c], gcm_edid_get_bits (data[0x19], 5, 4));

        /* get color green */
        priv->green->x = gcm_edid_decode_fraction (data[0x1d], gcm_edid_get_bits (data[0x19], 2, 3));
        priv->green->y = gcm_edid_decode_fraction (data[0x1e], gcm_edid_get_bits (data[0x19], 0, 1));

        /* get color blue */
        priv->blue->x = gcm_edid_decode_fraction (data[0x1f], gcm_edid_get_bits (data[0x1a], 6, 7));
        priv->blue->y = gcm_edid_decode_fraction (data[0x20], gcm_edid_get_bits (data[0x1a], 4, 5));

        /* get color white */
        priv->white->x = gcm_edid_decode_fraction (data[0x21], gcm_edid_get_bits (data[0x1a], 2, 3));
        priv->white->y = gcm_edid_decode_fraction (data[0x22], gcm_edid_get_bits (data[0x1a], 0, 1));

        /* parse EDID data */
        for (i = GCM_EDID_OFFSET_DATA_BLOCKS;
             i <= GCM_EDID_OFFSET_LAST_BLOCK;
             i += 18) {
                /* ignore pixel clock data */
                if (data[i] != 0)
                        continue;
                if (data[i+2] != 0)
                        continue;

                /* any useful blocks? */
                if (data[i+3] == GCM_DESCRIPTOR_DISPLAY_PRODUCT_NAME) {
                        tmp = gcm_edid_parse_string (&data[i+5]);
                        if (tmp != NULL) {
                                g_free (priv->monitor_name);
                                priv->monitor_name = tmp;
                        }
                } else if (data[i+3] == GCM_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER) {
                        tmp = gcm_edid_parse_string (&data[i+5]);
                        if (tmp != NULL) {
                                g_free (priv->serial_number);
                                priv->serial_number = tmp;
                        }
                } else if (data[i+3] == GCM_DESCRIPTOR_COLOR_MANAGEMENT_DATA) {
                        g_warning ("failing to parse color management data");
                } else if (data[i+3] == GCM_DESCRIPTOR_ALPHANUMERIC_DATA_STRING) {
                        tmp = gcm_edid_parse_string (&data[i+5]);
                        if (tmp != NULL) {
                                g_free (priv->eisa_id);
                                priv->eisa_id = tmp;
                        }
                } else if (data[i+3] == GCM_DESCRIPTOR_COLOR_POINT) {
                        if (data[i+3+9] != 0xff) {
                                /* extended EDID block(1) which contains
                                 * a better gamma value */
                                priv->gamma = ((gfloat) data[i+3+9] / 100) + 1;
                        }
                        if (data[i+3+14] != 0xff) {
                                /* extended EDID block(2) which contains
                                 * a better gamma value */
                                priv->gamma = ((gfloat) data[i+3+9] / 100) + 1;
                        }
                }
        }

        /* calculate checksum */
        priv->checksum = g_compute_checksum_for_data (G_CHECKSUM_MD5, data, length);
out:
        return ret;
}

static void
gcm_edid_class_init (GcmEdidClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gcm_edid_finalize;
        g_type_class_add_private (klass, sizeof (GcmEdidPrivate));
}

static void
gcm_edid_init (GcmEdid *edid)
{
        edid->priv = GCM_EDID_GET_PRIVATE (edid);
        edid->priv->pnp_ids = gnome_pnp_ids_new ();
        edid->priv->pnp_id = g_new0 (gchar, 4);
        edid->priv->red = cd_color_yxy_new ();
        edid->priv->green = cd_color_yxy_new ();
        edid->priv->blue = cd_color_yxy_new ();
        edid->priv->white = cd_color_yxy_new ();
}

static void
gcm_edid_finalize (GObject *object)
{
        GcmEdid *edid = GCM_EDID (object);
        GcmEdidPrivate *priv = edid->priv;

        g_free (priv->monitor_name);
        g_free (priv->vendor_name);
        g_free (priv->serial_number);
        g_free (priv->eisa_id);
        g_free (priv->checksum);
        g_free (priv->pnp_id);
        cd_color_yxy_free (priv->white);
        cd_color_yxy_free (priv->red);
        cd_color_yxy_free (priv->green);
        cd_color_yxy_free (priv->blue);
        g_object_unref (priv->pnp_ids);

        G_OBJECT_CLASS (gcm_edid_parent_class)->finalize (object);
}

GcmEdid *
gcm_edid_new (void)
{
        GcmEdid *edid;
        edid = g_object_new (GCM_TYPE_EDID, NULL);
        return GCM_EDID (edid);
}


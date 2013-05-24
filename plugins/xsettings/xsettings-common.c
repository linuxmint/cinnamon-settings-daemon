/*
 * Copyright © 2001 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL RED HAT
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Owen Taylor, Red Hat, Inc.
 */

#include <glib.h>

#include "string.h"
#include "stdlib.h"

#include <X11/Xlib.h>
#include <X11/Xmd.h>		/* For CARD32 */

#include "xsettings-common.h"

XSettingsSetting *
xsettings_setting_new (const gchar *name)
{
  XSettingsSetting *result;

  result = g_slice_new0 (XSettingsSetting);
  result->name = g_strdup (name);

  return result;
}

static gboolean
xsettings_variant_equal0 (GVariant *a,
                          GVariant *b)
{
  if (a == b)
    return TRUE;

  if (!a || !b)
    return FALSE;

  return g_variant_equal (a, b);
}

GVariant *
xsettings_setting_get (XSettingsSetting *setting)
{
  gint i;

  for (i = G_N_ELEMENTS (setting->value) - 1; 0 <= i; i--)
    if (setting->value[i])
      return setting->value[i];

  return NULL;
}

void
xsettings_setting_set (XSettingsSetting *setting,
                       gint              tier,
                       GVariant         *value,
                       guint32           serial)
{
  GVariant *old_value;

  old_value = xsettings_setting_get (setting);
  if (old_value)
    g_variant_ref (old_value);

  if (setting->value[tier])
    g_variant_unref (setting->value[tier]);
  setting->value[tier] = value ? g_variant_ref_sink (value) : NULL;

  if (!xsettings_variant_equal0 (old_value, xsettings_setting_get (setting)))
    setting->last_change_serial = serial;

  if (old_value)
    g_variant_unref (old_value);
}

void
xsettings_setting_free (XSettingsSetting *setting)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (setting->value); i++)
    if (setting->value[i])
      g_variant_unref (setting->value[i]);

  g_free (setting->name);

  g_slice_free (XSettingsSetting, setting);
}

char
xsettings_byte_order (void)
{
  CARD32 myint = 0x01020304;
  return (*(char *)&myint == 1) ? MSBFirst : LSBFirst;
}

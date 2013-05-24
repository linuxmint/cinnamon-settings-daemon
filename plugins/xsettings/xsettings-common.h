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
#ifndef XSETTINGS_COMMON_H
#define XSETTINGS_COMMON_H

#include <glib.h>

#define XSETTINGS_N_TIERS 2

typedef struct _XSettingsColor   XSettingsColor;
typedef struct _XSettingsSetting XSettingsSetting;

/* Types of settings possible. Enum values correspond to
 * protocol values.
 */
typedef enum 
{
  XSETTINGS_TYPE_INT     = 0,
  XSETTINGS_TYPE_STRING  = 1,
  XSETTINGS_TYPE_COLOR   = 2
} XSettingsType;

struct _XSettingsColor
{
  unsigned short red, green, blue, alpha;
};

struct _XSettingsSetting
{
  char *name;
  GVariant *value[XSETTINGS_N_TIERS];
  unsigned long last_change_serial;
};

XSettingsSetting *xsettings_setting_new   (const gchar      *name);
GVariant *        xsettings_setting_get   (XSettingsSetting *setting);
void              xsettings_setting_set   (XSettingsSetting *setting,
                                           gint              tier,
                                           GVariant         *value,
                                           guint32           serial);
void              xsettings_setting_free  (XSettingsSetting *setting);

char xsettings_byte_order (void);

#endif /* XSETTINGS_COMMON_H */

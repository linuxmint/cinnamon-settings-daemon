/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005-2011 Richard Hughes <richard@hughsie.com>
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

#include <math.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "gpm-common.h"

#define GPM_UP_TIME_PRECISION                   5*60
#define GPM_UP_TEXT_MIN_TIME                    120

/**
 * Return value: The time string, e.g. "2 hours 3 minutes"
 **/
gchar *
gpm_get_timestring (guint time_secs)
{
        char* timestring = NULL;
        gint  hours;
        gint  minutes;

        /* Add 0.5 to do rounding */
        minutes = (int) ( ( time_secs / 60.0 ) + 0.5 );

        if (minutes == 0) {
                timestring = g_strdup (_("Unknown time"));
                return timestring;
        }

        if (minutes < 60) {
                timestring = g_strdup_printf (ngettext ("%i minute",
                                                        "%i minutes",
                                                        minutes), minutes);
                return timestring;
        }

        hours = minutes / 60;
        minutes = minutes % 60;
        if (minutes == 0)
                timestring = g_strdup_printf (ngettext (
                                "%i hour",
                                "%i hours",
                                hours), hours);
        else
                /* TRANSLATOR: "%i %s %i %s" are "%i hours %i minutes"
                 * Swap order with "%2$s %2$i %1$s %1$i if needed */
                timestring = g_strdup_printf (_("%i %s %i %s"),
                                hours, ngettext ("hour", "hours", hours),
                                minutes, ngettext ("minute", "minutes", minutes));
        return timestring;
}

static const gchar *
gpm_upower_get_device_icon_index (UpDevice *device)
{
        gdouble percentage;
        /* get device properties */
        g_object_get (device, "percentage", &percentage, NULL);
        if (percentage < 10)
                return "000";
        else if (percentage < 30)
                return "020";
        else if (percentage < 50)
                return "040";
        else if (percentage < 70)
                return "060";
        else if (percentage < 90)
                return "080";
        return "100";
}

static const gchar *
gpm_upower_get_device_icon_suffix (UpDevice *device)
{
        gdouble percentage;
        /* get device properties */
        g_object_get (device, "percentage", &percentage, NULL);
        if (percentage < 10)
                return "caution";
        else if (percentage < 30)
                return "low";
        else if (percentage < 60)
                return "good";
        return "full";
}

static const gchar *
gpm_upower_get_precise_icon_index (UpDevice *device)
{
        gdouble percentage;
        /* get device properties */
        g_object_get (device, "percentage", &percentage, NULL);

        if (percentage < 10)
                return "0";
        else if (percentage < 20)
                return "10";
        else if (percentage < 30)
                return "20";
        else if (percentage < 40)
                return "30";
        else if (percentage < 50)
                return "40";
        else if (percentage < 60)
                return "50";
        else if (percentage < 70)
                return "60";
        else if (percentage < 80)
                return "70";
        else if (percentage < 90)
                return "80";
        else if (percentage < 99)
                return "90";

        return "100";
}

GIcon *
gpm_upower_get_device_icon (UpDevice *device, gboolean use_symbolic)
{
        GString *filename;
        gchar **iconnames;
        const gchar *kind_str;
        const gchar *suffix_str;
        const gchar *index_str;
        const gchar *precise_str;
        UpDeviceKind kind;
        UpDeviceState state;
        gboolean is_present;
        gdouble percentage;
        GIcon *icon = NULL;

        g_return_val_if_fail (device != NULL, NULL);

        /* get device properties */
        g_object_get (device,
                      "kind", &kind,
                      "state", &state,
                      "percentage", &percentage,
                      "is-present", &is_present,
                      NULL);

        /* get correct icon prefix */
        filename = g_string_new (NULL);

        /* get the icon from some simple rules */
        if (kind == UP_DEVICE_KIND_LINE_POWER) {
                if (use_symbolic)
                        g_string_append (filename, "xsi-battery-ac-symbolic;");
                g_string_append (filename, "ac-adapter;");

        } else if (kind == UP_DEVICE_KIND_MONITOR) {
                if (use_symbolic)
                        g_string_append (filename, "gpm-monitor-symbolic;");
                g_string_append (filename, "gpm-monitor;");

        } else {

                kind_str = up_device_kind_to_string (kind);
                if (!is_present) {
                        if (use_symbolic)
                                g_string_append (filename, "xsi-battery-missing-symbolic;");
                        g_string_append_printf (filename, "gpm-%s-missing;", kind_str);
                        g_string_append_printf (filename, "gpm-%s-000;", kind_str);
                        g_string_append (filename, "battery-missing;");

                } else {
                        switch (state) {
                        case UP_DEVICE_STATE_EMPTY:
                                if (use_symbolic)
                                        g_string_append (filename, "xsi-battery-level-0-symbolic;");
                                g_string_append_printf (filename, "gpm-%s-empty;", kind_str);
                                g_string_append_printf (filename, "gpm-%s-000;", kind_str);
                                g_string_append (filename, "battery-empty;");
                                break;
                        case UP_DEVICE_STATE_FULLY_CHARGED:
                                if (use_symbolic) {
                                        g_string_append (filename, "xsi-battery-level-100-charged-symbolic;");
                                }
                                g_string_append_printf (filename, "gpm-%s-full;", kind_str);
                                g_string_append_printf (filename, "gpm-%s-100;", kind_str);
                                g_string_append (filename, "battery-full-charged;");
                                g_string_append (filename, "battery-full-charging;");
                                break;
                        case UP_DEVICE_STATE_CHARGING:
                        case UP_DEVICE_STATE_PENDING_CHARGE:
                                suffix_str = gpm_upower_get_device_icon_suffix (device);
                                index_str = gpm_upower_get_device_icon_index (device);
                                precise_str = gpm_upower_get_precise_icon_index (device);
                                if (use_symbolic) {
                                        g_string_append_printf (filename, "xsi-battery-level-%s-charging-symbolic;", precise_str);
                                        g_string_append_printf (filename, "xsi-battery-%s-charging-symbolic;", suffix_str);
                                }
                                g_string_append_printf (filename, "gpm-%s-%s-charging;", kind_str, index_str);
                                g_string_append_printf (filename, "battery-%s-charging;", suffix_str);
                                break;
                        case UP_DEVICE_STATE_DISCHARGING:
                        case UP_DEVICE_STATE_PENDING_DISCHARGE:
                                suffix_str = gpm_upower_get_device_icon_suffix (device);
                                index_str = gpm_upower_get_device_icon_index (device);
                                precise_str = gpm_upower_get_precise_icon_index (device);
                                if (use_symbolic) {
                                        g_string_append_printf (filename, "xsi-battery-level-%s-symbolic;", precise_str);
                                        g_string_append_printf (filename, "xsi-battery-%s-symbolic;", suffix_str);
                                }
                                g_string_append_printf (filename, "gpm-%s-%s;", kind_str, index_str);
                                g_string_append_printf (filename, "battery-%s;", suffix_str);
                                break;
                        case UP_DEVICE_STATE_UNKNOWN:
                        case UP_DEVICE_STATE_LAST:
                        default:
                                if (use_symbolic)
                                        g_string_append (filename, "xsi-battery-missing-symbolic;");
                                g_string_append (filename, "gpm-battery-missing;");
                                g_string_append (filename, "battery-missing;");
                        }
                }
        }

        /* nothing matched */
        if (filename->len == 0) {
                g_warning ("nothing matched, falling back to default icon");
                g_string_append (filename, "dialog-warning;");
        }

        g_debug ("got filename: %s", filename->str);

        iconnames = g_strsplit (filename->str, ";", -1);
        icon = g_themed_icon_new_from_names (iconnames, -1);

        g_strfreev (iconnames);
        g_string_free (filename, TRUE);
        return icon;
}

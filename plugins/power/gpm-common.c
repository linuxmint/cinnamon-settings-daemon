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
                        g_string_append (filename, "ac-adapter-symbolic;");
                g_string_append (filename, "ac-adapter;");

        } else if (kind == UP_DEVICE_KIND_MONITOR) {
                if (use_symbolic)
                        g_string_append (filename, "gpm-monitor-symbolic;");
                g_string_append (filename, "gpm-monitor;");

        } else {

                kind_str = up_device_kind_to_string (kind);
                if (!is_present) {
                        if (use_symbolic)
                                g_string_append (filename, "battery-missing-symbolic;");
                        g_string_append_printf (filename, "gpm-%s-missing;", kind_str);
                        g_string_append_printf (filename, "gpm-%s-000;", kind_str);
                        g_string_append (filename, "battery-missing;");

                } else {
                        switch (state) {
                        case UP_DEVICE_STATE_EMPTY:
                                if (use_symbolic)
                                        g_string_append (filename, "battery-empty-symbolic;");
                                g_string_append_printf (filename, "gpm-%s-empty;", kind_str);
                                g_string_append_printf (filename, "gpm-%s-000;", kind_str);
                                g_string_append (filename, "battery-empty;");
                                break;
                        case UP_DEVICE_STATE_FULLY_CHARGED:
                                if (use_symbolic) {
                                        g_string_append (filename, "battery-level-100-charged-symbolic;");
                                        g_string_append (filename, "battery-full-charged-symbolic;");
                                        g_string_append (filename, "battery-full-charging-symbolic;");
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
                                        g_string_append_printf (filename, "battery-level-%s-charging-symbolic;", precise_str);
                                        g_string_append_printf (filename, "battery-%s-charging-symbolic;", suffix_str);
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
                                        g_string_append_printf (filename, "battery-level-%s-symbolic;", precise_str);
                                        g_string_append_printf (filename, "battery-%s-symbolic;", suffix_str);
                                }
                                g_string_append_printf (filename, "gpm-%s-%s;", kind_str, index_str);
                                g_string_append_printf (filename, "battery-%s;", suffix_str);
                                break;
                        case UP_DEVICE_STATE_UNKNOWN:
                        case UP_DEVICE_STATE_LAST:
                        default:
                                if (use_symbolic)
                                        g_string_append (filename, "battery-missing-symbolic;");
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

/**
 * gpm_precision_round_down:
 * @value: The input value
 * @smallest: The smallest increment allowed
 *
 * 101, 10      100
 * 95,  10      90
 * 0,   10      0
 * 112, 10      110
 * 100, 10      100
 **/
static gint
gpm_precision_round_down (gfloat value, gint smallest)
{
        gfloat division;
        if (fabs (value) < 0.01)
                return 0;
        if (smallest == 0) {
                g_warning ("divisor zero");
                return 0;
        }
        division = (gfloat) value / (gfloat) smallest;
        division = floorf (division);
        division *= smallest;
        return (gint) division;
}

gchar *
gpm_upower_get_device_summary (UpDevice *device)
{
        const gchar *kind_desc = NULL;
        const gchar *device_desc = NULL;
        GString *description;
        guint time_to_full_round;
        guint time_to_empty_round;
        gchar *time_to_full_str = NULL;
        gchar *time_to_empty_str = NULL;
        UpDeviceKind kind;
        UpDeviceState state;
        gdouble percentage;
        gboolean is_present;
        gint64 time_to_full;
        gint64 time_to_empty;

        /* get device properties */
        g_object_get (device,
                      "kind", &kind,
                      "state", &state,
                      "percentage", &percentage,
                      "is-present", &is_present,
                      "time-to-full", &time_to_full,
                      "time-to-empty", &time_to_empty,
                      NULL);

        description = g_string_new (NULL);
        kind_desc = gpm_device_kind_to_localised_string (kind, 1);
        device_desc = gpm_device_to_localised_string (device);

        /* not installed */
        if (!is_present) {
                g_string_append (description, device_desc);
                goto out;
        }

        /* don't display all the extra stuff for keyboards and mice */
        if (kind == UP_DEVICE_KIND_MOUSE ||
            kind == UP_DEVICE_KIND_KEYBOARD ||
            kind == UP_DEVICE_KIND_PDA) {
                g_string_append (description, kind_desc);
                g_string_append_printf (description, " (%.0f%%)", percentage);
                goto out;
        }

        /* we care if we are on AC */
        if (kind == UP_DEVICE_KIND_PHONE) {
                if (state == UP_DEVICE_STATE_CHARGING || !(state == UP_DEVICE_STATE_DISCHARGING)) {
                        g_string_append (description, device_desc);
                        g_string_append_printf (description, " (%.0f%%)", percentage);
                        goto out;
                }
                g_string_append (description, kind_desc);
                g_string_append_printf (description, " (%.0f%%)", percentage);
                goto out;
        }

        /* precalculate so we don't get Unknown time remaining */
        time_to_full_round = gpm_precision_round_down (time_to_full, GPM_UP_TIME_PRECISION);
        time_to_empty_round = gpm_precision_round_down (time_to_empty, GPM_UP_TIME_PRECISION);

        /* we always display "Laptop battery 16 minutes remaining" as we need to clarify what device we are referring to */
        if (state == UP_DEVICE_STATE_FULLY_CHARGED) {

                g_string_append (description, device_desc);

                if (kind == UP_DEVICE_KIND_BATTERY && time_to_empty_round > GPM_UP_TEXT_MIN_TIME) {
                        time_to_empty_str = gpm_get_timestring (time_to_empty_round);
                        g_string_append (description, " - ");
                        /* TRANSLATORS: The laptop battery is charged, and we know a time.
                         * The parameter is the time, e.g. 7 hours 6 minutes */
                        g_string_append_printf (description, _("provides %s laptop runtime"), time_to_empty_str);
                }
                goto out;
        }
        if (state == UP_DEVICE_STATE_DISCHARGING) {

                if (time_to_empty_round > GPM_UP_TEXT_MIN_TIME) {
                        time_to_empty_str = gpm_get_timestring (time_to_empty_round);
                        /* TRANSLATORS: the device is discharging, and we have a time remaining
                         * The first parameter is the device type, e.g. "Laptop battery" and
                         * the second is the time, e.g. 7 hours 6 minutes */
                        g_string_append_printf (description, _("%s %s remaining"),
                                                kind_desc, time_to_empty_str);
                        g_string_append_printf (description, " (%.0f%%)", percentage);
                } else {
                        g_string_append (description, device_desc);
                        g_string_append_printf (description, " (%.0f%%)", percentage);
                }
                goto out;
        }
        if (state == UP_DEVICE_STATE_CHARGING) {

                if (time_to_full_round > GPM_UP_TEXT_MIN_TIME &&
                    time_to_empty_round > GPM_UP_TEXT_MIN_TIME) {

                        /* display both discharge and charge time */
                        time_to_full_str = gpm_get_timestring (time_to_full_round);
                        time_to_empty_str = gpm_get_timestring (time_to_empty_round);

                        /* TRANSLATORS: device is charging, and we have a time to full and a percentage
                         * The first parameter is the device type, e.g. "Laptop battery" and
                         * the second is the time, e.g. "7 hours 6 minutes" */
                        g_string_append_printf (description, _("%s %s until charged"),
                                                kind_desc, time_to_full_str);
                        g_string_append_printf (description, " (%.0f%%)", percentage);

                        g_string_append (description, " - ");
                        /* TRANSLATORS: the device is charging, and we have a time to full and empty.
                         * The parameter is a time string, e.g. "7 hours 6 minutes" */
                        g_string_append_printf (description, _("provides %s battery runtime"),
                                                time_to_empty_str);
                } else if (time_to_full_round > GPM_UP_TEXT_MIN_TIME) {

                        /* display only charge time */
                        time_to_full_str = gpm_get_timestring (time_to_full_round);

                        /* TRANSLATORS: device is charging, and we have a time to full and a percentage.
                         * The first parameter is the device type, e.g. "Laptop battery" and
                         * the second is the time, e.g. "7 hours 6 minutes" */
                        g_string_append_printf (description, _("%s %s until charged"),
                                                kind_desc, time_to_full_str);
                        g_string_append_printf (description, " (%.0f%%)", percentage);
                } else {
                        g_string_append (description, device_desc);
                        g_string_append_printf (description, " (%.0f%%)", percentage);
                }
                goto out;
        }
        if (state == UP_DEVICE_STATE_PENDING_DISCHARGE) {
                g_string_append (description, device_desc);
                g_string_append_printf (description, " (%.0f%%)", percentage);
                goto out;
        }
        if (state == UP_DEVICE_STATE_PENDING_CHARGE) {
                g_string_append (description, device_desc);
                g_string_append_printf (description, " (%.0f%%)", percentage);
                goto out;
        }
        if (state == UP_DEVICE_STATE_EMPTY) {
                g_string_append (description, device_desc);
                goto out;
        }

        /* fallback */
        g_warning ("in an undefined state we are not charging or "
                     "discharging and the batteries are also not charged");
        g_string_append (description, device_desc);
        g_string_append_printf (description, " (%.0f%%)", percentage);
out:
        g_free (time_to_full_str);
        g_free (time_to_empty_str);
        return g_string_free (description, FALSE);
}

gchar *
gpm_upower_get_device_description (UpDevice *device)
{
        GString *details;
        const gchar *text;
        gchar *time_str;
        UpDeviceKind kind;
        UpDeviceState state;
        UpDeviceTechnology technology;
        gdouble percentage;
        gdouble capacity;
        gdouble energy;
        gdouble energy_full;
        gdouble energy_full_design;
        gdouble energy_rate;
        gboolean is_present;
        gint64 time_to_full;
        gint64 time_to_empty;
        gchar *vendor = NULL;
        gchar *serial = NULL;
        gchar *model = NULL;

        g_return_val_if_fail (device != NULL, NULL);

        /* get device properties */
        g_object_get (device,
                      "kind", &kind,
                      "state", &state,
                      "percentage", &percentage,
                      "is-present", &is_present,
                      "time-to-full", &time_to_full,
                      "time-to-empty", &time_to_empty,
                      "technology", &technology,
                      "capacity", &capacity,
                      "energy", &energy,
                      "energy-full", &energy_full,
                      "energy-full-design", &energy_full_design,
                      "energy-rate", &energy_rate,
                      "vendor", &vendor,
                      "serial", &serial,
                      "model", &model,
                      NULL);

        details = g_string_new ("");
        text = gpm_device_kind_to_localised_string (kind, 1);
        /* TRANSLATORS: the type of data, e.g. Laptop battery */
        g_string_append_printf (details, "<b>%s</b> %s\n", _("Product:"), text);

        if (!is_present) {
                /* TRANSLATORS: device is missing */
                g_string_append_printf (details, "<b>%s</b> %s\n", _("Status:"), _("Missing"));
        } else if (state == UP_DEVICE_STATE_FULLY_CHARGED) {
                /* TRANSLATORS: device is charged */
                g_string_append_printf (details, "<b>%s</b> %s\n", _("Status:"), _("Charged"));
        } else if (state == UP_DEVICE_STATE_CHARGING) {
                /* TRANSLATORS: device is charging */
                g_string_append_printf (details, "<b>%s</b> %s\n", _("Status:"), _("Charging"));
        } else if (state == UP_DEVICE_STATE_DISCHARGING) {
                /* TRANSLATORS: device is discharging */
                g_string_append_printf (details, "<b>%s</b> %s\n", _("Status:"), _("Discharging"));
        }

        if (percentage >= 0) {
                /* TRANSLATORS: percentage */
                g_string_append_printf (details, "<b>%s</b> %.1f%%\n", _("Percentage charge:"), percentage);
        }
        if (vendor) {
                /* TRANSLATORS: manufacturer */
                g_string_append_printf (details, "<b>%s</b> %s\n", _("Vendor:"), vendor);
        }
        if (technology != UP_DEVICE_TECHNOLOGY_UNKNOWN) {
                text = gpm_device_technology_to_localised_string (technology);
                /* TRANSLATORS: how the battery is made, e.g. Lithium Ion */
                g_string_append_printf (details, "<b>%s</b> %s\n", _("Technology:"), text);
        }
        if (serial) {
                /* TRANSLATORS: serial number of the battery */
                g_string_append_printf (details, "<b>%s</b> %s\n", _("Serial number:"), serial);
        }
        if (model) {
                /* TRANSLATORS: model number of the battery */
                g_string_append_printf (details, "<b>%s</b> %s\n", _("Model:"), model);
        }
        if (time_to_full > 0) {
                time_str = gpm_get_timestring (time_to_full);
                /* TRANSLATORS: time to fully charged */
                g_string_append_printf (details, "<b>%s</b> %s\n", _("Charge time:"), time_str);
                g_free (time_str);
        }
        if (time_to_empty > 0) {
                time_str = gpm_get_timestring (time_to_empty);
                /* TRANSLATORS: time to empty */
                g_string_append_printf (details, "<b>%s</b> %s\n", _("Discharge time:"), time_str);
                g_free (time_str);
        }
        if (capacity > 0) {
                const gchar *condition;
                if (capacity > 99) {
                        /* TRANSLATORS: Excellent, Good, Fair and Poor are all related to battery Capacity */
                        condition = _("Excellent");
                } else if (capacity > 90) {
                        condition = _("Good");
                } else if (capacity > 70) {
                        condition = _("Fair");
                } else {
                        condition = _("Poor");
                }
                /* TRANSLATORS: %.1f is a percentage and %s the condition (Excellent, Good, ...) */
                g_string_append_printf (details, "<b>%s</b> %.1f%% (%s)\n",
                                        _("Capacity:"), capacity, condition);
        }
        if (kind == UP_DEVICE_KIND_BATTERY) {
                if (energy > 0) {
                        /* TRANSLATORS: current charge */
                        g_string_append_printf (details, "<b>%s</b> %.1f Wh\n",
                                                _("Current charge:"), energy);
                }
                if (energy_full > 0 &&
                    energy_full_design != energy_full) {
                        /* TRANSLATORS: last full is the charge the battery was seen to charge to */
                        g_string_append_printf (details, "<b>%s</b> %.1f Wh\n",
                                                _("Last full charge:"), energy_full);
                }
                if (energy_full_design > 0) {
                        /* Translators:  */
                        /* TRANSLATORS: Design charge is the amount of charge the battery is designed to have when brand new */
                        g_string_append_printf (details, "<b>%s</b> %.1f Wh\n",
                                                _("Design charge:"), energy_full_design);
                }
                if (energy_rate > 0) {
                        /* TRANSLATORS: the charge or discharge rate */
                        g_string_append_printf (details, "<b>%s</b> %.1f W\n",
                                                _("Charge rate:"), energy_rate);
                }
        }
        if (kind == UP_DEVICE_KIND_MOUSE ||
            kind == UP_DEVICE_KIND_KEYBOARD) {
                if (energy > 0) {
                        /* TRANSLATORS: the current charge for CSR devices */
                        g_string_append_printf (details, "<b>%s</b> %.0f/7\n",
                                                _("Current charge:"), energy);
                }
                if (energy_full_design > 0) {
                        /* TRANSLATORS: the design charge for CSR devices */
                        g_string_append_printf (details, "<b>%s</b> %.0f/7\n",
                                                _("Design charge:"), energy_full_design);
                }
        }
        /* remove the last \n */
        g_string_truncate (details, details->len-1);

        g_free (vendor);
        g_free (serial);
        g_free (model);
        return g_string_free (details, FALSE);
}

const gchar *
gpm_device_kind_to_localised_string (UpDeviceKind kind, guint number)
{
        const gchar *text = NULL;
        switch (kind) {
        case UP_DEVICE_KIND_LINE_POWER:
                /* TRANSLATORS: system power cord */
                text = ngettext ("AC adapter", "AC adapters", number);
                break;
        case UP_DEVICE_KIND_BATTERY:
                /* TRANSLATORS: laptop primary battery */
                text = ngettext ("Laptop battery", "Laptop batteries", number);
                break;
        case UP_DEVICE_KIND_UPS:
                /* TRANSLATORS: battery-backed AC power source */
                text = ngettext ("UPS", "UPSs", number);
                break;
        case UP_DEVICE_KIND_MONITOR:
                /* TRANSLATORS: a monitor is a device to measure voltage and current */
                text = ngettext ("Monitor", "Monitors", number);
                break;
        case UP_DEVICE_KIND_MOUSE:
                /* TRANSLATORS: wireless mice with internal batteries */
                text = ngettext ("Mouse", "Mice", number);
                break;
        case UP_DEVICE_KIND_KEYBOARD:
                /* TRANSLATORS: wireless keyboard with internal battery */
                text = ngettext ("Keyboard", "Keyboards", number);
                break;
        case UP_DEVICE_KIND_PDA:
                /* TRANSLATORS: portable device */
                text = ngettext ("PDA", "PDAs", number);
                break;
        case UP_DEVICE_KIND_PHONE:
                /* TRANSLATORS: cell phone (mobile...) */
                text = ngettext ("Cell phone", "Cell phones", number);
                break;
        case UP_DEVICE_KIND_MEDIA_PLAYER:
                /* TRANSLATORS: media player, mp3 etc */
                text = ngettext ("Media player", "Media players", number);
                break;
        case UP_DEVICE_KIND_TABLET:
                /* TRANSLATORS: tablet device */
                text = ngettext ("Tablet", "Tablets", number);
                break;
        case UP_DEVICE_KIND_COMPUTER:
                /* TRANSLATORS: tablet device */
                text = ngettext ("Computer", "Computers", number);
                break;
#if UP_CHECK_VERSION(0,99,6)
        case UP_DEVICE_KIND_GAMING_INPUT:
                /* TRANSLATORS: gaming peripherals */
                text = ngettext ("Game controller", "Game controllers", number);
                break;
#endif
        case UP_DEVICE_KIND_UNKNOWN:
                text = ngettext ("Unknown device", "Unknown devices", number);
                break;
        case UP_DEVICE_KIND_LAST:
        default:
                g_warning ("enum unrecognised: %i", kind);
                text = up_device_kind_to_string (kind);
        }
        return text;
}

const gchar *
gpm_device_kind_to_icon (UpDeviceKind kind)
{
        const gchar *icon = NULL;
        switch (kind) {
        case UP_DEVICE_KIND_LINE_POWER:
                icon = "ac-adapter";
                break;
        case UP_DEVICE_KIND_BATTERY:
                icon = "battery";
                break;
        case UP_DEVICE_KIND_UPS:
                icon = "network-wired";
                break;
        case UP_DEVICE_KIND_MONITOR:
                icon = "application-certificate";
                break;
        case UP_DEVICE_KIND_MOUSE:
                icon = "input-mouse";
                break;
        case UP_DEVICE_KIND_KEYBOARD:
                icon = "input-keyboard";
                break;
        case UP_DEVICE_KIND_PDA:
                icon = "pda";
                break;
        case UP_DEVICE_KIND_PHONE:
                icon = "phone";
                break;
        case UP_DEVICE_KIND_MEDIA_PLAYER:
                icon = "multimedia-player";
                break;
        case UP_DEVICE_KIND_TABLET:
                icon = "input-tablet";
                break;
        case UP_DEVICE_KIND_COMPUTER:
                icon = "computer-apple-ipad";
                break;
#if UP_CHECK_VERSION(0,99,6)
        case UP_DEVICE_KIND_GAMING_INPUT:
                icon = "input-gaming";
                break;
#endif
        case UP_DEVICE_KIND_UNKNOWN:
                icon = "gtk-help";
                break;
        case UP_DEVICE_KIND_LAST:
        default:
                g_warning ("enum unrecognised: %i", kind);
                icon = "gtk-help";
        }
        return icon;
}

const gchar *
gpm_device_technology_to_localised_string (UpDeviceTechnology technology_enum)
{
        const gchar *technology = NULL;
        switch (technology_enum) {
        case UP_DEVICE_TECHNOLOGY_LITHIUM_ION:
                /* TRANSLATORS: battery technology */
                technology = _("Lithium Ion");
                break;
        case UP_DEVICE_TECHNOLOGY_LITHIUM_POLYMER:
                /* TRANSLATORS: battery technology */
                technology = _("Lithium Polymer");
                break;
        case UP_DEVICE_TECHNOLOGY_LITHIUM_IRON_PHOSPHATE:
                /* TRANSLATORS: battery technology */
                technology = _("Lithium Iron Phosphate");
                break;
        case UP_DEVICE_TECHNOLOGY_LEAD_ACID:
                /* TRANSLATORS: battery technology */
                technology = _("Lead acid");
                break;
        case UP_DEVICE_TECHNOLOGY_NICKEL_CADMIUM:
                /* TRANSLATORS: battery technology */
                technology = _("Nickel Cadmium");
                break;
        case UP_DEVICE_TECHNOLOGY_NICKEL_METAL_HYDRIDE:
                /* TRANSLATORS: battery technology */
                technology = _("Nickel metal hydride");
                break;
        case UP_DEVICE_TECHNOLOGY_UNKNOWN:
                /* TRANSLATORS: battery technology */
                technology = _("Unknown technology");
                break;
        case UP_DEVICE_TECHNOLOGY_LAST:
        default:
                g_assert_not_reached ();
                break;
        }
        return technology;
}

const gchar *
gpm_device_state_to_localised_string (UpDeviceState state)
{
        const gchar *state_string = NULL;

        switch (state) {
        case UP_DEVICE_STATE_CHARGING:
                /* TRANSLATORS: battery state */
                state_string = _("Charging");
                break;
        case UP_DEVICE_STATE_DISCHARGING:
                /* TRANSLATORS: battery state */
                state_string = _("Discharging");
                break;
        case UP_DEVICE_STATE_EMPTY:
                /* TRANSLATORS: battery state */
                state_string = _("Empty");
                break;
        case UP_DEVICE_STATE_FULLY_CHARGED:
                /* TRANSLATORS: battery state */
                state_string = _("Charged");
                break;
        case UP_DEVICE_STATE_PENDING_CHARGE:
                /* TRANSLATORS: battery state */
                state_string = _("Waiting to charge");
                break;
        case UP_DEVICE_STATE_PENDING_DISCHARGE:
                /* TRANSLATORS: battery state */
                state_string = _("Waiting to discharge");
                break;
        case UP_DEVICE_STATE_UNKNOWN:
                /* TRANSLATORS: battery state */
                state_string = _("Unknown");
                break;
        case UP_DEVICE_STATE_LAST:
        default:
                g_assert_not_reached ();
                break;
        }
        return state_string;
}

const gchar *
gpm_device_to_localised_string (UpDevice *device)
{
        UpDeviceState state;
        UpDeviceKind kind;
        gboolean present;

        /* get device parameters */
        g_object_get (device,
                      "is-present", &present,
                      "kind", &kind,
                      "state", &state,
                      NULL);

        /* laptop battery */
        if (kind == UP_DEVICE_KIND_BATTERY) {

                if (!present) {
                        /* TRANSLATORS: device not present */
                        return _("Laptop battery not present");
                }
                if (state == UP_DEVICE_STATE_CHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Laptop battery is charging");
                }
                if (state == UP_DEVICE_STATE_DISCHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Laptop battery is discharging");
                }
                if (state == UP_DEVICE_STATE_EMPTY) {
                        /* TRANSLATORS: battery state */
                        return _("Laptop battery is empty");
                }
                if (state == UP_DEVICE_STATE_FULLY_CHARGED) {
                        /* TRANSLATORS: battery state */
                        return _("Laptop battery is charged");
                }
                if (state == UP_DEVICE_STATE_PENDING_CHARGE) {
                        /* TRANSLATORS: battery state */
                        return _("Laptop battery is waiting to charge");
                }
                if (state == UP_DEVICE_STATE_PENDING_DISCHARGE) {
                        /* TRANSLATORS: battery state */
                        return _("Laptop battery is waiting to discharge");
                }
        }

        /* UPS */
        if (kind == UP_DEVICE_KIND_UPS) {

                if (state == UP_DEVICE_STATE_CHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("UPS is charging");
                }
                if (state == UP_DEVICE_STATE_DISCHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("UPS is discharging");
                }
                if (state == UP_DEVICE_STATE_EMPTY) {
                        /* TRANSLATORS: battery state */
                        return _("UPS is empty");
                }
                if (state == UP_DEVICE_STATE_FULLY_CHARGED) {
                        /* TRANSLATORS: battery state */
                        return _("UPS is charged");
                }
        }

        /* mouse */
        if (kind == UP_DEVICE_KIND_MOUSE) {

                if (state == UP_DEVICE_STATE_CHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Mouse is charging");
                }
                if (state == UP_DEVICE_STATE_DISCHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Mouse is discharging");
                }
                if (state == UP_DEVICE_STATE_EMPTY) {
                        /* TRANSLATORS: battery state */
                        return _("Mouse is empty");
                }
                if (state == UP_DEVICE_STATE_FULLY_CHARGED) {
                        /* TRANSLATORS: battery state */
                        return _("Mouse is charged");
                }
        }

        /* keyboard */
        if (kind == UP_DEVICE_KIND_KEYBOARD) {

                if (state == UP_DEVICE_STATE_CHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Keyboard is charging");
                }
                if (state == UP_DEVICE_STATE_DISCHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Keyboard is discharging");
                }
                if (state == UP_DEVICE_STATE_EMPTY) {
                        /* TRANSLATORS: battery state */
                        return _("Keyboard is empty");
                }
                if (state == UP_DEVICE_STATE_FULLY_CHARGED) {
                        /* TRANSLATORS: battery state */
                        return _("Keyboard is charged");
                }
        }

        /* PDA */
        if (kind == UP_DEVICE_KIND_PDA) {

                if (state == UP_DEVICE_STATE_CHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("PDA is charging");
                }
                if (state == UP_DEVICE_STATE_DISCHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("PDA is discharging");
                }
                if (state == UP_DEVICE_STATE_EMPTY) {
                        /* TRANSLATORS: battery state */
                        return _("PDA is empty");
                }
                if (state == UP_DEVICE_STATE_FULLY_CHARGED) {
                        /* TRANSLATORS: battery state */
                        return _("PDA is charged");
                }
        }

        /* phone */
        if (kind == UP_DEVICE_KIND_PHONE) {

                if (state == UP_DEVICE_STATE_CHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Cell phone is charging");
                }
                if (state == UP_DEVICE_STATE_DISCHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Cell phone is discharging");
                }
                if (state == UP_DEVICE_STATE_EMPTY) {
                        /* TRANSLATORS: battery state */
                        return _("Cell phone is empty");
                }
                if (state == UP_DEVICE_STATE_FULLY_CHARGED) {
                        /* TRANSLATORS: battery state */
                        return _("Cell phone is charged");
                }
        }

        /* media player */
        if (kind == UP_DEVICE_KIND_MEDIA_PLAYER) {

                if (state == UP_DEVICE_STATE_CHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Media player is charging");
                }
                if (state == UP_DEVICE_STATE_DISCHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Media player is discharging");
                }
                if (state == UP_DEVICE_STATE_EMPTY) {
                        /* TRANSLATORS: battery state */
                        return _("Media player is empty");
                }
                if (state == UP_DEVICE_STATE_FULLY_CHARGED) {
                        /* TRANSLATORS: battery state */
                        return _("Media player is charged");
                }
        }

        /* tablet */
        if (kind == UP_DEVICE_KIND_TABLET) {

                if (state == UP_DEVICE_STATE_CHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Tablet is charging");
                }
                if (state == UP_DEVICE_STATE_DISCHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Tablet is discharging");
                }
                if (state == UP_DEVICE_STATE_EMPTY) {
                        /* TRANSLATORS: battery state */
                        return _("Tablet is empty");
                }
                if (state == UP_DEVICE_STATE_FULLY_CHARGED) {
                        /* TRANSLATORS: battery state */
                        return _("Tablet is charged");
                }
        }

        /* computer */
        if (kind == UP_DEVICE_KIND_COMPUTER) {

                if (state == UP_DEVICE_STATE_CHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Computer is charging");
                }
                if (state == UP_DEVICE_STATE_DISCHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Computer is discharging");
                }
                if (state == UP_DEVICE_STATE_EMPTY) {
                        /* TRANSLATORS: battery state */
                        return _("Computer is empty");
                }
                if (state == UP_DEVICE_STATE_FULLY_CHARGED) {
                        /* TRANSLATORS: battery state */
                        return _("Computer is charged");
                }
        }

#if UP_CHECK_VERSION(0,99,6)
        /* computer */
        if (kind == UP_DEVICE_KIND_GAMING_INPUT) {

                if (state == UP_DEVICE_STATE_CHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Game controller is charging");
                }
                if (state == UP_DEVICE_STATE_DISCHARGING) {
                        /* TRANSLATORS: battery state */
                        return _("Game controller is discharging");
                }
                if (state == UP_DEVICE_STATE_EMPTY) {
                        /* TRANSLATORS: battery state */
                        return _("Game controller is empty");
                }
                if (state == UP_DEVICE_STATE_FULLY_CHARGED) {
                        /* TRANSLATORS: battery state */
                        return _("Game controller is charged");
                }
        }
#endif

        return gpm_device_kind_to_localised_string (kind, 1);
}

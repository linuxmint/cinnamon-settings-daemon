/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <glib.h>
#include <math.h>

#include "csd-night-light-common.h"

static gdouble
deg2rad (gdouble degrees)
{
        return (M_PI * degrees) / 180.f;
}

static gdouble
rad2deg (gdouble radians)
{
        return radians * (180.f / M_PI);
}

/*
 * Formulas taken from https://www.esrl.noaa.gov/gmd/grad/solcalc/calcdetails.html
 *
 * The returned values are fractional hours, so 6am would be 6.0 and 4:30pm
 * would be 16.5.
 *
 * The values returned by this function might not make sense for locations near
 * the polar regions. For example, in the north of Lapland there might not be
 * a sunrise at all.
 */
gboolean
csd_night_light_get_sunrise_sunset (GDateTime *dt,
                                    gdouble pos_lat, gdouble pos_long,
                                    gdouble *sunrise, gdouble *sunset)
{
        g_autoptr(GDateTime) dt_zero = g_date_time_new_utc (1900, 1, 1, 0, 0, 0);
        GTimeSpan ts = g_date_time_difference (dt, dt_zero);

        g_return_val_if_fail (pos_lat <= 90.f && pos_lat >= -90.f, FALSE);
        g_return_val_if_fail (pos_long <= 180.f && pos_long >= -180.f, FALSE);

        gdouble tz_offset = (gdouble) g_date_time_get_utc_offset (dt) / G_USEC_PER_SEC / 60 / 60; // B5
        gdouble date_as_number = ts / G_USEC_PER_SEC / 24 / 60 / 60 + 2;  // B7
        gdouble time_past_local_midnight = 0;  // E2, unused in this calculation
        gdouble julian_day = date_as_number + 2415018.5 +
                        time_past_local_midnight - tz_offset / 24;
        gdouble julian_century = (julian_day - 2451545) / 36525;
        gdouble geom_mean_long_sun = fmod (280.46646 + julian_century *
                        (36000.76983 + julian_century * 0.0003032), 360); // I2
        gdouble geom_mean_anom_sun = 357.52911 + julian_century *
                        (35999.05029 - 0.0001537 * julian_century);  // J2
        gdouble eccent_earth_orbit = 0.016708634 - julian_century *
                        (0.000042037 + 0.0000001267 * julian_century); // K2
        gdouble sun_eq_of_ctr = sin (deg2rad (geom_mean_anom_sun)) *
                        (1.914602 - julian_century * (0.004817 + 0.000014 * julian_century)) +
                        sin (deg2rad (2 * geom_mean_anom_sun)) * (0.019993 - 0.000101 * julian_century) +
                        sin (deg2rad (3 * geom_mean_anom_sun)) * 0.000289; // L2
        gdouble sun_true_long = geom_mean_long_sun + sun_eq_of_ctr; // M2
        gdouble sun_app_long = sun_true_long - 0.00569 - 0.00478 *
                        sin (deg2rad (125.04 - 1934.136 * julian_century)); // P2
        gdouble mean_obliq_ecliptic = 23 +  (26 +  ((21.448 - julian_century *
                        (46.815 + julian_century * (0.00059 - julian_century * 0.001813)))) / 60) / 60; // Q2
        gdouble obliq_corr = mean_obliq_ecliptic + 0.00256 *
                        cos (deg2rad (125.04 - 1934.136 * julian_century)); // R2
        gdouble sun_declin = rad2deg (asin (sin (deg2rad (obliq_corr)) *
                                            sin (deg2rad (sun_app_long)))); // T2
        gdouble var_y = tan (deg2rad (obliq_corr/2)) * tan (deg2rad (obliq_corr / 2)); // U2
        gdouble eq_of_time = 4 * rad2deg (var_y * sin (2 * deg2rad (geom_mean_long_sun)) -
                        2 * eccent_earth_orbit * sin (deg2rad (geom_mean_anom_sun)) +
                        4 * eccent_earth_orbit * var_y *
                                sin (deg2rad (geom_mean_anom_sun)) *
                                cos (2 * deg2rad (geom_mean_long_sun)) -
                        0.5 * var_y * var_y * sin (4 * deg2rad (geom_mean_long_sun)) -
                        1.25 * eccent_earth_orbit * eccent_earth_orbit *
                                sin (2 * deg2rad (geom_mean_anom_sun))); // V2
        gdouble ha_sunrise = rad2deg (acos (cos (deg2rad (90.833)) / (cos (deg2rad (pos_lat)) *
                        cos (deg2rad (sun_declin))) - tan (deg2rad (pos_lat)) *
                        tan (deg2rad (sun_declin)))); // W2
        gdouble solar_noon =  (720 - 4 * pos_long - eq_of_time + tz_offset * 60) / 1440; // X2
        gdouble sunrise_time = solar_noon - ha_sunrise * 4 / 1440; //  Y2
        gdouble sunset_time = solar_noon + ha_sunrise * 4 / 1440; // Z2

        /* convert to hours */
        if (sunrise != NULL)
                *sunrise = sunrise_time * 24;
        if (sunset != NULL)
                *sunset = sunset_time * 24;
        return TRUE;
}

gdouble
csd_night_light_frac_day_from_dt (GDateTime *dt)
{
        return g_date_time_get_hour (dt) +
                (gdouble) g_date_time_get_minute (dt) / 60.f +
                (gdouble) g_date_time_get_second (dt) / 3600.f;
}

gchar *
csd_night_light_time_string_from_frac (gdouble fraction)
{
    g_autoptr(GDateTime) dt = g_date_time_new_local (2000, 1, 1, (int) trunc (fraction), (int) ((fraction - trunc(fraction)) * 60.0f), 0);
    return g_date_time_format (dt, "%H:%M");
}

gboolean
csd_night_light_frac_day_is_between (gdouble  value,
                                     gdouble  start,
                                     gdouble  end)
{
        /* wrap end to the next day if it is before start,
         * considering equal values as a full 24h period
         */
        if (end <= start)
                end += 24;

        /* wrap value to the next day if it is before the range */
        if (value < start && value < end)
                value += 24;

        /* Check whether value falls into range; together with the 24h
         * wrap around above this means that TRUE is always returned when
         * start == end.
         */
        return value >= start && value < end;
}

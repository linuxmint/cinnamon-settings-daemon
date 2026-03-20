/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
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

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "gnome-datetime-source.h"

#include "csd-color-state.h"

#include "csd-night-mode.h"
#include "csd-night-mode-common.h"
#include "tz-coords.h"

struct _CsdNightMode {
        GObject            parent;
        GSettings         *settings;
        GSettings         *theme_settings;
        GSettings         *x_theme_settings;
        GSettings         *cinnamon_theme_settings;
        gboolean           cached_light_active_unset;
        gboolean           cached_theme_active_unset;
        gboolean           light_forced;
        gboolean           theme_forced;
        gboolean           disabled_until_tmw;
        GDateTime         *disabled_until_tmw_dt;
        GSource           *source;
        guint              validate_id;
        gdouble            cached_sunrise;
        gdouble            cached_sunset;
        gdouble            cached_temperature;
        gboolean           cached_light_active;
        gboolean           cached_theme_active;
        gboolean           smooth_enabled;
        GTimer            *smooth_timer;
        guint              smooth_id;
        gdouble            smooth_target_temperature;
        GCancellable      *cancellable;
        GDateTime         *datetime_override;
};

enum {
        PROP_0,
        PROP_LIGHT_ACTIVE,
        PROP_THEME_ACTIVE,
        PROP_SUNRISE,
        PROP_SUNSET,
        PROP_TEMPERATURE,
        PROP_DISABLED_UNTIL_TMW,
        PROP_LIGHT_FORCED,
        PROP_THEME_FORCED,
        PROP_LAST
};

enum {
        NIGHT_MODE_SCHEDULE_AUTO = 0,
        NIGHT_MODE_SCHEDULE_MANUAL = 1,
        NIGHT_MODE_SCHEDULE_ALWAYS_ON = 2
};

#define CSD_NIGHT_MODE_SCHEDULE_TIMEOUT      5       /* seconds */
#define CSD_NIGHT_MODE_POLL_TIMEOUT          60      /* seconds */
#define CSD_NIGHT_LIGHT_POLL_SMEAR            1       /* hours */
#define CSD_NIGHT_LIGHT_SMOOTH_SMEAR          5.f     /* seconds */

#define CSD_FRAC_DAY_MAX_DELTA                  (1.f/60.f)     /* 1 minute */
#define CSD_TEMPERATURE_MAX_DELTA               (10.f)          /* Kelvin */

static void poll_timeout_destroy (CsdNightMode *self);
static void poll_timeout_create (CsdNightMode *self);
static void night_mode_recheck (CsdNightMode *self);
static void night_light_recheck (CsdNightMode *self);
static void night_theme_recheck (CsdNightMode *self);

G_DEFINE_TYPE (CsdNightMode, csd_night_mode, G_TYPE_OBJECT);

static GDateTime *
csd_night_mode_get_date_time_now (CsdNightMode *self)
{
        if (self->datetime_override != NULL)
                return g_date_time_ref (self->datetime_override);
        return g_date_time_new_now_local ();
}

void
csd_night_mode_set_date_time_now (CsdNightMode *self, GDateTime *datetime)
{
        if (self->datetime_override != NULL)
                g_date_time_unref (self->datetime_override);
        self->datetime_override = g_date_time_ref (datetime);

        night_mode_recheck (self);
}

static void
poll_smooth_destroy (CsdNightMode *self)
{
        if (self->smooth_id != 0) {
                g_source_remove (self->smooth_id);
                self->smooth_id = 0;
        }
        if (self->smooth_timer != NULL)
                g_clear_pointer (&self->smooth_timer, g_timer_destroy);
}

void
csd_night_mode_set_smooth_enabled (CsdNightMode *self,
                                   gboolean smooth_enabled)
{
        /* ensure the timeout is stopped if called at runtime */
        if (!smooth_enabled)
                poll_smooth_destroy (self);
        self->smooth_enabled = smooth_enabled;
}

static gdouble
linear_interpolate (gdouble val1, gdouble val2, gdouble factor)
{
        g_return_val_if_fail (factor >= 0.f, -1.f);
        g_return_val_if_fail (factor <= 1.f, -1.f);
        return ((val1 - val2) * factor) + val2;
}

static gboolean
update_cached_sunrise_sunset (CsdNightMode *self)
{
        gboolean ret = FALSE;
        gdouble latitude;
        gdouble longitude;
        gdouble sunrise;
        gdouble sunset;
        g_autoptr(GVariant) tmp = NULL;
        g_autoptr(GDateTime) dt_now = csd_night_mode_get_date_time_now (self);

        /* calculate the sunrise/sunset for the location */
        tmp = g_settings_get_value (self->settings, "night-light-last-coordinates");
        g_variant_get (tmp, "(dd)", &latitude, &longitude);
        if (latitude > 90.f || latitude < -90.f)
                return FALSE;
        if (longitude > 180.f || longitude < -180.f)
                return FALSE;
        if (!csd_night_mode_get_sunrise_sunset (dt_now, latitude, longitude,
                                                &sunrise, &sunset)) {
                g_warning ("failed to get sunset/sunrise for %.3f,%.3f",
                           latitude, longitude);
                return FALSE;
        }

        /* anything changed */
        if (ABS (self->cached_sunrise - sunrise) > CSD_FRAC_DAY_MAX_DELTA) {
                self->cached_sunrise = sunrise;
                g_object_notify (G_OBJECT (self), "sunrise");
                g_autofree gchar *formatted = csd_night_mode_time_string_from_frac (sunrise);
                g_debug ("Sunrise updated: %.3f (%s)", sunrise, formatted);
                ret = TRUE;
        }
        if (ABS (self->cached_sunset - sunset) > CSD_FRAC_DAY_MAX_DELTA) {
                self->cached_sunset = sunset;
                g_object_notify (G_OBJECT (self), "sunset");
                g_autofree gchar *formatted = csd_night_mode_time_string_from_frac (sunset);
                g_debug ("Sunset updated: %.3f (%s)", sunset, formatted);
                ret = TRUE;
        }
        return ret;
}

static void
csd_night_light_set_temperature_internal (CsdNightMode *self, gdouble temperature)
{
        if (ABS (self->cached_temperature - temperature) <= CSD_TEMPERATURE_MAX_DELTA)
                return;
        self->cached_temperature = temperature;
        g_object_notify (G_OBJECT (self), "temperature");
}

static gboolean
csd_night_light_smooth_cb (gpointer user_data)
{
        CsdNightMode *self = CSD_NIGHT_MODE (user_data);
        gdouble tmp;
        gdouble frac;

        /* find fraction */
        frac = g_timer_elapsed (self->smooth_timer, NULL) / CSD_NIGHT_LIGHT_SMOOTH_SMEAR;
        if (frac >= 1.f) {
                csd_night_light_set_temperature_internal (self,
                                                          self->smooth_target_temperature);
                self->smooth_id = 0;
                return G_SOURCE_REMOVE;
        }

        /* set new temperature step using log curve */
        tmp = self->smooth_target_temperature - self->cached_temperature;
        tmp *= frac;
        tmp += self->cached_temperature;
        csd_night_light_set_temperature_internal (self, tmp);

        return G_SOURCE_CONTINUE;
}

static void
poll_smooth_create (CsdNightMode *self, gdouble temperature)
{
        g_assert (self->smooth_id == 0);
        self->smooth_target_temperature = temperature;
        self->smooth_timer = g_timer_new ();
        self->smooth_id = g_timeout_add (50, csd_night_light_smooth_cb, self);
}

static void
csd_night_light_set_temperature (CsdNightMode *self, gdouble temperature)
{
        /* immediate */
        if (!self->smooth_enabled) {
                csd_night_light_set_temperature_internal (self, temperature);
                return;
        }

        /* Destroy any smooth transition, it will be recreated if neccessary */
        poll_smooth_destroy (self);

        /* small jump */
        if (ABS (temperature - self->cached_temperature) < CSD_TEMPERATURE_MAX_DELTA) {
                csd_night_light_set_temperature_internal (self, temperature);
                return;
        }

        /* smooth out the transition */
        poll_smooth_create (self, temperature);
}

static void
night_theme_switch_on (CsdNightMode *self)
{
        gboolean is_active;
        /* get backups */
        gchar *backup_day_theme = g_settings_get_string (self->settings, "backup-day-theme");
        gchar *backup_day_icon_theme = g_settings_get_string (self->settings, "backup-day-icon-theme");
        gchar *backup_day_cinnamon_theme = g_settings_get_string (self->settings, "backup-day-cinnamon-theme");
        /* check if there are backups */
        is_active = (
                (backup_day_theme != NULL &&
                g_strcmp0 (backup_day_theme, "") != 0) ||
                (backup_day_icon_theme != NULL &&
                g_strcmp0 (backup_day_icon_theme, "") != 0) ||
                (backup_day_cinnamon_theme != NULL &&
                g_strcmp0 (backup_day_cinnamon_theme, "") != 0)
        );
        /* free memory */
        g_free (backup_day_theme);
        g_free (backup_day_icon_theme);
        g_free (backup_day_cinnamon_theme);
        
        if (is_active) {
                g_debug ("night theme already active => not switching on");
                return;
        }
        g_debug ("switching on night theme...");
        /* copy values to the backups */
        g_settings_set_string (self->settings, "backup-day-theme", g_settings_get_string (self->theme_settings, "gtk-theme"));
        g_settings_set_string (self->settings, "backup-day-cinnamon-theme", g_settings_get_string (self->cinnamon_theme_settings, "name"));
        g_settings_set_string (self->settings, "backup-day-icon-theme", g_settings_get_string (self->theme_settings, "icon-theme"));
        g_settings_set_enum (self->settings, "backup-day-color-scheme", g_settings_get_enum (self->x_theme_settings, "color-scheme"));
        /* activate the night themes */
        g_settings_set_string (self->theme_settings, "gtk-theme", g_settings_get_string (self->settings, "night-theme"));
        g_settings_set_string (self->theme_settings, "icon-theme", g_settings_get_string (self->settings, "night-icon-theme"));
        g_settings_set_string (self->cinnamon_theme_settings, "name", g_settings_get_string (self->settings, "night-cinnamon-theme"));
        g_settings_set_enum (self->x_theme_settings, "color-scheme", g_settings_get_enum (self->settings, "night-color-scheme"));
}

static void
night_theme_switch_off (CsdNightMode *self)
{
        gboolean is_active;
        /* get backups */
        gchar *backup_day_theme = g_settings_get_string (self->settings, "backup-day-theme");
        gchar *backup_day_icon_theme = g_settings_get_string (self->settings, "backup-day-icon-theme");
        gchar *backup_day_cinnamon_theme = g_settings_get_string (self->settings, "backup-day-cinnamon-theme");
        /* check if there are no backups */
        is_active = (
                (backup_day_theme != NULL &&
                g_strcmp0 (backup_day_theme, "") != 0) ||
                (backup_day_icon_theme != NULL &&
                g_strcmp0 (backup_day_icon_theme, "") != 0) ||
                (backup_day_cinnamon_theme != NULL &&
                g_strcmp0 (backup_day_cinnamon_theme, "") != 0)
        );
        
        if (!is_active) {
                g_debug ("night theme already inactive => not switching off");
                g_free (backup_day_theme);
                g_free (backup_day_icon_theme);
                g_free (backup_day_cinnamon_theme);
                return;
        }
        g_debug ("switching off night theme...");
        /* save the current night theme */
        g_settings_set_string (self->settings, "night-theme", g_settings_get_string (self->theme_settings, "gtk-theme"));
        g_settings_set_string (self->settings, "night-icon-theme", g_settings_get_string (self->theme_settings, "icon-theme"));
        g_settings_set_string (self->settings, "night-cinnamon-theme", g_settings_get_string (self->cinnamon_theme_settings, "name"));
        g_settings_set_enum (self->settings, "night-color-scheme", g_settings_get_enum (self->x_theme_settings, "color-scheme"));
        /* restore the backups */
        g_settings_set_string (self->theme_settings, "gtk-theme", backup_day_theme);
        g_settings_set_string (self->theme_settings, "icon-theme", backup_day_icon_theme);
        g_settings_set_string (self->cinnamon_theme_settings, "name", backup_day_cinnamon_theme);
        g_settings_set_enum (self->x_theme_settings, "color-scheme", g_settings_get_enum (self->settings, "backup-day-color-scheme"));
        /* clear the backups */
        g_settings_set_string (self->settings, "backup-day-theme", "");
        g_settings_set_string (self->settings, "backup-day-icon-theme", "");
        g_settings_set_string (self->settings, "backup-day-cinnamon-theme", "");
        /* free memory */
        g_free (backup_day_theme);
        g_free (backup_day_icon_theme);
        g_free (backup_day_cinnamon_theme);
}

static void
csd_night_light_set_active (CsdNightMode *self, gboolean active)
{
        if (self->cached_light_active == active && !self->cached_light_active_unset) {
                return;
        } else if (self->cached_light_active_unset) {
                self->cached_light_active_unset = FALSE;
        }
        self->cached_light_active = active;

        /* ensure set to unity temperature */
        if (!active)
                csd_night_light_set_temperature (self, CSD_COLOR_TEMPERATURE_DEFAULT);

        g_object_notify (G_OBJECT (self), "light-active");
}

static void
csd_night_theme_set_active (CsdNightMode *self, gboolean active)
{
        if (self->cached_theme_active == active && !self->cached_theme_active_unset) {
                return;
        } else if (self->cached_theme_active_unset) {
                self->cached_theme_active_unset = FALSE;
        }
        self->cached_theme_active = active;

        /* switch off theme if not active & switch on else */
        if (!active)
                night_theme_switch_off (self);
        else
                night_theme_switch_on (self);

        g_object_notify (G_OBJECT (self), "theme-active");
}

static void
night_mode_recheck (CsdNightMode *self)
{
        if (self->light_forced && self->theme_forced) {
                night_light_recheck (self);
                night_theme_recheck (self);
                return;
        }

        /* calculate the position of the sun */
        update_cached_sunrise_sunset (self);

        gdouble frac_day;
        g_autoptr(GDateTime) dt_now = csd_night_mode_get_date_time_now (self);
        frac_day = csd_night_mode_frac_day_from_dt (dt_now);

        /* check if it's still not tomorrow */
        if (self->disabled_until_tmw) {
                GTimeSpan time_span;
                gboolean reset = FALSE;

                time_span = g_date_time_difference (dt_now, self->disabled_until_tmw_dt);

                /* Reset if disabled until tomorrow is more than 24h ago. */
                if (time_span > (GTimeSpan) 24 * 60 * 60 * 1000000) {
                        g_debug ("night mode disabled-until-tomorrow is older than 24h, resetting disabled-until-tomorrow");
                        reset = TRUE;
                } else if (time_span > 0) {
                        /* Or if a sunrise lies between the time it was disabled and now. */
                        gdouble frac_disabled;
                        frac_disabled = csd_night_mode_frac_day_from_dt (self->disabled_until_tmw_dt);
                        if (frac_disabled != frac_day &&
                            csd_night_mode_frac_day_is_between (self->cached_sunrise,
                                                                frac_disabled,
                                                                frac_day)) {
                                g_debug ("night mode sun rise happened, resetting disabled-until-tomorrow");
                                reset = TRUE;
                        }
                }

                if (reset) {
                        self->disabled_until_tmw = FALSE;
                        g_clear_pointer(&self->disabled_until_tmw_dt, g_date_time_unref);
                        g_object_notify (G_OBJECT (self), "disabled-until-tmw");
                } else {
                        g_debug ("night mode disabled - it's still not tomorrow ):");
                }
        }

        night_light_recheck (self);
        night_theme_recheck (self);
}

static void
night_theme_recheck (CsdNightMode *self)
{
        gdouble frac_day;
        gdouble schedule_from = -1.f;
        gdouble schedule_to = -1.f;
        g_autoptr(GDateTime) dt_now = csd_night_mode_get_date_time_now (self);

        /* If forced (e.g. for preview), just switch on and return */
        if (self->theme_forced) {
                night_theme_switch_on (self);
                g_debug ("night theme forced on");
                return;
        }

        if (!g_settings_get_boolean (self->settings, "night-theme-enabled")) {
                /* settings say NO */
                csd_night_theme_set_active (self, FALSE);
                g_debug ("night theme disabled");
                return;
        }

        switch (g_settings_get_enum (self->settings, "night-light-schedule-mode")) {
        case NIGHT_MODE_SCHEDULE_ALWAYS_ON:
                /* turn on and return */
                g_debug ("night mode always on - switching on theme");
                csd_night_theme_set_active (self, TRUE);
                return;
        case NIGHT_MODE_SCHEDULE_AUTO:
                /* was updated in night_mode_recheck(self) */
                schedule_to = self->cached_sunrise;
                schedule_from = self->cached_sunset;
                break;
        default:
                break;
        }

         /* fall back to manual settings (if nothing was calculated) */
        if (schedule_to <= 0.f || schedule_from <= 0.f) {
                schedule_from = g_settings_get_double (self->settings,
                                                       "night-light-schedule-from");
                schedule_to = g_settings_get_double (self->settings,
                                                     "night-light-schedule-to");
        }

        /* get the current hour of a day as a fraction */
        frac_day = csd_night_mode_frac_day_from_dt (dt_now);

        /* disabled until tomorrow - updated in night_mode_recheck(self) */
        if (self->disabled_until_tmw) {
                csd_night_theme_set_active(self, FALSE);
                return;
        }

        /* theme is activated from schedule_from to schedule_to - easy */
        if (!csd_night_mode_frac_day_is_between (frac_day,
                                                 schedule_from,
                                                 schedule_to)) {
                g_debug ("not time for night theme");
                csd_night_theme_set_active (self, FALSE);
                return;
        }

        csd_night_theme_set_active(self, TRUE);
        g_debug ("night theme is active!");
}

static void
night_light_recheck (CsdNightMode *self)
{
        gdouble frac_day;
        gdouble schedule_from = -1.f;
        gdouble schedule_to = -1.f;
        gdouble smear = CSD_NIGHT_LIGHT_POLL_SMEAR; /* hours */
        guint temperature;
        guint temp_smeared;
        g_autoptr(GDateTime) dt_now = csd_night_mode_get_date_time_now (self);

        /* Forced mode, just set the temperature to night.
         * Proper rechecking will happen once forced mode is disabled again */
        if (self->light_forced) {
                temperature = g_settings_get_uint (self->settings, "night-light-temperature");
                csd_night_light_set_temperature (self, temperature);
                g_debug ("night light forced on");
                return;
        }

        /* enabled */
        if (!g_settings_get_boolean (self->settings, "night-light-enabled")) {
                g_debug ("night light disabled, resetting");
                csd_night_light_set_active (self, FALSE);
                return;
        }

        /* schedule-mode */
        switch (g_settings_get_enum (self->settings, "night-light-schedule-mode")) {
        case NIGHT_MODE_SCHEDULE_ALWAYS_ON:
                /* just set the temperature to night light */
                temperature = g_settings_get_uint (self->settings, "night-light-temperature");
                g_debug ("night light always on, using temperature of %uK",
                         temperature);
                csd_night_light_set_active (self, TRUE);
                csd_night_light_set_temperature (self, temperature);
                return;
        case NIGHT_MODE_SCHEDULE_AUTO:
                /* sun was updated in night_mode_recheck(self) */
                schedule_to = self->cached_sunrise;
                schedule_from = self->cached_sunset;
                break;
        default:
                break;
        }

        /* fall back to manual settings in case of no calculation */
        if (schedule_to <= 0.f || schedule_from <= 0.f) {
                schedule_from = g_settings_get_double (self->settings,
                                                       "night-light-schedule-from");
                schedule_to = g_settings_get_double (self->settings,
                                                     "night-light-schedule-to");
        }

        /* get the current hour of a day as a fraction */
        frac_day = csd_night_mode_frac_day_from_dt (dt_now);
        g_debug ("fractional day = %.3f, limits = %.3f->%.3f",
                 frac_day, schedule_from, schedule_to);

        /* disabled until tomorrow - is updated in night_mode_recheck(self) */
        if (self->disabled_until_tmw) {
                csd_night_light_set_active(self, FALSE);
                return;
        }

        /* lower smearing period to be smaller than the time between start/stop */
        smear = MIN (smear,
                     MIN (     ABS (schedule_to - schedule_from),
                          24 - ABS (schedule_to - schedule_from)));

        if (!csd_night_mode_frac_day_is_between (frac_day,
                                                 schedule_from - smear,
                                                 schedule_to)) {
                g_debug ("not time for night light");
                csd_night_light_set_active (self, FALSE);
                return;
        }

        /* smear the temperature for a short duration before the set limits
         *
         *   |----------------------| = from->to
         * |-|                        = smear down
         *                        |-| = smear up
         *
         * \                        /
         *  \                      /
         *   \--------------------/
         */
        temperature = g_settings_get_uint (self->settings, "night-light-temperature");
        if (smear < 0.01) {
                /* Don't try to smear for extremely short or zero periods */
                temp_smeared = temperature;
        } else if (csd_night_mode_frac_day_is_between (frac_day,
                                                       schedule_from - smear,
                                                       schedule_from)) {
                gdouble factor = 1.f - ((frac_day - (schedule_from - smear)) / smear);
                temp_smeared = linear_interpolate (CSD_COLOR_TEMPERATURE_DEFAULT,
                                                   temperature, factor);
        } else if (csd_night_mode_frac_day_is_between (frac_day,
                                                       schedule_to - smear,
                                                       schedule_to)) {
                gdouble factor = (frac_day - (schedule_to - smear)) / smear;
                temp_smeared = linear_interpolate (CSD_COLOR_TEMPERATURE_DEFAULT,
                                                   temperature, factor);
        } else {
                temp_smeared = temperature;
        }
        g_debug ("night light mode on, using temperature of %uK (aiming for %uK)",
                 temp_smeared, temperature);
        csd_night_light_set_active (self, TRUE);
        csd_night_light_set_temperature (self, temp_smeared);
}

/* called when the time may have changed */
static gboolean
night_mode_recheck_cb (gpointer user_data)
{
        CsdNightMode *self = CSD_NIGHT_MODE (user_data);

        /* recheck parameters, then reschedule a new timeout */
        night_mode_recheck (self);
        poll_timeout_destroy (self);
        poll_timeout_create (self);

        /* return value ignored for a one-time watch */
        return G_SOURCE_REMOVE;
}

static void
poll_timeout_create (CsdNightMode *self)
{
        g_autoptr(GDateTime) dt_now = NULL;
        g_autoptr(GDateTime) dt_expiry = NULL;

        if (self->source != NULL)
                return;

        dt_now = csd_night_mode_get_date_time_now (self);
        dt_expiry = g_date_time_add_seconds (dt_now, CSD_NIGHT_MODE_POLL_TIMEOUT);
        self->source = _gnome_datetime_source_new (dt_now,
                                                   dt_expiry,
                                                   TRUE);
        g_source_set_callback (self->source,
                               night_mode_recheck_cb,
                               self, NULL);
        g_source_attach (self->source, NULL);
}

static void
poll_timeout_destroy (CsdNightMode *self)
{

        if (self->source == NULL)
                return;

        g_source_destroy (self->source);
        g_source_unref (self->source);
        self->source = NULL;
}

static void
settings_changed_cb (GSettings *settings, gchar *key, gpointer user_data)
{
        CsdNightMode *self = CSD_NIGHT_MODE (user_data);
        g_debug ("settings changed");
        night_mode_recheck (self);
}

static void
update_location_from_timezone (CsdNightMode *self)
{
    GTimeZone *tz = g_time_zone_new_local ();
    const gchar *id = g_time_zone_get_identifier (tz);

    for (int i = 0; i < G_N_ELEMENTS (tz_coord_list); i++)
    {
        const TZCoords coords = tz_coord_list[i];
        if (g_strcmp0 (coords.timezone, id) == 0)
        {
            g_debug ("Coordinates updated, timezone: %s, lat:%.3f, long:%.3f.",
                    id, coords.latitude, coords.longitude);
            g_settings_set_value (self->settings,
                                  "night-light-last-coordinates",
                                  g_variant_new ("(dd)", coords.latitude, coords.longitude));
            break;
        }
    }

    g_time_zone_unref (tz);
}

void
csd_night_mode_set_disabled_until_tmw (CsdNightMode *self, gboolean value)
{
        g_autoptr(GDateTime) dt = csd_night_mode_get_date_time_now (self);

        if (self->disabled_until_tmw == value)
                return;

        self->disabled_until_tmw = value;
        g_clear_pointer (&self->disabled_until_tmw_dt, g_date_time_unref);
        if (self->disabled_until_tmw)
                self->disabled_until_tmw_dt = g_steal_pointer (&dt);
        night_mode_recheck (self);
        g_object_notify (G_OBJECT (self), "disabled-until-tmw");
}

gboolean
csd_night_mode_get_disabled_until_tmw (CsdNightMode *self)
{
        return self->disabled_until_tmw;
}

void
csd_night_light_set_forced (CsdNightMode *self, gboolean value)
{
        if (self->light_forced == value)
                return;

        self->light_forced = value;
        g_object_notify (G_OBJECT (self), "light-forced");

        /* A simple recheck might not reset the temperature if
         * night light is currently disabled. */
        if (!self->light_forced && !self->cached_light_active) {
                csd_night_light_set_temperature (self, CSD_COLOR_TEMPERATURE_DEFAULT);
                return;
        }

        night_light_recheck (self);
}

void
csd_night_theme_set_forced (CsdNightMode *self, gboolean value)
{
        if (self->theme_forced == value)
                return;

        self->theme_forced = value;
        g_object_notify (G_OBJECT (self), "light-forced");

        /* A simple recheck might not switch off if
         * night theme is currently disabled. */
        if (!self->theme_forced && !self->cached_theme_active) {
                night_theme_switch_off (self);
                return;
        }

        night_theme_recheck (self);
}

gboolean
csd_night_light_get_forced (CsdNightMode *self)
{
        return self->light_forced;
}

gboolean
csd_night_theme_get_forced (CsdNightMode *self)
{
        return self->theme_forced;
}

gboolean
csd_night_light_get_active (CsdNightMode *self)
{
        return self->cached_light_active;
}

gboolean
csd_night_theme_get_active (CsdNightMode *self)
{
        return self->cached_theme_active;
}

gdouble
csd_night_mode_get_sunrise (CsdNightMode *self)
{
        return self->cached_sunrise;
}

gdouble
csd_night_mode_get_sunset (CsdNightMode *self)
{
        return self->cached_sunset;
}

gdouble
csd_night_light_get_temperature (CsdNightMode *self)
{
        return self->cached_temperature;
}

gboolean
csd_night_mode_start (CsdNightMode *self, GError **error)
{
        night_mode_recheck (self);
        poll_timeout_create (self);

        /* care about changes */
        g_signal_connect (self->settings, "changed",
                          G_CALLBACK (settings_changed_cb), self);

        update_location_from_timezone (self);

        return TRUE;
}

static void
csd_night_mode_finalize (GObject *object)
{
        CsdNightMode *self = CSD_NIGHT_MODE (object);

        poll_timeout_destroy (self);
        poll_smooth_destroy (self);

        g_clear_object (&self->settings);
        g_clear_object (&self->theme_settings);
        g_clear_object (&self->cinnamon_theme_settings);
        g_clear_object (&self->x_theme_settings);
        g_clear_pointer (&self->datetime_override, g_date_time_unref);
        g_clear_pointer (&self->disabled_until_tmw_dt, g_date_time_unref);

        if (self->validate_id > 0) {
                g_source_remove (self->validate_id);
                self->validate_id = 0;
        }

        G_OBJECT_CLASS (csd_night_mode_parent_class)->finalize (object);
}

static void
csd_night_mode_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
        CsdNightMode *self = CSD_NIGHT_MODE (object);

        switch (prop_id) {
        case PROP_SUNRISE:
                self->cached_sunrise = g_value_get_double (value);
                break;
        case PROP_SUNSET:
                self->cached_sunset = g_value_get_double (value);
                break;
        case PROP_TEMPERATURE:
                self->cached_temperature = g_value_get_double (value);
                break;
        case PROP_DISABLED_UNTIL_TMW:
                csd_night_mode_set_disabled_until_tmw (self, g_value_get_boolean (value));
                break;
        case PROP_LIGHT_FORCED:
                csd_night_light_set_forced (self, g_value_get_boolean (value));
                break;
        case PROP_THEME_FORCED:
                csd_night_theme_set_forced (self, g_value_get_boolean (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
csd_night_mode_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
        CsdNightMode *self = CSD_NIGHT_MODE (object);

        switch (prop_id) {
        case PROP_LIGHT_ACTIVE:
                g_value_set_boolean (value, self->cached_light_active);
                break;
        case PROP_THEME_ACTIVE:
                g_value_set_boolean (value, self->cached_theme_active);
                break;
        case PROP_SUNRISE:
                g_value_set_double (value, self->cached_sunrise);
                break;
        case PROP_SUNSET:
                g_value_set_double (value, self->cached_sunrise);
                break;
        case PROP_TEMPERATURE:
                g_value_set_double (value, self->cached_sunrise);
                break;
        case PROP_DISABLED_UNTIL_TMW:
                g_value_set_boolean (value, csd_night_mode_get_disabled_until_tmw (self));
                break;
        case PROP_LIGHT_FORCED:
                g_value_set_boolean (value, csd_night_light_get_forced (self));
                break;
        case PROP_THEME_FORCED:
                g_value_set_boolean (value, csd_night_theme_get_forced (self));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
csd_night_mode_class_init (CsdNightModeClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = csd_night_mode_finalize;

        object_class->set_property = csd_night_mode_set_property;
        object_class->get_property = csd_night_mode_get_property;

        g_object_class_install_property (object_class,
                                         PROP_LIGHT_ACTIVE,
                                         g_param_spec_boolean ("light-active",
                                                               "Light active",
                                                               "If night light functionality is active right now",
                                                               FALSE,
                                                               G_PARAM_READABLE));
        
        g_object_class_install_property (object_class,
                                         PROP_THEME_ACTIVE,
                                         g_param_spec_boolean ("theme-active",
                                                               "Theme active",
                                                               "If night theme is active right now",
                                                               FALSE,
                                                               G_PARAM_READABLE));

        g_object_class_install_property (object_class,
                                         PROP_SUNRISE,
                                         g_param_spec_double ("sunrise",
                                                              "Sunrise",
                                                              "Sunrise in fractional hours",
                                                              0,
                                                              24.f,
                                                              12,
                                                              G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_SUNSET,
                                         g_param_spec_double ("sunset",
                                                              "Sunset",
                                                              "Sunset in fractional hours",
                                                              0,
                                                              24.f,
                                                              12,
                                                              G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_TEMPERATURE,
                                         g_param_spec_double ("temperature",
                                                              "Temperature",
                                                              "Temperature in Kelvin",
                                                              CSD_COLOR_TEMPERATURE_MIN,
                                                              CSD_COLOR_TEMPERATURE_MAX,
                                                              CSD_COLOR_TEMPERATURE_DEFAULT,
                                                              G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_DISABLED_UNTIL_TMW,
                                         g_param_spec_boolean ("disabled-until-tmw",
                                                               "Disabled until tomorrow",
                                                               "If the night mode is disabled until the next day",
                                                               FALSE,
                                                               G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_LIGHT_FORCED,
                                         g_param_spec_boolean ("light-forced",
                                                               "Light forced",
                                                               "Whether night light should be forced on, useful for previewing",
                                                               FALSE,
                                                               G_PARAM_READWRITE));
        
        g_object_class_install_property (object_class,
                                         PROP_THEME_FORCED,
                                         g_param_spec_boolean ("theme-forced",
                                                               "Theme forced",
                                                               "Whether night theme should be forced on, useful for previewing",
                                                               FALSE,
                                                               G_PARAM_READWRITE));

}

static void
csd_night_mode_init (CsdNightMode *self)
{
        self->smooth_enabled = TRUE;
        self->cached_light_active_unset = TRUE;
        self->cached_theme_active_unset = TRUE;
        self->cached_sunrise = -1.f;
        self->cached_sunset = -1.f;
        self->cached_temperature = CSD_COLOR_TEMPERATURE_DEFAULT;
        self->settings = g_settings_new ("org.cinnamon.settings-daemon.plugins.color");
        self->theme_settings = g_settings_new ("org.cinnamon.desktop.interface");
        self->x_theme_settings = g_settings_new ("org.x.apps.portal");
        self->cinnamon_theme_settings = g_settings_new ("org.cinnamon.theme");
}

CsdNightMode *
csd_night_mode_new (void)
{
        return g_object_new (CSD_TYPE_NIGHT_MODE, NULL);
}

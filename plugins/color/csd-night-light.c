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

// #include <geoclue.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include "gnome-datetime-source.h"

#include "csd-color-state.h"

#include "csd-night-light.h"
#include "csd-night-light-common.h"

struct _CsdNightLight {
        GObject            parent;
        GSettings         *settings;
        gboolean           forced;
        gboolean           disabled_until_tmw;
        GDateTime         *disabled_until_tmw_dt;
        gboolean           geoclue_enabled;
        GSource           *source;
        guint              validate_id;
        // GClueClient       *geoclue_client;
        // GClueSimple       *geoclue_simple;
        GSettings         *location_settings;
        gdouble            cached_sunrise;
        gdouble            cached_sunset;
        gdouble            cached_temperature;
        gboolean           cached_active;
        gboolean           smooth_enabled;
        GTimer            *smooth_timer;
        guint              smooth_id;
        gdouble            smooth_target_temperature;
        GCancellable      *cancellable;
        GDateTime         *datetime_override;
};

enum {
        PROP_0,
        PROP_ACTIVE,
        PROP_SUNRISE,
        PROP_SUNSET,
        PROP_TEMPERATURE,
        PROP_DISABLED_UNTIL_TMW,
        PROP_FORCED,
        PROP_LAST
};

#define CSD_NIGHT_LIGHT_SCHEDULE_TIMEOUT      5       /* seconds */
#define CSD_NIGHT_LIGHT_POLL_TIMEOUT          60      /* seconds */
#define CSD_NIGHT_LIGHT_POLL_SMEAR            1       /* hours */
#define CSD_NIGHT_LIGHT_SMOOTH_SMEAR          5.f     /* seconds */

#define CSD_FRAC_DAY_MAX_DELTA                  (1.f/60.f)     /* 1 minute */
#define CSD_TEMPERATURE_MAX_DELTA               (10.f)          /* Kelvin */

#define DESKTOP_ID "gnome-color-panel"

static void poll_timeout_destroy (CsdNightLight *self);
static void poll_timeout_create (CsdNightLight *self);
static void night_light_recheck (CsdNightLight *self);

G_DEFINE_TYPE (CsdNightLight, csd_night_light, G_TYPE_OBJECT);

static GDateTime *
csd_night_light_get_date_time_now (CsdNightLight *self)
{
        if (self->datetime_override != NULL)
                return g_date_time_ref (self->datetime_override);
        return g_date_time_new_now_local ();
}

void
csd_night_light_set_date_time_now (CsdNightLight *self, GDateTime *datetime)
{
        if (self->datetime_override != NULL)
                g_date_time_unref (self->datetime_override);
        self->datetime_override = g_date_time_ref (datetime);

        night_light_recheck (self);
}

static void
poll_smooth_destroy (CsdNightLight *self)
{
        if (self->smooth_id != 0) {
                g_source_remove (self->smooth_id);
                self->smooth_id = 0;
        }
        if (self->smooth_timer != NULL)
                g_clear_pointer (&self->smooth_timer, g_timer_destroy);
}

void
csd_night_light_set_smooth_enabled (CsdNightLight *self,
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
update_cached_sunrise_sunset (CsdNightLight *self)
{
        gboolean ret = FALSE;
        gdouble latitude;
        gdouble longitude;
        gdouble sunrise;
        gdouble sunset;
        g_autoptr(GVariant) tmp = NULL;
        g_autoptr(GDateTime) dt_now = csd_night_light_get_date_time_now (self);

        /* calculate the sunrise/sunset for the location */
        tmp = g_settings_get_value (self->settings, "night-light-last-coordinates");
        g_variant_get (tmp, "(dd)", &latitude, &longitude);
        if (latitude > 90.f || latitude < -90.f)
                return FALSE;
        if (longitude > 180.f || longitude < -180.f)
                return FALSE;
        if (!csd_night_light_get_sunrise_sunset (dt_now, latitude, longitude,
                                                   &sunrise, &sunset)) {
                g_warning ("failed to get sunset/sunrise for %.3f,%.3f",
                           longitude, longitude);
                return FALSE;
        }

        /* anything changed */
        if (ABS (self->cached_sunrise - sunrise) > CSD_FRAC_DAY_MAX_DELTA) {
                self->cached_sunrise = sunrise;
                g_object_notify (G_OBJECT (self), "sunrise");
                ret = TRUE;
        }
        if (ABS (self->cached_sunset - sunset) > CSD_FRAC_DAY_MAX_DELTA) {
                self->cached_sunset = sunset;
                g_object_notify (G_OBJECT (self), "sunset");
                ret = TRUE;
        }
        return ret;
}

static void
csd_night_light_set_temperature_internal (CsdNightLight *self, gdouble temperature)
{
        if (ABS (self->cached_temperature - temperature) <= CSD_TEMPERATURE_MAX_DELTA)
                return;
        self->cached_temperature = temperature;
        g_object_notify (G_OBJECT (self), "temperature");
}

static gboolean
csd_night_light_smooth_cb (gpointer user_data)
{
        CsdNightLight *self = CSD_NIGHT_LIGHT (user_data);
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
poll_smooth_create (CsdNightLight *self, gdouble temperature)
{
        g_assert (self->smooth_id == 0);
        self->smooth_target_temperature = temperature;
        self->smooth_timer = g_timer_new ();
        self->smooth_id = g_timeout_add (50, csd_night_light_smooth_cb, self);
}

static void
csd_night_light_set_temperature (CsdNightLight *self, gdouble temperature)
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
csd_night_light_set_active (CsdNightLight *self, gboolean active)
{
        if (self->cached_active == active)
                return;
        self->cached_active = active;

        /* ensure set to unity temperature */
        if (!active)
                csd_night_light_set_temperature (self, CSD_COLOR_TEMPERATURE_DEFAULT);

        g_object_notify (G_OBJECT (self), "active");
}

static void
night_light_recheck (CsdNightLight *self)
{
        gdouble frac_day;
        gdouble schedule_from = -1.f;
        gdouble schedule_to = -1.f;
        gdouble smear = CSD_NIGHT_LIGHT_POLL_SMEAR; /* hours */
        guint temperature;
        guint temp_smeared;
        g_autoptr(GDateTime) dt_now = csd_night_light_get_date_time_now (self);

        /* Forced mode, just set the temperature to night light.
         * Proper rechecking will happen once forced mode is disabled again */
        if (self->forced) {
                temperature = g_settings_get_uint (self->settings, "night-light-temperature");
                csd_night_light_set_temperature (self, temperature);
                return;
        }

        /* enabled */
        if (!g_settings_get_boolean (self->settings, "night-light-enabled")) {
                g_debug ("night light disabled, resetting");
                csd_night_light_set_active (self, FALSE);
                return;
        }

        /* calculate the position of the sun */
        if (g_settings_get_boolean (self->settings, "night-light-schedule-automatic")) {
                update_cached_sunrise_sunset (self);
                if (self->cached_sunrise > 0.f && self->cached_sunset > 0.f) {
                        schedule_to = self->cached_sunrise;
                        schedule_from = self->cached_sunset;
                }
        }

        /* fall back to manual settings */
        if (schedule_to <= 0.f || schedule_from <= 0.f) {
                schedule_from = g_settings_get_double (self->settings,
                                                       "night-light-schedule-from");
                schedule_to = g_settings_get_double (self->settings,
                                                     "night-light-schedule-to");
        }

        /* get the current hour of a day as a fraction */
        frac_day = csd_night_light_frac_day_from_dt (dt_now);
        g_debug ("fractional day = %.3f, limits = %.3f->%.3f",
                 frac_day, schedule_from, schedule_to);

        /* disabled until tomorrow */
        if (self->disabled_until_tmw) {
                GTimeSpan time_span;
                gboolean reset = FALSE;

                time_span = g_date_time_difference (dt_now, self->disabled_until_tmw_dt);

                /* Reset if disabled until tomorrow is more than 24h ago. */
                if (time_span > (GTimeSpan) 24 * 60 * 60 * 1000000) {
                        g_debug ("night light disabled until tomorrow is older than 24h, resetting disabled until tomorrow");
                        reset = TRUE;
                } else if (time_span > 0) {
                        /* Or if a sunrise lies between the time it was disabled and now. */
                        gdouble frac_disabled;
                        frac_disabled = csd_night_light_frac_day_from_dt (self->disabled_until_tmw_dt);
                        if (frac_disabled != frac_day &&
                            csd_night_light_frac_day_is_between (schedule_to,
                                                                 frac_disabled,
                                                                 frac_day)) {
                                g_debug ("night light sun rise happened, resetting disabled until tomorrow");
                                reset = TRUE;
                        }
                }

                if (reset) {
                        self->disabled_until_tmw = FALSE;
                        g_clear_pointer(&self->disabled_until_tmw_dt, g_date_time_unref);
                        g_object_notify (G_OBJECT (self), "disabled-until-tmw");
                } else {
                        g_debug ("night light still day-disabled, resetting");
                        csd_night_light_set_temperature (self,
                                                         CSD_COLOR_TEMPERATURE_DEFAULT);
                        return;
                }
        }

        /* lower smearing period to be smaller than the time between start/stop */
        smear = MIN (smear,
                     MIN (     ABS (schedule_to - schedule_from),
                          24 - ABS (schedule_to - schedule_from)));

        if (!csd_night_light_frac_day_is_between (frac_day,
                                                  schedule_from - smear,
                                                  schedule_to)) {
                g_debug ("not time for night-light");
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
        } else if (csd_night_light_frac_day_is_between (frac_day,
                                                        schedule_from - smear,
                                                        schedule_from)) {
                gdouble factor = 1.f - ((frac_day - (schedule_from - smear)) / smear);
                temp_smeared = linear_interpolate (CSD_COLOR_TEMPERATURE_DEFAULT,
                                                   temperature, factor);
        } else if (csd_night_light_frac_day_is_between (frac_day,
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

static gboolean
night_light_recheck_schedule_cb (gpointer user_data)
{
        CsdNightLight *self = CSD_NIGHT_LIGHT (user_data);
        night_light_recheck (self);
        self->validate_id = 0;
        return G_SOURCE_REMOVE;
}

/* called when something changed */
static void
night_light_recheck_schedule (CsdNightLight *self)
{
        if (self->validate_id != 0)
                g_source_remove (self->validate_id);
        self->validate_id =
                g_timeout_add_seconds (CSD_NIGHT_LIGHT_SCHEDULE_TIMEOUT,
                                       night_light_recheck_schedule_cb,
                                       self);
}

/* called when the time may have changed */
static gboolean
night_light_recheck_cb (gpointer user_data)
{
        CsdNightLight *self = CSD_NIGHT_LIGHT (user_data);

        /* recheck parameters, then reschedule a new timeout */
        night_light_recheck (self);
        poll_timeout_destroy (self);
        poll_timeout_create (self);

        /* return value ignored for a one-time watch */
        return G_SOURCE_REMOVE;
}

static void
poll_timeout_create (CsdNightLight *self)
{
        g_autoptr(GDateTime) dt_now = NULL;
        g_autoptr(GDateTime) dt_expiry = NULL;

        if (self->source != NULL)
                return;

        dt_now = csd_night_light_get_date_time_now (self);
        dt_expiry = g_date_time_add_seconds (dt_now, CSD_NIGHT_LIGHT_POLL_TIMEOUT);
        self->source = _gnome_datetime_source_new (dt_now,
                                                   dt_expiry,
                                                   TRUE);
        g_source_set_callback (self->source,
                               night_light_recheck_cb,
                               self, NULL);
        g_source_attach (self->source, NULL);
}

static void
poll_timeout_destroy (CsdNightLight *self)
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
        CsdNightLight *self = CSD_NIGHT_LIGHT (user_data);
        g_debug ("settings changed");
        night_light_recheck (self);
}

// static void
// on_location_notify (GClueSimple *simple,
//                     GParamSpec  *pspec,
//                     gpointer     user_data)
// {
//         CsdNightLight *self = CSD_NIGHT_LIGHT (user_data);
//         GClueLocation *location;
//         gdouble latitude, longitude;

//         location = gclue_simple_get_location (simple);
//         latitude = gclue_location_get_latitude (location);
//         longitude = gclue_location_get_longitude (location);

//         g_settings_set_value (self->settings,
//                               "night-light-last-coordinates",
//                               g_variant_new ("(dd)", latitude, longitude));

//         g_debug ("got geoclue latitude %f, longitude %f", latitude, longitude);

//         /* recheck the levels if the location changed significantly */
//         if (update_cached_sunrise_sunset (self))
//                 night_light_recheck_schedule (self);
// }

// static void
// on_geoclue_simple_ready (GObject      *source_object,
//                          GAsyncResult *res,
//                          gpointer      user_data)
// {
//         CsdNightLight *self = CSD_NIGHT_LIGHT (user_data);
//         GClueSimple *geoclue_simple;
//         g_autoptr(GError) error = NULL;

//         geoclue_simple = gclue_simple_new_finish (res, &error);
//         if (geoclue_simple == NULL) {
//                 if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
//                         g_warning ("Failed to connect to GeoClue2 service: %s", error->message);
//                 return;
//         }

//         self->geoclue_simple = geoclue_simple;
//         self->geoclue_client = gclue_simple_get_client (self->geoclue_simple);
//         g_object_set (G_OBJECT (self->geoclue_client),
//                       "time-threshold", 60*60, NULL); /* 1 hour */

//         g_signal_connect (self->geoclue_simple, "notify::location",
//                           G_CALLBACK (on_location_notify), user_data);

//         on_location_notify (self->geoclue_simple, NULL, user_data);
// }

// static void
// start_geoclue (CsdNightLight *self)
// {
//         self->cancellable = g_cancellable_new ();
//         gclue_simple_new (DESKTOP_ID,
//                           GCLUE_ACCURACY_LEVEL_CITY,
//                           self->cancellable,
//                           on_geoclue_simple_ready,
//                           self);

// }

// static void
// stop_geoclue (CsdNightLight *self)
// {
//         g_cancellable_cancel (self->cancellable);
//         g_clear_object (&self->cancellable);

//         if (self->geoclue_client != NULL) {
//                 gclue_client_call_stop (self->geoclue_client, NULL, NULL, NULL);
//                 self->geoclue_client = NULL;
//         }
//         g_clear_object (&self->geoclue_simple);
// }

static void
check_location_settings (CsdNightLight *self)
{
        // if (g_settings_get_boolean (self->location_settings, "enabled") && self->geoclue_enabled)
        //         start_geoclue (self);
        // else
        //         stop_geoclue (self);
}

void
csd_night_light_set_disabled_until_tmw (CsdNightLight *self, gboolean value)
{
        g_autoptr(GDateTime) dt = csd_night_light_get_date_time_now (self);

        if (self->disabled_until_tmw == value)
                return;

        self->disabled_until_tmw = value;
        g_clear_pointer (&self->disabled_until_tmw_dt, g_date_time_unref);
        if (self->disabled_until_tmw)
                self->disabled_until_tmw_dt = g_steal_pointer (&dt);
        night_light_recheck (self);
        g_object_notify (G_OBJECT (self), "disabled-until-tmw");
}

gboolean
csd_night_light_get_disabled_until_tmw (CsdNightLight *self)
{
        return self->disabled_until_tmw;
}

void
csd_night_light_set_forced (CsdNightLight *self, gboolean value)
{
        if (self->forced == value)
                return;

        self->forced = value;
        g_object_notify (G_OBJECT (self), "forced");

        /* A simple recheck might not reset the temperature if
         * night light is currently disabled. */
        if (!self->forced && !self->cached_active)
                csd_night_light_set_temperature (self, CSD_COLOR_TEMPERATURE_DEFAULT);

        night_light_recheck (self);
}

gboolean
csd_night_light_get_forced (CsdNightLight *self)
{
        return self->forced;
}

gboolean
csd_night_light_get_active (CsdNightLight *self)
{
        return self->cached_active;
}

gdouble
csd_night_light_get_sunrise (CsdNightLight *self)
{
        return self->cached_sunrise;
}

gdouble
csd_night_light_get_sunset (CsdNightLight *self)
{
        return self->cached_sunset;
}

gdouble
csd_night_light_get_temperature (CsdNightLight *self)
{
        return self->cached_temperature;
}

void
csd_night_light_set_geoclue_enabled (CsdNightLight *self, gboolean enabled)
{
        self->geoclue_enabled = enabled;
}

gboolean
csd_night_light_start (CsdNightLight *self, GError **error)
{
        night_light_recheck (self);
        poll_timeout_create (self);

        /* care about changes */
        g_signal_connect (self->settings, "changed",
                          G_CALLBACK (settings_changed_cb), self);

        g_signal_connect_swapped (self->location_settings, "changed::enabled",
                                  G_CALLBACK (check_location_settings), self);
        check_location_settings (self);

        return TRUE;
}

static void
csd_night_light_finalize (GObject *object)
{
        CsdNightLight *self = CSD_NIGHT_LIGHT (object);

        // stop_geoclue (self);

        poll_timeout_destroy (self);
        poll_smooth_destroy (self);

        g_clear_object (&self->settings);
        g_clear_pointer (&self->datetime_override, g_date_time_unref);
        g_clear_pointer (&self->disabled_until_tmw_dt, g_date_time_unref);

        if (self->validate_id > 0) {
                g_source_remove (self->validate_id);
                self->validate_id = 0;
        }

        g_clear_object (&self->location_settings);
        G_OBJECT_CLASS (csd_night_light_parent_class)->finalize (object);
}

static void
csd_night_light_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        CsdNightLight *self = CSD_NIGHT_LIGHT (object);

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
                csd_night_light_set_disabled_until_tmw (self, g_value_get_boolean (value));
                break;
        case PROP_FORCED:
                csd_night_light_set_forced (self, g_value_get_boolean (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
csd_night_light_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        CsdNightLight *self = CSD_NIGHT_LIGHT (object);

        switch (prop_id) {
        case PROP_ACTIVE:
                g_value_set_boolean (value, self->cached_active);
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
                g_value_set_boolean (value, csd_night_light_get_disabled_until_tmw (self));
                break;
        case PROP_FORCED:
                g_value_set_boolean (value, csd_night_light_get_forced (self));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
csd_night_light_class_init (CsdNightLightClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = csd_night_light_finalize;

        object_class->set_property = csd_night_light_set_property;
        object_class->get_property = csd_night_light_get_property;

        g_object_class_install_property (object_class,
                                         PROP_ACTIVE,
                                         g_param_spec_boolean ("active",
                                                               "Active",
                                                               "If night light functionality is active right now",
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
                                                               "If the night light is disabled until the next day",
                                                               FALSE,
                                                               G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_FORCED,
                                         g_param_spec_boolean ("forced",
                                                               "Forced",
                                                               "Whether night light should be forced on, useful for previewing",
                                                               FALSE,
                                                               G_PARAM_READWRITE));

}

static void
csd_night_light_init (CsdNightLight *self)
{
        self->geoclue_enabled = TRUE;
        self->smooth_enabled = TRUE;
        self->cached_sunrise = -1.f;
        self->cached_sunset = -1.f;
        self->cached_temperature = CSD_COLOR_TEMPERATURE_DEFAULT;
        self->settings = g_settings_new ("org.cinnamon.settings-daemon.plugins.color");
        self->location_settings = g_settings_new ("org.gnome.system.location");
}

CsdNightLight *
csd_night_light_new (void)
{
        return g_object_new (CSD_TYPE_NIGHT_LIGHT, NULL);
}

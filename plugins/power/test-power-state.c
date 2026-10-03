/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#include <glib.h>

#include "csd-power-state.h"

static void
assert_peripheral_does_not_replace_battery (void)
{
        CsdPowerPercentageState state = { 0 };

        csd_power_percentage_state_consider (&state,
                                             UP_DEVICE_KIND_BATTERY,
                                             TRUE,
                                             85.0);
        csd_power_percentage_state_consider (&state,
                                             UP_DEVICE_KIND_HEADSET,
                                             TRUE,
                                             80.0);

        g_assert_true (state.valid);
        g_assert_cmpint (state.kind, ==, UP_DEVICE_KIND_BATTERY);
        g_assert_cmpuint (state.percentage, ==, 85);
}

static void
assert_ups_is_fallback (void)
{
        CsdPowerPercentageState state = { 0 };

        csd_power_percentage_state_consider (&state,
                                             UP_DEVICE_KIND_UPS,
                                             TRUE,
                                             42.0);

        g_assert_true (state.valid);
        g_assert_cmpint (state.kind, ==, UP_DEVICE_KIND_UPS);
        g_assert_cmpuint (state.percentage, ==, 42);

        csd_power_percentage_state_consider (&state,
                                             UP_DEVICE_KIND_BATTERY,
                                             TRUE,
                                             85.0);

        g_assert_true (state.valid);
        g_assert_cmpint (state.kind, ==, UP_DEVICE_KIND_BATTERY);
        g_assert_cmpuint (state.percentage, ==, 85);
}

int
main (int argc, char **argv)
{
        g_test_init (&argc, &argv, NULL);

        g_test_add_func ("/power/percentage/peripheral-does-not-replace-battery",
                         assert_peripheral_does_not_replace_battery);
        g_test_add_func ("/power/percentage/ups-fallback",
                         assert_ups_is_fallback);

        return g_test_run ();
}

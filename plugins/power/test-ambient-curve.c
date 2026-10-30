#include <glib.h>

#include "csd-ambient-curve.h"

#define SEC(s) ((gint64) ((s) * G_USEC_PER_SEC))

static const gdouble default_points[CSD_AMBIENT_CURVE_POINTS] = { 0, 5, 50, 300, 1000, 5000 };

static void
test_lookup_interpolates_between_points (void)
{
    CsdAmbientCurve c;

    csd_ambient_curve_init (&c, default_points);
    g_assert_cmpint (csd_ambient_curve_lookup (&c, 0.0), ==, 0);
    g_assert_cmpint (csd_ambient_curve_lookup (&c, 5.0), ==, 20);
    g_assert_cmpint (csd_ambient_curve_lookup (&c, 175.0), ==, 50);
    g_assert_cmpint (csd_ambient_curve_lookup (&c, 1000.0), ==, 80);
    g_assert_cmpint (csd_ambient_curve_lookup (&c, 5000.0), ==, 100);
    g_assert_cmpint (csd_ambient_curve_lookup (&c, 20000.0), ==, 100);
}

static void
assert_monotonic (const CsdAmbientCurve *c)
{
    guint i;

    for (i = 1; i < CSD_AMBIENT_CURVE_POINTS; i++)
        g_assert_cmpfloat (c->points[i], >, c->points[i - 1]);
}

static void
test_adjust_makes_current_lux_map_to_chosen_level (void)
{
    CsdAmbientCurve c;

    csd_ambient_curve_init (&c, default_points);
    csd_ambient_curve_adjust (&c, 66.0, 70);
    g_assert_cmpint (csd_ambient_curve_lookup (&c, 66.0), ==, 70);
    assert_monotonic (&c);
    g_assert_cmpint (csd_ambient_curve_lookup (&c, 5000.0), ==, 100);
}

static void
test_adjust_keeps_segment_shape (void)
{
    CsdAmbientCurve c;

    csd_ambient_curve_init (&c, default_points);
    csd_ambient_curve_adjust (&c, 175.0, 50);
    g_assert_cmpfloat_with_epsilon (c.points[2], 112.5, 1e-9);
    g_assert_cmpfloat_with_epsilon (c.points[3], 237.5, 1e-9);
    g_assert_cmpfloat (c.points[1], ==, 5.0);
    g_assert_cmpfloat (c.points[4], ==, 1000.0);
    g_assert_cmpint (csd_ambient_curve_lookup (&c, 150.0), ==, 46);
    g_assert_cmpint (csd_ambient_curve_lookup (&c, 195.0), ==, 53);
}

static void
test_adjust_to_extremes_keeps_curve_monotonic (void)
{
    CsdAmbientCurve c;

    csd_ambient_curve_init (&c, default_points);
    csd_ambient_curve_adjust (&c, 2.0, 100);
    g_assert_cmpint (csd_ambient_curve_lookup (&c, 2.0), ==, 100);
    assert_monotonic (&c);

    csd_ambient_curve_init (&c, default_points);
    csd_ambient_curve_adjust (&c, 4000.0, 0);
    g_assert_cmpint (csd_ambient_curve_lookup (&c, 4000.0), ==, 0);
    assert_monotonic (&c);
}

static void
test_first_reading_applies_immediately (void)
{
    CsdAmbientCurve c;
    gboolean wait = TRUE;

    csd_ambient_curve_init (&c, default_points);
    g_assert_cmpint (csd_ambient_curve_update (&c, 175.0, SEC (0), &wait), ==, 50);
    g_assert_false (wait);
}

static void
test_change_inside_band_does_nothing (void)
{
    CsdAmbientCurve c;
    gboolean wait;

    csd_ambient_curve_init (&c, default_points);
    csd_ambient_curve_update (&c, 100.0, SEC (0), &wait);
    g_assert_cmpint (csd_ambient_curve_update (&c, 108.0, SEC (1), &wait), ==, -1);
    g_assert_false (wait);
    g_assert_cmpint (csd_ambient_curve_update (&c, 92.0, SEC (2), &wait), ==, -1);
    g_assert_false (wait);
}

static void
test_dark_room_flicker_does_nothing (void)
{
    CsdAmbientCurve c;
    gboolean wait;
    gdouble t;

    csd_ambient_curve_init (&c, default_points);
    csd_ambient_curve_update (&c, 2.0, SEC (0), &wait);
    for (t = 0.8; t < 60.0; t += 0.8) {
        gdouble lux = ((gint) (t / 0.8)) % 2 ? 1.0 : 2.0;

        g_assert_cmpint (csd_ambient_curve_update (&c, lux, SEC (t), &wait), ==, -1);
        g_assert_false (wait);
    }
}

static void
test_change_outside_band_applies_after_settling (void)
{
    CsdAmbientCurve c;
    gboolean wait;

    csd_ambient_curve_init (&c, default_points);
    csd_ambient_curve_update (&c, 2.0, SEC (0), &wait);
    g_assert_cmpint (csd_ambient_curve_update (&c, 66.0, SEC (10), &wait), ==, -1);
    g_assert_true (wait);
    g_assert_cmpint (csd_ambient_curve_update (&c, 66.0, SEC (11), &wait), ==, -1);
    g_assert_true (wait);
    g_assert_cmpint (csd_ambient_curve_update (&c, 66.0, SEC (12.1), &wait), ==, 41);
    g_assert_false (wait);
    g_assert_cmpint (csd_ambient_curve_update (&c, 66.0, SEC (13), &wait), ==, -1);
    g_assert_cmpint (csd_ambient_curve_update (&c, 2.0, SEC (20), &wait), ==, -1);
    g_assert_true (wait);
    g_assert_cmpint (csd_ambient_curve_update (&c, 2.0, SEC (22.1), &wait), ==, 8);
}

static void
test_change_that_reverts_before_settling_is_dropped (void)
{
    CsdAmbientCurve c;
    gboolean wait;

    csd_ambient_curve_init (&c, default_points);
    csd_ambient_curve_update (&c, 100.0, SEC (0), &wait);
    g_assert_cmpint (csd_ambient_curve_update (&c, 300.0, SEC (10), &wait), ==, -1);
    g_assert_true (wait);
    g_assert_cmpint (csd_ambient_curve_update (&c, 100.0, SEC (11), &wait), ==, -1);
    g_assert_false (wait);
    g_assert_cmpint (csd_ambient_curve_update (&c, 100.0, SEC (13), &wait), ==, -1);
    g_assert_false (wait);
}

static void
test_adjust_resets_band_to_current_lux (void)
{
    CsdAmbientCurve c;
    gboolean wait;

    csd_ambient_curve_init (&c, default_points);
    csd_ambient_curve_update (&c, 2.0, SEC (0), &wait);
    csd_ambient_curve_adjust (&c, 66.0, 70);
    g_assert_cmpint (csd_ambient_curve_update (&c, 66.0, SEC (5), &wait), ==, -1);
    g_assert_false (wait);
    g_assert_cmpint (csd_ambient_curve_update (&c, 70.0, SEC (6), &wait), ==, -1);
    g_assert_false (wait);
}

static void
test_darkness_applies (void)
{
    CsdAmbientCurve c;
    gboolean wait;

    csd_ambient_curve_init (&c, default_points);
    g_assert_cmpint (csd_ambient_curve_update (&c, 300.0, SEC (0), &wait), ==, 60);
    g_assert_cmpint (csd_ambient_curve_update (&c, 0.0, SEC (10), &wait), ==, -1);
    g_assert_true (wait);
    g_assert_cmpint (csd_ambient_curve_update (&c, 0.0, SEC (12.1), &wait), ==, 0);
    g_assert_false (wait);
}

static void
test_adjust_in_darkness (void)
{
    CsdAmbientCurve c;

    csd_ambient_curve_init (&c, default_points);
    csd_ambient_curve_adjust (&c, 0.0, 70);
    g_assert_cmpint (csd_ambient_curve_lookup (&c, 0.0), ==, 70);
    assert_monotonic (&c);
}

static void
test_negative_reading_ignored (void)
{
    CsdAmbientCurve c;
    gboolean wait = TRUE;

    csd_ambient_curve_init (&c, default_points);
    g_assert_cmpint (csd_ambient_curve_update (&c, -1.0, SEC (0), &wait), ==, -1);
    g_assert_false (wait);
}

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/ambient-curve/lookup-interpolates-between-points", test_lookup_interpolates_between_points);
    g_test_add_func ("/ambient-curve/adjust-makes-current-lux-map-to-chosen-level", test_adjust_makes_current_lux_map_to_chosen_level);
    g_test_add_func ("/ambient-curve/adjust-keeps-segment-shape", test_adjust_keeps_segment_shape);
    g_test_add_func ("/ambient-curve/adjust-to-extremes-keeps-curve-monotonic", test_adjust_to_extremes_keeps_curve_monotonic);
    g_test_add_func ("/ambient-curve/first-reading-applies-immediately", test_first_reading_applies_immediately);
    g_test_add_func ("/ambient-curve/change-inside-band-does-nothing", test_change_inside_band_does_nothing);
    g_test_add_func ("/ambient-curve/dark-room-flicker-does-nothing", test_dark_room_flicker_does_nothing);
    g_test_add_func ("/ambient-curve/change-outside-band-applies-after-settling", test_change_outside_band_applies_after_settling);
    g_test_add_func ("/ambient-curve/change-that-reverts-before-settling-is-dropped", test_change_that_reverts_before_settling_is_dropped);
    g_test_add_func ("/ambient-curve/adjust-resets-band-to-current-lux", test_adjust_resets_band_to_current_lux);
    g_test_add_func ("/ambient-curve/darkness-applies", test_darkness_applies);
    g_test_add_func ("/ambient-curve/adjust-in-darkness", test_adjust_in_darkness);
    g_test_add_func ("/ambient-curve/negative-reading-ignored", test_negative_reading_ignored);

    return g_test_run ();
}

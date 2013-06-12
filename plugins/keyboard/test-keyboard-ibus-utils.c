#include "gsd-keyboard-manager.c"

static void
test_make_xkb_source_id (void)
{
        gint i;
        const gchar *test_strings[][2] = {
                /* input                output */
                { "xkb:aa:bb:cc",       "aa+bb" },
                { "xkb:aa:bb:",         "aa+bb" },
                { "xkb:aa::cc",         "aa" },
                { "xkb:aa::",           "aa" },
                { "xkb::bb:cc",         "+bb" },
                { "xkb::bb:",           "+bb" },
                { "xkb:::cc",           "" },
                { "xkb:::",             "" },
        };

        for (i = 0; i < G_N_ELEMENTS (test_strings); ++i)
                g_assert_cmpstr (make_xkb_source_id (test_strings[i][0]), ==, test_strings[i][1]);
}

static void
test_layout_from_ibus_layout (void)
{
        gint i;
        const gchar *test_strings[][2] = {
                /* input                output */
                { "",                   "" },
                { "a",                  "a" },
                { "a(",                 "a" },
                { "a[",                 "a" },
        };

        for (i = 0; i < G_N_ELEMENTS (test_strings); ++i)
                g_assert_cmpstr (layout_from_ibus_layout (test_strings[i][0]), ==, test_strings[i][1]);
}

static void
test_variant_from_ibus_layout (void)
{
        gint i;
        const gchar *test_strings[][2] = {
                /* input                output */
                { "",                   NULL },
                { "a",                  NULL },
                { "(",                  NULL },
                { "()",                 "" },
                { "(b)",                "b" },
                { "a(",                 NULL },
                { "a()",                "" },
                { "a(b)",               "b" },
        };

        for (i = 0; i < G_N_ELEMENTS (test_strings); ++i)
                g_assert_cmpstr (variant_from_ibus_layout (test_strings[i][0]), ==, test_strings[i][1]);
}

static void
test_options_from_ibus_layout (void)
{
        gint i, j;
        gchar *output_0[] = {
                NULL
        };
        gchar *output_1[] = {
                "",
                NULL
        };
        gchar *output_2[] = {
                "b",
                NULL
        };
        gchar *output_3[] = {
                "b", "",
                NULL
        };
        gchar *output_4[] = {
                "b", "c",
                NULL
        };
        const gpointer tests[][2] = {
                /* input                output */
                { "",                   NULL },
                { "a",                  NULL },
                { "a[",                 output_0 },
                { "a[]",                output_1 },
                { "a[b]",               output_2 },
                { "a[b,]",              output_3 },
                { "a[b,c]",             output_4 },
        };

        for (i = 0; i < G_N_ELEMENTS (tests); ++i) {
                if (tests[i][1] == NULL) {
                        g_assert (options_from_ibus_layout (tests[i][0]) == NULL);
                } else {
                        gchar **strv_a = options_from_ibus_layout (tests[i][0]);
                        gchar **strv_b = tests[i][1];

                        g_assert (g_strv_length (strv_a) == g_strv_length (strv_b));
                        for (j = 0; j < g_strv_length (strv_a); ++j)
                                g_assert_cmpstr (strv_a[j], ==, strv_b[j]);
                }
        }
}

int
main (void)
{
        test_make_xkb_source_id ();
        test_layout_from_ibus_layout ();
        test_variant_from_ibus_layout ();
        test_options_from_ibus_layout ();

        return 0;
}

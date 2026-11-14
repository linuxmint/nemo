#include <glib.h>
#include <libnemo-private/nemo-speed-graph-math.h>

static void
test_ceiling_steps (void)
{
    g_assert_cmpuint (nemo_speed_nice_ceiling (0), ==, 1);
    g_assert_cmpuint (nemo_speed_nice_ceiling (1), ==, 1);
    g_assert_cmpuint (nemo_speed_nice_ceiling (2), ==, 2);
    g_assert_cmpuint (nemo_speed_nice_ceiling (7), ==, 8);
    g_assert_cmpuint (nemo_speed_nice_ceiling (9), ==, 10);
    g_assert_cmpuint (nemo_speed_nice_ceiling (10), ==, 10);
    g_assert_cmpuint (nemo_speed_nice_ceiling (11), ==, 12);
    g_assert_cmpuint (nemo_speed_nice_ceiling (13), ==, 15);
    g_assert_cmpuint (nemo_speed_nice_ceiling (21), ==, 25);
    g_assert_cmpuint (nemo_speed_nice_ceiling (81), ==, 100);
    g_assert_cmpuint (nemo_speed_nice_ceiling (100), ==, 100);
    g_assert_cmpuint (nemo_speed_nice_ceiling (101), ==, 120);
    g_assert_cmpuint (nemo_speed_nice_ceiling (123456789), ==, 150000000);
}

static void
test_ceiling_properties (void)
{
    /* never below the value, never more than 34% above it (largest step is 6 -> 8) (for v >= 8 where
     * the step grid applies), and monotonic */
    guint64 v, prev = 0;

    for (v = 1; v < 5000000; v += (v < 1000 ? 1 : v / 7 + 1)) {
        guint64 c = nemo_speed_nice_ceiling (v);
        g_assert_cmpuint (c, >=, v);
        if (v >= 8) {
            g_assert_cmpuint (c * 100, <=, v * 134 + 100);
        }
        g_assert_cmpuint (c, >=, prev);
        prev = c;
    }
}

static void
test_ceiling_large (void)
{
    /* no overflow / hang near the top of the range used for speeds */
    g_assert_cmpuint (nemo_speed_nice_ceiling (G_GUINT64_CONSTANT (10000000000)), ==,
                      G_GUINT64_CONSTANT (10000000000));
    g_assert_cmpuint (nemo_speed_nice_ceiling (G_GUINT64_CONSTANT (10000000001)), ==,
                      G_GUINT64_CONSTANT (12000000000));
    g_assert_cmpuint (nemo_speed_nice_ceiling (G_MAXUINT32), >=, G_MAXUINT32);
}

static void
test_average (void)
{
    guint64 d[] = { 100, 200, 300, 400 };

    g_assert_cmpuint (nemo_speed_average (d, 0, 200), ==, 0);
    g_assert_cmpuint (nemo_speed_average (d, 4, 0), ==, 0);
    /* 1000 bytes over 4 samples of 0.2 s = 0.8 s -> 1250 B/s */
    g_assert_cmpuint (nemo_speed_average (d, 4, 200), ==, 1250);
    /* one sample of 100 bytes per 1000 ms */
    g_assert_cmpuint (nemo_speed_average (d, 1, 1000), ==, 100);
    /* a window shorter than the array only looks at `count` entries */
    g_assert_cmpuint (nemo_speed_average (d, 2, 100), ==, 1500);
}

static void
test_average_big (void)
{
    /* 64 samples of 8 MiB at 50 ms must not overflow */
    guint64 d[64];
    guint i;

    for (i = 0; i < 64; i++) {
        d[i] = 8 * 1024 * 1024;
    }
    g_assert_cmpuint (nemo_speed_average (d, 64, 50), ==, (guint64) 8 * 1024 * 1024 * 20);
}

static void
test_bucket_range_covers_history (void)
{
    guint n, columns, col;

    for (n = 1; n <= 40; n++) {
        for (columns = 1; columns <= n; columns++) {
            guint expect_from = 0;
            for (col = 0; col < columns; col++) {
                guint from, to;
                nemo_speed_bucket_range (n, columns, col, &from, &to);
                g_assert_cmpuint (to, >, from);
                g_assert_cmpuint (to, <=, n);
                /* contiguous, so every sample is used exactly once */
                g_assert_cmpuint (from, ==, expect_from);
                expect_from = to;
            }
            g_assert_cmpuint (expect_from, ==, n);
        }
    }
}

static void
test_bucket_range_huge (void)
{
    guint from, to;

    /* 32-bit overflow guard: col * n would overflow guint */
    nemo_speed_bucket_range (G_MAXUINT32 - 1, 4000, 3999, &from, &to);
    g_assert_cmpuint (to, <=, G_MAXUINT32 - 1);
    g_assert_cmpuint (to, >, from);
}

static void
test_bucket_mean (void)
{
    guint64 d[] = { 10, 20, 30, 40, 50, 60 };

    /* one column per sample */
    g_assert_cmpfloat (nemo_speed_bucket_mean (d, 6, 6, 0), ==, 10.0);
    g_assert_cmpfloat (nemo_speed_bucket_mean (d, 6, 6, 5), ==, 60.0);
    /* three columns of two samples */
    g_assert_cmpfloat (nemo_speed_bucket_mean (d, 6, 3, 0), ==, 15.0);
    g_assert_cmpfloat (nemo_speed_bucket_mean (d, 6, 3, 2), ==, 55.0);
    /* a single column is the mean of everything */
    g_assert_cmpfloat (nemo_speed_bucket_mean (d, 6, 1, 0), ==, 35.0);
    /* uneven split 6 -> 4 columns: ranges [0,1) [1,3) [3,4) [4,6) */
    g_assert_cmpfloat (nemo_speed_bucket_mean (d, 6, 4, 1), ==, 25.0);
    g_assert_cmpfloat (nemo_speed_bucket_mean (d, 6, 4, 3), ==, 55.0);
}

static void
test_overall_average (void)
{
    g_assert_cmpuint (nemo_speed_overall_average (1000, 0), ==, 0);
    g_assert_cmpuint (nemo_speed_overall_average (1000, -5), ==, 0);
    g_assert_cmpuint (nemo_speed_overall_average (0, 1000000), ==, 0);
    g_assert_cmpuint (nemo_speed_overall_average (1000, 1000000), ==, 1000);
    g_assert_cmpuint (nemo_speed_overall_average (1000, 500000), ==, 2000);
    /* 10 GiB in 100 s */
    g_assert_cmpuint (nemo_speed_overall_average ((guint64) 10 << 30, 100 * G_USEC_PER_SEC),
                      ==, ((guint64) 10 << 30) / 100);
}

#define MB ((gint64) 1000000)

/* The operation's own counter alone: coarse, but it never lies. */
static void
test_counters_done_only (void)
{
    NemoSpeedCounters c;

    nemo_speed_counters_init (&c, 500 * MB);
    g_assert_cmpint (nemo_speed_counters_step (&c, 500 * MB, -1, FALSE), ==, 0);
    g_assert_cmpint (nemo_speed_counters_step (&c, 560 * MB, -1, FALSE), ==, 60 * MB);
    g_assert_cmpint (nemo_speed_counters_step (&c, 560 * MB, -1, FALSE), ==, 0);
    g_assert_cmpint (c.total, ==, 60 * MB);
}

/* The size of the file being written refines it between the reports. */
static void
test_counters_live_size (void)
{
    NemoSpeedCounters c;

    nemo_speed_counters_init (&c, 0);
    g_assert_cmpint (nemo_speed_counters_step (&c, 0, 20 * MB, FALSE), ==, 20 * MB);
    g_assert_cmpint (nemo_speed_counters_step (&c, 0, 50 * MB, FALSE), ==, 30 * MB);
    /* the operation's counter catches up: nothing is counted twice */
    g_assert_cmpint (nemo_speed_counters_step (&c, 80 * MB, -1, FALSE), ==, 30 * MB);
    g_assert_cmpint (nemo_speed_counters_step (&c, 80 * MB, 80 * MB, FALSE), ==, 0);
    g_assert_cmpint (c.total, ==, 80 * MB);
}

/* A pause stops the counting, and what the chunk in flight still writes
 * is not turned into one huge sample on resume. */
static void
test_counters_pause_holds_back (void)
{
    NemoSpeedCounters c;

    nemo_speed_counters_init (&c, 0);
    nemo_speed_counters_step (&c, 0, 40 * MB, FALSE);

    g_assert_cmpint (nemo_speed_counters_step (&c, 40 * MB, 40 * MB, TRUE), ==, 0);
    /* the chunk in flight completes while paused */
    g_assert_cmpint (nemo_speed_counters_step (&c, 40 * MB, 1740 * MB, TRUE), ==, 0);
    g_assert_cmpint (nemo_speed_counters_step (&c, 1740 * MB, 1740 * MB, TRUE), ==, 0);

    /* resumed: only what is written from now on counts */
    g_assert_cmpint (nemo_speed_counters_step (&c, 1740 * MB, 1740 * MB, FALSE), ==, 0);
    g_assert_cmpint (nemo_speed_counters_step (&c, 1740 * MB, 1770 * MB, FALSE), ==, 30 * MB);
    g_assert_cmpint (nemo_speed_counters_step (&c, 1740 * MB, 1970 * MB, FALSE), ==, 200 * MB);
}

static void
test_counters_never_decrease (void)
{
    NemoSpeedCounters c;
    gint64 prev = 0, i;
    gint64 done[] = { 10, 5, 30, 30, 0, 50, 20 };
    gint64 live[] = { 10, 4, -1, 40, 40, 38, 90 };

    nemo_speed_counters_init (&c, 0);
    for (i = 0; i < 7; i++) {
        gint64 d = nemo_speed_counters_step (&c, done[i] * MB, live[i] < 0 ? -1 : live[i] * MB, i % 3 == 2);
        g_assert_cmpint (d, >=, 0);
        g_assert_cmpint (c.total, >=, prev);
        prev = c.total;
    }
}

int
main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/speed-graph/ceiling/steps", test_ceiling_steps);
    g_test_add_func ("/speed-graph/ceiling/properties", test_ceiling_properties);
    g_test_add_func ("/speed-graph/ceiling/large", test_ceiling_large);
    g_test_add_func ("/speed-graph/average/basic", test_average);
    g_test_add_func ("/speed-graph/average/no-overflow", test_average_big);
    g_test_add_func ("/speed-graph/bucket/range-covers-history", test_bucket_range_covers_history);
    g_test_add_func ("/speed-graph/bucket/range-huge", test_bucket_range_huge);
    g_test_add_func ("/speed-graph/bucket/mean", test_bucket_mean);
    g_test_add_func ("/speed-graph/overall-average", test_overall_average);
    g_test_add_func ("/speed-graph/counters/done-only", test_counters_done_only);
    g_test_add_func ("/speed-graph/counters/live-size", test_counters_live_size);
    g_test_add_func ("/speed-graph/counters/pause-holds-back", test_counters_pause_holds_back);
    g_test_add_func ("/speed-graph/counters/never-decrease", test_counters_never_decrease);
    return g_test_run ();
}

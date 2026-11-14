/*
 * nemo-speed-graph-math.c: pure helpers behind the transfer speed graph.
 */
#include <config.h>
#include "nemo-speed-graph-math.h"

guint64
nemo_speed_nice_ceiling (guint64 v)
{
    static const double steps[] = { 1.0, 1.2, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 8.0, 10.0 };
    double p = 1.0;
    guint i;

    while (p * 10.0 <= (double) v) {
        p *= 10.0;
    }
    for (i = 0; i < G_N_ELEMENTS (steps); i++) {
        if (steps[i] * p >= (double) v) {
            return (guint64) (steps[i] * p);
        }
    }
    return (guint64) (10.0 * p);
}

guint64
nemo_speed_average (const guint64 *deltas, guint count, guint interval_ms)
{
    guint64 sum = 0;
    guint i;

    if (count == 0 || interval_ms == 0) {
        return 0;
    }
    for (i = 0; i < count; i++) {
        sum += deltas[i];
    }
    return sum * 1000 / ((guint64) count * interval_ms);
}

void
nemo_speed_bucket_range (guint n, guint columns, guint col, guint *from, guint *to)
{
    *from = (guint) ((guint64) col * n / columns);
    *to = (guint) ((guint64) (col + 1) * n / columns);
    if (*to <= *from) {
        *to = *from + 1;
    }
    if (*to > n) {
        *to = n;
    }
}

double
nemo_speed_bucket_mean (const guint64 *data, guint n, guint columns, guint col)
{
    guint from, to, i;
    guint64 sum = 0;

    nemo_speed_bucket_range (n, columns, col, &from, &to);
    for (i = from; i < to; i++) {
        sum += data[i];
    }
    return (double) sum / (double) (to - from);
}

guint64
nemo_speed_overall_average (guint64 total_bytes, gint64 active_usec)
{
    if (active_usec <= 0) {
        return 0;
    }
    return (guint64) ((double) total_bytes / ((double) active_usec / 1e6));
}

void
nemo_speed_counters_init (NemoSpeedCounters *c, gint64 done_raw)
{
    c->done_base = done_raw;
    c->last_done = 0;
    c->total = 0;
}

gint64
nemo_speed_counters_step (NemoSpeedCounters *c, gint64 done_raw,
                          gint64 live_raw, gboolean paused)
{
    gint64 delta;

    c->last_done = MAX (done_raw - c->done_base, c->last_done);
    if (live_raw >= 0) {
        c->last_done = MAX (live_raw - c->done_base, c->last_done);
    }

    delta = MAX (c->last_done - c->total, 0);
    c->total += delta;
    return paused ? 0 : delta;
}

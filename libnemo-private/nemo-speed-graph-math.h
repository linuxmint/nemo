/*
 * nemo-speed-graph-math.h: pure helpers behind the transfer speed graph
 * (no GTK, so they can be unit tested).
 */
#ifndef NEMO_SPEED_GRAPH_MATH_H
#define NEMO_SPEED_GRAPH_MATH_H

#include <glib.h>

G_BEGIN_DECLS

/* Round up to a "round" value (1, 1.2, 1.5, 2, 2.5, 3, 4, 5, 6, 8 times a
 * power of ten), so the y-axis only changes in steps. 0 gives 1. */
guint64 nemo_speed_nice_ceiling (guint64 v);

/* Bytes/sec from the per-sample byte deltas of the last `count` samples taken
 * every `interval_ms`. Returns 0 for empty input or a zero interval. */
guint64 nemo_speed_average (const guint64 *deltas, guint count, guint interval_ms);

/* Range [*from, *to) of history samples (n in total) that make up column
 * `col` of `columns`. Never empty, never past n. Requires col < columns <= n. */
void    nemo_speed_bucket_range (guint n, guint columns, guint col,
                                 guint *from, guint *to);

/* Mean of the samples in a bucket. */
double  nemo_speed_bucket_mean (const guint64 *data, guint n, guint columns, guint col);

/* Average speed over the transferred bytes and the active (unpaused) time in
 * microseconds. Returns 0 if no time has passed. */
guint64 nemo_speed_overall_average (guint64 total_bytes, gint64 active_usec);

/* Byte accounting of one operation. Two sources follow the same bytes: the
 * operation's own counter (coarse, but works everywhere) and base + size of
 * the file being written (smooth, per job). */
typedef struct {
    gint64 done_base;   /* operation counter when we started looking */
    gint64 last_done;
    gint64 total;       /* bytes transferred so far, never decreases */
} NemoSpeedCounters;

void    nemo_speed_counters_init (NemoSpeedCounters *c, gint64 done_raw);

/* One sampling step; returns the bytes transferred since the previous step.
 * live_raw is -1 when unavailable. While paused nothing is counted: bytes
 * the chunk in flight still writes only move the baseline. */
gint64  nemo_speed_counters_step (NemoSpeedCounters *c, gint64 done_raw,
                                  gint64 live_raw, gboolean paused);

G_END_DECLS

#endif

/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 * nemo-malloc-utils.c - allocator tuning and heap-trim debounce.
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 */

#include <config.h>

#include "nemo-malloc-utils.h"

#if HAVE_MALLOC_H
#include <malloc.h>
#endif

void
nemo_malloc_setup (void)
{
#if HAVE_MALLOPT
    /* Nemo mixes lots of small/medium allocations (file lists, icons,
     * metadata) with occasional large ones (desktop background, big
     * directory loads). Left to its defaults, glibc raises its dynamic
     * mmap threshold as it sees larger allocations, after which freed
     * temporaries stay on the heap instead of being returned to the OS
     * - memory usage grows and never comes back down. Pin the allocator
     * to make that return-to-OS behavior predictable:
     *   - M_MMAP_THRESHOLD: force allocations >=128K to mmap (so they
     *     get munmap'd on free instead of fragmenting the heap).
     *   - M_ARENA_MAX: limit per-thread arenas; each arena fragments
     *     independently and rarely shrinks.
     *   - M_TRIM_THRESHOLD: pin the trim threshold so the heap shrinks
     *     more eagerly than glibc's dynamic default.
     */
    mallopt (M_MMAP_THRESHOLD, 128 * 1024);
    mallopt (M_ARENA_MAX, 4);
    mallopt (M_TRIM_THRESHOLD, 128 * 1024);
#endif
}

/* heap_trim_timeout_id: main thread only - no locking */
static guint heap_trim_timeout_id = 0;

static gboolean
heap_trim_timeout_cb (gpointer user_data)
{
    heap_trim_timeout_id = 0;

#if HAVE_MALLOC_H
    malloc_trim (0);
#endif

    return G_SOURCE_REMOVE;
}

void
nemo_schedule_heap_trim (void)
{
#if HAVE_MALLOC_H
    if (heap_trim_timeout_id != 0) {
        return;
    }

    heap_trim_timeout_id = g_timeout_add_seconds_full (G_PRIORITY_LOW,
                                                       3,
                                                       heap_trim_timeout_cb,
                                                       NULL,
                                                       NULL);
#endif
}

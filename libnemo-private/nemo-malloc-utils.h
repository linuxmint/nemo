/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 * nemo-malloc-utils.h - allocator tuning and heap-trim debounce.
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

#ifndef NEMO_MALLOC_UTILS_H
#define NEMO_MALLOC_UTILS_H

#include <glib.h>

G_BEGIN_DECLS

void nemo_malloc_setup        (void);
void nemo_schedule_heap_trim  (void);

G_END_DECLS

#endif /* NEMO_MALLOC_UTILS_H */

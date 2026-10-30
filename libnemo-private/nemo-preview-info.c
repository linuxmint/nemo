/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
 * nemo-preview-info.c - Probe results shown in the preview pane's details
 *
 * Copyright (C) 2026 Linux Mint
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
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#include <config.h>
#include "nemo-preview-info.h"

void
nemo_preview_info_free (NemoPreviewInfo *info)
{
    gint i;

    if (info == NULL) {
        return;
    }

    for (i = 0; i < NEMO_PREVIEW_N_ROWS; i++) {
        g_free (info->rows[i]);
    }

    g_clear_object (&info->cover);
    g_clear_pointer (&info->cover_bytes, g_bytes_unref);
    g_free (info);
}

gchar *
nemo_preview_format_dimensions (gint width,
                                gint height)
{
    return g_strdup_printf ("%d × %d", width, height);
}

gchar *
nemo_preview_format_time (gint64 nanoseconds)
{
    gint64 seconds = MAX (nanoseconds, 0) / (G_TIME_SPAN_SECOND * 1000);
    gint hours = seconds / 3600;
    gint minutes = (seconds % 3600) / 60;
    gint secs = seconds % 60;

    if (hours > 0) {
        return g_strdup_printf ("%d:%02d:%02d", hours, minutes, secs);
    }

    return g_strdup_printf ("%d:%02d", minutes, secs);
}

/* Joins and frees a GPtrArray of strings, NULL when it is empty */
gchar *
nemo_preview_join_parts (GPtrArray *parts)
{
    gchar *joined = NULL;

    if (parts->len > 0) {
        g_ptr_array_add (parts, NULL);
        joined = g_strjoinv (" · ", (gchar **) parts->pdata);
    }

    g_ptr_array_unref (parts);

    return joined;
}

/* Strips value in place, NULL when nothing is left */
gchar *
nemo_preview_take_stripped (gchar *value)
{
    if (value != NULL && *g_strstrip (value) == '\0') {
        g_clear_pointer (&value, g_free);
    }

    return value;
}

/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
 * nemo-preview-info.h - Probe results shown in the preview pane's details
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

#ifndef NEMO_PREVIEW_INFO_H
#define NEMO_PREVIEW_INFO_H

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/* Type-specific rows, shown in this order between Type and Modified */
typedef enum {
    NEMO_PREVIEW_ROW_TITLE,
    NEMO_PREVIEW_ROW_ARTIST,
    NEMO_PREVIEW_ROW_ALBUM,
    NEMO_PREVIEW_ROW_YEAR,
    NEMO_PREVIEW_ROW_TRACK,
    NEMO_PREVIEW_ROW_GENRE,
    NEMO_PREVIEW_ROW_DURATION,
    NEMO_PREVIEW_ROW_DIMENSIONS,
    NEMO_PREVIEW_ROW_FRAME_RATE,
    NEMO_PREVIEW_ROW_VIDEO_CODEC,
    NEMO_PREVIEW_ROW_AUDIO,
    NEMO_PREVIEW_ROW_FORMAT,
    NEMO_PREVIEW_ROW_MISSING_CODECS,
    NEMO_PREVIEW_ROW_DATE_TAKEN,
    NEMO_PREVIEW_ROW_CAMERA,
    NEMO_PREVIEW_ROW_EXPOSURE,
    NEMO_PREVIEW_N_ROWS
} NemoPreviewRow;

typedef struct {
    gchar *rows[NEMO_PREVIEW_N_ROWS];   /* NULL rows are hidden */
    GdkPixbuf *cover;                   /* a grabbed video frame, media only */
    GBytes *cover_bytes;                /* encoded embedded art, media only, unset when cover is set */
    gint64 duration;                    /* nanoseconds, 0 when unknown, media only */
} NemoPreviewInfo;

void   nemo_preview_info_free         (NemoPreviewInfo *info);

gchar *nemo_preview_format_dimensions (gint       width,
                                       gint       height);
gchar *nemo_preview_format_time       (gint64     nanoseconds);
gchar *nemo_preview_join_parts        (GPtrArray *parts);   /* frees parts */
gchar *nemo_preview_take_stripped     (gchar     *value);   /* frees value if blank */

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NemoPreviewInfo, nemo_preview_info_free)

#endif /* NEMO_PREVIEW_INFO_H */

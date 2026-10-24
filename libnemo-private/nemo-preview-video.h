/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
 * nemo-preview-video.h - Video playback widget for the preview pane
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

#ifndef NEMO_PREVIEW_VIDEO_H
#define NEMO_PREVIEW_VIDEO_H

#include <gtk/gtk.h>
#include "nemo-file.h"

#define NEMO_TYPE_PREVIEW_VIDEO (nemo_preview_video_get_type ())

G_DECLARE_FINAL_TYPE (NemoPreviewVideo, nemo_preview_video, NEMO, PREVIEW_VIDEO, GtkBox)

gboolean   nemo_preview_video_available   (void);
GtkWidget *nemo_preview_video_new         (void);
void       nemo_preview_video_set_file    (NemoPreviewVideo *video,
                                           NemoFile         *file);
void       nemo_preview_video_set_cover   (NemoPreviewVideo *video,
                                           GdkPixbuf        *cover);
void       nemo_preview_video_clear       (NemoPreviewVideo *video);
gboolean   nemo_preview_video_toggle_playback (NemoPreviewVideo *video);
gchar     *nemo_preview_video_format_time (gint64            nanoseconds);

#endif /* NEMO_PREVIEW_VIDEO_H */

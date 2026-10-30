/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
 * nemo-preview-media-info.h - Background probe of media files for the preview pane
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

#ifndef NEMO_PREVIEW_MEDIA_INFO_H
#define NEMO_PREVIEW_MEDIA_INFO_H

#include <gio/gio.h>
#include "nemo-file.h"
#include "nemo-preview-info.h"

gboolean         nemo_preview_media_info_available    (void);
gboolean         nemo_preview_media_info_supported    (NemoFile            *file);
void             nemo_preview_media_info_fetch        (NemoFile            *file,
                                                       GCancellable        *cancellable,
                                                       GAsyncReadyCallback  callback,
                                                       gpointer             user_data);
NemoPreviewInfo *nemo_preview_media_info_fetch_finish (GAsyncResult        *result,
                                                       GError             **error);

#endif /* NEMO_PREVIEW_MEDIA_INFO_H */

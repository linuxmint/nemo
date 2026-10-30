/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
 * nemo-preview-image.h - Widget for displaying image preview in preview pane
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

#ifndef NEMO_PREVIEW_IMAGE_H
#define NEMO_PREVIEW_IMAGE_H

#include <gtk/gtk.h>
#include "nemo-file.h"
#include "nemo-preview-info.h"

#define NEMO_TYPE_PREVIEW_IMAGE (nemo_preview_image_get_type())

G_DECLARE_FINAL_TYPE (NemoPreviewImage, nemo_preview_image, NEMO, PREVIEW_IMAGE, GtkBox)
GtkWidget *nemo_preview_image_new      (void);

void       nemo_preview_image_set_file (NemoPreviewImage *widget,
                                        NemoFile         *file);
void       nemo_preview_image_set_pixbuf (NemoPreviewImage *widget,
                                          GdkPixbuf        *pixbuf);
void       nemo_preview_image_set_bytes  (NemoPreviewImage *widget,
                                          GBytes           *bytes);
void       nemo_preview_image_set_cover  (NemoPreviewImage      *widget,
                                          const NemoPreviewInfo *info);
void       nemo_preview_image_refresh  (NemoPreviewImage *widget,
                                        gboolean          content_changed);
gboolean   nemo_preview_image_get_image_size (NemoPreviewImage *widget,
                                              gint             *width,
                                              gint             *height);

#endif /* NEMO_PREVIEW_IMAGE_H */

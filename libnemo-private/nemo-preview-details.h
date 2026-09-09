/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-preview-details.h - Widget for displaying file details in preview pane
 *
 * Copyright (C) 2025 Linux Mint
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

#ifndef NEMO_PREVIEW_DETAILS_H
#define NEMO_PREVIEW_DETAILS_H

#include <gtk/gtk.h>
#include "nemo-file.h"

#define NEMO_TYPE_PREVIEW_DETAILS (nemo_preview_details_get_type())

G_DECLARE_FINAL_TYPE (NemoPreviewDetails, nemo_preview_details, NEMO, PREVIEW_DETAILS, GtkBox)
GtkWidget *nemo_preview_details_new      (void);

void       nemo_preview_details_set_file (NemoPreviewDetails *widget,
                                          NemoFile           *file);
void       nemo_preview_details_clear    (NemoPreviewDetails *widget);

#endif /* NEMO_PREVIEW_DETAILS_H */

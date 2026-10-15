/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-preview-pane.h - Container widget for preview pane
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

#ifndef NEMO_PREVIEW_PANE_H
#define NEMO_PREVIEW_PANE_H

#include <gtk/gtk.h>
#include <libnemo-private/nemo-file.h>

/* Forward declaration */
typedef struct NemoWindow NemoWindow;

#define NEMO_TYPE_PREVIEW_PANE (nemo_preview_pane_get_type())

G_DECLARE_FINAL_TYPE (NemoPreviewPane, nemo_preview_pane, NEMO, PREVIEW_PANE, GtkBox);
GtkWidget *nemo_preview_pane_new      (NemoWindow *window);

void       nemo_preview_pane_set_file (NemoPreviewPane *pane,
                                       NemoFile        *file);
void       nemo_preview_pane_clear    (NemoPreviewPane *pane);

#endif /* NEMO_PREVIEW_PANE_H */

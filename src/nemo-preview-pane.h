/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nemo
 *
 *  Copyright (C) 2025 The Nemo contributors
 *
 *  Nemo is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Nemo is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500,
 *  MA 02110-1335, USA.
 */

#ifndef NEMO_PREVIEW_PANE_H
#define NEMO_PREVIEW_PANE_H

#include <gtk/gtk.h>
#include <libnemo-private/nemo-file.h>

G_BEGIN_DECLS

#define NEMO_TYPE_PREVIEW_PANE (nemo_preview_pane_get_type ())

G_DECLARE_FINAL_TYPE (NemoPreviewPane, nemo_preview_pane,
		      NEMO, PREVIEW_PANE, GtkBox)

GtkWidget *nemo_preview_pane_new             (void);
void       nemo_preview_pane_set_file        (NemoPreviewPane *self,
					      NemoFile        *file);
void       nemo_preview_pane_clear           (NemoPreviewPane *self);

G_END_DECLS

#endif /* NEMO_PREVIEW_PANE_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-canvas-dnd.h - Drag & drop handling for the canvas container widget.

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Authors: Ettore Perazzoli <ettore@gnu.org>,
            Darin Adler <darin@bentspoon.com>,
	    Andy Hertzfeld <andy@eazel.com>
*/

#ifndef NEMO_CANVAS_DND_H
#define NEMO_CANVAS_DND_H

#include <libnemo-private/nemo-canvas-container.h>
#include <libnemo-private/nemo-dnd.h>

/* DnD-related information. */
typedef struct {
	/* inherited drag info context */
	NemoDragInfo drag_info;

	gboolean highlighted;
	char *target_uri;

	/* Shadow for the icons being dragged.  */
	EelCanvasItem *shadow;
	guint hover_id;
} NemoCanvasDndInfo;


void   nemo_canvas_dnd_init                  (NemoCanvasContainer *container);
void   nemo_canvas_dnd_fini                  (NemoCanvasContainer *container);
void   nemo_canvas_dnd_begin_drag            (NemoCanvasContainer *container,
						  GdkDragAction          actions,
						  gint                   button,
						  GdkEventMotion        *event,
						  int                    start_x,
						  int                    start_y);
void   nemo_canvas_dnd_end_drag              (NemoCanvasContainer *container);

#endif /* NEMO_CANVAS_DND_H */

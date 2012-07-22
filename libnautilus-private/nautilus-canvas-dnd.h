/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-canvas-dnd.h - Drag & drop handling for the canvas container widget.

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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ettore Perazzoli <ettore@gnu.org>,
            Darin Adler <darin@bentspoon.com>,
	    Andy Hertzfeld <andy@eazel.com>
*/

#ifndef NAUTILUS_CANVAS_DND_H
#define NAUTILUS_CANVAS_DND_H

#include <libnautilus-private/nautilus-canvas-container.h>
#include <libnautilus-private/nautilus-dnd.h>

/* DnD-related information. */
typedef struct {
	/* inherited drag info context */
	NautilusDragInfo drag_info;

	gboolean highlighted;
	
	/* Shadow for the icons being dragged.  */
	EelCanvasItem *shadow;
} NautilusCanvasDndInfo;


void   nautilus_canvas_dnd_init                  (NautilusCanvasContainer *container);
void   nautilus_canvas_dnd_fini                  (NautilusCanvasContainer *container);
void   nautilus_canvas_dnd_begin_drag            (NautilusCanvasContainer *container,
						  GdkDragAction          actions,
						  gint                   button,
						  GdkEventMotion        *event,
						  int                    start_x,
						  int                    start_y);
void   nautilus_canvas_dnd_end_drag              (NautilusCanvasContainer *container);

#endif /* NAUTILUS_CANVAS_DND_H */

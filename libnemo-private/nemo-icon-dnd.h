/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-icon-dnd.h - Drag & drop handling for the icon container widget.

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
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Ettore Perazzoli <ettore@gnu.org>,
            Darin Adler <darin@bentspoon.com>,
	    Andy Hertzfeld <andy@eazel.com>
*/

#ifndef NEMO_ICON_DND_H
#define NEMO_ICON_DND_H

#include <libnemo-private/nemo-icon-container.h>
#include <libnemo-private/nemo-dnd.h>

/* DnD-related information. */
typedef struct {
	/* inherited drag info context */
	NemoDragInfo drag_info;

	gboolean highlighted;
	
	/* Shadow for the icons being dragged.  */
	EelCanvasItem *shadow;
} NemoIconDndInfo;


void   nemo_icon_dnd_init                  (NemoIconContainer *container);
void   nemo_icon_dnd_fini                  (NemoIconContainer *container);
void   nemo_icon_dnd_begin_drag            (NemoIconContainer *container,
						GdkDragAction          actions,
						gint                   button,
						GdkEventMotion        *event,
						int                    start_x,
						int                    start_y);
void   nemo_icon_dnd_end_drag              (NemoIconContainer *container);

#endif /* NEMO_ICON_DND_H */

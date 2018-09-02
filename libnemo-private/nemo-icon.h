/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-icon-container-private.h

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

   Author: Ettore Perazzoli <ettore@gnu.org>
*/

#ifndef NEMO_ICON_H
#define NEMO_ICON_H

#include <eel/eel-glib-extensions.h>
#include <libnemo-private/nemo-icon-canvas-item.h>
#include <libnemo-private/nemo-icon-container.h>

#define NEMO_ICON_CONTAINER_ICON_DATA(pointer) \
    ((NemoIconData *) (pointer))

typedef struct NemoIconData NemoIconData;

typedef void (* NemoIconCallback) (NemoIconData *icon_data,
                       gpointer callback_data);

/* An Icon. */

typedef struct {
	/* Object represented by this icon. */
	NemoIconData *data;

	/* Canvas item for the icon. */
	NemoIconCanvasItem *item;

	/* X/Y coordinates. */
	double x, y;

	/*
	 * In RTL mode x is RTL x position, we use saved_ltr_x for
	 * keeping track of x value before it gets converted into
	 * RTL value, this is used for saving the icon position 
	 * to the nemo metafile. 
	 */
	 double saved_ltr_x;
	
	/* Scale factor (stretches icon). */
	double scale;

	/* Whether this item is selected. */
	eel_boolean_bit is_selected : 1;

	/* Whether this item was selected before rubberbanding. */
	eel_boolean_bit was_selected_before_rubberband : 1;

	/* Whether this item is visible in the view. */
	eel_boolean_bit is_visible : 1;

	eel_boolean_bit has_lazy_position : 1;
} NemoIcon;

#endif /* NEMO_ICON_CONTAINER_PRIVATE_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-canvas-view.h - interface for canvas view of directory.
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 *
 */

#ifndef NAUTILUS_CANVAS_VIEW_H
#define NAUTILUS_CANVAS_VIEW_H

#include "nautilus-view.h"
#include "libnautilus-private/nautilus-canvas-container.h"

typedef struct NautilusCanvasView NautilusCanvasView;
typedef struct NautilusCanvasViewClass NautilusCanvasViewClass;

#define NAUTILUS_TYPE_CANVAS_VIEW nautilus_canvas_view_get_type()
#define NAUTILUS_CANVAS_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_CANVAS_VIEW, NautilusCanvasView))
#define NAUTILUS_CANVAS_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CANVAS_VIEW, NautilusCanvasViewClass))
#define NAUTILUS_IS_CANVAS_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_CANVAS_VIEW))
#define NAUTILUS_IS_CANVAS_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_CANVAS_VIEW))
#define NAUTILUS_CANVAS_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_CANVAS_VIEW, NautilusCanvasViewClass))

#define NAUTILUS_CANVAS_VIEW_ID "OAFIID:Nautilus_File_Manager_Canvas_View"

typedef struct NautilusCanvasViewDetails NautilusCanvasViewDetails;

struct NautilusCanvasView {
	NautilusView parent;
	NautilusCanvasViewDetails *details;
};

struct NautilusCanvasViewClass {
	NautilusViewClass parent_class;
};

/* GObject support */
GType   nautilus_canvas_view_get_type      (void);
int     nautilus_canvas_view_compare_files (NautilusCanvasView   *canvas_view,
					  NautilusFile *a,
					  NautilusFile *b);
void    nautilus_canvas_view_filter_by_screen (NautilusCanvasView *canvas_view,
					     gboolean filter);
void    nautilus_canvas_view_clean_up_by_name (NautilusCanvasView *canvas_view);

void    nautilus_canvas_view_register         (void);

NautilusCanvasContainer * nautilus_canvas_view_get_canvas_container (NautilusCanvasView *view);

#endif /* NAUTILUS_CANVAS_VIEW_H */

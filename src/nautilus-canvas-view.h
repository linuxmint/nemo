/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-canvas-view.h - interface for canvas view of directory.
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
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 *
 */

#ifndef NEMO_CANVAS_VIEW_H
#define NEMO_CANVAS_VIEW_H

#include "nemo-view.h"
#include "libnemo-private/nemo-canvas-container.h"

typedef struct NemoCanvasView NemoCanvasView;
typedef struct NemoCanvasViewClass NemoCanvasViewClass;

#define NEMO_TYPE_CANVAS_VIEW nemo_canvas_view_get_type()
#define NEMO_CANVAS_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_CANVAS_VIEW, NemoCanvasView))
#define NEMO_CANVAS_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_CANVAS_VIEW, NemoCanvasViewClass))
#define NEMO_IS_CANVAS_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_CANVAS_VIEW))
#define NEMO_IS_CANVAS_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_CANVAS_VIEW))
#define NEMO_CANVAS_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_CANVAS_VIEW, NemoCanvasViewClass))

#define NEMO_CANVAS_VIEW_ID "OAFIID:Nemo_File_Manager_Canvas_View"
#define NEMO_COMPACT_VIEW_ID "OAFIID:Nemo_File_Manager_Compact_View"

typedef struct NemoCanvasViewDetails NemoCanvasViewDetails;

struct NemoCanvasView {
	NemoView parent;
	NemoCanvasViewDetails *details;
};

struct NemoCanvasViewClass {
	NemoViewClass parent_class;
};

/* GObject support */
GType   nemo_canvas_view_get_type      (void);
int     nemo_canvas_view_compare_files (NemoCanvasView   *canvas_view,
					  NemoFile *a,
					  NemoFile *b);
void    nemo_canvas_view_filter_by_screen (NemoCanvasView *canvas_view,
					     gboolean filter);
gboolean nemo_canvas_view_is_compact   (NemoCanvasView *icon_view);

void    nemo_canvas_view_register         (void);
void    nemo_canvas_view_compact_register (void);

NemoCanvasContainer * nemo_canvas_view_get_canvas_container (NemoCanvasView *view);

#endif /* NEMO_CANVAS_VIEW_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-icon-view.h - interface for icon view of directory.

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

   Authors: Mike Engber <engber@eazel.com>
*/

#ifndef NAUTILUS_DESKTOP_CANVAS_VIEW_H
#define NAUTILUS_DESKTOP_CANVAS_VIEW_H

#include "nautilus-canvas-view.h"

#define NAUTILUS_TYPE_DESKTOP_CANVAS_VIEW nautilus_desktop_canvas_view_get_type()
#define NAUTILUS_DESKTOP_CANVAS_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_DESKTOP_CANVAS_VIEW, NautilusDesktopCanvasView))
#define NAUTILUS_DESKTOP_CANVAS_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DESKTOP_CANVAS_VIEW, NautilusDesktopCanvasViewClass))
#define NAUTILUS_IS_DESKTOP_CANVAS_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_DESKTOP_CANVAS_VIEW))
#define NAUTILUS_IS_DESKTOP_CANVAS_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DESKTOP_CANVAS_VIEW))
#define NAUTILUS_DESKTOP_CANVAS_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_DESKTOP_CANVAS_VIEW, NautilusDesktopCanvasViewClass))

typedef struct NautilusDesktopCanvasViewDetails NautilusDesktopCanvasViewDetails;
typedef struct {
	NautilusCanvasView parent;
	NautilusDesktopCanvasViewDetails *details;
} NautilusDesktopCanvasView;

typedef struct {
	NautilusCanvasViewClass parent_class;
} NautilusDesktopCanvasViewClass;

/* GObject support */
GType   nautilus_desktop_canvas_view_get_type (void);
NautilusView * nautilus_desktop_canvas_view_new (NautilusWindowSlot *slot);

#endif /* NAUTILUS_DESKTOP_CANVAS_VIEW_H */

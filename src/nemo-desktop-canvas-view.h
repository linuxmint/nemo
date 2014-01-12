/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-desktop-canvas-view.h - interface for icon view of directory.

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

   Authors: Mike Engber <engber@eazel.com>
*/

#ifndef NEMO_DESKTOP_CANVAS_VIEW_H
#define NEMO_DESKTOP_CANVAS_VIEW_H

#include "nemo-canvas-view.h"

#define NEMO_TYPE_DESKTOP_CANVAS_VIEW nemo_desktop_canvas_view_get_type()
#define NEMO_DESKTOP_CANVAS_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_DESKTOP_CANVAS_VIEW, NemoDesktopCanvasView))
#define NEMO_DESKTOP_CANVAS_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_DESKTOP_CANVAS_VIEW, NemoDesktopCanvasViewClass))
#define NEMO_IS_DESKTOP_CANVAS_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_DESKTOP_CANVAS_VIEW))
#define NEMO_IS_DESKTOP_CANVAS_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_DESKTOP_CANVAS_VIEW))
#define NEMO_DESKTOP_CANVAS_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_DESKTOP_CANVAS_VIEW, NemoDesktopCanvasViewClass))

#define NEMO_DESKTOP_CANVAS_VIEW_ID "OAFIID:Nemo_File_Manager_Desktop_Canvas_View"

typedef struct NemoDesktopCanvasViewDetails NemoDesktopCanvasViewDetails;
typedef struct {
	NemoCanvasView parent;
	NemoDesktopCanvasViewDetails *details;
} NemoDesktopCanvasView;

typedef struct {
	NemoCanvasViewClass parent_class;
} NemoDesktopCanvasViewClass;

/* GObject support */
GType   nemo_desktop_canvas_view_get_type (void);
void nemo_desktop_canvas_view_register (void);

#endif /* NEMO_DESKTOP_CANVAS_VIEW_H */

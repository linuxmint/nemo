/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-cell-renderer-text-ellipsized.c: Cell renderer for text which
   will use pango ellipsization but deactivate it temporarily for the size
   calculation to get the size based on the actual text length.
 
   Copyright (C) 2007 Martin Wehner
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Martin Wehner <martin.wehner@gmail.com>
*/

#ifndef NAUTILUS_CELL_RENDERER_TEXT_ELLIPSIZED_H
#define NAUTILUS_CELL_RENDERER_TEXT_ELLIPSIZED_H

#include <gtk/gtk.h>

#define NAUTILUS_TYPE_CELL_RENDERER_TEXT_ELLIPSIZED nautilus_cell_renderer_text_ellipsized_get_type()
#define NAUTILUS_CELL_RENDERER_TEXT_ELLIPSIZED(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_CELL_RENDERER_TEXT_ELLIPSIZED, NautilusCellRendererTextEllipsized))
#define NAUTILUS_CELL_RENDERER_TEXT_ELLIPSIZED_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CELL_RENDERER_TEXT_ELLIPSIZED, NautilusCellRendererTextEllipsizedClass))
#define NAUTILUS_IS_CELL_RENDERER_TEXT_ELLIPSIZED(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_CELL_RENDERER_TEXT_ELLIPSIZED))
#define NAUTILUS_IS_CELL_RENDERER_TEXT_ELLIPSIZED_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_CELL_RENDERER_TEXT_ELLIPSIZED))
#define NAUTILUS_CELL_RENDERER_TEXT_ELLIPSIZED_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_CELL_RENDERER_TEXT_ELLIPSIZED, NautilusCellRendererTextEllipsizedClass))


typedef struct _NautilusCellRendererTextEllipsized NautilusCellRendererTextEllipsized;
typedef struct _NautilusCellRendererTextEllipsizedClass NautilusCellRendererTextEllipsizedClass;

struct _NautilusCellRendererTextEllipsized {
	GtkCellRendererText parent;
};

struct _NautilusCellRendererTextEllipsizedClass {
	GtkCellRendererTextClass parent_class;
};

GType		 nautilus_cell_renderer_text_ellipsized_get_type (void);
GtkCellRenderer *nautilus_cell_renderer_text_ellipsized_new      (void);

#endif /* NAUTILUS_CELL_RENDERER_TEXT_ELLIPSIZED_H */

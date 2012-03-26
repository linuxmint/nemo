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

#include "nautilus-cell-renderer-text-ellipsized.h"

G_DEFINE_TYPE (NautilusCellRendererTextEllipsized, nautilus_cell_renderer_text_ellipsized,
	       GTK_TYPE_CELL_RENDERER_TEXT);

static void
nautilus_cell_renderer_text_ellipsized_init (NautilusCellRendererTextEllipsized *cell)
{
	g_object_set (cell,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "ellipsize-set", TRUE,
		      NULL);
}

static void
nautilus_cell_renderer_text_ellipsized_get_preferred_width (GtkCellRenderer *cell,
							    GtkWidget       *widget,
							    gint            *minimum_size,
							    gint            *natural_size)
{
	g_object_set (cell,
		      "ellipsize", PANGO_ELLIPSIZE_NONE,
		      "ellipsize-set", FALSE,
		      NULL);

	GTK_CELL_RENDERER_CLASS
		(nautilus_cell_renderer_text_ellipsized_parent_class)->get_preferred_width (cell, widget,
											    minimum_size, natural_size);

	g_object_set (cell,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "ellipsize-set", TRUE,
		      NULL);
}

static void
nautilus_cell_renderer_text_ellipsized_class_init (NautilusCellRendererTextEllipsizedClass *klass)
{
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

	cell_class->get_preferred_width = nautilus_cell_renderer_text_ellipsized_get_preferred_width;
}

GtkCellRenderer *
nautilus_cell_renderer_text_ellipsized_new (void)
{
	return g_object_new (NAUTILUS_TYPE_CELL_RENDERER_TEXT_ELLIPSIZED, NULL);
}

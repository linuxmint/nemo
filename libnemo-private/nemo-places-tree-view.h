/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 
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
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
*/

#ifndef NEMO_PLACES_TREE_VIEW_H
#define NEMO_PLACES_TREE_VIEW_H

#include <gtk/gtk.h>

#define NEMO_TYPE_PLACES_TREE_VIEW nemo_places_tree_view_get_type()
#define NEMO_PLACES_TREE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_PLACES_TREE_VIEW, NemoPlacesTreeView))
#define NEMO_PLACES_TREE_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_PLACES_TREE_VIEW, NemoPlacesTreeViewClass))
#define NEMO_IS_PLACES_TREE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_PLACES_TREE_VIEW))
#define NEMO_IS_PLACES_TREE_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_PLACES_TREE_VIEW))
#define NEMO_PLACES_TREE_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_PLACES_TREE_VIEW, NemoPlacesTreeViewClass))

typedef struct _NemoPlacesTreeView NemoPlacesTreeView;
typedef struct _NemoPlacesTreeViewClass NemoPlacesTreeViewClass;

struct _NemoPlacesTreeView {
	GtkTreeView parent;
};

struct _NemoPlacesTreeViewClass {
	GtkTreeViewClass parent_class;
};

GType        nemo_places_tree_view_get_type (void);
GtkWidget   *nemo_places_tree_view_new      (void);

#endif /* NEMO_PLACES_TREE_VIEW_H */

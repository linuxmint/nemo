/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000, 2001 Eazel, Inc
 * Copyright (C) 2002 Anders Carlsson
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Maciej Stachowiak <mjs@eazel.com>
 *          Anders Carlsson <andersca@gnu.org> 
 */

/* fm-tree-view.h - tree view. */


#ifndef FM_TREE_VIEW_H
#define FM_TREE_VIEW_H

#include <gtk/gtk.h>

#include "nautilus-window.h"

#define FM_TYPE_TREE_VIEW fm_tree_view_get_type()
#define FM_TREE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), FM_TYPE_TREE_VIEW, FMTreeView))
#define FM_TREE_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), FM_TYPE_TREE_VIEW, FMTreeViewClass))
#define FM_IS_TREE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FM_TYPE_TREE_VIEW))
#define FM_IS_TREE_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), FM_TYPE_TREE_VIEW))
#define FM_TREE_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), FM_TYPE_TREE_VIEW, FMTreeViewClass))

#define TREE_SIDEBAR_ID "tree"

typedef struct FMTreeViewDetails FMTreeViewDetails;

typedef struct {
	GtkScrolledWindow parent;
	
	FMTreeViewDetails *details;
} FMTreeView;

typedef struct {
	GtkScrolledWindowClass parent_class;
} FMTreeViewClass;

GType fm_tree_view_get_type (void);

GtkWidget *nautilus_tree_sidebar_new (NautilusWindow *window);

#endif /* FM_TREE_VIEW_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * Author: Dave Camp <dave@ximian.com>
 */

/* nemo-tree-view-drag-dest.h: Handles drag and drop for treeviews which 
 *                                 contain a hierarchy of files
 */

#ifndef NEMO_TREE_VIEW_DRAG_DEST_H
#define NEMO_TREE_VIEW_DRAG_DEST_H

#include <gtk/gtk.h>

#include "nemo-file.h"

G_BEGIN_DECLS

#define NEMO_TYPE_TREE_VIEW_DRAG_DEST	(nemo_tree_view_drag_dest_get_type ())
#define NEMO_TREE_VIEW_DRAG_DEST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_TREE_VIEW_DRAG_DEST, NemoTreeViewDragDest))
#define NEMO_TREE_VIEW_DRAG_DEST_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_TREE_VIEW_DRAG_DEST, NemoTreeViewDragDestClass))
#define NEMO_IS_TREE_VIEW_DRAG_DEST(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_TREE_VIEW_DRAG_DEST))
#define NEMO_IS_TREE_VIEW_DRAG_DEST_CLASS(klass)	(G_TYPE_CLASS_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_TREE_VIEW_DRAG_DEST))

typedef struct _NemoTreeViewDragDest        NemoTreeViewDragDest;
typedef struct _NemoTreeViewDragDestClass   NemoTreeViewDragDestClass;
typedef struct _NemoTreeViewDragDestDetails NemoTreeViewDragDestDetails;

struct _NemoTreeViewDragDest {
	GObject parent;
	
	NemoTreeViewDragDestDetails *details;
};

struct _NemoTreeViewDragDestClass {
	GObjectClass parent;
	
	char *(*get_root_uri) (NemoTreeViewDragDest *dest);
	NemoFile *(*get_file_for_path) (NemoTreeViewDragDest *dest,
					    GtkTreePath *path);
	void (*move_copy_items) (NemoTreeViewDragDest *dest,
				 const GList *item_uris,
				 const char *target_uri,
				 GdkDragAction action,
				 int x,
				 int y);
	void (* handle_netscape_url) (NemoTreeViewDragDest *dest,
				 const char *url,
				 const char *target_uri,
				 GdkDragAction action,
				 int x,
				 int y);
	void (* handle_uri_list) (NemoTreeViewDragDest *dest,
				  const char *uri_list,
				  const char *target_uri,
				  GdkDragAction action,
				  int x,
				  int y);
	void (* handle_text)    (NemoTreeViewDragDest *dest,
				  const char *text,
				  const char *target_uri,
				  GdkDragAction action,
				  int x,
				  int y);
	void (* handle_raw)    (NemoTreeViewDragDest *dest,
				  char *raw_data,
				  int length,
				  const char *target_uri,
				  const char *direct_save_uri,
				  GdkDragAction action,
				  int x,
				  int y);
};

GType                     nemo_tree_view_drag_dest_get_type (void);
NemoTreeViewDragDest *nemo_tree_view_drag_dest_new      (GtkTreeView *tree_view);

G_END_DECLS

#endif

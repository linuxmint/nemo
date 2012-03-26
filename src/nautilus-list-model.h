/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-model.h - a GtkTreeModel for file lists. 

   Copyright (C) 2001, 2002 Anders Carlsson

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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Anders Carlsson <andersca@gnu.org>
*/

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-directory.h>
#include <libnautilus-extension/nautilus-column.h>

#ifndef NAUTILUS_LIST_MODEL_H
#define NAUTILUS_LIST_MODEL_H

#define NAUTILUS_TYPE_LIST_MODEL nautilus_list_model_get_type()
#define NAUTILUS_LIST_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_LIST_MODEL, NautilusListModel))
#define NAUTILUS_LIST_MODEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_LIST_MODEL, NautilusListModelClass))
#define NAUTILUS_IS_LIST_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_LIST_MODEL))
#define NAUTILUS_IS_LIST_MODEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_LIST_MODEL))
#define NAUTILUS_LIST_MODEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_LIST_MODEL, NautilusListModelClass))

enum {
	NAUTILUS_LIST_MODEL_FILE_COLUMN,
	NAUTILUS_LIST_MODEL_SUBDIRECTORY_COLUMN,
	NAUTILUS_LIST_MODEL_SMALLEST_ICON_COLUMN,
	NAUTILUS_LIST_MODEL_SMALLER_ICON_COLUMN,
	NAUTILUS_LIST_MODEL_SMALL_ICON_COLUMN,
	NAUTILUS_LIST_MODEL_STANDARD_ICON_COLUMN,
	NAUTILUS_LIST_MODEL_LARGE_ICON_COLUMN,
	NAUTILUS_LIST_MODEL_LARGER_ICON_COLUMN,
	NAUTILUS_LIST_MODEL_LARGEST_ICON_COLUMN,
	NAUTILUS_LIST_MODEL_FILE_NAME_IS_EDITABLE_COLUMN,
	NAUTILUS_LIST_MODEL_NUM_COLUMNS
};

typedef struct NautilusListModelDetails NautilusListModelDetails;

typedef struct NautilusListModel {
	GObject parent_instance;
	NautilusListModelDetails *details;
} NautilusListModel;

typedef struct {
	GObjectClass parent_class;

	void (* subdirectory_unloaded)(NautilusListModel *model,
				       NautilusDirectory *subdirectory);
} NautilusListModelClass;

GType    nautilus_list_model_get_type                          (void);
gboolean nautilus_list_model_add_file                          (NautilusListModel          *model,
								NautilusFile         *file,
								NautilusDirectory    *directory);
void     nautilus_list_model_file_changed                      (NautilusListModel          *model,
								NautilusFile         *file,
								NautilusDirectory    *directory);
gboolean nautilus_list_model_is_empty                          (NautilusListModel          *model);
guint    nautilus_list_model_get_length                        (NautilusListModel          *model);
void     nautilus_list_model_remove_file                       (NautilusListModel          *model,
								NautilusFile         *file,
								NautilusDirectory    *directory);
void     nautilus_list_model_clear                             (NautilusListModel          *model);
gboolean nautilus_list_model_get_tree_iter_from_file           (NautilusListModel          *model,
								NautilusFile         *file,
								NautilusDirectory    *directory,
								GtkTreeIter          *iter);
GList *  nautilus_list_model_get_all_iters_for_file            (NautilusListModel          *model,
								NautilusFile         *file);
gboolean nautilus_list_model_get_first_iter_for_file           (NautilusListModel          *model,
								NautilusFile         *file,
								GtkTreeIter          *iter);
void     nautilus_list_model_set_should_sort_directories_first (NautilusListModel          *model,
								gboolean              sort_directories_first);

int      nautilus_list_model_get_sort_column_id_from_attribute (NautilusListModel *model,
								GQuark       attribute);
GQuark   nautilus_list_model_get_attribute_from_sort_column_id (NautilusListModel *model,
								int sort_column_id);
void     nautilus_list_model_sort_files                        (NautilusListModel *model,
								GList **files);

NautilusZoomLevel nautilus_list_model_get_zoom_level_from_column_id (int               column);
int               nautilus_list_model_get_column_id_from_zoom_level (NautilusZoomLevel zoom_level);

NautilusFile *    nautilus_list_model_file_for_path (NautilusListModel *model, GtkTreePath *path);
gboolean          nautilus_list_model_load_subdirectory (NautilusListModel *model, GtkTreePath *path, NautilusDirectory **directory);
void              nautilus_list_model_unload_subdirectory (NautilusListModel *model, GtkTreeIter *iter);

void              nautilus_list_model_set_drag_view (NautilusListModel *model,
						     GtkTreeView *view,
						     int begin_x, 
						     int begin_y);

GtkTargetList *   nautilus_list_model_get_drag_target_list (void);

int               nautilus_list_model_compare_func (NautilusListModel *model,
						    NautilusFile *file1,
						    NautilusFile *file2);


int               nautilus_list_model_add_column (NautilusListModel *model,
						  NautilusColumn *column);
int               nautilus_list_model_get_column_number (NautilusListModel *model,
							 const char *column_name);

void              nautilus_list_model_subdirectory_done_loading (NautilusListModel       *model,
								 NautilusDirectory *directory);

void              nautilus_list_model_set_highlight_for_files (NautilusListModel *model,
							       GList *files);
						   
#endif /* NAUTILUS_LIST_MODEL_H */

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
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Anders Carlsson <andersca@gnu.org>
*/

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-directory.h>
#include <libnemo-extension/nemo-column.h>

#ifndef NEMO_LIST_MODEL_H
#define NEMO_LIST_MODEL_H

#define NEMO_TYPE_LIST_MODEL nemo_list_model_get_type()
#define NEMO_LIST_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_LIST_MODEL, NemoListModel))
#define NEMO_LIST_MODEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_LIST_MODEL, NemoListModelClass))
#define NEMO_IS_LIST_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_LIST_MODEL))
#define NEMO_IS_LIST_MODEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_LIST_MODEL))
#define NEMO_LIST_MODEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_LIST_MODEL, NemoListModelClass))

enum {
	NEMO_LIST_MODEL_FILE_COLUMN,
	NEMO_LIST_MODEL_SUBDIRECTORY_COLUMN,
	NEMO_LIST_MODEL_SMALLEST_ICON_COLUMN,
	NEMO_LIST_MODEL_SMALLER_ICON_COLUMN,
	NEMO_LIST_MODEL_SMALL_ICON_COLUMN,
	NEMO_LIST_MODEL_STANDARD_ICON_COLUMN,
	NEMO_LIST_MODEL_LARGE_ICON_COLUMN,
	NEMO_LIST_MODEL_LARGER_ICON_COLUMN,
	NEMO_LIST_MODEL_LARGEST_ICON_COLUMN,
	NEMO_LIST_MODEL_FILE_NAME_IS_EDITABLE_COLUMN,
	NEMO_LIST_MODEL_NUM_COLUMNS
};

typedef struct NemoListModelDetails NemoListModelDetails;

typedef struct NemoListModel {
	GObject parent_instance;
	NemoListModelDetails *details;
} NemoListModel;

typedef struct {
	GObjectClass parent_class;

	void (* subdirectory_unloaded)(NemoListModel *model,
				       NemoDirectory *subdirectory);
} NemoListModelClass;

GType    nemo_list_model_get_type                          (void);
gboolean nemo_list_model_add_file                          (NemoListModel          *model,
								NemoFile         *file,
								NemoDirectory    *directory);
void     nemo_list_model_file_changed                      (NemoListModel          *model,
								NemoFile         *file,
								NemoDirectory    *directory);
gboolean nemo_list_model_is_empty                          (NemoListModel          *model);
guint    nemo_list_model_get_length                        (NemoListModel          *model);
void     nemo_list_model_remove_file                       (NemoListModel          *model,
								NemoFile         *file,
								NemoDirectory    *directory);
void     nemo_list_model_clear                             (NemoListModel          *model);
gboolean nemo_list_model_get_tree_iter_from_file           (NemoListModel          *model,
								NemoFile         *file,
								NemoDirectory    *directory,
								GtkTreeIter          *iter);
GList *  nemo_list_model_get_all_iters_for_file            (NemoListModel          *model,
								NemoFile         *file);
gboolean nemo_list_model_get_first_iter_for_file           (NemoListModel          *model,
								NemoFile         *file,
								GtkTreeIter          *iter);
void     nemo_list_model_set_should_sort_directories_first (NemoListModel          *model,
								gboolean              sort_directories_first);

int      nemo_list_model_get_sort_column_id_from_attribute (NemoListModel *model,
								GQuark       attribute);
GQuark   nemo_list_model_get_attribute_from_sort_column_id (NemoListModel *model,
								int sort_column_id);
void     nemo_list_model_sort_files                        (NemoListModel *model,
								GList **files);

NemoZoomLevel nemo_list_model_get_zoom_level_from_column_id (int               column);
int               nemo_list_model_get_column_id_from_zoom_level (NemoZoomLevel zoom_level);

NemoFile *    nemo_list_model_file_for_path (NemoListModel *model, GtkTreePath *path);
gboolean          nemo_list_model_load_subdirectory (NemoListModel *model, GtkTreePath *path, NemoDirectory **directory);
void              nemo_list_model_unload_subdirectory (NemoListModel *model, GtkTreeIter *iter);

void              nemo_list_model_set_drag_view (NemoListModel *model,
						     GtkTreeView *view,
						     int begin_x, 
						     int begin_y);

GtkTargetList *   nemo_list_model_get_drag_target_list (void);

int               nemo_list_model_compare_func (NemoListModel *model,
						    NemoFile *file1,
						    NemoFile *file2);


int               nemo_list_model_add_column (NemoListModel *model,
						  NemoColumn *column);
int               nemo_list_model_get_column_number (NemoListModel *model,
							 const char *column_name);

void              nemo_list_model_subdirectory_done_loading (NemoListModel       *model,
								 NemoDirectory *directory);

void              nemo_list_model_set_highlight_for_files (NemoListModel *model,
							       GList *files);
						   
#endif /* NEMO_LIST_MODEL_H */

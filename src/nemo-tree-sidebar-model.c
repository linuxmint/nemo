/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright C) 2000, 2001 Eazel, Inc
 * Copyright (C) 2002 Anders Carlsson
 * Copyright (C) 2002 Bent Spoon Software
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
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Anders Carlsson <andersca@gnu.org>
 *          Darin Adler <darin@bentspoon.com>
 */

/* fm-tree-model.c - model for the tree view */

#include <config.h>

#include "nemo-tree-sidebar-model.h"

#include <eel/eel-graphic-effects.h>

#include <libnemo-private/nemo-directory.h>
#include <libnemo-private/nemo-file-attributes.h>
#include <libnemo-private/nemo-file.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

enum {
  ROW_LOADED,
  LAST_SIGNAL
};

static guint tree_model_signals[LAST_SIGNAL] = { 0 };

typedef gboolean (* FilePredicate) (NemoFile *);

/* The user_data of the GtkTreeIter is the TreeNode pointer.
 * It's NULL for the dummy node. If it's NULL, then user_data2
 * is the TreeNode pointer to the parent.
 */

typedef struct TreeNode TreeNode;
typedef struct FMTreeModelRoot FMTreeModelRoot;

struct TreeNode {
	/* part of this node for the file itself */
	int ref_count;

	NemoFile *file;
	char *display_name;
	GIcon *icon;
	GMount *mount;
	GdkPixbuf *closed_pixbuf;
	GdkPixbuf *open_pixbuf;

	FMTreeModelRoot *root;

	TreeNode *parent;
	TreeNode *next;
	TreeNode *prev;

	/* part of the node used only for directories */
	int dummy_child_ref_count;
	int all_children_ref_count;
	
	NemoDirectory *directory;
	guint done_loading_id;
	guint files_added_id;
	guint files_changed_id;

	TreeNode *first_child;

	/* misc. flags */
	guint done_loading : 1;
	guint force_has_dummy : 1;
	guint inserted : 1;
};

struct FMTreeModelDetails {
	int stamp;
	
	TreeNode *root_node;	

	guint monitoring_update_idle_id;

	gboolean show_hidden_files;
	gboolean show_only_directories;

	GList *highlighted_files;
};

struct FMTreeModelRoot {
	FMTreeModel *model;

	/* separate hash table for each root node needed */
	GHashTable *file_to_node_map;
	
	TreeNode *root_node;
};

typedef struct {
	NemoDirectory *directory;
	FMTreeModel *model;
} DoneLoadingParameters;

static void fm_tree_model_tree_model_init (GtkTreeModelIface *iface);
static void schedule_monitoring_update     (FMTreeModel *model);
static void destroy_node_without_reporting (FMTreeModel *model,
					    TreeNode          *node);
static void report_node_contents_changed   (FMTreeModel *model,
					    TreeNode          *node);

G_DEFINE_TYPE_WITH_CODE (FMTreeModel, fm_tree_model, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
						fm_tree_model_tree_model_init));

static GtkTreeModelFlags 
fm_tree_model_get_flags (GtkTreeModel *tree_model)
{
	return GTK_TREE_MODEL_ITERS_PERSIST;
}

static void
object_unref_if_not_NULL (gpointer object)
{
	if (object == NULL) {
		return;
	}
	g_object_unref (object);
}

static FMTreeModelRoot *
tree_model_root_new (FMTreeModel *model)
{
	FMTreeModelRoot *root;

	root = g_new0 (FMTreeModelRoot, 1);
	root->model = model;
	root->file_to_node_map = g_hash_table_new (NULL, NULL);

	return root;
}

static TreeNode *
tree_node_new (NemoFile *file, FMTreeModelRoot *root)
{
	TreeNode *node;

	node = g_new0 (TreeNode, 1);
	node->file = nemo_file_ref (file);
	node->root = root;
	return node;
}

static void
tree_node_unparent (FMTreeModel *model, TreeNode *node)
{
	TreeNode *parent, *next, *prev;

	parent = node->parent;
	next = node->next;
	prev = node->prev;

	if (parent == NULL &&
	    node == model->details->root_node) {
		/* it's the first root node -> if there is a next then let it be the first root node */
		model->details->root_node = next;
	}

	if (next != NULL) {
		next->prev = prev;
	}
	if (prev == NULL && parent != NULL) {
		g_assert (parent->first_child == node);
		parent->first_child = next;
	} else if (prev != NULL) {
		prev->next = next;
	}

	node->parent = NULL;
	node->next = NULL;
	node->prev = NULL;
	node->root = NULL;
}

static void
tree_node_destroy (FMTreeModel *model, TreeNode *node)
{
	g_assert (node->first_child == NULL);
	g_assert (node->ref_count == 0);

	tree_node_unparent (model, node);

	g_object_unref (node->file);
	g_free (node->display_name);
	object_unref_if_not_NULL (node->icon);
	object_unref_if_not_NULL (node->closed_pixbuf);
	object_unref_if_not_NULL (node->open_pixbuf);

	g_assert (node->done_loading_id == 0);
	g_assert (node->files_added_id == 0);
	g_assert (node->files_changed_id == 0);
	nemo_directory_unref (node->directory);

	g_free (node);
}

static void
tree_node_parent (TreeNode *node, TreeNode *parent)
{
	TreeNode *first_child;

	g_assert (parent != NULL);
	g_assert (node->parent == NULL);
	g_assert (node->prev == NULL);
	g_assert (node->next == NULL);

	first_child = parent->first_child;
	
	node->parent = parent;
	node->root = parent->root;
	node->next = first_child;

	if (first_child != NULL) {
		g_assert (first_child->prev == NULL);
		first_child->prev = node;
	}

	parent->first_child = node;
}

static GdkPixbuf *
get_menu_icon (GIcon *icon)
{
	NemoIconInfo *info;
	GdkPixbuf *pixbuf;
	int size;

	size = nemo_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
	
	info = nemo_icon_info_lookup (icon, size);
	pixbuf = nemo_icon_info_get_pixbuf_nodefault_at_size (info, size);
	g_object_unref (info);
	
	return pixbuf;
}

static GdkPixbuf *
get_menu_icon_for_file (TreeNode *node,
                        NemoFile *file,
			NemoFileIconFlags flags)
{
	NemoIconInfo *info;
	GIcon *gicon, *emblem_icon, *emblemed_icon;
	GEmblem *emblem;
	GdkPixbuf *pixbuf, *retval;
	gboolean highlight;
	int size;
	FMTreeModel *model;
	GList *emblem_icons, *l;
	char *emblems_to_ignore[3];
	int i;

	size = nemo_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
	gicon = G_ICON (nemo_file_get_icon_pixbuf (file, size, TRUE, flags));

	i = 0;
	emblems_to_ignore[i++] = NEMO_FILE_EMBLEM_NAME_TRASH;

	if (node->parent && node->parent->file) {
		if (!nemo_file_can_write (node->parent->file)) {
			emblems_to_ignore[i++] = NEMO_FILE_EMBLEM_NAME_CANT_WRITE;
		}
	}
	
	emblems_to_ignore[i++] = NULL;

	emblem = NULL;
	emblem_icons = nemo_file_get_emblem_icons (node->file,
						       emblems_to_ignore);

	/* pick only the first emblem we can render for the tree view */
	for (l = emblem_icons; l != NULL; l = l->next) {
		emblem_icon = l->data;
		if (nemo_icon_theme_can_render (G_THEMED_ICON (emblem_icon))) {
			emblem = g_emblem_new (emblem_icon);
			emblemed_icon = g_emblemed_icon_new (gicon, emblem);

			g_object_unref (gicon);
			g_object_unref (emblem);
			gicon = emblemed_icon;

			break;
		}
	}

	g_list_free_full (emblem_icons, g_object_unref);

	info = nemo_icon_info_lookup (gicon, size);
	retval = nemo_icon_info_get_pixbuf_nodefault_at_size (info, size);
	model = node->root->model;

	g_object_unref (gicon);

	highlight = (g_list_find_custom (model->details->highlighted_files,
	                                 file, (GCompareFunc) nemo_file_compare_location) != NULL);

	if (highlight) {
		pixbuf = eel_create_spotlight_pixbuf (retval);

		if (pixbuf != NULL) {
			g_object_unref (retval);
			retval = pixbuf;
		}
	}

	g_object_unref (info);

	return retval;
}

static GdkPixbuf *
tree_node_get_pixbuf (TreeNode *node,
		      NemoFileIconFlags flags)
{
	if (node->parent == NULL) {
		return get_menu_icon (node->icon);
	}
	return get_menu_icon_for_file (node, node->file, flags);
}

static gboolean
tree_node_update_pixbuf (TreeNode *node,
			 GdkPixbuf **pixbuf_storage,
			 NemoFileIconFlags flags)
{
	GdkPixbuf *pixbuf;

	if (*pixbuf_storage == NULL) {
		return FALSE;
	}
	pixbuf = tree_node_get_pixbuf (node, flags);
	if (pixbuf == *pixbuf_storage) {
		g_object_unref (pixbuf);
		return FALSE;
	}
	g_object_unref (*pixbuf_storage);
	*pixbuf_storage = pixbuf;
	return TRUE;
}

static gboolean
tree_node_update_closed_pixbuf (TreeNode *node)
{
	return tree_node_update_pixbuf (node, &node->closed_pixbuf, 0);
}

static gboolean
tree_node_update_open_pixbuf (TreeNode *node)
{
	return tree_node_update_pixbuf (node, &node->open_pixbuf, NEMO_FILE_ICON_FLAGS_FOR_OPEN_FOLDER);
}

static gboolean
tree_node_update_display_name (TreeNode *node)
{
	char *display_name;

	if (node->display_name == NULL) {
		return FALSE;
	}
	/* don't update root node display names */
	if (node->parent == NULL) {
		return FALSE;
	} 
	display_name = nemo_file_get_display_name (node->file);
	if (strcmp (display_name, node->display_name) == 0) {
		g_free (display_name);
		return FALSE;
	}
	g_free (node->display_name);
	node->display_name = NULL;
	return TRUE;
}

static GdkPixbuf *
tree_node_get_closed_pixbuf (TreeNode *node)
{
	if (node->closed_pixbuf == NULL) {
		node->closed_pixbuf = tree_node_get_pixbuf (node, 0);
	}
	return node->closed_pixbuf;
}

static GdkPixbuf *
tree_node_get_open_pixbuf (TreeNode *node)
{
	if (node->open_pixbuf == NULL) {
		node->open_pixbuf = tree_node_get_pixbuf (node, NEMO_FILE_ICON_FLAGS_FOR_OPEN_FOLDER);
	}
	return node->open_pixbuf;
}

static const char *
tree_node_get_display_name (TreeNode *node)
{
	if (node->display_name == NULL) {
		node->display_name = nemo_file_get_display_name (node->file);
	}
	return node->display_name;
}

static gboolean
tree_node_has_dummy_child (TreeNode *node)
{
	return (node->directory != NULL
		&& (!node->done_loading
		    || node->first_child == NULL
		    || node->force_has_dummy)) ||
		/* Roots always have dummy nodes if directory isn't loaded yet */
		(node->directory == NULL && node->parent == NULL);
}

static int
tree_node_get_child_index (TreeNode *parent, TreeNode *child)
{
	int i;
	TreeNode *node;

	if (child == NULL) {
		g_assert (tree_node_has_dummy_child (parent));
		return 0;
	}

	i = tree_node_has_dummy_child (parent) ? 1 : 0;
	for (node = parent->first_child; node != NULL; node = node->next, i++) {
		if (child == node) {
			return i;
		}
	}

	g_assert_not_reached ();
	return 0;
}

static gboolean
make_iter_invalid (GtkTreeIter *iter)
{
	iter->stamp = 0;
	iter->user_data = NULL;
	iter->user_data2 = NULL;
	iter->user_data3 = NULL;
	return FALSE;
}

static gboolean
make_iter_for_node (TreeNode *node, GtkTreeIter *iter, int stamp)
{
	if (node == NULL) {
		return make_iter_invalid (iter);
	}
	iter->stamp = stamp;
	iter->user_data = node;
	iter->user_data2 = NULL;
	iter->user_data3 = NULL;
	return TRUE;
}

static gboolean
make_iter_for_dummy_row (TreeNode *parent, GtkTreeIter *iter, int stamp)
{
	g_assert (tree_node_has_dummy_child (parent));
	g_assert (parent != NULL);
	iter->stamp = stamp;
	iter->user_data = NULL;
	iter->user_data2 = parent;
	iter->user_data3 = NULL;
	return TRUE;
}

static TreeNode *
get_node_from_file (FMTreeModelRoot *root, NemoFile *file)
{
	return g_hash_table_lookup (root->file_to_node_map, file);
}

static TreeNode *
get_parent_node_from_file (FMTreeModelRoot *root, NemoFile *file)
{
	NemoFile *parent_file;
	TreeNode *parent_node;
	
	parent_file = nemo_file_get_parent (file);
	parent_node = get_node_from_file (root, parent_file);
	nemo_file_unref (parent_file);
	return parent_node;
}

static TreeNode *
create_node_for_file (FMTreeModelRoot *root, NemoFile *file)
{
	TreeNode *node;

	g_assert (get_node_from_file (root, file) == NULL);
	node = tree_node_new (file, root);
	g_hash_table_insert (root->file_to_node_map, node->file, node);
	return node;
}

#ifdef LOG_REF_COUNTS

static char *
get_node_uri (GtkTreeIter *iter)
{
	TreeNode *node, *parent;
	char *parent_uri, *node_uri;

	node = iter->user_data;
	if (node != NULL) {
		return nemo_file_get_uri (node->file);
	}

	parent = iter->user_data2;
	parent_uri = nemo_file_get_uri (parent->file);
	node_uri = g_strconcat (parent_uri, " -- DUMMY", NULL);
	g_free (parent_uri);
	return node_uri;
}

#endif

static void
decrement_ref_count (FMTreeModel *model, TreeNode *node, int count)
{
	node->all_children_ref_count -= count;
	if (node->all_children_ref_count == 0) {
		schedule_monitoring_update (model);
	}
}

static void
abandon_node_ref_count (FMTreeModel *model, TreeNode *node)
{
	if (node->parent != NULL) {
		decrement_ref_count (model, node->parent, node->ref_count);
#ifdef LOG_REF_COUNTS
		if (node->ref_count != 0) {
			char *uri;

			uri = nemo_file_get_uri (node->file);
			g_message ("abandoning %d ref of %s, count is now %d",
				   node->ref_count, uri, node->parent->all_children_ref_count);
			g_free (uri);
		}
#endif
	}
	node->ref_count = 0;
}

static void
abandon_dummy_row_ref_count (FMTreeModel *model, TreeNode *node)
{
	decrement_ref_count (model, node, node->dummy_child_ref_count);
	if (node->dummy_child_ref_count != 0) {
#ifdef LOG_REF_COUNTS
		char *uri;

		uri = nemo_file_get_uri (node->file);
		g_message ("abandoning %d ref of %s -- DUMMY, count is now %d",
			   node->dummy_child_ref_count, uri, node->all_children_ref_count);
		g_free (uri);
#endif
	}
	node->dummy_child_ref_count = 0;
}

static void
report_row_inserted (FMTreeModel *model, GtkTreeIter *iter)
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, iter);
	gtk_tree_path_free (path);
}

static void
report_row_contents_changed (FMTreeModel *model, GtkTreeIter *iter)
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
	gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, iter);
	gtk_tree_path_free (path);
}

static void
report_row_has_child_toggled (FMTreeModel *model, GtkTreeIter *iter)
{
	GtkTreePath *path;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
	gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, iter);
	gtk_tree_path_free (path);
}

static GtkTreePath *
get_node_path (FMTreeModel *model, TreeNode *node)
{
	GtkTreeIter iter;

	make_iter_for_node (node, &iter, model->details->stamp);
	return gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
}

static void
report_dummy_row_inserted (FMTreeModel *model, TreeNode *parent)
{
	GtkTreeIter iter;

	if (!parent->inserted) {
		return;
	}
	make_iter_for_dummy_row (parent, &iter, model->details->stamp);
	report_row_inserted (model, &iter);
}

static void
report_dummy_row_deleted (FMTreeModel *model, TreeNode *parent)
{
	GtkTreeIter iter;
	GtkTreePath *path;

	if (parent->inserted) {
		make_iter_for_node (parent, &iter, model->details->stamp);
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
		gtk_tree_path_append_index (path, 0);
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
		gtk_tree_path_free (path);
	}
	abandon_dummy_row_ref_count (model, parent);
}

static void
report_node_inserted (FMTreeModel *model, TreeNode *node)
{
	GtkTreeIter iter;

	make_iter_for_node (node, &iter, model->details->stamp);
	report_row_inserted (model, &iter);
	node->inserted = TRUE;

	if (tree_node_has_dummy_child (node)) {
		report_dummy_row_inserted (model, node);
	}

    gboolean add_child = FALSE;

	if (node->directory != NULL) {
        gint count;
        if (nemo_file_get_directory_item_count (node->file, &count, NULL)) {
            add_child = count > 0 || node->parent == NULL;
        } else {
            add_child = node->parent == NULL ||
                        nemo_dir_has_children_now (nemo_file_get_location (node->file));
        }
    }

    if (add_child)
        report_row_has_child_toggled (model, &iter);
}

static void
report_node_contents_changed (FMTreeModel *model, TreeNode *node)
{
	GtkTreeIter iter;

	if (!node->inserted) {
		return;
	}
	make_iter_for_node (node, &iter, model->details->stamp);
	report_row_contents_changed (model, &iter);
}

static void
report_node_has_child_toggled (FMTreeModel *model, TreeNode *node)
{
	GtkTreeIter iter;

	if (!node->inserted) {
		return;
	}
	make_iter_for_node (node, &iter, model->details->stamp);
	report_row_has_child_toggled (model, &iter);
}

static void
report_dummy_row_contents_changed (FMTreeModel *model, TreeNode *parent)
{
	GtkTreeIter iter;

	if (!parent->inserted) {
		return;
	}
	make_iter_for_dummy_row (parent, &iter, model->details->stamp);
	report_row_contents_changed (model, &iter);
}

static void
stop_monitoring_directory (FMTreeModel *model, TreeNode *node)
{
	if (node->done_loading_id == 0) {
		g_assert (node->files_added_id == 0);
		g_assert (node->files_changed_id == 0);
		return;
	}

	g_signal_handler_disconnect (node->directory, node->done_loading_id);
	g_signal_handler_disconnect (node->directory, node->files_added_id);
	g_signal_handler_disconnect (node->directory, node->files_changed_id);

	node->done_loading_id = 0;
	node->files_added_id = 0;
	node->files_changed_id = 0;

	nemo_directory_file_monitor_remove (node->directory, model);
}

static void
destroy_children_without_reporting (FMTreeModel *model, TreeNode *parent)
{
	while (parent->first_child != NULL) {
		destroy_node_without_reporting (model, parent->first_child);
	}
}

static void
destroy_node_without_reporting (FMTreeModel *model, TreeNode *node)
{
	abandon_node_ref_count (model, node);
	stop_monitoring_directory (model, node);
	node->inserted = FALSE;
	destroy_children_without_reporting (model, node);
	g_hash_table_remove (node->root->file_to_node_map, node->file);
	tree_node_destroy (model, node);
}

static void
destroy_node (FMTreeModel *model, TreeNode *node)
{
	TreeNode *parent;
	gboolean parent_had_dummy_child;
	GtkTreePath *path;

	parent = node->parent;
	parent_had_dummy_child = tree_node_has_dummy_child (parent);

	path = get_node_path (model, node);

	/* Report row_deleted before actually deleting */
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
	gtk_tree_path_free (path);
	
	destroy_node_without_reporting (model, node);

	if (tree_node_has_dummy_child (parent)) {
		if (!parent_had_dummy_child) {
			report_dummy_row_inserted (model, parent);
		}
	} else {
		g_assert (!parent_had_dummy_child);
	}
}

static void
destroy_children (FMTreeModel *model, TreeNode *parent)
{
	while (parent->first_child != NULL) {
		destroy_node (model, parent->first_child);
	}
}

static void
destroy_children_by_function (FMTreeModel *model, TreeNode *parent, FilePredicate f)
{
	TreeNode *child, *next;

	for (child = parent->first_child; child != NULL; child = next) {
		next = child->next;
		if (f (child->file)) {
			destroy_node (model, child);
		} else {
			destroy_children_by_function (model, child, f);
		}
	}
}

static void
destroy_by_function (FMTreeModel *model, FilePredicate f)
{
	TreeNode *node;
	for (node = model->details->root_node; node != NULL; node = node->next) {
		destroy_children_by_function (model, node, f);
	}
}

static gboolean
update_node_without_reporting (FMTreeModel *model, TreeNode *node)
{
	gboolean changed;

	changed = FALSE;
	
	if (node->directory == NULL &&
	    (nemo_file_is_directory (node->file) || node->parent == NULL)) {
		node->directory = nemo_directory_get_for_file (node->file);
	} else if (node->directory != NULL &&
		   !(nemo_file_is_directory (node->file) || node->parent == NULL)) {
		stop_monitoring_directory (model, node);
		destroy_children (model, node);
		nemo_directory_unref (node->directory);
		node->directory = NULL;
	}

	changed |= tree_node_update_display_name (node);
	changed |= tree_node_update_closed_pixbuf (node);
	changed |= tree_node_update_open_pixbuf (node);

	return changed;
}

static void
insert_node (FMTreeModel *model, TreeNode *parent, TreeNode *node)
{
	gboolean parent_empty;

	parent_empty = parent->first_child == NULL;
	if (parent_empty) {
		/* Make sure the dummy lives as we insert the new row */
		parent->force_has_dummy = TRUE;
	}

	tree_node_parent (node, parent);

	update_node_without_reporting (model, node);
	report_node_inserted (model, node);

	if (parent_empty) {
		parent->force_has_dummy = FALSE;
		if (!tree_node_has_dummy_child (parent)) {
			/* Temporarily set this back so that row_deleted is
			 * sent before actually removing the dummy child */
			parent->force_has_dummy = TRUE;
			report_dummy_row_deleted (model, parent);
			parent->force_has_dummy = FALSE;
		}
	}
}

static void
reparent_node (FMTreeModel *model, TreeNode *node)
{
	GtkTreePath *path;
	TreeNode *new_parent;

	new_parent = get_parent_node_from_file (node->root, node->file);
	if (new_parent == NULL || new_parent->directory == NULL) {
		destroy_node (model, node);
		return;
	}

	path = get_node_path (model, node);

	/* Report row_deleted before actually deleting */
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
	gtk_tree_path_free (path);
	
	abandon_node_ref_count (model, node);
	tree_node_unparent (model, node);

	insert_node (model, new_parent, node);
}

static gboolean
should_show_file (FMTreeModel *model, NemoFile *file)
{
	gboolean should;
	TreeNode *node;

	should = nemo_file_should_show (file,
					    model->details->show_hidden_files,
					    TRUE);

	if (should
	    && model->details->show_only_directories
	    &&! nemo_file_is_directory (file)) {
		should = FALSE;
	}

	if (should && nemo_file_is_gone (file)) {
		should = FALSE;
	}

	for (node = model->details->root_node; node != NULL; node = node->next) {
		if (!should && node != NULL && file == node->file) {
			should = TRUE;
		}
	}

	return should;
}

static void
update_node (FMTreeModel *model, TreeNode *node)
{
	gboolean had_dummy_child, has_dummy_child;
	gboolean had_directory, has_directory;
	gboolean changed;

	if (!should_show_file (model, node->file)) {
		destroy_node (model, node);
		return;
	}

	if (node->parent != NULL && node->parent->directory != NULL
	    && !nemo_directory_contains_file (node->parent->directory, node->file)) {
		reparent_node (model, node);
		return;
	}

	had_dummy_child = tree_node_has_dummy_child (node);
	had_directory = node->directory != NULL;

	changed = update_node_without_reporting (model, node);

	has_dummy_child = tree_node_has_dummy_child (node);
	has_directory = node->directory != NULL;
	
	if (had_dummy_child != has_dummy_child) {
		if (has_dummy_child) {
			report_dummy_row_inserted (model, node);
		} else {
			/* Temporarily set this back so that row_deleted is
			 * sent before actually removing the dummy child */
			node->force_has_dummy = TRUE;
			report_dummy_row_deleted (model, node);
			node->force_has_dummy = FALSE;
		}
	}
	if (had_directory != has_directory) {
		report_node_has_child_toggled (model, node);
	}

	if (changed) {
		report_node_contents_changed (model, node);
	}
}

static void
process_file_change (FMTreeModelRoot *root,
		     NemoFile *file)
{
	TreeNode *node, *parent;

	node = get_node_from_file (root, file);
	if (node != NULL) {
		update_node (root->model, node);
		return;
	}

	if (!should_show_file (root->model, file)) {
		return;
	}

	parent = get_parent_node_from_file (root, file);
	if (parent == NULL) {
		return;
	}

	insert_node (root->model, parent, create_node_for_file (root, file));
}

static void
files_changed_callback (NemoDirectory *directory,
			GList *changed_files,
			gpointer callback_data)
{
	FMTreeModelRoot *root;
	GList *node;

	root = (FMTreeModelRoot *) (callback_data);

	for (node = changed_files; node != NULL; node = node->next) {
		process_file_change (root, NEMO_FILE (node->data));
	}
}

static void
set_done_loading (FMTreeModel *model, TreeNode *node, gboolean done_loading)
{
	gboolean had_dummy;

	if (node == NULL || node->done_loading == done_loading) {
		return;
	}

	had_dummy = tree_node_has_dummy_child (node);

	node->done_loading = done_loading;

	if (tree_node_has_dummy_child (node)) {
		if (had_dummy) {
			report_dummy_row_contents_changed (model, node);
		} else {
			report_dummy_row_inserted (model, node);
		}
	} else {
		if (had_dummy) {
			/* Temporarily set this back so that row_deleted is
			 * sent before actually removing the dummy child */
			node->force_has_dummy = TRUE;
			report_dummy_row_deleted (model, node);
			node->force_has_dummy = FALSE;
		} else {
			g_assert_not_reached ();
		}
	}
}

static void
done_loading_callback (NemoDirectory *directory,
		       FMTreeModelRoot *root)
{
	NemoFile *file;
	TreeNode *node;
	GtkTreeIter iter;

	file = nemo_directory_get_corresponding_file (directory);
	node = get_node_from_file (root, file);
	if (node == NULL) {
		/* This can happen for non-existing files as tree roots,
		 * since the directory <-> file object relation gets
		 * broken due to nemo_directory_remove_file()
		 * getting called when i/o fails.
		 */
		return;
	}
	set_done_loading (root->model, node, TRUE);
	nemo_file_unref (file);

	make_iter_for_node (node, &iter, root->model->details->stamp);
	g_signal_emit (root->model,
		       tree_model_signals[ROW_LOADED], 0,
		       &iter);
}

static NemoFileAttributes
get_tree_monitor_attributes (void)
{
	NemoFileAttributes attributes;

	attributes =
		NEMO_FILE_ATTRIBUTES_FOR_ICON |
		NEMO_FILE_ATTRIBUTE_INFO |
		NEMO_FILE_ATTRIBUTE_LINK_INFO;
	
	return attributes;
}

static void
start_monitoring_directory (FMTreeModel *model, TreeNode *node)
{
	NemoDirectory *directory;
	NemoFileAttributes attributes;

	if (node->done_loading_id != 0) {
		return;
	}

	g_assert (node->files_added_id == 0);
	g_assert (node->files_changed_id == 0);

	directory = node->directory;
	
	node->done_loading_id = g_signal_connect
		(directory, "done_loading",
		 G_CALLBACK (done_loading_callback), node->root);
	node->files_added_id = g_signal_connect 
		(directory, "files_added",
		 G_CALLBACK (files_changed_callback), node->root);
	node->files_changed_id = g_signal_connect 
		(directory, "files_changed",
		 G_CALLBACK (files_changed_callback), node->root);

	set_done_loading (model, node, nemo_directory_are_all_files_seen (directory));

	attributes = get_tree_monitor_attributes ();
	nemo_directory_file_monitor_add (directory, model,
					     model->details->show_hidden_files,
					     attributes, files_changed_callback, node->root);
}

static int
fm_tree_model_get_n_columns (GtkTreeModel *model)
{
	return FM_TREE_MODEL_NUM_COLUMNS;
}

static GType
fm_tree_model_get_column_type (GtkTreeModel *model, int index)
{
	switch (index) {
	case FM_TREE_MODEL_DISPLAY_NAME_COLUMN:
		return G_TYPE_STRING;
	case FM_TREE_MODEL_CLOSED_PIXBUF_COLUMN:
		return GDK_TYPE_PIXBUF;
	case FM_TREE_MODEL_OPEN_PIXBUF_COLUMN:
		return GDK_TYPE_PIXBUF;
	case FM_TREE_MODEL_FONT_STYLE_COLUMN:
		return PANGO_TYPE_STYLE;
	default:
		g_assert_not_reached ();
	}
	
	return G_TYPE_INVALID;
}

static gboolean
iter_is_valid (FMTreeModel *model, const GtkTreeIter *iter)
{
	TreeNode *node, *parent;

	if (iter->stamp != model->details->stamp) {
		return FALSE;
	}

	node = iter->user_data;
	parent = iter->user_data2;
	if (node == NULL) {
		if (parent != NULL) {
			if (!NEMO_IS_FILE (parent->file)) {
				return FALSE;
			}
			if (!tree_node_has_dummy_child (parent)) {
				return FALSE;
			}
		}
	} else {
		if (!NEMO_IS_FILE (node->file)) {
			return FALSE;
		}
		if (parent != NULL) {
			return FALSE;
		}
	}
	if (iter->user_data3 != NULL) {
		return FALSE;
	}
	return TRUE;
}

static gboolean
fm_tree_model_get_iter (GtkTreeModel *model, GtkTreeIter *iter, GtkTreePath *path)
{
	int *indices;
	GtkTreeIter parent;
	int depth, i;

	indices = gtk_tree_path_get_indices (path);
	depth = gtk_tree_path_get_depth (path);

	if (! gtk_tree_model_iter_nth_child (model, iter, NULL, indices[0])) {
		return FALSE;
	}

	for (i = 1; i < depth; i++) {
		parent = *iter;

		if (! gtk_tree_model_iter_nth_child (model, iter, &parent, indices[i])) {
			return FALSE;
		}
	}

	return TRUE;
}

static GtkTreePath *
fm_tree_model_get_path (GtkTreeModel *model, GtkTreeIter *iter)
{
	FMTreeModel *tree_model;
	TreeNode *node, *parent, *cnode;
	GtkTreePath *path;
	GtkTreeIter parent_iter;
	int i;

	g_return_val_if_fail (FM_IS_TREE_MODEL (model), NULL);
	tree_model = FM_TREE_MODEL (model);
	g_return_val_if_fail (iter_is_valid (tree_model, iter), NULL);

	node = iter->user_data;
	if (node == NULL) {
		parent = iter->user_data2;
		if (parent == NULL) {
			return gtk_tree_path_new ();
		}
	} else {
		parent = node->parent;
		if (parent == NULL) {
			i = 0;
			for (cnode = tree_model->details->root_node; cnode != node; cnode = cnode->next) {
				i++;
			}
			path = gtk_tree_path_new ();
			gtk_tree_path_append_index (path, i);
			return path;
		}
	}

	parent_iter.stamp = iter->stamp;
	parent_iter.user_data = parent;
	parent_iter.user_data2 = NULL;
	parent_iter.user_data3 = NULL;

	path = fm_tree_model_get_path (model, &parent_iter);
	
	gtk_tree_path_append_index (path, tree_node_get_child_index (parent, node));

	return path;
}

static void
fm_tree_model_get_value (GtkTreeModel *model, GtkTreeIter *iter, int column, GValue *value)
{
	TreeNode *node, *parent;

	g_return_if_fail (FM_IS_TREE_MODEL (model));
	g_return_if_fail (iter_is_valid (FM_TREE_MODEL (model), iter));

	node = iter->user_data;

	switch (column) {
	case FM_TREE_MODEL_DISPLAY_NAME_COLUMN:
		g_value_init (value, G_TYPE_STRING);
		if (node == NULL) {
			parent = iter->user_data2;
			g_value_set_static_string (value, parent->done_loading
						   ? _("(Empty)") : _("Loading..."));
		} else {
			g_value_set_string (value, tree_node_get_display_name (node));
		}
		break;
	case FM_TREE_MODEL_CLOSED_PIXBUF_COLUMN:
		g_value_init (value, GDK_TYPE_PIXBUF);
		g_value_set_object (value, node == NULL ? NULL : tree_node_get_closed_pixbuf (node));
		break;
	case FM_TREE_MODEL_OPEN_PIXBUF_COLUMN:
		g_value_init (value, GDK_TYPE_PIXBUF);
		g_value_set_object (value, node == NULL ? NULL : tree_node_get_open_pixbuf (node));
		break;
	case FM_TREE_MODEL_FONT_STYLE_COLUMN:
		g_value_init (value, PANGO_TYPE_STYLE);
		if (node == NULL) {
			g_value_set_enum (value, PANGO_STYLE_ITALIC);
		} else {
			g_value_set_enum (value, PANGO_STYLE_NORMAL);
		}
		break;
	default:
		g_assert_not_reached ();
	}
}

static gboolean
fm_tree_model_iter_next (GtkTreeModel *model, GtkTreeIter *iter)
{
	TreeNode *node, *parent, *next;

	g_return_val_if_fail (FM_IS_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (iter_is_valid (FM_TREE_MODEL (model), iter), FALSE);
	
	node = iter->user_data;

	if (node == NULL) {
		parent = iter->user_data2;
		next = parent->first_child;
	} else {
		next = node->next;
	}

	return make_iter_for_node (next, iter, iter->stamp);
}

static gboolean
fm_tree_model_iter_children (GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *parent_iter)
{
	TreeNode *parent;

	g_return_val_if_fail (FM_IS_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (iter_is_valid (FM_TREE_MODEL (model), parent_iter), FALSE);
	
	parent = parent_iter->user_data;
	if (parent == NULL) {
		return make_iter_invalid (iter);
	}

	if (tree_node_has_dummy_child (parent)) {
		return make_iter_for_dummy_row (parent, iter, parent_iter->stamp);
	}
	return make_iter_for_node (parent->first_child, iter, parent_iter->stamp);
}

static gboolean
fm_tree_model_iter_parent (GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *child_iter)
{	TreeNode *child, *parent;
	
	g_return_val_if_fail (FM_IS_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (iter_is_valid (FM_TREE_MODEL (model), child_iter), FALSE);

	child = child_iter->user_data;

	if (child == NULL) {
		parent = child_iter->user_data2;
	} else {
		parent = child->parent;
	}

	return make_iter_for_node (parent, iter, child_iter->stamp);
}

static gboolean
fm_tree_model_iter_has_child (GtkTreeModel *model, GtkTreeIter *iter)
{
	gboolean has_child;
	TreeNode *node;

	g_return_val_if_fail (FM_IS_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (iter_is_valid (FM_TREE_MODEL (model), iter), FALSE);

	node = iter->user_data;

	has_child = node != NULL && (node->directory != NULL || node->parent == NULL);

#if 0
	g_warning ("Node '%s' %s",
		   node && node->file ? nemo_file_get_uri (node->file) : "no name",
		   has_child ? "has child" : "no child");
#endif
		   
	return has_child;
}

static int
fm_tree_model_iter_n_children (GtkTreeModel *model, GtkTreeIter *iter)
{
	TreeNode *parent, *node;
	int n;
	
	g_return_val_if_fail (FM_IS_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (iter == NULL || iter_is_valid (FM_TREE_MODEL (model), iter), FALSE);
	
	if (iter == NULL) {
		return 1;
	}

	parent = iter->user_data;
	if (parent == NULL) {
		return 0;
	}

	n = tree_node_has_dummy_child (parent) ? 1 : 0;
	for (node = parent->first_child; node != NULL; node = node->next) {
		n++;
	}

	return n;
}

static gboolean
fm_tree_model_iter_nth_child (GtkTreeModel *model, GtkTreeIter *iter,
				    GtkTreeIter *parent_iter, int n)
{
	FMTreeModel *tree_model;
	TreeNode *parent, *node;
	int i;
	
	g_return_val_if_fail (FM_IS_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (parent_iter == NULL
			      || iter_is_valid (FM_TREE_MODEL (model), parent_iter), FALSE);
	
	tree_model = FM_TREE_MODEL (model);

	if (parent_iter == NULL) {
		node = tree_model->details->root_node;
		for (i = 0; i < n && node != NULL; i++, node = node->next);
		return make_iter_for_node (node, iter,
		                           tree_model->details->stamp);
	}

	parent = parent_iter->user_data;
	if (parent == NULL) {
		return make_iter_invalid (iter);
	}

	i = tree_node_has_dummy_child (parent) ? 1 : 0;
	if (n == 0 && i == 1) {
		return make_iter_for_dummy_row (parent, iter, parent_iter->stamp);
	}
	for (node = parent->first_child; i != n; i++, node = node->next) {
		if (node == NULL) {
			return make_iter_invalid (iter);
		}
	}

	return make_iter_for_node (node, iter, parent_iter->stamp);	
}

static void
update_monitoring (FMTreeModel *model, TreeNode *node)
{
	TreeNode *child;

	if (node->all_children_ref_count == 0) {
		stop_monitoring_directory (model, node);
		destroy_children (model, node);
	} else {
		for (child = node->first_child; child != NULL; child = child->next) {
			update_monitoring (model, child);
		}
		start_monitoring_directory (model, node);
	}
}

static gboolean
update_monitoring_idle_callback (gpointer callback_data)
{
	FMTreeModel *model;
	TreeNode *node;

	model = FM_TREE_MODEL (callback_data);
	model->details->monitoring_update_idle_id = 0;
	for (node = model->details->root_node; node != NULL; node = node->next) { 
		update_monitoring (model, node);
	}
	return FALSE;
}

static void
schedule_monitoring_update (FMTreeModel *model)
{
	if (model->details->monitoring_update_idle_id == 0) {
		model->details->monitoring_update_idle_id =
			g_idle_add (update_monitoring_idle_callback, model);
	}
}

static void
stop_monitoring_directory_and_children (FMTreeModel *model, TreeNode *node)
{
	TreeNode *child;

	stop_monitoring_directory (model, node);
	for (child = node->first_child; child != NULL; child = child->next) {
		stop_monitoring_directory_and_children (model, child);
	}
}

static void
stop_monitoring (FMTreeModel *model)
{
	TreeNode *node;

	for (node = model->details->root_node; node != NULL; node = node->next) {
		stop_monitoring_directory_and_children (model, node);
	}
}

static void
fm_tree_model_ref_node (GtkTreeModel *model, GtkTreeIter *iter)
{
	TreeNode *node, *parent;
#ifdef LOG_REF_COUNTS
	char *uri;
#endif

	g_return_if_fail (FM_IS_TREE_MODEL (model));
	g_return_if_fail (iter_is_valid (FM_TREE_MODEL (model), iter));

	node = iter->user_data;
	if (node == NULL) {
		parent = iter->user_data2;
		g_assert (parent->dummy_child_ref_count >= 0);
		++parent->dummy_child_ref_count;
	} else {
		parent = node->parent;
		g_assert (node->ref_count >= 0);
		++node->ref_count;
	}

	if (parent != NULL) {
		g_assert (parent->all_children_ref_count >= 0);
		if (++parent->all_children_ref_count == 1) {
			if (parent->first_child == NULL) {
				parent->done_loading = FALSE;
			}
			schedule_monitoring_update (FM_TREE_MODEL (model));
		}
#ifdef LOG_REF_COUNTS
		uri = get_node_uri (iter);
		g_message ("ref of %s, count is now %d",
			   uri, parent->all_children_ref_count);
		g_free (uri);
#endif
	}
}

static void
fm_tree_model_unref_node (GtkTreeModel *model, GtkTreeIter *iter)
{
	TreeNode *node, *parent;
#ifdef LOG_REF_COUNTS
	char *uri;
#endif

	g_return_if_fail (FM_IS_TREE_MODEL (model));
	g_return_if_fail (iter_is_valid (FM_TREE_MODEL (model), iter));

	node = iter->user_data;
	if (node == NULL) {
		parent = iter->user_data2;
		g_assert (parent->dummy_child_ref_count > 0);
		--parent->dummy_child_ref_count;
	} else {
		parent = node->parent;
		g_assert (node->ref_count > 0);
		--node->ref_count;
	}

	if (parent != NULL) {
		g_assert (parent->all_children_ref_count > 0);
#ifdef LOG_REF_COUNTS
		uri = get_node_uri (iter);
		g_message ("unref of %s, count is now %d",
			   uri, parent->all_children_ref_count - 1);
		g_free (uri);
#endif
		if (--parent->all_children_ref_count == 0) {
			schedule_monitoring_update (FM_TREE_MODEL (model));
		}
	}
}

void
fm_tree_model_add_root_uri (FMTreeModel *model, const char *root_uri, const char *display_name, GIcon *icon, GMount *mount)
{
	NemoFile *file;
	TreeNode *node, *cnode;
	FMTreeModelRoot *newroot;
	
	file = nemo_file_get_by_uri (root_uri);

	newroot = tree_model_root_new (model);
	node = create_node_for_file (newroot, file);
	node->display_name = g_strdup (display_name);
	node->icon = g_object_ref (icon);
	if (mount) {
		node->mount = g_object_ref (mount);
	}
	newroot->root_node = node;
	node->parent = NULL;
	if (model->details->root_node == NULL) {
		model->details->root_node = node;
	} else {
		/* append it */
		for (cnode = model->details->root_node; cnode->next != NULL; cnode = cnode->next);
		cnode->next = node;
		node->prev = cnode;
	}

	nemo_file_unref (file);

	update_node_without_reporting (model, node);
	report_node_inserted (model, node);
}

GMount *
fm_tree_model_get_mount_for_root_node_file (FMTreeModel *model, NemoFile *file)
{
	TreeNode *node;

	for (node = model->details->root_node; node != NULL; node = node->next) {
		if (file == node->file) {
			break;
		}
	}

	if (node) {
		return node->mount;
	}

	return NULL;
}

void
fm_tree_model_remove_root_uri (FMTreeModel *model, const char *uri)
{
	TreeNode *node;
	GtkTreePath *path;
	FMTreeModelRoot *root;
	NemoFile *file;

	file = nemo_file_get_by_uri (uri);
	for (node = model->details->root_node; node != NULL; node = node->next) {
		if (file == node->file) {
			break;
		}
	}
	nemo_file_unref (file);

	if (node) {
		/* remove the node */
		
		if (node->mount) {
			g_object_unref (node->mount);
			node->mount = NULL;
		}

		nemo_file_monitor_remove (node->file, model);
		path = get_node_path (model, node);

		/* Report row_deleted before actually deleting */
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
		gtk_tree_path_free (path);
		
		if (node->prev) {
			node->prev->next = node->next;
		}
		if (node->next) {
			node->next->prev = node->prev;
		}
		if (node == model->details->root_node) {
			model->details->root_node = node->next;
		}
		
		/* destroy the root identifier */
		root = node->root;
		destroy_node_without_reporting (model, node);
		g_hash_table_destroy (root->file_to_node_map);
		g_free (root);
	}
}

FMTreeModel *
fm_tree_model_new (void)
{
	FMTreeModel *model;

	model = g_object_new (FM_TYPE_TREE_MODEL, NULL);
	
	return model;
}

void
fm_tree_model_set_show_hidden_files (FMTreeModel *model,
					   gboolean show_hidden_files)
{
	g_return_if_fail (FM_IS_TREE_MODEL (model));
	g_return_if_fail (show_hidden_files == FALSE || show_hidden_files == TRUE);

	show_hidden_files = show_hidden_files != FALSE;
	if (model->details->show_hidden_files == show_hidden_files) {
		return;
	}
	model->details->show_hidden_files = show_hidden_files;
	stop_monitoring (model);
	if (!show_hidden_files) {
		destroy_by_function (model, nemo_file_is_hidden_file);
	}
	schedule_monitoring_update (model);
}

static gboolean
file_is_not_directory (NemoFile *file)
{
	return !nemo_file_is_directory (file);
}

void
fm_tree_model_set_show_only_directories (FMTreeModel *model,
					       gboolean show_only_directories)
{
	g_return_if_fail (FM_IS_TREE_MODEL (model));
	g_return_if_fail (show_only_directories == FALSE || show_only_directories == TRUE);

	show_only_directories = show_only_directories != FALSE;
	if (model->details->show_only_directories == show_only_directories) {
		return;
	}
	model->details->show_only_directories = show_only_directories;
	stop_monitoring (model);
	if (show_only_directories) {
		destroy_by_function (model, file_is_not_directory);
	}
	schedule_monitoring_update (model);
}

NemoFile *
fm_tree_model_iter_get_file (FMTreeModel *model, GtkTreeIter *iter)
{
	TreeNode *node;

	g_return_val_if_fail (FM_IS_TREE_MODEL (model), NULL);
	g_return_val_if_fail (iter_is_valid (FM_TREE_MODEL (model), iter), NULL);

	node = iter->user_data;
	return node == NULL ? NULL : nemo_file_ref (node->file);
}

/* This is used to work around some sort order stability problems
   with gtktreemodelsort */
int
fm_tree_model_iter_compare_roots (FMTreeModel *model,
				  GtkTreeIter *iter_a,
				  GtkTreeIter *iter_b)
{
	TreeNode *a, *b, *n;

	g_return_val_if_fail (FM_IS_TREE_MODEL (model), 0);
	g_return_val_if_fail (iter_is_valid (model, iter_a), 0);
	g_return_val_if_fail (iter_is_valid (model, iter_b), 0);
	
	a = iter_a->user_data;
	b = iter_b->user_data;
	
	g_assert (a != NULL && a->parent == NULL);
	g_assert (b != NULL && b->parent == NULL);

	if (a == b) {
		return 0;
	}
	
	for (n = model->details->root_node; n != NULL; n = n->next) {
		if (n == a) {
			return -1;
		}
		if (n == b) {
			return 1;
		}
	}
	g_assert_not_reached ();
}

gboolean
fm_tree_model_iter_is_root (FMTreeModel *model, GtkTreeIter *iter)
{
	TreeNode *node;

	g_return_val_if_fail (FM_IS_TREE_MODEL (model), 0);
	g_return_val_if_fail (iter_is_valid (model, iter), 0);
	node = iter->user_data;
	if (node == NULL) {
		return FALSE;
	} else {
		return (node->parent == NULL);
	}
}

gboolean
fm_tree_model_file_get_iter (FMTreeModel *model,
				   GtkTreeIter *iter,
				   NemoFile *file,
				   GtkTreeIter *current_iter)
{
	TreeNode *node, *root_node;

	if (current_iter != NULL && current_iter->user_data != NULL) {
		node = get_node_from_file (((TreeNode *) current_iter->user_data)->root, file);
		return make_iter_for_node (node, iter, model->details->stamp);
	}

	for (root_node = model->details->root_node; root_node != NULL; root_node = root_node->next) {
		node = get_node_from_file (root_node->root, file);
		if (node != NULL) {
			return make_iter_for_node (node, iter, model->details->stamp);
		}
	}
	return FALSE;
}

static void
do_update_node (NemoFile *file,
                  FMTreeModel *model)
{
	TreeNode *root, *node = NULL;

	for (root = model->details->root_node; root != NULL; root = root->next) {
		node = get_node_from_file (root->root, file);

		if (node != NULL) {
			break;
		}
	}

	if (node == NULL) {
		return;
	}

	update_node (model, node);
}

void
fm_tree_model_set_highlight_for_files (FMTreeModel *model,
                                       GList *files)
{
	GList *old_files;

	if (model->details->highlighted_files != NULL) {
		old_files = model->details->highlighted_files;
		model->details->highlighted_files = NULL;

		g_list_foreach (old_files,
		                (GFunc) do_update_node, model);

		nemo_file_list_free (old_files);
	}

	if (files != NULL) {
		model->details->highlighted_files = 
			nemo_file_list_copy (files);
		g_list_foreach (model->details->highlighted_files,
		                (GFunc) do_update_node, model);
	}
}

static void
fm_tree_model_init (FMTreeModel *model)
{
	model->details = g_new0 (FMTreeModelDetails, 1);

	do {
		model->details->stamp = g_random_int ();
	} while (model->details->stamp == 0);
}

static void
fm_tree_model_finalize (GObject *object)
{
	FMTreeModel *model;
	TreeNode *root_node, *next_root;
	FMTreeModelRoot *root;

	model = FM_TREE_MODEL (object);

	for (root_node = model->details->root_node; root_node != NULL; root_node = next_root) {
		next_root = root_node->next;
		root = root_node->root;
		destroy_node_without_reporting (model, root_node);
		g_hash_table_destroy (root->file_to_node_map);
		g_free (root);
	}

	if (model->details->monitoring_update_idle_id != 0) {
		g_source_remove (model->details->monitoring_update_idle_id);
	}

	if (model->details->highlighted_files != NULL) {
		nemo_file_list_free (model->details->highlighted_files);
	}

	g_free (model->details);

	G_OBJECT_CLASS (fm_tree_model_parent_class)->finalize (object);
}

static void
fm_tree_model_class_init (FMTreeModelClass *class)
{
	G_OBJECT_CLASS (class)->finalize = fm_tree_model_finalize;

	tree_model_signals[ROW_LOADED] =
		g_signal_new ("row_loaded",
                      FM_TYPE_TREE_MODEL,
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (FMTreeModelClass, row_loaded),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__BOXED,
                      G_TYPE_NONE, 1,
                      GTK_TYPE_TREE_ITER);
}

static void
fm_tree_model_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags = fm_tree_model_get_flags;
	iface->get_n_columns = fm_tree_model_get_n_columns;
	iface->get_column_type = fm_tree_model_get_column_type;
	iface->get_iter = fm_tree_model_get_iter;
	iface->get_path = fm_tree_model_get_path;
	iface->get_value = fm_tree_model_get_value;
	iface->iter_next = fm_tree_model_iter_next;
	iface->iter_children = fm_tree_model_iter_children;
	iface->iter_has_child = fm_tree_model_iter_has_child;
	iface->iter_n_children = fm_tree_model_iter_n_children;
	iface->iter_nth_child = fm_tree_model_iter_nth_child;
	iface->iter_parent = fm_tree_model_iter_parent;
	iface->ref_node = fm_tree_model_ref_node;
	iface->unref_node = fm_tree_model_unref_node;
}



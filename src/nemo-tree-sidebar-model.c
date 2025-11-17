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
#include <libnemo-private/nemo-file-utilities.h>

#include <cairo-gobject.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>


enum {
  ROW_LOADED,
  GET_ICON_SCALE,
  LAST_SIGNAL
};

static guint tree_model_signals[LAST_SIGNAL] = { 0 };

typedef gboolean (* FilePredicate) (NemoFile *);

/* The user_data of the GtkTreeIter is the TreeNode pointer.
 * It's NULL for the dummy node. If it's NULL, then user_data2
 * is the TreeNode pointer to the parent.
 */

#define ISROOTNODE(node) (node->parent == NULL || node->parent->isheadnode)

typedef struct TreeNode TreeNode;
typedef struct FMTreeModelRoot FMTreeModelRoot;

struct TreeNode {
	/* part of this node for the file itself */
	int ref_count;

	NemoFile *file;
	char *display_name;
	GIcon *icon;
	GMount *mount;
	GIcon *closed_icon;
	GIcon *open_icon;
	PangoStyle font_style;
	int text_weight;
	gboolean text_weight_override;

	FMTreeModelRoot *root;

	TreeNode *parent;
	TreeNode *next;
	TreeNode *prev;

	/* part of the node used only for directories */
	int dummy_child_ref_count;
	int all_children_ref_count;
    guint icon_scale;
	NemoDirectory *directory;
	guint done_loading_id;
	guint files_added_id;
	guint files_changed_id;

	TreeNode *first_child;
	gboolean isheadnode;
	GValue *extra_values;
	int num_extra_values;

	/* misc. flags */
	guint done_loading : 1;
	guint is_empty : 1;
	guint force_has_dummy : 1;
	guint inserted : 1;
    guint pinned : 1;
    guint fav_unavailable : 1;
};

struct FMTreeModelDetails {
	int stamp;

	TreeNode *head_root_node;  // New top-level root node
	TreeNode *root_node;

	guint monitoring_update_idle_id;

	gboolean show_hidden_files;
	gboolean show_only_directories;

	GList *highlighted_files;

	GType *column_types;
	int    num_columns;

	gboolean (*custom_row_draggable_func)(GtkTreeDragSource *drag_source, GtkTreePath *path, gpointer user_data);
    	gpointer custom_row_draggable_data;  // Custom data for the callback
};

struct FMTreeModelRoot {
	FMTreeModel *model;

	/* separate hash table for each root node needed */
	GHashTable *file_to_node_map;
	guint icon_scale;
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
static GtkTreePath *fm_tree_model_get_path(GtkTreeModel *model, GtkTreeIter *iter);
static int fm_tree_model_iter_n_children (GtkTreeModel *model, GtkTreeIter *iter);
static void fm_tree_model_drag_source_init (GtkTreeDragSourceIface *iface);

G_DEFINE_TYPE_WITH_CODE (FMTreeModel, fm_tree_model, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
						fm_tree_model_tree_model_init)
		         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
						fm_tree_model_drag_source_init)

);

static GtkTreeModelFlags
fm_tree_model_get_flags (GtkTreeModel *tree_model)
{
	return GTK_TREE_MODEL_ITERS_PERSIST;
}

static gint
fm_tree_model_get_icon_scale (GtkTreeModel *model)
{
    gint retval = -1;

    g_signal_emit (model, tree_model_signals[GET_ICON_SCALE], 0,
               &retval);

    if (retval == -1) {
        retval = gdk_screen_get_monitor_scale_factor (gdk_screen_get_default (), 0);
    }

    return retval;
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
	root->file_to_node_map = g_hash_table_new (g_direct_hash, g_direct_equal);
	root->icon_scale = fm_tree_model_get_icon_scale (GTK_TREE_MODEL (model));

	return root;
}

static TreeNode *
tree_node_new (NemoFile *file, FMTreeModelRoot *root)
{
	TreeNode *node;

	node = g_new0 (TreeNode, 1);
	node->isheadnode = FALSE;
	node->file = nemo_file_ref (file);
	node->root = root;
	node->icon_scale = root->icon_scale;
	node->extra_values = NULL;
	node->num_extra_values = 0;
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
tree_model_root_free (FMTreeModelRoot *root)
{
    if (!root) return;
    if (root->file_to_node_map) {
        g_hash_table_destroy (root->file_to_node_map);
        root->file_to_node_map = NULL;
    }
    g_free (root);
}

static void
tree_node_destroy (FMTreeModel *model, TreeNode *node)
{
	g_assert (node->first_child == NULL);
	g_assert (node->ref_count == 0);

	if (node->root && node->root->file_to_node_map && node->file) {
        	g_hash_table_remove(node->root->file_to_node_map, node->file);
    	}

	tree_node_unparent (model, node);

	object_unref_if_not_NULL (node->file); 		node->file=NULL;
	g_free (node->display_name); 			node->display_name=NULL;
	object_unref_if_not_NULL (node->icon); 		node->icon = NULL;
	object_unref_if_not_NULL (node->closed_icon); 	node->closed_icon=NULL;
	object_unref_if_not_NULL (node->open_icon);	node->open_icon=NULL;

	g_assert (node->done_loading_id == 0);
	g_assert (node->files_added_id == 0);
	g_assert (node->files_changed_id == 0);
	nemo_directory_unref (node->directory);
	if (node->extra_values) {
		for (int i = 0; i < node->num_extra_values; i++) {
		    g_value_unset(&node->extra_values[i]);
		}
		g_free(node->extra_values);
		node->extra_values = NULL;
		node->num_extra_values = 0;
	}
	g_free (node);
}

static void
tree_node_parent (TreeNode *node, TreeNode *parent)
{
    g_assert (parent != NULL);
    g_assert (node->parent == NULL);
    g_assert (node->prev == NULL);
    g_assert (node->next == NULL);

    node->parent = parent;
    node->root = parent->root;
    node->parent->is_empty = FALSE;

    if (parent->first_child == NULL) {
        parent->first_child = node;
    } else {
        TreeNode *last_node = parent->first_child;
        while (last_node->next != NULL) {
            last_node = last_node->next;
        }
        last_node->next = node;
        node->prev = last_node;
    }
}


static GIcon *
get_menu_icon_for_file (TreeNode *node,
                        NemoFile *file,
                        NemoFileIconFlags flags)
{
    NemoFile *parent_file;
	GIcon *gicon, *emblem_icon, *emblemed_icon;
	GEmblem *emblem;
//	int size;
	GList *emblem_icons, *l;

	//size = nemo_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
	//gicon = G_ICON (nemo_file_get_icon_pixbuf (file, size, TRUE, node->icon_scale, flags));
	gicon = nemo_file_get_gicon(file, flags);

    parent_file = NULL;

	if (node->parent && node->parent->file) {
        parent_file = node->parent->file;
	}

	emblem_icons = nemo_file_get_emblem_icons (node->file, parent_file);

	/* pick only the first emblem we can render for the tree view */
	for (l = emblem_icons; l != NULL; l = l->next) {
		emblem_icon = l->data;
		emblem = g_emblem_new (emblem_icon);
		emblemed_icon = g_emblemed_icon_new (gicon, emblem);

		g_object_unref (gicon);
		g_object_unref (emblem);
		gicon = emblemed_icon;

		break;
	}

	g_list_free_full (emblem_icons, g_object_unref);

    if (gicon)
        return g_object_ref (gicon);
    return NULL;
}

static GIcon *
tree_node_get_icon (TreeNode *node,
                    NemoFileIconFlags flags)
{
	if (ISROOTNODE(node)) {
		return node->icon ? g_object_ref(node->icon) : NULL;
	}
	return get_menu_icon_for_file (node, node->file, flags);
}

static gboolean
tree_node_update_icon (TreeNode *node,
                       GIcon **icon_storage,
                       NemoFileIconFlags flags)
{
    GIcon *new_icon = tree_node_get_icon (node, flags); /* liefert eine new ref (oder NULL) */

    /* If both NULL -> no change */
    if (new_icon == NULL && *icon_storage == NULL) {
        return FALSE;
    }

    /* pointer-equality is fine as heuristic (same object pointer) */
    if (new_icon == *icon_storage) {
        if (new_icon)
            g_object_unref (new_icon); /* we got a ref from get_icon, drop it */
        return FALSE;
    }

    /* replace: unref old storage, store new ref */
    if (*icon_storage) {
        g_object_unref (*icon_storage);
    }
    *icon_storage = new_icon; /* take ownership */

    return TRUE;
}

static gboolean
tree_node_update_closed_icon (TreeNode *node)
{
	return tree_node_update_icon (node, &node->closed_icon, 0);
}

static gboolean
tree_node_update_open_icon (TreeNode *node)
{
	return tree_node_update_icon (node, &node->open_icon, NEMO_FILE_ICON_FLAGS_FOR_OPEN_FOLDER);
}

static gboolean
tree_node_update_display_name (TreeNode *node)
{
	char *display_name;

	if (node->display_name == NULL) {
		return FALSE;
	}
	/* don't update root node display names */
	if (ISROOTNODE(node)) {
		return FALSE;
	}
	display_name = nemo_file_get_display_name (node->file);
	if (strcmp (display_name, node->display_name) == 0) {
		g_free (display_name);
		return FALSE;
	}
	g_free (node->display_name);
	node->display_name = display_name;
	return TRUE;
}

static gboolean
tree_node_update_pinning (TreeNode *node)
{
    gboolean file_value;

    file_value = nemo_file_get_pinning (node->file);

    if (file_value != node->pinned) {
        node->pinned = file_value;

        return TRUE;
    }

    return FALSE;
}

static gboolean
tree_node_update_fav_unavailable (TreeNode *node)
{
    gboolean file_value;

    file_value = nemo_file_is_unavailable_favorite (node->file);

    if (file_value != node->fav_unavailable) {
        node->fav_unavailable = file_value;

        return TRUE;
    }

    return FALSE;
}

static GIcon *
tree_node_get_closed_icon (TreeNode *node)
{
	if (node->closed_icon == NULL) {
		node->closed_icon = tree_node_get_icon (node, 0);
	}
	return node->closed_icon;
}

static GIcon *
tree_node_get_open_icon (TreeNode *node)
{
	if (node->open_icon == NULL) {
		node->open_icon = tree_node_get_icon (node, NEMO_FILE_ICON_FLAGS_FOR_OPEN_FOLDER);
	}
	return node->open_icon;
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
#if 0
	return (node != NULL && !node->isheadnode &&
	       ((node->directory != NULL && (!node->done_loading || node->first_child == NULL || node->force_has_dummy)) ||
		(node->directory == NULL && ISROOTNODE(node))) );
#else
  // same code as above
	if (!node) return FALSE;


        // 1) node is a directory
	if (node->directory != NULL) {
		// contains a dummy if:
		// - not loading, or
		// - no child exists or
		// - a dummy is forced

	    if (!node->done_loading || node->first_child == NULL || node->force_has_dummy) {
		return TRUE;
	    }
	}

	// head_root_node has file == NULL; we don't want him to be used not loaded
	if (node->isheadnode)
		return FALSE;

	// 2) node is a root node without directory
	if (node->directory == NULL && (ISROOTNODE(node))) {
		return TRUE;
	}

	return FALSE;

#endif
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
    if (root == NULL || root->file_to_node_map == NULL || file == NULL) {
        return NULL;
    }

    return g_hash_table_lookup(root->file_to_node_map, file);
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

static void
tree_node_init_extra_columns (TreeNode *node, FMTreeModel *model)
{
    int base = FM_TREE_MODEL_NUM_COLUMNS;
    int extra = model->details->num_columns - base;

    if (extra <= 0) {
        node->extra_values = NULL;
        return;
    }
    // free if extra_values already exists
    if (node->extra_values) {
        for (int i = 0; i < node->num_extra_values; i++) {
            g_value_unset(&node->extra_values[i]);
        }
        g_free(node->extra_values);
        node->extra_values = NULL;
        node->num_extra_values = 0;
    }

    node->extra_values = g_new0(GValue, extra);
    node->num_extra_values = extra;

    for (int i = 0; i < extra; i++) {
        g_value_init(&node->extra_values[i],
                     model->details->column_types[base + i]);
    }
}

static void
tree_node_init_extra_columns_recursive(TreeNode *node, FMTreeModel *model)
{
    for (TreeNode *n = node; n; n = n->next) {
        tree_node_init_extra_columns(n, model);
        if (n->first_child)
            tree_node_init_extra_columns_recursive(n->first_child, model);
    }
}

void
fm_tree_model_set_column_types(FMTreeModel *model, int new_count, const GType *types)
{
    g_return_if_fail(FM_IS_TREE_MODEL(model));
    g_return_if_fail(types != NULL);
    g_return_if_fail(new_count > 0);

    // Free old columns if they exist
    if (model->details->column_types)
        g_free(model->details->column_types);

    // Allocate new columns (correct)
    model->details->column_types = g_new0(GType, new_count);

    memcpy(model->details->column_types, types, sizeof(GType) * new_count);

    model->details->num_columns = new_count;

    /* EXTEND EXISTING NODES */
    for (TreeNode *n = model->details->head_root_node; n; n = n->next)
        tree_node_init_extra_columns_recursive(n, model);

    for (TreeNode *r = model->details->root_node; r; r = r->next)
        tree_node_init_extra_columns_recursive(r, model);

}


static TreeNode *
create_node_for_file (FMTreeModelRoot *root, NemoFile *file)
{
	TreeNode *node;

	g_assert (get_node_from_file (root, file) == NULL);
	node = tree_node_new (file, root);
	g_hash_table_insert (root->file_to_node_map, node->file, node);
	tree_node_init_extra_columns(node, root->model);
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
    if (iter == NULL || iter->stamp != model->details->stamp) {
        g_warning("Invalid iterator");
        return;
    }

	GtkTreePath *path;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
    if (path == NULL) {
        g_warning("Failed to get path for iterator");
        return;
    }

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

    if (node == NULL) {
        g_warning("Node is NULL");
        return NULL;
    }

    if (!make_iter_for_node (node, &iter, model->details->stamp)) {
        g_warning("Failed to make iterator for node");
        return NULL;
    }

    return fm_tree_model_get_path(GTK_TREE_MODEL(model), &iter);
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
        guint count;
        if (nemo_file_get_directory_item_count (node->file, &count, NULL)) {
            add_child = count > 0 || ISROOTNODE(node);
        } else {
            add_child = TRUE;
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
	if (node == NULL) {
		g_warning("Node is NULL");
		return;
    	}
	abandon_node_ref_count (model, node);
	stop_monitoring_directory (model, node);
	node->inserted = FALSE;
	destroy_children_without_reporting (model, node);

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
	TreeNode *head;
	for (head = model->details->head_root_node; head != NULL; head = head->next) {
	   for (node = head->first_child; node != NULL; node = node->next) {
		destroy_children_by_function (model, node, f);
	   }
	}
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
	    (nemo_file_is_directory(node->file) || ISROOTNODE(node))) {
		node->directory = nemo_directory_get_for_file (node->file);
	} else if (node->directory != NULL &&
		   !(nemo_file_is_directory (node->file) || ISROOTNODE(node))) {
		stop_monitoring_directory (model, node);
		destroy_children (model, node);
		nemo_directory_unref (node->directory);
		node->directory = NULL;
	}

	changed |= tree_node_update_display_name (node);
	changed |= tree_node_update_closed_icon (node);
    changed |= tree_node_update_open_icon (node);
    changed |= tree_node_update_pinning (node);
    changed |= tree_node_update_fav_unavailable (node);

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
	TreeNode *head;
	for (head = model->details->head_root_node; head != NULL; head = head->next) {
	   for (node = head->first_child; node != NULL; node = node->next) {
		if (!should && node != NULL && file == node->file) {
			return TRUE;
		}
	   }
	}
	for (node = model->details->root_node; node != NULL; node = node->next) {
		if (!should && node != NULL && file == node->file) {
			return TRUE;
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

	// Check if the node has no children
	if (node->first_child == NULL) {
		// Inform GTK that the node has no children
		if (tree_node_has_dummy_child(node)) {
		    GtkTreeIter dummy_iter;
		    make_iter_for_dummy_row(node, &dummy_iter, root->model->details->stamp);
		    GtkTreePath *dummy_path = gtk_tree_model_get_path(GTK_TREE_MODEL(root->model), &dummy_iter);
		    gtk_tree_model_row_deleted(GTK_TREE_MODEL(root->model), dummy_path);
		    gtk_tree_path_free(dummy_path);
		}
		node->is_empty = TRUE;
		update_node(root->model, node);
		make_iter_for_node(node, &iter, root->model->details->stamp);
		report_row_has_child_toggled(root->model, &iter);
	}

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
	FMTreeModel *fm_model = FM_TREE_MODEL(model);
	return fm_model->details->num_columns;
}

static GType
fm_tree_model_get_column_type(GtkTreeModel *model, int index) {
    FMTreeModel *fm_model = FM_TREE_MODEL(model);

    if (index < 0 || index >= fm_model->details->num_columns) {
        return G_TYPE_INVALID;
    }
    return fm_model->details->column_types[index];
}

gboolean
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
			if (!parent->isheadnode && !NEMO_IS_FILE (parent->file)) {
				return FALSE;
			}
			if (!tree_node_has_dummy_child (parent)) {
				return FALSE;
			}
		}
	} else {
		if (node->isheadnode) {
			return TRUE;
		}
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


static int
get_top_level_index(FMTreeModel *model, TreeNode *node)
{
    int i = 0;
    for (TreeNode *n = model->details->head_root_node; n != NULL; n = n->next, i++) {
        if (n == node) return i;
    }
    for (TreeNode *n = model->details->root_node; n != NULL; n = n->next) {
        if (n == node) return i;
	if(n->parent == NULL) i++;
    }
    return -1; // Node not found
}

static GtkTreePath *
fm_tree_model_get_path (GtkTreeModel *model, GtkTreeIter *iter)
{
	FMTreeModel *tree_model;
	TreeNode *node, *parent;
	GtkTreePath *path;
	GtkTreeIter parent_iter;

	g_return_val_if_fail (FM_IS_TREE_MODEL (model), NULL);
	tree_model = FM_TREE_MODEL (model);
	g_return_val_if_fail (iter_is_valid (tree_model, iter), NULL);

	node = iter->user_data;

        // if the iterator is a dummy node
	if (node == NULL) {
		parent = iter->user_data2;
		if (parent == NULL) {
			return gtk_tree_path_new ();
		}
	}  else {
	    parent = node->parent;
	    if (parent == NULL) {
		int index = get_top_level_index(tree_model, node);
		path = gtk_tree_path_new();
		gtk_tree_path_append_index(path, index);
		return path;
	    }
	}
	if (parent == NULL) {
		g_warning("Failed parent==NULL");
	}

	/* Recursively get path of parent */
	parent_iter.stamp = iter->stamp;
	parent_iter.user_data = parent;
	parent_iter.user_data2 = NULL;
	parent_iter.user_data3 = NULL;

	path = fm_tree_model_get_path (model, &parent_iter);
	if (path == NULL) {
		g_warning("Failed to get path for parent node");
		return NULL;
	}

	gtk_tree_path_append_index (path, tree_node_get_child_index (parent, node));

	return path;
}

void
fm_tree_model_set (FMTreeModel *model, GtkTreeIter *iter, ...)
{
    TreeNode *node;
    va_list args;
    int column;

    g_return_if_fail(FM_IS_TREE_MODEL(model));
    g_return_if_fail(iter_is_valid(model, iter));

    node = iter->user_data;
    if (!node)
        return;

    va_start(args, iter);

    while ((column = va_arg(args, int)) != -1) {
        switch (column) {

        case FM_TREE_MODEL_DISPLAY_NAME_COLUMN: {
            const char *str = va_arg(args, const char *);
            g_free(node->display_name);
            node->display_name = g_strdup(str);
            break;
        }

        case FM_TREE_MODEL_CLOSED_ICON_COLUMN: {
            GIcon *icon = va_arg(args, GIcon *);
            if (node->closed_icon)
                g_object_unref(node->closed_icon);
            node->closed_icon = icon ? g_object_ref(icon) : NULL;
            break;
        }

        case FM_TREE_MODEL_OPEN_ICON_COLUMN: {
            GIcon *icon = va_arg(args, GIcon *);
            if (node->open_icon)
                g_object_unref(node->open_icon);
            node->open_icon = icon ? g_object_ref(icon) : NULL;
            break;
        }

        case FM_TREE_MODEL_FONT_STYLE_COLUMN:
            node->font_style = va_arg(args, PangoStyle);
            break;

        case FM_TREE_MODEL_TEXT_WEIGHT_COLUMN:
            node->text_weight = va_arg(args, int);
            node->text_weight_override = TRUE;
            break;

        default: {
                int dynamic_index = column - FM_TREE_MODEL_NUM_COLUMNS;
                if (dynamic_index >= 0 && dynamic_index < node->num_extra_values) {
                    GValue *dst = &node->extra_values[dynamic_index];
                    GType type = G_VALUE_TYPE(dst);

                    /* Initialize dst if not already done */
                    if (G_VALUE_TYPE(dst) == 0)
                        g_value_init(dst, type);

                    /* Set value from va_arg depending on type */
                    if (type == G_TYPE_STRING) {
                        const char *str = va_arg(args, const char *);
                        g_value_set_string(dst, str);
                    } else if (type == G_TYPE_INT) {
                        int v = va_arg(args, int);
                        g_value_set_int(dst, v);
                    } else if (type == G_TYPE_BOOLEAN) {
                        gboolean b = va_arg(args, int);
                        g_value_set_boolean(dst, b);
                    } else if (g_type_is_a(type, G_TYPE_OBJECT)) {
                        GObject *obj = va_arg(args, GObject *);
                        g_value_set_object(dst, obj ? G_OBJECT(obj) : NULL);
                    } else {
                        g_warning("fm_tree_model_set: unsupported dynamic column type %s",
                                  g_type_name(type));
                        /* Consume arg anyway to correctly advance va_list */
                        (void)va_arg(args, void *);
                    }
                } else {
                    g_warning("fm_tree_model_set: invalid column %d", column);
                }
                break;
            }
        }
    }

    va_end(args);

    /* Update TreeView */
    GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(model), iter);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(model), path, iter);
    gtk_tree_path_free(path);
}

gboolean      isDummyNode(GtkTreeIter *iter)
{
	if (!iter->user_data) {
		return TRUE;
	}
	return FALSE;
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
	case FM_TREE_MODEL_CLOSED_ICON_COLUMN:
		g_value_init (value, G_TYPE_ICON);
		g_value_set_object (value, node == NULL ? NULL : tree_node_get_closed_icon (node));
        	break;
	case FM_TREE_MODEL_OPEN_ICON_COLUMN:
		g_value_init (value, G_TYPE_ICON);
		g_value_set_object (value, node == NULL ? NULL : tree_node_get_open_icon (node));
		break;
	case FM_TREE_MODEL_FONT_STYLE_COLUMN:
		g_value_init (value, PANGO_TYPE_STYLE);
		if (node == NULL) {
			g_value_set_enum (value, PANGO_STYLE_ITALIC);
		} else {
			g_value_set_enum (value, PANGO_STYLE_NORMAL);
		}
		break;
	case FM_TREE_MODEL_TEXT_WEIGHT_COLUMN:
		g_value_init (value, G_TYPE_INT);

		if (node != NULL) {
		    if (node->fav_unavailable) {
			g_value_set_int (value, UNAVAILABLE_TEXT_WEIGHT);
		    }
		    else
		    if (node->pinned) {
			g_value_set_int (value, PINNED_TEXT_WEIGHT);
		    }
		    else {
			g_value_set_int (value, NORMAL_TEXT_WEIGHT);
		    }
		} else {
		    g_value_set_int (value, NORMAL_TEXT_WEIGHT);
		}

		break;
	default:
	    if (!node) {
		// dummy node
		FMTreeModel *m=(FMTreeModel *)model;
		if (column >= 0 && column < m->details->num_columns) {
			g_value_init(value, m->details->column_types[column]);
			return;
		 }
	         return;
	    } else {
		    /* --- Dynamic columns --- */
		    int dynamic_index = column - FM_TREE_MODEL_NUM_COLUMNS;
		    if (dynamic_index >= 0 && dynamic_index < node->num_extra_values) {
			GValue *val = &node->extra_values[dynamic_index];
			g_value_init(value, G_VALUE_TYPE(val));
			g_value_copy(val, value);
			return;
		    }
	    }
	    g_warning("fm_tree_model_get_value: invalid column %d", column);
	    g_value_init(value, G_TYPE_INVALID);
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
	if(parent_iter == NULL) {
	   FMTreeModel *tree_model = FM_TREE_MODEL(model);
	   parent = tree_model->details->head_root_node;
	   if(!parent)parent = tree_model->details->root_node;
	   return make_iter_for_node(parent, iter, tree_model->details->stamp);
	} else {
	   g_return_val_if_fail (iter_is_valid (FM_TREE_MODEL (model), parent_iter), FALSE);
	   parent = parent_iter->user_data;
	}


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
	if(!node) return FALSE;

	if(node->is_empty) return FALSE;

	has_child = node != NULL && (node->directory != NULL || ISROOTNODE(node));

	return has_child;
}

static int
fm_tree_model_iter_n_children (GtkTreeModel *model, GtkTreeIter *iter)
{
	FMTreeModel *tree_model;
	TreeNode *parent=NULL, *node;
	int n;

	g_return_val_if_fail (FM_IS_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (iter == NULL || iter_is_valid (FM_TREE_MODEL (model), iter), FALSE);

	tree_model = FM_TREE_MODEL(model);

	if (iter == NULL) {
		// If no iterator is given, we are at the top level
		int count = 0;
		for (node = tree_model->details->head_root_node; node != NULL; node = node->next) {
    			count++;
		}
		for (node = tree_model->details->root_node; node != NULL; node = node->next) {
			count++;
		}
		return count;
	}

	parent = iter->user_data;
	if (parent == NULL) {
		return 0;
	}

	// Check if the parent is a head node
	TreeNode *head_node;
	for (head_node = tree_model->details->head_root_node; head_node != NULL; head_node = head_node->next) {
		if (parent == head_node) {
		    n = 0;
		    for (node = head_node->first_child; node != NULL; node = node->next) {
			n++;
		    }
		    return n;
		}
	}

	 //  Default case: number of children of the parent
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
	int i=0;

	g_return_val_if_fail (FM_IS_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (parent_iter == NULL
			      || iter_is_valid (FM_TREE_MODEL (model), parent_iter), FALSE);

	tree_model = FM_TREE_MODEL (model);

	if (parent_iter == NULL) {
		// If no parent iterator is given, we are at the top level
		if(tree_model->details->head_root_node) {
		    // Check the head nodes
		    for (node = tree_model->details->head_root_node; node != NULL; node = node->next, i++) {
		        if (i == n) {
		            return make_iter_for_node(node, iter, tree_model->details->stamp);
		        }
		    }
		} else {
		    // Only regular root nodes
		    for (node = tree_model->details->root_node; node != NULL; node = node->next, i++) {
		        if (i == n) {
		            return make_iter_for_node(node, iter, tree_model->details->stamp);
		        }
		    }
		}
		return make_iter_invalid(iter);
	}

	parent = parent_iter->user_data;
	if (parent == NULL) {
		return make_iter_invalid (iter);
	}
	// Check if the parent is a head node
	TreeNode *head_node;
	for (head_node = tree_model->details->head_root_node; head_node != NULL; head_node = head_node->next) {
		if (parent == head_node) {
		    for (node = head_node->first_child; node != NULL; node = node->next, i++) {
			if (i == n) {
			    return make_iter_for_node(node, iter, parent_iter->stamp);
			}
		    }
		    return make_iter_invalid(iter);
		}
	}
	// Check if the parent is a dummy node
	if (tree_node_has_dummy_child(parent)) {
		if (n == 0) {
		    return make_iter_for_dummy_row(parent, iter, parent_iter->stamp);
		}
		n--;
	}
	// Default case: Check the children of the parent
	for (node = parent->first_child; node != NULL; node = node->next, i++) {
		if (i == n) {
		    return make_iter_for_node(node, iter, parent_iter->stamp);
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

	if(model->details->head_root_node) {
		TreeNode *head;
		for (head = model->details->head_root_node; head != NULL; head = head->next) {
		   for (node = head->first_child; node != NULL; node = node->next) {
			update_monitoring (model, node);
		   }
		}
	} //else {
		for (node = model->details->root_node; node != NULL; node = node->next) {
			update_monitoring (model, node);
		}
//	}
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

	if(model->details->head_root_node) {
		TreeNode *head;
		for (head = model->details->head_root_node; head != NULL; head = head->next) {
		   for (node = head->first_child; node != NULL; node = node->next) {
			stop_monitoring_directory_and_children (model, node);
		   }
		}
	} else {
		for (node = model->details->root_node; node != NULL; node = node->next) {
			stop_monitoring_directory_and_children (model, node);
		}
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

gboolean
fm_tree_model_append_head_root_node(FMTreeModel *model, const char *nodeName, GtkTreeIter *iter)
{
    if (!FM_IS_TREE_MODEL(model) || nodeName == NULL || iter == NULL) {
        if(iter)make_iter_invalid(iter);
        return FALSE;
    }

    /* Create node */
    TreeNode *node = g_new0 (TreeNode, 1);
    node->ref_count = 1;
    node->display_name = g_strdup(nodeName);
    node->file = NULL;
    node->parent = NULL;
    node->root = NULL;
    node->isheadnode = TRUE;    /*defines head nodes just for organization purpose (2nd level is possible)*/
    node->inserted = FALSE; /* default */
    tree_node_init_extra_columns(node, model);

    /* Append into linked list of head nodes */
    if (model->details->head_root_node == NULL) {
        model->details->head_root_node = node;
    } else {
        TreeNode *last_head_node;
        for (last_head_node = model->details->head_root_node;
             last_head_node->next != NULL;
             last_head_node = last_head_node->next) {}
        last_head_node->next = node;
        node->prev = last_head_node;
    }

    /* --- Notify GTK: build child-path (index among top-level entries) --- */

    /* Compute the top-level index for the new node.
       Important: if your top-level sequence is head_nodes followed by root_node list,
       this index is just the position in the head list (0..n-1). */
    int index = 0;
    for (TreeNode *t = model->details->head_root_node; t != NULL; t = t->next) {
        if (t == node)
            break;
        index++;
    }

    /* Create a one-element path [index] */
    GtkTreePath *path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, index);

    /* Make a valid iter for the node (this sets iter->stamp etc.) */
    if (!make_iter_for_node(node, iter, model->details->stamp)) {
        /* defensive: shouldn't happen */
        gtk_tree_path_free(path);
	tree_node_destroy (model, node);
	make_iter_invalid(iter);
        return FALSE;;
    }

    /* Mark node inserted in your model state (if your code relies on this) */
    node->inserted = TRUE;

    /* Emit the row-inserted signal on the child model;
       the GtkTreeModelSort above will hear this and update itself. */
    gtk_tree_model_row_inserted(GTK_TREE_MODEL(model), path, iter);

    gtk_tree_path_free(path);

    return TRUE;
}

gboolean
fm_tree_model_append_child_node(FMTreeModel *model,
                                GtkTreeIter *parent_iter,
                                GtkTreeIter *new_iter)
{

    /* Default: return invalid if something goes wrong */

    g_return_val_if_fail(new_iter !=NULL, FALSE);
    g_return_val_if_fail(parent_iter != NULL, FALSE);

    TreeNode *parent = parent_iter->user_data;
    if (parent == NULL) {
        g_warning("fm_tree_model_append_child_node: invalid parent_iter");
        return FALSE;
    }

    /* --- Create new child --- */
    TreeNode *node = g_new0(TreeNode, 1);
    node->ref_count = 1;
    node->display_name = NULL;
    node->file = NULL;
    node->root = NULL;
    node->parent = parent;
    node->isheadnode = TRUE;
    node->inserted = FALSE;
    tree_node_init_extra_columns(node, model);

    /* --- Insert into child list --- */
    if (parent->first_child == NULL) {
        parent->first_child = node;
    } else {
        TreeNode *last = parent->first_child;
        while (last->next)
            last = last->next;
        last->next = node;
        node->prev = last;
    }

    /* --- Determine index among siblings --- */
    int index = 0;
    for (TreeNode *t = parent->first_child; t != NULL; t = t->next) {
        if (t == node)
            break;
        index++;
    }

    /* --- TreePath of the parent node + index --- */
    GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(model), parent_iter);
    gtk_tree_path_append_index(path, index);

    /* --- Create iterator --- */
    if (!make_iter_for_node(node, new_iter, model->details->stamp)) {
	/* defensive: shouldn't happen */
	tree_node_destroy (model, node);
        gtk_tree_path_free(path);
        g_free(node);
        return FALSE;
    }

    /* --- Mark as inserted and send signal --- */
    node->inserted = TRUE;
    gtk_tree_model_row_inserted(GTK_TREE_MODEL(model), path, new_iter);

    /* --- Notify the TreeView that the parent now has children after insertion --- */
    GtkTreePath *parent_path = gtk_tree_model_get_path(GTK_TREE_MODEL(model), parent_iter);
    gtk_tree_model_row_has_child_toggled(GTK_TREE_MODEL(model), parent_path, parent_iter);


    gtk_tree_path_free(parent_path);
    gtk_tree_path_free(path);
    return TRUE;
}


void
fm_tree_model_add_root_uri (FMTreeModel *model, const char *root_uri, const char *display_name, GIcon *icon, GMount *mount)
{
	GtkTreeIter child_iter;
	fm_tree_model_add_root_uri_head(model, root_uri, NULL, &child_iter, display_name, icon, mount);
}


gboolean
fm_tree_model_add_root_uri_head (FMTreeModel *model, const char *root_uri, GtkTreeIter *parent_iter, GtkTreeIter *child_iter,
				 const char *display_name, GIcon *icon, GMount *mount)
{
	NemoFile *file=NULL;
	TreeNode *node, *cnode;
	FMTreeModelRoot *newroot;

	if (!FM_IS_TREE_MODEL(model) || root_uri == NULL || child_iter == NULL) {
		if(child_iter)make_iter_invalid(child_iter);
		return FALSE;
	}

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

	if(parent_iter) {
	  /* Add the new node as a child of the head_root_node */
	  TreeNode *head_node = parent_iter->user_data;
	  if (!head_node) {
	      g_warning("head_root_node not found in model->details->head_root_node!");
	      g_free(node->display_name);
	      g_object_unref(node->icon);
	      if (mount) {
		g_object_unref(node->mount);
	      }
	      tree_model_root_free (newroot);
	      nemo_file_unref(file);
	      return FALSE;
	  }
	  tree_node_parent (node, head_node);
	  /* Explicitly set the root pointer of the node to newroot */
	  node->root = newroot;

	  GtkTreeIter parent_iter;
	  if (!make_iter_for_node(head_node, &parent_iter, model->details->stamp)) return FALSE;
	//  GtkTreePath *parent_path = gtk_tree_path_new();
	  int index = get_top_level_index(model, head_node);
	  GtkTreePath *parent_path = gtk_tree_path_new_from_indices(index, -1);
	  gtk_tree_model_row_has_child_toggled(GTK_TREE_MODEL(model),
				            parent_path,
				            &parent_iter);
	  gtk_tree_path_free(parent_path);

	} else {

		if (model->details->root_node == NULL) {
			model->details->root_node = node;
		} else {
			/* append it */
			for (cnode = model->details->root_node; cnode->next != NULL; cnode = cnode->next);
			cnode->next = node;
			node->prev = cnode;
		}
	}

	nemo_file_unref (file);

	update_node_without_reporting (model, node);
	report_node_inserted (model, node);
	    /* Make a valid iter for the node (this sets iter->stamp etc.) */
	if (!make_iter_for_node(node, child_iter, model->details->stamp)) {
		/* defensive: shouldn't happen */
		make_iter_invalid(child_iter);
		return FALSE;;
	}
	return TRUE;
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
	TreeNode *head;
	for (head = model->details->head_root_node; head != NULL; head = head->next) {
	   for (node = head->first_child; node != NULL; node = node->next) {
		if (file == node->file) {
			return node->mount;
		}
	   }
	}

	return NULL;
}
static void
fm_tree_model_free_node(FMTreeModel *model, TreeNode *node)
{
	GtkTreePath *path;
	FMTreeModelRoot *root;
	if (node) {
		/* remove the node */

		if (node->mount) {
			g_object_unref (node->mount);
			node->mount = NULL;
		}

		if(node->file)nemo_file_monitor_remove (node->file, model);
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
	        if (node == model->details->head_root_node) {
			model->details->head_root_node = node->next;
		}

		/* destroy the root identifier */
		root = node->root;
		destroy_node_without_reporting (model, node);
		if(root) {
		  if(root->file_to_node_map)g_hash_table_destroy (root->file_to_node_map);
		  g_free (root);
		}
	}
}
static void
fm_tree_model_free_top_node(FMTreeModel *model, TreeNode *node)
{
	GtkTreePath *path;
	if (node) {
		/* remove the node */

		if (node->mount) {
			g_object_unref (node->mount);
			node->mount = NULL;
		}

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
	        if (node == model->details->head_root_node) {
			model->details->head_root_node = node->next;
		}

		/* destroy the root identifier */
		destroy_node_without_reporting (model, node);
	}
}

void
fm_tree_model_remove_all_nodes (FMTreeModel *model)
{
	TreeNode *head_node;
	if(model->details->head_root_node) {
	    for (head_node = model->details->head_root_node; head_node != NULL; head_node = head_node->next) {
		while(head_node->first_child) {
		     if (!head_node->first_child->isheadnode) {
			fm_tree_model_free_node (model, head_node->first_child);
		     } else {
			fm_tree_model_free_top_node (model, model->details->head_root_node);
		     }
		}
	    }
	}
	while(model->details->root_node) {
	  fm_tree_model_free_node (model, model->details->root_node);
	}
	while(model->details->head_root_node) {
	  fm_tree_model_free_top_node (model, model->details->head_root_node);
	}
}
void
fm_tree_model_remove_root_uri (FMTreeModel *model, const char *uri)
{
	TreeNode *node, *foundnode=NULL;
	NemoFile *file;

	file = nemo_file_get_by_uri (uri);
	if(model->details->head_root_node) {
		TreeNode *head_node;
		for (head_node = model->details->head_root_node; head_node != NULL; head_node = head_node->next) {
			for (node = head_node->first_child; node != NULL; node = node->next) {
				if (file == node->file) {
					foundnode = node;
					break;
				}
			}
		}
	} else {
		for (node = model->details->root_node; node != NULL; node = node->next) {
			if (file == node->file) {
				foundnode = node;
				break;
			}
		}
	}
	nemo_file_unref (file);
        node = foundnode;
	fm_tree_model_free_node (model, node);
}

FMTreeModel *
fm_tree_model_new (void)
{
	FMTreeModel *model;

	model = g_object_new (FM_TYPE_TREE_MODEL, NULL);


	model->details->column_types = g_new(GType, FM_TREE_MODEL_NUM_COLUMNS);
	model->details->column_types[FM_TREE_MODEL_DISPLAY_NAME_COLUMN] = G_TYPE_STRING;
	model->details->column_types[FM_TREE_MODEL_CLOSED_ICON_COLUMN] = g_icon_get_type();
	model->details->column_types[FM_TREE_MODEL_OPEN_ICON_COLUMN] = g_icon_get_type();
	model->details->column_types[FM_TREE_MODEL_FONT_STYLE_COLUMN] = PANGO_TYPE_STYLE;
	model->details->column_types[FM_TREE_MODEL_TEXT_WEIGHT_COLUMN] = G_TYPE_INT;
	// Weitere Spaltentypen hier

	model->details->num_columns = FM_TREE_MODEL_NUM_COLUMNS;

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
	if (!iter_is_valid (FM_TREE_MODEL (model), iter)) return NULL;

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

    g_return_val_if_fail(FM_IS_TREE_MODEL(model), 0);
    g_return_val_if_fail(iter_is_valid(model, iter_a), 0);
    g_return_val_if_fail(iter_is_valid(model, iter_b), 0);

    a = iter_a->user_data;
    b = iter_b->user_data;

    if (a == b) {
       return 0;
    }

    /* Administrative nodes come first */
    TreeNode *head;
    for (head = model->details->head_root_node; head != NULL; head = head->next) {
        if (a == head) return -1;
        if (b == head) return 1;


        /* Check children of this administrative node */
        TreeNode *child;
        for (child = head->first_child; child != NULL; child = child->next) {
            if (a == child) return -1;
            if (b == child) return 1;
        }
    }
    /* Optional: normal root nodes */
    for (n = model->details->root_node; n != NULL; n = n->next) {
	if (a == n) return -1;
	if (b == n) return 1;
    }
    /* Should never be reached now */
    g_warning("fm_tree_model_iter_compare_roots: unexpected nodes a=%p b=%p", a, b);
    return 0;
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
	}
	if (node->parent && node->parent->isheadnode) {
        	return TRUE;
    	}
	if (node->parent == NULL)
		return TRUE;
	return FALSE;

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
	TreeNode *head_node;
	for (head_node = model->details->head_root_node; head_node != NULL; head_node = head_node->next) {
	    for (root_node = head_node->first_child; root_node != NULL; root_node = root_node->next) {
		node = get_node_from_file (root_node->root, file);
		if (node != NULL) {
			return make_iter_for_node (node, iter, model->details->stamp);
		}
	    }
	}
	return FALSE;
}

gboolean
fm_tree_model_path_get_iter (FMTreeModel *model, GtkTreePath *path, GtkTreeIter *iter)
{
    g_return_val_if_fail(FM_IS_TREE_MODEL(model), FALSE);
    g_return_val_if_fail(path != NULL, FALSE);
    g_return_val_if_fail(iter != NULL, FALSE);

    // Get the indices from the path
    gint *indices = gtk_tree_path_get_indices(path);
    gint depth = gtk_tree_path_get_depth(path);

    // Check if the path is valid
    if (depth <= 0 || indices == NULL) {
        return FALSE;
    }

    // Start with the root node
    TreeNode *node = model->details->head_root_node;

    // Check the path to find the desired node
    for (gint i = 0; i < depth; i++) {
        gint index = indices[i];

        // Check if the index is valid
        if (node == NULL) {
            return FALSE;
        }

        // Check sibling nodes to find the correct node
        for (gint j = 0; j < index; j++) {
            if (node == NULL) {
                return FALSE;
            }
            node = node->next;
        }

        // If we didn't find the node, return FALSE
        if (node == NULL) {
            return FALSE;
        }

        // Go to the child node if we are not at the end of the path
        if (i < depth - 1) {
            node = node->first_child;
        }
    }

    // Create the GtkTreeIter for the found node
    return make_iter_for_node(node, iter, model->details->stamp);
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
		TreeNode *head, *cnode;
		for (head = model->details->head_root_node; head != NULL; head = head->next) {
		   for (cnode = head->first_child; cnode != NULL; cnode = cnode->next) {
			node = get_node_from_file (cnode->root, file);
			if (node != NULL) {
				break;
			}
		   }
		}
		return;
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

	if (model->details->column_types) {
		g_free(model->details->column_types);
		model->details->column_types = NULL;
	}

	for (root_node = model->details->root_node; root_node != NULL; root_node = next_root) {
		next_root = root_node->next;
		root = root_node->root;
		destroy_node_without_reporting (model, root_node);
		g_hash_table_destroy (root->file_to_node_map);
		g_free (root);
	}
	TreeNode *head, *next_node;
        for (head = model->details->head_root_node; head != NULL; head = next_node) {
            next_node = head->next;
            g_free(head);
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

    tree_model_signals[GET_ICON_SCALE] =
         g_signal_new ("get-icon-scale",
                       FM_TYPE_TREE_MODEL,
                       G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
                       0, NULL, NULL,
                       NULL,
                       G_TYPE_INT, 0);
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

static gboolean
fm_tree_model_row_draggable (GtkTreeDragSource *drag_source,
                             GtkTreePath       *path)
{
    FMTreeModel *model = FM_TREE_MODEL(drag_source);
    GtkTreeIter iter;

    // Standard logic: Check if the path is valid
    if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path)) {
        return FALSE;
    }

    // If a custom function is registered, call it
    if (model->details->custom_row_draggable_func != NULL) {
        return model->details->custom_row_draggable_func(
            drag_source,
            path,
            model->details->custom_row_draggable_data
        );
    }

    return TRUE;
}

void
fm_tree_model_set_custom_row_draggable_func(
    FMTreeModel *model,
    gboolean (*func)(GtkTreeDragSource *drag_source, GtkTreePath *path, gpointer user_data),
    gpointer user_data
) {
    g_return_if_fail(FM_IS_TREE_MODEL(model));
    model->details->custom_row_draggable_func = func;
    model->details->custom_row_draggable_data = user_data;
}


static gboolean
fm_tree_model_drag_data_get(GtkTreeDragSource *drag_source,
                            GtkTreePath *path,
                            GtkSelectionData *selection_data)
{
    FMTreeModel *model = FM_TREE_MODEL(drag_source);
    GtkTreeIter iter;
    TreeNode *node;

    if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &iter, path))
        return FALSE;

    node = iter.user_data;
    if (!node) return FALSE;

    // send the URI as text
    if (node->file) {
        char *uri = nemo_file_get_uri(node->file);
        gtk_selection_data_set_text(selection_data, uri, -1);
        g_free(uri);
    }

    return TRUE;
}


static void
fm_tree_model_drag_source_init (GtkTreeDragSourceIface *iface)
{
    iface->row_draggable = fm_tree_model_row_draggable;
    iface->drag_data_get = fm_tree_model_drag_data_get;
}


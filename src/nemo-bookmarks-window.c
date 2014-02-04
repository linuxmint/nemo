/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

/* nemo-bookmarks-window.c - implementation of bookmark-editing window.
 */

#include <config.h>

#include "nemo-application.h"
#include "nemo-bookmarks-window.h"
#include "nemo-window.h"

#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-entry.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

/* We store a pointer to the bookmark in a column so when an item is moved
   with DnD we know which item it is. However we have to be careful to keep
   this in sync with the actual bookmark. Note that
   nemo_bookmark_list_insert_item() makes a copy of the bookmark, so we
   have to fetch the new copy and update our pointer. */

enum {
	BOOKMARK_LIST_COLUMN_ICON,
	BOOKMARK_LIST_COLUMN_NAME,
	BOOKMARK_LIST_COLUMN_BOOKMARK,
	BOOKMARK_LIST_COLUMN_STYLE,
	BOOKMARK_LIST_NUM_COLUMNS
};

/* Larger size initially; user can stretch or shrink (but not shrink below min) */
#define BOOKMARKS_WINDOW_INITIAL_WIDTH	500
#define BOOKMARKS_WINDOW_INITIAL_HEIGHT	400

G_DEFINE_TYPE (NemoBookmarksWindow, nemo_bookmarks_window, GTK_TYPE_WINDOW)

enum {
	PROP_PARENT_WINDOW = 1,
	NUM_PROPERTIES
};

static GParamSpec* properties[NUM_PROPERTIES] = { NULL, };

struct NemoBookmarksWindowPrivate {
	NemoWindow *parent_window;

	NemoBookmarkList *bookmarks;
	gulong bookmarks_changed_id;

	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	gulong row_activated_id;
	gulong button_press_id;
	gulong key_press_id;
	gulong selection_changed_id;

	GtkListStore *model;
	gulong row_changed_id;
	gulong row_deleted_id;

	GtkWidget *name_field;
	GtkWidget *uri_field;
	gulong name_changed_id;
	gulong uri_changed_id;
	gboolean text_changed;
	gboolean name_text_changed;

	GtkWidget *remove_button;
	GtkWidget *up_button;
	GtkWidget *down_button;
};

static gboolean
get_selection_exists (NemoBookmarksWindow *self)
{
	return gtk_tree_selection_get_selected (self->priv->selection, NULL, NULL);
}

static guint
get_selected_row (NemoBookmarksWindow *self)
{
	GtkTreeIter       iter;
	GtkTreePath      *path;
	GtkTreeModel     *model;
	gint		 *indices, row;
	
	if (!gtk_tree_selection_get_selected (self->priv->selection, &model, &iter)) {
		g_assert_not_reached ();
	}
	
	path = gtk_tree_model_get_path (model, &iter);
	indices = gtk_tree_path_get_indices (path);
	row = indices[0];
	gtk_tree_path_free (path);
	return row;
}

static NemoBookmark *
get_selected_bookmark (NemoBookmarksWindow *self)
{
	if (!get_selection_exists (self)) {
		return NULL;
	}

	if (nemo_bookmark_list_length (self->priv->bookmarks) < 1) {
		return NULL;
	}

	return nemo_bookmark_list_item_at (self->priv->bookmarks,
					       get_selected_row (self));
}

static int
nemo_bookmarks_window_key_press_event_cb (GtkWindow *window, 
					      GdkEventKey *event, 
					      gpointer user_data)
{
	if (event->state & GDK_CONTROL_MASK && event->keyval == GDK_KEY_w) {
		gtk_widget_destroy (GTK_WIDGET (window));
		return TRUE;
	}

	return FALSE;
}

static GtkListStore *
create_bookmark_store (void)
{
	return gtk_list_store_new (BOOKMARK_LIST_NUM_COLUMNS,
				   G_TYPE_ICON,
				   G_TYPE_STRING,
				   G_TYPE_OBJECT,
				   PANGO_TYPE_STYLE);
}

static void
setup_empty_list (NemoBookmarksWindow *self)
{
	GtkListStore *empty_model;
	GtkTreeIter iter;

	empty_model = create_bookmark_store ();
	gtk_list_store_append (empty_model, &iter);

	gtk_list_store_set (empty_model, &iter,
			    BOOKMARK_LIST_COLUMN_NAME, _("No bookmarks defined"),
			    BOOKMARK_LIST_COLUMN_STYLE, PANGO_STYLE_ITALIC,
			    -1);
	gtk_tree_view_set_model (self->priv->tree_view,
				 GTK_TREE_MODEL (empty_model));

	g_object_unref (empty_model);
}

static void
update_widgets_sensitivity (NemoBookmarksWindow *self)
{
	NemoBookmark *selected;
	int n_active;
	int index = -1;

	selected = get_selected_bookmark (self);
	n_active = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (self->priv->model), NULL);
	if (selected != NULL) {
		index = get_selected_row (self);
	}

	/* Set the sensitivity of widgets that require a selection */
	gtk_widget_set_sensitive (self->priv->remove_button, index >= 0 && n_active > 1);
	gtk_widget_set_sensitive (self->priv->up_button, index > 0);
	gtk_widget_set_sensitive (self->priv->down_button, index >= 0 && index < n_active - 1);
	gtk_widget_set_sensitive (self->priv->name_field, selected != NULL);
	gtk_widget_set_sensitive (self->priv->uri_field, selected != NULL);
}

static void
on_selection_changed (GtkTreeSelection *treeselection,
		      gpointer user_data)
{
	NemoBookmarksWindow *self = user_data;
	NemoBookmark *selected;
	const char *name = NULL;
	char *entry_text = NULL;
	GFile *location;

	selected = get_selected_bookmark (self);

	if (selected) {
		name = nemo_bookmark_get_name (selected);
		location = nemo_bookmark_get_location (selected);
		entry_text = g_file_get_parse_name (location);

		g_object_unref (location);
	}

	update_widgets_sensitivity (self);

	g_signal_handler_block (self->priv->name_field, self->priv->name_changed_id);
	nemo_entry_set_text (NEMO_ENTRY (self->priv->name_field),
				 name ? name : "");
	g_signal_handler_unblock (self->priv->name_field, self->priv->name_changed_id);

	g_signal_handler_block (self->priv->uri_field, self->priv->uri_changed_id);
	nemo_entry_set_text (NEMO_ENTRY (self->priv->uri_field),
				 entry_text ? entry_text : "");
	g_signal_handler_unblock (self->priv->uri_field, self->priv->uri_changed_id);

	self->priv->text_changed = FALSE;
	self->priv->name_text_changed = FALSE;

	g_free (entry_text);
}

static void
bookmarks_set_empty (NemoBookmarksWindow *self,
		     gboolean empty)
{
	GtkTreeIter iter;

	if (empty) {
		setup_empty_list (self);
		gtk_widget_set_sensitive (GTK_WIDGET (self->priv->tree_view), FALSE);
	} else {
		gtk_tree_view_set_model (self->priv->tree_view, GTK_TREE_MODEL (self->priv->model));
		gtk_widget_set_sensitive (GTK_WIDGET (self->priv->tree_view), TRUE);

		if (nemo_bookmark_list_length (self->priv->bookmarks) > 0 &&
		    !get_selection_exists (self)) {
			gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (self->priv->model),
						       &iter, NULL, 0);
			gtk_tree_selection_select_iter (self->priv->selection, &iter);
		}
	}

	on_selection_changed (self->priv->selection, self);
}

static void
repopulate (NemoBookmarksWindow *self)
{
	NemoBookmark *selected;
	GtkListStore *store;
	GtkTreePath *path;
	GtkTreeRowReference *reference;
	guint index;

	selected = get_selected_bookmark (self);

	g_signal_handler_block (self->priv->selection, self->priv->selection_changed_id);
	g_signal_handler_block (self->priv->model, self->priv->row_deleted_id);
        g_signal_handler_block (self->priv->tree_view, self->priv->row_activated_id);
        g_signal_handler_block (self->priv->tree_view, self->priv->key_press_id);
        g_signal_handler_block (self->priv->tree_view, self->priv->button_press_id);

	gtk_list_store_clear (self->priv->model);

	g_signal_handler_unblock (self->priv->selection, self->priv->selection_changed_id);
	g_signal_handler_unblock (self->priv->model, self->priv->row_deleted_id);
        g_signal_handler_unblock (self->priv->tree_view, self->priv->row_activated_id);
        g_signal_handler_unblock (self->priv->tree_view, self->priv->key_press_id);
        g_signal_handler_unblock (self->priv->tree_view, self->priv->button_press_id);

	/* Fill the list in with the bookmark names. */
	g_signal_handler_block (self->priv->model, self->priv->row_changed_id);

	reference = NULL;
	store = self->priv->model;

	for (index = 0; index < nemo_bookmark_list_length (self->priv->bookmarks); ++index) {
		NemoBookmark *bookmark;
		const char       *bookmark_name;
		GIcon            *bookmark_icon;
		GtkTreeIter       iter;

		bookmark = nemo_bookmark_list_item_at (self->priv->bookmarks, index);
		bookmark_name = nemo_bookmark_get_name (bookmark);
		bookmark_icon = nemo_bookmark_get_icon (bookmark);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 
				    BOOKMARK_LIST_COLUMN_ICON, bookmark_icon,
				    BOOKMARK_LIST_COLUMN_NAME, bookmark_name,
				    BOOKMARK_LIST_COLUMN_BOOKMARK, bookmark,
				    BOOKMARK_LIST_COLUMN_STYLE, PANGO_STYLE_NORMAL,
				    -1);

		if (bookmark == selected) {
			/* save old selection */
			GtkTreePath *path;

			path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
			reference = gtk_tree_row_reference_new (GTK_TREE_MODEL (store), path);
			gtk_tree_path_free (path);
		}

		g_object_unref (bookmark_icon);
	}

	g_signal_handler_unblock (self->priv->model, self->priv->row_changed_id);

	if (reference != NULL) {
		/* restore old selection */

		/* bookmarks_set_empty() will call the selection change handler,
 		 * so we block it here in case of selection change.
 		 */
		g_signal_handler_block (self->priv->selection, self->priv->selection_changed_id);

		g_assert (index != 0);
		g_assert (gtk_tree_row_reference_valid (reference));

		path = gtk_tree_row_reference_get_path (reference);
		gtk_tree_selection_select_path (self->priv->selection, path);
		gtk_tree_row_reference_free (reference);
		gtk_tree_path_free (path);

		g_signal_handler_unblock (self->priv->selection, self->priv->selection_changed_id);
	}

	bookmarks_set_empty (self, (index == 0));
}

static void
on_bookmark_list_changed (NemoBookmarkList *bookmarks,
			  gpointer user_data)
{
	NemoBookmarksWindow *self = user_data;

	/* maybe add logic here or in repopulate to save/restore selection */
	repopulate (self);
}

static void
on_name_field_changed (GtkEditable *editable,
		       gpointer     user_data)
{
	NemoBookmarksWindow *self = user_data;
	GtkTreeIter   iter;

	if (!get_selection_exists(self)) {
		return;
	}

	/* Update text displayed in list instantly. Also remember that 
	 * user has changed text so we update real bookmark later. 
	 */
	gtk_tree_selection_get_selected (self->priv->selection, NULL, &iter);
	gtk_list_store_set (self->priv->model,
			    &iter, BOOKMARK_LIST_COLUMN_NAME, 
			    gtk_entry_get_text (GTK_ENTRY (self->priv->name_field)),
			    -1);

	self->priv->text_changed = TRUE;
	self->priv->name_text_changed = TRUE;
}

static void
bookmarks_delete_bookmark (NemoBookmarksWindow *self)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	gint *indices, row, rows;
	
	if (!gtk_tree_selection_get_selected (self->priv->selection, NULL, &iter)) {
		return;
	}

	/* Remove the selected item from the list store. on_row_deleted() will
	   remove it from the bookmark list. */
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->priv->model), &iter);
	indices = gtk_tree_path_get_indices (path);
	row = indices[0];
	gtk_tree_path_free (path);

	gtk_list_store_remove (self->priv->model, &iter);

	/* Try to select the same row, or the last one in the list. */
	rows = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (self->priv->model), NULL);
	if (row >= rows) {
		row = rows - 1;
	}

	if (row < 0) {
		bookmarks_set_empty (self, TRUE);
	} else {
		gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (self->priv->model),
					       &iter, NULL, row);
		gtk_tree_selection_select_iter (self->priv->selection, &iter);
	}
}

static void
on_remove_button_clicked (GtkButton *button,
                          gpointer   user_data)
{
	NemoBookmarksWindow *self = user_data;
        bookmarks_delete_bookmark (self);
}

static void
on_up_button_clicked (GtkButton *button,
		      gpointer   user_data)
{
	NemoBookmarksWindow *self = user_data;
	guint row;
	GtkTreeIter iter;

	row = get_selected_row (self);
	nemo_bookmark_list_move_item (self->priv->bookmarks, row, row - 1);
	gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (self->priv->model),
				       &iter, NULL, row - 1);
	gtk_tree_selection_select_iter (self->priv->selection, &iter);
}

static void
on_down_button_clicked (GtkButton *button,
			gpointer   user_data)
{
	NemoBookmarksWindow *self = user_data;
	guint row;
	GtkTreeIter iter;

	row = get_selected_row (self);
	nemo_bookmark_list_move_item (self->priv->bookmarks, row, row + 1);
	gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (self->priv->model),
				       &iter, NULL, row + 1);
	gtk_tree_selection_select_iter (self->priv->selection, &iter);
}

/* This is a bit of a kludge to get DnD to work. We check if the row in the
   GtkListStore matches the one in the bookmark list. If it doesn't, we assume
   the bookmark has just been dragged here and we insert it into the bookmark
   list. */
static void
on_row_changed (GtkListStore *store,
		GtkTreePath *path,
		GtkTreeIter *iter,
		gpointer user_data)
{
	NemoBookmarksWindow *self = user_data;
	NemoBookmark *bookmark = NULL, *bookmark_in_list;
	gint *indices, row;
	gboolean insert_bookmark = TRUE;

	indices = gtk_tree_path_get_indices (path);
	row = indices[0];
	gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
			    BOOKMARK_LIST_COLUMN_BOOKMARK, &bookmark,
			    -1);

	/* If the bookmark in the list doesn't match the changed one, it must
	   have been dragged here, so we insert it into the list. */
	if (row < (gint) nemo_bookmark_list_length (self->priv->bookmarks)) {
		bookmark_in_list = nemo_bookmark_list_item_at (self->priv->bookmarks, row);
		if (bookmark_in_list == bookmark)
			insert_bookmark = FALSE;
	}

	if (insert_bookmark) {
		g_signal_handler_block (self->priv->bookmarks,
					self->priv->bookmarks_changed_id);
		nemo_bookmark_list_insert_item (self->priv->bookmarks,
						    bookmark, row);
		g_signal_handler_unblock (self->priv->bookmarks,
					  self->priv->bookmarks_changed_id);

		/* The bookmark will be copied when inserted into the list, so
		   we have to update the pointer in the list store. */
		bookmark = nemo_bookmark_list_item_at (self->priv->bookmarks, row);
		g_signal_handler_block (store, self->priv->row_changed_id);
		gtk_list_store_set (store, iter,
				    BOOKMARK_LIST_COLUMN_BOOKMARK, bookmark,
				    -1);
		g_signal_handler_unblock (store, self->priv->row_changed_id);
	}
}

static void
update_bookmark_from_text (NemoBookmarksWindow *self)
{
	NemoBookmark *bookmark, *bookmark_in_list;
	const char *name;
	GIcon *icon;
	guint selected_row;
	GtkTreeIter iter;
	GFile *location;

	if (!self->priv->text_changed ||
	    gtk_entry_get_text_length (GTK_ENTRY (self->priv->uri_field)) == 0) {
		return;
	}

	location = g_file_parse_name 
		(gtk_entry_get_text (GTK_ENTRY (self->priv->uri_field)));

	bookmark = nemo_bookmark_new (location,
					  self->priv->name_text_changed ?
					  gtk_entry_get_text (GTK_ENTRY (self->priv->name_field)) : NULL,
					  NULL);
	g_object_unref (location);

	selected_row = get_selected_row (self);

	/* turn off list updating 'cuz otherwise the list-reordering code runs
	 * after repopulate(), thus reordering the correctly-ordered list.
	 */
	g_signal_handler_block (self->priv->bookmarks, self->priv->bookmarks_changed_id);
	nemo_bookmark_list_delete_item_at (self->priv->bookmarks, selected_row);
	nemo_bookmark_list_insert_item (self->priv->bookmarks, bookmark, selected_row);
	g_signal_handler_unblock (self->priv->bookmarks, self->priv->bookmarks_changed_id);
	g_object_unref (bookmark);

	/* We also have to update the bookmark pointer in the list
	   store. */
	gtk_tree_selection_get_selected (self->priv->selection, NULL, &iter);
	g_signal_handler_block (self->priv->model, self->priv->row_changed_id);

	bookmark_in_list = nemo_bookmark_list_item_at (self->priv->bookmarks, selected_row);
	name = nemo_bookmark_get_name (bookmark_in_list);
	icon = nemo_bookmark_get_icon (bookmark_in_list);

	gtk_list_store_set (self->priv->model, &iter,
			    BOOKMARK_LIST_COLUMN_BOOKMARK, bookmark_in_list,
			    BOOKMARK_LIST_COLUMN_NAME, name,
			    BOOKMARK_LIST_COLUMN_ICON, icon,
			    -1);

	g_signal_handler_unblock (self->priv->model, self->priv->row_changed_id);
	g_object_unref (icon);
}

/* The update_bookmark_from_text() calls in the
 * on_button_pressed() and on_key_pressed() handlers
 * of the tree view are a hack.
 *
 * The purpose is to track selection changes to the view
 * and write the text fields back before the selection
 * actually changed.
 *
 * Note that the focus-out event of the text entries is emitted
 * after the selection changed, else this would not not be neccessary.
 */

static gboolean
on_button_pressed (GtkTreeView *view,
		   GdkEventButton *event,
		   gpointer user_data)
{
	NemoBookmarksWindow *self = user_data;
	update_bookmark_from_text (self);

	return FALSE;
}

static gboolean
on_key_pressed (GtkTreeView *view,
                GdkEventKey *event,
                gpointer user_data)
{
	NemoBookmarksWindow *self = user_data;

        if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete) {
                bookmarks_delete_bookmark (self);
                return TRUE;
        }

	update_bookmark_from_text (self);

        return FALSE;
}

static void
on_row_activated (GtkTreeView       *view,
                  GtkTreePath       *path,
                  GtkTreeViewColumn *column,
                  gpointer           user_data)
{
	NemoBookmarksWindow *self = user_data;
	NemoBookmark *selected;
	GFile *location;

	selected = get_selected_bookmark (self);

	if (!selected) {
		return;
	}

	location = nemo_bookmark_get_location (selected);
	if (location == NULL) {
		return;
	}

	nemo_window_go_to (self->priv->parent_window, location);
	g_object_unref (location);
}

static void
on_row_deleted (GtkListStore *store,
		GtkTreePath *path,
		gpointer user_data)
{
	NemoBookmarksWindow *self = user_data;
	gint *indices, row;

	indices = gtk_tree_path_get_indices (path);
	row = indices[0];

	g_signal_handler_block (self->priv->bookmarks, self->priv->bookmarks_changed_id);
	nemo_bookmark_list_delete_item_at (self->priv->bookmarks, row);
	g_signal_handler_unblock (self->priv->bookmarks, self->priv->bookmarks_changed_id);
}

static gboolean
on_text_field_focus_out_event (GtkWidget *widget,
			       GdkEventFocus *event,
			       gpointer user_data)
{
	NemoBookmarksWindow *self = user_data;
	update_bookmark_from_text (self);

	return FALSE;
}

static void
name_or_uri_field_activate (NemoEntry *entry,
			    gpointer user_data)
{
	NemoBookmarksWindow *self = user_data;

	update_bookmark_from_text (self);
	nemo_entry_select_all_at_idle (entry);
}

static void
on_uri_field_changed (GtkEditable *editable,
		      gpointer user_data)
{
	NemoBookmarksWindow *self = user_data;

	/* Remember that user has changed text so we 
	 * update real bookmark later. 
	 */
	self->priv->text_changed = TRUE;
}

static void
nemo_bookmarks_window_dispose (GObject *object)
{
	NemoBookmarksWindow *self = NEMO_BOOKMARKS_WINDOW (object);

	if (self->priv->bookmarks_changed_id != 0) {
		g_signal_handler_disconnect (self->priv->bookmarks,
					     self->priv->bookmarks_changed_id);
		self->priv->bookmarks_changed_id = 0;
	}

	g_clear_object (&self->priv->model);

	G_OBJECT_CLASS (nemo_bookmarks_window_parent_class)->dispose (object);
}

static void
nemo_bookmarks_window_constructed (GObject *object)
{
	NemoBookmarksWindow *self = NEMO_BOOKMARKS_WINDOW (object);
	GtkBuilder *builder;
	GError *error = NULL;
	GtkWindow *window;
	GtkWidget *content;
	GtkTreeViewColumn *col;
	GtkCellRenderer *rend;

	G_OBJECT_CLASS (nemo_bookmarks_window_parent_class)->constructed (object);

	builder = gtk_builder_new ();
	if (!gtk_builder_add_from_resource (builder,
					    "/org/nemo/nemo-bookmarks-window.glade",
					    &error)) {
		g_object_unref (builder);

		g_critical ("Can't load UI description for the bookmarks editor: %s", error->message);
		g_error_free (error);
		return;
	}

	window = GTK_WINDOW (object);
	gtk_window_set_title (window, _("Bookmarks"));
	gtk_window_set_default_size (window, 
				     BOOKMARKS_WINDOW_INITIAL_WIDTH, 
				     BOOKMARKS_WINDOW_INITIAL_HEIGHT);
	gtk_window_set_application (window,
				    gtk_window_get_application (GTK_WINDOW (self->priv->parent_window)));

	gtk_window_set_destroy_with_parent (window, TRUE);
	gtk_window_set_transient_for (window, GTK_WINDOW (self->priv->parent_window));
	gtk_window_set_position (window, GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_container_set_border_width (GTK_CONTAINER (window), 6);

	g_signal_connect (window, "key-press-event",
			  G_CALLBACK (nemo_bookmarks_window_key_press_event_cb), NULL);

	content = GTK_WIDGET (gtk_builder_get_object (builder, "bookmarks_window_content"));
	gtk_container_add (GTK_CONTAINER (window), content);

	/* tree view */
	self->priv->tree_view = GTK_TREE_VIEW (gtk_builder_get_object (builder, "bookmark_tree_view"));
	self->priv->selection = gtk_tree_view_get_selection (self->priv->tree_view);
	gtk_tree_selection_set_mode (self->priv->selection, GTK_SELECTION_BROWSE);

	rend = gtk_cell_renderer_pixbuf_new ();
	g_object_set (rend, "follow-state", TRUE, NULL);
	col = gtk_tree_view_column_new_with_attributes ("Icon", 
							rend,
							"gicon", 
							BOOKMARK_LIST_COLUMN_ICON,
							NULL);
	gtk_tree_view_append_column (self->priv->tree_view,
				     GTK_TREE_VIEW_COLUMN (col));
	gtk_tree_view_column_set_fixed_width (GTK_TREE_VIEW_COLUMN (col),
					      NEMO_ICON_SIZE_SMALLER);

	rend = gtk_cell_renderer_text_new ();
	g_object_set (rend,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "ellipsize-set", TRUE,
		      NULL);

	col = gtk_tree_view_column_new_with_attributes ("Icon", 
							rend,
							"text", 
							BOOKMARK_LIST_COLUMN_NAME,
							"style",
							BOOKMARK_LIST_COLUMN_STYLE,
							NULL);
	gtk_tree_view_append_column (self->priv->tree_view,
				     GTK_TREE_VIEW_COLUMN (col));

	self->priv->model = create_bookmark_store ();
	setup_empty_list (self);

	/* name entry */
	self->priv->name_field = nemo_entry_new ();
	gtk_widget_show (self->priv->name_field);
	gtk_box_pack_start (GTK_BOX (gtk_builder_get_object (builder, "bookmark_name_placeholder")),
			    self->priv->name_field, TRUE, TRUE, 0);
	
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (gtk_builder_get_object (builder, "bookmark_name_label")),
		self->priv->name_field);

	/* URI entry */
	self->priv->uri_field = nemo_entry_new ();
	gtk_widget_show (self->priv->uri_field);
	gtk_box_pack_start (GTK_BOX (gtk_builder_get_object (builder, "bookmark_location_placeholder")),
			    self->priv->uri_field, TRUE, TRUE, 0);

	gtk_label_set_mnemonic_widget (
		GTK_LABEL (gtk_builder_get_object (builder, "bookmark_location_label")),
		self->priv->uri_field);

	/* buttons */
	self->priv->remove_button = GTK_WIDGET (gtk_builder_get_object (builder, "bookmark_remove_button"));
	self->priv->up_button = GTK_WIDGET (gtk_builder_get_object (builder, "bookmark_up_button"));
	self->priv->down_button = GTK_WIDGET (gtk_builder_get_object (builder, "bookmark_down_button"));

	g_object_unref (builder);

	/* setup bookmarks list and signals */
	self->priv->bookmarks = nemo_application_get_bookmarks
		(NEMO_APPLICATION (g_application_get_default ()));
	self->priv->bookmarks_changed_id =
		g_signal_connect (self->priv->bookmarks, "changed",
				  G_CALLBACK (on_bookmark_list_changed), self);

	self->priv->row_changed_id =
		g_signal_connect (self->priv->model, "row-changed",
				  G_CALLBACK (on_row_changed), self);
	self->priv->row_deleted_id =
		g_signal_connect (self->priv->model, "row-deleted",
				  G_CALLBACK (on_row_deleted), self);

        self->priv->row_activated_id =
                g_signal_connect (self->priv->tree_view, "row-activated",
                                  G_CALLBACK (on_row_activated), self);
        self->priv->button_press_id =
                g_signal_connect (self->priv->tree_view, "button-press-event",
                                  G_CALLBACK (on_button_pressed), self);
        self->priv->key_press_id =
                g_signal_connect (self->priv->tree_view, "key-press-event",
                                  G_CALLBACK (on_key_pressed), self);
	self->priv->selection_changed_id =
		g_signal_connect (self->priv->selection, "changed",
				  G_CALLBACK (on_selection_changed), self);

	self->priv->name_changed_id =
		g_signal_connect (self->priv->name_field, "changed",
				  G_CALLBACK (on_name_field_changed), self);
	g_signal_connect (self->priv->name_field, "focus_out_event",
			  G_CALLBACK (on_text_field_focus_out_event), self);
	g_signal_connect (self->priv->name_field, "activate",
			  G_CALLBACK (name_or_uri_field_activate), self);

	self->priv->uri_changed_id =
		g_signal_connect (self->priv->uri_field, "changed",
				  G_CALLBACK (on_uri_field_changed), self);
	g_signal_connect (self->priv->uri_field, "focus_out_event",
			  G_CALLBACK (on_text_field_focus_out_event), self);
	g_signal_connect (self->priv->uri_field, "activate",
			  G_CALLBACK (name_or_uri_field_activate), self);

	g_signal_connect (self->priv->remove_button, "clicked",
			  G_CALLBACK (on_remove_button_clicked), self);
	g_signal_connect (self->priv->up_button, "clicked",
			  G_CALLBACK (on_up_button_clicked), self);
	g_signal_connect (self->priv->down_button, "clicked",
			  G_CALLBACK (on_down_button_clicked), self);
	
	/* Fill in list widget with bookmarks, must be after signals are wired up. */
	repopulate (self);
}

static void
nemo_bookmarks_window_set_property (GObject *object,
					guint arg_id,
					const GValue *value,
					GParamSpec *pspec)
{
	NemoBookmarksWindow *self = NEMO_BOOKMARKS_WINDOW (object);

	switch (arg_id) {
	case PROP_PARENT_WINDOW:
		self->priv->parent_window = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
		break;
	}
}

static void
nemo_bookmarks_window_init (NemoBookmarksWindow *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NEMO_TYPE_BOOKMARKS_WINDOW,
						  NemoBookmarksWindowPrivate);
}

static void
nemo_bookmarks_window_class_init (NemoBookmarksWindowClass *klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	oclass->set_property = nemo_bookmarks_window_set_property;
	oclass->dispose = nemo_bookmarks_window_dispose;
	oclass->constructed = nemo_bookmarks_window_constructed;

	properties[PROP_PARENT_WINDOW] =
		g_param_spec_object ("parent-window",
				     "The NemoWindow",
				     "The parent NemoWindow",
				     NEMO_TYPE_WINDOW,
				     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
	g_type_class_add_private (klass, sizeof (NemoBookmarksWindowPrivate));
}

/**
 * nemo_bookmarks_window_new:
 * 
 * Create a new bookmark-editing window. 
 * @parent_window: The parent NemoWindow.
 *
 * Return value: A pointer to the new window.
 **/
GtkWindow *
nemo_bookmarks_window_new (NemoWindow *parent_window)
{
	return g_object_new (NEMO_TYPE_BOOKMARKS_WINDOW,
			     "parent-window", parent_window,
			     NULL);
}

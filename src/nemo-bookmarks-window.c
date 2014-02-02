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
#include "nemo-bookmarks-window.h"
#include "nemo-window.h"

#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-entry.h>

#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gnome-extensions.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

/* Static variables to keep track of window state. If there were
 * more than one bookmark-editing window, these would be struct or
 * class fields. 
 */
static int		     bookmark_list_changed_signal_id;
static NemoBookmarkList *bookmarks = NULL;
static GtkTreeView	    *bookmark_list_widget = NULL; /* awkward name to distinguish from NemoBookmarkList */
static GtkListStore	    *bookmark_list_store = NULL;
static GtkListStore	    *bookmark_empty_list_store = NULL;
static GtkTreeSelection     *bookmark_selection = NULL;
static int                   selection_changed_id = 0;
static GtkWidget	    *name_field = NULL;
static int		     name_field_changed_signal_id;
static GtkWidget	    *remove_button = NULL;
static GtkWidget	    *up_button = NULL;
static GtkWidget	    *down_button = NULL;
static gboolean		     text_changed = FALSE;
static gboolean		     name_text_changed = FALSE;
static GtkWidget	    *uri_field = NULL;
static int		     uri_field_changed_signal_id;
static int		     row_changed_signal_id;
static int		     row_deleted_signal_id;
static int                   row_activated_signal_id;
static int                   button_pressed_signal_id;
static int                   key_pressed_signal_id;

/* We store a pointer to the bookmark in a column so when an item is moved
   with DnD we know which item it is. However we have to be careful to keep
   this in sync with the actual bookmark. Note that
   nemo_bookmark_list_insert_item() makes a copy of the bookmark, so we
   have to fetch the new copy and update our pointer. */
#define BOOKMARK_LIST_COLUMN_ICON		0
#define BOOKMARK_LIST_COLUMN_NAME		1
#define BOOKMARK_LIST_COLUMN_BOOKMARK		2
#define BOOKMARK_LIST_COLUMN_STYLE		3
#define BOOKMARK_LIST_COLUMN_COUNT		4

/* Larger size initially; user can stretch or shrink (but not shrink below min) */
#define BOOKMARKS_WINDOW_INITIAL_WIDTH	500
#define BOOKMARKS_WINDOW_INITIAL_HEIGHT	400

static gboolean
get_selection_exists (void)
{
	return gtk_tree_selection_get_selected (bookmark_selection, NULL, NULL);
}

static guint
get_selected_row (void)
{
	GtkTreeIter       iter;
	GtkTreePath      *path;
	GtkTreeModel     *model;
	gint		 *indices, row;
	
	g_assert (get_selection_exists());
	
	model = GTK_TREE_MODEL (bookmark_list_store);
	gtk_tree_selection_get_selected (bookmark_selection,
					 &model,
					 &iter);
	
	path = gtk_tree_model_get_path (model, &iter);
	indices = gtk_tree_path_get_indices (path);
	row = indices[0];
	gtk_tree_path_free (path);
	return row;
}

static NemoBookmark *
get_selected_bookmark (void)
{
	g_return_val_if_fail(NEMO_IS_BOOKMARK_LIST(bookmarks), NULL);

	if (!get_selection_exists())
		return NULL;

	if (nemo_bookmark_list_length (bookmarks) < 1)
		return NULL;

	return nemo_bookmark_list_item_at(bookmarks, get_selected_row ());
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
	return gtk_list_store_new (BOOKMARK_LIST_COLUMN_COUNT,
				   G_TYPE_ICON,
				   G_TYPE_STRING,
				   G_TYPE_OBJECT,
				   PANGO_TYPE_STYLE);
}

static void
setup_empty_list (void)
{
	GtkTreeIter iter;

	bookmark_empty_list_store = create_bookmark_store ();
	gtk_list_store_append (bookmark_empty_list_store, &iter);

	gtk_list_store_set (bookmark_empty_list_store, &iter,
			    BOOKMARK_LIST_COLUMN_NAME, _("No bookmarks defined"),
			    BOOKMARK_LIST_COLUMN_STYLE, PANGO_STYLE_ITALIC,
			    -1);
}

static void
update_button_sensitivity (void)
{
	NemoBookmark *selected;
	int n_active;
	int index = -1;

	selected = get_selected_bookmark ();
	n_active = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (bookmark_list_store), NULL);
	if (selected != NULL) {
		index = get_selected_row ();
	}

	/* Set the sensitivity of widgets that require a selection */
	gtk_widget_set_sensitive (remove_button, index >= 0 && n_active > 1);
	gtk_widget_set_sensitive (up_button, index > 0);
	gtk_widget_set_sensitive (down_button, index >= 0 && index < n_active - 1);
	gtk_widget_set_sensitive (name_field, selected != NULL);
	gtk_widget_set_sensitive (uri_field, selected != NULL);
}

static void
on_selection_changed (GtkTreeSelection *treeselection,
		      gpointer user_data)
{
	NemoBookmark *selected;
	const char *name = NULL;
	char *entry_text = NULL;
	GFile *location;

	g_assert (GTK_IS_ENTRY (name_field));
	g_assert (GTK_IS_ENTRY (uri_field));

	selected = get_selected_bookmark ();

	if (selected) {
		name = nemo_bookmark_get_name (selected);
		location = nemo_bookmark_get_location (selected);
		entry_text = g_file_get_parse_name (location);

		g_object_unref (location);
	}

	update_button_sensitivity ();

	g_signal_handler_block (name_field, name_field_changed_signal_id);
	nemo_entry_set_text (NEMO_ENTRY (name_field),
				 name ? name : "");
	g_signal_handler_unblock (name_field, name_field_changed_signal_id);

	g_signal_handler_block (uri_field, uri_field_changed_signal_id);
	nemo_entry_set_text (NEMO_ENTRY (uri_field),
				 entry_text ? entry_text : "");
	g_signal_handler_unblock (uri_field, uri_field_changed_signal_id);

	text_changed = FALSE;
	name_text_changed = FALSE;

	g_free (entry_text);
}

static void
bookmarks_set_empty (gboolean empty)
{
	GtkTreeIter iter;

	if (empty) {
		gtk_tree_view_set_model (bookmark_list_widget,
					 GTK_TREE_MODEL (bookmark_empty_list_store));
		gtk_widget_set_sensitive (GTK_WIDGET (bookmark_list_widget), FALSE);
	} else {
		gtk_tree_view_set_model (bookmark_list_widget,
					 GTK_TREE_MODEL (bookmark_list_store));
		gtk_widget_set_sensitive (GTK_WIDGET (bookmark_list_widget), TRUE);

		if (nemo_bookmark_list_length (bookmarks) > 0 &&
		    !get_selection_exists ()) {
			gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (bookmark_list_store),
						       &iter, NULL, 0);
			gtk_tree_selection_select_iter (bookmark_selection, &iter);
		}
	}

	on_selection_changed (bookmark_selection, NULL);
}

static void
repopulate (void)
{
	NemoBookmark *selected;
	GtkListStore *store;
	GtkTreePath *path;
	GtkTreeRowReference *reference;
	guint index;

	g_assert (GTK_IS_TREE_VIEW (bookmark_list_widget));
	g_assert (NEMO_IS_BOOKMARK_LIST (bookmarks));
	
	store = GTK_LIST_STORE (bookmark_list_store);

	selected = get_selected_bookmark ();

	g_signal_handler_block (bookmark_selection,
				selection_changed_id);
	g_signal_handler_block (bookmark_list_store,
				row_deleted_signal_id);
        g_signal_handler_block (bookmark_list_widget,
                                row_activated_signal_id);
        g_signal_handler_block (bookmark_list_widget,
                                key_pressed_signal_id);
        g_signal_handler_block (bookmark_list_widget,
                                button_pressed_signal_id);

	gtk_list_store_clear (store);
	
	g_signal_handler_unblock (bookmark_list_widget,
				  row_activated_signal_id);
        g_signal_handler_unblock (bookmark_list_widget,
                                  key_pressed_signal_id);
        g_signal_handler_unblock (bookmark_list_widget,
                                  button_pressed_signal_id);
	g_signal_handler_unblock (bookmark_list_store,
				  row_deleted_signal_id);
	g_signal_handler_unblock (bookmark_selection,
				  selection_changed_id);
	
	/* Fill the list in with the bookmark names. */
	g_signal_handler_block (store, row_changed_signal_id);

	reference = NULL;

	for (index = 0; index < nemo_bookmark_list_length (bookmarks); ++index) {
		NemoBookmark *bookmark;
		const char       *bookmark_name;
		GIcon            *bookmark_icon;
		GtkTreeIter       iter;

		bookmark = nemo_bookmark_list_item_at (bookmarks, index);
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

	g_signal_handler_unblock (store, row_changed_signal_id);

	if (reference != NULL) {
		/* restore old selection */

		/* bookmarks_set_empty() will call the selection change handler,
 		 * so we block it here in case of selection change.
 		 */
		g_signal_handler_block (bookmark_selection, selection_changed_id);

		g_assert (index != 0);
		g_assert (gtk_tree_row_reference_valid (reference));

		path = gtk_tree_row_reference_get_path (reference);
		gtk_tree_selection_select_path (bookmark_selection, path);
		gtk_tree_row_reference_free (reference);
		gtk_tree_path_free (path);

		g_signal_handler_unblock (bookmark_selection, selection_changed_id);
	}

	bookmarks_set_empty (index == 0);	  
}

static void
on_bookmark_list_changed (NemoBookmarkList *bookmarks, gpointer data)
{
	g_return_if_fail (NEMO_IS_BOOKMARK_LIST (bookmarks));

	/* maybe add logic here or in repopulate to save/restore selection */
	repopulate ();
}

static void
on_name_field_changed (GtkEditable *editable,
		       gpointer     user_data)
{
	GtkTreeIter   iter;
	g_return_if_fail(GTK_IS_TREE_VIEW(bookmark_list_widget));
	g_return_if_fail(GTK_IS_ENTRY(name_field));

	if (!get_selection_exists())
		return;

	/* Update text displayed in list instantly. Also remember that 
	 * user has changed text so we update real bookmark later. 
	 */
	gtk_tree_selection_get_selected (bookmark_selection,
					 NULL,
					 &iter);
	
	gtk_list_store_set (bookmark_list_store, 
			    &iter, BOOKMARK_LIST_COLUMN_NAME, 
			    gtk_entry_get_text (GTK_ENTRY (name_field)),
			    -1);
	text_changed = TRUE;
	name_text_changed = TRUE;
}

static void
open_selected_bookmark (NemoWindow *window)
{
	NemoBookmark *selected;
	GFile *location;
	
	selected = get_selected_bookmark ();

	if (!selected) {
		return;
	}

	location = nemo_bookmark_get_location (selected);
	if (location == NULL) { 
		return;
	}

	nemo_window_go_to (window, location);

	g_object_unref (location);
}

static void
bookmarks_delete_bookmark (void)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	gint *indices, row, rows;

	g_assert (GTK_IS_TREE_VIEW (bookmark_list_widget));
	
	if (!gtk_tree_selection_get_selected (bookmark_selection, NULL, &iter))
		return;

	/* Remove the selected item from the list store. on_row_deleted() will
	   remove it from the bookmark list. */
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (bookmark_list_store),
					&iter);
	indices = gtk_tree_path_get_indices (path);
	row = indices[0];
	gtk_tree_path_free (path);

	gtk_list_store_remove (bookmark_list_store, &iter);

	/* Try to select the same row, or the last one in the list. */
	rows = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (bookmark_list_store), NULL);
	if (row >= rows)
		row = rows - 1;

	if (row < 0) {
		bookmarks_set_empty (TRUE);
	} else {
		gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (bookmark_list_store),
					       &iter, NULL, row);
		gtk_tree_selection_select_iter (bookmark_selection, &iter);
	}
}

static void
on_remove_button_clicked (GtkButton *button,
                          gpointer   user_data)
{
        bookmarks_delete_bookmark ();
}

static void
on_up_button_clicked (GtkButton *button,
		      gpointer   user_data)
{
	guint row;
	GtkTreeIter iter;

	row = get_selected_row ();
	nemo_bookmark_list_move_item (bookmarks, row, row - 1);
	gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (bookmark_list_store),
				       &iter, NULL, row - 1);
	gtk_tree_selection_select_iter (bookmark_selection, &iter);
}

static void
on_down_button_clicked (GtkButton *button,
			gpointer   user_data)
{
	guint row;
	GtkTreeIter iter;

	row = get_selected_row ();
	nemo_bookmark_list_move_item (bookmarks, row, row + 1);
	gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (bookmark_list_store),
				       &iter, NULL, row + 1);
	gtk_tree_selection_select_iter (bookmark_selection, &iter);
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
	NemoBookmark *bookmark = NULL, *bookmark_in_list;
	gint *indices, row;
	gboolean insert_bookmark = TRUE;

	store = bookmark_list_store;

	indices = gtk_tree_path_get_indices (path);
	row = indices[0];
	gtk_tree_model_get (GTK_TREE_MODEL (store), iter,
			    BOOKMARK_LIST_COLUMN_BOOKMARK, &bookmark,
			    -1);

	/* If the bookmark in the list doesn't match the changed one, it must
	   have been dragged here, so we insert it into the list. */
	if (row < (gint) nemo_bookmark_list_length (bookmarks)) {
		bookmark_in_list = nemo_bookmark_list_item_at (bookmarks,
								   row);
		if (bookmark_in_list == bookmark)
			insert_bookmark = FALSE;
	}

	if (insert_bookmark) {
		g_signal_handler_block (bookmarks,
					bookmark_list_changed_signal_id);
		nemo_bookmark_list_insert_item (bookmarks, bookmark, row);
		g_signal_handler_unblock (bookmarks,
					  bookmark_list_changed_signal_id);

		/* The bookmark will be copied when inserted into the list, so
		   we have to update the pointer in the list store. */
		bookmark = nemo_bookmark_list_item_at (bookmarks, row);
		g_signal_handler_block (store, row_changed_signal_id);
		gtk_list_store_set (store, iter,
				    BOOKMARK_LIST_COLUMN_BOOKMARK, bookmark,
				    -1);
		g_signal_handler_unblock (store, row_changed_signal_id);
	}
}

static void
update_bookmark_from_text (void)
{
	NemoBookmark *bookmark, *bookmark_in_list;
	const char *name;
	GIcon *icon;
	guint selected_row;
	GtkTreeIter iter;
	GFile *location;

	g_assert (GTK_IS_ENTRY (name_field));
	g_assert (GTK_IS_ENTRY (uri_field));

	if (!text_changed ||
	    gtk_entry_get_text_length (GTK_ENTRY (uri_field)) == 0) {
		return;
	}

	location = g_file_parse_name 
		(gtk_entry_get_text (GTK_ENTRY (uri_field)));
		
	bookmark = nemo_bookmark_new (location,
					  name_text_changed ? gtk_entry_get_text (GTK_ENTRY (name_field)) : NULL,
					  NULL);
		
	g_object_unref (location);

	selected_row = get_selected_row ();

	/* turn off list updating 'cuz otherwise the list-reordering code runs
	 * after repopulate(), thus reordering the correctly-ordered list.
	 */
	g_signal_handler_block (bookmarks, 
				bookmark_list_changed_signal_id);
	nemo_bookmark_list_delete_item_at (bookmarks, selected_row);
	nemo_bookmark_list_insert_item (bookmarks, bookmark, selected_row);
	g_signal_handler_unblock (bookmarks, 
				  bookmark_list_changed_signal_id);
	g_object_unref (bookmark);

	/* We also have to update the bookmark pointer in the list
	   store. */
	gtk_tree_selection_get_selected (bookmark_selection,
					 NULL, &iter);
	g_signal_handler_block (bookmark_list_store,
				row_changed_signal_id);

	bookmark_in_list = nemo_bookmark_list_item_at (bookmarks,
							   selected_row);

	name = nemo_bookmark_get_name (bookmark_in_list);
	icon = nemo_bookmark_get_icon (bookmark_in_list);

	gtk_list_store_set (bookmark_list_store, &iter,
			    BOOKMARK_LIST_COLUMN_BOOKMARK, bookmark_in_list,
			    BOOKMARK_LIST_COLUMN_NAME, name,
			    BOOKMARK_LIST_COLUMN_ICON, icon,
			    -1);
	g_signal_handler_unblock (bookmark_list_store,
				  row_changed_signal_id);

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
	update_bookmark_from_text ();

	return FALSE;
}

static gboolean
on_key_pressed (GtkTreeView *view,
                GdkEventKey *event,
                gpointer user_data)
{
        if (event->keyval == GDK_KEY_Delete || event->keyval == GDK_KEY_KP_Delete) {
                bookmarks_delete_bookmark ();
                return TRUE;
        }

	update_bookmark_from_text ();

        return FALSE;
}

static void
on_row_activated (GtkTreeView       *view,
                  GtkTreePath       *path,
                  GtkTreeViewColumn *column,
                  gpointer           user_data)
{
	open_selected_bookmark (user_data);
}

static void
on_row_deleted (GtkListStore *store,
		GtkTreePath *path,
		gpointer user_data)
{
	gint *indices, row;

	indices = gtk_tree_path_get_indices (path);
	row = indices[0];

	g_signal_handler_block (bookmarks, bookmark_list_changed_signal_id);
	nemo_bookmark_list_delete_item_at (bookmarks, row);
	g_signal_handler_unblock (bookmarks, bookmark_list_changed_signal_id);
}

static gboolean
on_text_field_focus_out_event (GtkWidget *widget,
			       GdkEventFocus *event,
			       gpointer user_data)
{
	g_assert (NEMO_IS_ENTRY (widget));

	update_bookmark_from_text ();
	return FALSE;
}

static void
name_or_uri_field_activate (NemoEntry *entry)
{
	g_assert (NEMO_IS_ENTRY (entry));

	update_bookmark_from_text ();
	nemo_entry_select_all_at_idle (entry);
}

static void
on_uri_field_changed (GtkEditable *editable,
		      gpointer user_data)
{
	/* Remember that user has changed text so we 
	 * update real bookmark later. 
	 */
	text_changed = TRUE;
}

/**
 * nemo_bookmarks_window_new:
 * 
 * Create a new bookmark-editing window. 
 * @list: The NemoBookmarkList that this window will edit.
 *
 * Return value: A pointer to the new window.
 **/
GtkWindow *
nemo_bookmarks_window_new (NemoWindow *parent_window,
			       NemoBookmarkList *list)
{
	GtkWindow         *window;
	GtkTreeViewColumn *col;
	GtkCellRenderer   *rend;
	GtkBuilder        *builder;

	bookmarks = list;

	builder = gtk_builder_new ();
	if (!gtk_builder_add_from_resource (builder,
					    "/org/nemo/nemo-bookmarks-window.glade",
					    NULL)) {
		return NULL;
	}

	window = GTK_WINDOW (gtk_builder_get_object (builder, "bookmarks_dialog"));

	gtk_window_set_wmclass (window, "bookmarks", "Nemo");
	gtk_window_set_default_size (window, 
				     BOOKMARKS_WINDOW_INITIAL_WIDTH, 
				     BOOKMARKS_WINDOW_INITIAL_HEIGHT);
	gtk_window_set_application (window,
				    gtk_window_get_application (GTK_WINDOW (parent_window)));
	gtk_window_set_destroy_with_parent (window, TRUE);

	g_signal_connect (window, "key-press-event",
			  G_CALLBACK (nemo_bookmarks_window_key_press_event_cb), NULL);

	bookmark_list_widget = GTK_TREE_VIEW (gtk_builder_get_object (builder, "bookmark_tree_view"));
	remove_button = GTK_WIDGET (gtk_builder_get_object (builder, "bookmark_remove_button"));
	up_button = GTK_WIDGET (gtk_builder_get_object (builder, "bookmark_up_button"));
	down_button = GTK_WIDGET (gtk_builder_get_object (builder, "bookmark_down_button"));

	rend = gtk_cell_renderer_pixbuf_new ();
	g_object_set (rend,
		      "follow-state", TRUE,
		      NULL);
	col = gtk_tree_view_column_new_with_attributes ("Icon", 
							rend,
							"gicon", 
							BOOKMARK_LIST_COLUMN_ICON,
							NULL);
	gtk_tree_view_append_column (bookmark_list_widget,
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
	gtk_tree_view_append_column (bookmark_list_widget,
				     GTK_TREE_VIEW_COLUMN (col));
	
	bookmark_list_store = create_bookmark_store ();
	setup_empty_list ();
	gtk_tree_view_set_model (bookmark_list_widget,
				 GTK_TREE_MODEL (bookmark_empty_list_store));
	
	bookmark_selection =
		GTK_TREE_SELECTION (gtk_tree_view_get_selection (bookmark_list_widget));

	name_field = nemo_entry_new ();
	
	gtk_widget_show (name_field);
	gtk_box_pack_start (GTK_BOX (gtk_builder_get_object (builder, "bookmark_name_placeholder")),
			    name_field, TRUE, TRUE, 0);
	
	gtk_label_set_mnemonic_widget (
		GTK_LABEL (gtk_builder_get_object (builder, "bookmark_name_label")),
		name_field);

	uri_field = nemo_entry_new ();
	gtk_widget_show (uri_field);
	gtk_box_pack_start (GTK_BOX (gtk_builder_get_object (builder, "bookmark_location_placeholder")),
			    uri_field, TRUE, TRUE, 0);

	gtk_label_set_mnemonic_widget (
		GTK_LABEL (gtk_builder_get_object (builder, "bookmark_location_label")),
		uri_field);

	bookmark_list_changed_signal_id =
		g_signal_connect (bookmarks, "changed",
				  G_CALLBACK (on_bookmark_list_changed), NULL);
	row_changed_signal_id =
		g_signal_connect (bookmark_list_store, "row_changed",
				  G_CALLBACK (on_row_changed), NULL);
	row_deleted_signal_id =
		g_signal_connect (bookmark_list_store, "row_deleted",
				  G_CALLBACK (on_row_deleted), NULL);
        row_activated_signal_id =
                g_signal_connect (bookmark_list_widget, "row_activated",
                                  G_CALLBACK (on_row_activated), parent_window);
        button_pressed_signal_id =
                g_signal_connect (bookmark_list_widget, "button_press_event",
                                  G_CALLBACK (on_button_pressed), NULL);
        key_pressed_signal_id =
                g_signal_connect (bookmark_list_widget, "key_press_event",
                                  G_CALLBACK (on_key_pressed), NULL);
	selection_changed_id =
		g_signal_connect (bookmark_selection, "changed",
				  G_CALLBACK (on_selection_changed), NULL);	

	name_field_changed_signal_id =
		g_signal_connect (name_field, "changed",
				  G_CALLBACK (on_name_field_changed), NULL);
                      		    
	g_signal_connect (name_field, "focus_out_event",
			  G_CALLBACK (on_text_field_focus_out_event), NULL);                            
	g_signal_connect (name_field, "activate",
			  G_CALLBACK (name_or_uri_field_activate), NULL);

	uri_field_changed_signal_id = 
		g_signal_connect (uri_field, "changed",
				  G_CALLBACK (on_uri_field_changed), NULL);
                      		    
	g_signal_connect (uri_field, "focus_out_event",
			  G_CALLBACK (on_text_field_focus_out_event), NULL);
	g_signal_connect (uri_field, "activate",
			  G_CALLBACK (name_or_uri_field_activate), NULL);
	g_signal_connect (remove_button, "clicked",
			  G_CALLBACK (on_remove_button_clicked), NULL);
	g_signal_connect (up_button, "clicked",
			  G_CALLBACK (on_up_button_clicked), NULL);
	g_signal_connect (down_button, "clicked",
			  G_CALLBACK (on_down_button_clicked), NULL);

	gtk_tree_selection_set_mode (bookmark_selection, GTK_SELECTION_BROWSE);
	
	/* Fill in list widget with bookmarks, must be after signals are wired up. */
	repopulate();

	g_object_unref (builder);
	
	return GTK_WINDOW (window);
}

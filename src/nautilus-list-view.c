/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-view.c - implementation of list view of directory.

   Copyright (C) 2000 Eazel, Inc.
   Copyright (C) 2001, 2002 Anders Carlsson <andersca@gnu.org>
   
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

   Authors: John Sullivan <sullivan@eazel.com>
            Anders Carlsson <andersca@gnu.org>
	    David Emory Watson <dwatson@cs.ucr.edu>
*/

#include <config.h>
#include "nautilus-list-view.h"

#include "nautilus-list-model.h"
#include "nautilus-error-reporting.h"
#include "nautilus-view-dnd.h"
#include "nautilus-view-factory.h"

#include <string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <libnautilus-extension/nautilus-column-provider.h>
#include <libnautilus-private/nautilus-clipboard-monitor.h>
#include <libnautilus-private/nautilus-column-chooser.h>
#include <libnautilus-private/nautilus-column-utilities.h>
#include <libnautilus-private/nautilus-dnd.h>
#include <libnautilus-private/nautilus-file-dnd.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-tree-view-drag-dest.h>
#include <libnautilus-private/nautilus-clipboard.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_LIST_VIEW
#include <libnautilus-private/nautilus-debug.h>

struct NautilusListViewDetails {
	GtkTreeView *tree_view;
	NautilusListModel *model;
	GtkActionGroup *list_action_group;
	guint list_merge_id;

	GtkTreeViewColumn   *file_name_column;
	int file_name_column_num;

	GtkCellRendererPixbuf *pixbuf_cell;
	GtkCellRendererText   *file_name_cell;
	GList *cells;
	GtkCellEditable *editable_widget;

	NautilusZoomLevel zoom_level;

	NautilusTreeViewDragDest *drag_dest;

	GtkTreePath *double_click_path[2]; /* Both clicks in a double click need to be on the same row */

	GtkTreePath *new_selection_path;   /* Path of the new selection after removing a file */

	GtkTreePath *hover_path;

	guint drag_button;
	int drag_x;
	int drag_y;

	gboolean drag_started;
	gboolean ignore_button_release;
	gboolean row_selected_on_button_down;
	gboolean menus_ready;
	gboolean active;
	
	GHashTable *columns;
	GtkWidget *column_editor;

	char *original_name;
	
	NautilusFile *renaming_file;
	gboolean rename_done;
	guint renaming_file_activate_timeout;

	gulong clipboard_handler_id;

	GQuark last_sort_attr;
};

struct SelectionForeachData {
	GList *list;
	GtkTreeSelection *selection;
};

/*
 * The row height should be large enough to not clip emblems.
 * Computing this would be costly, so we just choose a number
 * that works well with the set of emblems we've designed.
 */
#define LIST_VIEW_MINIMUM_ROW_HEIGHT	28

/* We wait two seconds after row is collapsed to unload the subdirectory */
#define COLLAPSE_TO_UNLOAD_DELAY 2

/* Wait for the rename to end when activating a file being renamed */
#define WAIT_FOR_RENAME_ON_ACTIVATE 200

static GdkCursor *              hand_cursor = NULL;

static GtkTargetList *          source_target_list = NULL;

static GList *nautilus_list_view_get_selection                   (NautilusView   *view);
static GList *nautilus_list_view_get_selection_for_file_transfer (NautilusView   *view);
static void   nautilus_list_view_set_zoom_level                  (NautilusListView        *view,
								  NautilusZoomLevel  new_level,
								  gboolean           always_set_level);
static void   nautilus_list_view_scroll_to_file                  (NautilusListView        *view,
								  NautilusFile      *file);
static void   nautilus_list_view_rename_callback                 (NautilusFile      *file,
								  GFile             *result_location,
								  GError            *error,
								  gpointer           callback_data);


G_DEFINE_TYPE (NautilusListView, nautilus_list_view, NAUTILUS_TYPE_VIEW);

static const char * default_trash_visible_columns[] = {
	"name", "size", "type", "trashed_on", "trash_orig_path", NULL
};

static const char * default_trash_columns_order[] = {
	"name", "size", "type", "trashed_on", "trash_orig_path", NULL
};

static const gchar*
get_default_sort_order (NautilusFile *file, gboolean *reversed)
{
	NautilusFileSortType default_sort_order;
	gboolean default_sort_reversed;
	const gchar *retval;
	const char *attributes[] = {
		"name", /* is really "manually" which doesn't apply to lists */
		"name",
		"size",
		"type",
		"date_modified",
		"date_accessed",
		"trashed_on",
		NULL
	};

	retval = nautilus_file_get_default_sort_attribute (file, reversed);

	if (retval == NULL) {
		default_sort_order = g_settings_get_enum (nautilus_preferences,
							  NAUTILUS_PREFERENCES_DEFAULT_SORT_ORDER);
		default_sort_reversed = g_settings_get_boolean (nautilus_preferences,
								NAUTILUS_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER);

		retval = attributes[default_sort_order];
		*reversed = default_sort_reversed;
	}

	return retval;
}

static void
list_selection_changed_callback (GtkTreeSelection *selection, gpointer user_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (user_data);

	nautilus_view_notify_selection_changed (view);
}

/* Move these to eel? */

static void
tree_selection_foreach_set_boolean (GtkTreeModel *model,
				    GtkTreePath *path,
				    GtkTreeIter *iter,
				    gpointer callback_data)
{
	* (gboolean *) callback_data = TRUE;
}

static gboolean
tree_selection_not_empty (GtkTreeSelection *selection)
{
	gboolean not_empty;

	not_empty = FALSE;
	gtk_tree_selection_selected_foreach (selection,
					     tree_selection_foreach_set_boolean,
					     &not_empty);
	return not_empty;
}

static gboolean
tree_view_has_selection (GtkTreeView *view)
{
	return tree_selection_not_empty (gtk_tree_view_get_selection (view));
}

static void
preview_selected_items (NautilusListView *view)
{
	GList *file_list;
	
	file_list = nautilus_list_view_get_selection (NAUTILUS_VIEW (view));

	if (file_list != NULL) {
		nautilus_view_preview_files (NAUTILUS_VIEW (view),
					     file_list, NULL);
		nautilus_file_list_free (file_list);
	}
}

static void
activate_selected_items (NautilusListView *view)
{
	GList *file_list;
	
	file_list = nautilus_list_view_get_selection (NAUTILUS_VIEW (view));

	
	if (view->details->renaming_file) {
		/* We're currently renaming a file, wait until the rename is
		   finished, or the activation uri will be wrong */
		if (view->details->renaming_file_activate_timeout == 0) {
			view->details->renaming_file_activate_timeout =
				g_timeout_add (WAIT_FOR_RENAME_ON_ACTIVATE, (GSourceFunc) activate_selected_items, view);
		}
		return;
	}
	
	if (view->details->renaming_file_activate_timeout != 0) {
		g_source_remove (view->details->renaming_file_activate_timeout);
		view->details->renaming_file_activate_timeout = 0;
	}
	
	nautilus_view_activate_files (NAUTILUS_VIEW (view),
				      file_list,
				      0, TRUE);
	nautilus_file_list_free (file_list);

}

static void
activate_selected_items_alternate (NautilusListView *view,
				   NautilusFile *file,
				   gboolean open_in_tab)
{
	GList *file_list;
	NautilusWindowOpenFlags flags;

	flags = 0;

	if (open_in_tab) {
		flags |= NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB;
	} else {
		flags |= NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW;
	}

	if (file != NULL) {
		nautilus_file_ref (file);
		file_list = g_list_prepend (NULL, file);
	} else {
		file_list = nautilus_list_view_get_selection (NAUTILUS_VIEW (view));
	}
	nautilus_view_activate_files (NAUTILUS_VIEW (view),
				      file_list,
				      flags,
				      TRUE);
	nautilus_file_list_free (file_list);

}

static gboolean
button_event_modifies_selection (GdkEventButton *event)
{
	return (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) != 0;
}

static int
get_click_policy (void)
{
	return g_settings_get_enum (nautilus_preferences,
				    NAUTILUS_PREFERENCES_CLICK_POLICY);
}

static void
nautilus_list_view_did_not_drag (NautilusListView *view,
				 GdkEventButton *event)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	
	tree_view = view->details->tree_view;
	selection = gtk_tree_view_get_selection (tree_view);

	if (gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y,
					   &path, NULL, NULL, NULL)) {
		if ((event->button == 1 || event->button == 2)
		    && ((event->state & GDK_CONTROL_MASK) != 0 ||
			(event->state & GDK_SHIFT_MASK) == 0)
		    && view->details->row_selected_on_button_down) {
			if (!button_event_modifies_selection (event)) {
				gtk_tree_selection_unselect_all (selection);
				gtk_tree_selection_select_path (selection, path);
			} else {
				gtk_tree_selection_unselect_path (selection, path);
			}
		}

		if ((get_click_policy () == NAUTILUS_CLICK_POLICY_SINGLE)
		    && !button_event_modifies_selection(event)) {
			if (event->button == 1) {
				activate_selected_items (view);
			} else if (event->button == 2) {
				activate_selected_items_alternate (view, NULL, TRUE);
			}
		}
		gtk_tree_path_free (path);
	}
	
}

static void 
drag_data_get_callback (GtkWidget *widget,
			GdkDragContext *context,
			GtkSelectionData *selection_data,
			guint info,
			guint time)
{
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GList *selection_cache;

	tree_view = GTK_TREE_VIEW (widget);
  
	model = gtk_tree_view_get_model (tree_view);
  
	if (model == NULL) {
		return;
	}

	selection_cache = g_object_get_data (G_OBJECT (context), "drag-info");
	if (selection_cache == NULL) {
		return;
	}

	nautilus_drag_drag_data_get_from_cache (selection_cache, context, selection_data, info, time);
}

static void
stop_drag_check (NautilusListView *view)
{		
	view->details->drag_button = 0;
}

static GdkPixbuf *
get_drag_pixbuf (NautilusListView *view)
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GdkPixbuf *ret;
	GdkRectangle cell_area;
	
	ret = NULL;
	
	if (gtk_tree_view_get_path_at_pos (view->details->tree_view, 
					   view->details->drag_x,
					   view->details->drag_y,
					   &path, NULL, NULL, NULL)) {
		model = gtk_tree_view_get_model (view->details->tree_view);
		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter,
				    nautilus_list_model_get_column_id_from_zoom_level (view->details->zoom_level),
				    &ret,
				    -1);

		gtk_tree_view_get_cell_area (view->details->tree_view,
					     path, 
					     view->details->file_name_column, 
					     &cell_area);

		gtk_tree_path_free (path);
	}

	return ret;
}

/* iteration glue struct */
typedef struct {
	NautilusListView *view;
	NautilusDragEachSelectedItemDataGet iteratee;
	gpointer iteratee_data;
} ListGetDataBinderContext;

static void
item_get_data_binder (GtkTreeModel *model,
		      GtkTreePath *path,
		      GtkTreeIter *iter,
		      gpointer data)
{
	ListGetDataBinderContext *context = data;
	NautilusFile *file;
	GtkTreeView *treeview;
	GtkTreeViewColumn *column;
	GdkRectangle cell_area;
	int drag_begin_y  = 0;
	char *uri;

	treeview = nautilus_list_model_get_drag_view (context->view->details->model,
						      NULL,
						      &drag_begin_y);
	column = gtk_tree_view_get_column (treeview, 0);

	file = nautilus_list_model_file_for_path (NAUTILUS_LIST_MODEL (model), path);
	if (file == NULL) {
		return;
	}

	gtk_tree_view_get_cell_area (treeview,
				     path,
				     column,
				     &cell_area);

	uri = nautilus_file_get_activation_uri (file);
	nautilus_file_unref (file);

	/* pass the uri, mouse-relative x/y and icon width/height */
	context->iteratee (uri,
			   0,
			   cell_area.y - drag_begin_y,
			   cell_area.width,
			   cell_area.height,
			   context->iteratee_data);

	g_free (uri);
}

static void
each_item_get_data_binder (NautilusDragEachSelectedItemDataGet iteratee,
			   gpointer iterator_context,
			   gpointer data)
{
	NautilusListView *view = NAUTILUS_LIST_VIEW (iterator_context);
	ListGetDataBinderContext context;
	GtkTreeSelection *selection;

	context.view = view;
	context.iteratee = iteratee;
	context.iteratee_data = data;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->details->tree_view));
	gtk_tree_selection_selected_foreach (selection, item_get_data_binder, &context);
}


static void
drag_begin_callback (GtkWidget *widget,
		     GdkDragContext *context,
		     NautilusListView *view)
{
	GList *selection_cache;
	GdkPixbuf *pixbuf;

	pixbuf = get_drag_pixbuf (view);
	if (pixbuf) {
		gtk_drag_set_icon_pixbuf (context,
					  pixbuf,
					  0, 0);
		g_object_unref (pixbuf);
	} else {
		gtk_drag_set_icon_default (context);
	}

	stop_drag_check (view);
	view->details->drag_started = TRUE;

	selection_cache = nautilus_drag_create_selection_cache (view,
								each_item_get_data_binder);

	g_object_set_data_full (G_OBJECT (context),
				"drag-info",
				selection_cache,
				(GDestroyNotify)nautilus_drag_destroy_selection_list);
}

static gboolean
motion_notify_callback (GtkWidget *widget,
			GdkEventMotion *event,
			gpointer callback_data)
{
	NautilusListView *view;
	
	view = NAUTILUS_LIST_VIEW (callback_data);

	if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget))) {
		return FALSE;
	}

	if (get_click_policy () == NAUTILUS_CLICK_POLICY_SINGLE) {
		GtkTreePath *old_hover_path;

		old_hover_path = view->details->hover_path;
		gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
					       event->x, event->y,
					       &view->details->hover_path,
					       NULL, NULL, NULL);

		if ((old_hover_path != NULL) != (view->details->hover_path != NULL)) {
			if (view->details->hover_path != NULL) {
				gdk_window_set_cursor (gtk_widget_get_window (widget), hand_cursor);
			} else {
				gdk_window_set_cursor (gtk_widget_get_window (widget), NULL);
			}
		}

		if (old_hover_path != NULL) {
			gtk_tree_path_free (old_hover_path);
		}
	}

	if (view->details->drag_button != 0) {
		if (!source_target_list) {
			source_target_list = nautilus_list_model_get_drag_target_list ();
		}

		if (gtk_drag_check_threshold (widget,
					      view->details->drag_x,
					      view->details->drag_y,
					      event->x, 
					      event->y)) {
			gtk_drag_begin
				(widget,
				 source_target_list,
				 GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_ASK,
				 view->details->drag_button,
				 (GdkEvent*)event);
		}		      
		return TRUE;
	}
	
	return FALSE;
}

static gboolean
leave_notify_callback (GtkWidget *widget,
		       GdkEventCrossing *event,
		       gpointer callback_data)
{
	NautilusListView *view;

	view = NAUTILUS_LIST_VIEW (callback_data);

	if (get_click_policy () == NAUTILUS_CLICK_POLICY_SINGLE &&
	    view->details->hover_path != NULL) {
		gtk_tree_path_free (view->details->hover_path);
		view->details->hover_path = NULL;
	}

	return FALSE;
}

static gboolean
enter_notify_callback (GtkWidget *widget,
		       GdkEventCrossing *event,
		       gpointer callback_data)
{
	NautilusListView *view;

	view = NAUTILUS_LIST_VIEW (callback_data);

	if (get_click_policy () == NAUTILUS_CLICK_POLICY_SINGLE) {
		if (view->details->hover_path != NULL) {
			gtk_tree_path_free (view->details->hover_path);
		}

		gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
					       event->x, event->y,
					       &view->details->hover_path,
					       NULL, NULL, NULL);

		if (view->details->hover_path != NULL) {
			gdk_window_set_cursor (gtk_widget_get_window (widget), hand_cursor);
		}
	}

	return FALSE;
}

static void
do_popup_menu (GtkWidget *widget, NautilusListView *view, GdkEventButton *event)
{
 	if (tree_view_has_selection (GTK_TREE_VIEW (widget))) {
		nautilus_view_pop_up_selection_context_menu (NAUTILUS_VIEW (view), event);
	} else {
                nautilus_view_pop_up_background_context_menu (NAUTILUS_VIEW (view), event);
	}
}

static void
row_activated_callback (GtkTreeView *treeview, GtkTreePath *path, 
			GtkTreeViewColumn *column, NautilusListView *view)
{
	activate_selected_items (view);
}

static gboolean
button_press_callback (GtkWidget *widget, GdkEventButton *event, gpointer callback_data)
{
	NautilusListView *view;
	GtkTreeView *tree_view;
	GtkTreePath *path;
	GtkTreeSelection *selection;
	GtkWidgetClass *tree_view_class;
	gint64 current_time;
	static gint64 last_click_time = 0;
	static int click_count = 0;
	int double_click_time;
	gboolean call_parent, on_expander, show_expanders;
	gboolean is_simple_click, path_selected;
	NautilusFile *file;

	view = NAUTILUS_LIST_VIEW (callback_data);
	tree_view = GTK_TREE_VIEW (widget);
	tree_view_class = GTK_WIDGET_GET_CLASS (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	/* Don't handle extra mouse buttons here */
	if (event->button > 5) {
		return FALSE;
	}

	if (event->window != gtk_tree_view_get_bin_window (tree_view)) {
		return FALSE;
	}

	nautilus_list_model_set_drag_view
		(NAUTILUS_LIST_MODEL (gtk_tree_view_get_model (tree_view)),
		 tree_view,
		 event->x, event->y);

	g_object_get (G_OBJECT (gtk_widget_get_settings (widget)),
		      "gtk-double-click-time", &double_click_time,
		      NULL);

	/* Determine click count */
	current_time = g_get_monotonic_time ();
	if (current_time - last_click_time < double_click_time * 1000) {
		click_count++;
	} else {
		click_count = 0;
	}

	/* Stash time for next compare */
	last_click_time = current_time;

	/* Ignore double click if we are in single click mode */
	if (get_click_policy () == NAUTILUS_CLICK_POLICY_SINGLE && click_count >= 2) {
		return TRUE;
	}

	view->details->ignore_button_release = FALSE;
	is_simple_click = ((event->button == 1 || event->button == 2) && (event->type == GDK_BUTTON_PRESS));

	/* No item at this position */
	if (!gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y,
					    &path, NULL, NULL, NULL)) {
		if (is_simple_click) {
			g_clear_pointer (&view->details->double_click_path[1], gtk_tree_path_free);
			view->details->double_click_path[1] = view->details->double_click_path[0];
			view->details->double_click_path[0] = NULL;
		}

		/* Deselect if people click outside any row. It's OK to
		   let default code run; it won't reselect anything. */
		gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (tree_view));
		tree_view_class->button_press_event (widget, event);

		if (event->button == 3) {
			do_popup_menu (widget, view, event);
		}

		return TRUE;
	}

	call_parent = TRUE;
	on_expander = FALSE;
	path_selected = gtk_tree_selection_path_is_selected (selection, path);
	show_expanders = g_settings_get_boolean (nautilus_list_view_preferences,
						 NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE);

	if (show_expanders) {
		int expander_size, horizontal_separator;
		gtk_widget_style_get (widget,
				      "expander-size", &expander_size,
				      "horizontal-separator", &horizontal_separator,
				      NULL);
		/* TODO we should not hardcode this extra padding. It is
		 * EXPANDER_EXTRA_PADDING from GtkTreeView.
		 */
		expander_size += 4;
		on_expander = (event->x <= horizontal_separator / 2 +
			       gtk_tree_path_get_depth (path) * expander_size);
	}

	/* Keep track of path of last click so double clicks only happen
	 * on the same item */
	if (is_simple_click) {
		g_clear_pointer (&view->details->double_click_path[1], gtk_tree_path_free);
		view->details->double_click_path[1] = view->details->double_click_path[0];
		view->details->double_click_path[0] = gtk_tree_path_copy (path);
	}

	if (event->type == GDK_2BUTTON_PRESS) {
		/* Double clicking does not trigger a D&D action. */
		view->details->drag_button = 0;

		/* NOTE: Activation can actually destroy the view if we're switching */
		if (!on_expander &&
		    view->details->double_click_path[1] &&
		    gtk_tree_path_compare (view->details->double_click_path[0], view->details->double_click_path[1]) == 0) {
			if ((event->button == 1) && button_event_modifies_selection (event)) {
				file = nautilus_list_model_file_for_path (view->details->model, path);
				if (file != NULL) {
					activate_selected_items_alternate (view, file, TRUE);
					nautilus_file_unref (file);
				}
			} else {
				if ((event->button == 1 || event->button == 3)) {
					activate_selected_items (view);
				} else if (event->button == 2) {
					activate_selected_items_alternate (view, NULL, TRUE);
				}
			}
		} else {
			tree_view_class->button_press_event (widget, event);
		}
	} else {
		/* We're going to filter out some situations where
		 * we can't let the default code run because all
		 * but one row would be would be deselected. We don't
		 * want that; we want the right click menu or single
		 * click to apply to everything that's currently selected.
		 */
		if (event->button == 3 && path_selected) {
			call_parent = FALSE;
		}

		if ((event->button == 1 || event->button == 2) &&
		    ((event->state & GDK_CONTROL_MASK) != 0 || (event->state & GDK_SHIFT_MASK) == 0)) {
			view->details->row_selected_on_button_down = path_selected;

			if (path_selected) {
				call_parent = on_expander;
				view->details->ignore_button_release = on_expander;
			} else if ((event->state & GDK_CONTROL_MASK) != 0) {
				GList *selected_rows, *l;

				call_parent = FALSE;
				if ((event->state & GDK_SHIFT_MASK) != 0) {
					GtkTreePath *cursor;
					gtk_tree_view_get_cursor (tree_view, &cursor, NULL);
					if (cursor != NULL) {
						gtk_tree_selection_select_range (selection, cursor, path);
					} else {
						gtk_tree_selection_select_path (selection, path);
					}
				} else {
					gtk_tree_selection_select_path (selection, path);
				}
				selected_rows = gtk_tree_selection_get_selected_rows (selection, NULL);

				/* This unselects everything */
				gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);

				/* So select it again */
				for (l = selected_rows; l != NULL; l = l->next) {
					gtk_tree_selection_select_path (selection, l->data);
				}
				g_list_free_full (selected_rows, (GDestroyNotify) gtk_tree_path_free);
			} else {
				view->details->ignore_button_release = on_expander;
			}
		}
		
		if (call_parent) {
			g_signal_handlers_block_by_func (tree_view, row_activated_callback, view);
			tree_view_class->button_press_event (widget, event);
			g_signal_handlers_unblock_by_func (tree_view, row_activated_callback, view);
		} else if (path_selected) {
			gtk_widget_grab_focus (widget);
		}

		if (is_simple_click) {
			view->details->drag_started = FALSE;
			view->details->drag_button = event->button;
			view->details->drag_x = event->x;
			view->details->drag_y = event->y;
		}

		if (event->button == 3) {
			do_popup_menu (widget, view, event);
		}
	}

	gtk_tree_path_free (path);

	/* We chained to the default handler in this method, so never
	 * let the default handler run */ 
	return TRUE;
}

static gboolean
button_release_callback (GtkWidget *widget, 
			 GdkEventButton *event, 
			 gpointer callback_data)
{
	NautilusListView *view;
	
	view = NAUTILUS_LIST_VIEW (callback_data);

	if (event->button == view->details->drag_button) {
		stop_drag_check (view);
		if (!view->details->drag_started &&
		    !view->details->ignore_button_release) {
			nautilus_list_view_did_not_drag (view, event);
		}
	}
	return FALSE;
}

static gboolean
popup_menu_callback (GtkWidget *widget, gpointer callback_data)
{
 	NautilusListView *view;

	view = NAUTILUS_LIST_VIEW (callback_data);

	do_popup_menu (widget, view, NULL);

	return TRUE;
}

static void
subdirectory_done_loading_callback (NautilusDirectory *directory, NautilusListView *view)
{
	nautilus_list_model_subdirectory_done_loading (view->details->model, directory);
}

static void
row_expanded_callback (GtkTreeView *treeview,
		       GtkTreeIter *iter,
		       GtkTreePath *path,
		       gpointer callback_data)
{
	NautilusListView *view;
	NautilusDirectory *directory;
	char *uri;

	view = NAUTILUS_LIST_VIEW (callback_data);

	if (!nautilus_list_model_load_subdirectory (view->details->model, path, &directory)) {
		return;
	}

	uri = nautilus_directory_get_uri (directory);
	DEBUG ("Row expaded callback for uri %s", uri);
	g_free (uri);

	nautilus_view_add_subdirectory (NAUTILUS_VIEW (view), directory);

	if (nautilus_directory_are_all_files_seen (directory)) {
		nautilus_list_model_subdirectory_done_loading (view->details->model,
							       directory);
	} else {
		g_signal_connect_object (directory, "done-loading",
					 G_CALLBACK (subdirectory_done_loading_callback),
					 view, 0);
	}

	nautilus_directory_unref (directory);
}

typedef struct {
	NautilusFile *file;
	NautilusDirectory *directory;
	NautilusListView *view;
} UnloadDelayData;

static void
unload_delay_data_free (UnloadDelayData *unload_data)
{
	if (unload_data->view != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (unload_data->view),
					      (gpointer *) &unload_data->view);
	}

	nautilus_directory_unref (unload_data->directory);
	nautilus_file_unref (unload_data->file);

	g_slice_free (UnloadDelayData, unload_data);
}

static UnloadDelayData *
unload_delay_data_new (NautilusFile *file,
		       NautilusDirectory *parent_directory,
		       NautilusListView *view)
{
	UnloadDelayData *unload_data;

	unload_data = g_slice_new0 (UnloadDelayData);
	unload_data->view = view;
	unload_data->file = nautilus_file_ref (file);
	unload_data->directory = nautilus_directory_ref (parent_directory);

	g_object_add_weak_pointer (G_OBJECT (unload_data->view),
				   (gpointer *) &unload_data->view);

	return unload_data;
}

static gboolean
unload_file_timeout (gpointer data)
{
	UnloadDelayData *unload_data = data;
	GtkTreeIter iter;
	NautilusListModel *model;
	GtkTreePath *path;

	if (unload_data->view == NULL) {
		goto out;
	}

	model = unload_data->view->details->model;
	if (nautilus_list_model_get_tree_iter_from_file (model,
							 unload_data->file,
							 unload_data->directory,
							 &iter)) {
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
		if (!gtk_tree_view_row_expanded (unload_data->view->details->tree_view,
						 path)) {
			nautilus_list_model_unload_subdirectory (model, &iter);
		}
		gtk_tree_path_free (path);
	}

 out:
	unload_delay_data_free (unload_data);
	return FALSE;
}

static void
row_collapsed_callback (GtkTreeView *treeview,
			GtkTreeIter *iter,
			GtkTreePath *path,
			gpointer callback_data)
{
	NautilusListView *view;
	NautilusFile *file;
	NautilusDirectory *directory;
	GtkTreeIter parent;
	UnloadDelayData *unload_data;
	GtkTreeModel *model;
	char *uri;

	view = NAUTILUS_LIST_VIEW (callback_data);
	model = GTK_TREE_MODEL (view->details->model);

	gtk_tree_model_get (model, iter,
			    NAUTILUS_LIST_MODEL_FILE_COLUMN, &file,
			    -1);

	uri = nautilus_file_get_uri (file);
	DEBUG ("Row collapsed callback for uri %s", uri);
	g_free (uri);

	directory = NULL;
	if (gtk_tree_model_iter_parent (model, &parent, iter)) {
		gtk_tree_model_get (model, &parent,
				    NAUTILUS_LIST_MODEL_SUBDIRECTORY_COLUMN, &directory,
				    -1);
	}

	unload_data = unload_delay_data_new (file, directory, view);
	g_timeout_add_seconds (COLLAPSE_TO_UNLOAD_DELAY,
			       unload_file_timeout,
			       unload_data);

	nautilus_file_unref (file);
	nautilus_directory_unref (directory);
}

static void
subdirectory_unloaded_callback (NautilusListModel *model,
				NautilusDirectory *directory,
				gpointer callback_data)
{
	NautilusListView *view;
	
	g_return_if_fail (NAUTILUS_IS_LIST_MODEL (model));
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

	view = NAUTILUS_LIST_VIEW(callback_data);
	
	g_signal_handlers_disconnect_by_func (directory,
					      G_CALLBACK (subdirectory_done_loading_callback),
					      view);
	nautilus_view_remove_subdirectory (NAUTILUS_VIEW (view), directory);
}

static gboolean
key_press_callback (GtkWidget *widget, GdkEventKey *event, gpointer callback_data)
{
	NautilusView *view;
	GdkEventButton button_event = { 0 };
	gboolean handled;
	GtkTreeView *tree_view;
	GtkTreePath *path;

	tree_view = GTK_TREE_VIEW (widget);

	view = NAUTILUS_VIEW (callback_data);
	handled = FALSE;

	switch (event->keyval) {
	case GDK_KEY_F10:
		if (event->state & GDK_CONTROL_MASK) {
			nautilus_view_pop_up_background_context_menu (view, &button_event);
			handled = TRUE;
		}
		break;
	case GDK_KEY_Right:
		gtk_tree_view_get_cursor (tree_view, &path, NULL);
		if (path) {
			gtk_tree_view_expand_row (tree_view, path, FALSE);
			gtk_tree_path_free (path);
		}
		handled = TRUE;
		break;
	case GDK_KEY_Left:
		gtk_tree_view_get_cursor (tree_view, &path, NULL);
		if (path) {
			if (!gtk_tree_view_collapse_row (tree_view, path)) {
				/* if the row is already collapsed or doesn't have any children,
				 * jump to the parent row instead.
				 */
				if ((gtk_tree_path_get_depth (path) > 1) && gtk_tree_path_up (path)) {
					gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
				}
			}

			gtk_tree_path_free (path);
		}
		handled = TRUE;
		break;
	case GDK_KEY_space:
		if (event->state & GDK_CONTROL_MASK) {
			handled = FALSE;
			break;
		}
		if (!gtk_widget_has_focus (GTK_WIDGET (NAUTILUS_LIST_VIEW (view)->details->tree_view))) {
			handled = FALSE;
			break;
		}
		if ((event->state & GDK_SHIFT_MASK) != 0) {
			activate_selected_items_alternate (NAUTILUS_LIST_VIEW (view), NULL, TRUE);
		} else {
			preview_selected_items (NAUTILUS_LIST_VIEW (view));
		}
		handled = TRUE;
		break;
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
		if ((event->state & GDK_SHIFT_MASK) != 0) {
			activate_selected_items_alternate (NAUTILUS_LIST_VIEW (view), NULL, TRUE);
		} else {
			activate_selected_items (NAUTILUS_LIST_VIEW (view));
		}
		handled = TRUE;
		break;
	case GDK_KEY_v:
		/* Eat Control + v to not enable type ahead */
		if ((event->state & GDK_CONTROL_MASK) != 0) {
			handled = TRUE;
		}
		break;

	default:
		handled = FALSE;
	}

	return handled;
}

static gboolean
test_expand_row_callback (GtkTreeView *tree_view,
			  GtkTreeIter *iter,
			  GtkTreePath *path,
			  gboolean user_data)
{
	return !g_settings_get_boolean (nautilus_list_view_preferences,
					NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE);
}

static void
nautilus_list_view_reveal_selection (NautilusView *view)
{
	GList *selection;

	g_return_if_fail (NAUTILUS_IS_LIST_VIEW (view));

        selection = nautilus_view_get_selection (view);

	/* Make sure at least one of the selected items is scrolled into view */
	if (selection != NULL) {
		NautilusListView *list_view;
		NautilusFile *file;
		GtkTreeIter iter;
		GtkTreePath *path;
		
		list_view = NAUTILUS_LIST_VIEW (view);
		file = selection->data;
		if (nautilus_list_model_get_first_iter_for_file (list_view->details->model, file, &iter)) {
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_view->details->model), &iter);

			gtk_tree_view_scroll_to_cell (list_view->details->tree_view, path, NULL, FALSE, 0.0, 0.0);
			
			gtk_tree_path_free (path);
		}
	}

        nautilus_file_list_free (selection);
}

static gboolean
sort_criterion_changes_due_to_user (GtkTreeView *tree_view)
{
	GList *columns, *p;
	GtkTreeViewColumn *column;
	GSignalInvocationHint *ihint;
	gboolean ret;

	ret = FALSE;

	columns = gtk_tree_view_get_columns (tree_view);
	for (p = columns; p != NULL; p = p->next) {
		column = p->data;
		ihint = g_signal_get_invocation_hint (column);
		if (ihint != NULL) {
			ret = TRUE;
			break;
		}
	}
	g_list_free (columns);

	return ret;
}

static void
sort_column_changed_callback (GtkTreeSortable *sortable, 
			      NautilusListView *view)
{
	NautilusFile *file;
	gint sort_column_id, default_sort_column_id;
	GtkSortType reversed;
	GQuark sort_attr, default_sort_attr;
	char *reversed_attr, *default_reversed_attr;
	gboolean default_sort_reversed;

	file = nautilus_view_get_directory_as_file (NAUTILUS_VIEW (view));

	gtk_tree_sortable_get_sort_column_id (sortable, &sort_column_id, &reversed);
	sort_attr = nautilus_list_model_get_attribute_from_sort_column_id (view->details->model, sort_column_id);

	default_sort_column_id = nautilus_list_model_get_sort_column_id_from_attribute (view->details->model,
										  g_quark_from_string (get_default_sort_order (file, &default_sort_reversed)));
	default_sort_attr = nautilus_list_model_get_attribute_from_sort_column_id (view->details->model, default_sort_column_id);
	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
				    g_quark_to_string (default_sort_attr), g_quark_to_string (sort_attr));

	default_reversed_attr = (default_sort_reversed ? "true" : "false");

	if (view->details->last_sort_attr != sort_attr &&
	    sort_criterion_changes_due_to_user (view->details->tree_view)) {
		/* at this point, the sort order is always GTK_SORT_ASCENDING, if the sort column ID
		 * switched. Invert the sort order, if it's the default criterion with a reversed preference,
		 * or if it makes sense for the attribute (i.e. date). */
		if (sort_attr == default_sort_attr) {
			/* use value from preferences */
			reversed = g_settings_get_boolean (nautilus_preferences,
							   NAUTILUS_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER);
		} else {
			reversed = nautilus_file_is_date_sort_attribute_q (sort_attr);
		}

		if (reversed) {
			g_signal_handlers_block_by_func (sortable, sort_column_changed_callback, view);
			gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (view->details->model),
							      sort_column_id,
							      GTK_SORT_DESCENDING);
			g_signal_handlers_unblock_by_func (sortable, sort_column_changed_callback, view);
		}
	}


	reversed_attr = (reversed ? "true" : "false");
	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
				    default_reversed_attr, reversed_attr);

	/* Make sure selected item(s) is visible after sort */
	nautilus_list_view_reveal_selection (NAUTILUS_VIEW (view));

	view->details->last_sort_attr = sort_attr;
}

static void
editable_focus_out_cb (GtkWidget *widget,
		       GdkEvent *event,
		       gpointer user_data)
{
	NautilusListView *view = user_data;

	view->details->editable_widget = NULL;

	nautilus_view_set_is_renaming (NAUTILUS_VIEW (view), FALSE);
	nautilus_view_unfreeze_updates (NAUTILUS_VIEW (view));
}

static void
cell_renderer_editing_started_cb (GtkCellRenderer *renderer,
				  GtkCellEditable *editable,
				  const gchar *path_str,
				  NautilusListView *list_view)
{
	GtkEntry *entry;

	entry = GTK_ENTRY (editable);
	list_view->details->editable_widget = editable;

	/* Free a previously allocated original_name */
	g_free (list_view->details->original_name);

	list_view->details->original_name = g_strdup (gtk_entry_get_text (entry));

	g_signal_connect (entry, "focus-out-event",
			  G_CALLBACK (editable_focus_out_cb), list_view);

	nautilus_clipboard_set_up_editable
		(GTK_EDITABLE (entry),
		 nautilus_view_get_ui_manager (NAUTILUS_VIEW (list_view)),
		 FALSE);
}

static void
cell_renderer_editing_canceled (GtkCellRendererText *cell,
				NautilusListView    *view)
{
	view->details->editable_widget = NULL;

	nautilus_view_set_is_renaming (NAUTILUS_VIEW (view), FALSE);
	nautilus_view_unfreeze_updates (NAUTILUS_VIEW (view));
}

static void
cell_renderer_edited (GtkCellRendererText *cell,
		      const char          *path_str,
		      const char          *new_text,
		      NautilusListView    *view)
{
	GtkTreePath *path;
	NautilusFile *file;
	GtkTreeIter iter;

	view->details->editable_widget = NULL;
	nautilus_view_set_is_renaming (NAUTILUS_VIEW (view), FALSE);

	/* Don't allow a rename with an empty string. Revert to original 
	 * without notifying the user.
	 */
	if (new_text[0] == '\0') {
		g_object_set (G_OBJECT (view->details->file_name_cell),
			      "editable", FALSE,
			      NULL);
		nautilus_view_unfreeze_updates (NAUTILUS_VIEW (view));
		return;
	}
	
	path = gtk_tree_path_new_from_string (path_str);

	gtk_tree_model_get_iter (GTK_TREE_MODEL (view->details->model),
				 &iter, path);

	gtk_tree_path_free (path);
	
	gtk_tree_model_get (GTK_TREE_MODEL (view->details->model),
			    &iter,
			    NAUTILUS_LIST_MODEL_FILE_COLUMN, &file,
			    -1);

	/* Only rename if name actually changed */
	if (strcmp (new_text, view->details->original_name) != 0) {
		view->details->renaming_file = nautilus_file_ref (file);
		view->details->rename_done = FALSE;
		nautilus_rename_file (file, new_text, nautilus_list_view_rename_callback, g_object_ref (view));
		g_free (view->details->original_name);
		view->details->original_name = g_strdup (new_text);
	}
	
	nautilus_file_unref (file);

	/*We're done editing - make the filename-cells readonly again.*/
	g_object_set (G_OBJECT (view->details->file_name_cell),
		      "editable", FALSE,
		      NULL);

	nautilus_view_unfreeze_updates (NAUTILUS_VIEW (view));
}

static char *
get_root_uri_callback (NautilusTreeViewDragDest *dest,
		       gpointer user_data)
{
	NautilusListView *view;
	
	view = NAUTILUS_LIST_VIEW (user_data);

	return nautilus_view_get_uri (NAUTILUS_VIEW (view));
}

static NautilusFile *
get_file_for_path_callback (NautilusTreeViewDragDest *dest,
			    GtkTreePath *path,
			    gpointer user_data)
{
	NautilusListView *view;
	
	view = NAUTILUS_LIST_VIEW (user_data);

	return nautilus_list_model_file_for_path (view->details->model, path);
}

/* Handles an URL received from Mozilla */
static void
list_view_handle_netscape_url (NautilusTreeViewDragDest *dest, const char *encoded_url,
			       const char *target_uri, GdkDragAction action, int x, int y, NautilusListView *view)
{
	nautilus_view_handle_netscape_url_drop (NAUTILUS_VIEW (view),
						encoded_url, target_uri, action, x, y);
}

static void
list_view_handle_uri_list (NautilusTreeViewDragDest *dest, const char *item_uris,
			   const char *target_uri,
			   GdkDragAction action, int x, int y, NautilusListView *view)
{
	nautilus_view_handle_uri_list_drop (NAUTILUS_VIEW (view),
					    item_uris, target_uri, action, x, y);
}

static void
list_view_handle_text (NautilusTreeViewDragDest *dest, const char *text,
		       const char *target_uri,
		       GdkDragAction action, int x, int y, NautilusListView *view)
{
	nautilus_view_handle_text_drop (NAUTILUS_VIEW (view),
					text, target_uri, action, x, y);
}

static void
list_view_handle_raw (NautilusTreeViewDragDest *dest, const char *raw_data,
		      int length, const char *target_uri, const char *direct_save_uri,
		      GdkDragAction action, int x, int y, NautilusListView *view)
{
	nautilus_view_handle_raw_drop (NAUTILUS_VIEW (view),
				       raw_data, length, target_uri, direct_save_uri,
				       action, x, y);
}

static void
list_view_handle_hover (NautilusTreeViewDragDest *dest,
			const char *target_uri,
			NautilusListView *view)
{
	nautilus_view_handle_hover (NAUTILUS_VIEW (view), target_uri);
}

static void
move_copy_items_callback (NautilusTreeViewDragDest *dest,
			  const GList *item_uris,
			  const char *target_uri,
			  guint action,
			  int x, 
			  int y,
			  gpointer user_data)

{
	NautilusView *view = user_data;

	nautilus_clipboard_clear_if_colliding_uris (GTK_WIDGET (view),
						    item_uris,
						    nautilus_view_get_copied_files_atom (view));
	nautilus_view_move_copy_items (view,
				       item_uris,
				       NULL,
				       target_uri,
				       action,
				       x, y);
}

static void
apply_columns_settings (NautilusListView *list_view,
			char **column_order,
			char **visible_columns)
{
	GList *all_columns;
	NautilusFile *file;
	GList *old_view_columns, *view_columns;
	GHashTable *visible_columns_hash;
	GtkTreeViewColumn *prev_view_column;
	GList *l;
	int i;

	file = nautilus_view_get_directory_as_file (NAUTILUS_VIEW (list_view));

	/* prepare ordered list of view columns using column_order and visible_columns */
	view_columns = NULL;

	all_columns = nautilus_get_columns_for_file (file);
	all_columns = nautilus_sort_columns (all_columns, column_order);

	/* hash table to lookup if a given column should be visible */
	visible_columns_hash = g_hash_table_new_full (g_str_hash,
						      g_str_equal,
						      (GDestroyNotify) g_free,
						      (GDestroyNotify) g_free);
	/* always show name column */
	g_hash_table_insert (visible_columns_hash, g_strdup ("name"), g_strdup ("name"));
	if (visible_columns != NULL) {
		for (i = 0; visible_columns[i] != NULL; ++i) {
			g_hash_table_insert (visible_columns_hash,
					     g_ascii_strdown (visible_columns[i], -1),
					     g_ascii_strdown (visible_columns[i], -1));
		}
	}

	for (l = all_columns; l != NULL; l = l->next) {
		char *name;
		char *lowercase;

		g_object_get (G_OBJECT (l->data), "name", &name, NULL);
		lowercase = g_ascii_strdown (name, -1);

		if (g_hash_table_lookup (visible_columns_hash, lowercase) != NULL) {
			GtkTreeViewColumn *view_column;

			view_column = g_hash_table_lookup (list_view->details->columns, name);
			if (view_column != NULL) {
				view_columns = g_list_prepend (view_columns, view_column);
			}
		}

		g_free (name);
		g_free (lowercase);
	}

	g_hash_table_destroy (visible_columns_hash);
	nautilus_column_list_free (all_columns);

	view_columns = g_list_reverse (view_columns);

	/* remove columns that are not present in the configuration */
	old_view_columns = gtk_tree_view_get_columns (list_view->details->tree_view);
	for (l = old_view_columns; l != NULL; l = l->next) {
		if (g_list_find (view_columns, l->data) == NULL) {
			gtk_tree_view_remove_column (list_view->details->tree_view, l->data);
		}
	}
	g_list_free (old_view_columns);

	/* append new columns from the configuration */
	old_view_columns = gtk_tree_view_get_columns (list_view->details->tree_view);
	for (l = view_columns; l != NULL; l = l->next) {
		if (g_list_find (old_view_columns, l->data) == NULL) {
			gtk_tree_view_append_column (list_view->details->tree_view, l->data);
		}
	}
	g_list_free (old_view_columns);

	/* place columns in the correct order */
	prev_view_column = NULL;
	for (l = view_columns; l != NULL; l = l->next) {
		gtk_tree_view_move_column_after (list_view->details->tree_view, l->data, prev_view_column);
		prev_view_column = l->data;
	}
	g_list_free (view_columns);
}

static void
filename_cell_data_func (GtkTreeViewColumn *column,
			 GtkCellRenderer   *renderer,
			 GtkTreeModel      *model,
			 GtkTreeIter       *iter,
			 NautilusListView        *view)
{
	char *text;
	GtkTreePath *path;
	PangoUnderline underline;

	gtk_tree_model_get (model, iter,
			    view->details->file_name_column_num, &text,
			    -1);

	if (get_click_policy () == NAUTILUS_CLICK_POLICY_SINGLE) {
		path = gtk_tree_model_get_path (model, iter);

		if (view->details->hover_path == NULL ||
		    gtk_tree_path_compare (path, view->details->hover_path)) {
			underline = PANGO_UNDERLINE_NONE;
		} else {
			underline = PANGO_UNDERLINE_SINGLE;
		}

		gtk_tree_path_free (path);
	} else {
		underline = PANGO_UNDERLINE_NONE;
	}

	g_object_set (G_OBJECT (renderer),
		      "text", text,
		      "underline", underline,
		      NULL);
	g_free (text);
}

static void
set_up_pixbuf_size (NautilusListView *view)
{
	int icon_size;

	/* Make all rows the same size. */
	icon_size = nautilus_get_icon_size_for_zoom_level (view->details->zoom_level);
	gtk_cell_renderer_set_fixed_size (GTK_CELL_RENDERER (view->details->pixbuf_cell),
					  -1, icon_size);


	/* FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=641518 */
	gtk_tree_view_columns_autosize (view->details->tree_view);
}

static void
create_and_set_up_tree_view (NautilusListView *view)
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	AtkObject *atk_obj;
	GList *nautilus_columns;
	GList *l;
	gchar **default_column_order, **default_visible_columns;
	
	view->details->tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
	view->details->columns = g_hash_table_new_full (g_str_hash, 
							g_str_equal,
							(GDestroyNotify)g_free,
							(GDestroyNotify) g_object_unref);
	gtk_tree_view_set_enable_search (view->details->tree_view, FALSE);

	view->details->drag_dest = 
		nautilus_tree_view_drag_dest_new (view->details->tree_view);

	g_signal_connect_object (view->details->drag_dest,
				 "get-root-uri",
				 G_CALLBACK (get_root_uri_callback),
				 view, 0);
	g_signal_connect_object (view->details->drag_dest,
				 "get-file-for-path",
				 G_CALLBACK (get_file_for_path_callback),
				 view, 0);
	g_signal_connect_object (view->details->drag_dest,
				 "move-copy-items",
				 G_CALLBACK (move_copy_items_callback),
				 view, 0);
	g_signal_connect_object (view->details->drag_dest, "handle-netscape-url",
				 G_CALLBACK (list_view_handle_netscape_url), view, 0);
	g_signal_connect_object (view->details->drag_dest, "handle-uri-list",
				 G_CALLBACK (list_view_handle_uri_list), view, 0);
	g_signal_connect_object (view->details->drag_dest, "handle-text",
				 G_CALLBACK (list_view_handle_text), view, 0);
	g_signal_connect_object (view->details->drag_dest, "handle-raw",
				 G_CALLBACK (list_view_handle_raw), view, 0);
	g_signal_connect_object (view->details->drag_dest, "handle-hover",
				 G_CALLBACK (list_view_handle_hover), view, 0);

	g_signal_connect_object (gtk_tree_view_get_selection (view->details->tree_view),
				 "changed",
				 G_CALLBACK (list_selection_changed_callback), view, 0);

	g_signal_connect_object (view->details->tree_view, "drag-begin",
				 G_CALLBACK (drag_begin_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "drag-data-get",
				 G_CALLBACK (drag_data_get_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "motion-notify-event",
				 G_CALLBACK (motion_notify_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "enter-notify-event",
				 G_CALLBACK (enter_notify_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "leave-notify-event",
				 G_CALLBACK (leave_notify_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "button-press-event",
				 G_CALLBACK (button_press_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "button-release-event",
				 G_CALLBACK (button_release_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "key-press-event",
				 G_CALLBACK (key_press_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "test-expand-row",
				 G_CALLBACK (test_expand_row_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "popup-menu",
                                 G_CALLBACK (popup_menu_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "row-expanded",
                                 G_CALLBACK (row_expanded_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "row-collapsed",
                                 G_CALLBACK (row_collapsed_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "row-activated",
                                 G_CALLBACK (row_activated_callback), view, 0);
	
	view->details->model = g_object_new (NAUTILUS_TYPE_LIST_MODEL, NULL);
	gtk_tree_view_set_model (view->details->tree_view, GTK_TREE_MODEL (view->details->model));
	/* Need the model for the dnd drop icon "accept" change */
	nautilus_list_model_set_drag_view (NAUTILUS_LIST_MODEL (view->details->model),
				     view->details->tree_view,  0, 0);

	g_signal_connect_object (view->details->model, "sort-column-changed",
				 G_CALLBACK (sort_column_changed_callback), view, 0);
	
	g_signal_connect_object (view->details->model, "subdirectory-unloaded",
				 G_CALLBACK (subdirectory_unloaded_callback), view, 0);

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (view->details->tree_view), GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_rules_hint (view->details->tree_view, TRUE);

	g_settings_bind (nautilus_list_view_preferences, NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE,
			 view->details->tree_view, "show-expanders",
			 G_SETTINGS_BIND_DEFAULT);

	nautilus_columns = nautilus_get_all_columns ();

	for (l = nautilus_columns; l != NULL; l = l->next) {
		NautilusColumn *nautilus_column;
		int column_num;		
		char *name;
		char *label;
		float xalign;
		GtkSortType sort_order;

		nautilus_column = NAUTILUS_COLUMN (l->data);

		g_object_get (nautilus_column, 
			      "name", &name, 
			      "label", &label,
			      "xalign", &xalign,
			      "default-sort-order", &sort_order,
			      NULL);

		column_num = nautilus_list_model_add_column (view->details->model,
						       nautilus_column);

		/* Created the name column specially, because it
		 * has the icon in it.*/
		if (!strcmp (name, "name")) {
			/* Create the file name column */
			view->details->file_name_column = gtk_tree_view_column_new ();
			g_object_ref_sink (view->details->file_name_column);
			view->details->file_name_column_num = column_num;
			
			g_hash_table_insert (view->details->columns,
					     g_strdup ("name"), 
					     view->details->file_name_column);

			gtk_tree_view_set_search_column (view->details->tree_view, column_num);

			gtk_tree_view_column_set_sort_column_id (view->details->file_name_column, column_num);
			gtk_tree_view_column_set_title (view->details->file_name_column, _("Name"));
			gtk_tree_view_column_set_resizable (view->details->file_name_column, TRUE);
			gtk_tree_view_column_set_expand (view->details->file_name_column, TRUE);

			/* Initial padding */
			cell = gtk_cell_renderer_text_new ();
			gtk_tree_view_column_pack_start (view->details->file_name_column, cell, FALSE);
			g_object_set (cell, "xpad", 6, NULL);
			g_settings_bind (nautilus_list_view_preferences, NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE,
					 cell, "visible",
					 G_SETTINGS_BIND_INVERT_BOOLEAN | G_SETTINGS_BIND_GET);

			/* File icon */
			cell = gtk_cell_renderer_pixbuf_new ();
			view->details->pixbuf_cell = (GtkCellRendererPixbuf *)cell;
			set_up_pixbuf_size (view);

			gtk_tree_view_column_pack_start (view->details->file_name_column, cell, FALSE);
			gtk_tree_view_column_set_attributes (view->details->file_name_column,
							     cell,
							     "pixbuf", nautilus_list_model_get_column_id_from_zoom_level (view->details->zoom_level),
							     NULL);
			
			cell = gtk_cell_renderer_text_new ();
			view->details->file_name_cell = (GtkCellRendererText *)cell;
			g_object_set (cell,
				      "ellipsize", PANGO_ELLIPSIZE_END,
				      "single-paragraph-mode", TRUE,
				      "xpad", 5,
				      NULL);

			g_signal_connect (cell, "edited", G_CALLBACK (cell_renderer_edited), view);
			g_signal_connect (cell, "editing-canceled", G_CALLBACK (cell_renderer_editing_canceled), view);
			g_signal_connect (cell, "editing-started", G_CALLBACK (cell_renderer_editing_started_cb), view);

			gtk_tree_view_column_pack_start (view->details->file_name_column, cell, TRUE);
			gtk_tree_view_column_set_cell_data_func (view->details->file_name_column, cell,
								 (GtkTreeCellDataFunc) filename_cell_data_func,
								 view, NULL);
		} else {
			cell = gtk_cell_renderer_text_new ();
			g_object_set (cell,
				      "xalign", xalign,
				      "xpad", 5,
				      NULL);
			view->details->cells = g_list_append (view->details->cells,
							      cell);
			column = gtk_tree_view_column_new_with_attributes (label,
									   cell,
									   "text", column_num,
									   NULL);
			g_object_ref_sink (column);
			gtk_tree_view_column_set_sort_column_id (column, column_num);
			g_hash_table_insert (view->details->columns, 
					     g_strdup (name), 
					     column);
			
			gtk_tree_view_column_set_resizable (column, TRUE);
			gtk_tree_view_column_set_visible (column, TRUE);
			gtk_tree_view_column_set_sort_order (column, sort_order);
		}
		g_free (name);
		g_free (label);
	}
	nautilus_column_list_free (nautilus_columns);

	default_visible_columns = g_settings_get_strv (nautilus_list_view_preferences,
						       NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
	default_column_order = g_settings_get_strv (nautilus_list_view_preferences,
						    NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);

	/* Apply the default column order and visible columns, to get it
	 * right most of the time. The metadata will be checked when a 
	 * folder is loaded */
	apply_columns_settings (view,
				default_column_order,
				default_visible_columns);

	gtk_widget_show (GTK_WIDGET (view->details->tree_view));
	gtk_container_add (GTK_CONTAINER (view), GTK_WIDGET (view->details->tree_view));

        atk_obj = gtk_widget_get_accessible (GTK_WIDGET (view->details->tree_view));
        atk_object_set_name (atk_obj, _("List View"));

	g_strfreev (default_visible_columns);
	g_strfreev (default_column_order);
}

static void
nautilus_list_view_add_file (NautilusView *view, NautilusFile *file, NautilusDirectory *directory)
{
	NautilusListModel *model;

	model = NAUTILUS_LIST_VIEW (view)->details->model;
	nautilus_list_model_add_file (model, file, directory);
}

static char **
get_visible_columns (NautilusListView *list_view)
{
	NautilusFile *file;
	GList *visible_columns;
	char **ret;

	ret = NULL;

	file = nautilus_view_get_directory_as_file (NAUTILUS_VIEW (list_view));

	visible_columns = nautilus_file_get_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS);

	if (visible_columns) {
		GPtrArray *res;
		GList *l;

		res = g_ptr_array_new ();
		for (l = visible_columns; l != NULL; l = l->next) {
			g_ptr_array_add (res, l->data);
		}
		g_ptr_array_add (res, NULL);

		ret = (char **) g_ptr_array_free (res, FALSE);
		g_list_free (visible_columns);
	}

	if (ret != NULL) {
		return ret;
	}

	return nautilus_file_is_in_trash (file) ?
		g_strdupv ((gchar **) default_trash_visible_columns) :
		g_settings_get_strv (nautilus_list_view_preferences,
				     NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
}

static char **
get_column_order (NautilusListView *list_view)
{
	NautilusFile *file;
	GList *column_order;
	char **ret;

	ret = NULL;

	file = nautilus_view_get_directory_as_file (NAUTILUS_VIEW (list_view));

	column_order = nautilus_file_get_metadata_list 
		(file,
		 NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER);

	if (column_order) {
		GPtrArray *res;
		GList *l;

		res = g_ptr_array_new ();
		for (l = column_order; l != NULL; l = l->next) {
			g_ptr_array_add (res, l->data);
		}
		g_ptr_array_add (res, NULL);

		ret = (char **) g_ptr_array_free (res, FALSE);
		g_list_free (column_order);
	}

	if (ret != NULL) {
		return ret;
	}

	return nautilus_file_is_in_trash (file) ?
		g_strdupv ((gchar **) default_trash_columns_order) :
		g_settings_get_strv (nautilus_list_view_preferences,
				     NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);
}

static void
set_columns_settings_from_metadata_and_preferences (NautilusListView *list_view)
{
	char **column_order;
	char **visible_columns;

	column_order = get_column_order (list_view);
	visible_columns = get_visible_columns (list_view);

	apply_columns_settings (list_view, column_order, visible_columns);

	g_strfreev (column_order);
	g_strfreev (visible_columns);
}

static void
set_sort_order_from_metadata_and_preferences (NautilusListView *list_view)
{
	char *sort_attribute;
	int sort_column_id;
	NautilusFile *file;
	gboolean sort_reversed, default_sort_reversed;
	const gchar *default_sort_order;
	
	file = nautilus_view_get_directory_as_file (NAUTILUS_VIEW (list_view));
	sort_attribute = nautilus_file_get_metadata (file,
						     NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
						     NULL);
	sort_column_id = nautilus_list_model_get_sort_column_id_from_attribute (list_view->details->model,
									  g_quark_from_string (sort_attribute));
	g_free (sort_attribute);

	default_sort_order = get_default_sort_order (file, &default_sort_reversed);
	
	if (sort_column_id == -1) {
		sort_column_id =
			nautilus_list_model_get_sort_column_id_from_attribute (list_view->details->model,
									 g_quark_from_string (default_sort_order));
	}

	sort_reversed = nautilus_file_get_boolean_metadata (file,
							    NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
							    default_sort_reversed);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_view->details->model),
					      sort_column_id,
					      sort_reversed ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING);					      
}

static NautilusZoomLevel
get_default_zoom_level (void) {
	NautilusZoomLevel default_zoom_level;

	default_zoom_level = g_settings_get_enum (nautilus_list_view_preferences,
						  NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL);

	if (default_zoom_level <  NAUTILUS_ZOOM_LEVEL_SMALLEST
	    || NAUTILUS_ZOOM_LEVEL_LARGEST < default_zoom_level) {
		default_zoom_level = NAUTILUS_ZOOM_LEVEL_SMALL;
	}

	return default_zoom_level;
}

static void
nautilus_list_view_begin_loading (NautilusView *view)
{
	NautilusListView *list_view;

	list_view = NAUTILUS_LIST_VIEW (view);

	set_sort_order_from_metadata_and_preferences (list_view);
	set_columns_settings_from_metadata_and_preferences (list_view);
}

static void
stop_cell_editing (NautilusListView *list_view)
{
	GtkTreeViewColumn *column;

	/* Stop an ongoing rename to commit the name changes when the user
	 * changes directories without exiting cell edit mode. It also prevents
	 * the edited handler from being called on the cleared list model.
	 */
	column = list_view->details->file_name_column;
	if (column != NULL && list_view->details->editable_widget != NULL &&
	    GTK_IS_CELL_EDITABLE (list_view->details->editable_widget)) {
		gtk_cell_editable_editing_done (list_view->details->editable_widget);
	}

	g_clear_object (&list_view->details->renaming_file);
}

static void
nautilus_list_view_clear (NautilusView *view)
{
	NautilusListView *list_view;

	list_view = NAUTILUS_LIST_VIEW (view);

	if (list_view->details->model != NULL) {
		stop_cell_editing (list_view);
		nautilus_list_model_clear (list_view->details->model);
	}
}

static void
nautilus_list_view_rename_callback (NautilusFile *file,
				    GFile *result_location,
				    GError *error,
				    gpointer callback_data)
{
	NautilusListView *view;
	
	view = NAUTILUS_LIST_VIEW (callback_data);

	if (view->details->renaming_file) {
		view->details->rename_done = TRUE;
		
		if (error != NULL) {
			/* If the rename failed (or was cancelled), kill renaming_file.
			 * We won't get a change event for the rename, so otherwise
			 * it would stay around forever.
			 */
			nautilus_file_unref (view->details->renaming_file);
			view->details->renaming_file = NULL;
		}
	}
	
	g_object_unref (view);
}


static void
nautilus_list_view_file_changed (NautilusView *view, NautilusFile *file, NautilusDirectory *directory)
{
	NautilusListView *listview;
	GtkTreeIter iter;
	GtkTreePath *file_path;

	listview = NAUTILUS_LIST_VIEW (view);
	
	nautilus_list_model_file_changed (listview->details->model, file, directory);

	if (listview->details->renaming_file != NULL &&
	    file == listview->details->renaming_file &&
	    listview->details->rename_done) {
		/* This is (probably) the result of the rename operation, and
		 * the tree-view changes above could have resorted the list, so
		 * scroll to the new position
		 */
		if (nautilus_list_model_get_tree_iter_from_file (listview->details->model, file, directory, &iter)) {
			file_path = gtk_tree_model_get_path (GTK_TREE_MODEL (listview->details->model), &iter);
			gtk_tree_view_scroll_to_cell (listview->details->tree_view,
						      file_path, NULL,
						      FALSE, 0.0, 0.0);
			gtk_tree_path_free (file_path);
		}
		
		nautilus_file_unref (listview->details->renaming_file);
		listview->details->renaming_file = NULL;
	}
}

typedef struct {
	GtkTreePath *path;
	gboolean is_common;
	gboolean is_root;
} HasCommonParentData;

static void
tree_selection_has_common_parent_foreach_func (GtkTreeModel *model,
						GtkTreePath *path,
						GtkTreeIter *iter,
						gpointer user_data)
{
	HasCommonParentData *data;
	GtkTreePath *parent_path;
	gboolean has_parent;

	data = (HasCommonParentData *) user_data;

	parent_path = gtk_tree_path_copy (path);
	gtk_tree_path_up (parent_path);

	has_parent = (gtk_tree_path_get_depth (parent_path) > 0) ? TRUE : FALSE;

	if (!has_parent) {
		data->is_root = TRUE;
	}

	if (data->is_common && !data->is_root) {
		if (data->path == NULL) {
			data->path = gtk_tree_path_copy (parent_path);
		} else if (gtk_tree_path_compare (data->path, parent_path) != 0) {
			data->is_common = FALSE;
		}
	}

	gtk_tree_path_free (parent_path);
}

static void
tree_selection_has_common_parent (GtkTreeSelection *selection,
				  gboolean *is_common,
				  gboolean *is_root)
{
	HasCommonParentData data;

	g_assert (is_common != NULL);
	g_assert (is_root != NULL);

	data.path = NULL;
	data.is_common = *is_common = TRUE;
	data.is_root = *is_root = FALSE;

	gtk_tree_selection_selected_foreach (selection,
					     tree_selection_has_common_parent_foreach_func,
					     &data);

	*is_common = data.is_common;
	*is_root = data.is_root;

	if (data.path != NULL) {
		gtk_tree_path_free (data.path);
	}
}

static char *
nautilus_list_view_get_backing_uri (NautilusView *view)
{
	NautilusListView *list_view;
	NautilusListModel *list_model;
	NautilusFile *file;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GList *paths;
	guint length;
	char *uri;

	g_return_val_if_fail (NAUTILUS_IS_LIST_VIEW (view), NULL);

	list_view = NAUTILUS_LIST_VIEW (view);
	list_model = list_view->details->model;
	tree_view = list_view->details->tree_view;

	g_assert (list_model);

	/* We currently handle three common cases here:
	 * (a) if the selection contains non-filesystem items (i.e., the
	 *     "(Empty)" label), we return the uri of the parent.
	 * (b) if the selection consists of exactly one _expanded_ directory, we
	 *     return its URI.
	 * (c) if the selection consists of either exactly one item which is not
	 *     an expanded directory) or multiple items in the same directory,
	 *     we return the URI of the common parent.
	 */

	uri = NULL;

	selection = gtk_tree_view_get_selection (tree_view);
	length = gtk_tree_selection_count_selected_rows (selection);

	if (length == 1) {

		paths = gtk_tree_selection_get_selected_rows (selection, NULL);
		path = (GtkTreePath *) paths->data;

		file = nautilus_list_model_file_for_path (list_model, path);
		if (file == NULL) {
			/* The selected item is a label, not a file */
			gtk_tree_path_up (path);
			file = nautilus_list_model_file_for_path (list_model, path);
		}

		if (file != NULL) {
			if (nautilus_file_is_directory (file) &&
			    gtk_tree_view_row_expanded (tree_view, path)) {
				uri = nautilus_file_get_uri (file);
			}
			nautilus_file_unref (file);
		}

		gtk_tree_path_free (path);
		g_list_free (paths);
	}

	if (uri == NULL && length > 0) {

		gboolean is_common, is_root;

		/* Check that all the selected items belong to the same
		 * directory and that directory is not the root directory (which
		 * is handled by NautilusView::get_backing_directory.) */

		tree_selection_has_common_parent (selection, &is_common, &is_root);

		if (is_common && !is_root) {
			paths = gtk_tree_selection_get_selected_rows (selection, NULL);
			path = (GtkTreePath *) paths->data;

			file = nautilus_list_model_file_for_path (list_model, path);
			g_assert (file != NULL);
			uri = nautilus_file_get_parent_uri (file);
			nautilus_file_unref (file);

			g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);
		}
	}

	if (uri != NULL) {
		return uri;
	}

	return NAUTILUS_VIEW_CLASS (nautilus_list_view_parent_class)->get_backing_uri (view);
}

static void
nautilus_list_view_get_selection_foreach_func (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	GList **list;
	NautilusFile *file;
	
	list = data;

	gtk_tree_model_get (model, iter,
			    NAUTILUS_LIST_MODEL_FILE_COLUMN, &file,
			    -1);

	if (file != NULL) {
		(* list) = g_list_prepend ((* list), file);
	}
}

static GList *
nautilus_list_view_get_selection (NautilusView *view)
{
	GList *list;

	list = NULL;

	gtk_tree_selection_selected_foreach (gtk_tree_view_get_selection (NAUTILUS_LIST_VIEW (view)->details->tree_view),
					     nautilus_list_view_get_selection_foreach_func, &list);

	return g_list_reverse (list);
}

static void
nautilus_list_view_get_selection_for_file_transfer_foreach_func (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	NautilusFile *file;
	struct SelectionForeachData *selection_data;
	GtkTreeIter parent, child;

	selection_data = data;

	gtk_tree_model_get (model, iter,
			    NAUTILUS_LIST_MODEL_FILE_COLUMN, &file,
			    -1);

	if (file != NULL) {
		/* If the parent folder is also selected, don't include this file in the
		 * file operation, since that would copy it to the toplevel target instead
		 * of keeping it as a child of the copied folder
		 */
		child = *iter;
		while (gtk_tree_model_iter_parent (model, &parent, &child)) {
			if (gtk_tree_selection_iter_is_selected (selection_data->selection,
								 &parent)) {
				return;
			}
			child = parent;
		}
		
		nautilus_file_ref (file);
		selection_data->list = g_list_prepend (selection_data->list, file);
	}
}


static GList *
nautilus_list_view_get_selection_for_file_transfer (NautilusView *view)
{
	struct SelectionForeachData selection_data;

	selection_data.list = NULL;
	selection_data.selection = gtk_tree_view_get_selection (NAUTILUS_LIST_VIEW (view)->details->tree_view);

	gtk_tree_selection_selected_foreach (selection_data.selection,
					     nautilus_list_view_get_selection_for_file_transfer_foreach_func, &selection_data);

	return g_list_reverse (selection_data.list);
}

static gboolean
nautilus_list_view_is_empty (NautilusView *view)
{
	return nautilus_list_model_is_empty (NAUTILUS_LIST_VIEW (view)->details->model);
}

static void
nautilus_list_view_end_file_changes (NautilusView *view)
{
	NautilusListView *list_view;

	list_view = NAUTILUS_LIST_VIEW (view);

	if (list_view->details->new_selection_path) {
		gtk_tree_view_set_cursor (list_view->details->tree_view,
					  list_view->details->new_selection_path,
					  NULL, FALSE);
		gtk_tree_path_free (list_view->details->new_selection_path);
		list_view->details->new_selection_path = NULL;
	}
}

static void
nautilus_list_view_remove_file (NautilusView *view, NautilusFile *file, NautilusDirectory *directory)
{
	GtkTreePath *path;
	GtkTreePath *file_path;
	GtkTreeIter iter;
	GtkTreeIter temp_iter;
	GtkTreeRowReference* row_reference;
	NautilusListView *list_view;
	GtkTreeModel* tree_model; 
	GtkTreeSelection *selection;

	path = NULL;
	row_reference = NULL;
	list_view = NAUTILUS_LIST_VIEW (view);
	tree_model = GTK_TREE_MODEL(list_view->details->model);
	
	if (nautilus_list_model_get_tree_iter_from_file (list_view->details->model, file, directory, &iter)) {
		selection = gtk_tree_view_get_selection (list_view->details->tree_view);
		file_path = gtk_tree_model_get_path (tree_model, &iter);

		if (gtk_tree_selection_path_is_selected (selection, file_path)) {
			/* get reference for next element in the list view. If the element to be deleted is the 
			 * last one, get reference to previous element. If there is only one element in view
			 * no need to select anything.
			 */
			temp_iter = iter;

			if (gtk_tree_model_iter_next (tree_model, &iter)) {
				path = gtk_tree_model_get_path (tree_model, &iter);
				row_reference = gtk_tree_row_reference_new (tree_model, path);
			} else {
				path = gtk_tree_model_get_path (tree_model, &temp_iter);
				if (gtk_tree_path_prev (path)) {
					row_reference = gtk_tree_row_reference_new (tree_model, path);
				}
			}
			gtk_tree_path_free (path);
		}
       
		gtk_tree_path_free (file_path);
		
		nautilus_list_model_remove_file (list_view->details->model, file, directory);

		if (gtk_tree_row_reference_valid (row_reference)) {
			if (list_view->details->new_selection_path) {
				gtk_tree_path_free (list_view->details->new_selection_path);
			}
			list_view->details->new_selection_path = gtk_tree_row_reference_get_path (row_reference);
		}
	  
		if (row_reference) {
			gtk_tree_row_reference_free (row_reference);
		}
	}   
	
	
}

static void
nautilus_list_view_set_selection (NautilusView *view, GList *selection)
{
	NautilusListView *list_view;
	GtkTreeSelection *tree_selection;
	GList *node;
	GList *iters, *l;
	NautilusFile *file;
	
	list_view = NAUTILUS_LIST_VIEW (view);
	tree_selection = gtk_tree_view_get_selection (list_view->details->tree_view);

	g_signal_handlers_block_by_func (tree_selection, list_selection_changed_callback, view);

	gtk_tree_selection_unselect_all (tree_selection);
	for (node = selection; node != NULL; node = node->next) {
		file = node->data;
		iters = nautilus_list_model_get_all_iters_for_file (list_view->details->model, file);

		for (l = iters; l != NULL; l = l->next) {
			gtk_tree_selection_select_iter (tree_selection,
							(GtkTreeIter *)l->data);
		}
		g_list_free_full (iters, g_free);
	}

	g_signal_handlers_unblock_by_func (tree_selection, list_selection_changed_callback, view);
	nautilus_view_notify_selection_changed (view);
}

static void
nautilus_list_view_invert_selection (NautilusView *view)
{
	NautilusListView *list_view;
	GtkTreeSelection *tree_selection;
	GList *node;
	GList *iters, *l;
	NautilusFile *file;
	GList *selection = NULL;
	
	list_view = NAUTILUS_LIST_VIEW (view);
	tree_selection = gtk_tree_view_get_selection (list_view->details->tree_view);

	g_signal_handlers_block_by_func (tree_selection, list_selection_changed_callback, view);
	
	gtk_tree_selection_selected_foreach (tree_selection,
					     nautilus_list_view_get_selection_foreach_func, &selection);

	gtk_tree_selection_select_all (tree_selection);
	
	for (node = selection; node != NULL; node = node->next) {
		file = node->data;
		iters = nautilus_list_model_get_all_iters_for_file (list_view->details->model, file);

		for (l = iters; l != NULL; l = l->next) {
			gtk_tree_selection_unselect_iter (tree_selection,
							  (GtkTreeIter *)l->data);
		}
		g_list_free_full (iters, g_free);
	}

	g_list_free (selection);

	g_signal_handlers_unblock_by_func (tree_selection, list_selection_changed_callback, view);
	nautilus_view_notify_selection_changed (view);
}

static void
nautilus_list_view_select_all (NautilusView *view)
{
	gtk_tree_selection_select_all (gtk_tree_view_get_selection (NAUTILUS_LIST_VIEW (view)->details->tree_view));
}

static void
nautilus_list_view_select_first (NautilusView *view)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (NAUTILUS_LIST_VIEW (view)->details->model), &iter)) {
		return;
	}
	selection = gtk_tree_view_get_selection (NAUTILUS_LIST_VIEW (view)->details->tree_view);
	gtk_tree_selection_select_iter (selection, &iter);
}

static void
column_editor_response_callback (GtkWidget *dialog, 
				 int response_id,
				 gpointer user_data)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
column_chooser_changed_callback (NautilusColumnChooser *chooser,
				 NautilusListView *view)
{
	NautilusFile *file;
	char **visible_columns;
	char **column_order;
	GList *list;
	int i;

	file = nautilus_view_get_directory_as_file (NAUTILUS_VIEW (view));

	nautilus_column_chooser_get_settings (chooser,
					      &visible_columns,
					      &column_order);

	list = NULL;
	for (i = 0; visible_columns[i] != NULL; ++i) {
		list = g_list_prepend (list, visible_columns[i]);
	}
	list = g_list_reverse (list);
	nautilus_file_set_metadata_list (file,
					 NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS,
					 list);
	g_list_free (list);

	list = NULL;
	for (i = 0; column_order[i] != NULL; ++i) {
		list = g_list_prepend (list, column_order[i]);
	}
	list = g_list_reverse (list);
	nautilus_file_set_metadata_list (file,
					 NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER,
					 list);
	g_list_free (list);

	apply_columns_settings (view, column_order, visible_columns);

	g_strfreev (visible_columns);
	g_strfreev (column_order);
}

static void
column_chooser_set_from_arrays (NautilusColumnChooser *chooser,
                                NautilusListView *view,
                                char **visible_columns,
                                char **column_order)
{
	g_signal_handlers_block_by_func 
		(chooser, G_CALLBACK (column_chooser_changed_callback), view);

	nautilus_column_chooser_set_settings (chooser,
					      visible_columns, 
					      column_order);

	g_signal_handlers_unblock_by_func 
		(chooser, G_CALLBACK (column_chooser_changed_callback), view);
}

static void
column_chooser_set_from_settings (NautilusColumnChooser *chooser,
				  NautilusListView *view)
{
	char **visible_columns;
	char **column_order;

	visible_columns = get_visible_columns (view);
	column_order = get_column_order (view);

	column_chooser_set_from_arrays (chooser, view,
	                                visible_columns, column_order);

	g_strfreev (visible_columns);
	g_strfreev (column_order);
}

static void
column_chooser_use_default_callback (NautilusColumnChooser *chooser,
				     NautilusListView *view)
{
	NautilusFile *file;
	char **default_columns;
	char **default_order;

	file = nautilus_view_get_directory_as_file 
		(NAUTILUS_VIEW (view));

	nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER, NULL);
	nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS, NULL);

	/* set view values ourselves, as new metadata could not have been
	 * updated yet.
	 */
	default_columns = nautilus_file_is_in_trash (file) ?
		g_strdupv ((gchar **) default_trash_visible_columns) :
		g_settings_get_strv (nautilus_list_view_preferences,
				     NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);

	default_order = nautilus_file_is_in_trash (file) ?
		g_strdupv ((gchar **) default_trash_columns_order) :
		g_settings_get_strv (nautilus_list_view_preferences,
				     NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);

	apply_columns_settings (view, default_order, default_columns);
	column_chooser_set_from_arrays (chooser, view,
	                                default_columns, default_order);

	g_strfreev (default_columns);
	g_strfreev (default_order);
}

static GtkWidget *
create_column_editor (NautilusListView *view)
{
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *box;
	GtkWidget *column_chooser;
	GtkWidget *alignment;
	NautilusFile *file;
	char *str;
	char *name;
	const char *label_text;
	
	file = nautilus_view_get_directory_as_file (NAUTILUS_VIEW (view));
	name = nautilus_file_get_display_name (file);
	str = g_strdup_printf (_("%s Visible Columns"), name);
	g_free (name);

	window = gtk_dialog_new_with_buttons (str,
					      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					      NULL);
	g_free (str);
	g_signal_connect (window, "response", 
			  G_CALLBACK (column_editor_response_callback), NULL);
	
	gtk_window_set_default_size (GTK_WINDOW (window), 300, 400);

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width (GTK_CONTAINER (box), 12);
	gtk_widget_show (box);
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))), box,
			    TRUE, TRUE, 0);

	label_text = _("Choose the order of information to appear in this folder:");
	str = g_strconcat ("<b>", label_text, "</b>", NULL);
	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label), str);
	gtk_label_set_line_wrap (GTK_LABEL (label), FALSE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

	g_free (str);

	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
				   0, 0, 12, 0);
	gtk_widget_show (alignment);
	gtk_box_pack_start (GTK_BOX (box), alignment, TRUE, TRUE, 0);

	column_chooser = nautilus_column_chooser_new (file);
	gtk_widget_show (column_chooser);
	gtk_container_add (GTK_CONTAINER (alignment), column_chooser);

	g_signal_connect (column_chooser, "changed",
			  G_CALLBACK (column_chooser_changed_callback),
			  view);
	g_signal_connect (column_chooser, "use-default",
			  G_CALLBACK (column_chooser_use_default_callback),
			  view);

	column_chooser_set_from_settings 
		(NAUTILUS_COLUMN_CHOOSER (column_chooser), view);

	return window;
}

static void
action_visible_columns_callback (GtkAction *action,
				 gpointer callback_data)
{
	NautilusListView *list_view;
	
	list_view = NAUTILUS_LIST_VIEW (callback_data);

	if (list_view->details->column_editor) {
		gtk_window_present (GTK_WINDOW (list_view->details->column_editor));
	} else {
		list_view->details->column_editor = create_column_editor (list_view);
		g_object_add_weak_pointer (G_OBJECT (list_view->details->column_editor),
					   (gpointer *) &list_view->details->column_editor);
		
		gtk_widget_show (list_view->details->column_editor);
	}
}

static const GtkActionEntry list_view_entries[] = {
	/* name, stock id */     { "Visible Columns", NULL,
				   /* label, accelerator */   N_("Visible _Columns…"), NULL,
				   /* tooltip */              N_("Select the columns visible in this folder"),
				   G_CALLBACK (action_visible_columns_callback) },
};

static void
nautilus_list_view_merge_menus (NautilusView *view)
{
	NautilusListView *list_view;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;

	list_view = NAUTILUS_LIST_VIEW (view);

	NAUTILUS_VIEW_CLASS (nautilus_list_view_parent_class)->merge_menus (view);

	ui_manager = nautilus_view_get_ui_manager (view);

	action_group = gtk_action_group_new ("ListViewActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	list_view->details->list_action_group = action_group;
	gtk_action_group_add_actions (action_group, 
				      list_view_entries, G_N_ELEMENTS (list_view_entries),
				      list_view);

	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui manager */

	list_view->details->list_merge_id =
		gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/gnome/nautilus/nautilus-list-view-ui.xml", NULL);

	list_view->details->menus_ready = TRUE;
}

static void
nautilus_list_view_unmerge_menus (NautilusView *view)
{
	NautilusListView *list_view;
	GtkUIManager *ui_manager;

	list_view = NAUTILUS_LIST_VIEW (view);

	NAUTILUS_VIEW_CLASS (nautilus_list_view_parent_class)->unmerge_menus (view);

	ui_manager = nautilus_view_get_ui_manager (view);
	if (ui_manager != NULL) {
		nautilus_ui_unmerge_ui (ui_manager,
					&list_view->details->list_merge_id,
					&list_view->details->list_action_group);
	}
}

static void
nautilus_list_view_update_menus (NautilusView *view)
{
	NautilusListView *list_view;

        list_view = NAUTILUS_LIST_VIEW (view);

	/* don't update if the menus aren't ready */
	if (!list_view->details->menus_ready) {
		return;
	}

	NAUTILUS_VIEW_CLASS (nautilus_list_view_parent_class)->update_menus (view);
}

/* Reset sort criteria and zoom level to match defaults */
static void
nautilus_list_view_reset_to_defaults (NautilusView *view)
{
	NautilusFile *file;
	const gchar *default_sort_order;
	gboolean default_sort_reversed;

	file = nautilus_view_get_directory_as_file (view);

	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN, NULL, NULL);
	nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED, NULL, NULL);
	nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER, NULL);
	nautilus_file_set_metadata_list (file, NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS, NULL);

	default_sort_order = get_default_sort_order (file, &default_sort_reversed);

	gtk_tree_sortable_set_sort_column_id
		(GTK_TREE_SORTABLE (NAUTILUS_LIST_VIEW (view)->details->model),
		 nautilus_list_model_get_sort_column_id_from_attribute (NAUTILUS_LIST_VIEW (view)->details->model,
								  g_quark_from_string (default_sort_order)),
		 default_sort_reversed ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING);

	nautilus_list_view_set_zoom_level (NAUTILUS_LIST_VIEW (view), get_default_zoom_level (), FALSE);
	set_columns_settings_from_metadata_and_preferences (NAUTILUS_LIST_VIEW (view));
}

static void
nautilus_list_view_set_zoom_level (NautilusListView *view,
				   NautilusZoomLevel new_level,
				   gboolean always_emit)
{
	int column;

	g_return_if_fail (NAUTILUS_IS_LIST_VIEW (view));
	g_return_if_fail (new_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
			  new_level <= NAUTILUS_ZOOM_LEVEL_LARGEST);

	if (view->details->zoom_level == new_level) {
		if (always_emit) {
			g_signal_emit_by_name (NAUTILUS_VIEW(view), "zoom-level-changed");
		}
		return;
	}

	view->details->zoom_level = new_level;
	g_signal_emit_by_name (NAUTILUS_VIEW(view), "zoom-level-changed");

	/* Select correctly scaled icons. */
	column = nautilus_list_model_get_column_id_from_zoom_level (new_level);
	gtk_tree_view_column_set_attributes (view->details->file_name_column,
					     GTK_CELL_RENDERER (view->details->pixbuf_cell),
					     "pixbuf", column,
					     NULL);

	nautilus_view_update_menus (NAUTILUS_VIEW (view));

	set_up_pixbuf_size (view);
}

static void
nautilus_list_view_bump_zoom_level (NautilusView *view, int zoom_increment)
{
	NautilusListView *list_view;
	gint new_level;

	g_return_if_fail (NAUTILUS_IS_LIST_VIEW (view));

	list_view = NAUTILUS_LIST_VIEW (view);
	new_level = list_view->details->zoom_level + zoom_increment;

	if (new_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
	    new_level <= NAUTILUS_ZOOM_LEVEL_LARGEST) {
		nautilus_list_view_set_zoom_level (list_view, new_level, FALSE);
	}
}

static NautilusZoomLevel
nautilus_list_view_get_zoom_level (NautilusView *view)
{
	NautilusListView *list_view;

	g_return_val_if_fail (NAUTILUS_IS_LIST_VIEW (view), NAUTILUS_ZOOM_LEVEL_STANDARD);

	list_view = NAUTILUS_LIST_VIEW (view);

	return list_view->details->zoom_level;
}

static void
nautilus_list_view_zoom_to_level (NautilusView *view,
				  NautilusZoomLevel zoom_level)
{
	NautilusListView *list_view;

	g_return_if_fail (NAUTILUS_IS_LIST_VIEW (view));

	list_view = NAUTILUS_LIST_VIEW (view);

	nautilus_list_view_set_zoom_level (list_view, zoom_level, FALSE);
}

static void
nautilus_list_view_restore_default_zoom_level (NautilusView *view)
{
	NautilusListView *list_view;

	g_return_if_fail (NAUTILUS_IS_LIST_VIEW (view));

	list_view = NAUTILUS_LIST_VIEW (view);

	nautilus_list_view_set_zoom_level (list_view, get_default_zoom_level (), FALSE);
}

static gboolean 
nautilus_list_view_can_zoom_in (NautilusView *view) 
{
	g_return_val_if_fail (NAUTILUS_IS_LIST_VIEW (view), FALSE);

	return NAUTILUS_LIST_VIEW (view)->details->zoom_level	< NAUTILUS_ZOOM_LEVEL_LARGEST;
}

static gboolean 
nautilus_list_view_can_zoom_out (NautilusView *view) 
{
	g_return_val_if_fail (NAUTILUS_IS_LIST_VIEW (view), FALSE);

	return NAUTILUS_LIST_VIEW (view)->details->zoom_level > NAUTILUS_ZOOM_LEVEL_SMALLEST;
}

static void
nautilus_list_view_start_renaming_file (NautilusView *view,
					NautilusFile *file,
					gboolean select_all)
{
	NautilusListView *list_view;
	GtkTreeIter iter;
	GtkTreePath *path;
	
	list_view = NAUTILUS_LIST_VIEW (view);
	
	/* Select all if we are in renaming mode already */
	if (list_view->details->file_name_column && list_view->details->editable_widget) {
		gtk_editable_select_region (GTK_EDITABLE (list_view->details->editable_widget),
					    0,
					    -1);
		return;
	}

	if (!nautilus_list_model_get_first_iter_for_file (list_view->details->model, file, &iter)) {
		return;
	}

	/* call parent class to make sure the right icon is selected */
	NAUTILUS_VIEW_CLASS (nautilus_list_view_parent_class)->start_renaming_file (view, file, select_all);

	/* Freeze updates to the view to prevent losing rename focus when the tree view updates */
	nautilus_view_freeze_updates (NAUTILUS_VIEW (view));

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_view->details->model), &iter);

	/* Make filename-cells editable. */
	g_object_set (G_OBJECT (list_view->details->file_name_cell),
		      "editable", TRUE,
		      NULL);

	gtk_tree_view_scroll_to_cell (list_view->details->tree_view,
				      NULL,
				      list_view->details->file_name_column,
				      TRUE, 0.0, 0.0);
	gtk_tree_view_set_cursor_on_cell (list_view->details->tree_view,
					  path,
					  list_view->details->file_name_column,
					  GTK_CELL_RENDERER (list_view->details->file_name_cell),
					  TRUE);

	/* set cursor also triggers editing-started, where we save the editable widget */
	if (list_view->details->editable_widget != NULL) {
		int start_offset = 0;
		int end_offset = -1;

		if (!select_all) {
			eel_filename_get_rename_region (list_view->details->original_name,
							&start_offset, &end_offset);
		}

		gtk_editable_select_region (GTK_EDITABLE (list_view->details->editable_widget),
					    start_offset, end_offset);
	}

	gtk_tree_path_free (path);
}

static void
nautilus_list_view_click_policy_changed (NautilusView *directory_view)
{
	GdkWindow *win;
	GdkDisplay *display;
	NautilusListView *view;
	GtkTreeIter iter;
	GtkTreeView *tree;

	view = NAUTILUS_LIST_VIEW (directory_view);

	/* ensure that we unset the hand cursor and refresh underlined rows */
	if (get_click_policy () == NAUTILUS_CLICK_POLICY_DOUBLE) {
		if (view->details->hover_path != NULL) {
			if (gtk_tree_model_get_iter (GTK_TREE_MODEL (view->details->model),
						     &iter, view->details->hover_path)) {
				gtk_tree_model_row_changed (GTK_TREE_MODEL (view->details->model),
							    view->details->hover_path, &iter);
			}

			gtk_tree_path_free (view->details->hover_path);
			view->details->hover_path = NULL;
		}

		tree = view->details->tree_view;
		if (gtk_widget_get_realized (GTK_WIDGET (tree))) {
			win = gtk_widget_get_window (GTK_WIDGET (tree));
			gdk_window_set_cursor (win, NULL);
			
			display = gtk_widget_get_display (GTK_WIDGET (view));
			if (display != NULL) {
				gdk_display_flush (display);
			}
		}

		g_clear_object (&hand_cursor);
	} else if (get_click_policy () == NAUTILUS_CLICK_POLICY_SINGLE) {
		if (hand_cursor == NULL) {
			hand_cursor = gdk_cursor_new(GDK_HAND2);
		}
	}
}

static void
default_sort_order_changed_callback (gpointer callback_data)
{
	NautilusListView *list_view;

	list_view = NAUTILUS_LIST_VIEW (callback_data);

	set_sort_order_from_metadata_and_preferences (list_view);
}

static void
default_visible_columns_changed_callback (gpointer callback_data)
{
	NautilusListView *list_view;
	
	list_view = NAUTILUS_LIST_VIEW (callback_data);

	set_columns_settings_from_metadata_and_preferences (list_view);	
}

static void
default_column_order_changed_callback (gpointer callback_data)
{
	NautilusListView *list_view;
	
	list_view = NAUTILUS_LIST_VIEW (callback_data);

	set_columns_settings_from_metadata_and_preferences (list_view);
}

static void
nautilus_list_view_sort_directories_first_changed (NautilusView *view)
{
	NautilusListView *list_view;

	list_view = NAUTILUS_LIST_VIEW (view);

	nautilus_list_model_set_should_sort_directories_first (list_view->details->model,
							 nautilus_view_should_sort_directories_first (view));
}

static int
nautilus_list_view_compare_files (NautilusView *view, NautilusFile *file1, NautilusFile *file2)
{
	NautilusListView *list_view;

	list_view = NAUTILUS_LIST_VIEW (view);
	return nautilus_list_model_compare_func (list_view->details->model, file1, file2);
}

static gboolean
nautilus_list_view_using_manual_layout (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_LIST_VIEW (view), FALSE);

	return FALSE;
}

static void
nautilus_list_view_dispose (GObject *object)
{
	NautilusListView *list_view;

	list_view = NAUTILUS_LIST_VIEW (object);

	if (list_view->details->model) {
		stop_cell_editing (list_view);
		g_object_unref (list_view->details->model);
		list_view->details->model = NULL;
	}

	if (list_view->details->drag_dest) {
		g_object_unref (list_view->details->drag_dest);
		list_view->details->drag_dest = NULL;
	}

	if (list_view->details->renaming_file_activate_timeout != 0) {
		g_source_remove (list_view->details->renaming_file_activate_timeout);
		list_view->details->renaming_file_activate_timeout = 0;
	}

	if (list_view->details->clipboard_handler_id != 0) {
		g_signal_handler_disconnect (nautilus_clipboard_monitor_get (),
		                             list_view->details->clipboard_handler_id);
		list_view->details->clipboard_handler_id = 0;
	}

	G_OBJECT_CLASS (nautilus_list_view_parent_class)->dispose (object);
}

static void
nautilus_list_view_finalize (GObject *object)
{
	NautilusListView *list_view;

	list_view = NAUTILUS_LIST_VIEW (object);

	g_free (list_view->details->original_name);
	list_view->details->original_name = NULL;
	
	if (list_view->details->double_click_path[0]) {
		gtk_tree_path_free (list_view->details->double_click_path[0]);
	}	
	if (list_view->details->double_click_path[1]) {
		gtk_tree_path_free (list_view->details->double_click_path[1]);
	}
	if (list_view->details->new_selection_path) {
		gtk_tree_path_free (list_view->details->new_selection_path);
	}
	
	g_list_free (list_view->details->cells);
	g_hash_table_destroy (list_view->details->columns);

	if (list_view->details->hover_path != NULL) {
		gtk_tree_path_free (list_view->details->hover_path);
	}

	if (list_view->details->column_editor != NULL) {
		gtk_widget_destroy (list_view->details->column_editor);
	}

	g_free (list_view->details);

	g_signal_handlers_disconnect_by_func (nautilus_preferences,
					      default_sort_order_changed_callback,
					      list_view);
	g_signal_handlers_disconnect_by_func (nautilus_list_view_preferences,
					      default_visible_columns_changed_callback,
					      list_view);
	g_signal_handlers_disconnect_by_func (nautilus_list_view_preferences,
					      default_column_order_changed_callback,
					      list_view);

	G_OBJECT_CLASS (nautilus_list_view_parent_class)->finalize (object);
}

static char *
nautilus_list_view_get_first_visible_file (NautilusView *view)
{
	NautilusFile *file;
	GtkTreePath *path;
	GtkTreeIter iter;
	NautilusListView *list_view;

	list_view = NAUTILUS_LIST_VIEW (view);

	if (gtk_tree_view_get_path_at_pos (list_view->details->tree_view,
					   0, 0,
					   &path, NULL, NULL, NULL)) {
		gtk_tree_model_get_iter (GTK_TREE_MODEL (list_view->details->model),
					 &iter, path);

		gtk_tree_path_free (path);
	
		gtk_tree_model_get (GTK_TREE_MODEL (list_view->details->model),
				    &iter,
				    NAUTILUS_LIST_MODEL_FILE_COLUMN, &file,
				    -1);
		if (file) {
			char *uri;
			
			uri = nautilus_file_get_uri (file);
			
			nautilus_file_unref (file);
			
			return uri;
		}
	}
	
	return NULL;
}

static void
nautilus_list_view_scroll_to_file (NautilusListView *view,
				   NautilusFile *file)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	
	if (!nautilus_list_model_get_first_iter_for_file (view->details->model, file, &iter)) {
		return;
	}
		
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (view->details->model), &iter);

	gtk_tree_view_scroll_to_cell (view->details->tree_view,
				      path, NULL,
				      TRUE, 0.0, 0.0);
	
	gtk_tree_path_free (path);
}

static void
list_view_scroll_to_file (NautilusView *view,
			  const char *uri)
{
	NautilusFile *file;

	if (uri != NULL) {
		/* Only if existing, since we don't want to add the file to
		   the directory if it has been removed since then */
		file = nautilus_file_get_existing_by_uri (uri);
		if (file != NULL) {
			nautilus_list_view_scroll_to_file (NAUTILUS_LIST_VIEW (view), file);
			nautilus_file_unref (file);
		}
	}
}

static void
list_view_notify_clipboard_info (NautilusClipboardMonitor *monitor,
                                 NautilusClipboardInfo *info,
                                 NautilusListView *view)
{
	/* this could be called as a result of _end_loading() being
	 * called after _dispose(), where the model is cleared.
	 */
	if (view->details->model == NULL) {
		return;
	}

	if (info != NULL && info->cut) {
		nautilus_list_model_set_highlight_for_files (view->details->model, info->files);
	} else {
		nautilus_list_model_set_highlight_for_files (view->details->model, NULL);
	}
}

static void
nautilus_list_view_end_loading (NautilusView *view,
				gboolean all_files_seen)
{
	NautilusClipboardMonitor *monitor;
	NautilusClipboardInfo *info;

	monitor = nautilus_clipboard_monitor_get ();
	info = nautilus_clipboard_monitor_get_clipboard_info (monitor);

	list_view_notify_clipboard_info (monitor, info, NAUTILUS_LIST_VIEW (view));
}

static const char *
nautilus_list_view_get_id (NautilusView *view)
{
	return NAUTILUS_LIST_VIEW_ID;
}

static void
nautilus_list_view_class_init (NautilusListViewClass *class)
{
	NautilusViewClass *nautilus_view_class;

	nautilus_view_class = NAUTILUS_VIEW_CLASS (class);

	G_OBJECT_CLASS (class)->dispose = nautilus_list_view_dispose;
	G_OBJECT_CLASS (class)->finalize = nautilus_list_view_finalize;

	nautilus_view_class->add_file = nautilus_list_view_add_file;
	nautilus_view_class->begin_loading = nautilus_list_view_begin_loading;
	nautilus_view_class->end_loading = nautilus_list_view_end_loading;
	nautilus_view_class->bump_zoom_level = nautilus_list_view_bump_zoom_level;
	nautilus_view_class->can_zoom_in = nautilus_list_view_can_zoom_in;
	nautilus_view_class->can_zoom_out = nautilus_list_view_can_zoom_out;
        nautilus_view_class->click_policy_changed = nautilus_list_view_click_policy_changed;
	nautilus_view_class->clear = nautilus_list_view_clear;
	nautilus_view_class->file_changed = nautilus_list_view_file_changed;
	nautilus_view_class->get_backing_uri = nautilus_list_view_get_backing_uri;
	nautilus_view_class->get_selection = nautilus_list_view_get_selection;
	nautilus_view_class->get_selection_for_file_transfer = nautilus_list_view_get_selection_for_file_transfer;
	nautilus_view_class->is_empty = nautilus_list_view_is_empty;
	nautilus_view_class->remove_file = nautilus_list_view_remove_file;
	nautilus_view_class->merge_menus = nautilus_list_view_merge_menus;
	nautilus_view_class->unmerge_menus = nautilus_list_view_unmerge_menus;
	nautilus_view_class->update_menus = nautilus_list_view_update_menus;
	nautilus_view_class->reset_to_defaults = nautilus_list_view_reset_to_defaults;
	nautilus_view_class->restore_default_zoom_level = nautilus_list_view_restore_default_zoom_level;
	nautilus_view_class->reveal_selection = nautilus_list_view_reveal_selection;
	nautilus_view_class->select_all = nautilus_list_view_select_all;
	nautilus_view_class->select_first = nautilus_list_view_select_first;
	nautilus_view_class->set_selection = nautilus_list_view_set_selection;
	nautilus_view_class->invert_selection = nautilus_list_view_invert_selection;
	nautilus_view_class->compare_files = nautilus_list_view_compare_files;
	nautilus_view_class->sort_directories_first_changed = nautilus_list_view_sort_directories_first_changed;
	nautilus_view_class->start_renaming_file = nautilus_list_view_start_renaming_file;
	nautilus_view_class->get_zoom_level = nautilus_list_view_get_zoom_level;
	nautilus_view_class->zoom_to_level = nautilus_list_view_zoom_to_level;
	nautilus_view_class->end_file_changes = nautilus_list_view_end_file_changes;
	nautilus_view_class->using_manual_layout = nautilus_list_view_using_manual_layout;
	nautilus_view_class->get_view_id = nautilus_list_view_get_id;
	nautilus_view_class->get_first_visible_file = nautilus_list_view_get_first_visible_file;
	nautilus_view_class->scroll_to_file = list_view_scroll_to_file;
}

static void
nautilus_list_view_init (NautilusListView *list_view)
{
	list_view->details = g_new0 (NautilusListViewDetails, 1);

	/* ensure that the zoom level is always set before settings up the tree view columns */
	list_view->details->zoom_level = get_default_zoom_level ();

	create_and_set_up_tree_view (list_view);

	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_DEFAULT_SORT_ORDER,
				  G_CALLBACK (default_sort_order_changed_callback),
				  list_view);
	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER,
				  G_CALLBACK (default_sort_order_changed_callback),
				  list_view);
	g_signal_connect_swapped (nautilus_list_view_preferences,
				  "changed::" NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS,
				  G_CALLBACK (default_visible_columns_changed_callback),
				  list_view);
	g_signal_connect_swapped (nautilus_list_view_preferences,
				  "changed::" NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER,
				  G_CALLBACK (default_column_order_changed_callback),
				  list_view);

	nautilus_list_view_click_policy_changed (NAUTILUS_VIEW (list_view));

	nautilus_list_view_sort_directories_first_changed (NAUTILUS_VIEW (list_view));
	nautilus_list_view_set_zoom_level (list_view, get_default_zoom_level (), TRUE);

	list_view->details->hover_path = NULL;
	list_view->details->clipboard_handler_id =
		g_signal_connect (nautilus_clipboard_monitor_get (),
		                  "clipboard-info",
		                  G_CALLBACK (list_view_notify_clipboard_info), list_view);
}

static NautilusView *
nautilus_list_view_create (NautilusWindowSlot *slot)
{
	NautilusListView *view;

	view = g_object_new (NAUTILUS_TYPE_LIST_VIEW,
			     "window-slot", slot,
			     NULL);
	return NAUTILUS_VIEW (view);
}

static gboolean
nautilus_list_view_supports_uri (const char *uri,
				 GFileType file_type,
				 const char *mime_type)
{
	if (file_type == G_FILE_TYPE_DIRECTORY) {
		return TRUE;
	}
	if (strcmp (mime_type, NAUTILUS_SAVED_SEARCH_MIMETYPE) == 0){
		return TRUE;
	}
	if (g_str_has_prefix (uri, "trash:")) {
		return TRUE;
	}
	if (g_str_has_prefix (uri, EEL_SEARCH_URI)) {
		return TRUE;
	}

	return FALSE;
}

static NautilusViewInfo nautilus_list_view = {
	NAUTILUS_LIST_VIEW_ID,
	/* translators: this is used in the view selection dropdown
	 * of navigation windows and in the preferences dialog */
	N_("List View"),
	/* translators: this is used in the view menu */
	N_("_List"),
	N_("The list view encountered an error."),
	N_("The list view encountered an error while starting up."),
	N_("Display this location with the list view."),
	nautilus_list_view_create,
	nautilus_list_view_supports_uri
};

void
nautilus_list_view_register (void)
{
	nautilus_list_view.view_combo_label = _(nautilus_list_view.view_combo_label);
	nautilus_list_view.view_menu_label_with_mnemonic = _(nautilus_list_view.view_menu_label_with_mnemonic);
	nautilus_list_view.error_label = _(nautilus_list_view.error_label);
	nautilus_list_view.startup_error_label = _(nautilus_list_view.startup_error_label);
	nautilus_list_view.display_location_label = _(nautilus_list_view.display_location_label);

	nautilus_view_factory_register (&nautilus_list_view);
}

GtkTreeView*
nautilus_list_view_get_tree_view (NautilusListView *list_view)
{
	return list_view->details->tree_view;
}

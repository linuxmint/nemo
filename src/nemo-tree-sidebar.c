/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000, 2001 Eazel, Inc
 * Copyright (C) 2002 Anders Carlsson
 * Copyright (C) 2002 Darin Adler
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
 * Authors: 
 *       Maciej Stachowiak <mjs@eazel.com>
 *       Anders Carlsson <andersca@gnu.org>
 *       Darin Adler <darin@bentspoon.com>
 */

/* fm-tree-view.c - tree sidebar panel
 */

#include <config.h>

#include "nemo-tree-sidebar.h"

#include "nemo-tree-sidebar-model.h"
#include "nemo-properties-window.h"
#include "nemo-window-slot.h"

#include <libnemo-private/nemo-clipboard.h>
#include <libnemo-private/nemo-clipboard-monitor.h>
#include <libnemo-private/nemo-desktop-icon-file.h>
#include <libnemo-private/nemo-file-attributes.h>
#include <libnemo-private/nemo-file-operations.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-icon-names.h>
#include <libnemo-private/nemo-program-choosing.h>
#include <libnemo-private/nemo-tree-view-drag-dest.h>
#include <libnemo-private/nemo-module.h>
#include <libnemo-private/nemo-action-manager.h>
#include <libnemo-private/nemo-action.h>

#include <string.h>
#include <eel/eel-gtk-extensions.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#define DEBUG_FLAG NEMO_DEBUG_LIST_VIEW
#include <libnemo-private/nemo-debug.h>

typedef struct {
        GObject parent;
} FMTreeViewProvider;

typedef struct {
        GObjectClass parent;
} FMTreeViewProviderClass;


struct FMTreeViewDetails {
	NemoWindow *window;
	GtkTreeView *tree_widget;
	GtkTreeModelSort *sort_model;
	FMTreeModel *child_model;

	GVolumeMonitor *volume_monitor;

	NemoFile *activation_file;
	NemoWindowOpenFlags activation_flags;

	NemoTreeViewDragDest *drag_dest;

	char *selection_location;
	gboolean selecting;

	guint show_selection_idle_id;
	gulong clipboard_handler_id;

	GtkWidget *popup;
	GtkWidget *popup_open;
	GtkWidget *popup_open_in_new_window;
	GtkWidget *popup_open_in_new_tab;
	GtkWidget *popup_create_folder;
	GtkWidget *popup_cut;
	GtkWidget *popup_copy;
	GtkWidget *popup_paste;
	GtkWidget *popup_rename;
	GtkWidget *popup_trash;
	GtkWidget *popup_delete;
	GtkWidget *popup_properties;
	GtkWidget *popup_unmount_separator;
	GtkWidget *popup_unmount;
	GtkWidget *popup_eject;
    GtkWidget *popup_action_separator;
	NemoFile *popup_file;
	guint popup_file_idle_handler;
	
	guint selection_changed_timer;

    NemoActionManager *action_manager;
    guint action_manager_changed_id;
    GList *action_items;
    guint hidden_files_changed_id;
};

typedef struct {
	GList *uris;
	FMTreeView *view;
} PrependURIParameters;

typedef struct {
    NemoAction *action;
    FMTreeView *view;
    GtkWidget *item;
} ActionPayload;

static GdkAtom copied_files_atom;

static void  fm_tree_view_activate_file     (FMTreeView *view, 
			    		     NemoFile *file,
					     NemoWindowOpenFlags flags);

static void create_popup_menu (FMTreeView *view);

static void add_action_popup_items (FMTreeView *view);

G_DEFINE_TYPE (FMTreeView, fm_tree_view, GTK_TYPE_SCROLLED_WINDOW)
#define parent_class fm_tree_view_parent_class

static void
notify_clipboard_info (NemoClipboardMonitor *monitor,
                       NemoClipboardInfo *info,
                       FMTreeView *view)
{
	if (info != NULL && info->cut) {
		fm_tree_model_set_highlight_for_files (view->details->child_model, info->files);
	} else {
		fm_tree_model_set_highlight_for_files (view->details->child_model, NULL);
	}
}


static gboolean
show_iter_for_file (FMTreeView *view, NemoFile *file, GtkTreeIter *iter)
{
	GtkTreeModel *model;
	NemoFile *parent_file;
	GtkTreeIter parent_iter;
	GtkTreePath *path, *sort_path;
	GtkTreeIter cur_iter;

	if (view->details->child_model == NULL) {
		return FALSE;
	}
	model = GTK_TREE_MODEL (view->details->child_model);

	/* check if file is visible in the same root as the currently selected folder is */
	gtk_tree_view_get_cursor (view->details->tree_widget, &path, NULL);
	if (path != NULL) {
		if (gtk_tree_model_get_iter (model, &cur_iter, path) &&
		    fm_tree_model_file_get_iter (view->details->child_model, iter,
						 file, &cur_iter)) {
			gtk_tree_path_free (path);
			return TRUE;
		}
		gtk_tree_path_free (path);
	}
	/* check if file is visible at all */
	if (fm_tree_model_file_get_iter (view->details->child_model,
					       iter, file, NULL)) {
		return TRUE;
	}

	parent_file = nemo_file_get_parent (file);

	if (parent_file == NULL) {
		return FALSE;
	}
	if (!show_iter_for_file (view, parent_file, &parent_iter)) {
		nemo_file_unref (parent_file);
		return FALSE;
	}
	nemo_file_unref (parent_file);

	if (parent_iter.user_data == NULL || parent_iter.stamp == 0) {
		return FALSE;
	}
	path = gtk_tree_model_get_path (model, &parent_iter);
	sort_path = gtk_tree_model_sort_convert_child_path_to_path
		(view->details->sort_model, path);
	gtk_tree_path_free (path);
	gtk_tree_view_expand_row (view->details->tree_widget, sort_path, FALSE);
	gtk_tree_path_free (sort_path);

	return FALSE;
}

static void
refresh_highlight (FMTreeView *view)
{
	NemoClipboardMonitor *monitor;
	NemoClipboardInfo *info;

	monitor = nemo_clipboard_monitor_get ();
	info = nemo_clipboard_monitor_get_clipboard_info (monitor);

	notify_clipboard_info (monitor, info, view);
}

static gboolean
show_selection_idle_callback (gpointer callback_data)
{
	FMTreeView *view;
	NemoFile *file, *old_file;
	GtkTreeIter iter;
	GtkTreePath *path, *sort_path;

	view = FM_TREE_VIEW (callback_data);

	view->details->show_selection_idle_id = 0;

	file = nemo_file_get_by_uri (view->details->selection_location);
	if (file == NULL) {
		return FALSE;
	}

	if (!nemo_file_is_directory (file)) {
		old_file = file;
		file = nemo_file_get_parent (file);
		nemo_file_unref (old_file);
		if (file == NULL) {
			return FALSE;
		}
	}
	
	view->details->selecting = TRUE;
	if (!show_iter_for_file (view, file, &iter)) {
		nemo_file_unref (file);
		return FALSE;
	}
	view->details->selecting = FALSE;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (view->details->child_model), &iter);
	sort_path = gtk_tree_model_sort_convert_child_path_to_path
		(view->details->sort_model, path);
	gtk_tree_path_free (path);
	gtk_tree_view_set_cursor (view->details->tree_widget, sort_path, NULL, FALSE);
	if (gtk_widget_get_realized (GTK_WIDGET (view->details->tree_widget))) {
		gtk_tree_view_scroll_to_cell (view->details->tree_widget, sort_path, NULL, FALSE, 0, 0);
	}
	gtk_tree_path_free (sort_path);

	nemo_file_unref (file);
	refresh_highlight (view);	

	return FALSE;
}

static void
schedule_show_selection (FMTreeView *view)
{
	if (view->details->show_selection_idle_id == 0) {
		view->details->show_selection_idle_id = g_idle_add (show_selection_idle_callback, view);
	}
}

static void
schedule_select_and_show_location (FMTreeView *view, char *location)
{
	if (view->details->selection_location != NULL) {
		g_free (view->details->selection_location);
	}
	view->details->selection_location = g_strdup (location);
	schedule_show_selection (view);
}

static void
row_loaded_callback (GtkTreeModel     *tree_model,
		     GtkTreeIter      *iter,
		     FMTreeView *view)
{
	NemoFile *file, *tmp_file, *selection_file;

	if (view->details->selection_location == NULL
	    || !view->details->selecting
	    || iter->user_data == NULL || iter->stamp == 0) {
		return;
	}

	file = fm_tree_model_iter_get_file (view->details->child_model, iter);
	if (file == NULL) {
		return;
	}
	if (!nemo_file_is_directory (file)) {
		nemo_file_unref(file);
		return;
	}

	/* if iter is ancestor of wanted selection_location then update selection */
	selection_file = nemo_file_get_by_uri (view->details->selection_location);
	while (selection_file != NULL) {
		if (file == selection_file) {
			nemo_file_unref (file);
			nemo_file_unref (selection_file);

			schedule_show_selection (view);
			return;
		}
		tmp_file = nemo_file_get_parent (selection_file);
		nemo_file_unref (selection_file);
		selection_file = tmp_file;
	}
	nemo_file_unref (file);
}

static NemoFile *
sort_model_iter_to_file (FMTreeView *view, GtkTreeIter *iter)
{
	GtkTreeIter child_iter;

	gtk_tree_model_sort_convert_iter_to_child_iter (view->details->sort_model, &child_iter, iter);
	return fm_tree_model_iter_get_file (view->details->child_model, &child_iter);
}

static NemoFile *
sort_model_path_to_file (FMTreeView *view, GtkTreePath *path)
{
	GtkTreeIter iter;

	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (view->details->sort_model), &iter, path)) {
		return NULL;
	}
	return sort_model_iter_to_file (view, &iter);
}

static void
got_activation_uri_callback (NemoFile *file, gpointer callback_data)
{
        char *uri, *file_uri;
        FMTreeView *view;
	GdkScreen *screen;
	GFile *location;
	NemoWindowSlot *slot;
	gboolean open_in_same_slot;
	
        view = FM_TREE_VIEW (callback_data);

	screen = gtk_widget_get_screen (GTK_WIDGET (view->details->tree_widget));

        g_assert (file == view->details->activation_file);

	open_in_same_slot =
		(view->details->activation_flags &
		 (NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW |
		  NEMO_WINDOW_OPEN_FLAG_NEW_TAB)) == 0;

	slot = nemo_window_get_active_slot (view->details->window);

	uri = nemo_file_get_activation_uri (file);
	if (nemo_file_is_launcher (file)) {
		file_uri = nemo_file_get_uri (file);
		DEBUG ("Tree sidebar, launching %s", file_uri);
		nemo_launch_desktop_file (screen, file_uri, NULL, NULL);
		g_free (file_uri);
	} else if (uri != NULL
		   && nemo_file_is_executable (file)
		   && nemo_file_can_execute (file)
		   && !nemo_file_is_directory (file)) {	
		   
		file_uri = g_filename_from_uri (uri, NULL, NULL);

		/* Non-local executables don't get launched. They act like non-executables. */
		if (file_uri == NULL) {
			DEBUG ("Tree sidebar, opening location %s", uri);

			location = g_file_new_for_uri (uri);
			nemo_window_slot_open_location
				(slot,
				 location, 
				 view->details->activation_flags,
				 NULL);
			g_object_unref (location);
		} else {
			DEBUG ("Tree sidebar, launching application for %s", file_uri);
			nemo_launch_application_from_command (screen, file_uri, FALSE, NULL);
			g_free (file_uri);
		}
		   
	} else if (uri != NULL) {
		if (!open_in_same_slot ||
		    view->details->selection_location == NULL ||
		    strcmp (uri, view->details->selection_location) != 0) {
			if (open_in_same_slot) {
				if (view->details->selection_location != NULL) {
					g_free (view->details->selection_location);
				}
				view->details->selection_location = g_strdup (uri);
			}

			DEBUG ("Tree sidebar, opening location %s", uri);

			location = g_file_new_for_uri (uri);
			nemo_window_slot_open_location
				(slot,
				 location,
				 view->details->activation_flags,
				 NULL);
			g_object_unref (location);
		}
	}

	g_free (uri);
	nemo_file_unref (view->details->activation_file);
	view->details->activation_file = NULL;
}

static void
cancel_activation (FMTreeView *view)
{
        if (view->details->activation_file == NULL) {
		return;
	}
	
	nemo_file_cancel_call_when_ready
		(view->details->activation_file,
		 got_activation_uri_callback, view);
	nemo_file_unref (view->details->activation_file);
        view->details->activation_file = NULL;
}

static void
row_activated_callback (GtkTreeView *treeview, GtkTreePath *path, 
			GtkTreeViewColumn *column, FMTreeView *view)
{
	if (gtk_tree_view_row_expanded (view->details->tree_widget, path)) {
		gtk_tree_view_collapse_row (view->details->tree_widget, path);
	} else {
		gtk_tree_view_expand_row (view->details->tree_widget, 
					  path, FALSE);
	}
}

static gboolean 
selection_changed_timer_callback(FMTreeView *view)
{
	NemoFileAttributes attributes;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->details->tree_widget));

	/* no activation if popup menu is open */
	if (view->details->popup_file != NULL) {
		return FALSE;
	}

	cancel_activation (view);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		return FALSE;
	}

	view->details->activation_file = sort_model_iter_to_file (view, &iter);
	if (view->details->activation_file == NULL) {
		return FALSE;
	}
	view->details->activation_flags = 0;
		
	attributes = NEMO_FILE_ATTRIBUTE_INFO | NEMO_FILE_ATTRIBUTE_LINK_INFO;
	nemo_file_call_when_ready (view->details->activation_file, attributes,
				       got_activation_uri_callback, view);
	return FALSE; /* remove timeout */
}

static void
selection_changed_callback (GtkTreeSelection *selection,
			    FMTreeView *view)
{
	GdkEvent *event;
	gboolean is_keyboard;

	if (view->details->selection_changed_timer) {
		g_source_remove (view->details->selection_changed_timer);
		view->details->selection_changed_timer = 0;
	}
	
	event = gtk_get_current_event ();
	if (event) {
		is_keyboard = (event->type == GDK_KEY_PRESS || event->type == GDK_KEY_RELEASE);
		gdk_event_free (event);

		if (is_keyboard) {
			/* on keyboard event: delay the change */
			/* TODO: make dependent on keyboard repeat rate as per Markus Bertheau ? */
			view->details->selection_changed_timer = g_timeout_add (300, (GSourceFunc) selection_changed_timer_callback, view);
		} else {
			/* on mouse event: show the change immediately */
			selection_changed_timer_callback (view);
		}
	}
}

static int
compare_rows (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer callback_data)
{
	NemoFile *file_a, *file_b;
	int result;

	/* Dummy rows are always first */
	if (a->user_data == NULL) {
		return -1;
	}
	else if (b->user_data == NULL) {
		return 1;
	}

	/* don't sort root nodes */
	if (fm_tree_model_iter_is_root (FM_TREE_MODEL (model), a) &&
	    fm_tree_model_iter_is_root (FM_TREE_MODEL (model), b)) {
		return fm_tree_model_iter_compare_roots (FM_TREE_MODEL (model), a, b);
	}

	file_a = fm_tree_model_iter_get_file (FM_TREE_MODEL (model), a);
	file_b = fm_tree_model_iter_get_file (FM_TREE_MODEL (model), b);

	if (file_a == file_b) {
		result = 0;
	} else if (file_a == NULL) {
		result = -1;
	} else if (file_b == NULL) {
		result = +1;
	} else {
		result = nemo_file_compare_for_sort (file_a, file_b,
							 NEMO_FILE_SORT_BY_DISPLAY_NAME,
							 FALSE, FALSE);
	}

	nemo_file_unref (file_a);
	nemo_file_unref (file_b);

	return result;
}


static char *
get_root_uri_callback (NemoTreeViewDragDest *dest,
		       gpointer user_data)
{
	/* Don't allow drops on background */
	return NULL;
}

static NemoFile *
get_file_for_path_callback (NemoTreeViewDragDest *dest,
			    GtkTreePath *path,
			    gpointer user_data)
{
	FMTreeView *view;
	
	view = FM_TREE_VIEW (user_data);

	return sort_model_path_to_file (view, path);
}

static void
move_copy_items_callback (NemoTreeViewDragDest *dest,
			  const GList *item_uris,
			  const char *target_uri,
			  GdkDragAction action,
			  int x,
			  int y,
			  gpointer user_data)
{
	FMTreeView *view;

	view = FM_TREE_VIEW (user_data);
	
	nemo_clipboard_clear_if_colliding_uris (GTK_WIDGET (view),
						    item_uris,
						    copied_files_atom);
	nemo_file_operations_copy_move
		(item_uris,
		 NULL,
		 target_uri,
		 action,
		 GTK_WIDGET (view->details->tree_widget),
		 NULL, NULL);
}

static void
add_root_for_mount (FMTreeView *view,
		     GMount *mount)
{
	char *mount_uri, *name;
	GFile *root;
	GIcon *icon;

	if (g_mount_is_shadowed (mount))
		return;

	icon = g_mount_get_icon (mount);
	root = g_mount_get_root (mount);
	mount_uri = g_file_get_uri (root);
	g_object_unref (root);
	name = g_mount_get_name (mount);
	
	fm_tree_model_add_root_uri(view->details->child_model,
				   mount_uri, name, icon, mount);

	g_object_unref (icon);
	g_free (name);
	g_free (mount_uri);
	
}

static void
mount_added_callback (GVolumeMonitor *volume_monitor,
		      GMount *mount,
		      FMTreeView *view)
{
	add_root_for_mount (view, mount);
}

static void
mount_removed_callback (GVolumeMonitor *volume_monitor,
			GMount *mount,
			FMTreeView *view)
{
	GFile *root;
	char *mount_uri;

	root = g_mount_get_root (mount);
	mount_uri = g_file_get_uri (root);
	g_object_unref (root);
	fm_tree_model_remove_root_uri (view->details->child_model,
				       mount_uri);
	g_free (mount_uri);
}

static void
clipboard_contents_received_callback (GtkClipboard     *clipboard,
				      GtkSelectionData *selection_data,
				      gpointer          data)
{
	FMTreeView *view;

	view = FM_TREE_VIEW (data);

	if (gtk_selection_data_get_data_type (selection_data) == copied_files_atom
	    && gtk_selection_data_get_length (selection_data) > 0 &&
	    view->details->popup != NULL) {
		gtk_widget_set_sensitive (view->details->popup_paste, TRUE);
	}

	g_object_unref (view);
}

static gboolean
is_parent_writable (NemoFile *file)
{
	NemoFile *parent;
	gboolean result;
	
	parent = nemo_file_get_parent (file);
	
	/* No parent directory, return FALSE */
	if (parent == NULL) {
		return FALSE;
	}
	
	result = nemo_file_can_write (parent);
	nemo_file_unref (parent);
	
	return result;	
}

static gboolean
button_pressed_callback (GtkTreeView *treeview, GdkEventButton *event,
			 FMTreeView *view)
{
	GtkTreePath *path, *cursor_path;
	gboolean parent_file_is_writable;
	gboolean file_is_home_or_desktop;
	gboolean file_is_special_link;
	gboolean can_move_file_to_trash;
	gboolean can_delete_file;
	gboolean using_browser;

	using_browser = g_settings_get_boolean (nemo_preferences,
						NEMO_PREFERENCES_ALWAYS_USE_BROWSER);

	if (event->button == 3) {
		gboolean show_unmount = FALSE;
		gboolean show_eject = FALSE;
		GMount *mount = NULL;

		if (view->details->popup_file != NULL) {
			return FALSE; /* Already up, ignore */
		}
		
		if (!gtk_tree_view_get_path_at_pos (treeview, event->x, event->y,
						    &path, NULL, NULL, NULL)) {
			return FALSE;
		}

		view->details->popup_file = sort_model_path_to_file (view, path);
		if (view->details->popup_file == NULL) {
			gtk_tree_path_free (path);
			return FALSE;
		}
		gtk_tree_view_get_cursor (view->details->tree_widget, &cursor_path, NULL);

		gtk_tree_path_free (path);

		create_popup_menu (view);

		if (using_browser) {
			gtk_widget_set_sensitive (view->details->popup_open_in_new_window,
						  nemo_file_is_directory (view->details->popup_file));
			gtk_widget_set_sensitive (view->details->popup_open_in_new_tab,
						  nemo_file_is_directory (view->details->popup_file));
		}

		gtk_widget_set_sensitive (view->details->popup_create_folder,
			nemo_file_is_directory (view->details->popup_file) &&
			nemo_file_can_write (view->details->popup_file));
		gtk_widget_set_sensitive (view->details->popup_paste, FALSE);
		if (nemo_file_is_directory (view->details->popup_file) &&
			nemo_file_can_write (view->details->popup_file)) {
			gtk_clipboard_request_contents (nemo_clipboard_get (GTK_WIDGET (view->details->tree_widget)),
							copied_files_atom,
							clipboard_contents_received_callback, g_object_ref (view));
		}
		can_move_file_to_trash = nemo_file_can_trash (view->details->popup_file);
		gtk_widget_set_sensitive (view->details->popup_trash, can_move_file_to_trash);
		
		if (g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_ENABLE_DELETE)) {
			parent_file_is_writable = is_parent_writable (view->details->popup_file);
			file_is_home_or_desktop = nemo_file_is_home (view->details->popup_file)
				|| nemo_file_is_desktop_directory (view->details->popup_file);
			file_is_special_link = NEMO_IS_DESKTOP_ICON_FILE (view->details->popup_file);
			
			can_delete_file = parent_file_is_writable 
				&& !file_is_home_or_desktop
				&& !file_is_special_link;

			gtk_widget_show (view->details->popup_delete);
			gtk_widget_set_sensitive (view->details->popup_delete, can_delete_file);
		} else {
			gtk_widget_hide (view->details->popup_delete);
		}

		mount = fm_tree_model_get_mount_for_root_node_file (view->details->child_model, view->details->popup_file);
		if (mount) {
			show_unmount = g_mount_can_unmount (mount);
			show_eject = g_mount_can_eject (mount);
		}

		if (show_unmount) {
			gtk_widget_show (view->details->popup_unmount);
		} else {
			gtk_widget_hide (view->details->popup_unmount);
		}

		if (show_eject) {
			gtk_widget_show (view->details->popup_eject);
		} else {
			gtk_widget_hide (view->details->popup_eject);
		}

		if (show_unmount || show_eject) {
			gtk_widget_show (view->details->popup_unmount_separator);
		} else {
			gtk_widget_hide (view->details->popup_unmount_separator);
		}

        gboolean actions_visible = FALSE;

        GList *l;
        NemoFile *file = view->details->popup_file;
        NemoFile *parent = nemo_file_get_parent (file);
        GList *tmp = NULL;
        tmp = g_list_append (tmp, file);
        ActionPayload *p;

        for (l = view->details->action_items; l != NULL; l = l->next) {
            p = l->data;
            if (nemo_action_get_visibility (p->action, tmp, parent)) {
                gtk_menu_item_set_label (GTK_MENU_ITEM (p->item), nemo_action_get_label (p->action, tmp, parent));
                gtk_widget_set_visible (p->item, TRUE);
                actions_visible = TRUE;
            } else {
                gtk_widget_set_visible (p->item, FALSE);
            }
        }

        gtk_widget_set_visible (view->details->popup_action_separator, actions_visible);

        nemo_file_list_free (tmp);

		gtk_menu_popup (GTK_MENU (view->details->popup),
				NULL, NULL, NULL, NULL,
				event->button, event->time);

		gtk_tree_view_set_cursor (view->details->tree_widget, cursor_path, NULL, FALSE);
		gtk_tree_path_free (cursor_path);

		return FALSE;
	} else if (event->button == 2 && event->type == GDK_BUTTON_PRESS) {
		NemoFile *file;
		NemoWindowOpenFlags flags = 0;

		if (!gtk_tree_view_get_path_at_pos (treeview, event->x, event->y,
						    &path, NULL, NULL, NULL)) {
			return FALSE;
		}

		file = sort_model_path_to_file (view, path);

		if (using_browser) {
			flags = (event->state & GDK_CONTROL_MASK) ?
				NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW :
				NEMO_WINDOW_OPEN_FLAG_NEW_TAB;
		} else {
			flags = NEMO_WINDOW_OPEN_FLAG_CLOSE_BEHIND;
		}

		if (file) {
			fm_tree_view_activate_file (view, file, flags);
			nemo_file_unref (file);
		}

		gtk_tree_path_free (path);

		return TRUE;
	}

	return FALSE;
}

static void
fm_tree_view_activate_file (FMTreeView *view, 
			    NemoFile *file,
			    NemoWindowOpenFlags flags)
{
	NemoFileAttributes attributes;

	cancel_activation (view);

	view->details->activation_file = nemo_file_ref (file);
	view->details->activation_flags = flags;
		
	attributes = NEMO_FILE_ATTRIBUTE_INFO | NEMO_FILE_ATTRIBUTE_LINK_INFO;
	nemo_file_call_when_ready (view->details->activation_file, attributes,
				       got_activation_uri_callback, view);
}

static void
fm_tree_view_open_cb (GtkWidget *menu_item,
		      FMTreeView *view)
{
	fm_tree_view_activate_file (view, view->details->popup_file, 0);
}

static void
fm_tree_view_open_in_new_tab_cb (GtkWidget *menu_item,
				    FMTreeView *view)
{
	fm_tree_view_activate_file (view, view->details->popup_file, NEMO_WINDOW_OPEN_FLAG_NEW_TAB);
}

static void
fm_tree_view_open_in_new_window_cb (GtkWidget *menu_item,
				    FMTreeView *view)
{
	fm_tree_view_activate_file (view, view->details->popup_file, NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW);
}

static void
new_folder_done (GFile *new_folder, 
		 gboolean success,
		 gpointer data)
{
	GList *list;

	/* show the properties window for the newly created
	 * folder so the user can change its name
	 */
	list = g_list_prepend (NULL, nemo_file_get (new_folder));

	nemo_properties_window_present (list, GTK_WIDGET (data), NULL);

        nemo_file_list_free (list);
}

static void
fm_tree_view_create_folder_cb (GtkWidget *menu_item,
			       FMTreeView *view)
{
	char *parent_uri;

	parent_uri = nemo_file_get_uri (view->details->popup_file);
	nemo_file_operations_new_folder (GTK_WIDGET (view->details->tree_widget),
					     NULL,
					     parent_uri,
					     new_folder_done, view->details->tree_widget);

	g_free (parent_uri);
}

static void
copy_or_cut_files (FMTreeView *view,
		   gboolean cut)
{
	char *status_string, *name;
	NemoClipboardInfo info;
        GtkTargetList *target_list;
        GtkTargetEntry *targets;
        int n_targets;

	info.cut = cut;
	info.files = g_list_prepend (NULL, view->details->popup_file);

        target_list = gtk_target_list_new (NULL, 0);
        gtk_target_list_add (target_list, copied_files_atom, 0, 0);
        gtk_target_list_add_uri_targets (target_list, 0);
        gtk_target_list_add_text_targets (target_list, 0);

        targets = gtk_target_table_new_from_list (target_list, &n_targets);
        gtk_target_list_unref (target_list);

	gtk_clipboard_set_with_data (nemo_clipboard_get (GTK_WIDGET (view->details->tree_widget)),
				     targets, n_targets,
				     nemo_get_clipboard_callback, nemo_clear_clipboard_callback,
				     NULL);
        gtk_target_table_free (targets, n_targets);

	nemo_clipboard_monitor_set_clipboard_info (nemo_clipboard_monitor_get (),
	                                               &info);
	g_list_free (info.files);

	name = nemo_file_get_display_name (view->details->popup_file);
	if (cut) {
		status_string = g_strdup_printf (_("\"%s\" will be moved "
						   "if you select the Paste command"),
						 name);
	} else {
		status_string = g_strdup_printf (_("\"%s\" will be copied "
						   "if you select the Paste command"),
						 name);
	}
	g_free (name);
	
	nemo_window_push_status (view->details->window,
					  status_string);
	g_free (status_string);
}

static void
fm_tree_view_cut_cb (GtkWidget *menu_item,
		     FMTreeView *view)
{
	copy_or_cut_files (view, TRUE);
}

static void
fm_tree_view_copy_cb (GtkWidget *menu_item,
		      FMTreeView *view)
{
	copy_or_cut_files (view, FALSE);
}

static void
paste_clipboard_data (FMTreeView *view,
		      GtkSelectionData *selection_data,
		      char *destination_uri)
{
	gboolean cut;
	GList *item_uris;

	cut = FALSE;
	item_uris = nemo_clipboard_get_uri_list_from_selection_data (selection_data, &cut,
									 copied_files_atom);

	if (item_uris == NULL|| destination_uri == NULL) {
		nemo_window_push_status (view->details->window,
						  _("There is nothing on the clipboard to paste."));
	} else {
		nemo_file_operations_copy_move
			(item_uris, NULL, destination_uri,
			 cut ? GDK_ACTION_MOVE : GDK_ACTION_COPY,
			 GTK_WIDGET (view->details->tree_widget),
			 NULL, NULL);

		/* If items are cut then remove from clipboard */
		if (cut) {
			gtk_clipboard_clear (nemo_clipboard_get (GTK_WIDGET (view)));
		}

		g_list_free_full (item_uris, g_free);
	}
}

static void
paste_into_clipboard_received_callback (GtkClipboard     *clipboard,
					GtkSelectionData *selection_data,
					gpointer          data)
{
	FMTreeView *view;
	char *directory_uri;

	view = FM_TREE_VIEW (data);

	directory_uri = nemo_file_get_uri (view->details->popup_file);

	paste_clipboard_data (view, selection_data, directory_uri);

	g_free (directory_uri);
}

static void
fm_tree_view_paste_cb (GtkWidget *menu_item,
		       FMTreeView *view)
{
	gtk_clipboard_request_contents (nemo_clipboard_get (GTK_WIDGET (view->details->tree_widget)),
					copied_files_atom,
					paste_into_clipboard_received_callback, view);
}

static GtkWindow *
fm_tree_view_get_containing_window (FMTreeView *view)
{
	GtkWidget *window;

	g_assert (FM_IS_TREE_VIEW (view));

	window = gtk_widget_get_ancestor (GTK_WIDGET (view), GTK_TYPE_WINDOW);
	if (window == NULL) {
		return NULL;
	}

	return GTK_WINDOW (window);
}

static void
fm_tree_view_trash_cb (GtkWidget *menu_item,
		       FMTreeView *view)
{
	GList *list;

	if (!nemo_file_can_trash (view->details->popup_file)) {
		return;
	}
	
	list = g_list_prepend (NULL,
			       nemo_file_get_location (view->details->popup_file));
	
	nemo_file_operations_trash_or_delete (list, 
						  fm_tree_view_get_containing_window (view),
						  NULL, NULL);
	g_list_free_full (list, g_object_unref);
}

static void
fm_tree_view_delete_cb (GtkWidget *menu_item,
		        FMTreeView *view)
{
	GList *location_list;
		
	if (!g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_ENABLE_DELETE)) {
		return;
	}
	
	location_list = g_list_prepend (NULL,
					nemo_file_get_location (view->details->popup_file));
	
	nemo_file_operations_delete (location_list, fm_tree_view_get_containing_window (view), NULL, NULL);
	g_list_free_full (location_list, g_object_unref);
}

static void
fm_tree_view_properties_cb (GtkWidget *menu_item,
			    FMTreeView *view)
{
	GList *list;
        
	list = g_list_prepend (NULL, nemo_file_ref (view->details->popup_file));

	nemo_properties_window_present (list, GTK_WIDGET (view->details->tree_widget), NULL);

        nemo_file_list_free (list);
}

static void
fm_tree_view_unmount_cb (GtkWidget *menu_item,
			 FMTreeView *view)
{
	NemoFile *file = view->details->popup_file;
	GMount *mount;
	
	if (file == NULL) {
		return;
	}

	mount = fm_tree_model_get_mount_for_root_node_file (view->details->child_model, file);
	
	if (mount != NULL) {
		nemo_file_operations_unmount_mount (fm_tree_view_get_containing_window (view),
							mount, FALSE, TRUE);
	}
}

static void
fm_tree_view_eject_cb (GtkWidget *menu_item,
		       FMTreeView *view)
{
	NemoFile *file = view->details->popup_file;
	GMount *mount;
	
	if (file == NULL) {
		return;
	}

	mount = fm_tree_model_get_mount_for_root_node_file (view->details->child_model, file);
	
	if (mount != NULL) {
		nemo_file_operations_unmount_mount (fm_tree_view_get_containing_window (view),
							mount, TRUE, TRUE);
	}
}

static gboolean
free_popup_file_in_idle_cb (gpointer data)
{
	FMTreeView *view;

	view = FM_TREE_VIEW (data);

	if (view->details->popup_file != NULL) {
		nemo_file_unref (view->details->popup_file);
		view->details->popup_file = NULL;
	}
	view->details->popup_file_idle_handler = 0;
	return FALSE;
}

static void
popup_menu_deactivated (GtkMenuShell *menu_shell, gpointer data)
{
	FMTreeView *view;

	view = FM_TREE_VIEW (data);

	/* The popup menu is deactivated. (I.E. hidden)
	   We want to free popup_file, but can't right away as it might immediately get
	   used if we're deactivation due to activating a menu item. So, we free it in
	   idle */
	
	if (view->details->popup_file != NULL &&
	    view->details->popup_file_idle_handler == 0) {
		view->details->popup_file_idle_handler = g_idle_add (free_popup_file_in_idle_cb, view);
	}
}

static void
action_payload_free (gpointer data)
{
    ActionPayload *p = (ActionPayload *) data;
    gtk_widget_destroy (GTK_WIDGET (p->item));
}

static void
actions_added_or_changed_callback (FMTreeView *view)
{
    GList *tmp = view->details->action_items;
    view->details->action_items = NULL;
    g_list_free_full (tmp, action_payload_free);

    add_action_popup_items (view);
}

static void
action_activated_callback (GtkMenuItem *item, ActionPayload *payload)
{
    gchar *uri = NULL;

    FMTreeView *view = payload->view;

    NemoFile *file = view->details->popup_file;
    NemoFile *parent = nemo_file_get_parent (file);
    GList *tmp = NULL;
    tmp = g_list_append (tmp, file);

    nemo_action_activate (NEMO_ACTION (payload->action), tmp, parent);

    nemo_file_list_free (tmp);

    g_free (uri);
}

static void
add_action_popup_items (FMTreeView *view)
{
    GList *action_list = nemo_action_manager_list_actions (view->details->action_manager);
    GList *l;
    GtkWidget *menu_item;
    NemoAction *action;
    ActionPayload *payload;

    gint index = 13;

    for (l = action_list; l != NULL; l = l->next) {
        action = l->data;
        payload = g_new0 (ActionPayload, 1);
        payload->action = action;
        payload->view = view;
        menu_item = gtk_menu_item_new_with_mnemonic (nemo_action_get_orig_label (action));
        payload->item = menu_item;
        g_signal_connect (menu_item, "activate", G_CALLBACK (action_activated_callback), payload);
        gtk_widget_show (menu_item);
        gtk_menu_shell_insert (GTK_MENU_SHELL (view->details->popup), menu_item, index);
        view->details->action_items = g_list_append (view->details->action_items, payload);
        index ++;
    }
}

static void
create_popup_menu (FMTreeView *view)
{
	GtkWidget *popup, *menu_item, *menu_image;

	if (view->details->popup != NULL) {
		/* already created */
		return;
	}
	
	popup = gtk_menu_new ();

	g_signal_connect (popup, "deactivate",
			  G_CALLBACK (popup_menu_deactivated),
			  view);
	
	
	/* add the "open" menu item */
	menu_image = gtk_image_new_from_stock (GTK_STOCK_OPEN,
					       GTK_ICON_SIZE_MENU);
	gtk_widget_show (menu_image);
	menu_item = gtk_image_menu_item_new_with_mnemonic (_("_Open"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
				       menu_image);
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (fm_tree_view_open_cb),
			  view);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup), menu_item);
	view->details->popup_open = menu_item;
	
	/* add the "open in new tab" menu item */
	menu_item = gtk_menu_item_new_with_mnemonic (_("Open in New _Tab"));
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (fm_tree_view_open_in_new_tab_cb),
			  view);
	g_settings_bind (nemo_preferences,
			 NEMO_PREFERENCES_ALWAYS_USE_BROWSER,
			 menu_item,
			 "visible",
			 G_SETTINGS_BIND_GET);

	gtk_menu_shell_append (GTK_MENU_SHELL (popup), menu_item);
	view->details->popup_open_in_new_tab = menu_item;
	
	/* add the "open in new window" menu item */
	menu_item = gtk_menu_item_new_with_mnemonic (_("Open in New _Window"));
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (fm_tree_view_open_in_new_window_cb),
			  view);
	g_settings_bind (nemo_preferences,
			 NEMO_PREFERENCES_ALWAYS_USE_BROWSER,
			 menu_item,
			 "visible",
			 G_SETTINGS_BIND_GET);

	gtk_menu_shell_append (GTK_MENU_SHELL (popup), menu_item);
	view->details->popup_open_in_new_window = menu_item;
	
	eel_gtk_menu_append_separator (GTK_MENU (popup));

	/* add the "create new folder" menu item */
	menu_item = gtk_image_menu_item_new_with_mnemonic (_("Create New _Folder"));
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (fm_tree_view_create_folder_cb),
			  view);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup), menu_item);
	view->details->popup_create_folder = menu_item;
	
	eel_gtk_menu_append_separator (GTK_MENU (popup));
	
	/* add the "cut folder" menu item */
	menu_item = gtk_image_menu_item_new_from_stock (GTK_STOCK_CUT, NULL);
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (fm_tree_view_cut_cb),
			  view);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup), menu_item);
	view->details->popup_cut = menu_item;
	
	/* add the "copy folder" menu item */
	menu_item = gtk_image_menu_item_new_from_stock (GTK_STOCK_COPY, NULL);
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (fm_tree_view_copy_cb),
			  view);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup), menu_item);
	view->details->popup_copy = menu_item;
	
	/* add the "paste files into folder" menu item */
	menu_image = gtk_image_new_from_stock (GTK_STOCK_PASTE,
					       GTK_ICON_SIZE_MENU);
	gtk_widget_show (menu_image);
	menu_item = gtk_image_menu_item_new_with_mnemonic (_("_Paste Into Folder"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
				       menu_image);
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (fm_tree_view_paste_cb),
			  view);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup), menu_item);
	view->details->popup_paste = menu_item;
	
	eel_gtk_menu_append_separator (GTK_MENU (popup));
	
	/* add the "move to trash" menu item */
	menu_image = gtk_image_new_from_icon_name (NEMO_ICON_TRASH_FULL,
						   GTK_ICON_SIZE_MENU);
	gtk_widget_show (menu_image);
	menu_item = gtk_image_menu_item_new_with_mnemonic (_("Mo_ve to Trash"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
				       menu_image);
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (fm_tree_view_trash_cb),
			  view);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup), menu_item);
	view->details->popup_trash = menu_item;
	
	/* add the "delete" menu item */
	menu_image = gtk_image_new_from_icon_name (NEMO_ICON_DELETE,
						   GTK_ICON_SIZE_MENU);
	gtk_widget_show (menu_image);
	menu_item = gtk_image_menu_item_new_with_mnemonic (_("_Delete"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
				       menu_image);
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (fm_tree_view_delete_cb),
			  view);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup), menu_item);
	view->details->popup_delete = menu_item;

    /* Nemo Actions */

    view->details->popup_action_separator =
        GTK_WIDGET (eel_gtk_menu_append_separator (GTK_MENU (popup)));

	eel_gtk_menu_append_separator (GTK_MENU (popup));

	/* add the "Unmount" menu item */
	menu_item = gtk_image_menu_item_new_with_mnemonic (_("_Unmount"));
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (fm_tree_view_unmount_cb),
			  view);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup), menu_item);
	view->details->popup_unmount = menu_item;

	/* add the "Eject" menu item */
	menu_item = gtk_image_menu_item_new_with_mnemonic (_("_Eject"));
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (fm_tree_view_eject_cb),
			  view);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup), menu_item);
	view->details->popup_eject = menu_item;

	/* add the unmount separator menu item */
	view->details->popup_unmount_separator =
		GTK_WIDGET (eel_gtk_menu_append_separator (GTK_MENU (popup)));

	/* add the "properties" menu item */
	menu_item = gtk_image_menu_item_new_from_stock (GTK_STOCK_PROPERTIES, NULL);
	g_signal_connect (menu_item, "activate",
			  G_CALLBACK (fm_tree_view_properties_cb),
			  view);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup), menu_item);
	view->details->popup_properties = menu_item;

    view->details->popup = popup;

    add_action_popup_items (view);
}

static void
create_tree (FMTreeView *view)
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GVolumeMonitor *volume_monitor;
	char *home_uri;
	GList *mounts, *l;
	char *location;
	GIcon *icon;
	NemoWindowSlot *slot;
	
	view->details->child_model = fm_tree_model_new ();
	view->details->sort_model = GTK_TREE_MODEL_SORT
		(gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (view->details->child_model)));
	view->details->tree_widget = GTK_TREE_VIEW
		(gtk_tree_view_new_with_model (GTK_TREE_MODEL (view->details->sort_model)));
	g_object_unref (view->details->sort_model);

	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (view->details->tree_widget)),
				     "NemoSidebar");

	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (view->details->sort_model),
						 compare_rows, view, NULL);

	g_signal_connect_object
		(view->details->child_model, "row_loaded",
		 G_CALLBACK (row_loaded_callback),
		 view, G_CONNECT_AFTER);
#ifdef NOT_YET_USABLE /* Do we really want this? */
	icon = g_themed_icon_new (NEMO_ICON_COMPUTER);
	fm_tree_model_add_root_uri (view->details->child_model, "computer:///", _("Computer"), icon, NULL);
	g_object_unref (icon);
#endif
 	home_uri = nemo_get_home_directory_uri ();
	icon = g_themed_icon_new (NEMO_ICON_HOME);
	fm_tree_model_add_root_uri (view->details->child_model, home_uri, _("Home"), icon, NULL);
	g_object_unref (icon);
	g_free (home_uri);
	icon = g_themed_icon_new (NEMO_ICON_FILESYSTEM);
	fm_tree_model_add_root_uri (view->details->child_model, "file:///", _("File System"), icon, NULL);
	g_object_unref (icon);
#ifdef NOT_YET_USABLE /* Do we really want this? */
	icon = g_themed_icon_new (NEMO_ICON_NETWORK);
	fm_tree_model_add_root_uri (view->details->child_model, "network:///", _("Network"), icon, NULL);
	g_object_unref (icon);
#endif
	
	volume_monitor = g_volume_monitor_get ();
	view->details->volume_monitor = volume_monitor;
	mounts = g_volume_monitor_get_mounts (volume_monitor);
	for (l = mounts; l != NULL; l = l->next) {
		add_root_for_mount (view, l->data);
		g_object_unref (l->data);
	}
	g_list_free (mounts);
	
	g_signal_connect_object (volume_monitor, "mount_added",
				 G_CALLBACK (mount_added_callback), view, 0);
	g_signal_connect_object (volume_monitor, "mount_removed",
				 G_CALLBACK (mount_removed_callback), view, 0);
	
	g_object_unref (view->details->child_model);

	gtk_tree_view_set_headers_visible (view->details->tree_widget, FALSE);

	view->details->drag_dest = 
		nemo_tree_view_drag_dest_new (view->details->tree_widget);
	g_signal_connect_object (view->details->drag_dest, 
				 "get_root_uri",
				 G_CALLBACK (get_root_uri_callback),
				 view, 0);
	g_signal_connect_object (view->details->drag_dest, 
				 "get_file_for_path",
				 G_CALLBACK (get_file_for_path_callback),
				 view, 0);
	g_signal_connect_object (view->details->drag_dest,
				 "move_copy_items",
				 G_CALLBACK (move_copy_items_callback),
				 view, 0);

    view->details->action_manager = nemo_action_manager_new ();

    view->details->action_manager_changed_id =
        g_signal_connect_swapped (view->details->action_manager, "changed",
                      G_CALLBACK (actions_added_or_changed_callback),
                                  view);

    view->details->action_items = NULL;

	/* Create column */
	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);
	gtk_tree_view_column_set_attributes (column, cell,
					     "pixbuf", FM_TREE_MODEL_CLOSED_PIXBUF_COLUMN,
					     "pixbuf_expander_closed", FM_TREE_MODEL_CLOSED_PIXBUF_COLUMN,
					     "pixbuf_expander_open", FM_TREE_MODEL_OPEN_PIXBUF_COLUMN,
					     NULL);
	
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_attributes (column, cell,
					     "text", FM_TREE_MODEL_DISPLAY_NAME_COLUMN,
					     "style", FM_TREE_MODEL_FONT_STYLE_COLUMN,
					     NULL);

	gtk_tree_view_append_column (view->details->tree_widget, column);

	gtk_widget_show (GTK_WIDGET (view->details->tree_widget));

	gtk_container_add (GTK_CONTAINER (view),
			   GTK_WIDGET (view->details->tree_widget));

	g_signal_connect_object (gtk_tree_view_get_selection (GTK_TREE_VIEW (view->details->tree_widget)), "changed",
				 G_CALLBACK (selection_changed_callback), view, 0);

	g_signal_connect (G_OBJECT (view->details->tree_widget), 
			  "row-activated", G_CALLBACK (row_activated_callback),
			  view);

	g_signal_connect (G_OBJECT (view->details->tree_widget), 
			  "button_press_event", G_CALLBACK (button_pressed_callback),
			  view);

	slot = nemo_window_get_active_slot (view->details->window);
	location = nemo_window_slot_get_current_uri (slot);
	schedule_select_and_show_location (view, location);
	g_free (location);
}

static void
update_filtering_from_preferences (FMTreeView *view)
{
    NemoWindowShowHiddenFilesMode mode;

    if (view->details->child_model == NULL) {
        return;
    }

    mode = nemo_window_get_hidden_files_mode (view->details->window);

    fm_tree_model_set_show_hidden_files (view->details->child_model,
                                         mode == NEMO_WINDOW_SHOW_HIDDEN_FILES_ENABLE);

    fm_tree_model_set_show_only_directories (view->details->child_model,
                                             g_settings_get_boolean (nemo_tree_sidebar_preferences,
                                                                     NEMO_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES));
}

static void
parent_set_callback (GtkWidget        *widget,
		     GtkWidget        *previous_parent,
		     gpointer          callback_data)
{
	FMTreeView *view;

	view = FM_TREE_VIEW (callback_data);

	if (gtk_widget_get_parent (widget) != NULL && view->details->tree_widget == NULL) {
		create_tree (view);
		update_filtering_from_preferences (view);
	}
}

static void
filtering_changed_callback (gpointer callback_data)
{
	update_filtering_from_preferences (FM_TREE_VIEW (callback_data));
}

static void
loading_uri_callback (NemoWindow *window,
		      char *location,
		      gpointer callback_data)
{
	FMTreeView *view;

	view = FM_TREE_VIEW (callback_data);
	schedule_select_and_show_location (view, location);
}

static void
fm_tree_view_init (FMTreeView *view)
{
	view->details = g_new0 (FMTreeViewDetails, 1);
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view), 
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (view), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (view), NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (view), GTK_SHADOW_IN);
	
	gtk_widget_show (GTK_WIDGET (view));

	g_signal_connect_object (view, "parent_set",
				 G_CALLBACK (parent_set_callback), view, 0);

	view->details->selection_location = NULL;
	
	view->details->selecting = FALSE;

	g_signal_connect_swapped (nemo_tree_sidebar_preferences,
				  "changed::" NEMO_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES,
				  G_CALLBACK (filtering_changed_callback), view);

	view->details->popup_file = NULL;

	view->details->clipboard_handler_id =
		g_signal_connect (nemo_clipboard_monitor_get (),
				  "clipboard_info",
				  G_CALLBACK (notify_clipboard_info), view);
}

static void 
hidden_files_mode_changed_callback (NemoWindow *window,
                    FMTreeView *view)
{
    update_filtering_from_preferences (view);
}

static void
fm_tree_view_dispose (GObject *object)
{
	FMTreeView *view;
	
	view = FM_TREE_VIEW (object);
	
	if (view->details->selection_changed_timer) {
		g_source_remove (view->details->selection_changed_timer);
		view->details->selection_changed_timer = 0;
	}

	if (view->details->drag_dest) {
		g_object_unref (view->details->drag_dest);
		view->details->drag_dest = NULL;
	}

	if (view->details->show_selection_idle_id) {
		g_source_remove (view->details->show_selection_idle_id);
		view->details->show_selection_idle_id = 0;
	}

	if (view->details->clipboard_handler_id != 0) {
		g_signal_handler_disconnect (nemo_clipboard_monitor_get (),
		                             view->details->clipboard_handler_id);
		view->details->clipboard_handler_id = 0;
	}

	cancel_activation (view);

	if (view->details->popup != NULL) {
		gtk_widget_destroy (view->details->popup);
		view->details->popup = NULL;
	}

	if (view->details->popup_file_idle_handler != 0) {
		g_source_remove (view->details->popup_file_idle_handler);
		view->details->popup_file_idle_handler = 0;
	}
	
	if (view->details->popup_file != NULL) {
		nemo_file_unref (view->details->popup_file);
		view->details->popup_file = NULL;
	}

	if (view->details->selection_location != NULL) {
		g_free (view->details->selection_location);
		view->details->selection_location = NULL;
	}

	if (view->details->volume_monitor != NULL) {
		g_object_unref (view->details->volume_monitor);
		view->details->volume_monitor = NULL;
	}

    if (view->details->action_manager_changed_id != 0) {
        g_signal_handler_disconnect (view->details->action_manager,
                                     view->details->action_manager_changed_id);
        view->details->action_manager_changed_id = 0;
    }

    if (view->details->hidden_files_changed_id != 0) {
        g_signal_handler_disconnect (view->details->window,
                                     view->details->hidden_files_changed_id);
        view->details->hidden_files_changed_id = 0;
    }

    g_clear_object (&view->details->action_manager);

	g_signal_handlers_disconnect_by_func (nemo_tree_sidebar_preferences,
					      G_CALLBACK(filtering_changed_callback),
					      view);

	view->details->window = NULL;

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
fm_tree_view_finalize (GObject *object)
{
	FMTreeView *view;

	view = FM_TREE_VIEW (object);

	g_free (view->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
fm_tree_view_class_init (FMTreeViewClass *class)
{
	G_OBJECT_CLASS (class)->dispose = fm_tree_view_dispose;
	G_OBJECT_CLASS (class)->finalize = fm_tree_view_finalize;

	copied_files_atom = gdk_atom_intern ("x-special/gnome-copied-files", FALSE);
}

static void
fm_tree_view_set_parent_window (FMTreeView *sidebar,
				NemoWindow *window)
{
	char *location;
	NemoWindowSlot *slot;
	
	sidebar->details->window = window;

	slot = nemo_window_get_active_slot (window);

	g_signal_connect_object (window, "loading_uri",
				 G_CALLBACK (loading_uri_callback), sidebar, 0);
	location = nemo_window_slot_get_current_uri (slot);
	loading_uri_callback (window, location, sidebar);
	g_free (location);

	sidebar->details->hidden_files_changed_id = 
                    g_signal_connect_object (window, "hidden-files-mode-changed",
                                             G_CALLBACK (hidden_files_mode_changed_callback), sidebar, 0);

}

GtkWidget *
nemo_tree_sidebar_new (NemoWindow *window)
{
	FMTreeView *sidebar;
	
	sidebar = g_object_new (fm_tree_view_get_type (), NULL);
	fm_tree_view_set_parent_window (sidebar, window);

	return GTK_WIDGET (sidebar);
}


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

#include "nemo-actions.h"
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
#include <libnemo-private/nemo-ui-utilities.h>

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

	GtkWidget *popup_menu;

	NemoFile *popup_file;
	guint popup_file_idle_handler;
	
	guint selection_changed_timer;

    NemoActionManager *action_manager;
    gulong actions_changed_id;
    guint actions_changed_idle_id;

    GtkUIManager *ui_manager;
    GtkActionGroup *tv_action_group;
    guint tv_action_group_merge_id;
    GtkActionGroup *action_action_group;
    guint action_action_group_merge_id;

    gboolean actions_need_update;

    guint hidden_files_changed_id;
    guint sort_directories_first : 1;
    guint sort_favorites_first : 1;
};

typedef struct {
	GList *uris;
	FMTreeView *view;
} PrependURIParameters;

static GdkAtom copied_files_atom;

static void  fm_tree_view_activate_file     (FMTreeView *view, 
			    		     NemoFile *file,
					     NemoWindowOpenFlags flags);

static void popup_menu (FMTreeView *view, GdkEventButton *event);
static void rebuild_menu (FMTreeView *view);
// static void create_popup_menu (FMTreeView *view);

// static void add_action_popup_items (FMTreeView *view);

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
				(slot, location, 
				 view->details->activation_flags);
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
				(slot, location,
				 view->details->activation_flags);
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

	/* no activation if popup menu is open */
	if (view->details->popup_file != NULL) {
		return FALSE;
	}

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->details->tree_widget));
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
                                             FM_TREE_VIEW (callback_data)->details->sort_directories_first,
                                             FM_TREE_VIEW (callback_data)->details->sort_favorites_first,
                                             FALSE,
                                             NULL);
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

static void
set_action_sensitive (GtkActionGroup *action_group,
                      const gchar    *name,
                      gboolean        sensitive)
{
    GtkAction *action;

    action = gtk_action_group_get_action (action_group, name);
    gtk_action_set_sensitive (action, sensitive);
}

static void
set_action_visible (GtkActionGroup *action_group,
                    const gchar    *name,
                    gboolean        visible)
{
    GtkAction *action;

    action = gtk_action_group_get_action (action_group, name);
    gtk_action_set_visible (action, visible);
}

static void
clipboard_contents_received_callback (GtkClipboard     *clipboard,
                      GtkSelectionData *selection_data,
                      gpointer          data)
{
    FMTreeView *view;

    view = FM_TREE_VIEW (data);

    if (gtk_selection_data_get_data_type (selection_data) == copied_files_atom
        && gtk_selection_data_get_length (selection_data) > 0) {
        set_action_sensitive (view->details->tv_action_group, NEMO_ACTION_PASTE, TRUE);
    }

    g_object_unref (view);
}

static void
update_menu_states (FMTreeView *view,
                    GdkEventButton *event)
{
    GtkTreePath *path;
    gboolean parent_file_is_writable;
    gboolean file_is_home_or_desktop;
    gboolean file_is_special_link;
    gboolean can_move_file_to_trash;
    gboolean can_delete_file;
    gboolean using_browser;
    gboolean is_dir;
    gboolean can_write;
    gint is_toplevel;

    using_browser = g_settings_get_boolean (nemo_preferences,
                                            NEMO_PREFERENCES_ALWAYS_USE_BROWSER);

    gboolean show_unmount = FALSE;
    gboolean show_eject = FALSE;
    GMount *mount = NULL;

    if (!gtk_tree_view_get_path_at_pos (view->details->tree_widget, event->x, event->y,
                                        &path, NULL, NULL, NULL)) {
        return;
    }

    NemoFile *file = sort_model_path_to_file (view, path);
    view->details->popup_file = nemo_file_ref (file);

    NemoFile *parent = nemo_file_get_parent (file);
    GList *selected = g_list_prepend (NULL, file);

    nemo_action_manager_update_action_states (view->details->action_manager,
                                              view->details->action_action_group,
                                              selected,
                                              parent,
                                              FALSE,
                                              FALSE,
                                              GTK_WINDOW (view->details->window));
    nemo_file_list_free (selected);
    nemo_file_unref (parent);

    is_dir = nemo_file_is_directory (file);
    can_write = nemo_file_can_write (file);

    is_toplevel = gtk_tree_path_get_depth (path) == 1;
    gtk_tree_path_free (path);

    set_action_sensitive (view->details->tv_action_group, NEMO_ACTION_OPEN_ALTERNATE, is_dir);
    set_action_sensitive (view->details->tv_action_group, NEMO_ACTION_OPEN_IN_NEW_TAB, is_dir);
    set_action_visible (view->details->tv_action_group, NEMO_ACTION_OPEN_ALTERNATE, using_browser);
    set_action_visible (view->details->tv_action_group, NEMO_ACTION_OPEN_IN_NEW_TAB, using_browser);
    set_action_sensitive (view->details->tv_action_group, NEMO_ACTION_NEW_FOLDER, is_dir && can_write);
    set_action_visible (view->details->tv_action_group, NEMO_ACTION_NEW_FOLDER, !nemo_file_is_in_favorites (file));
    set_action_sensitive (view->details->tv_action_group, NEMO_ACTION_PASTE, FALSE);

    if (is_dir && can_write) {
        gtk_clipboard_request_contents (nemo_clipboard_get (GTK_WIDGET (view->details->tree_widget)),
                                        copied_files_atom,
                                        clipboard_contents_received_callback, g_object_ref (view));
    }

    can_move_file_to_trash = nemo_file_can_trash (file);
    set_action_sensitive (view->details->tv_action_group, NEMO_ACTION_TRASH, can_move_file_to_trash);

    if (g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_ENABLE_DELETE)) {
    	parent_file_is_writable = is_parent_writable (file);
    	file_is_home_or_desktop = nemo_file_is_home (file)
    		|| nemo_file_is_desktop_directory (file);
    	file_is_special_link = NEMO_IS_DESKTOP_ICON_FILE (file);
    	
    	can_delete_file = parent_file_is_writable 
    		&& !file_is_home_or_desktop
    		&& !file_is_special_link;

        set_action_visible (view->details->tv_action_group, NEMO_ACTION_DELETE, TRUE);
        set_action_sensitive (view->details->tv_action_group, NEMO_ACTION_DELETE, can_delete_file);
    } else {
        set_action_visible (view->details->tv_action_group, NEMO_ACTION_DELETE, FALSE);
    }

    mount = fm_tree_model_get_mount_for_root_node_file (view->details->child_model, file);
    if (mount) {
    	show_unmount = g_mount_can_unmount (mount);
    	show_eject = g_mount_can_eject (mount);
    }

    set_action_visible (view->details->tv_action_group, NEMO_ACTION_UNMOUNT_VOLUME, show_unmount);
    set_action_visible (view->details->tv_action_group, NEMO_ACTION_EJECT_VOLUME, show_eject);

    if (!is_toplevel && !nemo_file_is_in_favorites (file)) {
        set_action_visible (view->details->tv_action_group, NEMO_ACTION_PIN_FILE,  !nemo_file_get_pinning (file));
        set_action_visible (view->details->tv_action_group, NEMO_ACTION_UNPIN_FILE, nemo_file_get_pinning (file));
    } else {
        set_action_visible (view->details->tv_action_group, NEMO_ACTION_PIN_FILE, FALSE);
        set_action_visible (view->details->tv_action_group, NEMO_ACTION_UNPIN_FILE, FALSE);
    }
}

static gboolean
button_pressed_callback (GtkTreeView *treeview,
                         GdkEventButton *event,
                         FMTreeView *view)
{
    g_return_val_if_fail (FM_IS_TREE_VIEW (view), GDK_EVENT_PROPAGATE);

    if (event->button == 3) {
        popup_menu (view, event);
        return GDK_EVENT_STOP;
    } else if (event->button == 2 && event->type == GDK_BUTTON_PRESS) {
        NemoFile *file;
        GtkTreePath *path;
        gboolean using_browser;
        NemoWindowOpenFlags flags = 0;

        using_browser = g_settings_get_boolean (nemo_preferences,
                            NEMO_PREFERENCES_ALWAYS_USE_BROWSER);

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

        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean
key_press_callback (GtkWidget   *widget,
                    GdkEventKey *event,
                    gpointer     user_data)
{
    FMTreeView *view = FM_TREE_VIEW (user_data);

    if (event->keyval == GDK_KEY_slash ||
        event->keyval == GDK_KEY_KP_Divide ||
        event->keyval == GDK_KEY_asciitilde) {
        if (gtk_bindings_activate_event (G_OBJECT (view->details->window), event)) {
            return GDK_EVENT_STOP;
        }
    }

    return GDK_EVENT_PROPAGATE;
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
fm_tree_view_open_cb (GtkAction *action,
		      FMTreeView *view)
{
	fm_tree_view_activate_file (view, view->details->popup_file, 0);
}

static void
fm_tree_view_open_in_new_tab_cb (GtkAction *action,
				    FMTreeView *view)
{
	fm_tree_view_activate_file (view, view->details->popup_file, NEMO_WINDOW_OPEN_FLAG_NEW_TAB);
}

static void
fm_tree_view_open_in_new_window_cb (GtkAction *action,
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
fm_tree_view_create_folder_cb (GtkAction *action,
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
    GtkClipboard *clipboard;
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

    clipboard = nemo_clipboard_get (GTK_WIDGET (view));

    gtk_clipboard_set_with_data (clipboard,
                                 targets, n_targets,
                                 nemo_get_clipboard_callback, nemo_clear_clipboard_callback,
                                 NULL);
    gtk_clipboard_set_can_store (clipboard, NULL, 0);
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
fm_tree_view_cut_cb (GtkAction *action,
		     FMTreeView *view)
{
	copy_or_cut_files (view, TRUE);
}

static void
fm_tree_view_copy_cb (GtkAction *action,
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
fm_tree_view_paste_cb (GtkAction *action,
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
fm_tree_view_pin_unpin_cb (GtkAction *action,
                           FMTreeView *view)
{
    nemo_file_set_pinning (view->details->popup_file,
                           !nemo_file_get_pinning (view->details->popup_file));
}

static void
fm_tree_view_trash_cb (GtkAction *action,
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
fm_tree_view_delete_cb (GtkAction *action,
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
fm_tree_view_properties_cb (GtkAction *action,
			    FMTreeView *view)
{
	GList *list;
        
	list = g_list_prepend (NULL, nemo_file_ref (view->details->popup_file));

	nemo_properties_window_present (list, GTK_WIDGET (view->details->tree_widget), NULL);

        nemo_file_list_free (list);
}

static void
fm_tree_view_unmount_cb (GtkAction *action,
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
fm_tree_view_eject_cb (GtkAction *action,
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
run_action_callback (GtkAction *action, gpointer user_data)
{
    FMTreeView *view = FM_TREE_VIEW (user_data);
    g_return_if_fail (view->details->popup_file != NULL);

    g_autofree gchar *uri = NULL;

    uri = nemo_file_get_uri (view->details->popup_file);

    if (!uri) {
        return;
    }

    NemoFile *file = nemo_file_get_by_uri (uri);
    NemoFile *parent = nemo_file_get_parent (file);
    GList *selection = g_list_prepend (NULL, file);

    nemo_action_activate (NEMO_ACTION (action), selection, parent, GTK_WINDOW (view->details->window));

    nemo_file_list_free (selection);
    nemo_file_unref (parent);
}

#if GTK_CHECK_VERSION (3, 24, 8)
static void
moved_to_rect_cb (GdkWindow          *window,
                  const GdkRectangle *flipped_rect,
                  const GdkRectangle *final_rect,
                  gboolean            flipped_x,
                  gboolean            flipped_y,
                  GtkMenu            *menu)
{
    g_signal_emit_by_name (menu,
                           "popped-up",
                           0,
                           flipped_rect,
                           final_rect,
                           flipped_x,
                           flipped_y);

    // Don't let the emission run in gtkmenu.c
    g_signal_stop_emission_by_name (window, "moved-to-rect");
}

static void
popup_menu_realized (GtkWidget    *menu,
                     gpointer      user_data)
{
    GdkWindow *toplevel;

    toplevel = gtk_widget_get_window (gtk_widget_get_toplevel (menu));

    g_signal_handlers_disconnect_by_func (toplevel, moved_to_rect_cb, menu);

    g_signal_connect (toplevel, "moved-to-rect", G_CALLBACK (moved_to_rect_cb),
                      menu);
}
#endif

static void
popup_menu (FMTreeView     *view,
            GdkEventButton *event)
{
    update_menu_states (view, event);
    eel_pop_up_context_menu (GTK_MENU (view->details->popup_menu),
                             (GdkEvent *) event,
                             GTK_WIDGET (view->details->tree_widget));
}

/* Callback used for the GtkWidget::popup-menu signal of the shortcuts list */
static gboolean
popup_menu_cb (GtkAction  *widget,
               FMTreeView *view)
{
    popup_menu (view, NULL);
    return TRUE;
}

static void
reset_menu (FMTreeView *view)
{
    view->details->actions_need_update = TRUE;
    rebuild_menu (view);
}

static gboolean
actions_changed_idle_cb (gpointer user_data)
{
    FMTreeView *view = FM_TREE_VIEW (user_data);

    reset_menu (view);

    view->details->actions_changed_idle_id = 0;
    return G_SOURCE_REMOVE;
}

static void
actions_changed (gpointer user_data)
{
    FMTreeView *view = FM_TREE_VIEW (user_data);

    g_clear_handle_id (&view->details->actions_changed_idle_id, g_source_remove);
    view->details->actions_changed_idle_id = g_idle_add (actions_changed_idle_cb, view);
}

static void
add_action_to_ui (NemoActionManager    *manager,
                  GtkAction            *action,
                  GtkUIManagerItemType  type,
                  const gchar          *path,
                  const gchar          *accelerator,
                  gpointer              user_data)
{
    FMTreeView *view = FM_TREE_VIEW (user_data);

    const gchar *roots[] = {
        "/selection/TreeSidebarActionsPlaceholder",
        NULL
    };

    nemo_action_manager_add_action_ui (manager,
                                       view->details->ui_manager,
                                       action,
                                       path,
                                       accelerator,
                                       view->details->action_action_group,
                                       view->details->action_action_group_merge_id,
                                       roots,
                                       type,
                                       G_CALLBACK (run_action_callback),
                                       view);
}

static void
clear_ui (FMTreeView *view)
{
    nemo_ui_unmerge_ui (view->details->ui_manager,
                        &view->details->tv_action_group_merge_id,
                        &view->details->tv_action_group);

    nemo_ui_unmerge_ui (view->details->ui_manager,
                        &view->details->action_action_group_merge_id,
                        &view->details->action_action_group);
}

static const GtkActionEntry tree_sidebar_menu_entries[] = {
    { NEMO_ACTION_OPEN,                    "folder-open-symbolic",        N_("_Open"),                NULL, NULL, G_CALLBACK (fm_tree_view_open_cb)               },
    { NEMO_ACTION_OPEN_IN_NEW_TAB,         NULL,                          N_("Open in New _Tab"),     NULL, NULL, G_CALLBACK (fm_tree_view_open_in_new_tab_cb)    },
    { NEMO_ACTION_OPEN_ALTERNATE,          NULL,                          N_("Open in New _Window"),  NULL, NULL, G_CALLBACK (fm_tree_view_open_in_new_window_cb) },
    { NEMO_ACTION_NEW_FOLDER,              NULL,                          N_("Create New _Folder"),   NULL, NULL, G_CALLBACK (fm_tree_view_create_folder_cb)      },
    { NEMO_ACTION_CUT,                     "edit-cut-symbolic",           N_("Cu_t"),                 NULL, NULL, G_CALLBACK (fm_tree_view_cut_cb)                },
    { NEMO_ACTION_COPY,                    "edit-copy-symbolic",          N_("_Copy"),                NULL, NULL, G_CALLBACK (fm_tree_view_copy_cb)               },
    { NEMO_ACTION_PASTE,                   "edit-paste-symbolic",         N_("_Paste Into Folder"),   NULL, NULL, G_CALLBACK (fm_tree_view_paste_cb)              },
    { NEMO_ACTION_PIN_FILE,                "xapp-pin-symbolic",           N_("P_in"),                 NULL, NULL, G_CALLBACK (fm_tree_view_pin_unpin_cb)          },
    { NEMO_ACTION_UNPIN_FILE,              "xapp-unpin-symbolic",         N_("Unp_in"),               NULL, NULL, G_CALLBACK (fm_tree_view_pin_unpin_cb)          },
    { NEMO_ACTION_TRASH,                   "user-trash-full-symbolic",    N_("Mo_ve to Trash"),       NULL, NULL, G_CALLBACK (fm_tree_view_trash_cb)              },
    { NEMO_ACTION_DELETE,                  "edit-delete-symbolic",        N_("_Delete"),              NULL, NULL, G_CALLBACK (fm_tree_view_delete_cb)             },
    { NEMO_ACTION_UNMOUNT_VOLUME,          NULL,                          N_("_Unmount"),             NULL, NULL, G_CALLBACK (fm_tree_view_unmount_cb)            },
    { NEMO_ACTION_EJECT_VOLUME,            NULL,                          N_("_Eject"),               NULL, NULL, G_CALLBACK (fm_tree_view_eject_cb)              },
    { NEMO_ACTION_PROPERTIES,             "document-properties-symbolic", N_("_Properties"),          NULL, NULL, G_CALLBACK (fm_tree_view_properties_cb)         },
};

static void
rebuild_menu (FMTreeView *view)
{
    if (!view->details->actions_need_update) {
        return;
    }

    clear_ui (view);

    nemo_ui_prepare_merge_ui (view->details->ui_manager,
                              "NemoTreeviewSidebarFileActions",
                              &view->details->tv_action_group_merge_id,
                              &view->details->tv_action_group);

    nemo_ui_prepare_merge_ui (view->details->ui_manager,
                              "NemoTreeviewSidebarActionActions",
                              &view->details->action_action_group_merge_id,
                              &view->details->action_action_group);

    view->details->tv_action_group_merge_id =
            gtk_ui_manager_add_ui_from_resource (view->details->ui_manager, "/org/nemo/nemo-tree-sidebar-ui.xml", NULL);

    nemo_action_manager_iterate_actions (view->details->action_manager,
                                         (NemoActionManagerIterFunc) add_action_to_ui,
                                         view);

    gtk_action_group_add_actions (view->details->tv_action_group,
                                  tree_sidebar_menu_entries,
                                  G_N_ELEMENTS (tree_sidebar_menu_entries),
                                  view);

    if (view->details->popup_menu == NULL) {
        GtkWidget *menu = gtk_ui_manager_get_widget (view->details->ui_manager, "/selection");
        gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (GTK_WIDGET (view->details->window)));
        view->details->popup_menu = menu;
        g_signal_connect (view->details->popup_menu, "deactivate",
                          G_CALLBACK (popup_menu_deactivated),
                          view);

#if GTK_CHECK_VERSION (3, 24, 8)
        g_signal_connect (view->details->popup_menu, "realize",
                          G_CALLBACK (popup_menu_realized),
                          view->details);
        gtk_widget_realize (view->details->popup_menu);
#endif

        gtk_widget_show (menu);
    }

    view->details->actions_need_update = FALSE;
}


static gint
get_icon_scale_callback (FMTreeModel *model,
                         FMTreeView  *view)
{
   return gtk_widget_get_scale_factor (GTK_WIDGET (view->details->tree_widget));
}

static void
icon_data_func (GtkTreeViewColumn *tree_column,
                  GtkCellRenderer *cell,
                     GtkTreeModel *tree_model,
                      GtkTreeIter *iter,
                         gpointer  data)
{
    gboolean expanded;
    GIcon *icon;

    g_object_get (cell,
                  "is-expanded", &expanded,
                  NULL);

    gtk_tree_model_get (tree_model, iter,
                        expanded ? FM_TREE_MODEL_OPEN_ICON_COLUMN : FM_TREE_MODEL_CLOSED_ICON_COLUMN,
                        &icon,
                        -1);

    g_object_set (cell,
                  "gicon", icon,
                  NULL);
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
    gtk_tree_view_set_search_column (GTK_TREE_VIEW (view->details->tree_widget), FM_TREE_MODEL_DISPLAY_NAME_COLUMN);

	g_object_unref (view->details->sort_model);

    g_signal_connect (view->details->tree_widget, "popup-menu", G_CALLBACK (popup_menu_cb), view);

	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (view->details->tree_widget)),
				     "NemoSidebar");

	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (view->details->sort_model),
						 compare_rows, view, NULL);

	g_signal_connect_object (view->details->child_model, "row_loaded",
		                     G_CALLBACK (row_loaded_callback),
                             view, G_CONNECT_AFTER);
    g_signal_connect_object (view->details->child_model, "get-icon-scale",
                             G_CALLBACK (get_icon_scale_callback), view, 0);

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

    icon = g_themed_icon_new (NEMO_ICON_FAVORITES);
    fm_tree_model_add_root_uri (view->details->child_model, "favorites:///", _("Favorites"), icon, NULL);
    g_object_unref (icon);

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
		nemo_tree_view_drag_dest_new (view->details->tree_widget, FALSE);
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

	/* Create column */
	column = gtk_tree_view_column_new ();

	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, cell, FALSE);

    gtk_tree_view_column_set_cell_data_func (column, cell,
                                             (GtkTreeCellDataFunc) icon_data_func,
                                             NULL, NULL);

    g_object_set (cell,
                  "follow-state", TRUE,
                  NULL);

	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell, TRUE);
	gtk_tree_view_column_set_attributes (column, cell,
                                         "text", FM_TREE_MODEL_DISPLAY_NAME_COLUMN,
                                         "style", FM_TREE_MODEL_FONT_STYLE_COLUMN,
                                         "weight", FM_TREE_MODEL_TEXT_WEIGHT_COLUMN,
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

    g_signal_connect (G_OBJECT (view->details->tree_widget), 
              "key-press-event", G_CALLBACK (key_press_callback),
              view);

	slot = nemo_window_get_active_slot (view->details->window);
	location = nemo_window_slot_get_current_uri (slot);
	schedule_select_and_show_location (view, location);
	g_free (location);

    reset_menu (view);
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
sort_directories_first_changed_callback (gpointer callback_data)
{
    FMTreeView *view;
    gboolean preference_value;

    view = FM_TREE_VIEW (callback_data);

    preference_value = g_settings_get_boolean (nemo_preferences,
                                               NEMO_PREFERENCES_SORT_DIRECTORIES_FIRST);

    if (preference_value != view->details->sort_directories_first) {
        view->details->sort_directories_first = preference_value;
    }

    gtk_tree_model_sort_reset_default_sort_func (GTK_TREE_MODEL_SORT (view->details->sort_model));

    gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (view->details->sort_model),
                         compare_rows, view, NULL);
}

static void
sort_favorites_first_changed_callback (gpointer callback_data)
{
    FMTreeView *view;
    gboolean preference_value;

    view = FM_TREE_VIEW (callback_data);

    preference_value = g_settings_get_boolean (nemo_preferences,
                                               NEMO_PREFERENCES_SORT_FAVORITES_FIRST);

    if (preference_value != view->details->sort_favorites_first) {
        view->details->sort_favorites_first = preference_value;
    }

    gtk_tree_model_sort_reset_default_sort_func (GTK_TREE_MODEL_SORT (view->details->sort_model));

    gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (view->details->sort_model),
                         compare_rows, view, NULL);
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


    view->details->action_manager = nemo_action_manager_new ();

    view->details->actions_changed_id = g_signal_connect_swapped (view->details->action_manager,
                                                                         "changed",
                                                                         G_CALLBACK (actions_changed),
                                                                         view);
    view->details->ui_manager = gtk_ui_manager_new ();

	g_signal_connect_swapped (nemo_tree_sidebar_preferences,
				  "changed::" NEMO_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES,
				  G_CALLBACK (filtering_changed_callback), view);

    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SORT_DIRECTORIES_FIRST,
                  G_CALLBACK (sort_directories_first_changed_callback), view);
                  
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SORT_FAVORITES_FIRST,
                  G_CALLBACK (sort_favorites_first_changed_callback), view);

    view->details->sort_directories_first = g_settings_get_boolean (nemo_preferences,
                                                                    NEMO_PREFERENCES_SORT_DIRECTORIES_FIRST);
    view->details->sort_favorites_first = g_settings_get_boolean (nemo_preferences,
                                                                    NEMO_PREFERENCES_SORT_FAVORITES_FIRST);

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
	
    g_clear_handle_id (&view->details->actions_changed_idle_id, g_source_remove);

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

    if (view->details->hidden_files_changed_id != 0) {
        g_signal_handler_disconnect (view->details->window,
                                     view->details->hidden_files_changed_id);
        view->details->hidden_files_changed_id = 0;
    }

    if (view->details->actions_changed_id != 0) {
        g_signal_handler_disconnect (view->details->action_manager,
                                     view->details->actions_changed_id);
        view->details->actions_changed_id = 0;
    }

    g_clear_object (&view->details->action_manager);
    g_clear_object (&view->details->ui_manager);

	g_signal_handlers_disconnect_by_func (nemo_tree_sidebar_preferences,
					      G_CALLBACK(filtering_changed_callback),
					      view);

    g_signal_handlers_disconnect_by_func (nemo_tree_sidebar_preferences,
                          G_CALLBACK(sort_directories_first_changed_callback),
                          view);
    g_signal_handlers_disconnect_by_func (nemo_tree_sidebar_preferences,
                          G_CALLBACK(sort_favorites_first_changed_callback),
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
fm_tree_view_set_parent_window (FMTreeView *view,
                                NemoWindow *window)
{
	char *location;
	NemoWindowSlot *slot;

	view->details->window = window;

	slot = nemo_window_get_active_slot (window);

	g_signal_connect_object (window, "loading_uri",
				 G_CALLBACK (loading_uri_callback), view, 0);
	location = nemo_window_slot_get_current_uri (slot);
	loading_uri_callback (window, location, view);
	g_free (location);

	view->details->hidden_files_changed_id = 
                    g_signal_connect_object (window, "hidden-files-mode-changed",
                                             G_CALLBACK (hidden_files_mode_changed_callback), view, 0);

}

GtkWidget *
nemo_tree_sidebar_new (NemoWindow *window)
{
	FMTreeView *sidebar;
	
	sidebar = g_object_new (fm_tree_view_get_type (), NULL);
	fm_tree_view_set_parent_window (sidebar, window);

	return GTK_WIDGET (sidebar);
}

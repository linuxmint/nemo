/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-view.c
 *
 * Copyright (C) 1999, 2000  Free Software Foundation
 * Copyright (C) 2000, 2001  Eazel, Inc.
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
 * Authors: Ettore Perazzoli,
 *          John Sullivan <sullivan@eazel.com>,
 *          Darin Adler <darin@bentspoon.com>,
 *          Pavel Cisler <pavel@eazel.com>,
 *          David Emory Watson <dwatson@cs.ucr.edu>
 */

#include <config.h>

#include "nemo-view.h"

#include "nemo-actions.h"
#include "nemo-desktop-icon-view.h"
#include "nemo-error-reporting.h"
#include "nemo-list-view.h"
#include "nemo-mime-actions.h"
#include "nemo-previewer.h"
#include "nemo-properties-window.h"
#include "nemo-bookmark-list.h"
#include "nemo-window-pane.h"

#include <sys/stat.h>
#include <fcntl.h>

#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <math.h>

#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>

#include <libnemo-extension/nemo-menu-provider.h>
#include <libnemo-private/nemo-bookmark.h>
#include <libnemo-private/nemo-clipboard.h>
#include <libnemo-private/nemo-clipboard-monitor.h>
#include <libnemo-private/nemo-desktop-icon-file.h>
#include <libnemo-private/nemo-desktop-directory.h>
#include <libnemo-private/nemo-search-directory.h>
#include <libnemo-private/nemo-directory.h>
#include <libnemo-private/nemo-dnd.h>
#include <libnemo-private/nemo-file-attributes.h>
#include <libnemo-private/nemo-file-changes-queue.h>
#include <libnemo-private/nemo-file-dnd.h>
#include <libnemo-private/nemo-file-operations.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-file-private.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-link.h>
#include <libnemo-private/nemo-metadata.h>
#include <libnemo-private/nemo-recent.h>
#include <libnemo-private/nemo-module.h>
#include <libnemo-private/nemo-program-choosing.h>
#include <libnemo-private/nemo-trash-monitor.h>
#include <libnemo-private/nemo-ui-utilities.h>
#include <libnemo-private/nemo-signaller.h>
#include <libnemo-private/nemo-icon-names.h>
#include <libnemo-private/nemo-file-undo-manager.h>
#include <libnemo-private/nemo-action.h>
#include <libnemo-private/nemo-action-manager.h>
#include <libnemo-private/nemo-mime-application-chooser.h>

#define DEBUG_FLAG NEMO_DEBUG_DIRECTORY_VIEW
#include <libnemo-private/nemo-debug.h>

/* Minimum starting update inverval */
#define UPDATE_INTERVAL_MIN 200
/* Maximum update interval */
#define UPDATE_INTERVAL_MAX 2000
/* Amount of miliseconds the update interval is increased */
#define UPDATE_INTERVAL_INC 250
/* Interval at which the update interval is increased */
#define UPDATE_INTERVAL_TIMEOUT_INTERVAL 250
/* Milliseconds that have to pass without a change to reset the update interval */
#define UPDATE_INTERVAL_RESET 1000

#define SILENT_WINDOW_OPEN_LIMIT 5

#define DUPLICATE_HORIZONTAL_ICON_OFFSET 70
#define DUPLICATE_VERTICAL_ICON_OFFSET   30

#define MAX_QUEUED_UPDATES 500

#define NEMO_VIEW_MENU_PATH_APPLICATIONS_SUBMENU_PLACEHOLDER  "/MenuBar/File/Open Placeholder/Open With/Applications Placeholder"
#define NEMO_VIEW_MENU_PATH_APPLICATIONS_PLACEHOLDER    	  "/MenuBar/File/Open Placeholder/Applications Placeholder"
#define NEMO_VIEW_MENU_PATH_SCRIPTS_PLACEHOLDER               "/MenuBar/File/Open Placeholder/Scripts/Scripts Placeholder"
#define NEMO_VIEW_MENU_PATH_ACTIONS_PLACEHOLDER               "/MenuBar/File/Open Placeholder/ActionsPlaceholder"
#define NEMO_VIEW_MENU_PATH_EXTENSION_ACTIONS_PLACEHOLDER     "/MenuBar/Edit/Extension Actions"
#define NEMO_VIEW_MENU_PATH_NEW_DOCUMENTS_PLACEHOLDER  	  "/MenuBar/File/New Items Placeholder/New Documents/New Documents Placeholder"
#define NEMO_VIEW_MENU_PATH_OPEN				  "/MenuBar/File/Open Placeholder/Open"

#define NEMO_VIEW_POPUP_PATH_SELECTION			  "/selection"
#define NEMO_VIEW_POPUP_PATH_APPLICATIONS_SUBMENU_PLACEHOLDER "/selection/Open Placeholder/Open With/Applications Placeholder"
#define NEMO_VIEW_POPUP_PATH_APPLICATIONS_PLACEHOLDER    	  "/selection/Open Placeholder/Applications Placeholder"
#define NEMO_VIEW_POPUP_PATH_SCRIPTS_PLACEHOLDER    	  "/selection/Open Placeholder/Scripts/Scripts Placeholder"
#define NEMO_VIEW_POPUP_PATH_ACTIONS_PLACEHOLDER           "/selection/Open Placeholder/ActionsPlaceholder"
#define NEMO_VIEW_POPUP_PATH_EXTENSION_ACTIONS		  "/selection/Extension Actions"
#define NEMO_VIEW_POPUP_PATH_OPEN				  "/selection/Open Placeholder/Open"

#define NEMO_VIEW_POPUP_PATH_BACKGROUND			  "/background"
#define NEMO_VIEW_POPUP_PATH_BACKGROUND_SCRIPTS_PLACEHOLDER	  "/background/Before Zoom Items/New Object Items/Scripts/Scripts Placeholder"
#define NEMO_VIEW_POPUP_PATH_BACKGROUND_ACTIONS_PLACEHOLDER   "/background/Before Zoom Items/New Object Items/ActionsPlaceholder"
#define NEMO_VIEW_POPUP_PATH_BACKGROUND_NEW_DOCUMENTS_PLACEHOLDER "/background/Before Zoom Items/New Object Items/New Documents/New Documents Placeholder"

#define NEMO_VIEW_POPUP_PATH_LOCATION			  "/location"

#define NEMO_VIEW_POPUP_PATH_BOOKMARK_MOVETO_ENTRIES_PLACEHOLDER "/selection/File Actions/MoveToMenu/BookmarkMoveToPlaceHolder"
#define NEMO_VIEW_POPUP_PATH_BOOKMARK_COPYTO_ENTRIES_PLACEHOLDER "/selection/File Actions/CopyToMenu/BookmarkCopyToPlaceHolder"
#define NEMO_VIEW_MENU_PATH_BOOKMARK_MOVETO_ENTRIES_PLACEHOLDER "/MenuBar/Edit/File Items Placeholder/MoveToMenu/BookmarkMoveToPlaceHolder"
#define NEMO_VIEW_MENU_PATH_BOOKMARK_COPYTO_ENTRIES_PLACEHOLDER "/MenuBar/Edit/File Items Placeholder/CopyToMenu/BookmarkCopyToPlaceHolder"

#define NEMO_VIEW_POPUP_PATH_PLACES_MOVETO_ENTRIES_PLACEHOLDER "/selection/File Actions/MoveToMenu/PlacesMoveToPlaceHolder"
#define NEMO_VIEW_POPUP_PATH_PLACES_COPYTO_ENTRIES_PLACEHOLDER "/selection/File Actions/CopyToMenu/PlacesCopyToPlaceHolder"
#define NEMO_VIEW_MENU_PATH_PLACES_MOVETO_ENTRIES_PLACEHOLDER "/MenuBar/Edit/File Items Placeholder/MoveToMenu/PlacesMoveToPlaceHolder"
#define NEMO_VIEW_MENU_PATH_PLACES_COPYTO_ENTRIES_PLACEHOLDER "/MenuBar/Edit/File Items Placeholder/CopyToMenu/PlacesCopyToPlaceHolder"

#define MAX_MENU_LEVELS 5
#define TEMPLATE_LIMIT 30

enum {
	ADD_FILE,
	BEGIN_FILE_CHANGES,
	BEGIN_LOADING,
	CLEAR,
	END_FILE_CHANGES,
	END_LOADING,
	FILE_CHANGED,
	LOAD_ERROR,
	MOVE_COPY_ITEMS,
	REMOVE_FILE,
	ZOOM_LEVEL_CHANGED,
	SELECTION_CHANGED,
	TRASH,
	DELETE,
	LAST_SIGNAL
};

enum {
	PROP_WINDOW_SLOT = 1,
	PROP_SUPPORTS_ZOOMING,
	NUM_PROPERTIES
};

static guint signals[LAST_SIGNAL];
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GdkAtom copied_files_atom;

static char *scripts_directory_uri = NULL;
static int scripts_directory_uri_length;

struct NemoViewDetails
{
	NemoWindow *window;
	NemoWindowSlot *slot;
	NemoDirectory *model;
	NemoFile *directory_as_file;
	NemoFile *location_popup_directory_as_file;
	NemoBookmarkList *bookmarks;
	GdkEventButton *location_popup_event;
	GtkActionGroup *dir_action_group;
	guint dir_merge_id;

	gboolean supports_zooming;

	GList *scripts_directory_list;
	GtkActionGroup *scripts_action_group;
	guint scripts_merge_id;

    GtkActionGroup *actions_action_group;
    guint actions_merge_id;
    guint action_manager_changed_id;
    NemoActionManager *action_manager;

	GList *templates_directory_list;
	GtkActionGroup *templates_action_group;
	guint templates_merge_id;

	GtkActionGroup *extensions_menu_action_group;
	guint extensions_menu_merge_id;
	
	guint display_selection_idle_id;
	guint update_menus_timeout_id;
	guint update_status_idle_id;
	guint reveal_selection_idle_id;

	guint display_pending_source_id;
	guint changes_timeout_id;

	guint update_interval;
 	guint64 last_queued;
	
	guint files_added_handler_id;
	guint files_changed_handler_id;
	guint load_error_handler_id;
	guint done_loading_handler_id;
	guint file_changed_handler_id;

	guint delayed_rename_file_id;

	GList *new_added_files;
	GList *new_changed_files;

	GHashTable *non_ready_files;

	GList *old_added_files;
	GList *old_changed_files;

	GList *pending_selection;

	/* whether we are in the active slot */
	gboolean active;

	/* loading indicates whether this view has begun loading a directory.
	 * This flag should need not be set inside subclasses. NemoView automatically
	 * sets 'loading' to TRUE before it begins loading a directory's contents and to FALSE
	 * after it finishes loading the directory and its view.
	 */
	gboolean loading;
	gboolean menu_states_untrustworthy;
	gboolean scripts_invalid;
	gboolean templates_invalid;
    gboolean actions_invalid;
	gboolean reported_load_error;

	/* flag to indicate that no file updates should be dispatched to subclasses.
	 * This is a workaround for bug #87701 that prevents the list view from
	 * losing focus when the underlying GtkTreeView is updated.
	 */
	gboolean updates_frozen;
	guint	 updates_queued;
	gboolean needs_reload;

	gboolean is_renaming;

	gboolean sort_directories_first;

	gboolean show_foreign_files;
	gboolean show_hidden_files;
	gboolean ignore_hidden_file_preferences;

	gboolean batching_selection_level;
	gboolean selection_changed_while_batched;

	gboolean selection_was_removed;

	gboolean metadata_for_directory_as_file_pending;
	gboolean metadata_for_files_in_directory_pending;

	gboolean selection_change_is_due_to_shell;
	gboolean send_selection_change_to_shell;

	GtkActionGroup *open_with_action_group;
	guint open_with_merge_id;

	GList *subdirectory_list;

    guint copy_move_merge_ids[4];
    GtkActionGroup *copy_move_action_groups[4];
    guint bookmarks_changed_id;

	GdkPoint context_menu_position;

    gboolean showing_bookmarks_in_to_menus;
    gboolean showing_places_in_to_menus;

    GVolumeMonitor *volume_monitor;
};

typedef struct {
	NemoFile *file;
	NemoDirectory *directory;
} FileAndDirectory;

/* forward declarations */

static gboolean display_selection_info_idle_callback           (gpointer              data);
static void     nemo_view_duplicate_selection              (NemoView      *view,
							        GList                *files,
							        GArray               *item_locations);
static void     nemo_view_create_links_for_files           (NemoView      *view,
							        GList                *files,
							        GArray               *item_locations);
static void     trash_or_delete_files                          (GtkWindow            *parent_window,
								const GList          *files,
								gboolean              delete_if_all_already_in_trash,
								NemoView      *view);
static void     load_directory                                 (NemoView      *view,
								NemoDirectory    *directory);
static void     nemo_view_merge_menus                      (NemoView      *view);
static void     nemo_view_unmerge_menus                    (NemoView      *view);
static void     nemo_view_init_show_hidden_files           (NemoView      *view);
static void     clipboard_changed_callback                     (NemoClipboardMonitor *monitor,
								NemoView      *view);
static void     open_one_in_new_window                         (gpointer              data,
								gpointer              callback_data);
static void     schedule_update_menus                          (NemoView      *view);
static void     remove_update_menus_timeout_callback           (NemoView      *view);
static void     schedule_update_status                          (NemoView      *view);
static void     remove_update_status_idle_callback             (NemoView *view); 
static void     reset_update_interval                          (NemoView      *view);
static void     schedule_idle_display_of_pending_files         (NemoView      *view);
static void     unschedule_display_of_pending_files            (NemoView      *view);
static void     disconnect_model_handlers                      (NemoView      *view);
static void     metadata_for_directory_as_file_ready_callback  (NemoFile         *file,
								gpointer              callback_data);
static void     metadata_for_files_in_directory_ready_callback (NemoDirectory    *directory,
								GList                *files,
								gpointer              callback_data);
static void     nemo_view_trash_state_changed_callback     (NemoTrashMonitor *trash,
							        gboolean              state,
							        gpointer              callback_data);
static void     nemo_view_select_file                      (NemoView      *view,
							        NemoFile         *file);

static void     update_templates_directory                     (NemoView *view);
static void     user_dirs_changed                              (NemoView *view);

static gboolean file_list_all_are_folders                      (GList *file_list);

static void unschedule_pop_up_location_context_menu (NemoView *view);
static void disconnect_bookmark_signals (NemoView *view);

G_DEFINE_TYPE (NemoView, nemo_view, GTK_TYPE_SCROLLED_WINDOW);
#define parent_class nemo_view_parent_class

/* virtual methods (public and non-public) */

/**
 * nemo_view_merge_menus:
 * 
 * Add this view's menus to the window's menu bar.
 * @view: NemoView in question.
 */
static void
nemo_view_merge_menus (NemoView *view)
{
	g_return_if_fail (NEMO_IS_VIEW (view));

	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->merge_menus (view);
}

static void
nemo_view_unmerge_menus (NemoView *view)
{
	g_return_if_fail (NEMO_IS_VIEW (view));

	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->unmerge_menus (view);}

static char *
real_get_backing_uri (NemoView *view)
{
	NemoDirectory *directory;
	char *uri;
       
	g_return_val_if_fail (NEMO_IS_VIEW (view), NULL);

	if (view->details->model == NULL) {
		return NULL;
	}
       
	directory = view->details->model;
       
	if (NEMO_IS_DESKTOP_DIRECTORY (directory)) {
		directory = nemo_desktop_directory_get_real_directory (NEMO_DESKTOP_DIRECTORY (directory));
	} else {
		nemo_directory_ref (directory);
	}
       
	uri = nemo_directory_get_uri (directory);

	nemo_directory_unref (directory);

	return uri;
}

/**
 *
 * nemo_view_get_backing_uri:
 *
 * Returns the URI for the target location of new directory, new file, new
 * link and paste operations.
 */

char *
nemo_view_get_backing_uri (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), NULL);

	return NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_backing_uri (view);
}

/**
 * nemo_view_select_all:
 *
 * select all the items in the view
 * 
 **/
static void
nemo_view_select_all (NemoView *view)
{
	g_return_if_fail (NEMO_IS_VIEW (view));

	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->select_all (view);
}

static void
nemo_view_call_set_selection (NemoView *view, GList *selection)
{
	g_return_if_fail (NEMO_IS_VIEW (view));

	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->set_selection (view, selection);
}

static GList *
nemo_view_get_selection_for_file_transfer (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), NULL);

	return NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_selection_for_file_transfer (view);
}

/**
 * nemo_view_get_selected_icon_locations:
 *
 * return an array of locations of selected icons if available
 * Return value: GArray of GdkPoints
 * 
 **/
static GArray *
nemo_view_get_selected_icon_locations (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), NULL);

	return NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_selected_icon_locations (view);
}

static void
nemo_view_invert_selection (NemoView *view)
{
	g_return_if_fail (NEMO_IS_VIEW (view));

	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->invert_selection (view);
}

/**
 * nemo_view_reveal_selection:
 *
 * Scroll as necessary to reveal the selected items.
 **/
static void
nemo_view_reveal_selection (NemoView *view)
{
	g_return_if_fail (NEMO_IS_VIEW (view));

	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->reveal_selection (view);
}

/**
 * nemo_view_reset_to_defaults:
 *
 * set sorting order, zoom level, etc. to match defaults
 * 
 **/
static void
nemo_view_reset_to_defaults (NemoView *view)
{
    g_return_if_fail (NEMO_IS_VIEW (view));

    NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->reset_to_defaults (view);

    gboolean show_hidden = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_HIDDEN_FILES);

    if (show_hidden) {
        nemo_window_set_hidden_files_mode (view->details->window, NEMO_WINDOW_SHOW_HIDDEN_FILES_ENABLE);
    } else {
        nemo_window_set_hidden_files_mode (view->details->window, NEMO_WINDOW_SHOW_HIDDEN_FILES_DISABLE);
    }
}

static gboolean
nemo_view_using_manual_layout (NemoView  *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), FALSE);

	return 	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->using_manual_layout (view);
}

static guint
nemo_view_get_item_count (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), 0);

	return 	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_item_count (view);
}

/**
 * nemo_view_can_rename_file
 *
 * Determine whether a file can be renamed.
 * @file: A NemoFile
 * 
 * Return value: TRUE if @file can be renamed, FALSE otherwise.
 * 
 **/
static gboolean
nemo_view_can_rename_file (NemoView *view, NemoFile *file)
{
	return 	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->can_rename_file (view, file);
}

static gboolean
nemo_view_is_read_only (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), FALSE);

	return 	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->is_read_only (view);
}

static gboolean
showing_trash_directory (NemoView *view)
{
	NemoFile *file;

	file = nemo_view_get_directory_as_file (view);
	if (file != NULL) {
		return nemo_file_is_in_trash (file);
	}
	return FALSE;
}

static gboolean
showing_recent_directory (NemoView *view)
{
	NemoFile *file;

	file = nemo_view_get_directory_as_file (view);
	if (file != NULL) {
		return nemo_file_is_in_recent (file);
	}
	return FALSE;
}

static gboolean
nemo_view_supports_creating_files (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), FALSE);

	return !nemo_view_is_read_only (view) 
		&& !showing_trash_directory (view)
		&& !showing_recent_directory (view);
}

static gboolean
nemo_view_is_empty (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), FALSE);

	return 	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->is_empty (view);
}

/**
 * nemo_view_bump_zoom_level:
 *
 * bump the current zoom level by invoking the relevant subclass through the slot
 * 
 **/
void
nemo_view_bump_zoom_level (NemoView *view,
			       int zoom_increment)
{
	g_return_if_fail (NEMO_IS_VIEW (view));

	if (!nemo_view_supports_zooming (view)) {
		return;
	}

	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->bump_zoom_level (view, zoom_increment);
}

/**
 * nemo_view_zoom_to_level:
 *
 * Set the current zoom level by invoking the relevant subclass through the slot
 * 
 **/
void
nemo_view_zoom_to_level (NemoView *view,
			     NemoZoomLevel zoom_level)
{
	g_return_if_fail (NEMO_IS_VIEW (view));

	if (!nemo_view_supports_zooming (view)) {
		return;
	}

	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->zoom_to_level (view, zoom_level);
}

NemoZoomLevel
nemo_view_get_zoom_level (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), NEMO_ZOOM_LEVEL_STANDARD);

	if (!nemo_view_supports_zooming (view)) {
		return NEMO_ZOOM_LEVEL_STANDARD;
	}

	return NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_zoom_level (view);
}

/**
 * nemo_view_can_zoom_in:
 *
 * Determine whether the view can be zoomed any closer.
 * @view: The zoomable NemoView.
 * 
 * Return value: TRUE if @view can be zoomed any closer, FALSE otherwise.
 * 
 **/
gboolean
nemo_view_can_zoom_in (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), FALSE);

	if (!nemo_view_supports_zooming (view)) {
		return FALSE;
	}

	return NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->can_zoom_in (view);
}

/**
 * nemo_view_can_zoom_out:
 *
 * Determine whether the view can be zoomed any further away.
 * @view: The zoomable NemoView.
 * 
 * Return value: TRUE if @view can be zoomed any further away, FALSE otherwise.
 * 
 **/
gboolean
nemo_view_can_zoom_out (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), FALSE);

	if (!nemo_view_supports_zooming (view)) {
		return FALSE;
	}

	return NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->can_zoom_out (view);
}

gboolean
nemo_view_supports_zooming (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), FALSE);

	return view->details->supports_zooming;
}

/**
 * nemo_view_restore_default_zoom_level:
 *
 * restore to the default zoom level by invoking the relevant subclass through the slot
 * 
 **/
void
nemo_view_restore_default_zoom_level (NemoView *view)
{
	g_return_if_fail (NEMO_IS_VIEW (view));

	if (!nemo_view_supports_zooming (view)) {
		return;
	}

	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->restore_default_zoom_level (view);
}

/*
static NemoZoomLevel
nemo_view_get_default_zoom_level (NemoView *view)
{
    g_return_if_fail (NEMO_IS_VIEW (view));

    if (!nemo_view_supports_zooming (view)) {
        return -1;
    }

    NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_default_zoom_level (view);
}
*/

const char *
nemo_view_get_view_id (NemoView *view)
{
	return NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_view_id (view);
}

char *
nemo_view_get_first_visible_file (NemoView *view)
{
	return NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_first_visible_file (view);
}

void
nemo_view_scroll_to_file (NemoView *view,
			      const char *uri)
{
	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->scroll_to_file (view, uri);
}

/**
 * nemo_view_get_selection:
 *
 * Get a list of NemoFile pointers that represents the
 * currently-selected items in this view. Subclasses must override
 * the signal handler for the 'get_selection' signal. Callers are
 * responsible for g_free-ing the list (but not its data).
 * @view: NemoView whose selected items are of interest.
 * 
 * Return value: GList of NemoFile pointers representing the selection.
 * 
 **/
GList *
nemo_view_get_selection (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), NULL);

	return NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->get_selection (view);
}

/**
 * nemo_view_update_menus:
 * 
 * Update the sensitivity and wording of dynamic menu items.
 * @view: NemoView in question.
 */
void
nemo_view_update_menus (NemoView *view)
{
	g_return_if_fail (NEMO_IS_VIEW (view));

	if (!view->details->active) {
		return;
	}

	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->update_menus (view);

	view->details->menu_states_untrustworthy = FALSE;
}

typedef struct {
	GAppInfo *application;
	GList *files;
	NemoView *directory_view;
} ApplicationLaunchParameters;

typedef struct {
	NemoFile *file;
	NemoView *directory_view;
} ScriptLaunchParameters;

typedef struct {
	NemoFile *file;
	NemoView *directory_view;
} CreateTemplateParameters;

typedef struct {
	NemoView *view;
	char *dest_uri;
} BookmarkCallbackData;


static BookmarkCallbackData *
bookmark_callback_data_new (NemoView *view,
							   gchar *uri)
{
    BookmarkCallbackData *result;

    result = g_new0 (BookmarkCallbackData, 1);
    result->view = view;
    result->dest_uri = g_strdup(uri);
    return result;
}

static void
bookmark_callback_data_free (BookmarkCallbackData *data)
{
    g_free ((char *)data->dest_uri);
    g_free (data);
}

static ApplicationLaunchParameters *
application_launch_parameters_new (GAppInfo *application,
			      	   GList *files,
			           NemoView *directory_view)
{
	ApplicationLaunchParameters *result;

	result = g_new0 (ApplicationLaunchParameters, 1);
	result->application = g_object_ref (application);
	result->files = nemo_file_list_copy (files);

	if (directory_view != NULL) {
		g_object_ref (directory_view);
		result->directory_view = directory_view;
	}

	return result;
}

static void
application_launch_parameters_free (ApplicationLaunchParameters *parameters)
{
	g_object_unref (parameters->application);
	nemo_file_list_free (parameters->files);

	if (parameters->directory_view != NULL) {
		g_object_unref (parameters->directory_view);
	}

	g_free (parameters);
}			      

static GList *
file_and_directory_list_to_files (GList *fad_list)
{
	GList *res, *l;
	FileAndDirectory *fad;

	res = NULL;
	for (l = fad_list; l != NULL; l = l->next) {
		fad = l->data;
		res = g_list_prepend (res, nemo_file_ref (fad->file));
	}
	return g_list_reverse (res);
}


static GList *
file_and_directory_list_from_files (NemoDirectory *directory, GList *files)
{
	GList *res, *l;
	FileAndDirectory *fad;

	res = NULL;
	for (l = files; l != NULL; l = l->next) {
		fad = g_new0 (FileAndDirectory, 1);
		fad->directory = nemo_directory_ref (directory);
		fad->file = nemo_file_ref (l->data);
		res = g_list_prepend (res, fad);
	}
	return g_list_reverse (res);
}

static void
file_and_directory_free (FileAndDirectory *fad)
{
	nemo_directory_unref (fad->directory);
	nemo_file_unref (fad->file);
	g_free (fad);
}


static void
file_and_directory_list_free (GList *list)
{
	GList *l;

	for (l = list; l != NULL; l = l->next) {
		file_and_directory_free (l->data);
	}

	g_list_free (list);
}

static gboolean
file_and_directory_equal (gconstpointer  v1,
			  gconstpointer  v2)
{
	const FileAndDirectory *fad1, *fad2;
	fad1 = v1;
	fad2 = v2;

	return (fad1->file == fad2->file &&
		fad1->directory == fad2->directory);
}

static guint
file_and_directory_hash  (gconstpointer  v)
{
	const FileAndDirectory *fad;

	fad = v;
	return GPOINTER_TO_UINT (fad->file) ^ GPOINTER_TO_UINT (fad->directory);
}




static ScriptLaunchParameters *
script_launch_parameters_new (NemoFile *file,
			      NemoView *directory_view)
{
	ScriptLaunchParameters *result;

	result = g_new0 (ScriptLaunchParameters, 1);
	g_object_ref (directory_view);
	result->directory_view = directory_view;
	nemo_file_ref (file);
	result->file = file;

	return result;
}

static void
script_launch_parameters_free (ScriptLaunchParameters *parameters)
{
	g_object_unref (parameters->directory_view);
	nemo_file_unref (parameters->file);
	g_free (parameters);
}			      

static CreateTemplateParameters *
create_template_parameters_new (NemoFile *file,
				NemoView *directory_view)
{
	CreateTemplateParameters *result;

	result = g_new0 (CreateTemplateParameters, 1);
	g_object_ref (directory_view);
	result->directory_view = directory_view;
	nemo_file_ref (file);
	result->file = file;

	return result;
}

static void
create_templates_parameters_free (CreateTemplateParameters *parameters)
{
	g_object_unref (parameters->directory_view);
	nemo_file_unref (parameters->file);
	g_free (parameters);
}			      

NemoWindow *
nemo_view_get_nemo_window (NemoView  *view)
{
	g_assert (view->details->window != NULL);

	return view->details->window;
}

NemoWindowSlot *
nemo_view_get_nemo_window_slot (NemoView  *view)
{
	g_assert (view->details->slot != NULL);

	return view->details->slot;
}

/* Returns the GtkWindow that this directory view occupies, or NULL
 * if at the moment this directory view is not in a GtkWindow or the
 * GtkWindow cannot be determined. Primarily used for parenting dialogs.
 */
static GtkWindow *
nemo_view_get_containing_window (NemoView *view)
{
	GtkWidget *window;

	g_assert (NEMO_IS_VIEW (view));
	
	window = gtk_widget_get_ancestor (GTK_WIDGET (view), GTK_TYPE_WINDOW);
	if (window == NULL) {
		return NULL;
	}

	return GTK_WINDOW (window);
}

static gboolean
nemo_view_confirm_multiple (GtkWindow *parent_window,
				int count,
				gboolean tabs)
{
	GtkDialog *dialog;
	char *prompt;
	char *detail;
	int response;

	if (count <= SILENT_WINDOW_OPEN_LIMIT) {
		return TRUE;
	}

	prompt = _("Are you sure you want to open all files?");
	if (tabs) {
		detail = g_strdup_printf (ngettext("This will open %'d separate tab.",
						   "This will open %'d separate tabs.", count), count);
	} else {
		detail = g_strdup_printf (ngettext("This will open %'d separate window.",
						   "This will open %'d separate windows.", count), count);
	}
	dialog = eel_show_yes_no_dialog (prompt, detail, 
					 GTK_STOCK_OK, GTK_STOCK_CANCEL,
					 parent_window);
	g_free (detail);

	response = gtk_dialog_run (dialog);
	gtk_widget_destroy (GTK_WIDGET (dialog));

	return response == GTK_RESPONSE_YES;
}

static gboolean
selection_contains_one_item_in_menu_callback (NemoView *view, GList *selection)
{
	if (g_list_length (selection) == 1) {
		return TRUE;
	}

	/* If we've requested a menu update that hasn't yet occurred, then
	 * the mismatch here doesn't surprise us, and we won't complain.
	 * Otherwise, we will complain.
	 */
	if (!view->details->menu_states_untrustworthy) {
		g_warning ("Expected one selected item, found %'d. No action will be performed.", 	
			   g_list_length (selection));
	}

	return FALSE;
}

static gboolean
selection_not_empty_in_menu_callback (NemoView *view, GList *selection)
{
	if (selection != NULL) {
		return TRUE;
	}

	/* If we've requested a menu update that hasn't yet occurred, then
	 * the mismatch here doesn't surprise us, and we won't complain.
	 * Otherwise, we will complain.
	 */
	if (!view->details->menu_states_untrustworthy) {
		g_warning ("Empty selection found when selection was expected. No action will be performed.");
	}

	return FALSE;
}

static char *
get_view_directory (NemoView *view)
{
	char *uri, *path;
	GFile *f;
	
	uri = nemo_directory_get_uri (view->details->model);
	if (eel_uri_is_desktop (uri)) {
		g_free (uri);
		uri = nemo_get_desktop_directory_uri ();
		
	}
	f = g_file_new_for_uri (uri);
	path = g_file_get_path (f);
	g_object_unref (f);
	g_free (uri);
	
	return path;
}

void
nemo_view_preview_files (NemoView *view,
			     GList *files,
			     GArray *locations)
{
	NemoPreviewer *previewer;
	gchar *uri;
	guint xid;
	GtkWidget *toplevel;

	previewer = nemo_previewer_get_singleton ();
	uri = nemo_file_get_uri (files->data);
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));

	xid = gdk_x11_window_get_xid (gtk_widget_get_window (toplevel));
	nemo_previewer_call_show_file (previewer, uri, xid, TRUE);

	g_free (uri);
}

void
nemo_view_activate_files (NemoView *view,
			      GList *files,
			      NemoWindowOpenFlags flags,
			      gboolean confirm_multiple)
{
	char *path;

	path = get_view_directory (view);
	nemo_mime_activate_files (nemo_view_get_containing_window (view),
				      view->details->slot,
				      files,
				      path,
				      flags,
				      confirm_multiple);

	g_free (path);
}

static void
nemo_view_activate_file (NemoView *view,
			     NemoFile *file,
			     NemoWindowOpenFlags flags)
{
	char *path;

	path = get_view_directory (view);
	nemo_mime_activate_file (nemo_view_get_containing_window (view),
				     view->details->slot,
				     file,
				     path,
				     flags);

	g_free (path);
}

static void
action_open_callback (GtkAction *action,
		      gpointer callback_data)
{
	GList *selection;
	NemoView *view;

	view = NEMO_VIEW (callback_data);

	selection = nemo_view_get_selection (view);
	nemo_view_activate_files (view,
				      selection,
				      0,
				      TRUE);
	nemo_file_list_free (selection);
}

static void
action_open_close_parent_callback (GtkAction *action,
				   gpointer callback_data)
{
	GList *selection;
	NemoView *view;

	view = NEMO_VIEW (callback_data);

	selection = nemo_view_get_selection (view);
	nemo_view_activate_files (view,
				      selection,
				      NEMO_WINDOW_OPEN_FLAG_CLOSE_BEHIND,
				      TRUE);
	nemo_file_list_free (selection);
}


static void
action_open_alternate_callback (GtkAction *action,
				gpointer callback_data)
{
	NemoView *view;
	GList *selection;
	GtkWindow *window;

	view = NEMO_VIEW (callback_data);
	selection = nemo_view_get_selection (view);

	window = nemo_view_get_containing_window (view);

	if (nemo_view_confirm_multiple (window, g_list_length (selection), FALSE)) {
		g_list_foreach (selection, open_one_in_new_window, view);
	}

	nemo_file_list_free (selection);
}

static void
action_open_new_tab_callback (GtkAction *action,
			      gpointer callback_data)
{
	NemoView *view;
	GList *selection;
	GtkWindow *window;

	view = NEMO_VIEW (callback_data);
	selection = nemo_view_get_selection (view);

	window = nemo_view_get_containing_window (view);

	if (nemo_view_confirm_multiple (window, g_list_length (selection), TRUE)) {
		nemo_view_activate_files (view,
					      selection,
					      NEMO_WINDOW_OPEN_FLAG_NEW_TAB,
					      FALSE);
	}

	nemo_file_list_free (selection);
}

static void
app_chooser_dialog_response_cb (GtkDialog *dialog,
				gint response_id,
				gpointer user_data)
{
	GtkWindow *parent_window;
	NemoFile *file;
	GAppInfo *info;
	GList files;

	parent_window = user_data;

	if (response_id != GTK_RESPONSE_OK) {
		gtk_widget_destroy (GTK_WIDGET (dialog));
		return;
	}

    GtkWidget *content = gtk_dialog_get_content_area (dialog);
    GList *children = gtk_container_get_children (GTK_CONTAINER (content));

    NemoMimeApplicationChooser *chooser = children->data;

    g_list_free (children);

	info = nemo_mime_application_chooser_get_info (chooser);
	file = nemo_file_get_by_uri (nemo_mime_application_chooser_get_uri (chooser));

	g_signal_emit_by_name (nemo_signaller_get_current (), "mime_data_changed");

	files.next = NULL;
	files.prev = NULL;
	files.data = file;
	nemo_launch_application (info, &files, parent_window);

	gtk_widget_destroy (GTK_WIDGET (dialog));
	g_object_unref (info);
}

static void
choose_program (NemoView *view,
		NemoFile *file)
{
	GtkWidget *dialog;

    char *mime_type;
    char *uri = NULL;
    GList *uris = NULL;

	g_assert (NEMO_IS_VIEW (view));
	g_assert (NEMO_IS_FILE (file));

    mime_type = nemo_file_get_mime_type (file);
    uri = nemo_file_get_uri (file);

    dialog = gtk_dialog_new_with_buttons (_("Open with"),
                          nemo_view_get_containing_window (view),
                          GTK_DIALOG_DESTROY_WITH_PARENT,
                          GTK_STOCK_CANCEL,
                          GTK_RESPONSE_CANCEL,
                          GTK_STOCK_OK,
                          GTK_RESPONSE_OK,
                          NULL);


    GtkWidget *chooser = nemo_mime_application_chooser_new (uri, uris, mime_type);

    GtkWidget *content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    gtk_box_pack_start (GTK_BOX (content), chooser, TRUE, TRUE, 0);

    gtk_widget_show_all (dialog);

	g_signal_connect_object (dialog, "response", 
				 G_CALLBACK (app_chooser_dialog_response_cb),
				 nemo_view_get_containing_window (view), 0);
}

static void
open_with_other_program (NemoView *view)
{
        GList *selection;

	g_assert (NEMO_IS_VIEW (view));

       	selection = nemo_view_get_selection (view);

	if (selection_contains_one_item_in_menu_callback (view, selection)) {
		choose_program (view, NEMO_FILE (selection->data));
	}

	nemo_file_list_free (selection);
}

static void
action_other_application_callback (GtkAction *action,
				   gpointer callback_data)
{
	g_assert (NEMO_IS_VIEW (callback_data));

	open_with_other_program (NEMO_VIEW (callback_data));
}

static void
trash_or_delete_selected_files (NemoView *view)
{
        GList *selection;

	/* This might be rapidly called multiple times for the same selection
	 * when using keybindings. So we remember if the current selection
	 * was already removed (but the view doesn't know about it yet).
	 */
	if (!view->details->selection_was_removed) {
		selection = nemo_view_get_selection_for_file_transfer (view);
		trash_or_delete_files (nemo_view_get_containing_window (view),
				       selection, TRUE,
				       view);
		nemo_file_list_free (selection);
		view->details->selection_was_removed = TRUE;
	}
}

static gboolean
real_trash (NemoView *view)
{
	GtkAction *action;

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_TRASH);
	if (gtk_action_get_sensitive (action) &&
	    gtk_action_get_visible (action)) {
		trash_or_delete_selected_files (view);
		return TRUE;
	}
	return FALSE;
}

static void
action_trash_callback (GtkAction *action,
		       gpointer callback_data)
{
        trash_or_delete_selected_files (NEMO_VIEW (callback_data));
}

static void
delete_selected_files (NemoView *view)
{
        GList *selection;
	GList *node;
	GList *locations;

	selection = nemo_view_get_selection_for_file_transfer (view);
	if (selection == NULL) {
		return;
	}

	locations = NULL;
	for (node = selection; node != NULL; node = node->next) {
		locations = g_list_prepend (locations,
					    nemo_file_get_location ((NemoFile *) node->data));
	}
	locations = g_list_reverse (locations);

	nemo_file_operations_delete (locations, nemo_view_get_containing_window (view), NULL, NULL);

	g_list_free_full (locations, g_object_unref);
        nemo_file_list_free (selection);
}

static void
action_delete_callback (GtkAction *action,
			gpointer callback_data)
{
        delete_selected_files (NEMO_VIEW (callback_data));
}

static void
action_restore_from_trash_callback (GtkAction *action,
				    gpointer callback_data)
{
	NemoView *view;
	GList *selection;

	view = NEMO_VIEW (callback_data);

	selection = nemo_view_get_selection_for_file_transfer (view);
	nemo_restore_files_from_trash (selection,
					   nemo_view_get_containing_window (view));

	nemo_file_list_free (selection);

}

static gboolean
real_delete (NemoView *view)
{
	GtkAction *action;

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_DELETE);
	if (gtk_action_get_sensitive (action) &&
	    gtk_action_get_visible (action)) {
		delete_selected_files (view);
		return TRUE;
	}
	return FALSE;
}

static void
action_duplicate_callback (GtkAction *action,
			   gpointer callback_data)
{
        NemoView *view;
        GList *selection;
        GArray *selected_item_locations;
 
        view = NEMO_VIEW (callback_data);
	selection = nemo_view_get_selection_for_file_transfer (view);
	if (selection_not_empty_in_menu_callback (view, selection)) {
		/* FIXME bugzilla.gnome.org 45061:
		 * should change things here so that we use a get_icon_locations (view, selection).
		 * Not a problem in this case but in other places the selection may change by
		 * the time we go and retrieve the icon positions, relying on the selection
		 * staying intact to ensure the right sequence and count of positions is fragile.
		 */
		selected_item_locations = nemo_view_get_selected_icon_locations (view);
	        nemo_view_duplicate_selection (view, selection, selected_item_locations);
	        g_array_free (selected_item_locations, TRUE);
	}

        nemo_file_list_free (selection);
}

static void
action_create_link_callback (GtkAction *action,
			     gpointer callback_data)
{
        NemoView *view;
        GList *selection;
        GArray *selected_item_locations;
        
        g_assert (NEMO_IS_VIEW (callback_data));

        view = NEMO_VIEW (callback_data);
	selection = nemo_view_get_selection (view);
	if (selection_not_empty_in_menu_callback (view, selection)) {
		selected_item_locations = nemo_view_get_selected_icon_locations (view);
	        nemo_view_create_links_for_files (view, selection, selected_item_locations);
	        g_array_free (selected_item_locations, TRUE);
	}

        nemo_file_list_free (selection);
}

static void
action_select_all_callback (GtkAction *action, 
			    gpointer callback_data)
{
	g_assert (NEMO_IS_VIEW (callback_data));

	nemo_view_select_all (callback_data);
}

static void
action_invert_selection_callback (GtkAction *action,
				  gpointer callback_data)
{
	g_assert (NEMO_IS_VIEW (callback_data));

	nemo_view_invert_selection (callback_data);
}

static void
pattern_select_response_cb (GtkWidget *dialog, int response, gpointer user_data)
{
	NemoView *view;
	NemoDirectory *directory;
	GtkWidget *entry;
	GList *selection;
	GError *error;

	view = NEMO_VIEW (user_data);

	switch (response) {
	case GTK_RESPONSE_OK :
		entry = g_object_get_data (G_OBJECT (dialog), "entry");
		directory = nemo_view_get_model (view);
		selection = nemo_directory_match_pattern (directory,
							      gtk_entry_get_text (GTK_ENTRY (entry)));
			
		if (selection) {
			nemo_view_call_set_selection (view, selection);
			nemo_file_list_free (selection);

			nemo_view_reveal_selection(view);
		}
		/* fall through */
	case GTK_RESPONSE_NONE :
	case GTK_RESPONSE_DELETE_EVENT :
	case GTK_RESPONSE_CANCEL :
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	case GTK_RESPONSE_HELP :
		error = NULL;
		gtk_show_uri (gtk_window_get_screen (GTK_WINDOW (dialog)),
			      "help:gnome-help/files-select",
			      gtk_get_current_event_time (), &error);
		if (error) {
			eel_show_error_dialog (_("There was an error displaying help."), error->message,
					       GTK_WINDOW (dialog));
			g_error_free (error);
		}
		break;
	default :
		g_assert_not_reached ();
	}
}

static void
select_pattern (NemoView *view)
{
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *example;
	GtkWidget *grid;
	GtkWidget *entry;
	char *example_pattern;

	dialog = gtk_dialog_new_with_buttons (_("Select Items Matching"),
					      nemo_view_get_containing_window (view),
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_HELP,
					      GTK_RESPONSE_HELP,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_OK,
					      NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 GTK_RESPONSE_OK);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 2);

	label = gtk_label_new_with_mnemonic (_("_Pattern:"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);

	example = gtk_label_new (NULL);
	gtk_widget_set_halign (example, GTK_ALIGN_START);
	example_pattern = g_strdup_printf ("<b>%s</b><i>%s</i> ", 
					   _("Examples: "),
					   "*.png, file\?\?.txt, pict*.\?\?\?");
	gtk_label_set_markup (GTK_LABEL (example), example_pattern);
	g_free (example_pattern);

	entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	gtk_widget_set_hexpand (entry, TRUE);

	grid = gtk_grid_new ();
	g_object_set (grid,
		      "orientation", GTK_ORIENTATION_VERTICAL,
		      "border-width", 6,
		      "row-spacing", 6,
		      "column-spacing", 12,
		      NULL);

	gtk_container_add (GTK_CONTAINER (grid), label);
	gtk_grid_attach_next_to (GTK_GRID (grid), entry, label,
				 GTK_POS_RIGHT, 1, 1);
	gtk_grid_attach_next_to (GTK_GRID (grid), example, entry,
				 GTK_POS_BOTTOM, 1, 1);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
	gtk_widget_show_all (grid);
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), grid);
	g_object_set_data (G_OBJECT (dialog), "entry", entry);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (pattern_select_response_cb),
			  view);
	gtk_widget_show_all (dialog);
}

static void
action_select_pattern_callback (GtkAction *action, 
				gpointer callback_data)
{
	g_assert (NEMO_IS_VIEW (callback_data));

	select_pattern(callback_data);
}

static void
action_reset_to_defaults_callback (GtkAction *action, 
				   gpointer callback_data)
{
	g_assert (NEMO_IS_VIEW (callback_data));

	nemo_view_reset_to_defaults (callback_data);
}


static void
hidden_files_mode_changed (NemoWindow *window,
			   gpointer callback_data)
{
	NemoView *directory_view;

	directory_view = NEMO_VIEW (callback_data);

	nemo_view_init_show_hidden_files (directory_view);
}

static void
action_save_search_callback (GtkAction *action,
			     gpointer callback_data)
{                
	NemoSearchDirectory *search;
	NemoView	*directory_view;
	
        directory_view = NEMO_VIEW (callback_data);

	if (directory_view->details->model &&
	    NEMO_IS_SEARCH_DIRECTORY (directory_view->details->model)) {
		search = NEMO_SEARCH_DIRECTORY (directory_view->details->model);
		nemo_search_directory_save_search (search);

		/* Save search is disabled */
		schedule_update_menus (directory_view);
	}
}

static void
query_name_entry_changed_cb  (GtkWidget *entry, GtkWidget *button)
{
	const char *text;
	gboolean sensitive;
	
	text = gtk_entry_get_text (GTK_ENTRY (entry));

	sensitive = (text != NULL) && (*text != 0);

	gtk_widget_set_sensitive (button, sensitive);
}


static void
action_save_search_as_callback (GtkAction *action,
				gpointer callback_data)
{
	NemoView	*directory_view;
	NemoSearchDirectory *search;
	GtkWidget *dialog, *grid, *label, *entry, *chooser, *save_button;
	const char *entry_text;
	char *filename, *filename_utf8, *dirname, *path, *uri;
	GFile *location;
	
        directory_view = NEMO_VIEW (callback_data);

	if (directory_view->details->model &&
	    NEMO_IS_SEARCH_DIRECTORY (directory_view->details->model)) {
		search = NEMO_SEARCH_DIRECTORY (directory_view->details->model);
		
		dialog = gtk_dialog_new_with_buttons (_("Save Search as"),
						      nemo_view_get_containing_window (directory_view),
						      0,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						      NULL);
		save_button = gtk_dialog_add_button (GTK_DIALOG (dialog),
						     GTK_STOCK_SAVE, GTK_RESPONSE_OK);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog),
						 GTK_RESPONSE_OK);
		gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
		gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 2);
		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

		grid = gtk_grid_new ();
		g_object_set (grid,
			      "orientation", GTK_ORIENTATION_VERTICAL,
			      "border-width", 5,
			      "row-spacing", 6,
			      "column-spacing", 12,
			      NULL);
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), grid, TRUE, TRUE, 0);
		gtk_widget_show (grid);
		
		label = gtk_label_new_with_mnemonic (_("Search _name:"));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_container_add (GTK_CONTAINER (grid), label);
		gtk_widget_show (label);

		entry = gtk_entry_new ();
		gtk_widget_set_hexpand (entry, TRUE);
		gtk_grid_attach_next_to (GTK_GRID (grid), entry, label,
					 GTK_POS_RIGHT, 1, 1);
		gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
		
		gtk_widget_set_sensitive (save_button, FALSE);
		g_signal_connect (entry, "changed",
				  G_CALLBACK (query_name_entry_changed_cb), save_button);
		
		gtk_widget_show (entry);
		label = gtk_label_new_with_mnemonic (_("_Folder:"));
		gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
		gtk_container_add (GTK_CONTAINER (grid), label);
		gtk_widget_show (label);

		chooser = gtk_file_chooser_button_new (_("Select Folder to Save Search In"),
						       GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
		gtk_widget_set_hexpand (chooser, TRUE);
		gtk_grid_attach_next_to (GTK_GRID (grid), chooser, label,
					 GTK_POS_RIGHT, 1, 1);
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), chooser);
		gtk_widget_show (chooser);

		gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);

		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
						     g_get_home_dir ());
		
		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
			entry_text = gtk_entry_get_text (GTK_ENTRY (entry));
			if (g_str_has_suffix (entry_text, NEMO_SAVED_SEARCH_EXTENSION)) {
				filename_utf8 = g_strdup (entry_text);
			} else {
				filename_utf8 = g_strconcat (entry_text, NEMO_SAVED_SEARCH_EXTENSION, NULL);
			}

			filename = g_filename_from_utf8 (filename_utf8, -1, NULL, NULL, NULL);
			g_free (filename_utf8);

			dirname = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
			
			path = g_build_filename (dirname, filename, NULL);
			g_free (filename);
			g_free (dirname);

			uri = g_filename_to_uri (path, NULL, NULL);
			g_free (path);
			
			nemo_search_directory_save_to_file (search, uri);
			location = g_file_new_for_uri (uri);
			nemo_file_changes_queue_file_added (location);
			g_object_unref (location);
			nemo_file_changes_consume_changes (TRUE);
			g_free (uri);
		}
		
		gtk_widget_destroy (dialog);
	}
}


static void
action_empty_trash_callback (GtkAction *action,
			     gpointer callback_data)
{                
        g_assert (NEMO_IS_VIEW (callback_data));

	nemo_file_operations_empty_trash (GTK_WIDGET (callback_data));
}

typedef struct {
	NemoView *view;
	NemoFile *new_file;
} RenameData;

static gboolean
delayed_rename_file_hack_callback (RenameData *data)
{
	NemoView *view;
	NemoFile *new_file;

	view = data->view;
	new_file = data->new_file;

	if (view->details->window != NULL &&
	    view->details->active) {
		NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->start_renaming_file (view, new_file, FALSE);
		nemo_view_reveal_selection (view);
	}

	return FALSE;
}

static void
delayed_rename_file_hack_removed (RenameData *data)
{
	g_object_unref (data->view);
	nemo_file_unref (data->new_file);
	g_free (data);
}


static void
rename_file (NemoView *view, NemoFile *new_file)
{
	RenameData *data;

	/* HACK!!!!
	   This is a work around bug in listview. After the rename is
	   enabled we will get file changes due to info about the new
	   file being read, which will cause the model to change. When
	   the model changes GtkTreeView clears the editing. This hack just
	   delays editing for some time to try to avoid this problem.
	   A major problem is that the selection of the row causes us
	   to load the slow mimetype for the file, which leads to a
	   file_changed. So, before we delay we select the row.
	*/
	if (NEMO_IS_LIST_VIEW (view)) {
		nemo_view_select_file (view, new_file);
		
		data = g_new (RenameData, 1);
		data->view = g_object_ref (view);
		data->new_file = nemo_file_ref (new_file);
		if (view->details->delayed_rename_file_id != 0) {
			g_source_remove (view->details->delayed_rename_file_id);
		}
		view->details->delayed_rename_file_id = 
			g_timeout_add_full (G_PRIORITY_DEFAULT,
					    100, (GSourceFunc)delayed_rename_file_hack_callback,
					    data, (GDestroyNotify) delayed_rename_file_hack_removed);
		
		return;
	}

	/* no need to select because start_renaming_file selects
	 * nemo_view_select_file (view, new_file);
	 */
	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->start_renaming_file (view, new_file, FALSE);
	nemo_view_reveal_selection (view);
}

static void
reveal_newly_added_folder (NemoView *view, NemoFile *new_file,
			   NemoDirectory *directory, GFile *target_location)
{
	GFile *location;

	location = nemo_file_get_location (new_file);
	if (g_file_equal (location, target_location)) {
		g_signal_handlers_disconnect_by_func (view,
						      G_CALLBACK (reveal_newly_added_folder),
						      (void *) target_location);
		rename_file (view, new_file);
	}
	g_object_unref (location);
}

typedef struct {
	NemoView *directory_view;
	GHashTable *added_locations;
	GList *selection;
} NewFolderData;

typedef struct {
	NemoView *directory_view;
	GHashTable *to_remove_locations;
	NemoFile *new_folder;
} NewFolderSelectionData;

static void
rename_newly_added_folder (NemoView *view, NemoFile *removed_file,
			   NemoDirectory *directory, NewFolderSelectionData *data);

static void
rename_newly_added_folder (NemoView *view, NemoFile *removed_file,
			   NemoDirectory *directory, NewFolderSelectionData *data)
{
	GFile *location;

	location = nemo_file_get_location (removed_file);
	if (!g_hash_table_remove (data->to_remove_locations, location)) {
		g_assert_not_reached ();
	}
	g_object_unref (location);
	if (g_hash_table_size (data->to_remove_locations) == 0) {
		nemo_view_set_selection (data->directory_view, NULL);
		g_signal_handlers_disconnect_by_func (data->directory_view,
						      G_CALLBACK (rename_newly_added_folder),
						      (void *) data);

		rename_file (data->directory_view, data->new_folder);
		g_object_unref (data->new_folder);
		g_hash_table_destroy (data->to_remove_locations);
		g_free (data);
	}
}

static void
track_newly_added_locations (NemoView *view, NemoFile *new_file,
			     NemoDirectory *directory, gpointer user_data)
{
	NewFolderData *data;

	data = user_data;

	g_hash_table_insert (data->added_locations, nemo_file_get_location (new_file), NULL);
}

static void
new_folder_done (GFile *new_folder, 
		 gboolean success,
		 gpointer user_data)
{
	NemoView *directory_view;
	NemoFile *file;
	char screen_string[32];
	GdkScreen *screen;
	NewFolderData *data;

	data = (NewFolderData *)user_data;

	directory_view = data->directory_view;

	if (directory_view == NULL) {
		goto fail;
	}

	g_signal_handlers_disconnect_by_func (directory_view,
					      G_CALLBACK (track_newly_added_locations),
					      (void *) data);

	if (new_folder == NULL) {
		goto fail;
	}
	
	screen = gtk_widget_get_screen (GTK_WIDGET (directory_view));
	g_snprintf (screen_string, sizeof (screen_string), "%d", gdk_screen_get_number (screen));

	
	file = nemo_file_get (new_folder);
	nemo_file_set_metadata
		(file, NEMO_METADATA_KEY_SCREEN,
		 NULL,
		 screen_string);

	if (data->selection != NULL) {
		NewFolderSelectionData *sdata;
		GList *uris, *l;

		sdata = g_new (NewFolderSelectionData, 1);
		sdata->directory_view = directory_view;
		sdata->to_remove_locations = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal,
								    g_object_unref, NULL);
		sdata->new_folder = g_object_ref (file);

		uris = NULL;
		for (l = data->selection; l != NULL; l = l->next) {
			GFile *old_location;
			GFile *new_location;
			char *basename;

			uris = g_list_prepend (uris, nemo_file_get_uri ((NemoFile *) l->data));

			old_location = nemo_file_get_location (l->data);
			basename = g_file_get_basename (old_location);
			new_location = g_file_resolve_relative_path (new_folder, basename);
			g_hash_table_insert (sdata->to_remove_locations, new_location, NULL);
			g_free (basename);
			g_object_unref (old_location);
		}
		uris = g_list_reverse (uris);

		g_signal_connect_data (directory_view,
				       "remove_file",
				       G_CALLBACK (rename_newly_added_folder),
				       sdata,
				       (GClosureNotify)NULL,
				       G_CONNECT_AFTER);

		nemo_view_move_copy_items (directory_view,
					       uris,
					       NULL,
					       nemo_file_get_uri (file),
					       GDK_ACTION_MOVE,
					       0, 0);
		g_list_free_full (uris, g_free);
	} else {
		if (g_hash_table_lookup_extended (data->added_locations, new_folder, NULL, NULL)) {
			/* The file was already added */
			rename_file (directory_view, file);
		} else {
			/* We need to run after the default handler adds the folder we want to
			 * operate on. The ADD_FILE signal is registered as G_SIGNAL_RUN_LAST, so we
			 * must use connect_after.
			 */
			g_signal_connect_data (directory_view,
					       "add_file",
					       G_CALLBACK (reveal_newly_added_folder),
					       g_object_ref (new_folder),
					       (GClosureNotify)g_object_unref,
					       G_CONNECT_AFTER);
		}
	}
	nemo_file_unref (file);

 fail:
	g_hash_table_destroy (data->added_locations);

	if (data->directory_view != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (data->directory_view),
					      (gpointer *) &data->directory_view);
	}

        nemo_file_list_free (data->selection);
	g_free (data);
}


static NewFolderData *
new_folder_data_new (NemoView *directory_view,
		     gboolean      with_selection)
{
	NewFolderData *data;

	data = g_new (NewFolderData, 1);
	data->directory_view = directory_view;
	data->added_locations = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal,
						       g_object_unref, NULL);
	if (with_selection) {
		data->selection = nemo_view_get_selection_for_file_transfer (directory_view);
	} else {
		data->selection = NULL;
	}
	g_object_add_weak_pointer (G_OBJECT (data->directory_view),
				   (gpointer *) &data->directory_view);

	return data;
}

static GdkPoint *
context_menu_to_file_operation_position (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), NULL);

	if (nemo_view_using_manual_layout (view)
	    && view->details->context_menu_position.x >= 0
	    && view->details->context_menu_position.y >= 0) {
		NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->widget_to_file_operation_position
			(view, &view->details->context_menu_position);
		return &view->details->context_menu_position;
	} else {
		return NULL;
	}
}

static void
nemo_view_new_folder (NemoView *directory_view,
			  gboolean      with_selection)
{
	char *parent_uri;
	NewFolderData *data;
	GdkPoint *pos;

	data = new_folder_data_new (directory_view, with_selection);

	g_signal_connect_data (directory_view,
			       "add_file",
			       G_CALLBACK (track_newly_added_locations),
			       data,
			       (GClosureNotify)NULL,
			       G_CONNECT_AFTER);

	pos = context_menu_to_file_operation_position (directory_view);

	parent_uri = nemo_view_get_backing_uri (directory_view);
	nemo_file_operations_new_folder (GTK_WIDGET (directory_view),
					     pos, parent_uri,
					     new_folder_done, data);

	g_free (parent_uri);
}

static NewFolderData *
setup_new_folder_data (NemoView *directory_view)
{
	NewFolderData *data;

	data = new_folder_data_new (directory_view, FALSE);

	g_signal_connect_data (directory_view,
			       "add_file",
			       G_CALLBACK (track_newly_added_locations),
			       data,
			       (GClosureNotify)NULL,
			       G_CONNECT_AFTER);

	return data;
}

void
nemo_view_new_file_with_initial_contents (NemoView *view,
					      const char *parent_uri,
					      const char *filename,
					      const char *initial_contents,
					      int length,
					      GdkPoint *pos)
{
	NewFolderData *data;

	g_assert (parent_uri != NULL);

	data = setup_new_folder_data (view);

	if (pos == NULL) {
		pos = context_menu_to_file_operation_position (view);
	}

	nemo_file_operations_new_file (GTK_WIDGET (view),
					   pos, parent_uri, filename,
					   initial_contents, length,
					   new_folder_done, data);
}

static void
nemo_view_new_file (NemoView *directory_view,
			const char *parent_uri,
			NemoFile *source)
{
	GdkPoint *pos;
	NewFolderData *data;
	char *source_uri;
	char *container_uri;

	container_uri = NULL;
	if (parent_uri == NULL) {
		container_uri = nemo_view_get_backing_uri (directory_view);
		g_assert (container_uri != NULL);
	}

	if (source == NULL) {
		nemo_view_new_file_with_initial_contents (directory_view,
							      parent_uri != NULL ? parent_uri : container_uri,
							      NULL,
							      NULL,
							      0,
							      NULL);
		g_free (container_uri);
		return;
	}

	g_return_if_fail (nemo_file_is_local (source));

	pos = context_menu_to_file_operation_position (directory_view);

	data = setup_new_folder_data (directory_view);

	source_uri = nemo_file_get_uri (source);

	nemo_file_operations_new_file_from_template (GTK_WIDGET (directory_view),
							 pos,
							 parent_uri != NULL ? parent_uri : container_uri,
							 NULL,
							 source_uri,
							 new_folder_done, data);

	g_free (source_uri);
	g_free (container_uri);
}

static void
action_new_folder_callback (GtkAction *action,
			    gpointer callback_data)
{                
        g_assert (NEMO_IS_VIEW (callback_data));

	nemo_view_new_folder (NEMO_VIEW (callback_data), FALSE);
}

static void
action_new_folder_with_selection_callback (GtkAction *action,
					   gpointer callback_data)
{                
        g_assert (NEMO_IS_VIEW (callback_data));

	nemo_view_new_folder (NEMO_VIEW (callback_data), TRUE);
}

static void
action_new_empty_file_callback (GtkAction *action,
				gpointer callback_data)
{                
        g_assert (NEMO_IS_VIEW (callback_data));

	nemo_view_new_file (NEMO_VIEW (callback_data), NULL, NULL);
}

static void
action_properties_callback (GtkAction *action,
			    gpointer callback_data)
{
        NemoView *view;
        GList *selection;
	GList *files;
        
        g_assert (NEMO_IS_VIEW (callback_data));

        view = NEMO_VIEW (callback_data);
	selection = nemo_view_get_selection (view);
	if (g_list_length (selection) == 0) {
		if (view->details->directory_as_file != NULL) {
			files = g_list_append (NULL, nemo_file_ref (view->details->directory_as_file));

			nemo_properties_window_present (files, GTK_WIDGET (view), NULL);

			nemo_file_list_free (files);
		}
	} else {
		nemo_properties_window_present (selection, GTK_WIDGET (view), NULL);
	}
        nemo_file_list_free (selection);
}

static void
action_location_properties_callback (GtkAction *action,
				     gpointer   callback_data)
{
	NemoView *view;
	GList           *files;

	g_assert (NEMO_IS_VIEW (callback_data));

	view = NEMO_VIEW (callback_data);
	g_assert (NEMO_IS_FILE (view->details->location_popup_directory_as_file));

	files = g_list_append (NULL, nemo_file_ref (view->details->location_popup_directory_as_file));

	nemo_properties_window_present (files, GTK_WIDGET (view), NULL);

	nemo_file_list_free (files);
}

static gboolean
all_files_in_trash (GList *files)
{
	GList *node;

	/* Result is ambiguous if called on NULL, so disallow. */
	g_return_val_if_fail (files != NULL, FALSE);

	for (node = files; node != NULL; node = node->next) {
		if (!nemo_file_is_in_trash (NEMO_FILE (node->data))) {
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
all_selected_items_in_trash (NemoView *view)
{
	GList *selection;
	gboolean result;

	/* If the contents share a parent directory, we need only
	 * check that parent directory. Otherwise we have to inspect
	 * each selected item.
	 */
	selection = nemo_view_get_selection (view);
	result = (selection == NULL) ? FALSE : all_files_in_trash (selection);
	nemo_file_list_free (selection);

	return result;
}

static void
click_policy_changed_callback (gpointer callback_data)
{
	NemoView *view;

	view = NEMO_VIEW (callback_data);

	NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->click_policy_changed (view);
}

static void
nemo_to_menu_preferences_changed_callback (NemoView *view)
{
    view->details->showing_bookmarks_in_to_menus = g_settings_get_boolean (nemo_preferences,
                                                                           NEMO_PREFERENCES_SHOW_BOOKMARKS_IN_TO_MENUS);
    view->details->showing_places_in_to_menus = g_settings_get_boolean (nemo_preferences,
                                                                        NEMO_PREFERENCES_SHOW_PLACES_IN_TO_MENUS);
}

gboolean
nemo_view_should_sort_directories_first (NemoView *view)
{
	return view->details->sort_directories_first;
}

static void
sort_directories_first_changed_callback (gpointer callback_data)
{
	NemoView *view;
	gboolean preference_value;

	view = NEMO_VIEW (callback_data);

	preference_value =
		g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SORT_DIRECTORIES_FIRST);

	if (preference_value != view->details->sort_directories_first) {
		view->details->sort_directories_first = preference_value;
		return NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->sort_directories_first_changed (view);
	}
}

static void
swap_delete_keybinding_changed_callback (gpointer callback_data)
{
    GtkBindingSet *binding_set = gtk_binding_set_find ("NemoView");

    gboolean swap_keys = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SWAP_TRASH_DELETE);

    gtk_binding_entry_remove (binding_set, GDK_KEY_Delete, 0);
    gtk_binding_entry_remove (binding_set, GDK_KEY_KP_Delete, 0);
    gtk_binding_entry_remove (binding_set, GDK_KEY_KP_Delete, GDK_SHIFT_MASK);
    gtk_binding_entry_remove (binding_set, GDK_KEY_Delete, GDK_SHIFT_MASK);

    if (swap_keys) {
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_Delete, 0,
                          "delete", 0);
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Delete, 0,
                          "delete", 0);
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Delete, GDK_SHIFT_MASK,
                          "trash", 0);
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_Delete, GDK_SHIFT_MASK,
                          "trash", 0);
    } else {
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_Delete, 0,
                          "trash", 0);
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Delete, 0,
                          "trash", 0);
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Delete, GDK_SHIFT_MASK,
                          "delete", 0);
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_Delete, GDK_SHIFT_MASK,
                          "delete", 0);
    }
}

static gboolean
set_up_scripts_directory_global (void)
{
	char *old_scripts_directory_path;
	char *scripts_directory_path;
	const char *override;

	if (scripts_directory_uri != NULL) {
		return TRUE;
	}

	scripts_directory_path = nemo_get_scripts_directory_path ();

	override = g_getenv ("GNOME22_USER_DIR");

	if (override) {
		old_scripts_directory_path = g_build_filename (override,
							       "nemo-scripts",
							       NULL);
	} else {
		old_scripts_directory_path = g_build_filename (g_get_home_dir (),
							       ".gnome2",
							       "nemo-scripts",
							       NULL);
	}

	if (g_file_test (old_scripts_directory_path, G_FILE_TEST_IS_DIR)
	    && !g_file_test (scripts_directory_path, G_FILE_TEST_EXISTS)) {
		char *updated;
		const char *message;

		/* test if we already attempted to migrate first */
		updated = g_build_filename (old_scripts_directory_path, "DEPRECATED-DIRECTORY", NULL);
		message = _("Nemo deprecated this directory and tried migrating "
			    "this configuration to ~/.local/share/nautilus");
		if (!g_file_test (updated, G_FILE_TEST_EXISTS)) {
			char *parent_dir;

			parent_dir = g_path_get_dirname (scripts_directory_path);
			if (g_mkdir_with_parents (parent_dir, 0700) == 0) {
				int fd, res;

				/* rename() works fine if the destination directory is
				 * empty.
				 */
				res = g_rename (old_scripts_directory_path, scripts_directory_path);
				if (res == -1) {
					fd = g_creat (updated, 0600);
					if (fd != -1) {
						res = write (fd, message, strlen (message));
						close (fd);
					}
				}
			}
			g_free (parent_dir);
		}

		g_free (updated);
	}

	if (g_mkdir_with_parents (scripts_directory_path, 0700) == 0) {
		scripts_directory_uri = g_filename_to_uri (scripts_directory_path, NULL, NULL);
		scripts_directory_uri_length = strlen (scripts_directory_uri);
	}

	g_free (scripts_directory_path);
	g_free (old_scripts_directory_path);

	return (scripts_directory_uri != NULL) ? TRUE : FALSE;
}

static void
scripts_added_or_changed_callback (NemoDirectory *directory,
				   GList *files,
				   gpointer callback_data)
{
	NemoView *view;

	view = NEMO_VIEW (callback_data);

	view->details->scripts_invalid = TRUE;
	if (view->details->active) {
		schedule_update_menus (view);
	}
}

static void
templates_added_or_changed_callback (NemoDirectory *directory,
				     GList *files,
				     gpointer callback_data)
{
	NemoView *view;

	view = NEMO_VIEW (callback_data);

	view->details->templates_invalid = TRUE;
	if (view->details->active) {
		schedule_update_menus (view);
	}
}

static void
actions_added_or_changed_callback (NemoView *view)
{
    view->details->actions_invalid = TRUE;
    if (view->details->active) {
        schedule_update_menus (view);
    }
}

static void
add_directory_to_directory_list (NemoView *view,
				 NemoDirectory *directory,
				 GList **directory_list,
				 GCallback changed_callback)
{
	NemoFileAttributes attributes;

	if (g_list_find (*directory_list, directory) == NULL) {
		nemo_directory_ref (directory);

		attributes =
			NEMO_FILE_ATTRIBUTES_FOR_ICON |
			NEMO_FILE_ATTRIBUTE_INFO |
			NEMO_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT;

		nemo_directory_file_monitor_add (directory, directory_list,
						     FALSE, attributes,
						     (NemoDirectoryCallback)changed_callback, view);

		g_signal_connect_object (directory, "files_added",
					 G_CALLBACK (changed_callback), view, 0);
		g_signal_connect_object (directory, "files_changed",
					 G_CALLBACK (changed_callback), view, 0);

		*directory_list = g_list_append	(*directory_list, directory);
	}
}

static void
remove_directory_from_directory_list (NemoView *view,
				      NemoDirectory *directory,
				      GList **directory_list,
				      GCallback changed_callback)
{
	*directory_list = g_list_remove	(*directory_list, directory);

	g_signal_handlers_disconnect_by_func (directory,
					      G_CALLBACK (changed_callback),
					      view);

	nemo_directory_file_monitor_remove (directory, directory_list);

	nemo_directory_unref (directory);
}

static void
add_directory_to_scripts_directory_list (NemoView *view,
					 NemoDirectory *directory)
{
	add_directory_to_directory_list (view, directory,
					 &view->details->scripts_directory_list,
					 G_CALLBACK (scripts_added_or_changed_callback));
}

static void
remove_directory_from_scripts_directory_list (NemoView *view,
					      NemoDirectory *directory)
{
	remove_directory_from_directory_list (view, directory,
					      &view->details->scripts_directory_list,
					      G_CALLBACK (scripts_added_or_changed_callback));
}

static void
add_directory_to_templates_directory_list (NemoView *view,
					   NemoDirectory *directory)
{
	add_directory_to_directory_list (view, directory,
					 &view->details->templates_directory_list,
					 G_CALLBACK (templates_added_or_changed_callback));
}

static void
remove_directory_from_templates_directory_list (NemoView *view,
						NemoDirectory *directory)
{
	remove_directory_from_directory_list (view, directory,
					      &view->details->templates_directory_list,
					      G_CALLBACK (templates_added_or_changed_callback));
}

static void
slot_active (NemoWindowSlot *slot,
	     NemoView *view)
{
	if (view->details->active) {
		return;
	}

	view->details->active = TRUE;

	nemo_view_merge_menus (view);
	schedule_update_menus (view);
}

static void
slot_inactive (NemoWindowSlot *slot,
	       NemoView *view)
{
	if (!view->details->active) {
		return;
	}

	view->details->active = FALSE;

	nemo_view_unmerge_menus (view);
	remove_update_menus_timeout_callback (view);
}

static void slot_changed_pane (NemoWindowSlot *slot,
			       NemoView *view)
{
	g_signal_handlers_disconnect_matched (view->details->window,
					      G_SIGNAL_MATCH_DATA, 0, 0,
					      NULL, NULL, view);
	
	view->details->window = nemo_window_slot_get_window (slot);
	schedule_update_menus (view);
	
	g_signal_connect_object (view->details->window,
		"hidden-files-mode-changed", G_CALLBACK (hidden_files_mode_changed),
		view, 0);
	hidden_files_mode_changed (view->details->window, view);
}

void
nemo_view_grab_focus (NemoView *view)
{
	/* focus the child of the scrolled window if it exists */
	GtkWidget *child;
	child = gtk_bin_get_child (GTK_BIN (view));
	if (child) {
		gtk_widget_grab_focus (GTK_WIDGET (child));
	}
}

int
nemo_view_get_selection_count (NemoView *view)
{
	/* FIXME: This could be faster if we special cased it in subclasses */
	GList *files;
	int len;

	files = nemo_view_get_selection (NEMO_VIEW (view));
	len = g_list_length (files);
	nemo_file_list_free (files);
	
	return len;
}

static void
update_undo_actions (NemoView *view)
{
	NemoFileUndoInfo *info;
	NemoFileUndoManagerState undo_state;
	GtkAction *action;
	const gchar *label, *tooltip;
	gboolean available, is_undo;
	gboolean undo_active, redo_active;
	gchar *undo_label, *undo_description, *redo_label, *redo_description;

	undo_label = undo_description = redo_label = redo_description = NULL;

	undo_active = FALSE;
	redo_active = FALSE;

	info = nemo_file_undo_manager_get_action ();
	undo_state = nemo_file_undo_manager_get_state ();

	if (info != NULL && 
	    (undo_state > NEMO_FILE_UNDO_MANAGER_STATE_NONE)) {
		is_undo = (undo_state == NEMO_FILE_UNDO_MANAGER_STATE_UNDO);

		if (is_undo) {
			undo_active = TRUE;
		} else {
			redo_active = TRUE;
		}

		nemo_file_undo_info_get_strings (info,
						     &undo_label, &undo_description,
						     &redo_label, &redo_description);
	}

	/* Update undo entry */
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      "Undo");
	available = undo_active;
	if (available) {
		label = undo_label;
		tooltip = undo_description;
	} else {
		/* Reset to default info */
		label = _("Undo");
		tooltip = _("Undo last action");
	}

	g_object_set (action,
		      "label", label,
		      "tooltip", tooltip,
		      NULL);
	gtk_action_set_sensitive (action, available);

	/* Update redo entry */
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      "Redo");
	available = redo_active;
	if (available) {
		label = redo_label;
		tooltip = redo_description;
	} else {
		/* Reset to default info */
		label = _("Redo");
		tooltip = _("Redo last undone action");
	}

	g_object_set (action,
		      "label", label,
		      "tooltip", tooltip,
		      NULL);
	gtk_action_set_sensitive (action, available);

	g_free (undo_label);
	g_free (undo_description);
	g_free (redo_label);
	g_free (redo_description);
}

static void
undo_manager_changed_cb (NemoFileUndoManager* manager,
			 NemoView *view)
{
	if (!view->details->active) {
		return;
	}

	update_undo_actions (view);
}

void
nemo_view_set_selection (NemoView *nemo_view,
			     GList *selection)
{
	NemoView *view;

	view = NEMO_VIEW (nemo_view);

	if (!view->details->loading) {
		/* If we aren't still loading, set the selection right now,
		 * and reveal the new selection.
		 */
		view->details->selection_change_is_due_to_shell = TRUE;
		nemo_view_call_set_selection (view, selection);
		view->details->selection_change_is_due_to_shell = FALSE;
		nemo_view_reveal_selection (view);
	} else {
		/* If we are still loading, set the list of pending URIs instead.
		 * done_loading() will eventually select the pending URIs and reveal them.
		 */
		g_list_free_full (view->details->pending_selection, g_object_unref);
		view->details->pending_selection =
			eel_g_object_list_copy (selection);
	}
}

static char *
get_bulk_rename_tool ()
{
	char *bulk_rename_tool;
	g_settings_get (nemo_preferences, NEMO_PREFERENCES_BULK_RENAME_TOOL, "^ay", &bulk_rename_tool);
	return g_strstrip (bulk_rename_tool);
}

static gboolean
have_bulk_rename_tool ()
{
	char *bulk_rename_tool;
	gboolean have_tool;

	bulk_rename_tool = get_bulk_rename_tool ();
	have_tool = ((bulk_rename_tool != NULL) && (*bulk_rename_tool != '\0'));
	g_free (bulk_rename_tool);
	return have_tool;
}

static void
nemo_view_init (NemoView *view)
{
	AtkObject *atk_object;
	NemoDirectory *scripts_directory;
	NemoDirectory *templates_directory;
	char *templates_uri;

	view->details = G_TYPE_INSTANCE_GET_PRIVATE (view, NEMO_TYPE_VIEW,
						     NemoViewDetails);

	/* Default to true; desktop-icon-view sets to false */
	view->details->show_foreign_files = TRUE;

	view->details->non_ready_files =
		g_hash_table_new_full (file_and_directory_hash,
				       file_and_directory_equal,
				       (GDestroyNotify)file_and_directory_free,
				       NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (view), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (view), NULL);

	gtk_style_context_set_junction_sides (gtk_widget_get_style_context (GTK_WIDGET (view)),
					      GTK_JUNCTION_TOP | GTK_JUNCTION_LEFT);

	if (set_up_scripts_directory_global ()) {
		scripts_directory = nemo_directory_get_by_uri (scripts_directory_uri);
		add_directory_to_scripts_directory_list (view, scripts_directory);
		nemo_directory_unref (scripts_directory);
	} else {
		g_warning ("Ignoring scripts directory, it may be a broken link\n");
	}

	if (nemo_should_use_templates_directory ()) {
		templates_uri = nemo_get_templates_directory_uri ();
		templates_directory = nemo_directory_get_by_uri (templates_uri);
		g_free (templates_uri);
		add_directory_to_templates_directory_list (view, templates_directory);
		nemo_directory_unref (templates_directory);
	}
	update_templates_directory (view);
	g_signal_connect_object (nemo_signaller_get_current (),
				 "user_dirs_changed",
				 G_CALLBACK (user_dirs_changed),
				 view, G_CONNECT_SWAPPED);

	view->details->sort_directories_first =
		g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SORT_DIRECTORIES_FIRST);

	g_signal_connect_object (nemo_trash_monitor_get (), "trash_state_changed",
				 G_CALLBACK (nemo_view_trash_state_changed_callback), view, 0);

	/* React to clipboard changes */
	g_signal_connect_object (nemo_clipboard_monitor_get (), "clipboard_changed",
				 G_CALLBACK (clipboard_changed_callback), view, 0);

	/* Register to menu provider extension signal managing menu updates */
	g_signal_connect_object (nemo_signaller_get_current (), "popup_menu_changed",
				 G_CALLBACK (nemo_view_update_menus), view, G_CONNECT_SWAPPED);

	gtk_widget_show (GTK_WIDGET (view));

	g_signal_connect_swapped (nemo_preferences,
				  "changed::" NEMO_PREFERENCES_ENABLE_DELETE,
				  G_CALLBACK (schedule_update_menus), view);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SWAP_TRASH_DELETE,
                  G_CALLBACK (swap_delete_keybinding_changed_callback), view);
	g_signal_connect_swapped (nemo_preferences,
				  "changed::" NEMO_PREFERENCES_CLICK_POLICY,
				  G_CALLBACK(click_policy_changed_callback),
				  view);
	g_signal_connect_swapped (nemo_preferences,
				  "changed::" NEMO_PREFERENCES_SORT_DIRECTORIES_FIRST, 
				  G_CALLBACK(sort_directories_first_changed_callback), view);
	g_signal_connect_swapped (gnome_lockdown_preferences,
				  "changed::" NEMO_PREFERENCES_LOCKDOWN_COMMAND_LINE,
				  G_CALLBACK (schedule_update_menus), view);

	g_signal_connect_swapped (nemo_window_state,
				  "changed::" NEMO_WINDOW_STATE_START_WITH_STATUS_BAR,
				  G_CALLBACK (nemo_view_display_selection_info), view);

    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_BOOKMARKS_IN_TO_MENUS,
                  G_CALLBACK (nemo_to_menu_preferences_changed_callback), view);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_PLACES_IN_TO_MENUS,
                  G_CALLBACK (nemo_to_menu_preferences_changed_callback), view);

    nemo_to_menu_preferences_changed_callback (view);

	g_signal_connect_object (nemo_file_undo_manager_get (), "undo-changed",
				 G_CALLBACK (undo_manager_changed_cb), view, 0);				  

	/* Accessibility */
	atk_object = gtk_widget_get_accessible (GTK_WIDGET (view));
	atk_object_set_name (atk_object, _("Content View"));
	atk_object_set_description (atk_object, _("View of the current folder"));

    view->details->action_manager = nemo_action_manager_new ();

    view->details->action_manager_changed_id =
        g_signal_connect_swapped (view->details->action_manager, "changed",
                      G_CALLBACK (actions_added_or_changed_callback),
                                  view);

    view->details->bookmarks = nemo_bookmark_list_new ();

    view->details->bookmarks_changed_id =
        g_signal_connect_swapped (view->details->bookmarks, "changed",
                      G_CALLBACK (schedule_update_menus),
                      view);
}

static void
real_unmerge_menus (NemoView *view)
{
	GtkUIManager *ui_manager;

	if (view->details->window == NULL) {
		return;
	}
    if (GTK_IS_ACTION_GROUP (view->details->copy_move_action_groups[0])) {
        disconnect_bookmark_signals (view);
    }

	ui_manager = nemo_window_get_ui_manager (view->details->window);

	nemo_ui_unmerge_ui (ui_manager,
				&view->details->dir_merge_id,
				&view->details->dir_action_group);
	nemo_ui_unmerge_ui (ui_manager,
				&view->details->extensions_menu_merge_id,
				&view->details->extensions_menu_action_group);
	nemo_ui_unmerge_ui (ui_manager,
				&view->details->open_with_merge_id,
				&view->details->open_with_action_group);
	nemo_ui_unmerge_ui (ui_manager,
				&view->details->scripts_merge_id,
				&view->details->scripts_action_group);
	nemo_ui_unmerge_ui (ui_manager,
				&view->details->templates_merge_id,
				&view->details->templates_action_group);
    nemo_ui_unmerge_ui (ui_manager,
                &view->details->actions_merge_id,
                &view->details->actions_action_group);
    int i;
    for (i = 0; i < 4; i++) {
        nemo_ui_unmerge_ui (ui_manager,
                &view->details->copy_move_merge_ids[i],
                &view->details->copy_move_action_groups[i]);
    }
}

static void
nemo_view_destroy (GtkWidget *object)
{
	NemoView *view;
	GList *node, *next;

	view = NEMO_VIEW (object);

	disconnect_model_handlers (view);

    if (view->details->bookmarks_changed_id != 0) {
        g_signal_handler_disconnect (view->details->bookmarks,
                         view->details->bookmarks_changed_id);
        view->details->bookmarks_changed_id = 0;
    }
    g_clear_object (&view->details->bookmarks);

    if (view->details->action_manager_changed_id != 0) {
        g_signal_handler_disconnect (view->details->action_manager,
                         view->details->action_manager_changed_id);
        view->details->action_manager_changed_id = 0;
    }
    g_clear_object (&view->details->action_manager);

	nemo_view_unmerge_menus (view);
	
	/* We don't own the window, so no unref */
	view->details->slot = NULL;
	view->details->window = NULL;
	
	nemo_view_stop_loading (view);

	for (node = view->details->scripts_directory_list; node != NULL; node = next) {
		next = node->next;
		remove_directory_from_scripts_directory_list (view, node->data);
	}

	for (node = view->details->templates_directory_list; node != NULL; node = next) {
		next = node->next;
		remove_directory_from_templates_directory_list (view, node->data);
	}

	while (view->details->subdirectory_list != NULL) {
		nemo_view_remove_subdirectory (view,
						   view->details->subdirectory_list->data);
	}

	remove_update_menus_timeout_callback (view);
	remove_update_status_idle_callback (view);

	if (view->details->display_selection_idle_id != 0) {
		g_source_remove (view->details->display_selection_idle_id);
		view->details->display_selection_idle_id = 0;
	}

	if (view->details->reveal_selection_idle_id != 0) {
		g_source_remove (view->details->reveal_selection_idle_id);
		view->details->reveal_selection_idle_id = 0;
	}

	if (view->details->delayed_rename_file_id != 0) {
		g_source_remove (view->details->delayed_rename_file_id);
		view->details->delayed_rename_file_id = 0;
	}

	if (view->details->model) {
		nemo_directory_unref (view->details->model);
		view->details->model = NULL;
	}
	
	if (view->details->directory_as_file) {
		nemo_file_unref (view->details->directory_as_file);
		view->details->directory_as_file = NULL;
	}

	GTK_WIDGET_CLASS (nemo_view_parent_class)->destroy (object);
}

static void
nemo_view_finalize (GObject *object)
{
	NemoView *view;

	view = NEMO_VIEW (object);

	g_signal_handlers_disconnect_by_func (nemo_preferences,
					      schedule_update_menus, view);
	g_signal_handlers_disconnect_by_func (nemo_preferences,
					      click_policy_changed_callback, view);
	g_signal_handlers_disconnect_by_func (nemo_preferences,
					      sort_directories_first_changed_callback, view);
	g_signal_handlers_disconnect_by_func (nemo_window_state,
					      nemo_view_display_selection_info, view);

	g_signal_handlers_disconnect_by_func (gnome_lockdown_preferences,
					      schedule_update_menus, view);

    g_signal_handlers_disconnect_by_func (nemo_preferences,
                          nemo_to_menu_preferences_changed_callback, view);

	unschedule_pop_up_location_context_menu (view);
	if (view->details->location_popup_event != NULL) {
		gdk_event_free ((GdkEvent *) view->details->location_popup_event);
	}

	g_hash_table_destroy (view->details->non_ready_files);

	G_OBJECT_CLASS (nemo_view_parent_class)->finalize (object);
}

/**
 * nemo_view_display_selection_info:
 *
 * Display information about the current selection, and notify the view frame of the changed selection.
 * @view: NemoView for which to display selection info.
 *
 **/
void
nemo_view_display_selection_info (NemoView *view)
{
	GList *selection;
	goffset non_folder_size;
	gboolean non_folder_size_known;
	guint non_folder_count, folder_count, folder_item_count;
	gboolean folder_item_count_known;
	guint file_item_count;
	GList *p;
	char *first_item_name;
	char *non_folder_str;
	char *folder_count_str;
	char *folder_item_count_str;
	char *status_string;
	char *view_status_string;
	char *free_space_str;
	char *obj_selected_free_space_str;
	NemoFile *file;

	g_return_if_fail (NEMO_IS_VIEW (view));

	selection = nemo_view_get_selection (view);
	
	folder_item_count_known = TRUE;
	folder_count = 0;
	folder_item_count = 0;
	non_folder_count = 0;
	non_folder_size_known = FALSE;
	non_folder_size = 0;
	first_item_name = NULL;
	folder_count_str = NULL;
	non_folder_str = NULL;
	folder_item_count_str = NULL;
	free_space_str = NULL;
	obj_selected_free_space_str = NULL;
	status_string = NULL;
	view_status_string = NULL;
	
	for (p = selection; p != NULL; p = p->next) {
		file = p->data;
		if (nemo_file_is_directory (file)) {
			folder_count++;
			if (nemo_file_get_directory_item_count (file, &file_item_count, NULL)) {
				folder_item_count += file_item_count;
			} else {
				folder_item_count_known = FALSE;
			}
		} else {
			non_folder_count++;
			if (!nemo_file_can_get_size (file)) {
				non_folder_size_known = TRUE;
				non_folder_size += nemo_file_get_size (file);
			}
		}

		if (first_item_name == NULL) {
			first_item_name = nemo_file_get_display_name (file);
		}
	}
	
	nemo_file_list_free (selection);
	
	/* Break out cases for localization's sake. But note that there are still pieces
	 * being assembled in a particular order, which may be a problem for some localizers.
	 */

	if (folder_count != 0) {
		if (folder_count == 1 && non_folder_count == 0) {
			folder_count_str = g_strdup_printf (_("\"%s\" selected"), first_item_name);
		} else {
			folder_count_str = g_strdup_printf (ngettext("%'d folder selected", 
								     "%'d folders selected", 
								     folder_count), 
							    folder_count);
		}

		if (folder_count == 1) {
			if (!folder_item_count_known) {
				folder_item_count_str = g_strdup ("");
			} else {
				folder_item_count_str = g_strdup_printf (ngettext(" (containing %'d item)",
										  " (containing %'d items)",
										  folder_item_count), 
									 folder_item_count);
			}
		}
		else {
			if (!folder_item_count_known) {
				folder_item_count_str = g_strdup ("");
			} else {
				/* translators: this is preceded with a string of form 'N folders' (N more than 1) */
				folder_item_count_str = g_strdup_printf (ngettext(" (containing a total of %'d item)",
										  " (containing a total of %'d items)",
										  folder_item_count), 
									 folder_item_count);
			}
			
		}
	}

	if (non_folder_count != 0) {
		char *items_string;

		if (folder_count == 0) {
			if (non_folder_count == 1) {
				items_string = g_strdup_printf (_("\"%s\" selected"), 
								first_item_name);
			} else {
				items_string = g_strdup_printf (ngettext("%'d item selected",
									 "%'d items selected",
									 non_folder_count), 
								non_folder_count);
			}
		} else {
			/* Folders selected also, use "other" terminology */
			items_string = g_strdup_printf (ngettext("%'d other item selected",
								 "%'d other items selected",
								 non_folder_count), 
							non_folder_count);
		}

		if (non_folder_size_known) {
			char *size_string;
			int prefix;
			
			prefix = g_settings_get_enum (nemo_preferences, NEMO_PREFERENCES_SIZE_PREFIXES);
			size_string = g_format_size_full (non_folder_size, prefix);
			/* This is marked for translation in case a localiser
			 * needs to use something other than parentheses. The
			 * first message gives the number of items selected;
			 * the message in parentheses the size of those items.
			 */
			non_folder_str = g_strdup_printf (_("%s (%s)"), 
							  items_string, 
							  size_string);

			g_free (size_string);
			g_free (items_string);
		} else {
			non_folder_str = items_string;
		}
	}

	free_space_str = nemo_file_get_volume_free_space (view->details->directory_as_file);
	if (free_space_str != NULL) {
		obj_selected_free_space_str = g_strdup_printf (_("Free space: %s"), free_space_str);
	}
	if (folder_count == 0 && non_folder_count == 0)	{
		char *item_count_str;
		guint item_count;

		item_count = nemo_view_get_item_count (view);
		
		item_count_str = g_strdup_printf (ngettext ("%'u item", "%'u items", item_count), item_count);

		if (free_space_str != NULL) {
			status_string = g_strdup_printf (_("%s, Free space: %s"), item_count_str, free_space_str);
			g_free (item_count_str);
		} else {
			status_string = item_count_str;
		}

	} else if (folder_count == 0) {
		view_status_string = g_strdup (non_folder_str);

		if (free_space_str != NULL) {
			/* Marking this for translation, since you
			 * might want to change "," to something else.
			 * After the comma the amount of free space will
			 * be shown.
			 */
			status_string = g_strdup_printf (_("%s, %s"),
							 non_folder_str,
							 obj_selected_free_space_str);
		}
	} else if (non_folder_count == 0) {
		/* No use marking this for translation, since you
		 * can't reorder the strings, which is the main thing
		 * you'd want to do.
		 */
		view_status_string = g_strdup_printf ("%s%s",
						      folder_count_str,
						      folder_item_count_str);

		if (free_space_str != NULL) {
			/* Marking this for translation, since you
			 * might want to change "," to something else.
			 * After the comma the amount of free space will
			 * be shown.
			 */
			status_string = g_strdup_printf (_("%s%s, %s"),
							 folder_count_str,
							 folder_item_count_str,
							 obj_selected_free_space_str);
		}
	} else {
		/* This is marked for translation in case a localizer
		 * needs to change ", " to something else. The comma
		 * is between the message about the number of folders
		 * and the number of items in those folders and the
		 * message about the number of other items and the
		 * total size of those items.
		 */
		view_status_string = g_strdup_printf (_("%s%s, %s"),
						      folder_count_str,
						      folder_item_count_str,
						      non_folder_str);

		if (obj_selected_free_space_str != NULL) {
			/* This is marked for translation in case a localizer
			 * needs to change ", " to something else. The first comma
			 * is between the message about the number of folders
			 * and the number of items in those folders and the
			 * message about the number of other items and the
			 * total size of those items. After the second comma
			 * the free space is written.
			 */
			status_string = g_strdup_printf (_("%s%s, %s, %s"),
							 folder_count_str,
							 folder_item_count_str,
							 non_folder_str,
							 obj_selected_free_space_str);
		}
	}

	g_free (free_space_str);
	g_free (obj_selected_free_space_str);
	g_free (first_item_name);
	g_free (folder_count_str);
	g_free (folder_item_count_str);
	g_free (non_folder_str);

	if (status_string == NULL) {
		status_string = g_strdup (view_status_string);
	}

	nemo_window_slot_set_status (view->details->slot,
					 status_string,
					 view_status_string);

	g_free (status_string);
	g_free (view_status_string);
}

static void
nemo_view_send_selection_change (NemoView *view)
{
	g_signal_emit (view, signals[SELECTION_CHANGED], 0);

	view->details->send_selection_change_to_shell = FALSE;
}

void
nemo_view_load_location (NemoView *nemo_view,
			     GFile        *location)
{
	NemoDirectory *directory;
	NemoView *directory_view;

	directory_view = NEMO_VIEW (nemo_view);

	directory = nemo_directory_get (location);
	load_directory (directory_view, directory);
	nemo_directory_unref (directory);
}

static gboolean
reveal_selection_idle_callback (gpointer data)
{
	NemoView *view;
	
	view = NEMO_VIEW (data);

	view->details->reveal_selection_idle_id = 0;
	nemo_view_reveal_selection (view);

	return FALSE;
}

static void
done_loading (NemoView *view,
	      gboolean all_files_seen)
{
	GList *selection;

	if (!view->details->loading) {
		return;
	}

	/* This can be called during destruction, in which case there
	 * is no NemoWindow any more.
	 */
	if (view->details->window != NULL) {
		if (all_files_seen) {
			nemo_window_report_load_complete (view->details->window, NEMO_VIEW (view));
		}

		schedule_update_menus (view);
		schedule_update_status (view);
		reset_update_interval (view);

		selection = view->details->pending_selection;
		if (selection != NULL && all_files_seen) {
			view->details->pending_selection = NULL;

			view->details->selection_change_is_due_to_shell = TRUE;
			nemo_view_call_set_selection (view, selection);
			view->details->selection_change_is_due_to_shell = FALSE;

			if (NEMO_IS_LIST_VIEW (view)) {
				/* HACK: We should be able to directly call reveal_selection here,
				 * but at this point the GtkTreeView hasn't allocated the new nodes
				 * yet, and it has a bug in the scroll calculation dealing with this
				 * special case. It would always make the selection the top row, even
				 * if no scrolling would be neccessary to reveal it. So we let it
				 * allocate before revealing.
				 */
				if (view->details->reveal_selection_idle_id != 0) {
					g_source_remove (view->details->reveal_selection_idle_id);
				}
				view->details->reveal_selection_idle_id = 
					g_idle_add (reveal_selection_idle_callback, view);
			} else {
				nemo_view_reveal_selection (view);
			}
		}
		g_list_free_full (selection, g_object_unref);
		nemo_view_display_selection_info (view);
	}

	g_signal_emit (view, signals[END_LOADING], 0, all_files_seen);

	view->details->loading = FALSE;
}


typedef struct {
	GHashTable *debuting_files;
	GList	   *added_files;
} DebutingFilesData;

static void
debuting_files_data_free (DebutingFilesData *data)
{
	g_hash_table_unref (data->debuting_files);
	nemo_file_list_free (data->added_files);
	g_free (data);
}
 
/* This signal handler watch for the arrival of the icons created
 * as the result of a file operation. Once the last one is detected
 * it selects and reveals them all.
 */
static void
debuting_files_add_file_callback (NemoView *view,
				  NemoFile *new_file,
				  NemoDirectory *directory,
				  DebutingFilesData *data)
{
	GFile *location;

	location = nemo_file_get_location (new_file);

	if (g_hash_table_remove (data->debuting_files, location)) {
		nemo_file_ref (new_file);
		data->added_files = g_list_prepend (data->added_files, new_file);

		if (g_hash_table_size (data->debuting_files) == 0) {
			nemo_view_call_set_selection (view, data->added_files);
			nemo_view_reveal_selection (view);
			g_signal_handlers_disconnect_by_func (view,
							      G_CALLBACK (debuting_files_add_file_callback),
							      data);
		}
	}
	
	g_object_unref (location);
}

typedef struct {
	GList		*added_files;
	NemoView *directory_view;
} CopyMoveDoneData;

static void
copy_move_done_data_free (CopyMoveDoneData *data)
{
	g_assert (data != NULL);
	
	if (data->directory_view != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (data->directory_view),
					      (gpointer *) &data->directory_view);
	}

	nemo_file_list_free (data->added_files);
	g_free (data);
}

static void
pre_copy_move_add_file_callback (NemoView *view,
				 NemoFile *new_file,
				 NemoDirectory *directory,
				 CopyMoveDoneData *data)
{
	nemo_file_ref (new_file);
	data->added_files = g_list_prepend (data->added_files, new_file);
}

/* This needs to be called prior to nemo_file_operations_copy_move.
 * It hooks up a signal handler to catch any icons that get added before
 * the copy_done_callback is invoked. The return value should  be passed
 * as the data for uri_copy_move_done_callback.
 */
static CopyMoveDoneData *
pre_copy_move (NemoView *directory_view)
{
	CopyMoveDoneData *copy_move_done_data;

	copy_move_done_data = g_new0 (CopyMoveDoneData, 1);
	copy_move_done_data->directory_view = directory_view;

	g_object_add_weak_pointer (G_OBJECT (copy_move_done_data->directory_view),
				   (gpointer *) &copy_move_done_data->directory_view);

	/* We need to run after the default handler adds the folder we want to
	 * operate on. The ADD_FILE signal is registered as G_SIGNAL_RUN_LAST, so we
	 * must use connect_after.
	 */
	g_signal_connect (directory_view, "add_file",
			  G_CALLBACK (pre_copy_move_add_file_callback), copy_move_done_data);

	return copy_move_done_data;
}

/* This function is used to pull out any debuting uris that were added
 * and (as a side effect) remove them from the debuting uri hash table.
 */
static gboolean
copy_move_done_partition_func (gpointer data, gpointer callback_data)
{
 	GFile *location;
 	gboolean result;
 	
	location = nemo_file_get_location (NEMO_FILE (data));
	result = g_hash_table_remove ((GHashTable *) callback_data, location);
	g_object_unref (location);

	return result;
}

static gboolean
remove_not_really_moved_files (gpointer key,
			       gpointer value,
			       gpointer callback_data)
{
	GList **added_files;
	GFile *loc;

	loc = key;

	if (GPOINTER_TO_INT (value)) {
		return FALSE;
	}
	
	added_files = callback_data;
	*added_files = g_list_prepend (*added_files,
				       nemo_file_get (loc));
	return TRUE;
}


/* When this function is invoked, the file operation is over, but all
 * the icons may not have been added to the directory view yet, so
 * we can't select them yet.
 * 
 * We're passed a hash table of the uri's to look out for, we hook
 * up a signal handler to await their arrival.
 */
static void
copy_move_done_callback (GHashTable *debuting_files, 
			 gboolean success,
			 gpointer data)
{
	NemoView  *directory_view;
	CopyMoveDoneData *copy_move_done_data;
	DebutingFilesData  *debuting_files_data;

	copy_move_done_data = (CopyMoveDoneData *) data;
	directory_view = copy_move_done_data->directory_view;

	if (directory_view != NULL) {
		g_assert (NEMO_IS_VIEW (directory_view));
	
		debuting_files_data = g_new (DebutingFilesData, 1);
		debuting_files_data->debuting_files = g_hash_table_ref (debuting_files);
		debuting_files_data->added_files = eel_g_list_partition
			(copy_move_done_data->added_files,
			 copy_move_done_partition_func,
			 debuting_files,
			 &copy_move_done_data->added_files);

		/* We're passed the same data used by pre_copy_move_add_file_callback, so disconnecting
		 * it will free data. We've already siphoned off the added_files we need, and stashed the
		 * directory_view pointer.
		 */
		g_signal_handlers_disconnect_by_func (directory_view,
						      G_CALLBACK (pre_copy_move_add_file_callback),
						      data);
	
		/* Any items in the debuting_files hash table that have
		 * "FALSE" as their value aren't really being copied
		 * or moved, so we can't wait for an add_file signal
		 * to come in for those.
		 */
		g_hash_table_foreach_remove (debuting_files,
					     remove_not_really_moved_files,
					     &debuting_files_data->added_files);
		
		if (g_hash_table_size (debuting_files) == 0) {
			/* on the off-chance that all the icons have already been added */
			if (debuting_files_data->added_files != NULL) {
				nemo_view_call_set_selection (directory_view,
								  debuting_files_data->added_files);
				nemo_view_reveal_selection (directory_view);
			}
			debuting_files_data_free (debuting_files_data);
		} else {
			/* We need to run after the default handler adds the folder we want to
			 * operate on. The ADD_FILE signal is registered as G_SIGNAL_RUN_LAST, so we
			 * must use connect_after.
			 */
			g_signal_connect_data (directory_view,
					       "add_file",
					       G_CALLBACK (debuting_files_add_file_callback),
					       debuting_files_data,
					       (GClosureNotify) debuting_files_data_free,
					       G_CONNECT_AFTER);
		}
		/* Schedule menu update for undo items */
		schedule_update_menus (directory_view);
	}

	copy_move_done_data_free (copy_move_done_data);
}

static gboolean
view_file_still_belongs (NemoView *view,
			 NemoFile *file,
			 NemoDirectory *directory)
{
	if (view->details->model != directory &&
	    g_list_find (view->details->subdirectory_list, directory) == NULL) {
		return FALSE;
	}
	
	return nemo_directory_contains_file (directory, file);
}

static gboolean
still_should_show_file (NemoView *view, NemoFile *file, NemoDirectory *directory)
{
	return nemo_view_should_show_file (view, file) &&
		view_file_still_belongs (view, file, directory);
}

static gboolean
ready_to_load (NemoFile *file)
{
	return nemo_file_check_if_ready (file,
					     NEMO_FILE_ATTRIBUTES_FOR_ICON);
}

static int
compare_files_cover (gconstpointer a, gconstpointer b, gpointer callback_data)
{
	const FileAndDirectory *fad1, *fad2;
	NemoView *view;
	
	view = callback_data;
	fad1 = a; fad2 = b;

	if (fad1->directory < fad2->directory) {
		return -1;
	} else if (fad1->directory > fad2->directory) {
		return 1;
	} else {
		return NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->compare_files (view, fad1->file, fad2->file);
	}
}
static void
sort_files (NemoView *view, GList **list)
{
	*list = g_list_sort_with_data (*list, compare_files_cover, view);
	
}

/* Go through all the new added and changed files.
 * Put any that are not ready to load in the non_ready_files hash table.
 * Add all the rest to the old_added_files and old_changed_files lists.
 * Sort the old_*_files lists if anything was added to them.
 */
static void
process_new_files (NemoView *view)
{
	GList *new_added_files, *new_changed_files, *old_added_files, *old_changed_files;
	GHashTable *non_ready_files;
	GList *node, *next;
	FileAndDirectory *pending;
	gboolean in_non_ready;

	new_added_files = view->details->new_added_files;
	view->details->new_added_files = NULL;
	new_changed_files = view->details->new_changed_files;
	view->details->new_changed_files = NULL;

	non_ready_files = view->details->non_ready_files;

	old_added_files = view->details->old_added_files;
	old_changed_files = view->details->old_changed_files;

	/* Newly added files go into the old_added_files list if they're
	 * ready, and into the hash table if they're not.
	 */
	for (node = new_added_files; node != NULL; node = next) {
		next = node->next;
		pending = (FileAndDirectory *)node->data;
		in_non_ready = g_hash_table_lookup (non_ready_files, pending) != NULL;
		if (nemo_view_should_show_file (view, pending->file)) {
			if (ready_to_load (pending->file)) {
				if (in_non_ready) {
					g_hash_table_remove (non_ready_files, pending);
				}
				new_added_files = g_list_delete_link (new_added_files, node);
				old_added_files = g_list_prepend (old_added_files, pending);
			} else {
				if (!in_non_ready) {
					new_added_files = g_list_delete_link (new_added_files, node);
					g_hash_table_insert (non_ready_files, pending, pending);
				}
			}
		}
	}
	file_and_directory_list_free (new_added_files);

	/* Newly changed files go into the old_added_files list if they're ready
	 * and were seen non-ready in the past, into the old_changed_files list
	 * if they are read and were not seen non-ready in the past, and into
	 * the hash table if they're not ready.
	 */
	for (node = new_changed_files; node != NULL; node = next) {
		next = node->next;
		pending = (FileAndDirectory *)node->data;
		if (!still_should_show_file (view, pending->file, pending->directory) || ready_to_load (pending->file)) {
			if (g_hash_table_lookup (non_ready_files, pending) != NULL) {
				g_hash_table_remove (non_ready_files, pending);
				if (still_should_show_file (view, pending->file, pending->directory)) {
					new_changed_files = g_list_delete_link (new_changed_files, node);
					old_added_files = g_list_prepend (old_added_files, pending);
				}
			} else if (nemo_view_should_show_file (view, pending->file)) {
				new_changed_files = g_list_delete_link (new_changed_files, node);
				old_changed_files = g_list_prepend (old_changed_files, pending);
			}
		}
	}
	file_and_directory_list_free (new_changed_files);

	/* If any files were added to old_added_files, then resort it. */
	if (old_added_files != view->details->old_added_files) {
		view->details->old_added_files = old_added_files;
		sort_files (view, &view->details->old_added_files);
	}

	/* Resort old_changed_files too, since file attributes
	 * relevant to sorting could have changed.
	 */
	if (old_changed_files != view->details->old_changed_files) {
		view->details->old_changed_files = old_changed_files;
		sort_files (view, &view->details->old_changed_files);
	}

}

static void
process_old_files (NemoView *view)
{
	GList *files_added, *files_changed, *node;
	FileAndDirectory *pending;
	GList *selection, *files;
	gboolean send_selection_change;

	files_added = view->details->old_added_files;
	files_changed = view->details->old_changed_files;
	
	send_selection_change = FALSE;

	if (files_added != NULL || files_changed != NULL) {
		g_signal_emit (view, signals[BEGIN_FILE_CHANGES], 0);

		for (node = files_added; node != NULL; node = node->next) {
			pending = node->data;
			g_signal_emit (view,
				       signals[ADD_FILE], 0, pending->file, pending->directory);
		}

		for (node = files_changed; node != NULL; node = node->next) {
			pending = node->data;
			g_signal_emit (view,
				       signals[still_should_show_file (view, pending->file, pending->directory)
					       ? FILE_CHANGED : REMOVE_FILE], 0,
				       pending->file, pending->directory);
		}

		g_signal_emit (view, signals[END_FILE_CHANGES], 0);

		if (files_changed != NULL) {
			selection = nemo_view_get_selection (view);
			files = file_and_directory_list_to_files (files_changed);
			send_selection_change = eel_g_lists_sort_and_check_for_intersection
				(&files, &selection);
			nemo_file_list_free (files);
			nemo_file_list_free (selection);
		}
		
		file_and_directory_list_free (view->details->old_added_files);
		view->details->old_added_files = NULL;

		file_and_directory_list_free (view->details->old_changed_files);
		view->details->old_changed_files = NULL;
	}

	if (send_selection_change) {
		/* Send a selection change since some file names could
		 * have changed.
		 */
		nemo_view_send_selection_change (view);
	}
}

static void
display_pending_files (NemoView *view)
{

	/* Don't dispatch any updates while the view is frozen. */
	if (view->details->updates_frozen) {
		return;
	}

	process_new_files (view);
	process_old_files (view);

	if (view->details->model != NULL
	    && nemo_directory_are_all_files_seen (view->details->model)
	    && g_hash_table_size (view->details->non_ready_files) == 0) {
		done_loading (view, TRUE);
	}
}

void
nemo_view_freeze_updates (NemoView *view)
{
	view->details->updates_frozen = TRUE;
	view->details->updates_queued = 0;
	view->details->needs_reload = FALSE;
}

void
nemo_view_unfreeze_updates (NemoView *view)
{
	view->details->updates_frozen = FALSE;

	if (view->details->needs_reload) {
		view->details->needs_reload = FALSE;
		if (view->details->model != NULL) {
			load_directory (view, view->details->model);
		}
	} else {
		schedule_idle_display_of_pending_files (view);
	}
}

static gboolean
display_selection_info_idle_callback (gpointer data)
{
	NemoView *view;
	
	view = NEMO_VIEW (data);

	g_object_ref (G_OBJECT (view));

	view->details->display_selection_idle_id = 0;
	nemo_view_display_selection_info (view);
	if (view->details->send_selection_change_to_shell) {
		nemo_view_send_selection_change (view);
	}

	g_object_unref (G_OBJECT (view));

	return FALSE;
}

static void
remove_update_menus_timeout_callback (NemoView *view) 
{
	if (view->details->update_menus_timeout_id != 0) {
		g_source_remove (view->details->update_menus_timeout_id);
		view->details->update_menus_timeout_id = 0;
	}
}

static void
update_menus_if_pending (NemoView *view)
{
	if (!view->details->menu_states_untrustworthy) {
		return;
	}

	remove_update_menus_timeout_callback (view);
	nemo_view_update_menus (view);
}

static gboolean
update_menus_timeout_callback (gpointer data)
{
	NemoView *view;
	view = NEMO_VIEW (data);

	g_object_ref (G_OBJECT (view));

	view->details->update_menus_timeout_id = 0;
	nemo_view_update_menus (view);

	g_object_unref (G_OBJECT (view));

	return FALSE;
}

static gboolean
display_pending_callback (gpointer data)
{
	NemoView *view;

	view = NEMO_VIEW (data);

	g_object_ref (G_OBJECT (view));

	view->details->display_pending_source_id = 0;

	display_pending_files (view);

	g_object_unref (G_OBJECT (view));

	return FALSE;
}

static void
schedule_idle_display_of_pending_files (NemoView *view)
{
	/* Get rid of a pending source as it might be a timeout */
	unschedule_display_of_pending_files (view);

	/* We want higher priority than the idle that handles the relayout
	   to avoid a resort on each add. But we still want to allow repaints
	   and other hight prio events while we have pending files to show. */
	view->details->display_pending_source_id =
		g_idle_add_full (G_PRIORITY_DEFAULT_IDLE - 20,
				 display_pending_callback, view, NULL);
}

static void
schedule_timeout_display_of_pending_files (NemoView *view, guint interval)
{
 	/* No need to schedule an update if there's already one pending. */
	if (view->details->display_pending_source_id != 0) {
 		return;
	}
 
	view->details->display_pending_source_id =
		g_timeout_add (interval, display_pending_callback, view);
}

static void
unschedule_display_of_pending_files (NemoView *view)
{
	/* Get rid of source if it's active. */
	if (view->details->display_pending_source_id != 0) {
		g_source_remove (view->details->display_pending_source_id);
		view->details->display_pending_source_id = 0;
	}
}

static void
queue_pending_files (NemoView *view,
		     NemoDirectory *directory,
		     GList *files,
		     GList **pending_list)
{
	if (files == NULL) {
		return;
	}

	/* Don't queue any more updates if we need to reload anyway */
	if (view->details->needs_reload) {
		return;
	}

	if (view->details->updates_frozen) {
		view->details->updates_queued += g_list_length (files);
		/* Mark the directory for reload when there are too much queued
		 * changes to prevent the pending list from growing infinitely.
		 */
		if (view->details->updates_queued > MAX_QUEUED_UPDATES) {
			view->details->needs_reload = TRUE;
			return;
		}
	}

	

	*pending_list = g_list_concat (file_and_directory_list_from_files (directory, files),
				       *pending_list);

	if (! view->details->loading || nemo_directory_are_all_files_seen (directory)) {
		schedule_timeout_display_of_pending_files (view, view->details->update_interval);
	}
}

static void
remove_changes_timeout_callback (NemoView *view) 
{
	if (view->details->changes_timeout_id != 0) {
		g_source_remove (view->details->changes_timeout_id);
		view->details->changes_timeout_id = 0;
	}
}

static void
reset_update_interval (NemoView *view)
{
	view->details->update_interval = UPDATE_INTERVAL_MIN;
	remove_changes_timeout_callback (view);
	/* Reschedule a pending timeout to idle */
	if (view->details->display_pending_source_id != 0) {
		schedule_idle_display_of_pending_files (view);
	}
}

static gboolean
changes_timeout_callback (gpointer data)
{
	gint64 now;
	gint64 time_delta;
	gboolean ret;
	NemoView *view;
	view = NEMO_VIEW (data);

	g_object_ref (G_OBJECT (view));

	now = g_get_monotonic_time ();
	time_delta = now - view->details->last_queued;

	if (time_delta < UPDATE_INTERVAL_RESET*1000) {
		if (view->details->update_interval < UPDATE_INTERVAL_MAX &&
		    view->details->loading) {
			/* Increase */
			view->details->update_interval += UPDATE_INTERVAL_INC;
		}
		ret = TRUE;
	} else {
		/* Reset */
		reset_update_interval (view);
		ret = FALSE;
	}

	g_object_unref (G_OBJECT (view));

	return ret;
}

static void
schedule_changes (NemoView *view)
{
	/* Remember when the change was queued */
	view->details->last_queued = g_get_monotonic_time ();

	/* No need to schedule if there are already changes pending or during loading */
	if (view->details->changes_timeout_id != 0 ||
	    view->details->loading) {
		return;
	}

	view->details->changes_timeout_id = 
		g_timeout_add (UPDATE_INTERVAL_TIMEOUT_INTERVAL, changes_timeout_callback, view);
}

static void
files_added_callback (NemoDirectory *directory,
		      GList *files,
		      gpointer callback_data)
{
	NemoView *view;
	GtkWindow *window;
	char *uri;

	view = NEMO_VIEW (callback_data);

	window = nemo_view_get_containing_window (view);
	uri = nemo_view_get_uri (view);
	DEBUG_FILES (files, "Files added in window %p: %s",
		     window, uri ? uri : "(no directory)");
	g_free (uri);

	schedule_changes (view);

	queue_pending_files (view, directory, files, &view->details->new_added_files);

	/* The number of items could have changed */
	schedule_update_status (view);
}

static void
files_changed_callback (NemoDirectory *directory,
			GList *files,
			gpointer callback_data)
{
	NemoView *view;
	GtkWindow *window;
	char *uri;
	
	view = NEMO_VIEW (callback_data);

	window = nemo_view_get_containing_window (view);
	uri = nemo_view_get_uri (view);
	DEBUG_FILES (files, "Files changed in window %p: %s",
		     window, uri ? uri : "(no directory)");
	g_free (uri);

	schedule_changes (view);

	queue_pending_files (view, directory, files, &view->details->new_changed_files);
	
	/* The free space or the number of items could have changed */
	schedule_update_status (view);

	/* A change in MIME type could affect the Open with menu, for
	 * one thing, so we need to update menus when files change.
	 */
	schedule_update_menus (view);
}

static void
done_loading_callback (NemoDirectory *directory,
		       gpointer callback_data)
{
	NemoView *view;

	view = NEMO_VIEW (callback_data);
	
	process_new_files (view);
	if (g_hash_table_size (view->details->non_ready_files) == 0) {
		/* Unschedule a pending update and schedule a new one with the minimal
		 * update interval. This gives the view a short chance at gathering the
		 * (cached) deep counts.
		 */
		unschedule_display_of_pending_files (view);
		schedule_timeout_display_of_pending_files (view, UPDATE_INTERVAL_MIN);
	}
}

static void
load_error_callback (NemoDirectory *directory,
		     GError *error,
		     gpointer callback_data)
{
	NemoView *view;

	view = NEMO_VIEW (callback_data);

	/* FIXME: By doing a stop, we discard some pending files. Is
	 * that OK?
	 */
	nemo_view_stop_loading (view);

    nemo_window_back_or_forward (NEMO_WINDOW (view->details->window),
                                 TRUE, 0, FALSE);

	/* Emit a signal to tell subclasses that a load error has
	 * occurred, so they can handle it in the UI.
	 */
	g_signal_emit (view,
		       signals[LOAD_ERROR], 0, error);
}

static void
real_load_error (NemoView *view, GError *error)
{
	/* Report only one error per failed directory load (from the UI
	 * point of view, not from the NemoDirectory point of view).
	 * Otherwise you can get multiple identical errors caused by 
	 * unrelated code that just happens to try to iterate this
	 * directory.
	 */
	if (!view->details->reported_load_error) {
		nemo_report_error_loading_directory 
			(nemo_view_get_directory_as_file (view),
			 error,
			 nemo_view_get_containing_window (view));
	}
	view->details->reported_load_error = TRUE;
}

void
nemo_view_add_subdirectory (NemoView  *view,
				NemoDirectory*directory)
{
	NemoFileAttributes attributes;

	g_assert (!g_list_find (view->details->subdirectory_list, directory));
	
	nemo_directory_ref (directory);

	attributes =
		NEMO_FILE_ATTRIBUTES_FOR_ICON |
		NEMO_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
		NEMO_FILE_ATTRIBUTE_INFO |
		NEMO_FILE_ATTRIBUTE_LINK_INFO |
		NEMO_FILE_ATTRIBUTE_MOUNT |
		NEMO_FILE_ATTRIBUTE_EXTENSION_INFO;

	nemo_directory_file_monitor_add (directory,
					     &view->details->model,
					     view->details->show_hidden_files,
					     attributes,
					     files_added_callback, view);
	
	g_signal_connect
		(directory, "files_added",
		 G_CALLBACK (files_added_callback), view);
	g_signal_connect
		(directory, "files_changed",
		 G_CALLBACK (files_changed_callback), view);
	
	view->details->subdirectory_list = g_list_prepend (
							   view->details->subdirectory_list, directory);
}

void
nemo_view_remove_subdirectory (NemoView  *view,
				   NemoDirectory*directory)
{
	g_assert (g_list_find (view->details->subdirectory_list, directory));
	
	view->details->subdirectory_list = g_list_remove (
							  view->details->subdirectory_list, directory);

	g_signal_handlers_disconnect_by_func (directory,
					      G_CALLBACK (files_added_callback),
					      view);
	g_signal_handlers_disconnect_by_func (directory,
					      G_CALLBACK (files_changed_callback),
					      view);

	nemo_directory_file_monitor_remove (directory, &view->details->model);

	nemo_directory_unref (directory);
}

/**
 * nemo_view_get_loading:
 * @view: an #NemoView.
 *
 * Return value: #gboolean inicating whether @view is currently loaded.
 * 
 **/
gboolean
nemo_view_get_loading (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), FALSE);

	return view->details->loading;
}

GtkUIManager *
nemo_view_get_ui_manager (NemoView  *view)
{
	if (view->details->window == NULL) {
		return NULL;
	}
	return nemo_window_get_ui_manager (view->details->window);	
}

/**
 * nemo_view_get_model:
 *
 * Get the model for this NemoView.
 * @view: NemoView of interest.
 * 
 * Return value: NemoDirectory for this view.
 * 
 **/
NemoDirectory *
nemo_view_get_model (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), NULL);

	return view->details->model;
}

GdkAtom
nemo_view_get_copied_files_atom (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), GDK_NONE);
	
	return copied_files_atom;
}

static void
prepend_uri_one (gpointer data, gpointer callback_data)
{
	NemoFile *file;
	GList **result;
	
	g_assert (NEMO_IS_FILE (data));
	g_assert (callback_data != NULL);

	result = (GList **) callback_data;
	file = (NemoFile *) data;
	*result = g_list_prepend (*result, nemo_file_get_uri (file));
}

static void
offset_drop_points (GArray *relative_item_points,
		    int x_offset, int y_offset)
{
	guint index;

	if (relative_item_points == NULL) {
		return;
	}

	for (index = 0; index < relative_item_points->len; index++) {
		g_array_index (relative_item_points, GdkPoint, index).x += x_offset;
		g_array_index (relative_item_points, GdkPoint, index).y += y_offset;
	}
}

static void
nemo_view_create_links_for_files (NemoView *view, GList *files,
				      GArray *relative_item_points)
{
	GList *uris;
	char *dir_uri;
	CopyMoveDoneData *copy_move_done_data;
	g_assert (relative_item_points->len == 0
		  || g_list_length (files) == relative_item_points->len);
	
        g_assert (NEMO_IS_VIEW (view));
        g_assert (files != NULL);

	/* create a list of URIs */
	uris = NULL;
	g_list_foreach (files, prepend_uri_one, &uris);
	uris = g_list_reverse (uris);

        g_assert (g_list_length (uris) == g_list_length (files));

	/* offset the drop locations a bit so that we don't pile
	 * up the icons on top of each other
	 */
	offset_drop_points (relative_item_points,
			    DUPLICATE_HORIZONTAL_ICON_OFFSET,
			    DUPLICATE_VERTICAL_ICON_OFFSET);

        copy_move_done_data = pre_copy_move (view);
	dir_uri = nemo_view_get_backing_uri (view);
	nemo_file_operations_copy_move (uris, relative_item_points, dir_uri, GDK_ACTION_LINK, 
					    GTK_WIDGET (view), copy_move_done_callback, copy_move_done_data);
	g_free (dir_uri);
	g_list_free_full (uris, g_free);
}

static void
nemo_view_duplicate_selection (NemoView *view, GList *files,
				   GArray *relative_item_points)
{
	GList *uris;
	CopyMoveDoneData *copy_move_done_data;

        g_assert (NEMO_IS_VIEW (view));
        g_assert (files != NULL);
	g_assert (g_list_length (files) == relative_item_points->len
		  || relative_item_points->len == 0);

	/* create a list of URIs */
	uris = NULL;
	g_list_foreach (files, prepend_uri_one, &uris);
	uris = g_list_reverse (uris);

        g_assert (g_list_length (uris) == g_list_length (files));
        
	/* offset the drop locations a bit so that we don't pile
	 * up the icons on top of each other
	 */
	offset_drop_points (relative_item_points,
			    DUPLICATE_HORIZONTAL_ICON_OFFSET,
			    DUPLICATE_VERTICAL_ICON_OFFSET);

        copy_move_done_data = pre_copy_move (view);
	nemo_file_operations_copy_move (uris, relative_item_points, NULL, GDK_ACTION_COPY,
					    GTK_WIDGET (view), copy_move_done_callback, copy_move_done_data);
	g_list_free_full (uris, g_free);
}

/* special_link_in_selection
 * 
 * Return TRUE if one of our special links is in the selection.
 * Special links include the following: 
 *	 NEMO_DESKTOP_LINK_TRASH, NEMO_DESKTOP_LINK_HOME, NEMO_DESKTOP_LINK_MOUNT
 */
 
static gboolean
special_link_in_selection (NemoView *view)
{
	gboolean saw_link;
	GList *selection, *node;
	NemoFile *file;

	g_return_val_if_fail (NEMO_IS_VIEW (view), FALSE);

	saw_link = FALSE;

	selection = nemo_view_get_selection (NEMO_VIEW (view));

	for (node = selection; node != NULL; node = node->next) {
		file = NEMO_FILE (node->data);

		saw_link = NEMO_IS_DESKTOP_ICON_FILE (file);
		
		if (saw_link) {
			break;
		}
	}
	
	nemo_file_list_free (selection);
	
	return saw_link;
}

/* desktop_or_home_dir_in_selection
 * 
 * Return TRUE if either the desktop or the home directory is in the selection.
 */
 
static gboolean
desktop_or_home_dir_in_selection (NemoView *view)
{
	gboolean saw_desktop_or_home_dir;
	GList *selection, *node;
	NemoFile *file;

	g_return_val_if_fail (NEMO_IS_VIEW (view), FALSE);

	saw_desktop_or_home_dir = FALSE;

	selection = nemo_view_get_selection (NEMO_VIEW (view));

	for (node = selection; node != NULL; node = node->next) {
		file = NEMO_FILE (node->data);

		saw_desktop_or_home_dir =
			nemo_file_is_home (file)
			|| nemo_file_is_desktop_directory (file);
		
		if (saw_desktop_or_home_dir) {
			break;
		}
	}
	
	nemo_file_list_free (selection);
	
	return saw_desktop_or_home_dir;
}

/* directory_in_selection
 *
 * Return TRUE if selection contains a directory.
 */

static gboolean
directory_in_selection (NemoView *view)
{
    gboolean has_dir;
    GList *selection, *node;
    NemoFile *file;

    g_return_val_if_fail (NEMO_IS_VIEW (view), FALSE);

    has_dir = FALSE;

    selection = nemo_view_get_selection (NEMO_VIEW (view));

    for (node = selection; node != NULL; node = node->next) {
        file = NEMO_FILE (node->data);

        has_dir = nemo_file_is_directory (file);
        if (has_dir) {
            break;
        }
    }
    nemo_file_list_free (selection);

    return has_dir;
}

static void
trash_or_delete_done_cb (GHashTable *debuting_uris,
			 gboolean user_cancel,
			 NemoView *view)
{
	if (user_cancel) {
		view->details->selection_was_removed = FALSE;
	}
}

static void
trash_or_delete_files (GtkWindow *parent_window,
		       const GList *files,
		       gboolean delete_if_all_already_in_trash,
		       NemoView *view)
{
	GList *locations;
	const GList *node;
	
	locations = NULL;
	for (node = files; node != NULL; node = node->next) {
		locations = g_list_prepend (locations,
					    nemo_file_get_location ((NemoFile *) node->data));
	}
	
	locations = g_list_reverse (locations);

	nemo_file_operations_trash_or_delete (locations,
						  parent_window,
						  (NemoDeleteCallback) trash_or_delete_done_cb,
						  view);
	g_list_free_full (locations, g_object_unref);
}

static gboolean
can_rename_file (NemoView *view, NemoFile *file)
{
	return nemo_file_can_rename (file);
}

gboolean
nemo_view_get_is_renaming (NemoView *view)
{
	return view->details->is_renaming;
}

void
nemo_view_set_is_renaming (NemoView *view,
			       gboolean      is_renaming)
{
	view->details->is_renaming = is_renaming;
}

static void
start_renaming_file (NemoView *view,
		     NemoFile *file,
		     gboolean select_all)
{
	view->details->is_renaming = TRUE;

	if (file !=  NULL) {
		nemo_view_select_file (view, file);
	}
}

static void
update_context_menu_position_from_event (NemoView *view,
					 GdkEventButton  *event)
{
	g_return_if_fail (NEMO_IS_VIEW (view));

	if (event != NULL) {
		view->details->context_menu_position.x = event->x;
		view->details->context_menu_position.y = event->y;
	} else {
		view->details->context_menu_position.x = -1;
		view->details->context_menu_position.y = -1;
	}
}

/* handle the open command */

static void
open_one_in_new_window (gpointer data, gpointer callback_data)
{
	g_assert (NEMO_IS_FILE (data));
	g_assert (NEMO_IS_VIEW (callback_data));

	nemo_view_activate_file (NEMO_VIEW (callback_data),
				     NEMO_FILE (data),
				     NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW);
}

NemoFile *
nemo_view_get_directory_as_file (NemoView *view)
{
	g_assert (NEMO_IS_VIEW (view));

	return view->details->directory_as_file; 
}

static void
open_with_launch_application_callback (GtkAction *action,
				       gpointer callback_data)
{
	ApplicationLaunchParameters *launch_parameters;
	
	launch_parameters = (ApplicationLaunchParameters *) callback_data;
	nemo_launch_application 
		(launch_parameters->application,
		 launch_parameters->files,
		 nemo_view_get_containing_window (launch_parameters->directory_view));
}

static char *
escape_action_path (const char *action_path)
{
	GString *s;

	if (action_path == NULL) {
		return NULL;
	}
	
	s = g_string_sized_new (strlen (action_path) + 2);

	while (*action_path != 0) {
		switch (*action_path) {
		case '\\':
			g_string_append (s, "\\\\");
			break;
		case '&':
			g_string_append (s, "\\a");
			break;
		case '"':
			g_string_append (s, "\\q");
			break;
		default:
			g_string_append_c (s, *action_path);
		}

		action_path ++;
	}
	return g_string_free (s, FALSE);
}


static void
add_submenu (GtkUIManager *ui_manager,
	     GtkActionGroup *action_group,
	     guint merge_id,
	     const char *parent_path,
	     const char *uri,
	     const char *label,
	     GdkPixbuf *pixbuf,
	     gboolean add_action)
{
	char *escaped_label;
	char *action_name;
	char *submenu_name;
	char *escaped_submenu_name;
	GtkAction *action;
	
	if (parent_path != NULL) {
		action_name = nemo_escape_action_name (uri, "submenu_");
		submenu_name = g_path_get_basename (uri);
		escaped_submenu_name = escape_action_path (submenu_name);
		escaped_label = eel_str_double_underscores (label);

		if (add_action) {
			action = gtk_action_new (action_name,
						 escaped_label,
						 NULL,
						 NULL);
			if (pixbuf != NULL) {
				g_object_set_data_full (G_OBJECT (action), "menu-icon",
							g_object_ref (pixbuf),
							g_object_unref);
			}
			
			g_object_set (action, "hide-if-empty", FALSE, NULL);
			
			gtk_action_group_add_action (action_group,
						     action);
			g_object_unref (action);
		}

		gtk_ui_manager_add_ui (ui_manager,
				       merge_id,
				       parent_path,
				       escaped_submenu_name,
				       action_name,
				       GTK_UI_MANAGER_MENU,
				       FALSE);
		g_free (action_name);
		g_free (escaped_label);
		g_free (submenu_name);
		g_free (escaped_submenu_name);
	}
}

static void
add_application_to_open_with_menu (NemoView *view,
				   GAppInfo *application, 
				   GList *files,
				   int index,
				   const char *menu_placeholder,
				   const char *popup_placeholder,
				   const gboolean submenu)
{
	ApplicationLaunchParameters *launch_parameters;
	char *tip;
	char *label;
	char *action_name;
	char *escaped_app;
	char *path;
	GtkAction *action;
	GIcon *app_icon;
	GtkWidget *menuitem;

	launch_parameters = application_launch_parameters_new 
		(application, files, view);
	escaped_app = eel_str_double_underscores (g_app_info_get_name (application));
	if (submenu)
		label = g_strdup_printf ("%s", escaped_app);
	else
		label = g_strdup_printf (_("Open With %s"), escaped_app);

	tip = g_strdup_printf (ngettext ("Use \"%s\" to open the selected item",
					 "Use \"%s\" to open the selected items",
					 g_list_length (files)),
			       escaped_app);
	g_free (escaped_app);

	action_name = g_strdup_printf ("open_with_%d", index);
	
	action = gtk_action_new (action_name,
				 label,
				 tip,
				 NULL);

	app_icon = g_app_info_get_icon (application);
	if (app_icon != NULL) {
		g_object_ref (app_icon);
	} else {
		app_icon = g_themed_icon_new ("application-x-executable");
	}

	gtk_action_set_gicon (action, app_icon);
	g_object_unref (app_icon);

	g_signal_connect_data (action, "activate",
			       G_CALLBACK (open_with_launch_application_callback),
			       launch_parameters, 
			       (GClosureNotify)application_launch_parameters_free, 0);

	gtk_action_group_add_action (view->details->open_with_action_group,
				     action);
	g_object_unref (action);
	
	gtk_ui_manager_add_ui (nemo_window_get_ui_manager (view->details->window),
			       view->details->open_with_merge_id,
			       menu_placeholder,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	path = g_strdup_printf ("%s/%s", menu_placeholder, action_name);
	menuitem = gtk_ui_manager_get_widget (
					      nemo_window_get_ui_manager (view->details->window),
					      path);
	gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (menuitem), TRUE);
	g_free (path);

	gtk_ui_manager_add_ui (nemo_window_get_ui_manager (view->details->window),
			       view->details->open_with_merge_id,
			       popup_placeholder,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	path = g_strdup_printf ("%s/%s", popup_placeholder, action_name);
	menuitem = gtk_ui_manager_get_widget (
					      nemo_window_get_ui_manager (view->details->window),
					      path);
	gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (menuitem), TRUE);

	g_free (path);
	g_free (action_name);
	g_free (label);
	g_free (tip);
}

static void
get_x_content_async_callback (const char **content,
			      gpointer user_data)
{
	NemoView *view;

	view = NEMO_VIEW (user_data);

	if (view->details->window != NULL) {
		schedule_update_menus (view);
	}
	g_object_unref (view);
}

static void
add_x_content_apps (NemoView *view, NemoFile *file, GList **applications)
{
	GMount *mount;
	char **x_content_types;
	unsigned int n;

	g_return_if_fail (applications != NULL);

	mount = nemo_file_get_mount (file);

	if (mount == NULL) {
		return;
	}
	
	x_content_types = nemo_get_cached_x_content_types_for_mount (mount);
	if (x_content_types != NULL) {
		for (n = 0; x_content_types[n] != NULL; n++) {
			char *x_content_type = x_content_types[n];
			GList *app_info_for_x_content_type;
			
			app_info_for_x_content_type = g_app_info_get_all_for_type (x_content_type);
			*applications = g_list_concat (*applications, app_info_for_x_content_type);
		}
		g_strfreev (x_content_types);
	} else {
		nemo_get_x_content_types_for_mount_async (mount,
							      get_x_content_async_callback,
							      NULL,
							      g_object_ref (view));
		
	}

	g_object_unref (mount);
}

static void
reset_open_with_menu (NemoView *view, GList *selection)
{
	GList *applications, *node;
	NemoFile *file;
	gboolean submenu_visible, filter_default;
	int num_applications;
	int index;
	gboolean other_applications_visible;
	gboolean open_with_chooser_visible;
	GtkUIManager *ui_manager;
	GtkAction *action;
	GAppInfo *default_app;

	/* Clear any previous inserted items in the applications and viewers placeholders */

	ui_manager = nemo_window_get_ui_manager (view->details->window);
	nemo_ui_unmerge_ui (ui_manager,
				&view->details->open_with_merge_id,
				&view->details->open_with_action_group);
	
	nemo_ui_prepare_merge_ui (ui_manager,
				      "OpenWithGroup",
				      &view->details->open_with_merge_id,
				      &view->details->open_with_action_group);

	other_applications_visible = (selection != NULL);
	filter_default = (selection != NULL);

	for (node = selection; node != NULL; node = node->next) {

		file = NEMO_FILE (node->data);

		other_applications_visible &= (!nemo_mime_file_opens_in_view (file) ||
                                        nemo_file_is_directory (file));
	}

	default_app = NULL;
	if (filter_default) {
		default_app = nemo_mime_get_default_application_for_files (selection);
	}

	applications = NULL;
	if (other_applications_visible) {
		applications = nemo_mime_get_applications_for_files (selection);
	}

	if (g_list_length (selection) == 1) {
		add_x_content_apps (view, NEMO_FILE (selection->data), &applications);
	}


	num_applications = g_list_length (applications);

	if (file_list_all_are_folders (selection)) {
		submenu_visible = (num_applications > 0);
	} else {
		submenu_visible = (num_applications > 1);
	}
	
	for (node = applications, index = 0; node != NULL; node = node->next, index++) {
		GAppInfo *application;
		char *menu_path;
		char *popup_path;
		
		application = node->data;

		if (default_app != NULL && g_app_info_equal (default_app, application)) {
			continue;
		}

		if (submenu_visible) {
			menu_path = NEMO_VIEW_MENU_PATH_APPLICATIONS_SUBMENU_PLACEHOLDER;
			popup_path = NEMO_VIEW_POPUP_PATH_APPLICATIONS_SUBMENU_PLACEHOLDER;
		} else {
			menu_path = NEMO_VIEW_MENU_PATH_APPLICATIONS_PLACEHOLDER;
			popup_path = NEMO_VIEW_POPUP_PATH_APPLICATIONS_PLACEHOLDER;
		}

		gtk_ui_manager_add_ui (nemo_window_get_ui_manager (view->details->window),
				       view->details->open_with_merge_id,
				       menu_path,
				       "separator",
				       NULL,
				       GTK_UI_MANAGER_SEPARATOR,
				       FALSE);
				       
		add_application_to_open_with_menu (view, 
						   node->data, 
						   selection, 
						   index, 
						   menu_path, popup_path, submenu_visible);
	}
	g_list_free_full (applications, g_object_unref);
	if (default_app != NULL) {
		g_object_unref (default_app);
	}

	open_with_chooser_visible = other_applications_visible &&
		g_list_length (selection) == 1;

	if (submenu_visible) {
		action = gtk_action_group_get_action (view->details->dir_action_group,
						      NEMO_ACTION_OTHER_APPLICATION1);
		gtk_action_set_visible (action, open_with_chooser_visible);
		action = gtk_action_group_get_action (view->details->dir_action_group,
						      NEMO_ACTION_OTHER_APPLICATION2);
		gtk_action_set_visible (action, FALSE);
	} else {
		action = gtk_action_group_get_action (view->details->dir_action_group,
						      NEMO_ACTION_OTHER_APPLICATION1);
		gtk_action_set_visible (action, FALSE);
		action = gtk_action_group_get_action (view->details->dir_action_group,
						      NEMO_ACTION_OTHER_APPLICATION2);
		gtk_action_set_visible (action, open_with_chooser_visible);
	}
}

static void
move_copy_selection_to_location (NemoView *view,
                 int copy_action,
                 char *target_uri)
{
    GList *selection, *uris, *l;

    selection = nemo_view_get_selection_for_file_transfer (view);
    if (selection == NULL) {
        return;
    }

    uris = NULL;
    for (l = selection; l != NULL; l = l->next) {
        uris = g_list_prepend (uris,
                       nemo_file_get_uri ((NemoFile *) l->data));
    }
    uris = g_list_reverse (uris);

    nemo_view_move_copy_items (view, uris, NULL, target_uri,
                       copy_action,
                       0, 0);

    g_list_free_full (uris, g_free);
    nemo_file_list_free (selection);
}

static void
action_move_bookmark_callback (GtkAction *action, gpointer callback_data)
{
    NemoView *view;
    BookmarkCallbackData *data;

    data = (BookmarkCallbackData *) callback_data;
    view = NEMO_VIEW(data->view);
    move_copy_selection_to_location (view, GDK_ACTION_MOVE, data->dest_uri);
}

static void
action_copy_bookmark_callback (GtkAction *action, gpointer callback_data)
{
    NemoView *view;
    BookmarkCallbackData *data;

    data = (BookmarkCallbackData *) callback_data;
    view = NEMO_VIEW(data->view);
    move_copy_selection_to_location (view, GDK_ACTION_COPY, data->dest_uri);
}

static void
setup_bookmark_action(      char *action_name,
                      const char *bookmark_name,
                           GIcon *icon,
                            char *mount_uri,
                    GtkUIManager *ui_manager,
                         gboolean move,
                  GtkActionGroup *action_group,
                             gint merge_id,
                            char *path,
                        NemoView *view)
{

    GtkAction *action;
    gchar *full_path;
    action = gtk_action_new (action_name,
             bookmark_name,
             NULL,
             NULL);
    gtk_action_set_gicon (action, icon);

    if (move) {
        g_signal_connect_data (action, "activate",
               G_CALLBACK (action_move_bookmark_callback),
               bookmark_callback_data_new(view, mount_uri),
               (GClosureNotify)bookmark_callback_data_free, 0);
    } else {
        g_signal_connect_data (action, "activate",
               G_CALLBACK (action_copy_bookmark_callback),
               bookmark_callback_data_new(view, mount_uri),
               (GClosureNotify)bookmark_callback_data_free, 0);
    }

    gtk_action_group_add_action (action_group, action);

    gtk_ui_manager_add_ui ( ui_manager,
                            merge_id,
                            path,
                            action_name,
                            action_name,
                            GTK_UI_MANAGER_MENUITEM,
                            FALSE);

    full_path = g_strdup_printf ("%s/%s", path, action_name);

    g_free (full_path);
    g_free (action_name);
}

static void
add_bookmark_to_action (NemoView *view, const gchar *bookmark_name, GIcon *icon, gchar *mount_uri, gint index)
{
    GtkUIManager *ui_manager;
    ui_manager = nemo_window_get_ui_manager (view->details->window);

    setup_bookmark_action(g_strdup_printf ("BM_MOVETO_POPUP_%d", index),
                                            bookmark_name,
                                            icon,
                                            mount_uri,
                                            ui_manager,
                                            TRUE,
                                            view->details->copy_move_action_groups[0],
                                            view->details->copy_move_merge_ids[0],
                                            NEMO_VIEW_POPUP_PATH_BOOKMARK_MOVETO_ENTRIES_PLACEHOLDER,
                                            view);

    setup_bookmark_action(g_strdup_printf ("BM_COPYTO_POPUP_%d", index),
                                            bookmark_name,
                                            icon,
                                            mount_uri,
                                            ui_manager,
                                            FALSE,
                                            view->details->copy_move_action_groups[1],
                                            view->details->copy_move_merge_ids[1],
                                            NEMO_VIEW_POPUP_PATH_BOOKMARK_COPYTO_ENTRIES_PLACEHOLDER,
                                            view);

    setup_bookmark_action(g_strdup_printf ("BM_MOVETO_MENU_%d", index),
                                            bookmark_name,
                                            icon,
                                            mount_uri,
                                            ui_manager,
                                            TRUE,
                                            view->details->copy_move_action_groups[2],
                                            view->details->copy_move_merge_ids[2],
                                            NEMO_VIEW_MENU_PATH_BOOKMARK_MOVETO_ENTRIES_PLACEHOLDER,
                                            view);

    setup_bookmark_action(g_strdup_printf ("BM_COPYTO_MENU_%d", index),
                                            bookmark_name,
                                            icon,
                                            mount_uri,
                                            ui_manager,
                                            FALSE,
                                            view->details->copy_move_action_groups[3],
                                            view->details->copy_move_merge_ids[3],
                                            NEMO_VIEW_MENU_PATH_BOOKMARK_COPYTO_ENTRIES_PLACEHOLDER,
                                            view);
}

static void
add_place_to_action (NemoView *view, const gchar *bookmark_name, GIcon *icon, gchar *mount_uri, gint index)
{
    GtkUIManager *ui_manager;
    ui_manager = nemo_window_get_ui_manager (view->details->window);

    setup_bookmark_action(g_strdup_printf ("PLACE_MOVETO_POPUP_%d", index),
                                            bookmark_name,
                                            icon,
                                            mount_uri,
                                            ui_manager,
                                            TRUE,
                                            view->details->copy_move_action_groups[0],
                                            view->details->copy_move_merge_ids[0],
                                            NEMO_VIEW_POPUP_PATH_PLACES_MOVETO_ENTRIES_PLACEHOLDER,
                                            view);

    setup_bookmark_action(g_strdup_printf ("PLACE_COPYTO_POPUP_%d", index),
                                            bookmark_name,
                                            icon,
                                            mount_uri,
                                            ui_manager,
                                            FALSE,
                                            view->details->copy_move_action_groups[1],
                                            view->details->copy_move_merge_ids[1],
                                            NEMO_VIEW_POPUP_PATH_PLACES_COPYTO_ENTRIES_PLACEHOLDER,
                                            view);

    setup_bookmark_action(g_strdup_printf ("PLACE_MOVETO_MENU_%d", index),
                                            bookmark_name,
                                            icon,
                                            mount_uri,
                                            ui_manager,
                                            TRUE,
                                            view->details->copy_move_action_groups[2],
                                            view->details->copy_move_merge_ids[2],
                                            NEMO_VIEW_MENU_PATH_PLACES_MOVETO_ENTRIES_PLACEHOLDER,
                                            view);

    setup_bookmark_action(g_strdup_printf ("PLACE_COPYTO_MENU_%d", index),
                                            bookmark_name,
                                            icon,
                                            mount_uri,
                                            ui_manager,
                                            FALSE,
                                            view->details->copy_move_action_groups[3],
                                            view->details->copy_move_merge_ids[3],
                                            NEMO_VIEW_MENU_PATH_PLACES_COPYTO_ENTRIES_PLACEHOLDER,
                                            view);
}


static void
reset_move_copy_to_menu (NemoView *view)
{
    NemoBookmark *bookmark;
    NemoFile *file;
    int bookmark_count, index;
    GtkUIManager *ui_manager;
    GFile *root;
    const gchar *bookmark_name;
    GIcon *icon;
    char *mount_uri;

    ui_manager = nemo_window_get_ui_manager (view->details->window);

    int i;

    for (i = 0; i < 4; i++) {
        nemo_ui_unmerge_ui (ui_manager,
                &view->details->copy_move_merge_ids[i],
                &view->details->copy_move_action_groups[i]);
        gchar *id = g_strdup_printf ("MoveCopyMenuGroup_%d", i);
        nemo_ui_prepare_merge_ui (ui_manager,
                                  id,
                                  &view->details->copy_move_merge_ids[i],
                                  &view->details->copy_move_action_groups[i]);
        g_free (id);
    }

    if (view->details->showing_bookmarks_in_to_menus) {
        bookmark_count = nemo_bookmark_list_length (view->details->bookmarks);
        for (index = 0; index < bookmark_count; ++index) {
            bookmark = nemo_bookmark_list_item_at (view->details->bookmarks, index);

            if (nemo_bookmark_uri_known_not_to_exist (bookmark)) {
                continue;
            }

            root = nemo_bookmark_get_location (bookmark);
            file = nemo_file_get (root);

            nemo_file_unref (file);

            bookmark_name = nemo_bookmark_get_name (bookmark);
            icon = nemo_bookmark_get_icon (bookmark);
            mount_uri = nemo_bookmark_get_uri (bookmark);

            add_bookmark_to_action (view,
                                    bookmark_name,
                                    icon,
                                    mount_uri,
                                    index);

            g_object_unref (root);
            g_object_unref (icon);
            g_free (mount_uri);
        }
    }

    if (view->details->showing_places_in_to_menus) {

        GList *mounts, *l, *ll, *drives, *volumes;
        GVolumeMonitor *volume_monitor;
        GMount *mount;
        GVolume *volume;
        GDrive *drive;
        GList *network_mounts = NULL;
        GList *network_volumes = NULL;
        gchar *name, *identifier;
        index = 0;

        /* add mounts that has no volume (/etc/mtab mounts, ftp, sftp,...) */
        volume_monitor = g_volume_monitor_get ();
        mounts = g_volume_monitor_get_mounts (volume_monitor);

        for (l = mounts; l != NULL; l = l->next) {
            mount = l->data;
            if (g_mount_is_shadowed (mount)) {
                g_object_unref (mount);
                continue;
            }
            volume = g_mount_get_volume (mount);
            if (volume != NULL) {
                    g_object_unref (volume);
                g_object_unref (mount);
                continue;
            }
            root = g_mount_get_default_location (mount);

            if (!g_file_is_native (root)) {
                gboolean really_network = TRUE;
                gchar *path = g_file_get_path (root);
                gchar *escaped1 = g_uri_unescape_string (path, "");
                gchar *escaped2 = g_uri_unescape_string (escaped1, "");
                gchar *ptr = g_strrstr (escaped2, "file://");
                if (ptr != NULL) {
                    GFile *actual_file = g_file_new_for_uri (ptr);
                    if (g_file_is_native(actual_file)) {
                        really_network = FALSE;
                    }
                    g_object_unref(actual_file);
                }
                g_free (path);
                g_free (escaped1);
                g_free (escaped2);
                if (really_network) {
                    network_mounts = g_list_prepend (network_mounts, mount);
                    g_object_unref (root);
                    continue;
                }
            }

            icon = g_mount_get_icon (mount);
            mount_uri = g_file_get_uri (root);
            name = g_mount_get_name (mount);

            add_place_to_action (view,
                                 name,
                                 icon,
                                 mount_uri,
                                 index);

            g_object_unref (root);
            g_object_unref (mount);
            g_object_unref (icon);
            g_free (name);
            g_free (mount_uri);

            index++;
        }
        g_list_free (mounts);

        /* first go through all connected drives */
        drives = g_volume_monitor_get_connected_drives (volume_monitor);

        for (l = drives; l != NULL; l = l->next) {
            drive = l->data;

            volumes = g_drive_get_volumes (drive);
            if (volumes != NULL) {
                for (ll = volumes; ll != NULL; ll = ll->next) {
                    volume = ll->data;
                    identifier = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);

                    if (g_strcmp0 (identifier, "network") == 0) {
                        g_free (identifier);
                        network_volumes = g_list_prepend (network_volumes, volume);
                        continue;
                    }
                    g_free (identifier);

                    mount = g_volume_get_mount (volume);
                    if (mount != NULL) {
                        /* Show mounted volume in the sidebar */
                        icon = g_mount_get_icon (mount);
                        root = g_mount_get_default_location (mount);
                        mount_uri = g_file_get_uri (root);
                        name = g_mount_get_name (mount);

                        add_place_to_action (view,
                                             name,
                                             icon,
                                             mount_uri,
                                             index);

                        g_object_unref (root);
                        g_object_unref (mount);
                        g_object_unref (icon);
                        g_free (name);
                        g_free (mount_uri);
                        index++;
                    }
                    g_object_unref (volume);
                }
                g_list_free (volumes);
            }
            g_object_unref (drive);
        }
        g_list_free (drives);

        /* add all volumes that is not associated with a drive */
        volumes = g_volume_monitor_get_volumes (volume_monitor);
        for (l = volumes; l != NULL; l = l->next) {
            volume = l->data;
            drive = g_volume_get_drive (volume);
            if (drive != NULL) {
                    g_object_unref (volume);
                g_object_unref (drive);
                continue;
            }

            identifier = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);

            if (g_strcmp0 (identifier, "network") == 0) {
                g_free (identifier);
                network_volumes = g_list_prepend (network_volumes, volume);
                continue;
            }
            g_free (identifier);

            mount = g_volume_get_mount (volume);
            if (mount != NULL) {
                icon = g_mount_get_icon (mount);
                root = g_mount_get_default_location (mount);
                mount_uri = g_file_get_uri (root);

                g_object_unref (root);
                name = g_mount_get_name (mount);

                add_place_to_action (view,
                                     name,
                                     icon,
                                     mount_uri,
                                     index);

                g_object_unref (mount);
                g_object_unref (icon);
                g_free (name);
                g_free (mount_uri);
                index++;
            }
            g_object_unref (volume);
        }
        g_list_free (volumes);
        g_object_unref (volume_monitor);

        network_volumes = g_list_reverse (network_volumes);
        for (l = network_volumes; l != NULL; l = l->next) {
            volume = l->data;
            mount = g_volume_get_mount (volume);

            if (mount != NULL) {
                network_mounts = g_list_prepend (network_mounts, mount);
                continue;
            }
        }

        g_list_free_full (network_volumes, g_object_unref);

        network_mounts = g_list_reverse (network_mounts);
        for (l = network_mounts; l != NULL; l = l->next) {
            mount = l->data;
            root = g_mount_get_default_location (mount);
            icon = g_mount_get_icon (mount);
            mount_uri = g_file_get_uri (root);
            name = g_mount_get_name (mount);

            add_place_to_action (view,
                                 name,
                                 icon,
                                 mount_uri,
                                 index);

            g_object_unref (root);
            g_object_unref (icon);
            g_free (name);
            g_free (mount_uri);
            index++;
        }

        g_list_free_full (network_mounts, g_object_unref);
    }
}

static void
disconnect_bookmark (gpointer data, gpointer callback_data)
{
    GtkAction *action = GTK_ACTION (data);
    g_signal_handlers_disconnect_matched (action,
                          G_SIGNAL_MATCH_FUNC, 0, 0,
                          NULL, action_move_bookmark_callback, NULL);
    g_signal_handlers_disconnect_matched (action,
                          G_SIGNAL_MATCH_FUNC, 0, 0,
                          NULL, action_copy_bookmark_callback, NULL);
}

static void
disconnect_bookmark_signals (NemoView *view)
{
    int i;
    GList *list;
    GtkActionGroup *group;
    for (i = 0; i < 4; i++) {
        group = GTK_ACTION_GROUP (view->details->copy_move_action_groups[i]);
        list = gtk_action_group_list_actions (group);
        g_list_foreach (list, disconnect_bookmark, NULL);
        g_list_free (list);
    }
}

static GList *
get_all_extension_menu_items (GtkWidget *window,
			      GList *selection)
{
	GList *items;
	GList *providers;
	GList *l;
	
	providers = nemo_module_get_extensions_for_type (NEMO_TYPE_MENU_PROVIDER);
	items = NULL;

	for (l = providers; l != NULL; l = l->next) {
		NemoMenuProvider *provider;
		GList *file_items;
		
		provider = NEMO_MENU_PROVIDER (l->data);
		file_items = nemo_menu_provider_get_file_items (provider,
								    window,
								    selection);
		items = g_list_concat (items, file_items);		
	}

	nemo_module_extension_list_free (providers);

	return items;
}

typedef struct 
{
	NemoMenuItem *item;
	NemoView *view;
	GList *selection;
	GtkAction *action;
} ExtensionActionCallbackData;


static void
extension_action_callback_data_free (ExtensionActionCallbackData *data)
{
	g_object_unref (data->item);
	nemo_file_list_free (data->selection);
	
	g_free (data);
}

static gboolean
search_in_menu_items (GList* items, const char *item_name)
{
	GList* list;
	
	for (list = items; list != NULL; list = list->next) {
		NemoMenu* menu;
		char *name;
		
		g_object_get (list->data, "name", &name, NULL);
		if (strcmp (name, item_name) == 0) {
			g_free (name);
			return TRUE;
		}
		g_free (name);

		menu = NULL;
		g_object_get (list->data, "menu", &menu, NULL);
		if (menu != NULL) {
			gboolean ret;
			GList* submenus;

			submenus = nemo_menu_get_items (menu);
			ret = search_in_menu_items (submenus, item_name);
			nemo_menu_item_list_free (submenus);
			g_object_unref (menu);
			if (ret) {
				return TRUE;
			}
		}
	}
	return FALSE;
}

static void
extension_action_callback (GtkAction *action,
			   gpointer callback_data)
{
	ExtensionActionCallbackData *data;
	char *item_name;
	gboolean is_valid;
	GList *l;
	GList *items;

	data = callback_data;

	/* Make sure the selected menu item is valid for the final sniffed
	 * mime type */
	g_object_get (data->item, "name", &item_name, NULL);
	items = get_all_extension_menu_items (gtk_widget_get_toplevel (GTK_WIDGET (data->view)), 
					      data->selection);
	
	is_valid = search_in_menu_items (items, item_name);

	for (l = items; l != NULL; l = l->next) {
		g_object_unref (l->data);
	}
	g_list_free (items);
	
	g_free (item_name);

	if (is_valid) {
		nemo_menu_item_activate (data->item);
	}
}

static GdkPixbuf *
get_menu_icon_for_file (NemoFile *file)
{
	NemoIconInfo *info;
	GdkPixbuf *pixbuf;
	int size;

	size = nemo_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
	
	info = nemo_file_get_icon (file, size, 0);
	pixbuf = nemo_icon_info_get_pixbuf_nodefault_at_size (info, size);
	g_object_unref (info);
	
	return pixbuf;
}

static GtkAction *
add_extension_action_for_files (NemoView *view, 
				NemoMenuItem *item,
				GList *files)
{
	char *name, *label, *tip, *icon;
	gboolean sensitive, priority;
	GtkAction *action;
	GdkPixbuf *pixbuf;
	ExtensionActionCallbackData *data;
	
	g_object_get (G_OBJECT (item), 
		      "name", &name, "label", &label, 
		      "tip", &tip, "icon", &icon,
		      "sensitive", &sensitive,
		      "priority", &priority,
		      NULL);

	action = gtk_action_new (name,
				 label,
				 tip,
				 NULL);

	if (icon != NULL) {
		pixbuf = nemo_ui_get_menu_icon (icon);
		if (pixbuf != NULL) {
			gtk_action_set_gicon (action, G_ICON (pixbuf));
			g_object_unref (pixbuf);
		}
	}

	gtk_action_set_sensitive (action, sensitive);
	g_object_set (action, "is-important", priority, NULL);

	data = g_new0 (ExtensionActionCallbackData, 1);
	data->item = g_object_ref (item);
	data->view = view;
	data->selection = nemo_file_list_copy (files);
	data->action = action;

	g_signal_connect_data (action, "activate",
			       G_CALLBACK (extension_action_callback),
			       data,
			       (GClosureNotify)extension_action_callback_data_free, 0);
		
	gtk_action_group_add_action (view->details->extensions_menu_action_group,
				     GTK_ACTION (action));
	g_object_unref (action);
	
	g_free (name);
	g_free (label);
	g_free (tip);
	g_free (icon);

	return action;
}

static void
add_extension_menu_items (NemoView *view,
			  GList *files,
			  GList *menu_items,
			  const char *subdirectory)
{
	GtkUIManager *ui_manager;
	GList *l;

	ui_manager = nemo_window_get_ui_manager (view->details->window);
	
	for (l = menu_items; l; l = l->next) {
		NemoMenuItem *item;
		NemoMenu *menu;
		GtkAction *action;
		char *path;
		
		item = NEMO_MENU_ITEM (l->data);
		
		g_object_get (item, "menu", &menu, NULL);
		
		action = add_extension_action_for_files (view, item, files);
		
		path = g_build_path ("/", NEMO_VIEW_POPUP_PATH_EXTENSION_ACTIONS, subdirectory, NULL);
		gtk_ui_manager_add_ui (ui_manager,
				       view->details->extensions_menu_merge_id,
				       path,
				       gtk_action_get_name (action),
				       gtk_action_get_name (action),
				       (menu != NULL) ? GTK_UI_MANAGER_MENU : GTK_UI_MANAGER_MENUITEM,
				       FALSE);
		g_free (path);

		path = g_build_path ("/", NEMO_VIEW_MENU_PATH_EXTENSION_ACTIONS_PLACEHOLDER, subdirectory, NULL);
		gtk_ui_manager_add_ui (ui_manager,
				       view->details->extensions_menu_merge_id,
				       path,
				       gtk_action_get_name (action),
				       gtk_action_get_name (action),
				       (menu != NULL) ? GTK_UI_MANAGER_MENU : GTK_UI_MANAGER_MENUITEM,
				       FALSE);
		g_free (path);

		/* recursively fill the menu */		       
		if (menu != NULL) {
			char *subdir;
			GList *children;
			
			children = nemo_menu_get_items (menu);
			
			subdir = g_build_path ("/", subdirectory, gtk_action_get_name (action), NULL);
			add_extension_menu_items (view,
						  files,
						  children,
						  subdir);

			nemo_menu_item_list_free (children);
			g_free (subdir);
		}			
	}
}

static void
reset_extension_actions_menu (NemoView *view, GList *selection)
{
	GList *items;
	GtkUIManager *ui_manager;
	
	/* Clear any previous inserted items in the extension actions placeholder */
	ui_manager = nemo_window_get_ui_manager (view->details->window);

	nemo_ui_unmerge_ui (ui_manager,
				&view->details->extensions_menu_merge_id,
				&view->details->extensions_menu_action_group);
	
	nemo_ui_prepare_merge_ui (ui_manager,
				      "DirExtensionsMenuGroup",
				      &view->details->extensions_menu_merge_id,
				      &view->details->extensions_menu_action_group);

	items = get_all_extension_menu_items (gtk_widget_get_toplevel (GTK_WIDGET (view)), 
					      selection);
	if (items != NULL) {
		add_extension_menu_items (view, selection, items, "");

		g_list_foreach (items, (GFunc) g_object_unref, NULL);
		g_list_free (items);
	}
}

static char *
change_to_view_directory (NemoView *view)
{
	char *path;
	char *old_path;

	old_path = g_get_current_dir ();

	path = get_view_directory (view);

	/* FIXME: What to do about non-local directories? */
	if (path != NULL) {
		g_chdir (path);
	}

	g_free (path);

	return old_path;
}

static char **
get_file_names_as_parameter_array (GList *selection,
				   NemoDirectory *model)
{
	NemoFile *file;
	char **parameters;
	GList *node;
	GFile *file_location;
	GFile *model_location;
	int i;

	if (model == NULL) {
		return NULL;
	}

	parameters = g_new (char *, g_list_length (selection) + 1);

	model_location = nemo_directory_get_location (model);

	for (node = selection, i = 0; node != NULL; node = node->next, i++) {
		file = NEMO_FILE (node->data);

		if (!nemo_file_is_local (file)) {
			parameters[i] = NULL;
			g_strfreev (parameters);
			return NULL;
		}

		file_location = nemo_file_get_location (NEMO_FILE (node->data));
		parameters[i] = g_file_get_relative_path (model_location, file_location);
		if (parameters[i] == NULL) {
			parameters[i] = g_file_get_path (file_location);
		}
		g_object_unref (file_location);
	}

	g_object_unref (model_location);

	parameters[i] = NULL;
	return parameters;
}

static char *
get_file_paths_or_uris_as_newline_delimited_string (GList *selection, gboolean get_paths)
{
	char *path;
	char *uri;
	char *result;
	NemoDesktopLink *link;
	GString *expanding_string;
	GList *node;
	GFile *location;

	expanding_string = g_string_new ("");
	for (node = selection; node != NULL; node = node->next) {
		uri = NULL;
		if (NEMO_IS_DESKTOP_ICON_FILE (node->data)) {
			link = nemo_desktop_icon_file_get_link (NEMO_DESKTOP_ICON_FILE (node->data));
			if (link != NULL) {
				location = nemo_desktop_link_get_activation_location (link);
				uri = g_file_get_uri (location);
				g_object_unref (location);
				g_object_unref (G_OBJECT (link));
			}
		} else {
			uri = nemo_file_get_uri (NEMO_FILE (node->data));
		}
		if (uri == NULL) {
			continue;
		}

		if (get_paths) {
			path = g_filename_from_uri (uri, NULL, NULL);
			if (path != NULL) {
				g_string_append (expanding_string, path);
				g_free (path);
				g_string_append (expanding_string, "\n");
			}
		} else {
			g_string_append (expanding_string, uri);
			g_string_append (expanding_string, "\n");
		}
		g_free (uri);
	}

	result = expanding_string->str;
	g_string_free (expanding_string, FALSE);

	return result;
}

static char *
get_file_paths_as_newline_delimited_string (GList *selection)
{
	return get_file_paths_or_uris_as_newline_delimited_string (selection, TRUE);
}

static char *
get_file_uris_as_newline_delimited_string (GList *selection)
{
	return get_file_paths_or_uris_as_newline_delimited_string (selection, FALSE);
}

/* returns newly allocated strings for setting the environment variables */
static void
get_strings_for_environment_variables (NemoView *view, GList *selected_files,
				       char **file_paths, char **uris, char **uri)
{
	char *directory_uri;

	/* We need to check that the directory uri starts with "file:" since
	 * nemo_directory_is_local returns FALSE for nfs.
	 */
	directory_uri = nemo_directory_get_uri (view->details->model);
	if (g_str_has_prefix (directory_uri, "file:") ||
	    eel_uri_is_desktop (directory_uri) ||
	    eel_uri_is_trash (directory_uri)) {
		*file_paths = get_file_paths_as_newline_delimited_string (selected_files);
	} else {
		*file_paths = g_strdup ("");
	}
	g_free (directory_uri);

	*uris = get_file_uris_as_newline_delimited_string (selected_files);

	*uri = nemo_directory_get_uri (view->details->model);
	if (eel_uri_is_desktop (*uri)) {
		g_free (*uri);
		*uri = nemo_get_desktop_directory_uri ();
	}
}

static NemoView *
get_directory_view_of_extra_pane (NemoView *view)
{
	NemoWindowSlot *slot;
	NemoView *next_view;

	slot = nemo_window_get_extra_slot (nemo_view_get_nemo_window (view));
	if (slot != NULL) {
		next_view = nemo_window_slot_get_current_view (slot);

		if (NEMO_IS_VIEW (next_view)) {
			return NEMO_VIEW (next_view);
		}
	}
	return NULL;
}

/*
 * Set up some environment variables that scripts can use
 * to take advantage of the current Nemo state.
 */
static void
set_script_environment_variables (NemoView *view, GList *selected_files)
{
	char *file_paths;
	char *uris;
	char *uri;
	char *geometry_string;
	NemoView *next_view;

	get_strings_for_environment_variables (view, selected_files,
					       &file_paths, &uris, &uri);

	g_setenv ("NEMO_SCRIPT_SELECTED_FILE_PATHS", file_paths, TRUE);
	g_free (file_paths);

	g_setenv ("NEMO_SCRIPT_SELECTED_URIS", uris, TRUE);
	g_free (uris);

	g_setenv ("NEMO_SCRIPT_CURRENT_URI", uri, TRUE);
	g_free (uri);

	geometry_string = eel_gtk_window_get_geometry_string
		(GTK_WINDOW (nemo_view_get_containing_window (view)));
	g_setenv ("NEMO_SCRIPT_WINDOW_GEOMETRY", geometry_string, TRUE);
	g_free (geometry_string);

	/* next pane */
	next_view = get_directory_view_of_extra_pane (view);
	if (next_view) {
		GList *next_pane_selected_files;
		next_pane_selected_files = nemo_view_get_selection (next_view);

		get_strings_for_environment_variables (next_view, next_pane_selected_files,
						       &file_paths, &uris, &uri);
		nemo_file_list_free (next_pane_selected_files);
	} else {
		file_paths = g_strdup("");
		uris = g_strdup("");
		uri = g_strdup("");
	}

	g_setenv ("NEMO_SCRIPT_NEXT_PANE_SELECTED_FILE_PATHS", file_paths, TRUE);
	g_free (file_paths);

	g_setenv ("NEMO_SCRIPT_NEXT_PANE_SELECTED_URIS", uris, TRUE);
	g_free (uris);

	g_setenv ("NEMO_SCRIPT_NEXT_PANE_CURRENT_URI", uri, TRUE);
	g_free (uri);
}

/* Unset all the special script environment variables. */
static void
unset_script_environment_variables (void)
{
	g_unsetenv ("NEMO_SCRIPT_SELECTED_FILE_PATHS");
	g_unsetenv ("NEMO_SCRIPT_SELECTED_URIS");
	g_unsetenv ("NEMO_SCRIPT_CURRENT_URI");
	g_unsetenv ("NEMO_SCRIPT_WINDOW_GEOMETRY");
	g_unsetenv ("NEMO_SCRIPT_NEXT_PANE_SELECTED_FILE_PATHS");
	g_unsetenv ("NEMO_SCRIPT_NEXT_PANE_SELECTED_URIS");
	g_unsetenv ("NEMO_SCRIPT_NEXT_PANE_CURRENT_URI");
}

static void
run_script_callback (GtkAction *action, gpointer callback_data)
{
	ScriptLaunchParameters *launch_parameters;
	GdkScreen *screen;
	GList *selected_files;
	char *file_uri;
	char *local_file_path;
	char *quoted_path;
	char *old_working_dir;
	char **parameters;
	
	launch_parameters = (ScriptLaunchParameters *) callback_data;

	file_uri = nemo_file_get_uri (launch_parameters->file);
	local_file_path = g_filename_from_uri (file_uri, NULL, NULL);
	g_assert (local_file_path != NULL);
	g_free (file_uri);

	quoted_path = g_shell_quote (local_file_path);
	g_free (local_file_path);

	old_working_dir = change_to_view_directory (launch_parameters->directory_view);

	selected_files = nemo_view_get_selection (launch_parameters->directory_view);
	set_script_environment_variables (launch_parameters->directory_view, selected_files);
	 
	parameters = get_file_names_as_parameter_array (selected_files,
						        launch_parameters->directory_view->details->model);

	screen = gtk_widget_get_screen (GTK_WIDGET (launch_parameters->directory_view));

	DEBUG ("run_script_callback, script_path=\"%s\" (omitting script parameters)",
	       local_file_path);

	nemo_launch_application_from_command_array (screen, quoted_path, FALSE,
							(const char * const *) parameters);
	g_strfreev (parameters);

	nemo_file_list_free (selected_files);
	unset_script_environment_variables ();
	g_chdir (old_working_dir);		
	g_free (old_working_dir);
	g_free (quoted_path);
}

static void
add_script_to_scripts_menus (NemoView *directory_view,
			     NemoFile *file,
			     const char *menu_path,
			     const char *popup_path, 
			     const char *popup_bg_path)
{
	ScriptLaunchParameters *launch_parameters;
	char *tip;
	char *name;
	char *uri;
	char *action_name;
	char *escaped_label;
	GdkPixbuf *pixbuf;
	GtkUIManager *ui_manager;
	GtkAction *action;

	name = nemo_file_get_display_name (file);
	uri = nemo_file_get_uri (file);
	tip = g_strdup_printf (_("Run \"%s\" on any selected items"), name);

	launch_parameters = script_launch_parameters_new (file, directory_view);

	action_name = nemo_escape_action_name (uri, "script_");
	escaped_label = eel_str_double_underscores (name);

	action = gtk_action_new (action_name,
				 escaped_label,
				 tip,
				 NULL);

	pixbuf = get_menu_icon_for_file (file);
	if (pixbuf != NULL) {
		g_object_set_data_full (G_OBJECT (action), "menu-icon",
					pixbuf,
					g_object_unref);
	}

	g_signal_connect_data (action, "activate",
			       G_CALLBACK (run_script_callback),
			       launch_parameters,
			       (GClosureNotify)script_launch_parameters_free, 0);

	gtk_action_group_add_action_with_accel (directory_view->details->scripts_action_group,
						action, NULL);
	g_object_unref (action);

	ui_manager = nemo_window_get_ui_manager (directory_view->details->window);

	gtk_ui_manager_add_ui (ui_manager,
			       directory_view->details->scripts_merge_id,
			       menu_path,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);
	gtk_ui_manager_add_ui (ui_manager,
			       directory_view->details->scripts_merge_id,
			       popup_path,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);
	gtk_ui_manager_add_ui (ui_manager,
			       directory_view->details->scripts_merge_id,
			       popup_bg_path,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	g_free (name);
	g_free (uri);
	g_free (tip);
	g_free (action_name);
	g_free (escaped_label);
}

static void
add_submenu_to_directory_menus (NemoView *directory_view,
				GtkActionGroup *action_group,
				guint merge_id,
				NemoFile *file,
				const char *menu_path,
				const char *popup_path,
				const char *popup_bg_path)
{
	char *name;
	GdkPixbuf *pixbuf;
	char *uri;
	GtkUIManager *ui_manager;

	ui_manager = nemo_window_get_ui_manager (directory_view->details->window);
	uri = nemo_file_get_uri (file);
	name = nemo_file_get_display_name (file);
	pixbuf = get_menu_icon_for_file (file);
	add_submenu (ui_manager, action_group, merge_id, menu_path, uri, name, pixbuf, TRUE);
	add_submenu (ui_manager, action_group, merge_id, popup_path, uri, name, pixbuf, FALSE);
	add_submenu (ui_manager, action_group, merge_id, popup_bg_path, uri, name, pixbuf, FALSE);
	if (pixbuf) {
		g_object_unref (pixbuf);
	}
	g_free (name);
	g_free (uri);
}

static gboolean
directory_belongs_in_scripts_menu (const char *uri)
{
	int num_levels;
	int i;

	if (!g_str_has_prefix (uri, scripts_directory_uri)) {
		return FALSE;
	}

	num_levels = 0;
	for (i = scripts_directory_uri_length; uri[i] != '\0'; i++) {
		if (uri[i] == '/') {
			num_levels++;
		}
	}

	if (num_levels > MAX_MENU_LEVELS) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
update_directory_in_scripts_menu (NemoView *view, NemoDirectory *directory)
{
	char *menu_path, *popup_path, *popup_bg_path;
	GList *file_list, *filtered, *node;
	gboolean any_scripts;
	NemoFile *file;
	NemoDirectory *dir;
	char *uri;
	char *escaped_path;
	
	uri = nemo_directory_get_uri (directory);
	escaped_path = escape_action_path (uri + scripts_directory_uri_length);
	g_free (uri);
	menu_path = g_strconcat (NEMO_VIEW_MENU_PATH_SCRIPTS_PLACEHOLDER,
				 escaped_path,
				 NULL);
	popup_path = g_strconcat (NEMO_VIEW_POPUP_PATH_SCRIPTS_PLACEHOLDER,
				  escaped_path,
				  NULL);
	popup_bg_path = g_strconcat (NEMO_VIEW_POPUP_PATH_BACKGROUND_SCRIPTS_PLACEHOLDER,
				     escaped_path,
				     NULL);
	g_free (escaped_path);

	file_list = nemo_directory_get_file_list (directory);
	filtered = nemo_file_list_filter_hidden (file_list, FALSE);
	nemo_file_list_free (file_list);

	file_list = nemo_file_list_sort_by_display_name (filtered);

	any_scripts = FALSE;
	for (node = file_list; node != NULL; node = node->next) {
		file = node->data;

		if (nemo_file_is_launchable (file)) {
			add_script_to_scripts_menus (view, file, menu_path, popup_path, popup_bg_path);
			any_scripts = TRUE;
		} else if (nemo_file_is_directory (file)) {
			uri = nemo_file_get_uri (file);
			if (directory_belongs_in_scripts_menu (uri)) {
				dir = nemo_directory_get_by_uri (uri);
				add_directory_to_scripts_directory_list (view, dir);
				nemo_directory_unref (dir);

				add_submenu_to_directory_menus (view,
								view->details->scripts_action_group,
								view->details->scripts_merge_id,
								file, menu_path, popup_path, popup_bg_path);

				any_scripts = TRUE;
			}
			g_free (uri);
		}
	}

	nemo_file_list_free (file_list);

	g_free (popup_path);
	g_free (popup_bg_path);
	g_free (menu_path);

	return any_scripts;
}

static void
update_scripts_menu (NemoView *view)
{
	gboolean any_scripts;
	GList *sorted_copy, *node;
	NemoDirectory *directory;
	char *uri;
	GtkUIManager *ui_manager;
	GtkAction *action;

	/* There is a race condition here.  If we don't mark the scripts menu as
	   valid before we begin our task then we can lose script menu updates that
	   occur before we finish. */
	view->details->scripts_invalid = FALSE;

	ui_manager = nemo_window_get_ui_manager (view->details->window);
	nemo_ui_unmerge_ui (ui_manager,
				&view->details->scripts_merge_id,
				&view->details->scripts_action_group);
	
	nemo_ui_prepare_merge_ui (ui_manager,
				      "ScriptsGroup",
				      &view->details->scripts_merge_id,
				      &view->details->scripts_action_group);

	/* As we walk through the directories, remove any that no longer belong. */
	any_scripts = FALSE;
	sorted_copy = nemo_directory_list_sort_by_uri
		(nemo_directory_list_copy (view->details->scripts_directory_list));
	for (node = sorted_copy; node != NULL; node = node->next) {
		directory = node->data;

		uri = nemo_directory_get_uri (directory);
		if (!directory_belongs_in_scripts_menu (uri)) {
			remove_directory_from_scripts_directory_list (view, directory);
		} else if (update_directory_in_scripts_menu (view, directory)) {
			any_scripts = TRUE;
		}
		g_free (uri);
	}
	nemo_directory_list_free (sorted_copy);

	action = gtk_action_group_get_action (view->details->dir_action_group, NEMO_ACTION_SCRIPTS);
	gtk_action_set_visible (action, any_scripts);
}

static void
run_action_callback (NemoAction *action, gpointer callback_data)
{

    NemoView *view = NEMO_VIEW (callback_data);
    GList *selected_files;

    selected_files = nemo_view_get_selection (view);
    NemoFile *parent = nemo_view_get_directory_as_file (view);

    nemo_action_activate (action, selected_files, parent);

    nemo_file_list_free (selected_files);
}

static void
determine_visibility (gpointer data, gpointer callback_data)
{
    NemoAction *action = NEMO_ACTION (data);
    NemoView *view = NEMO_VIEW (callback_data);

    GList *selected_files = nemo_view_get_selection (view);
    NemoFile *parent = nemo_view_get_directory_as_file (view);

    if (nemo_action_get_visibility (action, selected_files, parent)) {
        gtk_action_set_label (GTK_ACTION (action), nemo_action_get_label (action,
                                                                          selected_files,
                                                                          parent));
        gtk_action_set_tooltip (GTK_ACTION (action), nemo_action_get_tt (action,
                                                                         selected_files,
                                                                         parent));
        gtk_action_set_visible (GTK_ACTION (action), TRUE);
    } else {
        gtk_action_set_visible (GTK_ACTION (action), FALSE);
    }

    nemo_file_list_free (selected_files);
}

static void
update_actions_visibility (NemoView *view)
{
    GList *actions = gtk_action_group_list_actions (view->details->actions_action_group);
    g_list_foreach (actions, determine_visibility, view);
    g_list_free (actions);
}

static void
add_action_to_action_menus (NemoView *directory_view,
                            NemoAction *action,
                          const char *menu_path,
                          const char *popup_path, 
                          const char *popup_bg_path)
{
    GtkUIManager *ui_manager;

    const gchar *action_name = gtk_action_get_name (GTK_ACTION (action));

    gtk_action_group_add_action (directory_view->details->actions_action_group,
                                 GTK_ACTION (action));

    gtk_action_set_visible (GTK_ACTION (action), FALSE);

    g_signal_connect (action, "activate",
                   G_CALLBACK (run_action_callback),
                   directory_view);

    ui_manager = nemo_window_get_ui_manager (directory_view->details->window);

    gtk_ui_manager_add_ui (ui_manager,
                   directory_view->details->actions_merge_id,
                   menu_path,
                   action_name,
                   action_name,
                   GTK_UI_MANAGER_MENUITEM,
                   FALSE);
    gtk_ui_manager_add_ui (ui_manager,
                   directory_view->details->actions_merge_id,
                   popup_path,
                   action_name,
                   action_name,
                   GTK_UI_MANAGER_MENUITEM,
                   FALSE);
    gtk_ui_manager_add_ui (ui_manager,
                   directory_view->details->actions_merge_id,
                   popup_bg_path,
                   action_name,
                   action_name,
                   GTK_UI_MANAGER_MENUITEM,
                   FALSE);
}

static void
update_actions (NemoView *view)
{
    NemoAction *action;
    GList *action_list, *node;

    action_list = nemo_action_manager_list_actions (view->details->action_manager);

    for (node = action_list; node != NULL; node = node->next) {
        action = node->data;
        add_action_to_action_menus (view, action, NEMO_VIEW_MENU_PATH_ACTIONS_PLACEHOLDER,
                                                  NEMO_VIEW_POPUP_PATH_ACTIONS_PLACEHOLDER,
                                                  NEMO_VIEW_POPUP_PATH_BACKGROUND_ACTIONS_PLACEHOLDER);
    }
}

static void
update_actions_menu (NemoView *view)
{
    GtkUIManager *ui_manager;

    view->details->actions_invalid = FALSE;

    ui_manager = nemo_window_get_ui_manager (view->details->window);
    nemo_ui_unmerge_ui (ui_manager,
                &view->details->actions_merge_id,
                &view->details->actions_action_group);
    
    nemo_ui_prepare_merge_ui (ui_manager,
                      "ActionsGroup",
                      &view->details->actions_merge_id,
                      &view->details->actions_action_group);

    update_actions (view);
}

static void
create_template_callback (GtkAction *action, gpointer callback_data)
{
	CreateTemplateParameters *parameters;

	parameters = callback_data;
	
	nemo_view_new_file (parameters->directory_view, NULL, parameters->file);
}

static void
add_template_to_templates_menus (NemoView *directory_view,
				 NemoFile *file,
				 const char *menu_path,
				 const char *popup_bg_path)
{
	char *tmp, *tip, *uri, *name;
	char *escaped_label;
	GdkPixbuf *pixbuf;
	char *action_name;
	CreateTemplateParameters *parameters;
	GtkUIManager *ui_manager;
	GtkAction *action;

	tmp = nemo_file_get_display_name (file);
	name = eel_filename_strip_extension (tmp);
	g_free (tmp);

	uri = nemo_file_get_uri (file);
	tip = g_strdup_printf (_("Create a new document from template \"%s\""), name);

	action_name = nemo_escape_action_name (uri, "template_");
	escaped_label = eel_str_double_underscores (name);
	
	parameters = create_template_parameters_new (file, directory_view);

	action = gtk_action_new (action_name,
				 escaped_label,
				 tip,
				 NULL);
	
	pixbuf = get_menu_icon_for_file (file);
	if (pixbuf != NULL) {
		g_object_set_data_full (G_OBJECT (action), "menu-icon",
					pixbuf,
					g_object_unref);
	}

	g_signal_connect_data (action, "activate",
			       G_CALLBACK (create_template_callback),
			       parameters, 
			       (GClosureNotify)create_templates_parameters_free, 0);
	
	gtk_action_group_add_action (directory_view->details->templates_action_group,
				     action);
	g_object_unref (action);

	ui_manager = nemo_window_get_ui_manager (directory_view->details->window);

	gtk_ui_manager_add_ui (ui_manager,
			       directory_view->details->templates_merge_id,
			       menu_path,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);
	
	gtk_ui_manager_add_ui (ui_manager,
			       directory_view->details->templates_merge_id,
			       popup_bg_path,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);
	
	g_free (escaped_label);
	g_free (name);
	g_free (tip);
	g_free (uri);
	g_free (action_name);
}

static void
update_templates_directory (NemoView *view)
{
	NemoDirectory *templates_directory;
	GList *node, *next;
	char *templates_uri;

	for (node = view->details->templates_directory_list; node != NULL; node = next) {
		next = node->next;
		remove_directory_from_templates_directory_list (view, node->data);
	}
	
	if (nemo_should_use_templates_directory ()) {
		templates_uri = nemo_get_templates_directory_uri ();
		templates_directory = nemo_directory_get_by_uri (templates_uri);
		g_free (templates_uri);
		add_directory_to_templates_directory_list (view, templates_directory);
		nemo_directory_unref (templates_directory);
	}
}

static void
user_dirs_changed (NemoView *view)
{
	update_templates_directory (view);
	view->details->templates_invalid = TRUE;
	schedule_update_menus (view);
}

static gboolean
directory_belongs_in_templates_menu (const char *templates_directory_uri,
				     const char *uri)
{
	int num_levels;
	int i;

	if (templates_directory_uri == NULL) {
		return FALSE;
	}
	
	if (!g_str_has_prefix (uri, templates_directory_uri)) {
		return FALSE;
	}

	num_levels = 0;
	for (i = strlen (templates_directory_uri); uri[i] != '\0'; i++) {
		if (uri[i] == '/') {
			num_levels++;
		}
	}

	if (num_levels > MAX_MENU_LEVELS) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
update_directory_in_templates_menu (NemoView *view,
				    const char *templates_directory_uri,
				    NemoDirectory *directory)
{
	char *menu_path, *popup_bg_path;
	GList *file_list, *filtered, *node;
	gboolean any_templates;
	NemoFile *file;
	NemoDirectory *dir;
	char *escaped_path;
	char *uri;
	int num;

	/* We know this directory belongs to the template dir, so it must exist */
	g_assert (templates_directory_uri);
	
	uri = nemo_directory_get_uri (directory);
	escaped_path = escape_action_path (uri + strlen (templates_directory_uri));
	g_free (uri);
	menu_path = g_strconcat (NEMO_VIEW_MENU_PATH_NEW_DOCUMENTS_PLACEHOLDER,
				 escaped_path,
				 NULL);
	popup_bg_path = g_strconcat (NEMO_VIEW_POPUP_PATH_BACKGROUND_NEW_DOCUMENTS_PLACEHOLDER,
				     escaped_path,
				     NULL);
	g_free (escaped_path);

	file_list = nemo_directory_get_file_list (directory);
	filtered = nemo_file_list_filter_hidden (file_list, FALSE);
	nemo_file_list_free (file_list);

	file_list = nemo_file_list_sort_by_display_name (filtered);

	num = 0;
	any_templates = FALSE;
	for (node = file_list; num < TEMPLATE_LIMIT && node != NULL; node = node->next, num++) {
		file = node->data;

		if (nemo_file_is_directory (file)) {
			uri = nemo_file_get_uri (file);
			if (directory_belongs_in_templates_menu (templates_directory_uri, uri)) {
				dir = nemo_directory_get_by_uri (uri);
				add_directory_to_templates_directory_list (view, dir);
				nemo_directory_unref (dir);

				add_submenu_to_directory_menus (view,
								view->details->templates_action_group,
								view->details->templates_merge_id,
								file, menu_path, NULL, popup_bg_path);

				any_templates = TRUE;
			}
			g_free (uri);
		} else if (nemo_file_can_read (file)) {
			add_template_to_templates_menus (view, file, menu_path, popup_bg_path);
			any_templates = TRUE;
		}
	}

	nemo_file_list_free (file_list);

	g_free (popup_bg_path);
	g_free (menu_path);

	return any_templates;
}



static void
update_templates_menu (NemoView *view)
{
	gboolean any_templates;
	GList *sorted_copy, *node;
	NemoDirectory *directory;
	GtkUIManager *ui_manager;
	char *uri;
	GtkAction *action;
	char *templates_directory_uri;

	if (nemo_should_use_templates_directory ()) {
		templates_directory_uri = nemo_get_templates_directory_uri ();
	} else {
		templates_directory_uri = NULL;
	}

	/* There is a race condition here.  If we don't mark the scripts menu as
	   valid before we begin our task then we can lose template menu updates that
	   occur before we finish. */
	view->details->templates_invalid = FALSE;

	ui_manager = nemo_window_get_ui_manager (view->details->window);
	nemo_ui_unmerge_ui (ui_manager,
				&view->details->templates_merge_id,
				&view->details->templates_action_group);

	nemo_ui_prepare_merge_ui (ui_manager,
				      "TemplatesGroup",
				      &view->details->templates_merge_id,
				      &view->details->templates_action_group);

	/* As we walk through the directories, remove any that no longer belong. */
	any_templates = FALSE;
	sorted_copy = nemo_directory_list_sort_by_uri
		(nemo_directory_list_copy (view->details->templates_directory_list));
	for (node = sorted_copy; node != NULL; node = node->next) {
		directory = node->data;

		uri = nemo_directory_get_uri (directory);
		if (!directory_belongs_in_templates_menu (templates_directory_uri, uri)) {
			remove_directory_from_templates_directory_list (view, directory);
		} else if (update_directory_in_templates_menu (view,
							       templates_directory_uri,
							       directory)) {
			any_templates = TRUE;
		}
		g_free (uri);
	}
	nemo_directory_list_free (sorted_copy);

	action = gtk_action_group_get_action (view->details->dir_action_group, NEMO_ACTION_NO_TEMPLATES);
	gtk_action_set_visible (action, !any_templates);

	g_free (templates_directory_uri);
}


static void
action_open_scripts_folder_callback (GtkAction *action, 
				     gpointer callback_data)
{      
	NemoView *view;
	static GFile *location = NULL;

	if (location == NULL) {
		location = g_file_new_for_uri (scripts_directory_uri);
	}

	view = NEMO_VIEW (callback_data);
	nemo_window_slot_open_location (view->details->slot, location, 0);

	eel_show_info_dialog_with_details 
		(_("All executable files in this folder will appear in the "
		   "Scripts menu."),
		 _("Choosing a script from the menu will run "
		   "that script with any selected items as input."), 
		 _("All executable files in this folder will appear in the "
		   "Scripts menu. Choosing a script from the menu will run "
		   "that script.\n\n"
		   "When executed from a local folder, scripts will be passed "
		   "the selected file names. When executed from a remote folder "
		   "(e.g. a folder showing web or ftp content), scripts will "
		   "be passed no parameters.\n\n"
		   "In all cases, the following environment variables will be "
		   "set by Nemo, which the scripts may use:\n\n"
		   "NEMO_SCRIPT_SELECTED_FILE_PATHS: newline-delimited paths for selected files (only if local)\n\n"
		   "NEMO_SCRIPT_SELECTED_URIS: newline-delimited URIs for selected files\n\n"
		   "NEMO_SCRIPT_CURRENT_URI: URI for current location\n\n"
		   "NEMO_SCRIPT_WINDOW_GEOMETRY: position and size of current window\n\n"
		   "NEMO_SCRIPT_NEXT_PANE_SELECTED_FILE_PATHS: newline-delimited paths for selected files in the inactive pane of a split-view window (only if local)\n\n"
		   "NEMO_SCRIPT_NEXT_PANE_SELECTED_URIS: newline-delimited URIs for selected files in the inactive pane of a split-view window\n\n"
		   "NEMO_SCRIPT_NEXT_PANE_CURRENT_URI: URI for current location in the inactive pane of a split-view window"),
		 nemo_view_get_containing_window (view));
}

static GtkMenu *
create_popup_menu (NemoView *view, const char *popup_path)
{
	GtkWidget *menu;
	
	menu = gtk_ui_manager_get_widget (nemo_window_get_ui_manager (view->details->window),
					  popup_path);
	gtk_menu_set_screen (GTK_MENU (menu),
			     gtk_widget_get_screen (GTK_WIDGET (view)));
	gtk_widget_show (GTK_WIDGET (menu));

	return GTK_MENU (menu);
}

typedef struct _CopyCallbackData {
	NemoView   *view;
	GtkFileChooser *chooser;
	GHashTable     *locations;
	GList          *selection;
	gboolean        is_move;
} CopyCallbackData;

static void
add_bookmark_for_uri (CopyCallbackData *data,
		      const char       *uri)
{
	GError *error = NULL;
	int count;

	count = GPOINTER_TO_INT (g_hash_table_lookup (data->locations, uri));
	if (count == 0) {
		gtk_file_chooser_add_shortcut_folder_uri (data->chooser,
							  uri,
							  &error);
		if (error != NULL) {
			DEBUG ("Unable to add location '%s' to file selector: %s", uri, error->message);
			g_clear_error (&error);
		}
	}
	g_hash_table_replace (data->locations, g_strdup (uri), GINT_TO_POINTER (count + 1));
}

static void
remove_bookmark_for_uri (CopyCallbackData *data,
			 const char       *uri)
{
	GError *error = NULL;
	int count;

	count = GPOINTER_TO_INT (g_hash_table_lookup (data->locations, uri));
	if (count == 1) {
		gtk_file_chooser_remove_shortcut_folder_uri (data->chooser,
							     uri,
							     &error);
		if (error != NULL) {
			DEBUG ("Unable to remove location '%s' to file selector: %s", uri, error->message);
			g_clear_error (&error);
		}
		g_hash_table_remove (data->locations, uri);
	} else {
		g_hash_table_replace (data->locations, g_strdup (uri), GINT_TO_POINTER (count - 1));
	}
}

static void
add_bookmarks_for_window_slot (CopyCallbackData   *data,
			       NemoWindowSlot *slot)
{
	char *uri;

	uri = nemo_window_slot_get_location_uri (slot);
	if (uri != NULL) {
		add_bookmark_for_uri (data, uri);
	}
	g_free (uri);
}

static void
remove_bookmarks_for_window_slot (CopyCallbackData   *data,
				  NemoWindowSlot *slot)
{
	char *uri;

	uri = nemo_window_slot_get_location_uri (slot);
	if (uri != NULL) {
		remove_bookmark_for_uri (data, uri);
	}
	g_free (uri);
}

static void
on_slot_location_changed (NemoWindowSlot *slot,
			  const char         *from,
			  const char         *to,
			  CopyCallbackData   *data)
{
	if (from != NULL) {
		remove_bookmark_for_uri (data, from);
	}

	if (to != NULL) {
		add_bookmark_for_uri (data, to);
	}
}

static void
on_slot_added (NemoWindow     *window,
	       NemoWindowSlot *slot,
	       CopyCallbackData   *data)
{
	add_bookmarks_for_window_slot (data, slot);
	g_signal_connect (slot, "location-changed", G_CALLBACK (on_slot_location_changed), data);
}

static void
on_slot_removed (NemoWindow     *window,
		 NemoWindowSlot *slot,
		 CopyCallbackData   *data)
{
	remove_bookmarks_for_window_slot (data, slot);
	g_signal_handlers_disconnect_by_func (slot,
					      G_CALLBACK (on_slot_location_changed),
					      data);
}

static void
add_bookmarks_for_window (CopyCallbackData *data,
			  NemoWindow   *window)
{
    GList *s;
    GList *p;
	GList *panes;

	panes = nemo_window_get_panes (window);
    for (p = panes; p != NULL; p = p->next) {
        NemoWindowPane *pane = NEMO_WINDOW_PANE (p->data);
        for (s = pane->slots; s != NULL; s = s->next) {
            NemoWindowSlot *slot = NEMO_WINDOW_SLOT (s->data);
            add_bookmarks_for_window_slot (data, slot);
            g_signal_connect (slot, "location-changed", G_CALLBACK (on_slot_location_changed), data);
        }
    }
	g_signal_connect (window, "slot-added", G_CALLBACK (on_slot_added), data);
	g_signal_connect (window, "slot-removed", G_CALLBACK (on_slot_removed), data);
}

static void
remove_bookmarks_for_window (CopyCallbackData *data,
			     NemoWindow   *window)
{
    GList *s;
    GList *p;
    GList *panes;

    panes = nemo_window_get_panes (window);
    for (p = panes; p != NULL; p = p->next) {
        NemoWindowPane *pane = NEMO_WINDOW_PANE (p->data);
        for (s = pane->slots; s != NULL; s = s->next) {
            NemoWindowSlot *slot = NEMO_WINDOW_SLOT (s->data);
            remove_bookmarks_for_window_slot (data, slot);
            g_signal_handlers_disconnect_by_func (slot,
                                  G_CALLBACK (on_slot_location_changed),
                                  data);
        }
    }

	g_signal_handlers_disconnect_by_func (window,
					      G_CALLBACK (on_slot_added),
					      data);
	g_signal_handlers_disconnect_by_func (window,
					      G_CALLBACK (on_slot_removed),
					      data);
}

static void
on_app_window_added (GtkApplication   *application,
		     GtkWindow        *window,
		     CopyCallbackData *data)
{
	add_bookmarks_for_window (data, NEMO_WINDOW (window));
}

static void
on_app_window_removed (GtkApplication   *application,
		       GtkWindow        *window,
		       CopyCallbackData *data)
{
	remove_bookmarks_for_window (data, NEMO_WINDOW (window));
}

static void
copy_data_free (CopyCallbackData *data)
{
	GtkApplication *application;
	GList *windows;
	GList *w;

	application = gtk_window_get_application (GTK_WINDOW (data->view->details->window));
	g_signal_handlers_disconnect_by_func (application,
					      G_CALLBACK (on_app_window_added),
					      data);
	g_signal_handlers_disconnect_by_func (application,
					      G_CALLBACK (on_app_window_removed),
					      data);

	windows = gtk_application_get_windows (application);
	for (w = windows; w != NULL; w = w->next) {
		NemoWindow *window = w->data;
	    GList *s;
	    GList *p;
	    GList *panes;

	    panes = nemo_window_get_panes (window);
	    for (p = panes; p != NULL; p = p->next) {
	        NemoWindowPane *pane = NEMO_WINDOW_PANE (p->data);
	        for (s = pane->slots; s != NULL; s = s->next) {
	            NemoWindowSlot *slot = NEMO_WINDOW_SLOT (s->data);
	            g_signal_handlers_disconnect_by_func (slot, G_CALLBACK (on_slot_location_changed), data);

	        }
	    }

		g_signal_handlers_disconnect_by_func (window, G_CALLBACK (on_slot_added), data);
		g_signal_handlers_disconnect_by_func (window, G_CALLBACK (on_slot_removed), data);
	}

	nemo_file_list_free (data->selection);
	g_hash_table_destroy (data->locations);
	g_free (data);
}

static void
on_destination_dialog_response (GtkDialog *dialog,
				gint       response_id,
				gpointer   user_data)
{
	CopyCallbackData *copy_data = user_data;

	if (response_id == GTK_RESPONSE_OK) {
		char *target_uri;
		GList *uris, *l;

		target_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));

		uris = NULL;
		for (l = copy_data->selection; l != NULL; l = l->next) {
			uris = g_list_prepend (uris,
					       nemo_file_get_uri ((NemoFile *) l->data));
		}
		uris = g_list_reverse (uris);

		nemo_view_move_copy_items (copy_data->view, uris, NULL, target_uri,
					       copy_data->is_move ? GDK_ACTION_MOVE : GDK_ACTION_COPY,
					       0, 0);

		g_list_free_full (uris, g_free);
		g_free (target_uri);
	}

	copy_data_free (copy_data);
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static gboolean
destination_dialog_filter_cb (const GtkFileFilterInfo *filter_info,
			      gpointer                 user_data)
{
	GList *selection = user_data;
	GList *l;

	for (l = selection; l != NULL; l = l->next) {
		char *uri;
		uri = nemo_file_get_uri (l->data);
		if (strcmp (uri, filter_info->uri) == 0) {
			g_free (uri);
			return FALSE;
		}
		g_free (uri);
	}

	return TRUE;
}

static GList *
get_selected_folders (GList *selection)
{
	GList *folders;
	GList *l;

	folders = NULL;
	for (l = selection; l != NULL; l = l->next) {
		if (nemo_file_is_directory (l->data))
			folders = g_list_prepend (folders, nemo_file_ref (l->data));
	}
	return g_list_reverse (folders);
}

static void
add_window_location_bookmarks (CopyCallbackData *data)
{
	GtkApplication *application;
	GList *windows;
	GList *w;

	application = gtk_window_get_application (GTK_WINDOW (data->view->details->window));
	windows = gtk_application_get_windows (application);
	g_signal_connect (application, "window-added", G_CALLBACK (on_app_window_added), data);
	g_signal_connect (application, "window-removed", G_CALLBACK (on_app_window_removed), data);

	for (w = windows; w != NULL; w = w->next) {
		NemoWindow *window = w->data;
		add_bookmarks_for_window (data, window);
	}
}

static void
copy_or_move_selection (NemoView *view,
			gboolean      is_move)
{
	GtkWidget *dialog;
	char *uri;
	CopyCallbackData *copy_data;
	GList *selection;
    const gchar *title; 

    if (is_move) {
        title = _("Select Target Folder For Move");
    } else {
        title = _("Select Target Folder For Copy");
    }

	selection = nemo_view_get_selection_for_file_transfer (view);

	dialog = gtk_file_chooser_dialog_new (title,
					      GTK_WINDOW (view->details->window),
					      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      _("_Select"), GTK_RESPONSE_OK,
					      NULL);
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	copy_data = g_new0 (CopyCallbackData, 1);
	copy_data->view = view;
	copy_data->selection = selection;
	copy_data->is_move = is_move;
	copy_data->chooser = GTK_FILE_CHOOSER (dialog);
	copy_data->locations = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	add_window_location_bookmarks (copy_data);

	if (selection != NULL) {
		GtkFileFilter *filter;
		GList *folders;

		folders = get_selected_folders (selection);

		filter = gtk_file_filter_new ();
		gtk_file_filter_add_custom (filter,
					    GTK_FILE_FILTER_URI,
					    destination_dialog_filter_cb,
					    folders,
					    (GDestroyNotify)nemo_file_list_free);
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);
	}

	uri = nemo_directory_get_uri (view->details->model);
	gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dialog), uri);
	g_free (uri);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (on_destination_dialog_response),
			  copy_data);

	gtk_widget_show_all (dialog);
}

static void
copy_or_cut_files (NemoView *view,
		   GList           *clipboard_contents,
		   gboolean         cut)
{
	int count;
	char *status_string, *name;
	NemoClipboardInfo info;
        GtkTargetList *target_list;
        GtkTargetEntry *targets;
        int n_targets;

	info.files = clipboard_contents;
	info.cut = cut;

        target_list = gtk_target_list_new (NULL, 0);
        gtk_target_list_add (target_list, copied_files_atom, 0, 0);
        gtk_target_list_add_uri_targets (target_list, 0);
        gtk_target_list_add_text_targets (target_list, 0);

        targets = gtk_target_table_new_from_list (target_list, &n_targets);
        gtk_target_list_unref (target_list);

	gtk_clipboard_set_with_data (nemo_clipboard_get (GTK_WIDGET (view)),
				     targets, n_targets,
				     nemo_get_clipboard_callback, nemo_clear_clipboard_callback,
				     NULL);
        gtk_target_table_free (targets, n_targets);

	nemo_clipboard_monitor_set_clipboard_info (nemo_clipboard_monitor_get (), &info);

	count = g_list_length (clipboard_contents);
	if (count == 1) {
		name = nemo_file_get_display_name (clipboard_contents->data);
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
	} else {
		if (cut) {
			status_string = g_strdup_printf (ngettext("The %'d selected item will be moved "
								  "if you select the Paste command",
								  "The %'d selected items will be moved "
								  "if you select the Paste command",
								  count),
							 count);
		} else {
			status_string = g_strdup_printf (ngettext("The %'d selected item will be copied "
								  "if you select the Paste command",
								  "The %'d selected items will be copied "
								  "if you select the Paste command",
								  count),
							 count);
		}
	}

	nemo_window_slot_set_status (view->details->slot,
					 status_string, NULL);
	g_free (status_string);
}

static void
action_copy_files_callback (GtkAction *action,
			    gpointer callback_data)
{
	NemoView *view;
	GList *selection;

	view = NEMO_VIEW (callback_data);

	selection = nemo_view_get_selection_for_file_transfer (view);
	copy_or_cut_files (view, selection, FALSE);
	nemo_file_list_free (selection);
}

static void
move_copy_selection_to_next_pane (NemoView *view,
				  int copy_action)
{
	NemoWindowSlot *slot;
	char *dest_location;

	slot = nemo_window_get_extra_slot (nemo_view_get_nemo_window (view));
	g_return_if_fail (slot != NULL);

	dest_location = nemo_window_slot_get_current_uri (slot);
	g_return_if_fail (dest_location != NULL);

	move_copy_selection_to_location (view, copy_action, dest_location);
}

static void
action_copy_to_next_pane_callback (GtkAction *action, gpointer callback_data)
{
	NemoView *view;

	view = NEMO_VIEW (callback_data);
	move_copy_selection_to_next_pane (view,
					  GDK_ACTION_COPY);
}

static void
action_move_to_next_pane_callback (GtkAction *action, gpointer callback_data)
{
	NemoWindowSlot *slot;
	char *dest_location;
	NemoView *view;

	view = NEMO_VIEW (callback_data);

	slot = nemo_window_get_extra_slot (nemo_view_get_nemo_window (view));
	g_return_if_fail (slot != NULL);

	dest_location = nemo_window_slot_get_current_uri (slot);
	g_return_if_fail (dest_location != NULL);

	move_copy_selection_to_location (view, GDK_ACTION_MOVE, dest_location);
}

static void
action_copy_to_home_callback (GtkAction *action, gpointer callback_data)
{
	NemoView *view;
	char *dest_location;

	view = NEMO_VIEW (callback_data);

	dest_location = nemo_get_home_directory_uri ();
	move_copy_selection_to_location (view, GDK_ACTION_COPY, dest_location);
	g_free (dest_location);
}

static void
action_move_to_home_callback (GtkAction *action, gpointer callback_data)
{
	NemoView *view;
	char *dest_location;

	view = NEMO_VIEW (callback_data);

	dest_location = nemo_get_home_directory_uri ();
	move_copy_selection_to_location (view, GDK_ACTION_MOVE, dest_location);
	g_free (dest_location);
}

static void
action_copy_to_desktop_callback (GtkAction *action, gpointer callback_data)
{
	NemoView *view;
	char *dest_location;

	view = NEMO_VIEW (callback_data);

	dest_location = nemo_get_desktop_directory_uri ();
	move_copy_selection_to_location (view, GDK_ACTION_COPY, dest_location);
	g_free (dest_location);
}

static void
action_move_to_desktop_callback (GtkAction *action, gpointer callback_data)
{
	NemoView *view;
	char *dest_location;

	view = NEMO_VIEW (callback_data);

	dest_location = nemo_get_desktop_directory_uri ();
	move_copy_selection_to_location (view, GDK_ACTION_MOVE, dest_location);
	g_free (dest_location);
}

static void
action_browse_for_move_to_folder_callback (GtkAction *action, gpointer callback_data)
{
	NemoView *view;

	view = NEMO_VIEW (callback_data);
	copy_or_move_selection (view, TRUE);
}

static void
action_browse_for_copy_to_folder_callback (GtkAction *action, gpointer callback_data)
{
	NemoView *view;

	view = NEMO_VIEW (callback_data);
	copy_or_move_selection (view, FALSE);
}

static void
action_cut_files_callback (GtkAction *action,
			   gpointer callback_data)
{
	NemoView *view;
	GList *selection;

	view = NEMO_VIEW (callback_data);

	selection = nemo_view_get_selection_for_file_transfer (view);
	copy_or_cut_files (view, selection, TRUE);
	nemo_file_list_free (selection);
}

static void
paste_clipboard_data (NemoView *view,
		      GtkSelectionData *selection_data,
		      char *destination_uri)
{
	gboolean cut;
	GList *item_uris;

	cut = FALSE;
	item_uris = nemo_clipboard_get_uri_list_from_selection_data (selection_data, &cut,
									 copied_files_atom);

	if (item_uris == NULL|| destination_uri == NULL) {
		nemo_window_slot_set_status (view->details->slot,
						 _("There is nothing on the clipboard to paste."),
						 NULL);
	} else {
		nemo_view_move_copy_items (view, item_uris, NULL, destination_uri,
					       cut ? GDK_ACTION_MOVE : GDK_ACTION_COPY,
					       0, 0);

		/* If items are cut then remove from clipboard */
		if (cut) {
			gtk_clipboard_clear (nemo_clipboard_get (GTK_WIDGET (view)));
		}

		g_list_free_full (item_uris, g_free);
	}
}

static void
paste_clipboard_received_callback (GtkClipboard     *clipboard,
				   GtkSelectionData *selection_data,
				   gpointer          data)
{
	NemoView *view;
	char *view_uri;

	view = NEMO_VIEW (data);

	view_uri = nemo_view_get_backing_uri (view);

	if (view->details->window != NULL) {
		paste_clipboard_data (view, selection_data, view_uri);
	}

	g_free (view_uri);

	g_object_unref (view);
}

typedef struct {
	NemoView *view;
	NemoFile *target;
} PasteIntoData;

static void
paste_into_clipboard_received_callback (GtkClipboard     *clipboard,
					GtkSelectionData *selection_data,
					gpointer          callback_data)
{
	PasteIntoData *data;
	NemoView *view;
	char *directory_uri;

	data = (PasteIntoData *) callback_data;

	view = NEMO_VIEW (data->view);

	if (view->details->window != NULL) {
		directory_uri = nemo_file_get_activation_uri (data->target);

		paste_clipboard_data (view, selection_data, directory_uri);

		g_free (directory_uri);
	}

	g_object_unref (view);
	nemo_file_unref (data->target);
	g_free (data);
}

static void
action_paste_files_callback (GtkAction *action,
			     gpointer callback_data)
{
	NemoView *view;

	view = NEMO_VIEW (callback_data);

	g_object_ref (view);
	gtk_clipboard_request_contents (nemo_clipboard_get (GTK_WIDGET (view)),
					copied_files_atom,
					paste_clipboard_received_callback,
					view);
}

static void
paste_into (NemoView *view,
	    NemoFile *target)
{
	PasteIntoData *data;

	g_assert (NEMO_IS_VIEW (view));
	g_assert (NEMO_IS_FILE (target));

	data = g_new (PasteIntoData, 1);

	data->view = g_object_ref (view);
	data->target = nemo_file_ref (target);

	gtk_clipboard_request_contents (nemo_clipboard_get (GTK_WIDGET (view)),
					copied_files_atom,
					paste_into_clipboard_received_callback,
					data);
}

static void
open_as_root (const gchar *path)
{	
    gchar *argv[4];
    argv[0] = "pkexec";
    argv[1] = "nemo";
    argv[2] = g_strdup (path);
    argv[3] = NULL;
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                  NULL, NULL, NULL, NULL);
}

static void
open_in_terminal (const gchar *path)
{	
    gchar *argv[2];
    argv[0] = g_settings_get_string (gnome_terminal_preferences,
				     GNOME_DESKTOP_TERMINAL_EXEC);
    argv[1] = NULL;
    g_spawn_async(path, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

static void
action_paste_files_into_callback (GtkAction *action,
				  gpointer callback_data)
{
	NemoView *view;
	GList *selection;

	view = NEMO_VIEW (callback_data);
	selection = nemo_view_get_selection (view);
	if (selection != NULL) {
		paste_into (view, NEMO_FILE (selection->data));
		nemo_file_list_free (selection);
	}

}

static void
action_open_as_root_callback (GtkAction *action,
				  gpointer callback_data)
{
	NemoView *view;
	GList *selection;

	view = NEMO_VIEW (callback_data);
	selection = nemo_view_get_selection (view);
	if (selection != NULL) {
        gchar *path = nemo_file_get_path (NEMO_FILE (selection->data));
		open_as_root (path);
		nemo_file_list_free (selection);
        g_free (path);
	} else {
        gchar *path;
        gchar *uri = nemo_view_get_uri (view);
        GFile *gfile = g_file_new_for_uri (uri);
        if (g_file_has_uri_scheme (gfile, "x-nemo-desktop")) {
            path = nemo_get_desktop_directory ();
        } else {
            path = g_file_get_path (gfile);
        }
        open_as_root (path);
        g_free (uri);
        g_free (path);
        g_object_unref (gfile);
    }

}

static void
action_follow_symlink_callback (GtkAction *action,
                                gpointer callback_data)
{
    NemoView *view;
    GList *selection;

    view = NEMO_VIEW (callback_data);
    selection = nemo_view_get_selection (view);
    if (nemo_file_is_symbolic_link (selection->data)) {
        gchar *uri = nemo_file_get_symbolic_link_target_uri (selection->data);
        GFile *location = g_file_new_for_uri (uri);
        g_free (uri);
	    nemo_window_slot_open_location (view->details->slot, location, 0);
    }
    nemo_file_list_free (selection);
}

static void
action_open_in_terminal_callback(GtkAction *action,
				  gpointer callback_data)
{
	NemoView *view;
	GList *selection;

	view = NEMO_VIEW (callback_data);
	selection = nemo_view_get_selection (view);
	if (selection != NULL) {
        gchar *path = nemo_file_get_path (NEMO_FILE (selection->data));
        open_in_terminal (path);
        g_free (path);
		nemo_file_list_free (selection);
	} else {
        gchar *path;
        gchar *uri = nemo_view_get_uri (view);
        GFile *gfile = g_file_new_for_uri (uri);
        if (g_file_has_uri_scheme (gfile, "x-nemo-desktop")) {
            path = nemo_get_desktop_directory ();
        } else {
            path = g_file_get_path (gfile);
        }
        open_in_terminal (path);
        g_free (uri);
        g_free (path);
        g_object_unref (gfile);
    }
}

static void
invoke_external_bulk_rename_utility (NemoView *view,
				     GList *selection)
{
	GString *cmd;
	char *parameter;
	char *quoted_parameter;
	char *bulk_rename_tool;
	GList *walk;
	NemoFile *file;

	/* assemble command line */
	bulk_rename_tool = get_bulk_rename_tool ();
	cmd = g_string_new (bulk_rename_tool);
	g_free (bulk_rename_tool);
	for (walk = selection; walk; walk = walk->next) {
		file = walk->data;
		parameter = nemo_file_get_uri (file);
		quoted_parameter = g_shell_quote (parameter);
		g_free (parameter);
		cmd = g_string_append (cmd, " ");
		cmd = g_string_append (cmd, quoted_parameter);
		g_free (quoted_parameter);
	}

	/* spawning and error handling */
	nemo_launch_application_from_command (gtk_widget_get_screen (GTK_WIDGET (view)),
						  cmd->str, FALSE, NULL);
	g_string_free (cmd, TRUE);
}

static void
real_action_undo (NemoView *view)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
	nemo_file_undo_manager_undo (GTK_WINDOW (toplevel));
}

static void
real_action_redo (NemoView *view)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (view));
	nemo_file_undo_manager_redo (GTK_WINDOW (toplevel));
}

static void
action_undo_callback (GtkAction *action,
		      gpointer callback_data)
{
	real_action_undo (NEMO_VIEW (callback_data));
}

static void
action_redo_callback (GtkAction *action,
		      gpointer callback_data)
{
	real_action_redo (NEMO_VIEW (callback_data));
}

static void
real_action_rename (NemoView *view,
		    gboolean select_all)
{
	NemoFile *file;
	GList *selection;

	g_assert (NEMO_IS_VIEW (view));

	selection = nemo_view_get_selection (view);

	if (selection_not_empty_in_menu_callback (view, selection)) {
		/* If there is more than one file selected, invoke a batch renamer */
		if (selection->next != NULL) {
			if (have_bulk_rename_tool ()) {
				invoke_external_bulk_rename_utility (view, selection);
			}
		} else {
			file = NEMO_FILE (selection->data);
			if (!select_all) {
				/* directories don't have a file extension, so
				 * they are always pre-selected as a whole */
				select_all = nemo_file_is_directory (file);
			}
			NEMO_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->start_renaming_file (view, file, select_all);
		}
	}

	nemo_file_list_free (selection);
}

static void
action_rename_callback (GtkAction *action,
			gpointer callback_data)
{
	real_action_rename (NEMO_VIEW (callback_data), FALSE);
}

static void
action_rename_select_all_callback (GtkAction *action,
				   gpointer callback_data)
{
	real_action_rename (NEMO_VIEW (callback_data), TRUE);
}

static void
file_mount_callback (NemoFile  *file,
		     GFile         *result_location,
		     GError        *error,
		     gpointer       callback_data)
{
	if (error != NULL &&
	    (error->domain != G_IO_ERROR ||
	     (error->code != G_IO_ERROR_CANCELLED &&
	      error->code != G_IO_ERROR_FAILED_HANDLED &&
	      error->code != G_IO_ERROR_ALREADY_MOUNTED))) {
		eel_show_error_dialog (_("Unable to mount location"),
				       error->message, NULL);
	}
}

static void
file_unmount_callback (NemoFile  *file,
		       GFile         *result_location,
		       GError        *error,
		       gpointer       callback_data)
{
	NemoView *view;

	view = NEMO_VIEW (callback_data);
	g_object_unref (view);

	if (error != NULL &&
	    (error->domain != G_IO_ERROR ||
	     (error->code != G_IO_ERROR_CANCELLED &&
	      error->code != G_IO_ERROR_FAILED_HANDLED))) {
		eel_show_error_dialog (_("Unable to unmount location"),
				       error->message, NULL);
	}
}

static void
file_eject_callback (NemoFile  *file,
		     GFile         *result_location,
		     GError        *error,
		     gpointer       callback_data)
{
	NemoView *view;

	view = NEMO_VIEW (callback_data);
	g_object_unref (view);

	if (error != NULL &&
	    (error->domain != G_IO_ERROR ||
	     (error->code != G_IO_ERROR_CANCELLED &&
	      error->code != G_IO_ERROR_FAILED_HANDLED))) {
		eel_show_error_dialog (_("Unable to eject location"),
				       error->message, NULL);
	}
}

static void
file_stop_callback (NemoFile  *file,
		    GFile         *result_location,
		    GError        *error,
		    gpointer       callback_data)
{
	if (error != NULL &&
	    (error->domain != G_IO_ERROR ||
	     (error->code != G_IO_ERROR_CANCELLED &&
	      error->code != G_IO_ERROR_FAILED_HANDLED))) {
		eel_show_error_dialog (_("Unable to stop drive"),
				       error->message, NULL);
	}
}

static void
action_mount_volume_callback (GtkAction *action,
			      gpointer data)
{
	NemoFile *file;
	GList *selection, *l;
	NemoView *view;
	GMountOperation *mount_op;

        view = NEMO_VIEW (data);
	
	selection = nemo_view_get_selection (view);
	for (l = selection; l != NULL; l = l->next) {
		file = NEMO_FILE (l->data);
		
		if (nemo_file_can_mount (file)) {
			mount_op = gtk_mount_operation_new (nemo_view_get_containing_window (view));
			g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
			nemo_file_mount (file, mount_op, NULL,
					     file_mount_callback, NULL);
			g_object_unref (mount_op);
		}
	}
	nemo_file_list_free (selection);
}

static void
action_unmount_volume_callback (GtkAction *action,
				gpointer data)
{
	NemoFile *file;
	GList *selection, *l;
	NemoView *view;

        view = NEMO_VIEW (data);
	
	selection = nemo_view_get_selection (view);

	for (l = selection; l != NULL; l = l->next) {
		file = NEMO_FILE (l->data);
		if (nemo_file_can_unmount (file)) {
			GMountOperation *mount_op;
			mount_op = gtk_mount_operation_new (nemo_view_get_containing_window (view));
			nemo_file_unmount (file, mount_op, NULL,
					       file_unmount_callback, g_object_ref (view));
			g_object_unref (mount_op);
		}
	}
	nemo_file_list_free (selection);
}

static void
action_eject_volume_callback (GtkAction *action,
			      gpointer data)
{
	NemoFile *file;
	GList *selection, *l;
	NemoView *view;

        view = NEMO_VIEW (data);
	
	selection = nemo_view_get_selection (view);
	for (l = selection; l != NULL; l = l->next) {
		file = NEMO_FILE (l->data);
		
		if (nemo_file_can_eject (file)) {
			GMountOperation *mount_op;
			mount_op = gtk_mount_operation_new (nemo_view_get_containing_window (view));
			nemo_file_eject (file, mount_op, NULL,
					     file_eject_callback, g_object_ref (view));
			g_object_unref (mount_op);
		}
	}	
	nemo_file_list_free (selection);
}

static void
file_start_callback (NemoFile  *file,
		     GFile         *result_location,
		     GError        *error,
		     gpointer       callback_data)
{
	if (error != NULL &&
	    (error->domain != G_IO_ERROR ||
	     (error->code != G_IO_ERROR_CANCELLED &&
	      error->code != G_IO_ERROR_FAILED_HANDLED &&
	      error->code != G_IO_ERROR_ALREADY_MOUNTED))) {
		eel_show_error_dialog (_("Unable to start location"),
				       error->message, NULL);
	}
}

static void
action_start_volume_callback (GtkAction *action,
			      gpointer   data)
{
	NemoFile *file;
	GList *selection, *l;
	NemoView *view;
	GMountOperation *mount_op;

        view = NEMO_VIEW (data);

	selection = nemo_view_get_selection (view);
	for (l = selection; l != NULL; l = l->next) {
		file = NEMO_FILE (l->data);

		if (nemo_file_can_start (file) || nemo_file_can_start_degraded (file)) {
			mount_op = gtk_mount_operation_new (nemo_view_get_containing_window (view));
			nemo_file_start (file, mount_op, NULL,
					     file_start_callback, NULL);
			g_object_unref (mount_op);
		}
	}
	nemo_file_list_free (selection);
}

static void
action_stop_volume_callback (GtkAction *action,
			     gpointer   data)
{
	NemoFile *file;
	GList *selection, *l;
	NemoView *view;

        view = NEMO_VIEW (data);

	selection = nemo_view_get_selection (view);
	for (l = selection; l != NULL; l = l->next) {
		file = NEMO_FILE (l->data);

		if (nemo_file_can_stop (file)) {
			GMountOperation *mount_op;
			mount_op = gtk_mount_operation_new (nemo_view_get_containing_window (view));
			nemo_file_stop (file, mount_op, NULL,
					    file_stop_callback, NULL);
			g_object_unref (mount_op);
		}
	}
	nemo_file_list_free (selection);
}

static void
action_detect_media_callback (GtkAction *action,
			      gpointer   data)
{
	NemoFile *file;
	GList *selection, *l;
	NemoView *view;

        view = NEMO_VIEW (data);

	selection = nemo_view_get_selection (view);
	for (l = selection; l != NULL; l = l->next) {
		file = NEMO_FILE (l->data);

		if (nemo_file_can_poll_for_media (file) && !nemo_file_is_media_check_automatic (file)) {
			nemo_file_poll_for_media (file);
		}
	}
	nemo_file_list_free (selection);
}

static void
action_self_mount_volume_callback (GtkAction *action,
				   gpointer data)
{
	NemoFile *file;
	NemoView *view;
	GMountOperation *mount_op;

	view = NEMO_VIEW (data);

	file = nemo_view_get_directory_as_file (view);
	if (file == NULL) {
		return;
	}

	mount_op = gtk_mount_operation_new (nemo_view_get_containing_window (view));
	g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
	nemo_file_mount (file, mount_op, NULL, file_mount_callback, NULL);
	g_object_unref (mount_op);
}

static void
action_self_unmount_volume_callback (GtkAction *action,
				     gpointer data)
{
	NemoFile *file;
	NemoView *view;
	GMountOperation *mount_op;

	view = NEMO_VIEW (data);

	file = nemo_view_get_directory_as_file (view);
	if (file == NULL) {
		return;
	}

	mount_op = gtk_mount_operation_new (nemo_view_get_containing_window (view));
	nemo_file_unmount (file, mount_op, NULL, file_unmount_callback, g_object_ref (view));
	g_object_unref (mount_op);
}

static void
action_self_eject_volume_callback (GtkAction *action,
				   gpointer data)
{
	NemoFile *file;
	NemoView *view;
	GMountOperation *mount_op;

	view = NEMO_VIEW (data);

	file = nemo_view_get_directory_as_file (view);
	if (file == NULL) {
		return;
	}
	
	mount_op = gtk_mount_operation_new (nemo_view_get_containing_window (view));
	nemo_file_eject (file, mount_op, NULL, file_eject_callback, g_object_ref (view));
	g_object_unref (mount_op);
}

static void
action_self_start_volume_callback (GtkAction *action,
				   gpointer   data)
{
	NemoFile *file;
	NemoView *view;
	GMountOperation *mount_op;

	view = NEMO_VIEW (data);

	file = nemo_view_get_directory_as_file (view);
	if (file == NULL) {
		return;
	}

	mount_op = gtk_mount_operation_new (nemo_view_get_containing_window (view));
	nemo_file_start (file, mount_op, NULL, file_start_callback, NULL);
	g_object_unref (mount_op);
}

static void
action_self_stop_volume_callback (GtkAction *action,
				  gpointer   data)
{
	NemoFile *file;
	NemoView *view;
	GMountOperation *mount_op;

	view = NEMO_VIEW (data);

	file = nemo_view_get_directory_as_file (view);
	if (file == NULL) {
		return;
	}

	mount_op = gtk_mount_operation_new (nemo_view_get_containing_window (view));
	nemo_file_stop (file, mount_op, NULL,
			    file_stop_callback, NULL);
	g_object_unref (mount_op);
}

static void
action_self_detect_media_callback (GtkAction *action,
				   gpointer   data)
{
	NemoFile *file;
	NemoView *view;

	view = NEMO_VIEW (data);

	file = nemo_view_get_directory_as_file (view);
	if (file == NULL) {
		return;
	}

	nemo_file_poll_for_media (file);
}

static void
action_location_mount_volume_callback (GtkAction *action,
				       gpointer data)
{
	NemoFile *file;
	NemoView *view;
	GMountOperation *mount_op;

	view = NEMO_VIEW (data);

	file = view->details->location_popup_directory_as_file;
	if (file == NULL) {
		return;
	}

	mount_op = gtk_mount_operation_new (nemo_view_get_containing_window (view));
	g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
	nemo_file_mount (file, mount_op, NULL, file_mount_callback, NULL);
	g_object_unref (mount_op);
}

static void
action_location_unmount_volume_callback (GtkAction *action,
					 gpointer data)
{
	NemoFile *file;
	NemoView *view;
	GMountOperation *mount_op;

	view = NEMO_VIEW (data);

	file = view->details->location_popup_directory_as_file;
	if (file == NULL) {
		return;
	}

	mount_op = gtk_mount_operation_new (nemo_view_get_containing_window (view));
	nemo_file_unmount (file, mount_op, NULL,
			       file_unmount_callback, g_object_ref (view));
	g_object_unref (mount_op);
}

static void
action_location_eject_volume_callback (GtkAction *action,
				       gpointer data)
{
	NemoFile *file;
	NemoView *view;
	GMountOperation *mount_op;

	view = NEMO_VIEW (data);

	file = view->details->location_popup_directory_as_file;
	if (file == NULL) {
		return;
	}
	
	mount_op = gtk_mount_operation_new (nemo_view_get_containing_window (view));
	nemo_file_eject (file, mount_op, NULL,
			     file_eject_callback, g_object_ref (view));
	g_object_unref (mount_op);
}

static void
action_location_start_volume_callback (GtkAction *action,
				       gpointer   data)
{
	NemoFile *file;
	NemoView *view;
	GMountOperation *mount_op;

	view = NEMO_VIEW (data);

	file = view->details->location_popup_directory_as_file;
	if (file == NULL) {
		return;
	}

	mount_op = gtk_mount_operation_new (nemo_view_get_containing_window (view));
	nemo_file_start (file, mount_op, NULL, file_start_callback, NULL);
	g_object_unref (mount_op);
}

static void
action_location_stop_volume_callback (GtkAction *action,
				      gpointer   data)
{
	NemoFile *file;
	NemoView *view;
	GMountOperation *mount_op;

	view = NEMO_VIEW (data);

	file = view->details->location_popup_directory_as_file;
	if (file == NULL) {
		return;
	}

	mount_op = gtk_mount_operation_new (nemo_view_get_containing_window (view));
	nemo_file_stop (file, mount_op, NULL,
			    file_stop_callback, NULL);
	g_object_unref (mount_op);
}

static void
action_location_detect_media_callback (GtkAction *action,
				       gpointer   data)
{
	NemoFile *file;
	NemoView *view;

	view = NEMO_VIEW (data);

	file = view->details->location_popup_directory_as_file;
	if (file == NULL) {
		return;
	}

	nemo_file_poll_for_media (file);
}

static void
connect_to_server_response_callback (GtkDialog *dialog,
				     int response_id,
				     gpointer data)
{
#ifdef GIO_CONVERSION_DONE
	GtkEntry *entry;
	char *uri;
	const char *name;
	char *icon;

	entry = GTK_ENTRY (data);
	
	switch (response_id) {
	case GTK_RESPONSE_OK:
		uri = g_object_get_data (G_OBJECT (dialog), "link-uri");
		icon = g_object_get_data (G_OBJECT (dialog), "link-icon");
		name = gtk_entry_get_text (entry);
		gnome_vfs_connect_to_server (uri, (char *)name, icon);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	case GTK_RESPONSE_NONE:
	case GTK_RESPONSE_DELETE_EVENT:
	case GTK_RESPONSE_CANCEL:
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	default :
		g_assert_not_reached ();
	}
#endif
	/* FIXME: the above code should make a server connection permanent */
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
entry_activate_callback (GtkEntry *entry,
			 gpointer user_data)
{
	GtkDialog *dialog;
	
	dialog = GTK_DIALOG (user_data);
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
action_connect_to_server_link_callback (GtkAction *action,
					gpointer data)
{
	NemoFile *file;
	GList *selection;
	NemoView *view;
	char *uri;
	NemoIconInfo *icon;
	const char *icon_name;
	char *name;
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *box;
	char *title;

        view = NEMO_VIEW (data);
	
	selection = nemo_view_get_selection (view);

	if (g_list_length (selection) != 1) {
		nemo_file_list_free (selection);
		return;
	}

	file = NEMO_FILE (selection->data);

	uri = nemo_file_get_activation_uri (file);
	icon = nemo_file_get_icon (file, NEMO_ICON_SIZE_STANDARD, 0);
	icon_name = nemo_icon_info_get_used_name (icon);
	name = nemo_file_get_display_name (file);

	if (uri != NULL) {
		title = g_strdup_printf (_("Connect to Server %s"), name);
		dialog = gtk_dialog_new_with_buttons (title,
						      nemo_view_get_containing_window (view),
						      0,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						      _("_Connect"), GTK_RESPONSE_OK,
						      NULL);

		g_object_set_data_full (G_OBJECT (dialog), "link-uri", g_strdup (uri), g_free);
		g_object_set_data_full (G_OBJECT (dialog), "link-icon", g_strdup (icon_name), g_free);
		
		gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
		gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 2);

		box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
		gtk_widget_show (box);
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
				    box, TRUE, TRUE, 0);
		
		label = gtk_label_new_with_mnemonic (_("Link _name:"));
		gtk_widget_show (label);

		gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 12);
		
		entry = gtk_entry_new ();
		if (name) {
			gtk_entry_set_text (GTK_ENTRY (entry), name);
		}
		g_signal_connect (entry,
				  "activate", 
				  G_CALLBACK (entry_activate_callback),
				  dialog);
		
		gtk_widget_show (entry);
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
		
		gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 12);
		
		gtk_dialog_set_default_response (GTK_DIALOG (dialog),
						 GTK_RESPONSE_OK);
		g_signal_connect (dialog, "response",
				  G_CALLBACK (connect_to_server_response_callback),
				  entry);
		gtk_widget_show (dialog);
	}
	
	g_free (uri);
	g_object_unref (icon);
	g_free (name);
}

static void
action_location_open_alternate_callback (GtkAction *action,
					 gpointer   callback_data)
{
	NemoView *view;
	NemoFile *file;

	view = NEMO_VIEW (callback_data);

	file = view->details->location_popup_directory_as_file;
	if (file == NULL) {
		return;
	}
	nemo_view_activate_file (view,
				     file,
				     NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW);
}

static void
action_location_open_in_new_tab_callback (GtkAction *action,
					  gpointer   callback_data)
{
	NemoView *view;
	NemoFile *file;

	view = NEMO_VIEW (callback_data);

	file = view->details->location_popup_directory_as_file;
	if (file == NULL) {
		return;
	}

	nemo_view_activate_file (view,
				     file,
				     NEMO_WINDOW_OPEN_FLAG_NEW_TAB);
}

static void
action_location_cut_callback (GtkAction *action,
			      gpointer   callback_data)
{
	NemoView *view;
	NemoFile *file;
	GList *files;

	view = NEMO_VIEW (callback_data);

	file = view->details->location_popup_directory_as_file;
	g_return_if_fail (file != NULL);

	files = g_list_append (NULL, file);
	copy_or_cut_files (view, files, TRUE);
	g_list_free (files);
}

static void
action_location_copy_callback (GtkAction *action,
			       gpointer   callback_data)
{
	NemoView *view;
	NemoFile *file;
	GList *files;

	view = NEMO_VIEW (callback_data);

	file = view->details->location_popup_directory_as_file;
	g_return_if_fail (file != NULL);

	files = g_list_append (NULL, file);
	copy_or_cut_files (view, files, FALSE);
	g_list_free (files);
}

static void
action_location_paste_files_into_callback (GtkAction *action,
					   gpointer callback_data)
{
	NemoView *view;
	NemoFile *file;

	view = NEMO_VIEW (callback_data);

	file = view->details->location_popup_directory_as_file;
	g_return_if_fail (file != NULL);

	paste_into (view, file);
}

static void
action_location_trash_callback (GtkAction *action,
				gpointer   callback_data)
{
	NemoView *view;
	NemoFile *file;
	GList *files;

	view = NEMO_VIEW (callback_data);

	file = view->details->location_popup_directory_as_file;
	g_return_if_fail (file != NULL);

	files = g_list_append (NULL, file);
	trash_or_delete_files (nemo_view_get_containing_window (view),
			       files, TRUE,
			       view);
	g_list_free (files);
}

static void
action_location_delete_callback (GtkAction *action,
				 gpointer   callback_data)
{
	NemoView *view;
	NemoFile *file;
	GFile *location;
	GList *files;

	view = NEMO_VIEW (callback_data);

	file = view->details->location_popup_directory_as_file;
	g_return_if_fail (file != NULL);

	location = nemo_file_get_location (file);

	files = g_list_append (NULL, location);
	nemo_file_operations_delete (files, nemo_view_get_containing_window (view),
					 NULL, NULL);

	g_list_free_full (files, g_object_unref);
}

static void
action_location_restore_from_trash_callback (GtkAction *action,
					     gpointer callback_data)
{
	NemoView *view;
	NemoFile *file;
	GList l;

	view = NEMO_VIEW (callback_data);
	file = view->details->location_popup_directory_as_file;

	l.prev = NULL;
	l.next = NULL;
	l.data = file;
	nemo_restore_files_from_trash (&l,
					   nemo_view_get_containing_window (view));
}

static void
nemo_view_init_show_hidden_files (NemoView *view)
{
	NemoWindowShowHiddenFilesMode mode;
	gboolean show_hidden_changed;

	if (view->details->ignore_hidden_file_preferences) {
		return;
	}

	show_hidden_changed = FALSE;
	mode = nemo_window_get_hidden_files_mode (view->details->window);

    if (mode == NEMO_WINDOW_SHOW_HIDDEN_FILES_ENABLE) {
        show_hidden_changed = !view->details->show_hidden_files;
        view->details->show_hidden_files = TRUE;
    } else {
        show_hidden_changed = view->details->show_hidden_files;
        view->details->show_hidden_files = FALSE;
    }

    if (show_hidden_changed && (view->details->model != NULL)) {
        load_directory (view, view->details->model);	
    }
}

static const GtkActionEntry directory_view_entries[] = {
  /* name, stock id, label */  { "New Documents", "document-new", N_("New _Document") },
  /* name, stock id, label */  { "Open With", NULL, N_("Open Wit_h"),
				 NULL, N_("Choose a program with which to open the selected item") },
  /* name, stock id */         { "Properties", GTK_STOCK_PROPERTIES,
  /* label, accelerator */       N_("_Properties"), "<alt>Return",
  /* tooltip */                  N_("View or modify the properties of each selected item"),
				 G_CALLBACK (action_properties_callback) },
  /* name, stock id */         { "PropertiesAccel", NULL,
  /* label, accelerator */       "PropertiesAccel", "<control>I",
  /* tooltip */                  NULL,
				 G_CALLBACK (action_properties_callback) },
  /* name, stock id */         { "New Folder", "folder-new",
  /* label, accelerator */       N_("New _Folder"), "<control><shift>N",
  /* tooltip */                  N_("Create a new empty folder inside this folder"),
				 G_CALLBACK (action_new_folder_callback) },
  /* name, stock id */         { NEMO_ACTION_NEW_FOLDER_WITH_SELECTION, NULL,
  /* label, accelerator */       N_("New Folder with Selection"), NULL,
  /* tooltip */                  N_("Create a new folder containing the selected items"),
				 G_CALLBACK (action_new_folder_with_selection_callback) },
  /* name, stock id, label */  { "No Templates", NULL, N_("No templates installed") },
  /* name, stock id */         { "New Empty Document", NULL,
    /* translators: this is used to indicate that a document doesn't contain anything */
  /* label, accelerator */       N_("_Empty Document"), NULL,
  /* tooltip */                  N_("Create a new empty document inside this folder"),
				 G_CALLBACK (action_new_empty_file_callback) },
  /* name, stock id */         { "Open", NULL,
  /* label, accelerator */       N_("_Open"), "<control>o",
  /* tooltip */                  N_("Open the selected item in this window"),
				 G_CALLBACK (action_open_callback) },
  /* name, stock id */         { "OpenAccel", NULL,
  /* label, accelerator */       "OpenAccel", "<alt>Down",
  /* tooltip */                  NULL,
				 G_CALLBACK (action_open_callback) },
  /* name, stock id */         { "OpenAlternate", NULL,
  /* label, accelerator */       N_("Open in Navigation Window"), "<control><shift>o",
  /* tooltip */                  N_("Open each selected item in a navigation window"),
				 G_CALLBACK (action_open_alternate_callback) },
  /* name, stock id */         { "OpenInNewTab", NULL,
  /* label, accelerator */       N_("Open in New _Tab"), "<control><shift>o",
  /* tooltip */                  N_("Open each selected item in a new tab"),
				 G_CALLBACK (action_open_new_tab_callback) },
  /* name, stock id */         { NEMO_ACTION_OPEN_IN_TERMINAL, "terminal",
  /* label, accelerator */       N_("Open in Terminal"), "",
  /* tooltip */                  N_("Open terminal in the selected folder"),
				 G_CALLBACK (action_open_in_terminal_callback) },
  /* name, stock id */         { NEMO_ACTION_OPEN_AS_ROOT, GTK_STOCK_DIALOG_AUTHENTICATION,
  /* label, accelerator */       N_("Open as Root"), "",
  /* tooltip */                  N_("Open the folder with administration privileges"),
				 G_CALLBACK (action_open_as_root_callback) },

  /* name, stock id */         { NEMO_ACTION_FOLLOW_SYMLINK, GTK_STOCK_JUMP_TO,
  /* label, accelerator */       N_("Follow link to original file"), "",
  /* tooltip */                  N_("Navigate to the original file that this symbolic link points to"),
                 G_CALLBACK (action_follow_symlink_callback) },
  /* name, stock id */         { "OtherApplication1", NULL,
  /* label, accelerator */       N_("Other _Application..."), NULL,
  /* tooltip */                  N_("Choose another application with which to open the selected item"),
				 G_CALLBACK (action_other_application_callback) },
  /* name, stock id */         { "OtherApplication2", NULL,
  /* label, accelerator */       N_("Open With Other _Application..."), NULL,
  /* tooltip */                  N_("Choose another application with which to open the selected item"),
				 G_CALLBACK (action_other_application_callback) },
  /* name, stock id */         { "Open Scripts Folder", NULL,
  /* label, accelerator */       N_("_Open Scripts Folder"), NULL,
  /* tooltip */                 N_("Show the folder containing the scripts that appear in this menu"),
				 G_CALLBACK (action_open_scripts_folder_callback) },
  /* name, stock id */         { "Empty Trash", NULL,
  /* label, accelerator */       N_("E_mpty Trash"), NULL,
  /* tooltip */                  N_("Delete all items in the Trash"),
				 G_CALLBACK (action_empty_trash_callback) },
  /* name, stock id */         { "Cut", GTK_STOCK_CUT,
  /* label, accelerator */       NULL, NULL,
  /* tooltip */                  N_("Prepare the selected files to be moved with a Paste command"),
				 G_CALLBACK (action_cut_files_callback) },
  /* name, stock id */         { "Copy", GTK_STOCK_COPY,
  /* label, accelerator */       NULL, NULL,
  /* tooltip */                  N_("Prepare the selected files to be copied with a Paste command"),
				 G_CALLBACK (action_copy_files_callback) },
  /* name, stock id */         { "Paste", GTK_STOCK_PASTE,
  /* label, accelerator */       NULL, NULL,
  /* tooltip */                  N_("Move or copy files previously selected by a Cut or Copy command"),
				 G_CALLBACK (action_paste_files_callback) },
  /* We make accelerator "" instead of null here to not inherit the stock
     accelerator for paste */
  /* name, stock id */         { "Paste Files Into", GTK_STOCK_PASTE,
  /* label, accelerator */       N_("_Paste Into Folder"), "",
  /* tooltip */                  N_("Move or copy files previously selected by a Cut or Copy command into the selected folder"),
				 G_CALLBACK (action_paste_files_into_callback) },
  /* name, stock id, label */  { "CopyToMenu", NULL, N_("Cop_y to") },
  /* name, stock id, label */  { "MoveToMenu", NULL, N_("M_ove to") },                      
  /* name, stock id */         { "Select All", NULL,
  /* label, accelerator */       N_("Select _All"), "<control>A",
  /* tooltip */                  N_("Select all items in this window"),
				 G_CALLBACK (action_select_all_callback) },
  /* name, stock id */         { "Select Pattern", NULL,
  /* label, accelerator */       N_("Select I_tems Matching..."), "<control>S",
  /* tooltip */                  N_("Select items in this window matching a given pattern"),
				 G_CALLBACK (action_select_pattern_callback) },
  /* name, stock id */         { "Invert Selection", NULL,
  /* label, accelerator */       N_("_Invert Selection"), "<control><shift>I",
  /* tooltip */                  N_("Select all and only the items that are not currently selected"),
				 G_CALLBACK (action_invert_selection_callback) }, 
  /* name, stock id */         { "Duplicate", NULL,
  /* label, accelerator */       N_("D_uplicate"), NULL,
  /* tooltip */                  N_("Duplicate each selected item"),
				 G_CALLBACK (action_duplicate_callback) },
  /* name, stock id */         { "Create Link", NULL,
  /* label, accelerator */       N_("Ma_ke Link"), "<control>M",
  /* tooltip */                  N_("Create a symbolic link for each selected item"),
				 G_CALLBACK (action_create_link_callback) },
  /* name, stock id */         { "Rename", NULL,
  /* label, accelerator */       N_("_Rename..."), "F2",
  /* tooltip */                  N_("Rename selected item"),
				 G_CALLBACK (action_rename_callback) },
  /* name, stock id */         { "RenameSelectAll", NULL,
  /* label, accelerator */       "RenameSelectAll", "<shift>F2",
  /* tooltip */                  NULL,
				 G_CALLBACK (action_rename_select_all_callback) },
  /* name, stock id */         { "Trash", NULL,
  /* label, accelerator */       N_("Mo_ve to Trash"), NULL,
  /* tooltip */                  N_("Move each selected item to the Trash"),
				 G_CALLBACK (action_trash_callback) },
  /* name, stock id */         { "Delete", NULL,
  /* label, accelerator */       N_("_Delete"), NULL,
  /* tooltip */                  N_("Delete each selected item, without moving to the Trash"),
				 G_CALLBACK (action_delete_callback) },
  /* name, stock id */         { "Restore From Trash", NULL,
  /* label, accelerator */       N_("_Restore"), NULL,
				 NULL,
                 G_CALLBACK (action_restore_from_trash_callback) },
 /* name, stock id */          { "Undo", GTK_STOCK_UNDO,
 /* label, accelerator */        N_("_Undo"), "<control>Z",
 /* tooltip */                   N_("Undo the last action"),
                                 G_CALLBACK (action_undo_callback) },
 /* name, stock id */	       { "Redo", GTK_STOCK_REDO,
 /* label, accelerator */        N_("_Redo"), "<control>Y",
 /* tooltip */                   N_("Redo the last undone action"),
                                 G_CALLBACK (action_redo_callback) },
  /*
   * multiview-TODO: decide whether "Reset to Defaults" should
   * be window-wide, and not just view-wide.
   * Since this also resets the "Show hidden files" mode,
   * it is a mixture of both ATM.
   */
  /* name, stock id */         { "Reset to Defaults", NULL,
  /* label, accelerator */       N_("Reset View to _Defaults"), NULL,
  /* tooltip */                  N_("Reset sorting order and zoom level to match preferences for this view"),
				 G_CALLBACK (action_reset_to_defaults_callback) },
  /* name, stock id */         { "Connect To Server Link", NULL,
  /* label, accelerator */       N_("Connect To This Server"), NULL,
  /* tooltip */                  N_("Make a permanent connection to this server"),
				 G_CALLBACK (action_connect_to_server_link_callback) },
  /* name, stock id */         { "Mount Volume", NULL,
  /* label, accelerator */       N_("_Mount"), NULL,
  /* tooltip */                  N_("Mount the selected volume"),
				 G_CALLBACK (action_mount_volume_callback) },
  /* name, stock id */         { "Unmount Volume", NULL,
  /* label, accelerator */       N_("_Unmount"), NULL,
  /* tooltip */                  N_("Unmount the selected volume"),
				 G_CALLBACK (action_unmount_volume_callback) },
  /* name, stock id */         { "Eject Volume", NULL,
  /* label, accelerator */       N_("_Eject"), NULL,
  /* tooltip */                  N_("Eject the selected volume"),
				 G_CALLBACK (action_eject_volume_callback) },
  /* name, stock id */         { "Start Volume", NULL,
  /* label, accelerator */       N_("_Start"), NULL,
  /* tooltip */                  N_("Start the selected volume"),
				 G_CALLBACK (action_start_volume_callback) },
  /* name, stock id */         { "Stop Volume", NULL,
  /* label, accelerator */       N_("_Stop"), NULL,
  /* tooltip */                  N_("Stop the selected volume"),
				 G_CALLBACK (action_stop_volume_callback) },
  /* name, stock id */         { "Poll", NULL,
  /* label, accelerator */       N_("_Detect Media"), NULL,
  /* tooltip */                  N_("Detect media in the selected drive"),
				 G_CALLBACK (action_detect_media_callback) },
  /* name, stock id */         { "Self Mount Volume", NULL,
  /* label, accelerator */       N_("_Mount"), NULL,
  /* tooltip */                  N_("Mount the volume associated with the open folder"),
				 G_CALLBACK (action_self_mount_volume_callback) },
  /* name, stock id */         { "Self Unmount Volume", NULL,
  /* label, accelerator */       N_("_Unmount"), NULL,
  /* tooltip */                  N_("Unmount the volume associated with the open folder"),
				 G_CALLBACK (action_self_unmount_volume_callback) },
  /* name, stock id */         { "Self Eject Volume", NULL,
  /* label, accelerator */       N_("_Eject"), NULL,
  /* tooltip */                  N_("Eject the volume associated with the open folder"),
				 G_CALLBACK (action_self_eject_volume_callback) },
  /* name, stock id */         { "Self Start Volume", NULL,
  /* label, accelerator */       N_("_Start"), NULL,
  /* tooltip */                  N_("Start the volume associated with the open folder"),
				 G_CALLBACK (action_self_start_volume_callback) },
  /* name, stock id */         { "Self Stop Volume", NULL,
  /* label, accelerator */       N_("_Stop"), NULL,
  /* tooltip */                  N_("Stop the volume associated with the open folder"),
				 G_CALLBACK (action_self_stop_volume_callback) },
  /* name, stock id */         { "Self Poll", NULL,
  /* label, accelerator */       N_("_Detect Media"), NULL,
  /* tooltip */                  N_("Detect media in the selected drive"),
				 G_CALLBACK (action_self_detect_media_callback) },
  /* name, stock id */         { "OpenCloseParent", NULL,
  /* label, accelerator */       N_("Open File and Close window"), "<alt><shift>Down",
  /* tooltip */                  NULL,
				 G_CALLBACK (action_open_close_parent_callback) },
  /* name, stock id */         { "Save Search", NULL,
  /* label, accelerator */       N_("Sa_ve Search"), NULL,
  /* tooltip */                  N_("Save the edited search"),
				 G_CALLBACK (action_save_search_callback) },
  /* name, stock id */         { "Save Search As", NULL,
  /* label, accelerator */       N_("Sa_ve Search As..."), NULL,
  /* tooltip */                  N_("Save the current search as a file"),
				 G_CALLBACK (action_save_search_as_callback) },

  /* Location-specific actions */
  /* name, stock id */         { NEMO_ACTION_LOCATION_OPEN_ALTERNATE, NULL,
  /* label, accelerator */       N_("Open in Navigation Window"), "",
  /* tooltip */                  N_("Open this folder in a navigation window"),
				 G_CALLBACK (action_location_open_alternate_callback) },
  /* name, stock id */         { NEMO_ACTION_LOCATION_OPEN_IN_NEW_TAB, NULL,
  /* label, accelerator */       N_("Open in New _Tab"), "",
  /* tooltip */                  N_("Open this folder in a new tab"),
				 G_CALLBACK (action_location_open_in_new_tab_callback) },

  /* name, stock id */         { NEMO_ACTION_LOCATION_CUT, GTK_STOCK_CUT,
  /* label, accelerator */       NULL, "",
  /* tooltip */                  N_("Prepare this folder to be moved with a Paste command"),
				 G_CALLBACK (action_location_cut_callback) },
  /* name, stock id */         { NEMO_ACTION_LOCATION_COPY, GTK_STOCK_COPY,
  /* label, accelerator */       NULL, "",
  /* tooltip */                  N_("Prepare this folder to be copied with a Paste command"),
				 G_CALLBACK (action_location_copy_callback) },
  /* name, stock id */         { NEMO_ACTION_LOCATION_PASTE_FILES_INTO, GTK_STOCK_PASTE,
  /* label, accelerator */       N_("_Paste Into Folder"), "",
  /* tooltip */                  N_("Move or copy files previously selected by a Cut or Copy command into this folder"),
				 G_CALLBACK (action_location_paste_files_into_callback) },

  /* name, stock id */         { NEMO_ACTION_LOCATION_TRASH, NULL,
  /* label, accelerator */       N_("Mo_ve to Trash"), "",
  /* tooltip */                  N_("Move this folder to the Trash"),
				 G_CALLBACK (action_location_trash_callback) },
  /* name, stock id */         { NEMO_ACTION_LOCATION_DELETE, NEMO_ICON_DELETE,
  /* label, accelerator */       N_("_Delete"), "",
  /* tooltip */                  N_("Delete this folder, without moving to the Trash"),
				 G_CALLBACK (action_location_delete_callback) },
  /* name, stock id */         { NEMO_ACTION_LOCATION_RESTORE_FROM_TRASH, NULL,
  /* label, accelerator */       N_("_Restore"), NULL, NULL,
				 G_CALLBACK (action_location_restore_from_trash_callback) },

  /* name, stock id */         { "Location Mount Volume", NULL,
  /* label, accelerator */       N_("_Mount"), NULL,
  /* tooltip */                  N_("Mount the volume associated with this folder"),
				 G_CALLBACK (action_location_mount_volume_callback) },
  /* name, stock id */         { "Location Unmount Volume", NULL,
  /* label, accelerator */       N_("_Unmount"), NULL,
  /* tooltip */                  N_("Unmount the volume associated with this folder"),
				 G_CALLBACK (action_location_unmount_volume_callback) },
  /* name, stock id */         { "Location Eject Volume", NULL,
  /* label, accelerator */       N_("_Eject"), NULL,
  /* tooltip */                  N_("Eject the volume associated with this folder"),
				 G_CALLBACK (action_location_eject_volume_callback) },
  /* name, stock id */         { "Location Start Volume", NULL,
  /* label, accelerator */       N_("_Start"), NULL,
  /* tooltip */                  N_("Start the volume associated with this folder"),
				 G_CALLBACK (action_location_start_volume_callback) },
  /* name, stock id */         { "Location Stop Volume", NULL,
  /* label, accelerator */       N_("_Stop"), NULL,
  /* tooltip */                  N_("Stop the volume associated with this folder"),
				 G_CALLBACK (action_location_stop_volume_callback) },
  /* name, stock id */         { "Location Poll", NULL,
  /* label, accelerator */       N_("_Detect Media"), NULL,
  /* tooltip */                  N_("Detect media in the selected drive"),
				 G_CALLBACK (action_location_detect_media_callback) },

  /* name, stock id */         { "LocationProperties", GTK_STOCK_PROPERTIES,
  /* label, accelerator */       N_("_Properties"), NULL,
  /* tooltip */                  N_("View or modify the properties of this folder"),
				 G_CALLBACK (action_location_properties_callback) },

  /* name, stock id, label */  {NEMO_ACTION_COPY_TO_NEXT_PANE, NULL, N_("_Other pane"),
				NULL, N_("Copy the current selection to the other pane in the window"),
				G_CALLBACK (action_copy_to_next_pane_callback) },
  /* name, stock id, label */  {NEMO_ACTION_MOVE_TO_NEXT_PANE, NULL, N_("_Other pane"),
				NULL, N_("Move the current selection to the other pane in the window"),
				G_CALLBACK (action_move_to_next_pane_callback) },
  /* name, stock id, label */  {NEMO_ACTION_COPY_TO_HOME, NEMO_ICON_HOME,
				N_("_Home"), NULL,
				N_("Copy the current selection to the home folder"),
				G_CALLBACK (action_copy_to_home_callback) },
  /* name, stock id, label */  {NEMO_ACTION_MOVE_TO_HOME, NEMO_ICON_HOME,
				N_("_Home"), NULL,
				N_("Move the current selection to the home folder"),
				G_CALLBACK (action_move_to_home_callback) },
  /* name, stock id, label */  {NEMO_ACTION_COPY_TO_DESKTOP, NEMO_ICON_DESKTOP,
				N_("_Desktop"), NULL,
				N_("Copy the current selection to the desktop"),
				G_CALLBACK (action_copy_to_desktop_callback) },
  /* name, stock id, label */  {NEMO_ACTION_MOVE_TO_DESKTOP, NEMO_ICON_DESKTOP,
				N_("_Desktop"), NULL,
				N_("Move the current selection to the desktop"),
				G_CALLBACK (action_move_to_desktop_callback) },
                               {NEMO_ACTION_BROWSE_MOVE_TO, GTK_STOCK_DIRECTORY,
                N_("Browse…"), NULL,
                N_("Browse for a folder to move the selection to"),
                G_CALLBACK (action_browse_for_move_to_folder_callback) },
                               {NEMO_ACTION_BROWSE_COPY_TO, GTK_STOCK_DIRECTORY,
                N_("Browse…"), NULL,
                N_("Browse for a folder to copy the selection to"),
                G_CALLBACK (action_browse_for_copy_to_folder_callback) }
};

static void
connect_proxy (NemoView *view,
	       GtkAction *action,
	       GtkWidget *proxy,
	       GtkActionGroup *action_group)
{
	GdkPixbuf *pixbuf;
	GtkWidget *image;

	if (strcmp (gtk_action_get_name (action), NEMO_ACTION_NEW_EMPTY_DOCUMENT) == 0 &&
	    GTK_IS_IMAGE_MENU_ITEM (proxy)) {
		pixbuf = nemo_ui_get_menu_icon ("text-x-generic");
		if (pixbuf != NULL) {
			image = gtk_image_new_from_pixbuf (pixbuf);
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (proxy), image);

			g_object_unref (pixbuf);
		}
	}
}

static void
pre_activate (NemoView *view,
	      GtkAction *action,
	      GtkActionGroup *action_group)
{
	GdkEvent *event;
	GtkWidget *proxy;
	gboolean activated_from_popup;

	/* check whether action was activated through a popup menu.
	 * If not, unset the last stored context menu popup position */
	activated_from_popup = FALSE;

	event = gtk_get_current_event ();
	proxy = gtk_get_event_widget (event);

	if (proxy != NULL) {
		GtkWidget *toplevel;
		GdkWindowTypeHint hint;

		toplevel = gtk_widget_get_toplevel (proxy);

		if (GTK_IS_WINDOW (toplevel)) {
			hint = gtk_window_get_type_hint (GTK_WINDOW (toplevel));

			if (hint == GDK_WINDOW_TYPE_HINT_POPUP_MENU) {
				activated_from_popup = TRUE;
			}
		}
	}

	if (!activated_from_popup) {
		update_context_menu_position_from_event (view, NULL);
	}
}

static void
real_merge_menus (NemoView *view)
{
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkAction *action;
	char *tooltip;

	ui_manager = nemo_window_get_ui_manager (view->details->window);

	action_group = gtk_action_group_new ("DirViewActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	view->details->dir_action_group = action_group;
	gtk_action_group_add_actions (action_group, 
				      directory_view_entries, G_N_ELEMENTS (directory_view_entries),
				      view);

	/* Translators: %s is a directory */
	tooltip = g_strdup_printf (_("Run or manage scripts from %s"), "~/.gnome2/nemo-scripts");
	/* Create a script action here specially because its tooltip is dynamic */
	action = gtk_action_new ("Scripts", _("_Scripts"), tooltip, NULL);
	gtk_action_group_add_action (action_group, action);
	g_object_unref (action);
	g_free (tooltip);

	action = gtk_action_group_get_action (action_group, NEMO_ACTION_NO_TEMPLATES);
	gtk_action_set_sensitive (action, FALSE);

	g_signal_connect_object (action_group, "connect-proxy",
				 G_CALLBACK (connect_proxy), G_OBJECT (view),
				 G_CONNECT_SWAPPED);
	g_signal_connect_object (action_group, "pre-activate",
				 G_CALLBACK (pre_activate), G_OBJECT (view),
				 G_CONNECT_SWAPPED);

	/* Insert action group at end so clipboard action group ends up before it */
	gtk_ui_manager_insert_action_group (ui_manager, action_group, -1);
	g_object_unref (action_group); /* owned by ui manager */

	view->details->dir_merge_id = gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/nemo/nemo-directory-view-ui.xml", NULL);
	
	view->details->scripts_invalid = TRUE;
	view->details->templates_invalid = TRUE;
    view->details->actions_invalid = TRUE;
}

static gboolean
can_paste_into_file (NemoFile *file)
{
	if (nemo_file_is_directory (file) &&
	    nemo_file_can_write (file)) {
		return TRUE;
	}
	if (nemo_file_has_activation_uri (file)) {
		GFile *location;
		NemoFile *activation_file;
		gboolean res;
		
		location = nemo_file_get_activation_location (file);
		activation_file = nemo_file_get (location);
		g_object_unref (location);
	
		/* The target location might not have data for it read yet,
		   and we can't want to do sync I/O, so treat the unknown
		   case as can-write */
		res = (nemo_file_get_file_type (activation_file) == G_FILE_TYPE_UNKNOWN) ||
			(nemo_file_get_file_type (activation_file) == G_FILE_TYPE_DIRECTORY &&
			 nemo_file_can_write (activation_file));

		nemo_file_unref (activation_file);
		
		return res;
	}
	
	return FALSE;
}

static void
clipboard_targets_received (GtkClipboard     *clipboard,
                            GdkAtom          *targets,
                            int               n_targets,
			    gpointer          user_data)
{
	NemoView *view;
	gboolean can_paste;
	int i;
	GList *selection;
	int count;
	GtkAction *action;

	view = NEMO_VIEW (user_data);
	can_paste = FALSE;

	if (view->details->window == NULL ||
	    !view->details->active) {
		/* We've been destroyed or became inactive since call */
		g_object_unref (view);
		return;
	}

	if (targets) {
		for (i=0; i < n_targets; i++) {
			if (targets[i] == copied_files_atom) {
				can_paste = TRUE;
			}
		}
	}
	
	
	selection = nemo_view_get_selection (view);
	count = g_list_length (selection);
	
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_PASTE);
	gtk_action_set_sensitive (action,
				  can_paste && !nemo_view_is_read_only (view));

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_PASTE_FILES_INTO);
	gtk_action_set_sensitive (action,
	                          can_paste && count == 1 &&
	                          can_paste_into_file (NEMO_FILE (selection->data)));

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_LOCATION_PASTE_FILES_INTO);
	g_object_set_data (G_OBJECT (action),
			   "can-paste-according-to-clipboard",
			   GINT_TO_POINTER (can_paste));
	gtk_action_set_sensitive (action,
				  GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action),
								      "can-paste-according-to-clipboard")) &&
				  GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action),
								      "can-paste-according-to-destination")));

	nemo_file_list_free (selection);
	
	g_object_unref (view);
}

static gboolean
should_show_empty_trash (NemoView *view)
{
	return (showing_trash_directory (view));
}

static gboolean
file_list_all_are_folders (GList *file_list)
{
	GList *l;
	NemoFile *file, *linked_file;
	char *activation_uri;
	gboolean is_dir;
	
	for (l = file_list; l != NULL; l = l->next) {
		file = NEMO_FILE (l->data);
		if (nemo_file_is_nemo_link (file) &&
		    !NEMO_IS_DESKTOP_ICON_FILE (file)) {
			if (nemo_file_is_launcher (file)) {
				return FALSE;
			}
				
			activation_uri = nemo_file_get_activation_uri (file);
			
			if (activation_uri == NULL) {
				g_free (activation_uri);
				return FALSE;
			}

			linked_file = nemo_file_get_existing_by_uri (activation_uri);

			/* We might not actually know the type of the linked file yet,
			 * however we don't want to schedule a read, since that might do things
			 * like ask for password etc. This is a bit unfortunate, but I don't
			 * know any way around it, so we do various heuristics here
			 * to get things mostly right 
			 */
			is_dir =
				(linked_file != NULL &&
				 nemo_file_is_directory (linked_file)) ||
				(activation_uri != NULL &&
				 activation_uri[strlen (activation_uri) - 1] == '/');
			
			nemo_file_unref (linked_file);
			g_free (activation_uri);
			
			if (!is_dir) {
				return FALSE;
			}
		} else if (!(nemo_file_is_directory (file) ||
			     NEMO_IS_DESKTOP_ICON_FILE (file))) {
			return FALSE;
		}
	}
	return TRUE;
}

static void
file_should_show_foreach (NemoFile        *file,
			  gboolean            *show_mount,
			  gboolean            *show_unmount,
			  gboolean            *show_eject,
			  gboolean            *show_connect,
			  gboolean            *show_start,
			  gboolean            *show_stop,
			  gboolean            *show_poll,
			  GDriveStartStopType *start_stop_type)
{
	char *uri;

	*show_mount = FALSE;
	*show_unmount = FALSE;
	*show_eject = FALSE;
	*show_connect = FALSE;
	*show_start = FALSE;
	*show_stop = FALSE;
	*show_poll = FALSE;

	if (nemo_file_can_eject (file)) {
		*show_eject = TRUE;
	}

	if (nemo_file_can_mount (file)) {
		*show_mount = TRUE;
	}

	if (nemo_file_can_start (file) || nemo_file_can_start_degraded (file)) {
		*show_start = TRUE;
	}

	if (nemo_file_can_stop (file)) {
		*show_stop = TRUE;
	}

	/* Dot not show both Unmount and Eject/Safe Removal; too confusing to
	 * have too many menu entries */
	if (nemo_file_can_unmount (file) && !*show_eject && !*show_stop) {
		*show_unmount = TRUE;
	}

	if (nemo_file_can_poll_for_media (file) && !nemo_file_is_media_check_automatic (file)) {
		*show_poll = TRUE;
	}

	*start_stop_type = nemo_file_get_start_stop_type (file);

	if (nemo_file_is_nemo_link (file)) {
		uri = nemo_file_get_activation_uri (file);
		if (uri != NULL &&
		    (g_str_has_prefix (uri, "ftp:") ||
		     g_str_has_prefix (uri, "ssh:") ||
		     g_str_has_prefix (uri, "sftp:") ||
		     g_str_has_prefix (uri, "dav:") ||
		     g_str_has_prefix (uri, "davs:"))) {
			*show_connect = TRUE;
		}
		g_free (uri);
	}
}

static void
file_should_show_self (NemoFile        *file,
		       gboolean            *show_mount,
		       gboolean            *show_unmount,
		       gboolean            *show_eject,
		       gboolean            *show_start,
		       gboolean            *show_stop,
		       gboolean            *show_poll,
		       GDriveStartStopType *start_stop_type)
{
	*show_mount = FALSE;
	*show_unmount = FALSE;
	*show_eject = FALSE;
	*show_start = FALSE;
	*show_stop = FALSE;
	*show_poll = FALSE;

	if (file == NULL) {
		return;
	}

	if (nemo_file_can_eject (file)) {
		*show_eject = TRUE;
	}

	if (nemo_file_can_mount (file)) {
		*show_mount = TRUE;
	}

	if (nemo_file_can_start (file) || nemo_file_can_start_degraded (file)) {
		*show_start = TRUE;
	}

	if (nemo_file_can_stop (file)) {
		*show_stop = TRUE;
	}

	/* Dot not show both Unmount and Eject/Safe Removal; too confusing to
	 * have too many menu entries */
	if (nemo_file_can_unmount (file) && !*show_eject && !*show_stop) {
		*show_unmount = TRUE;
	}

	if (nemo_file_can_poll_for_media (file) && !nemo_file_is_media_check_automatic (file)) {
		*show_poll = TRUE;
	}

	*start_stop_type = nemo_file_get_start_stop_type (file);

}

static gboolean
files_are_all_directories (GList *files)
{
	NemoFile *file;
	GList *l;
	gboolean all_directories;

	all_directories = TRUE;

	for (l = files; l != NULL; l = l->next) {
		file = NEMO_FILE (l->data);
		all_directories &= nemo_file_is_directory (file);
	}

	return all_directories;
}

static gboolean
files_is_none_directory (GList *files)
{
	NemoFile *file;
	GList *l;
	gboolean no_directory;

	no_directory = TRUE;

	for (l = files; l != NULL; l = l->next) {
		file = NEMO_FILE (l->data);
		no_directory &= !nemo_file_is_directory (file);
	}

	return no_directory;
}

static void
update_restore_from_trash_action (GtkAction *action,
				  GList *files,
				  gboolean is_self)
{
	NemoFile *original_file;
	NemoFile *original_dir;
	GHashTable *original_dirs_hash;
	GList *original_dirs;
	GFile *original_location;
	char *tooltip, *original_name;

	original_file = NULL;
	original_dir = NULL;
	original_dirs = NULL;
	original_dirs_hash = NULL;
	original_location = NULL;
	original_name = NULL;

	if (files != NULL) {
		if (g_list_length (files) == 1) {
			original_file = nemo_file_get_trash_original_file (files->data);
		} else {
			original_dirs_hash = nemo_trashed_files_get_original_directories (files, NULL);
			if (original_dirs_hash != NULL) {
				original_dirs = g_hash_table_get_keys (original_dirs_hash);
				if (g_list_length (original_dirs) == 1) {
					original_dir = nemo_file_ref (NEMO_FILE (original_dirs->data));
				}
			}
		}
	}

	if (original_file != NULL || original_dirs != NULL) {
		gtk_action_set_visible (action, TRUE);

		if (original_file != NULL) {
			original_location = nemo_file_get_location (original_file);
		} else if (original_dir != NULL) {
			original_location = nemo_file_get_location (original_dir);
		}

		if (original_location != NULL) {
			original_name = g_file_get_parse_name (original_location);
		}

		if (is_self) {
			g_assert (g_list_length (files) == 1);
			g_assert (original_location != NULL);
			tooltip = g_strdup_printf (_("Move the open folder out of the trash to \"%s\""), original_name);
		} else if (files_are_all_directories (files)) {
			if (original_name != NULL) {
				tooltip = g_strdup_printf (ngettext ("Move the selected folder out of the trash to \"%s\"",
								     "Move the selected folders out of the trash to \"%s\"",
								     g_list_length (files)), original_name);
			} else {
				tooltip = g_strdup_printf (ngettext ("Move the selected folder out of the trash",
								     "Move the selected folders out of the trash",
								     g_list_length (files)));
			}
		} else if (files_is_none_directory (files)) {
			if (original_name != NULL) {
				tooltip = g_strdup_printf (ngettext ("Move the selected file out of the trash to \"%s\"",
								     "Move the selected files out of the trash to \"%s\"",
								     g_list_length (files)), original_name);
			} else {
				tooltip = g_strdup_printf (ngettext ("Move the selected file out of the trash",
								     "Move the selected files out of the trash",
								     g_list_length (files)));
			}
		} else {
			if (original_name != NULL) {
				tooltip = g_strdup_printf (ngettext ("Move the selected item out of the trash to \"%s\"",
								     "Move the selected items out of the trash to \"%s\"",
								     g_list_length (files)), original_name);
			} else {
				tooltip = g_strdup_printf (ngettext ("Move the selected item out of the trash",
								     "Move the selected items out of the trash",
								     g_list_length (files)));
			}
		}
		g_free (original_name);

		g_object_set (action, "tooltip", tooltip, NULL);
		g_free (tooltip);
		
		if (original_location != NULL) {
			g_object_unref (original_location);
		}
	} else {
		gtk_action_set_visible (action, FALSE);
	}

	nemo_file_unref (original_file);
	nemo_file_unref (original_dir);
	g_list_free (original_dirs);

	if (original_dirs_hash != NULL) {
		g_hash_table_destroy (original_dirs_hash);
	}
}

static void
real_update_menus_volumes (NemoView *view,
			   GList *selection,
			   gint selection_count)
{
	GList *l;
	NemoFile *file;
	gboolean show_mount;
	gboolean show_unmount;
	gboolean show_eject;
	gboolean show_connect;
	gboolean show_start;
	gboolean show_stop;
	gboolean show_poll;
	GDriveStartStopType start_stop_type;
	gboolean show_self_mount;
	gboolean show_self_unmount;
	gboolean show_self_eject;
	gboolean show_self_start;
	gboolean show_self_stop;
	gboolean show_self_poll;
	GDriveStartStopType self_start_stop_type;
	GtkAction *action;

	show_mount = (selection != NULL);
	show_unmount = (selection != NULL);
	show_eject = (selection != NULL);
	show_connect = (selection != NULL && selection_count == 1);
	show_start = (selection != NULL && selection_count == 1);
	show_stop = (selection != NULL && selection_count == 1);
	show_poll = (selection != NULL && selection_count == 1);
	start_stop_type = G_DRIVE_START_STOP_TYPE_UNKNOWN;
	self_start_stop_type = G_DRIVE_START_STOP_TYPE_UNKNOWN;

	for (l = selection; l != NULL && (show_mount || show_unmount
					  || show_eject || show_connect
                                          || show_start || show_stop
					  || show_poll);
	     l = l->next) {
		gboolean show_mount_one;
		gboolean show_unmount_one;
		gboolean show_eject_one;
		gboolean show_connect_one;
		gboolean show_start_one;
		gboolean show_stop_one;
		gboolean show_poll_one;

		file = NEMO_FILE (l->data);
		file_should_show_foreach (file,
					  &show_mount_one,
					  &show_unmount_one,
					  &show_eject_one,
					  &show_connect_one,
                                          &show_start_one,
                                          &show_stop_one,
					  &show_poll_one,
					  &start_stop_type);

		show_mount &= show_mount_one;
		show_unmount &= show_unmount_one;
		show_eject &= show_eject_one;
		show_connect &= show_connect_one;
		show_start &= show_start_one;
		show_stop &= show_stop_one;
		show_poll &= show_poll_one;
	}

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_CONNECT_TO_SERVER_LINK);
	gtk_action_set_visible (action, show_connect);
	
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_MOUNT_VOLUME);
	gtk_action_set_visible (action, show_mount);
	
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_UNMOUNT_VOLUME);
	gtk_action_set_visible (action, show_unmount);
	
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_EJECT_VOLUME);
	gtk_action_set_visible (action, show_eject);
	
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_START_VOLUME);
	gtk_action_set_visible (action, show_start);
	if (show_start) {
		switch (start_stop_type) {
		default:
		case G_DRIVE_START_STOP_TYPE_UNKNOWN:
			gtk_action_set_label (action, _("_Start"));
			gtk_action_set_tooltip (action, _("Start the selected drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
			gtk_action_set_label (action, _("_Start"));
			gtk_action_set_tooltip (action, _("Start the selected drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_NETWORK:
			gtk_action_set_label (action, _("_Connect"));
			gtk_action_set_tooltip (action, _("Connect to the selected drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_MULTIDISK:
			gtk_action_set_label (action, _("_Start Multi-disk Drive"));
			gtk_action_set_tooltip (action, _("Start the selected multi-disk drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_PASSWORD:
			gtk_action_set_label (action, _("U_nlock Drive"));
			gtk_action_set_tooltip (action, _("Unlock the selected drive"));
			break;
		}
	}

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_STOP_VOLUME);
	gtk_action_set_visible (action, show_stop);
	if (show_stop) {
		switch (start_stop_type) {
		default:
		case G_DRIVE_START_STOP_TYPE_UNKNOWN:
			gtk_action_set_label (action, _("_Stop"));
			gtk_action_set_tooltip (action, _("Stop the selected drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
			gtk_action_set_label (action, _("_Safely Remove Drive"));
			gtk_action_set_tooltip (action, _("Safely remove the selected drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_NETWORK:
			gtk_action_set_label (action, _("_Disconnect"));
			gtk_action_set_tooltip (action, _("Disconnect the selected drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_MULTIDISK:
			gtk_action_set_label (action, _("_Stop Multi-disk Drive"));
			gtk_action_set_tooltip (action, _("Stop the selected multi-disk drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_PASSWORD:
			gtk_action_set_label (action, _("_Lock Drive"));
			gtk_action_set_tooltip (action, _("Lock the selected drive"));
			break;
		}
	}

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_POLL);
	gtk_action_set_visible (action, show_poll);

	show_self_mount = show_self_unmount = show_self_eject =
		show_self_start = show_self_stop = show_self_poll = FALSE;

	file = nemo_view_get_directory_as_file (view);
	file_should_show_self (file,
			       &show_self_mount,
			       &show_self_unmount,
			       &show_self_eject,
			       &show_self_start,
			       &show_self_stop,
			       &show_self_poll,
			       &self_start_stop_type);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_SELF_MOUNT_VOLUME);
	gtk_action_set_visible (action, show_self_mount);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_SELF_UNMOUNT_VOLUME);
	gtk_action_set_visible (action, show_self_unmount);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_SELF_EJECT_VOLUME);
	gtk_action_set_visible (action, show_self_eject);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_SELF_START_VOLUME);
	gtk_action_set_visible (action, show_self_start);
	if (show_self_start) {
		switch (self_start_stop_type) {
		default:
		case G_DRIVE_START_STOP_TYPE_UNKNOWN:
			gtk_action_set_label (action, _("_Start"));
			gtk_action_set_tooltip (action, _("Start the drive associated with the open folder"));
			break;
		case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
			gtk_action_set_label (action, _("_Start"));
			gtk_action_set_tooltip (action, _("Start the drive associated with the open folder"));
			break;
		case G_DRIVE_START_STOP_TYPE_NETWORK:
			gtk_action_set_label (action, _("_Connect"));
			gtk_action_set_tooltip (action, _("Connect to the drive associated with the open folder"));
			break;
		case G_DRIVE_START_STOP_TYPE_MULTIDISK:
			gtk_action_set_label (action, _("_Start Multi-disk Drive"));
			gtk_action_set_tooltip (action, _("Start the multi-disk drive associated with the open folder"));
			break;
		case G_DRIVE_START_STOP_TYPE_PASSWORD:
			gtk_action_set_label (action, _("_Unlock Drive"));
			gtk_action_set_tooltip (action, _("Unlock the drive associated with the open folder"));
			break;
		}
	}

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_SELF_STOP_VOLUME);
	gtk_action_set_visible (action, show_self_stop);
	if (show_self_stop) {
		switch (self_start_stop_type) {
		default:
		case G_DRIVE_START_STOP_TYPE_UNKNOWN:
			gtk_action_set_label (action, _("_Stop"));
			gtk_action_set_tooltip (action, _("_Stop the drive associated with the open folder"));
			break;
		case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
			gtk_action_set_label (action, _("_Safely Remove Drive"));
			gtk_action_set_tooltip (action, _("Safely remove the drive associated with the open folder"));
			break;
		case G_DRIVE_START_STOP_TYPE_NETWORK:
			gtk_action_set_label (action, _("_Disconnect"));
			gtk_action_set_tooltip (action, _("Disconnect the drive associated with the open folder"));
			break;
		case G_DRIVE_START_STOP_TYPE_MULTIDISK:
			gtk_action_set_label (action, _("_Stop Multi-disk Drive"));
			gtk_action_set_tooltip (action, _("Stop the multi-disk drive associated with the open folder"));
			break;
		case G_DRIVE_START_STOP_TYPE_PASSWORD:
			gtk_action_set_label (action, _("_Lock Drive"));
			gtk_action_set_tooltip (action, _("Lock the drive associated with the open folder"));
			break;
		}
	}

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_SELF_POLL);
	gtk_action_set_visible (action, show_self_poll);

}

static void
real_update_location_menu_volumes (NemoView *view)
{
	GtkAction *action;
	NemoFile *file;
	gboolean show_mount;
	gboolean show_unmount;
	gboolean show_eject;
	gboolean show_connect;
	gboolean show_start;
	gboolean show_stop;
	gboolean show_poll;
	GDriveStartStopType start_stop_type;

	g_assert (NEMO_IS_VIEW (view));
	g_assert (NEMO_IS_FILE (view->details->location_popup_directory_as_file));

	file = NEMO_FILE (view->details->location_popup_directory_as_file);
	file_should_show_foreach (file,
				  &show_mount,
				  &show_unmount,
				  &show_eject,
				  &show_connect,
				  &show_start,
				  &show_stop,
				  &show_poll,
				  &start_stop_type);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_LOCATION_MOUNT_VOLUME);
	gtk_action_set_visible (action, show_mount);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_LOCATION_UNMOUNT_VOLUME);
	gtk_action_set_visible (action, show_unmount);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_LOCATION_EJECT_VOLUME);
	gtk_action_set_visible (action, show_eject);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_LOCATION_START_VOLUME);
	gtk_action_set_visible (action, show_start);
	if (show_start) {
		switch (start_stop_type) {
		default:
		case G_DRIVE_START_STOP_TYPE_UNKNOWN:
			gtk_action_set_label (action, _("_Start"));
			gtk_action_set_tooltip (action, _("Start the selected drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
			gtk_action_set_label (action, _("_Start"));
			gtk_action_set_tooltip (action, _("Start the selected drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_NETWORK:
			gtk_action_set_label (action, _("_Connect"));
			gtk_action_set_tooltip (action, _("Connect to the selected drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_MULTIDISK:
			gtk_action_set_label (action, _("_Start Multi-disk Drive"));
			gtk_action_set_tooltip (action, _("Start the selected multi-disk drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_PASSWORD:
			gtk_action_set_label (action, _("_Unlock Drive"));
			gtk_action_set_tooltip (action, _("Unlock the selected drive"));
			break;
		}
	}

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_LOCATION_STOP_VOLUME);
	gtk_action_set_visible (action, show_stop);
	if (show_stop) {
		switch (start_stop_type) {
		default:
		case G_DRIVE_START_STOP_TYPE_UNKNOWN:
			gtk_action_set_label (action, _("_Stop"));
			gtk_action_set_tooltip (action, _("Stop the selected volume"));
			break;
		case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
			gtk_action_set_label (action, _("_Safely Remove Drive"));
			gtk_action_set_tooltip (action, _("Safely remove the selected drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_NETWORK:
			gtk_action_set_label (action, _("_Disconnect"));
			gtk_action_set_tooltip (action, _("Disconnect the selected drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_MULTIDISK:
			gtk_action_set_label (action, _("_Stop Multi-disk Drive"));
			gtk_action_set_tooltip (action, _("Stop the selected multi-disk drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_PASSWORD:
			gtk_action_set_label (action, _("_Lock Drive"));
			gtk_action_set_tooltip (action, _("Lock the selected drive"));
			break;
		}
	}

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_LOCATION_POLL);
	gtk_action_set_visible (action, show_poll);
}

/* TODO: we should split out this routine into two functions:
 * Update on clipboard changes
 * Update on selection changes
 */
static void
real_update_paste_menu (NemoView *view,
			GList *selection,
			gint selection_count)
{
	gboolean can_paste_files_into;
	gboolean selection_is_read_only;
	gboolean selection_contains_recent;
	gboolean is_read_only;
	GtkAction *action;

	selection_is_read_only = selection_count == 1 &&
		(!nemo_file_can_write (NEMO_FILE (selection->data)) &&
		 !nemo_file_has_activation_uri (NEMO_FILE (selection->data)));

	is_read_only = nemo_view_is_read_only (view);
	selection_contains_recent = showing_recent_directory (view);

	can_paste_files_into = (!selection_contains_recent &&
				selection_count == 1 &&
	                        can_paste_into_file (NEMO_FILE (selection->data)));

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_PASTE);
	gtk_action_set_sensitive (action, !is_read_only);
	gtk_action_set_visible (action, !selection_contains_recent);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_PASTE_FILES_INTO);
	gtk_action_set_visible (action, can_paste_files_into);
	gtk_action_set_sensitive (action, !selection_is_read_only);

	/* Ask the clipboard */
	g_object_ref (view); /* Need to keep the object alive until we get the reply */
	gtk_clipboard_request_targets (nemo_clipboard_get (GTK_WIDGET (view)),
				       clipboard_targets_received,
				       view);
}

static void
real_update_location_menu (NemoView *view)
{
	GtkAction *action;
	NemoFile *file;
	gboolean is_special_link;
	gboolean is_desktop_or_home_dir;
	gboolean is_recent;
	gboolean can_delete_file, show_delete;
	gboolean show_separate_delete_command;
	gboolean show_open_in_new_tab;
	gboolean show_open_alternate;
	GList l;
	char *label;
	char *tip;

	show_open_in_new_tab = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_ALWAYS_USE_BROWSER);
	show_open_alternate = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_ALWAYS_USE_BROWSER);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_LOCATION_OPEN_ALTERNATE);
	gtk_action_set_visible (action, show_open_alternate);

	label = _("Open in New _Window");
	g_object_set (action,
		      "label", label,
		      NULL);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_LOCATION_OPEN_IN_NEW_TAB);
	gtk_action_set_visible (action, show_open_in_new_tab);

	label = _("Open in New _Tab");
	g_object_set (action,
		      "label", label,
		      NULL);

	file = view->details->location_popup_directory_as_file;
	g_assert (NEMO_IS_FILE (file));
	g_assert (nemo_file_check_if_ready (file, NEMO_FILE_ATTRIBUTE_INFO |
						NEMO_FILE_ATTRIBUTE_MOUNT |
						NEMO_FILE_ATTRIBUTE_FILESYSTEM_INFO));

	is_special_link = NEMO_IS_DESKTOP_ICON_FILE (file);
	is_desktop_or_home_dir = nemo_file_is_home (file)
		|| nemo_file_is_desktop_directory (file);
	is_recent = nemo_file_is_in_recent (file);

	can_delete_file =
		nemo_file_can_delete (file) &&
		!is_special_link &&
		!is_desktop_or_home_dir;

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_LOCATION_CUT);
	gtk_action_set_sensitive (action, !is_recent && can_delete_file);
	gtk_action_set_visible (action, !is_recent);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_LOCATION_PASTE_FILES_INTO);
	g_object_set_data (G_OBJECT (action),
			   "can-paste-according-to-destination",
			   GINT_TO_POINTER (can_paste_into_file (file)));
	gtk_action_set_sensitive (action,
				  !is_recent &&
				  GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action),
								      "can-paste-according-to-clipboard")) &&
				  GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action),
								      "can-paste-according-to-destination")));
	gtk_action_set_visible (action, !is_recent);

	show_delete = TRUE;

	if (file != NULL &&
	    nemo_file_is_in_trash (file)) {
		if (nemo_file_is_self_owned (file)) {
			show_delete = FALSE;
		}

		label = _("_Delete Permanently");
		tip = _("Delete the open folder permanently");
		show_separate_delete_command = FALSE;
	} else {
		label = _("Mo_ve to Trash");
		tip = _("Move the open folder to the Trash");
		show_separate_delete_command = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_ENABLE_DELETE);
	}

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_LOCATION_TRASH);
	g_object_set (action,
		      "label", label,
		      "tooltip", tip,
		      "icon-name", (file != NULL &&
				    nemo_file_is_in_trash (file)) ?
		      NEMO_ICON_DELETE : NEMO_ICON_TRASH_FULL,
		      NULL);
	gtk_action_set_sensitive (action, can_delete_file);
	gtk_action_set_visible (action, show_delete);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_LOCATION_DELETE);
	gtk_action_set_visible (action, show_separate_delete_command);
	if (show_separate_delete_command) {
		gtk_action_set_sensitive (action, can_delete_file);
		g_object_set (action,
			      "icon-name", NEMO_ICON_DELETE,
			      "sensitive", can_delete_file,
			      NULL);
	}

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_LOCATION_RESTORE_FROM_TRASH);
	l.prev = NULL;
	l.next = NULL;
	l.data = file;
	update_restore_from_trash_action (action, &l, TRUE);

	real_update_location_menu_volumes (view);
}

static void
clipboard_changed_callback (NemoClipboardMonitor *monitor, NemoView *view)
{
	GList *selection;
	gint selection_count;

	if (!view->details->active) {
		return;
	}

	selection = nemo_view_get_selection (view);
	selection_count = g_list_length (selection);

	real_update_paste_menu (view, selection, selection_count);

	nemo_file_list_free (selection);
	
}

static gboolean
can_delete_all (GList *files)
{
	NemoFile *file;
	GList *l;

	for (l = files; l != NULL; l = l->next) {
		file = l->data;
		if (!nemo_file_can_delete (file)) {
			return FALSE;
		}
	}
	return TRUE;
}

static gboolean
can_trash_all (GList *files)
{
	NemoFile *file;
	GList *l;

	for (l = files; l != NULL; l = l->next) {
		file = l->data;
		if (!nemo_file_can_trash (file)) {
			return FALSE;
		}
	}
	return TRUE;
}

static gboolean
has_writable_extra_pane (NemoView *view)
{
	NemoView *other_view;

	other_view = get_directory_view_of_extra_pane (view);
	if (other_view != NULL) {
		return !nemo_view_is_read_only (other_view);
	}
	return FALSE;
}

static void
real_update_menus (NemoView *view)
{
	GList *selection, *l;
	gint selection_count;
	const char *tip, *label;
	char *label_with_underscore;
	gboolean selection_contains_special_link;
	gboolean selection_contains_desktop_or_home_dir;
	gboolean selection_contains_directory;
	gboolean selection_contains_recent;
	gboolean can_create_files;
	gboolean can_delete_files;
	gboolean can_trash_files;
	gboolean can_copy_files;
	gboolean can_link_files;
	gboolean can_duplicate_files;
	gboolean show_separate_delete_command;
	gboolean show_open_alternate;
	gboolean show_open_in_new_tab;
	gboolean can_open;
	gboolean show_app;
	gboolean show_save_search;
	gboolean save_search_sensitive;
	gboolean show_save_search_as;
	gboolean show_desktop_target;
	GtkAction *action;
	GAppInfo *app;
	GIcon *app_icon;
	GtkWidget *menuitem;
	gboolean next_pane_is_writable;
	gboolean show_properties;

	selection = nemo_view_get_selection (view);
	selection_count = g_list_length (selection);

	selection_contains_special_link = special_link_in_selection (view);
	selection_contains_desktop_or_home_dir = desktop_or_home_dir_in_selection (view);
	selection_contains_directory = directory_in_selection (view);
	selection_contains_recent = showing_recent_directory (view);	

	can_create_files = nemo_view_supports_creating_files (view);
	can_delete_files =
		can_delete_all (selection) &&
		selection_count != 0 &&
		!selection_contains_special_link &&
		!selection_contains_desktop_or_home_dir;
	can_trash_files =
		can_trash_all (selection) &&
		selection_count != 0 &&
		!selection_contains_special_link &&
		!selection_contains_desktop_or_home_dir;
	can_copy_files = selection_count != 0
		&& !selection_contains_recent
		&& !selection_contains_special_link;

	can_duplicate_files = can_create_files && can_copy_files;
	can_link_files = can_create_files && can_copy_files;

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_RENAME);
	/* rename sensitivity depending on selection */
	if (selection_count > 1) {
		/* If multiple files are selected, sensitivity depends on whether a bulk renamer is registered. */
		gtk_action_set_sensitive (action, have_bulk_rename_tool ());
	} else {
		gtk_action_set_sensitive (action,
					  selection_count == 1 &&
					  nemo_view_can_rename_file (view, selection->data));
	}
	gtk_action_set_visible (action, !selection_contains_recent);

    gboolean no_selection_or_one_dir = ((selection_count == 1 && selection_contains_directory) ||
                                        selection_count == 0);

    gboolean show_open_as_root = (geteuid() != 0) && no_selection_or_one_dir;

    action = gtk_action_group_get_action (view->details->dir_action_group,
                                         NEMO_ACTION_OPEN_AS_ROOT);
    gtk_action_set_visible (action, show_open_as_root);

    action = gtk_action_group_get_action (view->details->dir_action_group,
                                         NEMO_ACTION_OPEN_IN_TERMINAL);
    gtk_action_set_visible (action, no_selection_or_one_dir);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_NEW_FOLDER);
	gtk_action_set_sensitive (action, can_create_files);
	gtk_action_set_visible (action, !selection_contains_recent);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_NEW_FOLDER_WITH_SELECTION);
	gtk_action_set_sensitive (action, can_create_files && can_delete_files && (selection_count > 1));
	gtk_action_set_visible (action, !selection_contains_recent && (selection_count > 1));
	label_with_underscore = g_strdup_printf (ngettext("New Folder with Selection (%'d Item)",
							  "New Folder with Selection (%'d Items)",
							  selection_count),
						 selection_count);
	g_object_set (action, "label", label_with_underscore, NULL);
	g_free (label_with_underscore);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_OPEN);
	gtk_action_set_sensitive (action, selection_count != 0);
	
	can_open = show_app = selection_count != 0;

	for (l = selection; l != NULL; l = l->next) {
		NemoFile *file;

		file = NEMO_FILE (selection->data);

		if (!nemo_mime_file_opens_in_external_app (file)) {
			show_app = FALSE;
		}

		if (!show_app) {
			break;
		}
	} 

	label_with_underscore = NULL;

	app = NULL;
	app_icon = NULL;

	if (can_open && show_app) {
		app = nemo_mime_get_default_application_for_files (selection);
	}

	if (app != NULL) {
		char *escaped_app;

		escaped_app = eel_str_double_underscores (g_app_info_get_name (app));
		label_with_underscore = g_strdup_printf (_("_Open With %s"),
							 escaped_app);

		app_icon = g_app_info_get_icon (app);
		if (app_icon != NULL) {
			g_object_ref (app_icon);
		}

		g_free (escaped_app);
		g_object_unref (app);
	}

	g_object_set (action, "label", 
		      label_with_underscore ? label_with_underscore : _("_Open"),
		      NULL);

	menuitem = gtk_ui_manager_get_widget (
					      nemo_window_get_ui_manager (view->details->window),
					      NEMO_VIEW_MENU_PATH_OPEN);

	/* Only force displaying the icon if it is an application icon */
	gtk_image_menu_item_set_always_show_image (
						   GTK_IMAGE_MENU_ITEM (menuitem), app_icon != NULL);

	menuitem = gtk_ui_manager_get_widget (
					      nemo_window_get_ui_manager (view->details->window),
					      NEMO_VIEW_POPUP_PATH_OPEN);

	/* Only force displaying the icon if it is an application icon */
	gtk_image_menu_item_set_always_show_image (
						   GTK_IMAGE_MENU_ITEM (menuitem), app_icon != NULL);

	if (app_icon == NULL) {
		app_icon = g_themed_icon_new (GTK_STOCK_OPEN);
	}

	gtk_action_set_gicon (action, app_icon);
	g_object_unref (app_icon);

	gtk_action_set_visible (action, can_open);
	
	g_free (label_with_underscore);

	show_open_alternate = file_list_all_are_folders (selection) &&
		selection_count > 0 &&
		g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_ALWAYS_USE_BROWSER) &&
		!NEMO_IS_DESKTOP_ICON_VIEW (view);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_OPEN_ALTERNATE);

	gtk_action_set_sensitive (action,  selection_count != 0);
	gtk_action_set_visible (action, show_open_alternate);

	if (selection_count == 0 || selection_count == 1) {
		label_with_underscore = g_strdup (_("Open in New _Window"));
	} else {
		label_with_underscore = g_strdup_printf (ngettext("Open in %'d New _Window",
								  "Open in %'d New _Windows",
								  selection_count), 
							 selection_count);
	}

	g_object_set (action, "label", 
		      label_with_underscore,
		      NULL);
	g_free (label_with_underscore);

	show_open_in_new_tab = show_open_alternate;
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_OPEN_IN_NEW_TAB);
	gtk_action_set_sensitive (action, selection_count != 0);
	gtk_action_set_visible (action, show_open_in_new_tab);

	if (selection_count == 0 || selection_count == 1) {
		label_with_underscore = g_strdup (_("Open in New _Tab"));
	} else {
		label_with_underscore = g_strdup_printf (ngettext("Open in %'d New _Tab",
								  "Open in %'d New _Tabs",
								  selection_count), 
							 selection_count);
	}

	g_object_set (action, "label", 
		      label_with_underscore,
		      NULL);
	g_free (label_with_underscore);

	/* Broken into its own function just for convenience */
	reset_open_with_menu (view, selection);
	reset_extension_actions_menu (view, selection);
    reset_move_copy_to_menu (view);

	if (all_selected_items_in_trash (view)) {
		label = _("_Delete Permanently");
		tip = _("Delete all selected items permanently");
		show_separate_delete_command = FALSE;
	} else {
		label = _("Mo_ve to Trash");
		tip = _("Move each selected item to the Trash");
		show_separate_delete_command = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_ENABLE_DELETE);
	}
	
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_TRASH);
	g_object_set (action,
		      "label", label,
		      "tooltip", tip,
		      "icon-name", all_selected_items_in_trash (view) ?
		      NEMO_ICON_DELETE : NEMO_ICON_TRASH_FULL,
		      NULL);
	/* if the backend supports delete but not trash then don't show trash */
	if (!can_trash_files && can_delete_files)
		gtk_action_set_visible (action, FALSE);
	else
		gtk_action_set_sensitive (action, can_trash_files);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_DELETE);
	/* if the backend doesn't support trash but supports delete
	   show the delete option. or if the user set this  pref */
	gtk_action_set_visible (action, (!can_trash_files && can_delete_files) || show_separate_delete_command);

	if ((!can_trash_files && can_delete_files) || show_separate_delete_command) {
		g_object_set (action,
			      "label", _("_Delete"),
			      "icon-name", NEMO_ICON_DELETE,
			      NULL);
	}
	gtk_action_set_sensitive (action, can_delete_files);


	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_RESTORE_FROM_TRASH);
	update_restore_from_trash_action (action, selection, FALSE);
	
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_DUPLICATE);
	gtk_action_set_sensitive (action, can_duplicate_files);
	gtk_action_set_visible (action, !selection_contains_recent);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_CREATE_LINK);
	gtk_action_set_sensitive (action, can_link_files);
	gtk_action_set_visible (action, !selection_contains_recent);
	g_object_set (action, "label",
		      ngettext ("Ma_ke Link",
			      	"Ma_ke Links",
				selection_count),
		      NULL);
	
	show_properties = (!NEMO_IS_DESKTOP_ICON_VIEW (view) || selection_count > 0);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_PROPERTIES);

	gtk_action_set_sensitive (action, show_properties);

	if (selection_count == 0) {
		gtk_action_set_tooltip (action, _("View or modify the properties of the open folder"));
	} else {
		gtk_action_set_tooltip (action, _("View or modify the properties of each selected item"));
	}

	gtk_action_set_visible (action, show_properties);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_PROPERTIES_ACCEL);

	gtk_action_set_sensitive (action, show_properties);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_EMPTY_TRASH);
	g_object_set (action,
		      "label", _("E_mpty Trash"),
		      NULL);
	gtk_action_set_sensitive (action, !nemo_trash_monitor_is_empty ());
	gtk_action_set_visible (action, should_show_empty_trash (view));

	show_save_search = FALSE;
	save_search_sensitive = FALSE;
	show_save_search_as = FALSE;
	if (view->details->model &&
	    NEMO_IS_SEARCH_DIRECTORY (view->details->model)) {
		NemoSearchDirectory *search;

		search = NEMO_SEARCH_DIRECTORY (view->details->model);
		if (nemo_search_directory_is_saved_search (search)) {
			show_save_search = TRUE;
			save_search_sensitive = nemo_search_directory_is_modified (search);
		} else {
			show_save_search_as = TRUE;
		}
	} 
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_SAVE_SEARCH);
	gtk_action_set_visible (action, show_save_search);
	gtk_action_set_sensitive (action, save_search_sensitive);
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_SAVE_SEARCH_AS);
	gtk_action_set_visible (action, show_save_search_as);


	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_SELECT_ALL);
	gtk_action_set_sensitive (action, !nemo_view_is_empty (view));

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_SELECT_PATTERN);
	gtk_action_set_sensitive (action, !nemo_view_is_empty (view));

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_INVERT_SELECTION);
	gtk_action_set_sensitive (action, !nemo_view_is_empty (view));

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_CUT);
	gtk_action_set_sensitive (action, can_delete_files);
	gtk_action_set_visible (action, !selection_contains_recent);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_COPY);
	gtk_action_set_sensitive (action, can_copy_files);

	real_update_paste_menu (view, selection, selection_count);

	real_update_menus_volumes (view, selection, selection_count);

	update_undo_actions (view);

	if (view->details->scripts_invalid) {
		update_scripts_menu (view);
	}

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_NEW_DOCUMENTS);
	gtk_action_set_sensitive (action, can_create_files);
	gtk_action_set_visible (action, !selection_contains_recent);

	if (can_create_files && view->details->templates_invalid) {
		update_templates_menu (view);
	}

    if (view->details->actions_invalid) {
        update_actions_menu (view);
    }

    update_actions_visibility (view);

	next_pane_is_writable = has_writable_extra_pane (view);

	/* next pane: works if file is copyable, and next pane is writable */
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_COPY_TO_NEXT_PANE);
	gtk_action_set_visible (action, can_copy_files && next_pane_is_writable);

	/* move to next pane: works if file is cuttable, and next pane is writable */
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_MOVE_TO_NEXT_PANE);
	gtk_action_set_visible (action, can_delete_files && next_pane_is_writable);


	show_desktop_target =
		g_settings_get_boolean (nemo_desktop_preferences, NEMO_PREFERENCES_SHOW_DESKTOP);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_COPY_TO_HOME);
	gtk_action_set_sensitive (action, can_copy_files);
	gtk_action_set_visible (action, !selection_contains_recent);
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_COPY_TO_DESKTOP);
	gtk_action_set_sensitive (action, can_copy_files);
	gtk_action_set_visible (action, show_desktop_target);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_MOVE_TO_HOME);
	gtk_action_set_sensitive (action, can_delete_files);
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      NEMO_ACTION_MOVE_TO_DESKTOP);
	gtk_action_set_sensitive (action, can_delete_files);
	gtk_action_set_visible (action, show_desktop_target);

	action = gtk_action_group_get_action (view->details->dir_action_group,
					      "CopyToMenu");
	gtk_action_set_sensitive (action, can_copy_files);
	action = gtk_action_group_get_action (view->details->dir_action_group,
					      "MoveToMenu");
	gtk_action_set_sensitive (action, can_delete_files);

    action = gtk_action_group_get_action (view->details->dir_action_group,
                                          NEMO_ACTION_FOLLOW_SYMLINK);
    gtk_action_set_visible (action,
                            selection_count == 1 &&
                            nemo_file_is_symbolic_link (selection->data));

    nemo_file_list_free (selection);
}

/**
 * nemo_view_pop_up_selection_context_menu
 *
 * Pop up a context menu appropriate to the selected items.
 * @view: NemoView of interest.
 * @event: The event that triggered this context menu.
 * 
 * Return value: NemoDirectory for this view.
 * 
 **/
void 
nemo_view_pop_up_selection_context_menu  (NemoView *view, 
					      GdkEventButton  *event)
{
	g_assert (NEMO_IS_VIEW (view));

	/* Make the context menu items not flash as they update to proper disabled,
	 * etc. states by forcing menus to update now.
	 */
	update_menus_if_pending (view);

	update_context_menu_position_from_event (view, event);

	eel_pop_up_context_menu (create_popup_menu 
				 (view, NEMO_VIEW_POPUP_PATH_SELECTION),
				 event);
}

/**
 * nemo_view_pop_up_background_context_menu
 *
 * Pop up a context menu appropriate to the view globally at the last right click location.
 * @view: NemoView of interest.
 * 
 * Return value: NemoDirectory for this view.
 * 
 **/
void 
nemo_view_pop_up_background_context_menu (NemoView *view, 
					      GdkEventButton  *event)
{
	g_assert (NEMO_IS_VIEW (view));

	/* Make the context menu items not flash as they update to proper disabled,
	 * etc. states by forcing menus to update now.
	 */
	update_menus_if_pending (view);

	update_context_menu_position_from_event (view, event);


	eel_pop_up_context_menu (create_popup_menu 
				 (view, NEMO_VIEW_POPUP_PATH_BACKGROUND),
				 event);
}

static void
real_pop_up_location_context_menu (NemoView *view)
{
	/* always update the menu before showing it. Shouldn't be too expensive. */
	real_update_location_menu (view);

	update_context_menu_position_from_event (view, view->details->location_popup_event);

	eel_pop_up_context_menu (create_popup_menu 
				 (view, NEMO_VIEW_POPUP_PATH_LOCATION),
				 view->details->location_popup_event);
}

static void
location_popup_file_attributes_ready (NemoFile *file,
				      gpointer      data)
{
	NemoView *view;

	view = NEMO_VIEW (data);
	g_assert (NEMO_IS_VIEW (view));

	g_assert (file == view->details->location_popup_directory_as_file);

	real_pop_up_location_context_menu (view);
}

static void
unschedule_pop_up_location_context_menu (NemoView *view)
{
	if (view->details->location_popup_directory_as_file != NULL) {
		g_assert (NEMO_IS_FILE (view->details->location_popup_directory_as_file));
		nemo_file_cancel_call_when_ready (view->details->location_popup_directory_as_file,
						      location_popup_file_attributes_ready,
						      view);
		nemo_file_unref (view->details->location_popup_directory_as_file);
		view->details->location_popup_directory_as_file = NULL;
	}
}

static void
schedule_pop_up_location_context_menu (NemoView *view,
				       GdkEventButton  *event,
				       NemoFile    *file)
{
	g_assert (NEMO_IS_FILE (file));

	if (view->details->location_popup_event != NULL) {
		gdk_event_free ((GdkEvent *) view->details->location_popup_event);
	}
	view->details->location_popup_event = (GdkEventButton *) gdk_event_copy ((GdkEvent *)event);

	if (file == view->details->location_popup_directory_as_file) {
		if (nemo_file_check_if_ready (file, NEMO_FILE_ATTRIBUTE_INFO |
						  NEMO_FILE_ATTRIBUTE_MOUNT |
						  NEMO_FILE_ATTRIBUTE_FILESYSTEM_INFO)) {
			real_pop_up_location_context_menu (view);
		}
	} else {
		unschedule_pop_up_location_context_menu (view);

		view->details->location_popup_directory_as_file = nemo_file_ref (file);
		nemo_file_call_when_ready (view->details->location_popup_directory_as_file,
					       NEMO_FILE_ATTRIBUTE_INFO |
					       NEMO_FILE_ATTRIBUTE_MOUNT |
					       NEMO_FILE_ATTRIBUTE_FILESYSTEM_INFO,
					       location_popup_file_attributes_ready,
					       view);
	}
}

/**
 * nemo_view_pop_up_location_context_menu
 *
 * Pop up a context menu appropriate to the view globally.
 * @view: NemoView of interest.
 * @event: GdkEventButton triggering the popup.
 * @location: The location the popup-menu should be created for,
 * or NULL for the currently displayed location.
 *
 **/
void 
nemo_view_pop_up_location_context_menu (NemoView *view, 
					    GdkEventButton  *event,
					    const char      *location)
{
	NemoFile *file;

	g_assert (NEMO_IS_VIEW (view));

	if (location != NULL) {
		file = nemo_file_get_by_uri (location);
	} else {
		file = nemo_file_ref (view->details->directory_as_file);
	}

	if (file != NULL) {
		schedule_pop_up_location_context_menu (view, event, file);
		nemo_file_unref (file);
	}
}

static void
schedule_update_menus (NemoView *view) 
{
	g_assert (NEMO_IS_VIEW (view));
	/* Don't schedule updates after destroy (#349551),
 	 * or if we are not active.
	 */
	if (view->details->window == NULL ||
	    !view->details->active) {
		return;
	}
	
	view->details->menu_states_untrustworthy = TRUE;
	/* Schedule a menu update with the current update interval */
    if (view->details->update_menus_timeout_id != 0) {
        g_source_remove (view->details->update_menus_timeout_id);
        view->details->update_menus_timeout_id = 0;
    }
    view->details->update_menus_timeout_id = g_timeout_add (view->details->update_interval,
                                                            update_menus_timeout_callback,
                                                            view);
}

static void
remove_update_status_idle_callback (NemoView *view) 
{
	if (view->details->update_status_idle_id != 0) {
		g_source_remove (view->details->update_status_idle_id);
		view->details->update_status_idle_id = 0;
	}
}

static gboolean
update_status_idle_callback (gpointer data)
{
	NemoView *view;

	view = NEMO_VIEW (data);
	nemo_view_display_selection_info (view);
	view->details->update_status_idle_id = 0;
	return FALSE;
}

static void
schedule_update_status (NemoView *view) 
{
	g_assert (NEMO_IS_VIEW (view));

	/* Make sure we haven't already destroyed it */
	if (view->details->window == NULL) {
		return;
	}

	if (view->details->loading) {
		/* Don't update status bar while loading the dir */
		return;
	}

	if (view->details->update_status_idle_id == 0) {
		view->details->update_status_idle_id =
			g_idle_add_full (G_PRIORITY_DEFAULT_IDLE - 20,
					 update_status_idle_callback, view, NULL);
	}
}

/**
 * nemo_view_notify_selection_changed:
 * 
 * Notify this view that the selection has changed. This is normally
 * called only by subclasses.
 * @view: NemoView whose selection has changed.
 * 
 **/
void
nemo_view_notify_selection_changed (NemoView *view)
{
	GtkWindow *window;
	GList *selection;
	
	g_return_if_fail (NEMO_IS_VIEW (view));

	selection = nemo_view_get_selection (view);
	window = nemo_view_get_containing_window (view);
	DEBUG_FILES (selection, "Selection changed in window %p", window);
	nemo_file_list_free (selection);

	view->details->selection_was_removed = FALSE;

	if (!view->details->selection_change_is_due_to_shell) {
		view->details->send_selection_change_to_shell = TRUE;
	}

	/* Schedule a display of the new selection. */
    if (view->details->display_selection_idle_id != 0) {
        g_source_remove (view->details->display_selection_idle_id);
        view->details->display_selection_idle_id = 0;
        nemo_window_slot_set_status (view->details->slot, "", "");
    }
    view->details->display_selection_idle_id = g_timeout_add (100,
                                                              display_selection_info_idle_callback,
                                                              view);

	if (view->details->batching_selection_level != 0) {
		view->details->selection_changed_while_batched = TRUE;
	} else {
		/* Here is the work we do only when we're not
		 * batching selection changes. In other words, it's the slower
		 * stuff that we don't want to slow down selection techniques
		 * such as rubberband-selecting in icon view.
		 */

		/* Schedule an update of menu item states to match selection */
		schedule_update_menus (view);
	}
}

static void
file_changed_callback (NemoFile *file, gpointer callback_data)
{
	NemoView *view = NEMO_VIEW (callback_data);

	schedule_changes (view);

	schedule_update_menus (view);
	schedule_update_status (view);
}

/**
 * load_directory:
 * 
 * Switch the displayed location to a new uri. If the uri is not valid,
 * the location will not be switched; user feedback will be provided instead.
 * @view: NemoView whose location will be changed.
 * @uri: A string representing the uri to switch to.
 * 
 **/
static void
load_directory (NemoView *view,
		NemoDirectory *directory)
{
	NemoDirectory *old_directory;
	NemoFile *old_file;
	NemoFileAttributes attributes;

	g_assert (NEMO_IS_VIEW (view));
	g_assert (NEMO_IS_DIRECTORY (directory));

	nemo_view_stop_loading (view);
	g_signal_emit (view, signals[CLEAR], 0);

	view->details->loading = TRUE;

	/* Update menus when directory is empty, before going to new
	 * location, so they won't have any false lingering knowledge
	 * of old selection.
	 */
	schedule_update_menus (view);
	
	while (view->details->subdirectory_list != NULL) {
		nemo_view_remove_subdirectory (view,
						   view->details->subdirectory_list->data);
	}

	disconnect_model_handlers (view);

	old_directory = view->details->model;
	nemo_directory_ref (directory);
	view->details->model = directory;
	nemo_directory_unref (old_directory);

	old_file = view->details->directory_as_file;
	view->details->directory_as_file =
		nemo_directory_get_corresponding_file (directory);
	nemo_file_unref (old_file);

	view->details->reported_load_error = FALSE;

	/* FIXME bugzilla.gnome.org 45062: In theory, we also need to monitor metadata here (as
         * well as doing a call when ready), in case external forces
         * change the directory's file metadata.
	 */
	attributes = 
		NEMO_FILE_ATTRIBUTE_INFO |
		NEMO_FILE_ATTRIBUTE_MOUNT |
		NEMO_FILE_ATTRIBUTE_FILESYSTEM_INFO;
	view->details->metadata_for_directory_as_file_pending = TRUE;
	view->details->metadata_for_files_in_directory_pending = TRUE;
	nemo_file_call_when_ready
		(view->details->directory_as_file,
		 attributes,
		 metadata_for_directory_as_file_ready_callback, view);
	nemo_directory_call_when_ready
		(view->details->model,
		 attributes,
		 FALSE,
		 metadata_for_files_in_directory_ready_callback, view);

	/* If capabilities change, then we need to update the menus
	 * because of New Folder, and relative emblems.
	 */
	attributes = 
		NEMO_FILE_ATTRIBUTE_INFO |
		NEMO_FILE_ATTRIBUTE_FILESYSTEM_INFO;
	nemo_file_monitor_add (view->details->directory_as_file,
				   &view->details->directory_as_file,
				   attributes);

	view->details->file_changed_handler_id = g_signal_connect
		(view->details->directory_as_file, "changed",
		 G_CALLBACK (file_changed_callback), view);
}

static void
finish_loading (NemoView *view)
{
	NemoFileAttributes attributes;

	nemo_window_report_load_underway (view->details->window,
					      NEMO_VIEW (view));

	/* Tell interested parties that we've begun loading this directory now.
	 * Subclasses use this to know that the new metadata is now available.
	 */
	g_signal_emit (view, signals[BEGIN_LOADING], 0);

	/* Assume we have now all information to show window */
	nemo_window_view_visible  (view->details->window, NEMO_VIEW (view));

	if (nemo_directory_are_all_files_seen (view->details->model)) {
		/* Unschedule a pending update and schedule a new one with the minimal
		 * update interval. This gives the view a short chance at gathering the
		 * (cached) deep counts.
		 */
		unschedule_display_of_pending_files (view);
		schedule_timeout_display_of_pending_files (view, UPDATE_INTERVAL_MIN);
	}
	
	/* Start loading. */

	/* Connect handlers to learn about loading progress. */
	view->details->done_loading_handler_id = g_signal_connect
		(view->details->model, "done_loading",
		 G_CALLBACK (done_loading_callback), view);
	view->details->load_error_handler_id = g_signal_connect
		(view->details->model, "load_error",
		 G_CALLBACK (load_error_callback), view);

	/* Monitor the things needed to get the right icon. Also
	 * monitor a directory's item count because the "size"
	 * attribute is based on that, and the file's metadata
	 * and possible custom name.
	 */
	attributes =
		NEMO_FILE_ATTRIBUTES_FOR_ICON |
		NEMO_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
		NEMO_FILE_ATTRIBUTE_INFO |
		NEMO_FILE_ATTRIBUTE_LINK_INFO |
		NEMO_FILE_ATTRIBUTE_MOUNT |
		NEMO_FILE_ATTRIBUTE_EXTENSION_INFO;

	nemo_directory_file_monitor_add (view->details->model,
					     &view->details->model,
					     view->details->show_hidden_files,
					     attributes,
					     files_added_callback, view);

    	view->details->files_added_handler_id = g_signal_connect
		(view->details->model, "files_added",
		 G_CALLBACK (files_added_callback), view);
	view->details->files_changed_handler_id = g_signal_connect
		(view->details->model, "files_changed",
		 G_CALLBACK (files_changed_callback), view);
}

static void
finish_loading_if_all_metadata_loaded (NemoView *view)
{
	if (!view->details->metadata_for_directory_as_file_pending &&
	    !view->details->metadata_for_files_in_directory_pending) {
		finish_loading (view);
	}
}

static void
metadata_for_directory_as_file_ready_callback (NemoFile *file,
			      		       gpointer callback_data)
{
	NemoView *view;

	view = callback_data;

	g_assert (NEMO_IS_VIEW (view));
	g_assert (view->details->directory_as_file == file);
	g_assert (view->details->metadata_for_directory_as_file_pending);

	view->details->metadata_for_directory_as_file_pending = FALSE;
	
	finish_loading_if_all_metadata_loaded (view);
}

static void
metadata_for_files_in_directory_ready_callback (NemoDirectory *directory,
				   		GList *files,
			           		gpointer callback_data)
{
	NemoView *view;

	view = callback_data;

	g_assert (NEMO_IS_VIEW (view));
	g_assert (view->details->model == directory);
	g_assert (view->details->metadata_for_files_in_directory_pending);

	view->details->metadata_for_files_in_directory_pending = FALSE;
	
	finish_loading_if_all_metadata_loaded (view);
}

static void
disconnect_handler (GObject *object, guint *id)
{
	if (*id != 0) {
		g_signal_handler_disconnect (object, *id);
		*id = 0;
	}
}

static void
disconnect_directory_handler (NemoView *view, guint *id)
{
	disconnect_handler (G_OBJECT (view->details->model), id);
}

static void
disconnect_directory_as_file_handler (NemoView *view, guint *id)
{
	disconnect_handler (G_OBJECT (view->details->directory_as_file), id);
}

static void
disconnect_model_handlers (NemoView *view)
{
	if (view->details->model == NULL) {
		return;
	}
	disconnect_directory_handler (view, &view->details->files_added_handler_id);
	disconnect_directory_handler (view, &view->details->files_changed_handler_id);
	disconnect_directory_handler (view, &view->details->done_loading_handler_id);
	disconnect_directory_handler (view, &view->details->load_error_handler_id);
	disconnect_directory_as_file_handler (view, &view->details->file_changed_handler_id);
	nemo_file_cancel_call_when_ready (view->details->directory_as_file,
					      metadata_for_directory_as_file_ready_callback,
					      view);
	nemo_directory_cancel_callback (view->details->model,
					    metadata_for_files_in_directory_ready_callback,
					    view);
	nemo_directory_file_monitor_remove (view->details->model,
						&view->details->model);
	nemo_file_monitor_remove (view->details->directory_as_file,
				      &view->details->directory_as_file);
}

static void
nemo_view_select_file (NemoView *view, NemoFile *file)
{
	GList file_list;

	file_list.data = file;
	file_list.next = NULL;
	file_list.prev = NULL;
	nemo_view_call_set_selection (view, &file_list);
}

static gboolean
remove_all (gpointer key, gpointer value, gpointer callback_data)
{
	return TRUE;
}

/**
 * nemo_view_stop_loading:
 * 
 * Stop the current ongoing process, such as switching to a new uri.
 * @view: NemoView in question.
 * 
 **/
void
nemo_view_stop_loading (NemoView *view)
{
	g_return_if_fail (NEMO_IS_VIEW (view));

	unschedule_display_of_pending_files (view);
	reset_update_interval (view);

	/* Free extra undisplayed files */
	file_and_directory_list_free (view->details->new_added_files);
	view->details->new_added_files = NULL;

	file_and_directory_list_free (view->details->new_changed_files);
	view->details->new_changed_files = NULL;

	g_hash_table_foreach_remove (view->details->non_ready_files, remove_all, NULL);

	file_and_directory_list_free (view->details->old_added_files);
	view->details->old_added_files = NULL;

	file_and_directory_list_free (view->details->old_changed_files);
	view->details->old_changed_files = NULL;

	g_list_free_full (view->details->pending_selection, g_object_unref);
	view->details->pending_selection = NULL;

	if (view->details->model != NULL) {
		nemo_directory_file_monitor_remove (view->details->model, view);
	}
	done_loading (view, FALSE);
}

gboolean
nemo_view_is_editable (NemoView *view)
{
	NemoDirectory *directory;

	directory = nemo_view_get_model (view);

	if (directory != NULL) {
		return nemo_directory_is_editable (directory);
	}

	return TRUE;
}

static gboolean
real_is_read_only (NemoView *view)
{
	NemoFile *file;
	
	if (!nemo_view_is_editable (view)) {
		return TRUE;
	}
	
	file = nemo_view_get_directory_as_file (view);
	if (file != NULL) {
		return !nemo_file_can_write (file);
	}
	return FALSE;
}

/**
 * nemo_view_should_show_file
 * 
 * Returns whether or not this file should be displayed based on
 * current filtering options.
 */
gboolean
nemo_view_should_show_file (NemoView *view, NemoFile *file)
{
	return nemo_file_should_show (file,
					  view->details->show_hidden_files,
					  view->details->show_foreign_files);
}

static gboolean
real_using_manual_layout (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), FALSE);

	return FALSE;
}

void
nemo_view_ignore_hidden_file_preferences (NemoView *view)
{
	g_return_if_fail (view->details->model == NULL);

	if (view->details->ignore_hidden_file_preferences) {
		return;
	}

	view->details->show_hidden_files = FALSE;
	view->details->ignore_hidden_file_preferences = TRUE;
}

void
nemo_view_set_show_foreign (NemoView *view,
				gboolean show_foreign)
{
	view->details->show_foreign_files = show_foreign;
}

char *
nemo_view_get_uri (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_VIEW (view), NULL);
	if (view->details->model == NULL) {
		return NULL;
	}
	return nemo_directory_get_uri (view->details->model);
}

void
nemo_view_move_copy_items (NemoView *view,
			       const GList *item_uris,
			       GArray *relative_item_points,
			       const char *target_uri,
			       int copy_action,
			       int x, int y)
{
	NemoFile *target_file;
	
	g_assert (relative_item_points == NULL
		  || relative_item_points->len == 0 
		  || g_list_length ((GList *)item_uris) == relative_item_points->len);

	/* add the drop location to the icon offsets */
	offset_drop_points (relative_item_points, x, y);

	target_file = nemo_file_get_existing_by_uri (target_uri);
	/* special-case "command:" here instead of starting a move/copy */
	if (target_file != NULL && nemo_file_is_launcher (target_file)) {
		nemo_file_unref (target_file);
		nemo_launch_desktop_file (
					      gtk_widget_get_screen (GTK_WIDGET (view)),
					      target_uri, item_uris,
					      nemo_view_get_containing_window (view));
		return;
	} else if (copy_action == GDK_ACTION_COPY &&
		   nemo_is_file_roller_installed () &&
		   target_file != NULL &&
		   nemo_file_is_archive (target_file)) {
		char *command, *quoted_uri, *tmp;
		const GList *l;
		GdkScreen  *screen;

		/* Handle dropping onto a file-roller archiver file, instead of starting a move/copy */

		nemo_file_unref (target_file);

		quoted_uri = g_shell_quote (target_uri);
		command = g_strconcat ("file-roller -a ", quoted_uri, NULL);
		g_free (quoted_uri);

		for (l = item_uris; l != NULL; l = l->next) {
			quoted_uri = g_shell_quote ((char *) l->data);

			tmp = g_strconcat (command, " ", quoted_uri, NULL);
			g_free (command);
			command = tmp;

			g_free (quoted_uri);
		} 

		screen = gtk_widget_get_screen (GTK_WIDGET (view));
		if (screen == NULL) {
			screen = gdk_screen_get_default ();
		}

		nemo_launch_application_from_command (screen, command, FALSE, NULL);
		g_free (command);

		return;
	}
	nemo_file_unref (target_file);

	nemo_file_operations_copy_move
		(item_uris, relative_item_points, 
		 target_uri, copy_action, GTK_WIDGET (view),
		 copy_move_done_callback, pre_copy_move (view));
}

static void
nemo_view_trash_state_changed_callback (NemoTrashMonitor *trash_monitor,
					    gboolean state, gpointer callback_data)
{
	NemoView *view;

	view = (NemoView *) callback_data;
	g_assert (NEMO_IS_VIEW (view));
	
	schedule_update_menus (view);
}

void
nemo_view_start_batching_selection_changes (NemoView *view)
{
	g_return_if_fail (NEMO_IS_VIEW (view));

	++view->details->batching_selection_level;
	view->details->selection_changed_while_batched = FALSE;
}

void
nemo_view_stop_batching_selection_changes (NemoView *view)
{
	g_return_if_fail (NEMO_IS_VIEW (view));
	g_return_if_fail (view->details->batching_selection_level > 0);

	if (--view->details->batching_selection_level == 0) {
		if (view->details->selection_changed_while_batched) {
			nemo_view_notify_selection_changed (view);
		}
	}
}

gboolean
nemo_view_get_active (NemoView *view)
{
	g_assert (NEMO_IS_VIEW (view));
	return view->details->active;
}

static GArray *
real_get_selected_icon_locations (NemoView *view)
{
        /* By default, just return an empty list. */
        return g_array_new (FALSE, TRUE, sizeof (GdkPoint));
}

static void
window_slots_changed (NemoWindow *window,
		      NemoWindowSlot *slot,
		      NemoView *view)
{
	GList *panes;
    GList *p;
	guint slot_count = 0; 

    panes = nemo_window_get_panes (window);
    for (p = panes; p != NULL; p = p->next) {
        NemoWindowPane *pane = NEMO_WINDOW_PANE (p->data);        
        slot_count = MAX (g_list_length (pane->slots), slot_count);    
    }

	/* Only add a shadow to the scrolled window when we're in a tabless
	 * notebook, since when the notebook has tabs, it will draw its own
	 * border.
	 */
	if (slot_count > 1) {
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (view), GTK_SHADOW_NONE);
	} else {
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (view), GTK_SHADOW_IN);
	}
}

static void
nemo_view_set_property (GObject         *object,
			    guint            prop_id,
			    const GValue    *value,
			    GParamSpec      *pspec)
{
	NemoView *directory_view;
	NemoWindowSlot *slot;
	NemoWindow *window;
  
	directory_view = NEMO_VIEW (object);

	switch (prop_id)  {
	case PROP_WINDOW_SLOT:
		g_assert (directory_view->details->slot == NULL);

		slot = NEMO_WINDOW_SLOT (g_value_get_object (value));
		window = nemo_window_slot_get_window (slot);

		directory_view->details->slot = slot;
		directory_view->details->window = window;

		g_signal_connect_object (directory_view->details->slot,
					 "active", G_CALLBACK (slot_active),
					 directory_view, 0);
		g_signal_connect_object (directory_view->details->slot,
					 "inactive", G_CALLBACK (slot_inactive),
					 directory_view, 0);
		g_signal_connect_object (directory_view->details->slot,
					 "changed-pane", G_CALLBACK (slot_changed_pane),
					 directory_view, 0);

		g_signal_connect_object (directory_view->details->window,
					 "slot-added", G_CALLBACK (window_slots_changed),
					 directory_view, 0);
		g_signal_connect_object (directory_view->details->window,
					 "slot-removed", G_CALLBACK (window_slots_changed),
					 directory_view, 0);
		window_slots_changed (window, slot, directory_view);

		g_signal_connect_object (directory_view->details->window,
					 "hidden-files-mode-changed", G_CALLBACK (hidden_files_mode_changed),
					 directory_view, 0);
		nemo_view_init_show_hidden_files (directory_view);
		break;
	case PROP_SUPPORTS_ZOOMING:
		directory_view->details->supports_zooming = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


gboolean
nemo_view_handle_scroll_event (NemoView *directory_view,
				   GdkEventScroll *event)
{
	static gdouble total_delta_y = 0;
	gdouble delta_x, delta_y;

	if (event->state & GDK_CONTROL_MASK) {
		switch (event->direction) {
		case GDK_SCROLL_UP:
			/* Zoom In */
			nemo_view_bump_zoom_level (directory_view, 1);
			return TRUE;

		case GDK_SCROLL_DOWN:
			/* Zoom Out */
			nemo_view_bump_zoom_level (directory_view, -1);
			return TRUE;

		case GDK_SCROLL_SMOOTH:
			gdk_event_get_scroll_deltas ((const GdkEvent *) event,
						     &delta_x, &delta_y);

			/* try to emulate a normal scrolling event by summing deltas */
			total_delta_y += delta_y;

			if (total_delta_y >= 1) {
				total_delta_y = 0;
				/* emulate scroll down */
				nemo_view_bump_zoom_level (directory_view, -1);
				return TRUE;
			} else if (total_delta_y <= - 1) {
				total_delta_y = 0;
				/* emulate scroll up */
				nemo_view_bump_zoom_level (directory_view, 1);
				return TRUE;				
			} else {
				/* eat event */
				return TRUE;
			}

		case GDK_SCROLL_LEFT:
		case GDK_SCROLL_RIGHT:
			break;

		default:
			g_assert_not_reached ();
		}
	}

	return FALSE;
}

/* handle Shift+Scroll, which will cause a zoom-in/out */
static gboolean
nemo_view_scroll_event (GtkWidget *widget,
			    GdkEventScroll *event)
{
	NemoView *directory_view;

	directory_view = NEMO_VIEW (widget);
	if (nemo_view_handle_scroll_event (directory_view, event)) {
		return TRUE;
	}

	return GTK_WIDGET_CLASS (parent_class)->scroll_event (widget, event);
}


static void
nemo_view_parent_set (GtkWidget *widget,
			  GtkWidget *old_parent)
{
	NemoView *view;
	GtkWidget *parent;

	view = NEMO_VIEW (widget);

	parent = gtk_widget_get_parent (widget);
	g_assert (parent == NULL || old_parent == NULL);

	if (GTK_WIDGET_CLASS (parent_class)->parent_set != NULL) {
		GTK_WIDGET_CLASS (parent_class)->parent_set (widget, old_parent);
	}

	if (parent != NULL) {
		g_assert (old_parent == NULL);

		if (view->details->slot == 
		    nemo_window_get_active_slot (view->details->window)) {
			view->details->active = TRUE;

			nemo_view_merge_menus (view);
			schedule_update_menus (view);
		}
	} else {
		nemo_view_unmerge_menus (view);
		remove_update_menus_timeout_callback (view);
	}
}

static void
nemo_view_class_init (NemoViewClass *klass)
{
	GObjectClass *oclass;
	GtkWidgetClass *widget_class;
	GtkScrolledWindowClass *scrolled_window_class;
	GtkBindingSet *binding_set;

	widget_class = GTK_WIDGET_CLASS (klass);
	scrolled_window_class = GTK_SCROLLED_WINDOW_CLASS (klass);
	oclass = G_OBJECT_CLASS (klass);

	oclass->finalize = nemo_view_finalize;
	oclass->set_property = nemo_view_set_property;

	widget_class->destroy = nemo_view_destroy;
	widget_class->scroll_event = nemo_view_scroll_event;
	widget_class->parent_set = nemo_view_parent_set;

	g_type_class_add_private (klass, sizeof (NemoViewDetails));

	/* Get rid of the strange 3-pixel gap that GtkScrolledWindow
	 * uses by default. It does us no good.
	 */
	scrolled_window_class->scrollbar_spacing = 0;

	signals[ADD_FILE] =
		g_signal_new ("add_file",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NemoViewClass, add_file),
		              NULL, NULL,
		              g_cclosure_marshal_generic,
		              G_TYPE_NONE, 2, NEMO_TYPE_FILE, NEMO_TYPE_DIRECTORY);
	signals[BEGIN_FILE_CHANGES] =
		g_signal_new ("begin_file_changes",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NemoViewClass, begin_file_changes),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[BEGIN_LOADING] =
		g_signal_new ("begin_loading",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NemoViewClass, begin_loading),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[CLEAR] =
		g_signal_new ("clear",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NemoViewClass, clear),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[END_FILE_CHANGES] =
		g_signal_new ("end_file_changes",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NemoViewClass, end_file_changes),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[END_LOADING] =
		g_signal_new ("end_loading",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NemoViewClass, end_loading),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__BOOLEAN,
		              G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
	signals[FILE_CHANGED] =
		g_signal_new ("file_changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NemoViewClass, file_changed),
		              NULL, NULL,
		              g_cclosure_marshal_generic,
		              G_TYPE_NONE, 2, NEMO_TYPE_FILE, NEMO_TYPE_DIRECTORY);
	signals[LOAD_ERROR] =
		g_signal_new ("load_error",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NemoViewClass, load_error),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__POINTER,
		              G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[REMOVE_FILE] =
		g_signal_new ("remove_file",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NemoViewClass, remove_file),
		              NULL, NULL,
		              g_cclosure_marshal_generic,
		              G_TYPE_NONE, 2, NEMO_TYPE_FILE, NEMO_TYPE_DIRECTORY);
	signals[ZOOM_LEVEL_CHANGED] =
		g_signal_new ("zoom-level-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[SELECTION_CHANGED] =
		g_signal_new ("selection-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[TRASH] =
		g_signal_new ("trash",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (NemoViewClass, trash),
			      g_signal_accumulator_true_handled, NULL,
			      g_cclosure_marshal_generic,
			      G_TYPE_BOOLEAN, 0);
	signals[DELETE] =
		g_signal_new ("delete",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (NemoViewClass, delete),
			      g_signal_accumulator_true_handled, NULL,
			      g_cclosure_marshal_generic,
			      G_TYPE_BOOLEAN, 0);

	klass->get_selected_icon_locations = real_get_selected_icon_locations;
	klass->is_read_only = real_is_read_only;
	klass->load_error = real_load_error;
	klass->can_rename_file = can_rename_file;
	klass->start_renaming_file = start_renaming_file;
	klass->get_backing_uri = real_get_backing_uri;
	klass->using_manual_layout = real_using_manual_layout;
        klass->merge_menus = real_merge_menus;
        klass->unmerge_menus = real_unmerge_menus;
        klass->update_menus = real_update_menus;
	klass->trash = real_trash;
	klass->delete = real_delete;

	copied_files_atom = gdk_atom_intern ("x-special/gnome-copied-files", FALSE);

	properties[PROP_WINDOW_SLOT] =
		g_param_spec_object ("window-slot",
				     "Window Slot",
				     "The parent window slot reference",
				     NEMO_TYPE_WINDOW_SLOT,
				     G_PARAM_WRITABLE |
				     G_PARAM_CONSTRUCT_ONLY);
	properties[PROP_SUPPORTS_ZOOMING] =
		g_param_spec_boolean ("supports-zooming",
				      "Supports zooming",
				      "Whether the view supports zooming",
				      TRUE,
				      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
				      G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);

	binding_set = gtk_binding_set_by_class (klass);

    gboolean swap_keys = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SWAP_TRASH_DELETE);

    if (swap_keys) {
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_Delete, 0,
                          "delete", 0);
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Delete, 0,
                          "delete", 0);
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Delete, GDK_SHIFT_MASK,
                          "trash", 0);
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_Delete, GDK_SHIFT_MASK,
                          "trash", 0);
    } else {
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_Delete, 0,
                          "trash", 0);
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Delete, 0,
                          "trash", 0);
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Delete, GDK_SHIFT_MASK,
                          "delete", 0);
        gtk_binding_entry_add_signal (binding_set, GDK_KEY_Delete, GDK_SHIFT_MASK,
                          "delete", 0);
    }
}

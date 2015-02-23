/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-window-slot.c: Nemo window slot
 
   Copyright (C) 2008 Free Software Foundation, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
  
   Author: Christian Neumair <cneumair@gnome.org>
*/

#include "config.h"

#include "nemo-window-slot.h"

#include "nemo-actions.h"
#include "nemo-application.h"
#include "nemo-canvas-view.h"
#include "nemo-desktop-window.h"
#include "nemo-floating-bar.h"
#include "nemo-special-location-bar.h"
#include "nemo-thumbnail-problem-bar.h"
#include "nemo-toolbar.h"
#include "nemo-trash-bar.h"
#include "nemo-view-factory.h"
#include "nemo-window-private.h"
#include "nemo-x-content-bar.h"
#include "nemo-metadata.h"

#include <glib/gi18n.h>
#include <eel/eel-stock-dialogs.h>

#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-module.h>
#include <libnemo-private/nemo-monitor.h>
#include <libnemo-private/nemo-profile.h>
#include <libnemo-private/nemo-action-manager.h>
#include <libnemo-extension/nemo-location-widget-provider.h>


#include <eel/eel-string.h>

G_DEFINE_TYPE (NemoWindowSlot, nemo_window_slot, GTK_TYPE_BOX);

enum {
	ACTIVE,
	INACTIVE,
	CHANGED_PANE,
	LOCATION_CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_PANE = 1,
	NUM_PROPERTIES
};

struct NemoWindowSlotDetails {
	NemoWindowPane *pane;

	/* floating bar */
	guint set_status_timeout_id;
	guint loading_timeout_id;
	GtkWidget *floating_bar;
	GtkWidget *view_overlay;
    GtkWidget *cache_bar;

	/* slot contains
	 *  1) an vbox containing extra_location_widgets
	 *  2) the view
	 */
	GtkWidget *extra_location_widgets;

	/* Current location. */
	GFile *location;
	gchar *title;
	gchar *status_text;

	/* Viewed file */
	NemoView *content_view;
	NemoView *new_content_view;
	NemoFile *viewed_file;
	gboolean viewed_file_seen;
	gboolean viewed_file_in_trash;

	/* Information about bookmarks and history list */
	NemoBookmark *current_location_bookmark;
	NemoBookmark *last_location_bookmark;
	GList *back_list;
	GList *forward_list;

	/* Query editor */
	NemoQueryEditor *query_editor;
	GtkWidget *query_editor_revealer;
	gulong qe_changed_id;
	gulong qe_cancel_id;
	gulong qe_activated_id;
	gboolean search_visible;

        /* Load state */
	GCancellable *find_mount_cancellable;
	gboolean allow_stop;
	gboolean needs_reload;
	gboolean load_with_search;

	/* Ensures that we do not react on signals of a
	 * view that is re-used as new view when its loading
	 * is cancelled
	 */
	gboolean temporarily_ignore_view_signals;

	/* New location. */
	GFile *pending_location;
	NemoLocationChangeType location_change_type;
	guint location_change_distance;
	char *pending_scroll_to;
	GList *pending_selection;
	gboolean pending_use_default_location;
	NemoFile *determine_view_file;
	GCancellable *mount_cancellable;
	GError *mount_error;
	gboolean tried_mount;
	NemoWindowGoToCallback open_callback;
	gpointer open_callback_user_data;
};

static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static void nemo_window_slot_force_reload (NemoWindowSlot *slot);
static void location_has_really_changed (NemoWindowSlot *slot);
static void nemo_window_slot_connect_new_content_view (NemoWindowSlot *slot);
static void nemo_window_slot_emit_location_change (NemoWindowSlot *slot, GFile *from, GFile *to);

static void
nemo_window_slot_sync_search_widgets (NemoWindowSlot *slot)
{
	NemoDirectory *directory;
	gboolean toggle;

	if (slot != nemo_window_get_active_slot (slot->details->pane->window)) {
		return;
	}

	toggle = slot->details->search_visible;

	if (slot->details->content_view != NULL) {
		directory = nemo_view_get_model (slot->details->content_view);
		if (NEMO_IS_SEARCH_DIRECTORY (directory)) {
			toggle = TRUE;
		}
	}

	nemo_window_slot_set_search_visible (slot, toggle);
}

gboolean
nemo_window_slot_content_view_matches_iid (NemoWindowSlot *slot,
					       const char *iid)
{
	if (slot->details->content_view == NULL) {
		return FALSE;
	}
	return g_strcmp0 (nemo_view_get_view_id (slot->details->content_view), iid) == 0;
}

static void
sync_search_directory (NemoWindowSlot *slot)
{
	NemoDirectory *directory;
	NemoQuery *query;
	gchar *text;
	GFile *location;

	g_assert (NEMO_IS_FILE (slot->details->viewed_file));

	directory = nemo_directory_get_for_file (slot->details->viewed_file);
	g_assert (NEMO_IS_SEARCH_DIRECTORY (directory));

	query = nemo_query_editor_get_query (slot->details->query_editor);
	text = nemo_query_get_text (query);

	if (!strlen (text)) {
		/* Prevent the location change from hiding the query editor in this case */
		slot->details->load_with_search = TRUE;
		location = nemo_query_editor_get_location (slot->details->query_editor);
		nemo_window_slot_open_location (slot, location, 0);
		g_object_unref (location);
	} else {
		nemo_search_directory_set_query (NEMO_SEARCH_DIRECTORY (directory),
						     query);
		nemo_window_slot_force_reload (slot);
	}

	g_free (text);
	g_object_unref (query);
	nemo_directory_unref (directory);
}

static void
create_new_search (NemoWindowSlot *slot)
{
	char *uri;
	NemoDirectory *directory;
	GFile *location;
	NemoQuery *query;

	uri = nemo_search_directory_generate_new_uri ();
	location = g_file_new_for_uri (uri);

	directory = nemo_directory_get (location);
	g_assert (NEMO_IS_SEARCH_DIRECTORY (directory));

	query = nemo_query_editor_get_query (slot->details->query_editor);
	nemo_search_directory_set_query (NEMO_SEARCH_DIRECTORY (directory), query);

	nemo_window_slot_open_location (slot, location, 0);

	nemo_directory_unref (directory);
	g_object_unref (query);
	g_object_unref (location);
	g_free (uri);
}

static void
query_editor_cancel_callback (NemoQueryEditor *editor,
			      NemoWindowSlot *slot)
{
	nemo_window_slot_set_search_visible (slot, FALSE);
}

static void
query_editor_activated_callback (NemoQueryEditor *editor,
				 NemoWindowSlot *slot)
{
	if (slot->details->content_view != NULL) {
		nemo_view_activate_selection (slot->details->content_view);
	}
}

static void
query_editor_changed_callback (NemoQueryEditor *editor,
			       NemoQuery *query,
			       gboolean reload,
			       NemoWindowSlot *slot)
{
	NemoDirectory *directory;

	g_assert (NEMO_IS_FILE (slot->details->viewed_file));

	directory = nemo_directory_get_for_file (slot->details->viewed_file);
	if (!NEMO_IS_SEARCH_DIRECTORY (directory)) {
		/* this is the first change from the query editor. we
		   ask for a location change to the search directory,
		   indicate the directory needs to be sync'd with the
		   current query. */
		create_new_search (slot);
		/* Focus is now on the new slot, move it back to query_editor */
		gtk_widget_grab_focus (GTK_WIDGET (slot->details->query_editor));
	} else {
		sync_search_directory (slot);
	}

	nemo_directory_unref (directory);
}

static void
hide_query_editor (NemoWindowSlot *slot)
{
	gtk_revealer_set_reveal_child (GTK_REVEALER (slot->details->query_editor_revealer),
				       FALSE);

	if (slot->details->qe_changed_id > 0) {
		g_signal_handler_disconnect (slot->details->query_editor, slot->details->qe_changed_id);
		slot->details->qe_changed_id = 0;
	}
	if (slot->details->qe_cancel_id > 0) {
		g_signal_handler_disconnect (slot->details->query_editor, slot->details->qe_cancel_id);
		slot->details->qe_cancel_id = 0;
	}
	if (slot->details->qe_activated_id > 0) {
		g_signal_handler_disconnect (slot->details->query_editor, slot->details->qe_activated_id);
		slot->details->qe_activated_id = 0;
	}

	nemo_query_editor_set_query (slot->details->query_editor, NULL);
}

static void
show_query_editor (NemoWindowSlot *slot)
{
	NemoDirectory *directory;
	NemoSearchDirectory *search_directory;
	GFile *location;

	if (slot->details->location) {
		location = slot->details->location;
	} else {
		location = slot->details->pending_location;
	}

	directory = nemo_directory_get (location);

	if (NEMO_IS_SEARCH_DIRECTORY (directory)) {
		NemoQuery *query;
		search_directory = NEMO_SEARCH_DIRECTORY (directory);
		query = nemo_search_directory_get_query (search_directory);
		if (query != NULL) {
			nemo_query_editor_set_query (slot->details->query_editor,
							 query);
			g_object_unref (query);
		}
	} else {
		nemo_query_editor_set_location (slot->details->query_editor, location);
	}

	nemo_directory_unref (directory);

	gtk_revealer_set_reveal_child (GTK_REVEALER (slot->details->query_editor_revealer),
				       TRUE);
	gtk_widget_grab_focus (GTK_WIDGET (slot->details->query_editor));

	if (slot->details->qe_changed_id == 0) {
		slot->details->qe_changed_id =
			g_signal_connect (slot->details->query_editor, "changed",
					  G_CALLBACK (query_editor_changed_callback), slot);
	}
	if (slot->details->qe_cancel_id == 0) {
		slot->details->qe_cancel_id =
			g_signal_connect (slot->details->query_editor, "cancel",
					  G_CALLBACK (query_editor_cancel_callback), slot);
	}
	if (slot->details->qe_activated_id == 0) {
		slot->details->qe_activated_id =
			g_signal_connect (slot->details->query_editor, "activated",
					  G_CALLBACK (query_editor_activated_callback), slot);
	}
}

void
nemo_window_slot_set_search_visible (NemoWindowSlot *slot,
					 gboolean            visible)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	gboolean old_visible;
	GFile *return_location;
	gboolean active_slot;

	/* set search active state for the slot */
	old_visible = slot->details->search_visible;
	slot->details->search_visible = visible;

	return_location = NULL;
	active_slot = (slot == slot->details->pane->active_slot);

	if (visible) {
		show_query_editor (slot);
	} else {
		/* If search was active on this slot and became inactive, change
		 * the slot location to the real directory.
		 */
		if (old_visible && active_slot) {
			/* Use the query editor search root if possible */
			return_location = nemo_query_editor_get_location (slot->details->query_editor);

			/* Use the home directory as a fallback */
			if (return_location == NULL) {
				return_location = g_file_new_for_path (g_get_home_dir ());
			}
		}

		if (active_slot) {
			nemo_window_pane_grab_focus (slot->details->pane);
		}

		/* Now hide the editor and clear its state */
		hide_query_editor (slot);
	}

	if (!active_slot) {
		return;
	}

	/* also synchronize the window action state */
	action_group = slot->details->pane->action_group;
	action = gtk_action_group_get_action (action_group, NEMO_ACTION_SEARCH);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);


	/* If search was active on this slot and became inactive, change
	 * the slot location to the real directory.
	 */
	if (return_location != NULL) {
		nemo_window_slot_open_location (slot, return_location, 0);
		g_object_unref (return_location);
	}
}

gboolean
nemo_window_slot_handle_event (NemoWindowSlot *slot,
				   GdkEventKey        *event)
{
	NemoWindow *window;

	window = nemo_window_slot_get_window (slot);
	if (NEMO_IS_DESKTOP_WINDOW (window))
		return FALSE;
	return nemo_query_editor_handle_event (slot->details->query_editor, event);
}

static void
real_active (NemoWindowSlot *slot)
{
	NemoWindow *window;
	NemoWindowPane *pane;
	int page_num;

	window = nemo_window_slot_get_window (slot);
	pane = nemo_window_slot_get_pane (slot);
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (pane->notebook),
					  GTK_WIDGET (slot));
	g_assert (page_num >= 0);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (pane->notebook), page_num);

	/* sync window to new slot */
	nemo_window_push_status (window, slot->details->status_text);
	nemo_window_sync_allow_stop (window, slot);
	nemo_window_sync_title (window, slot);
	nemo_window_sync_zoom_widgets (window);
	nemo_window_pane_sync_location_widgets (pane);
	nemo_window_slot_sync_search_widgets (slot);

	if (slot->details->viewed_file != NULL) {
		nemo_window_load_view_as_menus (window);
		nemo_window_load_extension_menus (window);
	}
}

static void
real_inactive (NemoWindowSlot *slot)
{
	NemoWindow *window;

	window = nemo_window_slot_get_window (slot);
	g_assert (slot == nemo_window_get_active_slot (window));
}

static void
floating_bar_action_cb (NemoFloatingBar *floating_bar,
			gint action,
			NemoWindowSlot *slot)
{
	if (action == NEMO_FLOATING_BAR_ACTION_ID_STOP) {
		nemo_window_slot_stop_loading (slot);

		NemoDirectory * directory = nemo_view_get_model (nemo_window_slot_get_view(slot));
		if (NEMO_IS_SEARCH_DIRECTORY (directory)) {
			nemo_search_directory_stop_search (NEMO_SEARCH_DIRECTORY(directory));
		}
	}
}

static void
remove_all_extra_location_widgets (GtkWidget *widget,
				   gpointer data)
{
	NemoWindowSlot *slot = data;
	NemoDirectory *directory;

	directory = nemo_directory_get (slot->details->location);
	if (widget != GTK_WIDGET (slot->details->query_editor_revealer)) {
		gtk_container_remove (GTK_CONTAINER (slot->details->extra_location_widgets), widget);
	}

	nemo_directory_unref (directory);
}

void
nemo_window_slot_remove_extra_location_widgets (NemoWindowSlot *slot)
{
	gtk_container_foreach (GTK_CONTAINER (slot->details->extra_location_widgets),
			       remove_all_extra_location_widgets,
			       slot);
}

void
nemo_window_slot_add_extra_location_widget (NemoWindowSlot *slot,
						GtkWidget *widget)
{
	gtk_box_pack_start (GTK_BOX (slot->details->extra_location_widgets),
			    widget, FALSE, TRUE, 0);
	gtk_widget_show (slot->details->extra_location_widgets);
}

static void
nemo_window_slot_set_property (GObject *object,
				   guint property_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	NemoWindowSlot *slot = NEMO_WINDOW_SLOT (object);

	switch (property_id) {
	case PROP_PANE:
		nemo_window_slot_set_pane (slot, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nemo_window_slot_get_property (GObject *object,
				   guint property_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	NemoWindowSlot *slot = NEMO_WINDOW_SLOT (object);

	switch (property_id) {
	case PROP_PANE:
		g_value_set_object (value, slot->details->pane);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nemo_window_slot_constructed (GObject *object)
{
	NemoWindowSlot *slot = NEMO_WINDOW_SLOT (object);
	GtkWidget *extras_vbox;

	G_OBJECT_CLASS (nemo_window_slot_parent_class)->constructed (object);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (slot),
					GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (GTK_WIDGET (slot));

	extras_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	slot->details->extra_location_widgets = extras_vbox;
	gtk_box_pack_start (GTK_BOX (slot), extras_vbox, FALSE, FALSE, 0);
	gtk_widget_show (extras_vbox);

	slot->details->query_editor = NEMO_QUERY_EDITOR (nemo_query_editor_new ());
	slot->details->query_editor_revealer = gtk_revealer_new ();
	gtk_container_add (GTK_CONTAINER (slot->details->query_editor_revealer),
			   GTK_WIDGET (slot->details->query_editor));
	gtk_widget_show_all (slot->details->query_editor_revealer);
	nemo_window_slot_add_extra_location_widget (slot, slot->details->query_editor_revealer);

	slot->details->view_overlay = gtk_overlay_new ();
	gtk_widget_add_events (slot->details->view_overlay,
			       GDK_ENTER_NOTIFY_MASK |
			       GDK_LEAVE_NOTIFY_MASK);
	gtk_box_pack_start (GTK_BOX (slot), slot->details->view_overlay, TRUE, TRUE, 0);
	gtk_widget_show (slot->details->view_overlay);

	slot->details->floating_bar = nemo_floating_bar_new (NULL, NULL, FALSE);
	gtk_widget_set_halign (slot->details->floating_bar, GTK_ALIGN_END);
	gtk_widget_set_valign (slot->details->floating_bar, GTK_ALIGN_END);
	gtk_overlay_add_overlay (GTK_OVERLAY (slot->details->view_overlay),
				 slot->details->floating_bar);

	g_signal_connect (slot->details->floating_bar, "action",
			  G_CALLBACK (floating_bar_action_cb), slot);

	slot->details->title = g_strdup (_("Loading…"));
}

static void
nemo_window_slot_init (NemoWindowSlot *slot)
{
	slot->details = G_TYPE_INSTANCE_GET_PRIVATE
		(slot, NEMO_TYPE_WINDOW_SLOT, NemoWindowSlotDetails);
}

static void
remove_loading_floating_bar (NemoWindowSlot *slot)
{
	if (slot->details->loading_timeout_id != 0) {
		g_source_remove (slot->details->loading_timeout_id);
		slot->details->loading_timeout_id = 0;
	}

	gtk_widget_hide (slot->details->floating_bar);
	nemo_floating_bar_cleanup_actions (NEMO_FLOATING_BAR (slot->details->floating_bar));
}

#define DEBUG_FLAG NEMO_DEBUG_WINDOW
#include <libnemo-private/nemo-debug.h>

/* FIXME bugzilla.gnome.org 41243:
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "nemo-desktop-window.h"

/* This number controls a maximum character count for a URL that is
 * displayed as part of a dialog. It's fairly arbitrary -- big enough
 * to allow most "normal" URIs to display in full, but small enough to
 * prevent the dialog from getting insanely wide.
 */
#define MAX_URI_IN_DIALOG_LENGTH 60

static void begin_location_change                     (NemoWindowSlot         *slot,
						       GFile                      *location,
						       GFile                      *previous_location,
						       GList                      *new_selection,
						       NemoLocationChangeType  type,
						       guint                       distance,
						       const char                 *scroll_pos,
						       NemoWindowGoToCallback  callback,
						       gpointer                    user_data);
static void free_location_change                      (NemoWindowSlot         *slot);
static void end_location_change                       (NemoWindowSlot         *slot);
static void cancel_location_change                    (NemoWindowSlot         *slot);
static void got_file_info_for_view_selection_callback (NemoFile               *file,
						       gpointer                    callback_data);
static gboolean create_content_view                   (NemoWindowSlot         *slot,
						       const char                 *view_id,
						       GError                    **error);
static void display_view_selection_failure            (NemoWindow             *window,
						       NemoFile               *file,
						       GFile                      *location,
						       GError                     *error);
static void load_new_location                         (NemoWindowSlot         *slot,
						       GFile                      *location,
						       GList                      *selection,
						       gboolean                    tell_current_content_view,
						       gboolean                    tell_new_content_view);

static void
new_window_show_callback (GtkWidget *widget,
			  gpointer user_data){
	NemoWindow *window;

	window = NEMO_WINDOW (user_data);
	nemo_window_close (window);

	g_signal_handlers_disconnect_by_func (widget,
					      G_CALLBACK (new_window_show_callback),
					      user_data);
}

void
nemo_window_slot_open_location_full (NemoWindowSlot *slot,
					 GFile *location,
					 NemoWindowOpenFlags flags,
					 GList *new_selection,
					 NemoWindowGoToCallback callback,
					 gpointer user_data)
{
	NemoWindow *window;
        NemoWindow *target_window;
        NemoWindowPane *pane;
        NemoWindowSlot *target_slot;
	NemoWindowOpenFlags slot_flags;
	GFile *old_location;
	GList *old_selection;
	char *old_uri, *new_uri;
	int new_slot_position;
	GList *panes;
	gboolean use_same;
	gboolean is_desktop;

	window = nemo_window_slot_get_window (slot);

        target_window = NULL;
	target_slot = NULL;
	use_same = TRUE;

	/* this happens at startup */
	old_uri = nemo_window_slot_get_location_uri (slot);
	if (old_uri == NULL) {
		old_uri = g_strdup ("(none)");
		use_same = TRUE;
	}
	new_uri = g_file_get_uri (location);

	DEBUG ("Opening location, old: %s, new: %s", old_uri, new_uri);
	nemo_profile_start ("Opening location, old: %s, new: %s", old_uri, new_uri);

	g_free (old_uri);
	g_free (new_uri);

	is_desktop = NEMO_IS_DESKTOP_WINDOW (window);

	if (is_desktop) {
		use_same = !nemo_desktop_window_loaded (NEMO_DESKTOP_WINDOW (window));

		/* if we're requested to open a new tab on the desktop, open a window
		 * instead.
		 */
		if (flags & NEMO_WINDOW_OPEN_FLAG_NEW_TAB) {
			flags ^= NEMO_WINDOW_OPEN_FLAG_NEW_TAB;
			flags |= NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW;
		}
	}

	g_assert (!((flags & NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW) != 0 &&
		    (flags & NEMO_WINDOW_OPEN_FLAG_NEW_TAB) != 0));

	/* and if the flags specify so, this is overridden */
	if ((flags & NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW) != 0) {
		use_same = FALSE;
	}

	old_location = nemo_window_slot_get_location (slot);

	/* now get/create the window */
	if (use_same) {
		target_window = window;
	} else {
		target_window = nemo_application_create_window
			(NEMO_APPLICATION (g_application_get_default ()),
			 gtk_window_get_screen (GTK_WINDOW (window)));
	}

        g_assert (target_window != NULL);

	/* if the flags say we want a new tab, open a slot in the current window */
	if ((flags & NEMO_WINDOW_OPEN_FLAG_NEW_TAB) != 0) {
		g_assert (target_window == window);

		slot_flags = 0;

		new_slot_position = g_settings_get_enum (nemo_preferences, NEMO_PREFERENCES_NEW_TAB_POSITION);
		if (new_slot_position == NEMO_NEW_TAB_POSITION_END) {
			slot_flags = NEMO_WINDOW_OPEN_SLOT_APPEND;
		}

		target_slot = nemo_window_pane_open_slot (nemo_window_get_active_pane (window),
							      slot_flags);
		nemo_window_set_active_slot (window, target_slot);
	}

	/* close the current window if the flags say so */
	if ((flags & NEMO_WINDOW_OPEN_FLAG_CLOSE_BEHIND) != 0) {
		if (!is_desktop) {
			if (gtk_widget_get_visible (GTK_WIDGET (target_window))) {
				nemo_window_close (window);
			} else {
				g_signal_connect_object (target_window,
							 "show",
							 G_CALLBACK (new_window_show_callback),
							 window,
							 G_CONNECT_AFTER);
			}
		}
	}

	if (target_slot == NULL) {
		if (target_window == window) {
			target_slot = slot;
		} else {
			target_slot = nemo_window_get_active_slot (target_window);
		}
	}

	old_selection = NULL;
	if (slot->details->content_view != NULL) {
		old_selection = nemo_view_get_selection (slot->details->content_view);
	}

	if (target_window == window && target_slot == slot && !is_desktop &&
	    old_location && g_file_equal (old_location, location) &&
	    nemo_file_selection_equal (old_selection, new_selection)) {

		if (callback != NULL) {
			callback (window, location, NULL, user_data);
		}

		goto done;
	}

	slot->details->pending_use_default_location = ((flags & NEMO_WINDOW_OPEN_FLAG_USE_DEFAULT_LOCATION) != 0);
        begin_location_change (target_slot, location, old_location, new_selection,
			       NEMO_LOCATION_CHANGE_STANDARD, 0, NULL, callback, user_data);

	/* Additionally, load this in all slots that have no location, this means
	   we load both panes in e.g. a newly opened dual pane window. */
	for (panes = target_window->details->panes; panes != NULL; panes = panes->next) {
		pane = panes->data;
		slot = pane->active_slot;
		if (slot->details->location == NULL && slot->details->pending_location == NULL) {
			begin_location_change (slot, location, old_location, new_selection,
					       NEMO_LOCATION_CHANGE_STANDARD, 0, NULL, NULL, NULL);
		}
	}

 done:
	nemo_file_list_free (old_selection);
	nemo_profile_end (NULL);
}

static gboolean
report_callback (NemoWindowSlot *slot,
		 GError *error)
{
	if (slot->details->open_callback != NULL) {
		gboolean res;
		res = slot->details->open_callback (nemo_window_slot_get_window (slot),
						    slot->details->pending_location,
						    error, slot->details->open_callback_user_data);
		slot->details->open_callback = NULL;
		slot->details->open_callback_user_data = NULL;

		return res;
	}

	return FALSE;
}

/*
 * begin_location_change
 *
 * Change a window slot's location.
 * @window: The NemoWindow whose location should be changed.
 * @location: A url specifying the location to load
 * @previous_location: The url that was previously shown in the window that initialized the change, if any
 * @new_selection: The initial selection to present after loading the location
 * @type: Which type of location change is this? Standard, back, forward, or reload?
 * @distance: If type is back or forward, the index into the back or forward chain. If
 * type is standard or reload, this is ignored, and must be 0.
 * @scroll_pos: The file to scroll to when the location is loaded.
 * @callback: function to be called when the location is changed.
 * @user_data: data for @callback.
 *
 * This is the core function for changing the location of a window. Every change to the
 * location begins here.
 */
static void
begin_location_change (NemoWindowSlot *slot,
                       GFile *location,
                       GFile *previous_location,
		       GList *new_selection,
                       NemoLocationChangeType type,
                       guint distance,
                       const char *scroll_pos,
		       NemoWindowGoToCallback callback,
		       gpointer user_data)
{
        NemoDirectory *directory;
        NemoFile *file;
	gboolean force_reload;
        char *current_pos;
	GFile *from_folder, *parent;
	GList *parent_selection = NULL;

	g_assert (slot != NULL);
        g_assert (location != NULL);
        g_assert (type == NEMO_LOCATION_CHANGE_BACK
                  || type == NEMO_LOCATION_CHANGE_FORWARD
                  || distance == 0);

	nemo_profile_start (NULL);

	/* If there is no new selection and the new location is
	 * a (grand)parent of the old location then we automatically
	 * select the folder the previous location was in */
	if (new_selection == NULL && previous_location != NULL &&
	    g_file_has_prefix (previous_location, location)) {
		from_folder = g_object_ref (previous_location);
		parent = g_file_get_parent (from_folder);
		while (parent != NULL && !g_file_equal (parent, location)) {
			g_object_unref (from_folder);
			from_folder = parent;
			parent = g_file_get_parent (from_folder);
		}

		if (parent != NULL) {
			new_selection = parent_selection = 
				g_list_prepend (NULL, nemo_file_get (from_folder));
			g_object_unref (parent);
		}

		g_object_unref (from_folder);
	}

	end_location_change (slot);

	nemo_window_slot_set_allow_stop (slot, TRUE);
	nemo_window_slot_set_status (slot, NULL, NULL);

	g_assert (slot->details->pending_location == NULL);
	g_assert (slot->details->pending_selection == NULL);

	slot->details->pending_location = g_object_ref (location);
	slot->details->location_change_type = type;
	slot->details->location_change_distance = distance;
	slot->details->tried_mount = FALSE;
	slot->details->pending_selection = eel_g_object_list_copy (new_selection);

	slot->details->pending_scroll_to = g_strdup (scroll_pos);

	slot->details->open_callback = callback;
	slot->details->open_callback_user_data = user_data;

        directory = nemo_directory_get (location);

	/* The code to force a reload is here because if we do it
	 * after determining an initial view (in the components), then
	 * we end up fetching things twice.
	 */
	if (type == NEMO_LOCATION_CHANGE_RELOAD) {
		force_reload = TRUE;
	} else if (!nemo_monitor_active ()) {
		force_reload = TRUE;
	} else {
		force_reload = !nemo_directory_is_local (directory);
	}

	if (force_reload) {
		nemo_directory_force_reload (directory);
		file = nemo_directory_get_corresponding_file (directory);
		nemo_file_invalidate_all_attributes (file);
		nemo_file_unref (file);
	}

        nemo_directory_unref (directory);

	if (parent_selection != NULL) {
		g_list_free_full (parent_selection, g_object_unref);
	}

        /* Set current_bookmark scroll pos */
        if (slot->details->current_location_bookmark != NULL &&
            slot->details->content_view != NULL) {
                current_pos = nemo_view_get_first_visible_file (slot->details->content_view);
                nemo_bookmark_set_scroll_pos (slot->details->current_location_bookmark, current_pos);
                g_free (current_pos);
        }

	/* Get the info needed for view selection */
	slot->details->determine_view_file = nemo_file_get (location);
	g_assert (slot->details->determine_view_file != NULL);

	nemo_file_call_when_ready (slot->details->determine_view_file,
				       NEMO_FILE_ATTRIBUTE_INFO |
				       NEMO_FILE_ATTRIBUTE_MOUNT,
                                       got_file_info_for_view_selection_callback,
				       slot);

	nemo_profile_end (NULL);
}

static void
nemo_window_slot_set_location (NemoWindowSlot *slot,
				   GFile *location)
{
	if (slot->details->location == location) {
		return;
	}

	GFile *old_location = slot->details->location;
	slot->details->location = g_object_ref (location);

	if (slot->details->location &&
	    g_file_equal (location, slot->details->location)) {
              	if (old_location) {
	        	g_object_unref (old_location);
	        }		
                return;
	}

	if (slot == nemo_window_get_active_slot (slot->details->pane->window)) {
		nemo_window_pane_sync_location_widgets (slot->details->pane);
		nemo_window_slot_update_title (slot);
	}

	nemo_window_slot_emit_location_change (slot, old_location, location);

	if (old_location) {
		g_object_unref (old_location);
	}
}

static void
viewed_file_changed_callback (NemoFile *file,
                              NemoWindowSlot *slot)
{
        GFile *new_location;
	gboolean is_in_trash, was_in_trash;

        g_assert (NEMO_IS_FILE (file));
	g_assert (NEMO_WINDOW_SLOT (slot));
	g_assert (file == slot->details->viewed_file);

        if (!nemo_file_is_not_yet_confirmed (file)) {
                slot->details->viewed_file_seen = TRUE;
        }

	was_in_trash = slot->details->viewed_file_in_trash;

	slot->details->viewed_file_in_trash = is_in_trash = nemo_file_is_in_trash (file);

	if (nemo_file_is_gone (file) || (is_in_trash && !was_in_trash)) {
                if (slot->details->viewed_file_seen) {
			GFile *go_to_file;
			GFile *parent;
			GFile *location;
			GMount *mount;

			parent = NULL;
			location = nemo_file_get_location (file);

			if (g_file_is_native (location)) {
				mount = nemo_get_mounted_mount_for_root (location);

				if (mount == NULL) {
					parent = g_file_get_parent (location);
				}

				g_clear_object (&mount);
			}

			if (parent != NULL) {
				/* auto-show existing parent */
				go_to_file = nemo_find_existing_uri_in_hierarchy (parent);
			} else {
				go_to_file = g_file_new_for_path (g_get_home_dir ());
			}

			nemo_window_slot_open_location (slot, go_to_file, 0);

			g_clear_object (&parent);
			g_object_unref (go_to_file);
			g_object_unref (location);
                }
	} else {
                new_location = nemo_file_get_location (file);
		nemo_window_slot_set_location (slot, new_location);
		g_object_unref (new_location);
        }
}

static void
nemo_window_slot_set_viewed_file (NemoWindowSlot *slot,
				      NemoFile *file)
{
	NemoFileAttributes attributes;

	if (slot->details->viewed_file == file) {
		return;
	}

	nemo_file_ref (file);

	if (slot->details->viewed_file != NULL) {
		g_signal_handlers_disconnect_by_func (slot->details->viewed_file,
						      G_CALLBACK (viewed_file_changed_callback),
						      slot);
		nemo_file_monitor_remove (slot->details->viewed_file,
					      slot);
	}

	if (file != NULL) {
		attributes =
			NEMO_FILE_ATTRIBUTE_INFO |
			NEMO_FILE_ATTRIBUTE_LINK_INFO;
		nemo_file_monitor_add (file, slot, attributes);

		g_signal_connect_object (file, "changed",
					 G_CALLBACK (viewed_file_changed_callback), slot, 0);
	}

	nemo_file_unref (slot->details->viewed_file);
	slot->details->viewed_file = file;
}

typedef struct {
	GCancellable *cancellable;
	NemoWindowSlot *slot;
} MountNotMountedData;

static void
mount_not_mounted_callback (GObject *source_object,
			    GAsyncResult *res,
			    gpointer user_data)
{
	MountNotMountedData *data;
	NemoWindowSlot *slot;
	GError *error;
	GCancellable *cancellable;

	data = user_data;
	slot = data->slot;
	cancellable = data->cancellable;
	g_free (data);

	if (g_cancellable_is_cancelled (cancellable)) {
		/* Cancelled, don't call back */
		g_object_unref (cancellable);
		return;
	}

	slot->details->mount_cancellable = NULL;

	slot->details->determine_view_file = nemo_file_get (slot->details->pending_location);

	error = NULL;
	if (!g_file_mount_enclosing_volume_finish (G_FILE (source_object), res, &error)) {
		slot->details->mount_error = error;
		got_file_info_for_view_selection_callback (slot->details->determine_view_file, slot);
		slot->details->mount_error = NULL;
		g_error_free (error);
	} else {
		nemo_file_invalidate_all_attributes (slot->details->determine_view_file);
		nemo_file_call_when_ready (slot->details->determine_view_file,
					       NEMO_FILE_ATTRIBUTE_INFO |
					       NEMO_FILE_ATTRIBUTE_MOUNT,
					       got_file_info_for_view_selection_callback,
					       slot);
	}

	g_object_unref (cancellable);
}

static void
got_file_info_for_view_selection_callback (NemoFile *file,
					   gpointer callback_data)
{
        GError *error = NULL;
	char *view_id;
	char *mimetype;
	NemoWindow *window;
	NemoWindowPane *pane;
	NemoWindowSlot *slot;
	NemoFile *viewed_file, *parent_file;
	GFile *location, *default_location;
	GMountOperation *mount_op;
	MountNotMountedData *data;
	GtkApplication *app;
	GMount *mount;

	slot = callback_data;
	window = nemo_window_slot_get_window (slot);
	pane = nemo_window_slot_get_pane (slot);

	g_assert (slot->details->determine_view_file == file);
	slot->details->determine_view_file = NULL;

	nemo_profile_start (NULL);

	if (slot->details->mount_error) {
		error = g_error_copy (slot->details->mount_error);
	} else if (nemo_file_get_file_info_error (file) != NULL) {
		error = g_error_copy (nemo_file_get_file_info_error (file));
	}

	if (error && error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_MOUNTED &&
	    !slot->details->tried_mount) {
		slot->details->tried_mount = TRUE;

		mount_op = gtk_mount_operation_new (GTK_WINDOW (window));
		g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
		GFile *file_location = nemo_file_get_location (file);
		data = g_new0 (MountNotMountedData, 1);
		data->cancellable = g_cancellable_new ();
		data->slot = slot;
		slot->details->mount_cancellable = data->cancellable;
		g_file_mount_enclosing_volume (file_location, 0, mount_op, slot->details->mount_cancellable,
					       mount_not_mounted_callback, data);
		g_object_unref (file_location);
		g_object_unref (mount_op);

		goto done;
	}

	mount = NULL;
	default_location = NULL;

	if (slot->details->pending_use_default_location) {
		mount = nemo_file_get_mount (file);
		slot->details->pending_use_default_location = FALSE;
	}

	if (mount != NULL) {
		default_location = g_mount_get_default_location (mount);
		g_object_unref (mount);
	}

	if (default_location != NULL &&
	    !g_file_equal (slot->details->pending_location, default_location)) {
		g_clear_object (&slot->details->pending_location);
		slot->details->pending_location = default_location;
		slot->details->determine_view_file = nemo_file_get (default_location);

		nemo_file_invalidate_all_attributes (slot->details->determine_view_file);
		nemo_file_call_when_ready (slot->details->determine_view_file,
					       NEMO_FILE_ATTRIBUTE_INFO |
					       NEMO_FILE_ATTRIBUTE_MOUNT,
					       got_file_info_for_view_selection_callback,
					       slot);

		goto done;
	}

	parent_file = nemo_file_get_parent (file);
	if ((parent_file != NULL) &&
	    nemo_file_get_file_type (file) == G_FILE_TYPE_REGULAR) {
		if (slot->details->pending_selection != NULL) {
			g_list_free_full (slot->details->pending_selection, (GDestroyNotify) nemo_file_unref);
		}

		g_clear_object (&slot->details->pending_location);
		g_free (slot->details->pending_scroll_to);

		slot->details->pending_location = nemo_file_get_parent_location (file);
		slot->details->pending_selection = g_list_prepend (NULL, nemo_file_ref (file));
		slot->details->determine_view_file = parent_file;
		slot->details->pending_scroll_to = nemo_file_get_uri (file);

		nemo_file_invalidate_all_attributes (slot->details->determine_view_file);
		nemo_file_call_when_ready (slot->details->determine_view_file,
					       NEMO_FILE_ATTRIBUTE_INFO |
					       NEMO_FILE_ATTRIBUTE_MOUNT,
					       got_file_info_for_view_selection_callback,
					       slot);

		goto done;
	}

	nemo_file_unref (parent_file);
	location = slot->details->pending_location;

	view_id = NULL;

        if (error == NULL ||
	    (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_SUPPORTED)) {
		/* We got the information we need, now pick what view to use: */

		mimetype = nemo_file_get_mime_type (file);

		/* Look in metadata for view */
		view_id = nemo_global_preferences_get_ignore_view_metadata () ? g_strdup (nemo_window_get_ignore_meta_view_id (window)) :
                                                                        nemo_file_get_metadata (file, NEMO_METADATA_KEY_DEFAULT_VIEW, NULL);
		if (view_id != NULL && 
		    !nemo_view_factory_view_supports_uri (view_id,
							      location,
							      nemo_file_get_file_type (file),
							      mimetype)) {
			g_free (view_id);
			view_id = NULL;
		}

		/* Otherwise, use default */
		if (view_id == NULL) {
            gchar *name;
            name = nemo_file_get_name (file);

            if (g_strcmp0 (name, "x-nemo-search") == 0)
                view_id = g_strdup (NEMO_LIST_VIEW_IID);
            else
                view_id = nemo_global_preferences_get_default_folder_viewer_preference_as_iid ();

            g_free (name);

			if (view_id != NULL &&
			    !nemo_view_factory_view_supports_uri (view_id,
								      location,
								      nemo_file_get_file_type (file),
								      mimetype)) {
				g_free (view_id);
				view_id = NULL;
			}
		}

		g_free (mimetype);
	}

	if (view_id != NULL) {
		GError *err = NULL;

		create_content_view (slot, view_id, &err);
		g_free (view_id);

		report_callback (slot, err);
		g_clear_error (&err);
	} else {
		if (error == NULL) {
			error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
					     _("Unable to load location"));
		}
		if (!report_callback (slot, error)) {
			display_view_selection_failure (window, file,
							location, error);
		}

		if (!gtk_widget_get_visible (GTK_WIDGET (window))) {
			/* Destroy never-had-a-chance-to-be-seen window. This case
			 * happens when a new window cannot display its initial URI.
			 */
			/* if this is the only window, we don't want to quit, so we redirect it to home */

			app = GTK_APPLICATION (g_application_get_default ());

			if (g_list_length (gtk_application_get_windows (app)) == 1) {
				/* the user could have typed in a home directory that doesn't exist,
				   in which case going home would cause an infinite loop, so we
				   better test for that */

				if (!nemo_is_root_directory (location)) {
					if (!nemo_is_home_directory (location)) {
						nemo_window_slot_go_home (slot, FALSE);
					} else {
						GFile *root;

						root = g_file_new_for_path ("/");
						/* the last fallback is to go to a known place that can't be deleted! */
						nemo_window_slot_open_location (slot, location, 0);
						g_object_unref (root);
					}
				} else {
					gtk_widget_destroy (GTK_WIDGET (window));
				}
			} else {
				/* Since this is a window, destroying it will also unref it. */
				gtk_widget_destroy (GTK_WIDGET (window));
			}
		} else {
			GFile *slot_location;

			/* Clean up state of already-showing window */
			end_location_change (slot);
			slot_location = nemo_window_slot_get_location (slot);

			/* We're missing a previous location (if opened location
			 * in a new tab) so close it and return */
			if (slot_location == NULL) {
				nemo_window_pane_close_slot (pane, slot);
			} else {
				/* We disconnected this, so we need to re-connect it */
				viewed_file = nemo_file_get (slot_location);
				nemo_window_slot_set_viewed_file (slot, viewed_file);
				nemo_file_monitor_add (viewed_file, slot, 0);
				nemo_file_unref (viewed_file);

				/* Leave the location bar showing the bad location that the user
				 * typed (or maybe achieved by dragging or something). Many times
				 * the mistake will just be an easily-correctable typo. The user
				 * can choose "Refresh" to get the original URI back in the location bar.
				 */
			}
		}
	}

 done:
	g_clear_error (&error);

	nemo_file_unref (file);

	nemo_profile_end (NULL);
}

/* Load a view into the window, either reusing the old one or creating
 * a new one. This happens when you want to load a new location, or just
 * switch to a different view.
 * If pending_location is set we're loading a new location and
 * pending_location/selection will be used. If not, we're just switching
 * view, and the current location will be used.
 */
static gboolean
create_content_view (NemoWindowSlot *slot,
		     const char *view_id,
		     GError **error_out)
{
	NemoWindow *window;
        NemoView *view;
	GList *selection;
	gboolean ret = TRUE;
	GError *error = NULL;
	NemoDirectory *old_directory, *new_directory;
	GFile *old_location;

	window = nemo_window_slot_get_window (slot);

	nemo_profile_start (NULL);

	/* FIXME bugzilla.gnome.org 41243:
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
	if (NEMO_IS_DESKTOP_WINDOW (window)) {
		/* We force the desktop to use a desktop_icon_view. It's simpler
		 * to fix it here than trying to make it pick the right view in
		 * the first place.
		 */
		view_id = NEMO_DESKTOP_CANVAS_VIEW_IID;
	}

        if (slot->details->content_view != NULL &&
	    g_strcmp0 (nemo_view_get_view_id (slot->details->content_view),
			view_id) == 0) {
                /* reuse existing content view */
                view = slot->details->content_view;
                slot->details->new_content_view = g_object_ref (view);

        } else {
                /* create a new content view */
		view = nemo_view_factory_create (view_id, slot);

		slot->details->new_content_view = view;
                nemo_window_slot_connect_new_content_view (slot);
		nemo_window_connect_content_view (window, slot->details->new_content_view);
        }

	/* Forward search selection and state before loading the new model */
	old_location = nemo_window_slot_get_location (slot);
	old_directory = nemo_directory_get (old_location);
	new_directory = nemo_directory_get (slot->details->pending_location);

	if (NEMO_IS_SEARCH_DIRECTORY (new_directory) &&
	    !NEMO_IS_SEARCH_DIRECTORY (old_directory)) {
		nemo_search_directory_set_base_model (NEMO_SEARCH_DIRECTORY (new_directory), old_directory);
	}

	if (NEMO_IS_SEARCH_DIRECTORY (old_directory) &&
	    !NEMO_IS_SEARCH_DIRECTORY (new_directory)) {
		/* Reset the search_active state when going out of a search directory,
		 * before nautilus_window_slot_sync_search_widgets() is called
		 * if we're not being loaded with search visible.
		 */
		if (!slot->details->load_with_search) {
			slot->details->search_visible = FALSE;
		}

		slot->details->load_with_search = FALSE;

		if (slot->details->pending_selection == NULL) {
			slot->details->pending_selection = nemo_view_get_selection (slot->details->content_view);
		}
	}

	/* Actually load the pending location and selection: */

        if (slot->details->pending_location != NULL) {
		load_new_location (slot,
				   slot->details->pending_location,
				   slot->details->pending_selection,
				   FALSE,
				   TRUE);

		g_list_free_full (slot->details->pending_selection, g_object_unref);
		slot->details->pending_selection = NULL;
	} else if (old_location != NULL) {
		selection = nemo_view_get_selection (slot->details->content_view);
		load_new_location (slot,
				   old_location,
				   selection,
				   FALSE,
				   TRUE);
		g_list_free_full (selection, g_object_unref);
	} else {
		/* Something is busted, there was no location to load. */
		ret = FALSE;
		error = g_error_new (G_IO_ERROR,
				     G_IO_ERROR_NOT_FOUND,
				     _("Unable to load location"));
	}

	if (error != NULL) {
		g_propagate_error (error_out, error);
	}

	nemo_profile_end (NULL);

	return ret;
}

static void
load_new_location (NemoWindowSlot *slot,
		   GFile *location,
		   GList *selection,
		   gboolean tell_current_content_view,
		   gboolean tell_new_content_view)
{
	GList *selection_copy;
	NemoView *view;

	g_assert (slot != NULL);
	g_assert (location != NULL);

	selection_copy = eel_g_object_list_copy (selection);
	view = NULL;

	nemo_profile_start (NULL);
	/* Note, these may recurse into report_load_underway */
        if (slot->details->content_view != NULL && tell_current_content_view) {
		view = slot->details->content_view;
		nemo_view_load_location (slot->details->content_view, location);
        }

        if (slot->details->new_content_view != NULL && tell_new_content_view &&
	    (!tell_current_content_view ||
	     slot->details->new_content_view != slot->details->content_view) ) {
		view = slot->details->new_content_view;
		nemo_view_load_location (slot->details->new_content_view, location);
        }
	if (view != NULL) {
		/* new_content_view might have changed here if
		   report_load_underway was called from load_location */
		nemo_view_set_selection (view, selection_copy);
	}

	g_list_free_full (selection_copy, g_object_unref);

	nemo_profile_end (NULL);
}

static void
end_location_change (NemoWindowSlot *slot)
{
	char *uri;

	uri = nemo_window_slot_get_location_uri (slot);
	if (uri) {
		DEBUG ("Finished loading window for uri %s", uri);
		g_free (uri);
	}

	nemo_window_slot_set_allow_stop (slot, FALSE);

        /* Now we can free details->pending_scroll_to, since the load_complete
         * callback already has been emitted.
         */
	g_free (slot->details->pending_scroll_to);
	slot->details->pending_scroll_to = NULL;

	free_location_change (slot);
}

static void
free_location_change (NemoWindowSlot *slot)
{
	g_clear_object (&slot->details->pending_location);
	g_list_free_full (slot->details->pending_selection, g_object_unref);
	slot->details->pending_selection = NULL;

        /* Don't free details->pending_scroll_to, since thats needed until
         * the load_complete callback.
         */

	if (slot->details->mount_cancellable != NULL) {
		g_cancellable_cancel (slot->details->mount_cancellable);
		slot->details->mount_cancellable = NULL;
	}

        if (slot->details->determine_view_file != NULL) {
		nemo_file_cancel_call_when_ready
			(slot->details->determine_view_file,
			 got_file_info_for_view_selection_callback, slot);
                slot->details->determine_view_file = NULL;
        }

        if (slot->details->new_content_view != NULL) {
		slot->details->temporarily_ignore_view_signals = TRUE;
		nemo_view_stop_loading (slot->details->new_content_view);
		slot->details->temporarily_ignore_view_signals = FALSE;

		NemoWindow *window  = nemo_window_slot_get_window (slot);
		nemo_window_disconnect_content_view (window, slot->details->new_content_view);
		g_clear_object (&slot->details->new_content_view);
        }
}

static void
cancel_location_change (NemoWindowSlot *slot)
{
	GList *selection;
	GFile *location;

	location = nemo_window_slot_get_location (slot);

        if (slot->details->pending_location != NULL
            && location != NULL
            && slot->details->content_view != NULL) {

                /* No need to tell the new view - either it is the
                 * same as the old view, in which case it will already
                 * be told, or it is the very pending change we wish
                 * to cancel.
                 */
		selection = nemo_view_get_selection (slot->details->content_view);
                load_new_location (slot,
				   location,
				   selection,
				   TRUE,
				   FALSE);
		g_list_free_full (selection, g_object_unref);
        }

        end_location_change (slot);
}

static void
display_view_selection_failure (NemoWindow *window, NemoFile *file,
				GFile *location, GError *error)
{
	char *full_uri_for_display;
	char *uri_for_display;
	char *error_message;
	char *detail_message;
	char *scheme_string;

	/* Some sort of failure occurred. How 'bout we tell the user? */
	full_uri_for_display = g_file_get_parse_name (location);
	/* Truncate the URI so it doesn't get insanely wide. Note that even
	 * though the dialog uses wrapped text, if the URI doesn't contain
	 * white space then the text-wrapping code is too stupid to wrap it.
	 */
	uri_for_display = eel_str_middle_truncate
		(full_uri_for_display, MAX_URI_IN_DIALOG_LENGTH);
	g_free (full_uri_for_display);

	error_message = NULL;
	detail_message = NULL;
	if (error == NULL) {
		if (nemo_file_is_directory (file)) {
			error_message = g_strdup_printf
				(_("Could not display \"%s\"."),
				 uri_for_display);
			detail_message = g_strdup 
				(_("Unable to display the contents of this folder."));
		} else {
			error_message = g_strdup_printf
				(_("Could not display \"%s\"."),
				 uri_for_display);
			detail_message = g_strdup 
				(_("This location doesn't appear to be a folder."));
		}
	} else if (error->domain == G_IO_ERROR) {
		switch (error->code) {
		case G_IO_ERROR_NOT_FOUND:
			error_message = g_strdup_printf
				(_("Could not find \"%s\"."), 
				 uri_for_display);
			detail_message = g_strdup 
				(_("Please check the spelling and try again."));
			break;
		case G_IO_ERROR_NOT_SUPPORTED:
			scheme_string = g_file_get_uri_scheme (location);
				
			error_message = g_strdup_printf (_("Could not display \"%s\"."),
							 uri_for_display);
			if (scheme_string != NULL) {
				detail_message = g_strdup_printf (_("“%s” locations are not supported."),
								  scheme_string);
			} else {
				detail_message = g_strdup (_("Unable to handle this kind of location."));
			}
			g_free (scheme_string);
			break;
		case G_IO_ERROR_NOT_MOUNTED:
			error_message = g_strdup_printf (_("Could not display \"%s\"."),
							 uri_for_display);
			detail_message = g_strdup (_("Unable to mount the location."));
			break;
			
		case G_IO_ERROR_PERMISSION_DENIED:
			error_message = g_strdup_printf (_("Could not display \"%s\"."),
							 uri_for_display);
			detail_message = g_strdup (_("Don't have permission to access the requested location."));
			break;
			
		case G_IO_ERROR_HOST_NOT_FOUND:
			/* This case can be hit for user-typed strings like "foo" due to
			 * the code that guesses web addresses when there's no initial "/".
			 * But this case is also hit for legitimate web addresses when
			 * the proxy is set up wrong.
			 */
			error_message = g_strdup_printf (_("Could not display \"%s\", because the host could not be found."),
							 uri_for_display);
			detail_message = g_strdup (_("Please check the spelling or the network settings."));
			break;
		case G_IO_ERROR_CANCELLED:
		case G_IO_ERROR_FAILED_HANDLED:
			g_free (uri_for_display);
			return;
			
		default:
			break;
		}
	}
	
	if (detail_message == NULL) {
		error_message = g_strdup_printf (_("Could not display \"%s\"."),
						 uri_for_display);
		detail_message = g_strdup_printf (_("Unhandled error message: %s"), error->message);
	}
	
	eel_show_error_dialog (error_message, detail_message, GTK_WINDOW (window));

	g_free (uri_for_display);
	g_free (error_message);
	g_free (detail_message);
}

void
nemo_window_slot_set_content_view (NemoWindowSlot *slot,
				       const char *id)
{
	NemoFile *file;
	char *uri;

	g_assert (slot != NULL);
	g_assert (id != NULL);

	uri = nemo_window_slot_get_location_uri (slot);
	DEBUG ("Change view of window %s to %s", uri, id);
	g_free (uri);

	if (nemo_window_slot_content_view_matches_iid (slot, id)) {
        	return;
        }

        end_location_change (slot);

	file = nemo_file_get (slot->details->location);

	if (nemo_global_preferences_get_ignore_view_metadata()) {
		nemo_window_set_ignore_meta_view_id(nemo_window_slot_get_window(slot), id);
	} else {
		nemo_file_set_metadata(file, NEMO_METADATA_KEY_DEFAULT_VIEW, NULL, id);
	}

	nemo_file_unref(file);

	nemo_window_slot_set_allow_stop (slot, TRUE);

	if (nemo_view_get_selection_count (slot->details->content_view) == 0) {
		/* If there is no selection, queue a scroll to the same icon that
		 * is currently visible 
                 */
		slot->details->pending_scroll_to = nemo_view_get_first_visible_file (slot->details->content_view);
	}
	slot->details->location_change_type = NEMO_LOCATION_CHANGE_RELOAD;
	
        if (!create_content_view (slot, id, NULL)) {
		/* Just load the homedir. */
		nemo_window_slot_go_home (slot, FALSE);
	}
}

void
nemo_window_back_or_forward (NemoWindow *window, 
				 gboolean back,
				 guint distance,
				 NemoWindowOpenFlags flags)
{
	NemoWindowSlot *slot;
	GList *list;
	GFile *location;
	guint len;
	NemoBookmark *bookmark;
	GFile *old_location;

	slot = nemo_window_get_active_slot (window);
	list = back ? slot->details->back_list : slot->details->forward_list;

        len = (guint) g_list_length (list);

        /* If we can't move in the direction at all, just return. */
        if (len == 0)
                return;

        /* If the distance to move is off the end of the list, go to the end
           of the list. */
        if (distance >= len)
                distance = len - 1;

        bookmark = g_list_nth_data (list, distance);
	location = nemo_bookmark_get_location (bookmark);

	if (flags != 0) {
		nemo_window_slot_open_location (slot, location, flags);
	} else {
		char *scroll_pos;

		old_location = nemo_window_slot_get_location (slot);
		scroll_pos = nemo_bookmark_get_scroll_pos (bookmark);
		begin_location_change
			(slot,
			 location, old_location, NULL,
			 back ? NEMO_LOCATION_CHANGE_BACK : NEMO_LOCATION_CHANGE_FORWARD,
			 distance,
			 scroll_pos,
			 NULL, NULL);

		g_free (scroll_pos);
	}

	g_object_unref (location);
}

/* reload the contents of the window */
static void
nemo_window_slot_force_reload (NemoWindowSlot *slot)
{
	GFile *location;
        char *current_pos;
	GList *selection;

	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	location = nemo_window_slot_get_location (slot);
	if (location == NULL) {
		return;
	}

	/* peek_slot_field (window, location) can be free'd during the processing
	 * of begin_location_change, so make a copy
	 */
	g_object_ref (location);
	current_pos = NULL;
	selection = NULL;
	if (slot->details->content_view != NULL) {
		current_pos = nemo_view_get_first_visible_file (slot->details->content_view);
		selection = nemo_view_get_selection (slot->details->content_view);
	}
	begin_location_change
		(slot, location, location, selection,
		 NEMO_LOCATION_CHANGE_RELOAD, 0, current_pos,
		 NULL, NULL);
        g_free (current_pos);
	g_object_unref (location);
	g_list_free_full (selection, g_object_unref);
}

void
nemo_window_slot_queue_reload (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	if (nemo_window_slot_get_location (slot) == NULL) {
		return;
	}

	if (slot->details->pending_location != NULL
	    || slot->details->content_view == NULL
	    || nemo_view_get_loading (slot->details->content_view)) {
		/* there is a reload in flight */
		slot->details->needs_reload = TRUE;
		return;
	}

	nemo_window_slot_force_reload (slot);
}

void
nemo_window_slot_clear_forward_list (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	g_list_free_full (slot->details->forward_list, g_object_unref);
	slot->details->forward_list = NULL;
}

void
nemo_window_slot_clear_back_list (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	g_list_free_full (slot->details->back_list, g_object_unref);
	slot->details->back_list = NULL;
}

static void
nemo_window_slot_update_bookmark (NemoWindowSlot *slot, NemoFile *file)
{
        gboolean recreate;
	GFile *new_location;

	new_location = nemo_file_get_location (file);

	if (slot->details->current_location_bookmark == NULL) {
		recreate = TRUE;
	} else {
		GFile *bookmark_location;
		bookmark_location = nemo_bookmark_get_location (slot->details->current_location_bookmark);
		recreate = !g_file_equal (bookmark_location, new_location);
		g_object_unref (bookmark_location);
        }

	if (recreate) {
		char *display_name = NULL;

		/* We've changed locations, must recreate bookmark for current location. */
		g_clear_object (&slot->details->last_location_bookmark);
		slot->details->last_location_bookmark = slot->details->current_location_bookmark;

		display_name = nemo_file_get_display_name (file);
		slot->details->current_location_bookmark = nemo_bookmark_new (new_location, display_name);
		g_free (display_name);
        }

	g_object_unref (new_location);
}

static void
check_bookmark_location_matches (NemoBookmark *bookmark, GFile *location)
{
        GFile *bookmark_location;
        char *bookmark_uri, *uri;

	bookmark_location = nemo_bookmark_get_location (bookmark);
	if (!g_file_equal (location, bookmark_location)) {
		bookmark_uri = g_file_get_uri (bookmark_location);
		uri = g_file_get_uri (location);
		g_warning ("bookmark uri is %s, but expected %s", bookmark_uri, uri);
		g_free (uri);
		g_free (bookmark_uri);
	}
	g_object_unref (bookmark_location);
}

/* Debugging function used to verify that the last_location_bookmark
 * is in the state we expect when we're about to use it to update the
 * Back or Forward list.
 */
static void
check_last_bookmark_location_matches_slot (NemoWindowSlot *slot)
{
	check_bookmark_location_matches (slot->details->last_location_bookmark,
					 nemo_window_slot_get_location (slot));
}

static void
handle_go_direction (NemoWindowSlot *slot,
		     GFile              *location,
		     gboolean            forward)
{
	GList **list_ptr, **other_list_ptr;
	GList *list, *other_list, *link;
	NemoBookmark *bookmark;
	gint i;

	list_ptr = (forward) ? (&slot->details->forward_list) : (&slot->details->back_list);
	other_list_ptr = (forward) ? (&slot->details->back_list) : (&slot->details->forward_list);
	list = *list_ptr;
	other_list = *other_list_ptr;

	/* Move items from the list to the other list. */
	g_assert (g_list_length (list) > slot->details->location_change_distance);
	check_bookmark_location_matches (g_list_nth_data (list, slot->details->location_change_distance),
					 location);
	g_assert (nemo_window_slot_get_location (slot) != NULL);

	/* Move current location to list */
	check_last_bookmark_location_matches_slot (slot);

	/* Use the first bookmark in the history list rather than creating a new one. */
	other_list = g_list_prepend (other_list, slot->details->last_location_bookmark);
	g_object_ref (other_list->data);

	/* Move extra links from the list to the other list */
	for (i = 0; i < slot->details->location_change_distance; ++i) {
		bookmark = NEMO_BOOKMARK (list->data);
		list = g_list_remove (list, bookmark);
		other_list = g_list_prepend (other_list, bookmark);
	}

	/* One bookmark falls out of back/forward lists and becomes viewed location */
	link = list;
	list = g_list_remove_link (list, link);
	g_object_unref (link->data);
	g_list_free_1 (link);

	*list_ptr = list;
	*other_list_ptr = other_list;
}

static void
handle_go_elsewhere (NemoWindowSlot *slot,
		     GFile *location)
{
	GFile *slot_location;

	/* Clobber the entire forward list, and move displayed location to back list */
	nemo_window_slot_clear_forward_list (slot);
	slot_location = nemo_window_slot_get_location (slot);

	if (slot_location != NULL) {
		/* If we're returning to the same uri somehow, don't put this uri on back list.
		 * This also avoids a problem where set_displayed_location
		 * didn't update last_location_bookmark since the uri didn't change.
		 */
		if (!g_file_equal (slot_location, location)) {
			/* Store bookmark for current location in back list, unless there is no current location */
			check_last_bookmark_location_matches_slot (slot);
			/* Use the first bookmark in the history list rather than creating a new one. */
			slot->details->back_list = g_list_prepend (slot->details->back_list,
							  slot->details->last_location_bookmark);
			g_object_ref (slot->details->back_list->data);
		}
	}
}

static void
update_history (NemoWindowSlot *slot,
                NemoLocationChangeType type,
                GFile *new_location)
{
        switch (type) {
        case NEMO_LOCATION_CHANGE_STANDARD:
		handle_go_elsewhere (slot, new_location);
                return;
        case NEMO_LOCATION_CHANGE_RELOAD:
                /* for reload there is no work to do */
                return;
        case NEMO_LOCATION_CHANGE_BACK:
                handle_go_direction (slot, new_location, FALSE);
                return;
        case NEMO_LOCATION_CHANGE_FORWARD:
                handle_go_direction (slot, new_location, TRUE);
                return;
        }
	g_return_if_fail (FALSE);
}

typedef struct {
	NemoWindowSlot *slot;
	GCancellable *cancellable;
	GMount *mount;
} FindMountData;

static void
nemo_window_slot_show_x_content_bar (NemoWindowSlot *slot, GMount *mount, const char **x_content_types)
{
	GtkWidget *bar;

	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	bar = nemo_x_content_bar_new (mount, x_content_types);
	gtk_widget_show (bar);
	nemo_window_slot_add_extra_location_widget (slot, bar);
}

static void
found_content_type_cb (const char **x_content_types,
		       gpointer user_data)
{
	NemoWindowSlot *slot;
	FindMountData *data = user_data;

	if (g_cancellable_is_cancelled (data->cancellable)) {
		goto out;
	}

	slot = data->slot;

	if (x_content_types != NULL && x_content_types[0] != NULL) {
		nemo_window_slot_show_x_content_bar (slot, data->mount, x_content_types);
	}

	slot->details->find_mount_cancellable = NULL;

 out:
	g_object_unref (data->mount);
	g_object_unref (data->cancellable);
	g_free (data);
}

static void
found_mount_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	FindMountData *data = user_data;
	GMount *mount;

	if (g_cancellable_is_cancelled (data->cancellable)) {
		goto out;
	}

	mount = g_file_find_enclosing_mount_finish (G_FILE (source_object),
						    res,
						    NULL);
	if (mount != NULL) {
		data->mount = mount;
		nemo_get_x_content_types_for_mount_async (mount,
							      found_content_type_cb,
							      data->cancellable,
							      data);
		return;
	}

	data->slot->details->find_mount_cancellable = NULL;

 out:
	g_object_unref (data->cancellable);
	g_free (data);
}

static void
nemo_window_slot_emit_location_change (NemoWindowSlot *slot,
					   GFile *from,
					   GFile *to)
{
	char *from_uri = NULL;
	char *to_uri = NULL;

	if (from != NULL)
		from_uri = g_file_get_uri (from);
	if (to != NULL)
		to_uri = g_file_get_uri (to);
	g_signal_emit_by_name (slot, "location-changed", from_uri, to_uri);
	g_free (to_uri);
	g_free (from_uri);
}

static void
nemo_window_slot_show_trash_bar (NemoWindowSlot *slot)
{
	GtkWidget *bar;
	NemoView *view;

	view = nemo_window_slot_get_current_view (slot);
	bar = nemo_trash_bar_new (view);
	gtk_widget_show (bar);

	nemo_window_slot_add_extra_location_widget (slot, bar);
}

static void
nemo_window_slot_show_special_location_bar (NemoWindowSlot     *slot,
						NemoSpecialLocation special_location)
{
	GtkWidget *bar;

	bar = nemo_special_location_bar_new (special_location);
	gtk_widget_show (bar);

	nemo_window_slot_add_extra_location_widget (slot, bar);
}

static void
slot_add_extension_extra_widgets (NemoWindowSlot *slot)
{
	GList *providers, *l;
	GtkWidget *widget;
	char *uri;
	NemoWindow *window;

	providers = nemo_module_get_extensions_for_type (NEMO_TYPE_LOCATION_WIDGET_PROVIDER);
	window = nemo_window_slot_get_window (slot);

	uri = nemo_window_slot_get_location_uri (slot);
	for (l = providers; l != NULL; l = l->next) {
		NemoLocationWidgetProvider *provider;

		provider = NEMO_LOCATION_WIDGET_PROVIDER (l->data);
		widget = nemo_location_widget_provider_get_widget (provider, uri, GTK_WIDGET (window));
		if (widget != NULL) {
			nemo_window_slot_add_extra_location_widget (slot, widget);
		}
	}
	g_free (uri);

	nemo_module_extension_list_free (providers);
}

/* Handle the changes for the NemoWindow itself. */
static void
nemo_window_slot_update_for_new_location (NemoWindowSlot *slot)
{
	NemoWindow *window;
	NemoWindowPane *pane;
	GFile *new_location, *old_location;
	NemoFile *file;
	NemoDirectory *directory;
	gboolean location_really_changed;
	FindMountData *data;

	window = nemo_window_slot_get_window (slot);
	pane = nemo_window_slot_get_pane (slot);
	new_location = g_object_ref(slot->details->pending_location);
	g_clear_object (&slot->details->pending_location);

	file = nemo_file_get (new_location);
	nemo_window_slot_update_bookmark (slot, file);

	update_history (slot, slot->details->location_change_type, new_location);
	old_location = nemo_window_slot_get_location (slot);

	location_really_changed =
		old_location == NULL ||
		!g_file_equal (old_location, new_location);

	/* Create a NemoFile for this location, so we can catch it
	 * if it goes away.
	 */
	nemo_window_slot_set_viewed_file (slot, file);
	slot->details->viewed_file_seen = !nemo_file_is_not_yet_confirmed (file);
	slot->details->viewed_file_in_trash = nemo_file_is_in_trash (file);
	nemo_file_unref (file);

	nemo_window_slot_set_location (slot, new_location);

	if (slot == nemo_window_get_active_slot (window)) {
		/* Sync up and zoom action states */
		nemo_window_sync_up_button (window);
		nemo_window_sync_zoom_widgets (window);

		/* Set up the content view menu for this new location. */
		nemo_window_load_view_as_menus (window);

		/* Load menus from nemo extensions for this location */
		nemo_window_load_extension_menus (window);
	}

	if (location_really_changed) {
		nemo_window_slot_remove_extra_location_widgets (slot);

		directory = nemo_directory_get (new_location);

		if (nemo_directory_is_in_trash (directory)) {
			nemo_window_slot_show_trash_bar (slot);
		} else {
			GFile *scripts_file;
			char *scripts_path = nemo_get_scripts_directory_path ();
			scripts_file = g_file_new_for_path (scripts_path);
			GFile *actions_file;
			gchar *actions_path = nemo_action_manager_get_user_directory_path ();
			actions_file = g_file_new_for_path (actions_path);
			g_free (scripts_path);
			if (nemo_should_use_templates_directory () &&
			    nemo_file_is_user_special_directory (file, G_USER_DIRECTORY_TEMPLATES)) {
				nemo_window_slot_show_special_location_bar (slot, NEMO_SPECIAL_LOCATION_TEMPLATES);
			} else if (g_file_equal (slot->details->location, scripts_file)) {
				nemo_window_slot_show_special_location_bar (slot, NEMO_SPECIAL_LOCATION_SCRIPTS);
			} else if (g_file_equal (new_location, actions_file)) {
				nemo_window_slot_show_special_location_bar (slot, NEMO_SPECIAL_LOCATION_ACTIONS);
			}
			g_object_unref (scripts_file);
			g_object_unref (actions_file);
		}

		/* need the mount to determine if we should put up the x-content cluebar */
		if (slot->details->find_mount_cancellable != NULL) {
			g_cancellable_cancel (slot->details->find_mount_cancellable);
			slot->details->find_mount_cancellable = NULL;
		}

		data = g_new (FindMountData, 1);
		data->slot = slot;
		data->cancellable = g_cancellable_new ();
		data->mount = NULL;

		slot->details->find_mount_cancellable = data->cancellable;
		g_file_find_enclosing_mount_async (new_location,
						   G_PRIORITY_DEFAULT,
						   data->cancellable,
						   found_mount_cb,
						   data);

		nemo_directory_unref (directory);

		slot_add_extension_extra_widgets (slot);
	}

	nemo_window_slot_update_title (slot);

	if (slot == pane->active_slot) {
		nemo_window_pane_sync_location_widgets (pane);

		if (location_really_changed) {
			nemo_window_slot_sync_search_widgets (slot);
		}
	}

	nemo_window_sync_menu_bar (window);
	g_object_unref (new_location);
}

static void
view_end_loading_cb (NemoView       *view,
		     gboolean            all_files_seen,
		     NemoWindowSlot *slot)
{
	if (slot->details->temporarily_ignore_view_signals) {
		return;
	}

        /* Only handle this if we're expecting it.
         * Don't handle it if its from an old view we've switched from */
        NemoView *slot_view = nemo_window_slot_get_view (slot);
        if (view == slot->details->content_view && all_files_seen) {
	        if (slot->details->pending_scroll_to != NULL) {
		        nemo_view_scroll_to_file (slot_view,
					              slot->details->pending_scroll_to);
	        }
	        end_location_change (slot);
        }	

        if (slot->details->needs_reload) {
		nemo_window_slot_queue_reload (slot);
		slot->details->needs_reload = FALSE;
	}

	remove_loading_floating_bar (slot);
}

static void
real_setup_loading_floating_bar (NemoWindowSlot *slot)
{
	gboolean disable_chrome;

	g_object_get (nemo_window_slot_get_window (slot),
		      "disable-chrome", &disable_chrome,
		      NULL);

	if (disable_chrome) {
		gtk_widget_hide (slot->details->floating_bar);
		return;
	}

	nemo_floating_bar_cleanup_actions (NEMO_FLOATING_BAR (slot->details->floating_bar));
	nemo_floating_bar_set_primary_label (NEMO_FLOATING_BAR (slot->details->floating_bar),
						 NEMO_IS_SEARCH_DIRECTORY (nemo_view_get_model (slot->details->content_view)) ?
						 _("Searching…") : _("Loading…"));
	nemo_floating_bar_set_details_label (NEMO_FLOATING_BAR (slot->details->floating_bar), NULL);
	nemo_floating_bar_set_show_spinner (NEMO_FLOATING_BAR (slot->details->floating_bar),
						TRUE);
	nemo_floating_bar_add_action (NEMO_FLOATING_BAR (slot->details->floating_bar),
					  GTK_STOCK_STOP,
					  NEMO_FLOATING_BAR_ACTION_ID_STOP);

	gtk_widget_set_halign (slot->details->floating_bar, GTK_ALIGN_END);
	gtk_widget_show (slot->details->floating_bar);
}

static gboolean
setup_loading_floating_bar_timeout_cb (gpointer user_data)
{
	NemoWindowSlot *slot = user_data;

	slot->details->loading_timeout_id = 0;
	real_setup_loading_floating_bar (slot);

	return FALSE;
}

static void
setup_loading_floating_bar (NemoWindowSlot *slot)
{
	/* setup loading overlay */
	if (slot->details->set_status_timeout_id != 0) {
		g_source_remove (slot->details->set_status_timeout_id);
		slot->details->set_status_timeout_id = 0;
	}

	if (slot->details->loading_timeout_id != 0) {
		g_source_remove (slot->details->loading_timeout_id);
		slot->details->loading_timeout_id = 0;
	}

	slot->details->loading_timeout_id =
		g_timeout_add (500, setup_loading_floating_bar_timeout_cb, slot);
}

static void
view_begin_loading_cb (NemoView *view,
		       NemoWindowSlot *slot)
{
	if (slot->details->temporarily_ignore_view_signals) {
		return;
	}

	nemo_profile_start (NULL);

	if (view == slot->details->new_content_view) {
		location_has_really_changed (slot);
	} else {
		nemo_window_slot_set_allow_stop (slot, TRUE);
	}

	setup_loading_floating_bar (slot);

	nemo_profile_end (NULL);
}

static void
nemo_window_slot_connect_new_content_view (NemoWindowSlot *slot)
{
	if (slot->details->content_view != NULL) {
		/* disconnect old view */
		g_signal_handlers_disconnect_by_func (slot->details->content_view, G_CALLBACK (view_end_loading_cb), slot);
		g_signal_handlers_disconnect_by_func (slot->details->content_view, G_CALLBACK (view_begin_loading_cb), slot);
	}

	if (slot->details->new_content_view != NULL) {
		g_signal_connect (slot->details->new_content_view, "begin-loading", G_CALLBACK (view_begin_loading_cb), slot);
		g_signal_connect (slot->details->new_content_view, "end-loading", G_CALLBACK (view_end_loading_cb), slot);
	}
}

static void
nemo_window_slot_switch_new_content_view (NemoWindowSlot *slot)
{
	NemoWindow *window;
	GtkWidget *widget;

	if ((slot->details->new_content_view == NULL) ||
	    gtk_widget_get_parent (GTK_WIDGET (slot->details->new_content_view)) != NULL) {
		return;
	}

	window = nemo_window_slot_get_window (slot);

	if (slot->details->content_view != NULL) {
		nemo_window_disconnect_content_view (window, slot->details->content_view);

		widget = GTK_WIDGET (slot->details->content_view);
		// Do not destroy here, because the view might be a drag source
                g_signal_emit_by_name (slot, "inactive");
		gtk_container_remove (GTK_CONTAINER (slot->details->view_overlay), widget);
                g_clear_object(&slot->details->content_view);
	}

	if (slot->details->new_content_view != NULL) {
		slot->details->content_view = slot->details->new_content_view;
		slot->details->new_content_view = NULL;

		widget = GTK_WIDGET (slot->details->content_view);
		gtk_container_add (GTK_CONTAINER (slot->details->view_overlay), widget);
		gtk_widget_show (widget);

		// connect new view
		nemo_window_connect_content_view (slot->details->pane->window, slot->details->content_view);

		if (!NEMO_IS_SEARCH_DIRECTORY (nemo_view_get_model (slot->details->content_view)) &&
		    slot == nemo_window_get_active_slot (window)) {
			nemo_view_grab_focus (slot->details->content_view);
		}
	}
}

/* This is called when we have decided we can actually change to the new view/location situation. */
static void
location_has_really_changed (NemoWindowSlot *slot)
{
	NemoWindow *window;
	GFile *location_copy;

	window = nemo_window_slot_get_window (slot);

	/* Switch to the new content view. */
	nemo_window_slot_switch_new_content_view (slot);

	if (slot->details->pending_location != NULL) {
		/* Tell the window we are finished. */
		nemo_window_slot_update_for_new_location (slot);
	}

	location_copy = NULL;
	if (slot->details->location != NULL) {
		location_copy = g_object_ref (slot->details->location);
	}

	if (location_copy != NULL) {
		if (slot == nemo_window_get_active_slot (window)) {
			char *uri;

			uri = g_file_get_uri (location_copy);
			g_signal_emit_by_name (window, "loading-uri", uri);
			g_free (uri);
		}

		g_object_unref (location_copy);
	}
}

static void
nemo_window_slot_dispose (GObject *object)
{
	NemoWindowSlot *slot;
	GtkWidget *widget;

	slot = NEMO_WINDOW_SLOT (object);

	nemo_window_slot_clear_forward_list (slot);
	nemo_window_slot_clear_back_list (slot);
	nemo_window_slot_remove_extra_location_widgets (slot);

	nemo_window_slot_remove_extra_location_widgets (slot);

	if (slot->details->content_view) {
		widget = GTK_WIDGET (slot->details->content_view);
		gtk_widget_destroy (widget);
		g_clear_object (&slot->details->content_view);
	}

	if (slot->details->new_content_view) {
		widget = GTK_WIDGET (slot->details->new_content_view);
		gtk_widget_destroy (widget);
		g_clear_object (&slot->details->new_content_view);
	}

	if (slot->details->set_status_timeout_id != 0) {
		g_source_remove (slot->details->set_status_timeout_id);
		slot->details->set_status_timeout_id = 0;
	}

	if (slot->details->loading_timeout_id != 0) {
		g_source_remove (slot->details->loading_timeout_id);
		slot->details->loading_timeout_id = 0;
	}

	nemo_window_slot_set_viewed_file (slot, NULL);
	/* TODO? why do we unref here? the file is NULL.
	 * It was already here before the slot move, though */
	nemo_file_unref (slot->details->viewed_file);

	g_clear_object (&slot->details->location);

	g_clear_object (&slot->details->current_location_bookmark);
	g_clear_object (&slot->details->last_location_bookmark);

	if (slot->details->find_mount_cancellable != NULL) {
		g_cancellable_cancel (slot->details->find_mount_cancellable);
		slot->details->find_mount_cancellable = NULL;
	}

	slot->details->pane = NULL;

	g_free (slot->details->title);
	slot->details->title = NULL;

	free_location_change (slot);

	g_free (slot->details->status_text);
	slot->details->status_text = NULL;

	G_OBJECT_CLASS (nemo_window_slot_parent_class)->dispose (object);
}

static void
nemo_window_slot_class_init (NemoWindowSlotClass *klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	klass->active = real_active;
	klass->inactive = real_inactive;

	oclass->dispose = nemo_window_slot_dispose;
	oclass->constructed = nemo_window_slot_constructed;
	oclass->set_property = nemo_window_slot_set_property;
	oclass->get_property = nemo_window_slot_get_property;

	signals[ACTIVE] =
		g_signal_new ("active",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NemoWindowSlotClass, active),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[INACTIVE] =
		g_signal_new ("inactive",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NemoWindowSlotClass, inactive),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	signals[CHANGED_PANE] =
		g_signal_new ("changed-pane",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET (NemoWindowSlotClass, changed_pane),
			NULL, NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);

	signals[LOCATION_CHANGED] =
		g_signal_new ("location-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_generic,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_STRING);

	properties[PROP_PANE] =
		g_param_spec_object ("pane",
				     "The NemoWindowPane",
				     "The NemoWindowPane this slot is part of",
				     NEMO_TYPE_WINDOW_PANE,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
	g_type_class_add_private (klass, sizeof (NemoWindowSlotDetails));
}

GFile *
nemo_window_slot_get_location (NemoWindowSlot *slot)
{
	g_assert (slot != NULL);

	return slot->details->location;
}

const gchar *
nemo_window_slot_get_title (NemoWindowSlot *slot)
{
	return slot->details->title;
}

char *
nemo_window_slot_get_location_uri (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	if (slot->details->location) {
		return g_file_get_uri (slot->details->location);
	}
	return NULL;
}

void
nemo_window_slot_make_hosting_pane_active (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_SLOT (slot));
	
	nemo_window_set_active_slot (nemo_window_slot_get_window (slot),
					 slot);
}

NemoWindow *
nemo_window_slot_get_window (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_SLOT (slot));
	return slot->details->pane->window;
}

NemoWindowPane *
nemo_window_slot_get_pane (NemoWindowSlot *slot)
{
    g_assert (NEMO_IS_WINDOW_SLOT (slot));
    return slot->details->pane;
}

void
nemo_window_slot_set_pane (NemoWindowSlot *slot,
				 NemoWindowPane *pane)
{
	g_assert (NEMO_IS_WINDOW_SLOT (slot));
	g_assert (NEMO_IS_WINDOW_PANE (pane));

	if (slot->details->pane != pane) {
		slot->details->pane = pane;
		g_object_notify_by_pspec (G_OBJECT (slot), properties[PROP_PANE]);
	}
}

NemoView *
nemo_window_slot_get_view (NemoWindowSlot *slot)
{
	return slot->details->content_view;
}

NemoView *
nemo_window_slot_get_new_view (NemoWindowSlot *slot)
{
	return slot->details->new_content_view;
}

/* nemo_window_slot_update_title:
 * 
 * Re-calculate the slot title.
 * Called when the location or view has changed.
 * @slot: The NemoWindowSlot in question.
 * 
 */
void
nemo_window_slot_update_title (NemoWindowSlot *slot)
{
	NemoWindow *window;
	char *title;
	gboolean do_sync = FALSE;

	title = nemo_compute_title_for_location (slot->details->location);
	window = nemo_window_slot_get_window (slot);

	if (g_strcmp0 (title, slot->details->title) != 0) {
		do_sync = TRUE;

		g_free (slot->details->title);
		slot->details->title = title;
		title = NULL;
	}

	if (strlen (slot->details->title) > 0 &&
	    slot->details->current_location_bookmark != NULL) {
		do_sync = TRUE;
	}

	if (do_sync) {
		nemo_window_sync_title (window, slot);
	}

	if (title != NULL) {
		g_free (title);
	}
}

gboolean
nemo_window_slot_get_allow_stop (NemoWindowSlot *slot)
{
	return slot->details->allow_stop;
}

void
nemo_window_slot_set_allow_stop (NemoWindowSlot *slot,
				     gboolean allow)
{
	NemoWindow *window;

	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	slot->details->allow_stop = allow;

	window = nemo_window_slot_get_window (slot);
	nemo_window_sync_allow_stop (window, slot);
}

void
nemo_window_slot_stop_loading (NemoWindowSlot *slot)
{
	nemo_view_stop_loading (slot->details->content_view);

	if (slot->details->new_content_view != NULL) {
		slot->details->temporarily_ignore_view_signals = TRUE;
		nemo_view_stop_loading (slot->details->new_content_view);
		slot->details->temporarily_ignore_view_signals = FALSE;
	}

        cancel_location_change (slot);
}

static void
real_slot_set_short_status (NemoWindowSlot *slot,
			    const gchar *primary_status,
			    const gchar *detail_status)
{	
	gboolean show_statusbar;
	gboolean disable_chrome;

	show_statusbar = g_settings_get_boolean (nemo_window_state,
						 NEMO_WINDOW_STATE_START_WITH_STATUS_BAR);

	g_object_get (nemo_window_slot_get_window (slot),
		      "disable-chrome", &disable_chrome,
		      NULL);

	if (show_statusbar || disable_chrome) {
		return;
	}

	nemo_floating_bar_cleanup_actions (NEMO_FLOATING_BAR (slot->details->floating_bar));
	nemo_floating_bar_set_show_spinner (NEMO_FLOATING_BAR (slot->details->floating_bar),
						FALSE);

	if ((primary_status == NULL && detail_status == NULL)) {
		gtk_widget_hide (slot->details->floating_bar);
		return;
	}

	nemo_floating_bar_set_labels (NEMO_FLOATING_BAR (slot->details->floating_bar),
					  primary_status, detail_status);
	gtk_widget_show (slot->details->floating_bar);
}

typedef struct {
	gchar *primary_status;
	gchar *detail_status;
	NemoWindowSlot *slot;
} SetStatusData;

static void
set_status_data_free (gpointer data)
{
	SetStatusData *status_data = data;

	g_free (status_data->primary_status);
	g_free (status_data->detail_status);

	g_slice_free (SetStatusData, data);
}

static gboolean
set_status_timeout_cb (gpointer data)
{
	SetStatusData *status_data = data;

	status_data->slot->details->set_status_timeout_id = 0;
	real_slot_set_short_status (status_data->slot,
				    status_data->primary_status,
				    status_data->detail_status);

	return FALSE;
}

static void
set_floating_bar_status (NemoWindowSlot *slot,
			 const gchar *primary_status,
			 const gchar *detail_status)
{
	GtkSettings *settings;
	gint double_click_time;
	SetStatusData *status_data;

	if (slot->details->set_status_timeout_id != 0) {
		g_source_remove (slot->details->set_status_timeout_id);
		slot->details->set_status_timeout_id = 0;
	}

	settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (slot->details->content_view)));
	g_object_get (settings,
		      "gtk-double-click-time", &double_click_time,
		      NULL);

	status_data = g_slice_new0 (SetStatusData);
	status_data->primary_status = g_strdup (primary_status);
	status_data->detail_status = g_strdup (detail_status);
	status_data->slot = slot;

	/* waiting for half of the double-click-time before setting
	 * the status seems to be a good approximation of not setting it
	 * too often and not delaying the statusbar too much.
	 */
	slot->details->set_status_timeout_id =
		g_timeout_add_full (G_PRIORITY_DEFAULT,
				    (guint) (double_click_time / 2),
				    set_status_timeout_cb,
				    status_data,
				    set_status_data_free);
}

void
nemo_window_slot_set_status (NemoWindowSlot *slot,
				 const char *primary_status,
				 const char *detail_status)
{
	NemoWindow *window;

	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	g_free (slot->details->status_text);
	if (primary_status) {
	    if (detail_status) {
	        slot->details->status_text = g_strdup_printf ("%s %s",
                                                 primary_status,
                                                 detail_status);
	    } else {
	        slot->details->status_text = g_strdup(primary_status);
	    }
	} else {
	    slot->details->status_text = g_strdup(detail_status);
	}

	if (slot->details->content_view != NULL) {
		set_floating_bar_status (slot, primary_status, detail_status);
	}

	window = nemo_window_slot_get_window (slot);
	if (slot == nemo_window_get_active_slot (window)) {
		nemo_window_push_status (window, slot->details->status_text);
	}
}

/* returns either the pending or the actual current uri */
char *
nemo_window_slot_get_current_uri (NemoWindowSlot *slot)
{
	if (slot->details->pending_location != NULL) {
		return g_file_get_uri (slot->details->pending_location);
	}

	if (slot->details->location != NULL) {
		return g_file_get_uri (slot->details->location);
	}

	return NULL;
}

NemoView *
nemo_window_slot_get_current_view (NemoWindowSlot *slot)
{
	if (slot->details->content_view != NULL) {
		return slot->details->content_view;
	} else if (slot->details->new_content_view) {
		return slot->details->new_content_view;
	}

	return NULL;
}

void
nemo_window_slot_go_home (NemoWindowSlot *slot,
			      NemoWindowOpenFlags flags)
{			      
	GFile *home;

	g_return_if_fail (NEMO_IS_WINDOW_SLOT (slot));

	home = g_file_new_for_path (g_get_home_dir ());
	nemo_window_slot_open_location (slot, home, flags);
	g_object_unref (home);
}

void
nemo_window_slot_go_up (NemoWindowSlot *slot,
			    NemoWindowOpenFlags flags)
{
	GFile *parent;
	NemoDirectory *directory;
	NemoWindow *window;

	if (slot->details->location == NULL) {
		return;
	}

	directory = nemo_directory_get (slot->details->location);
	if (NEMO_IS_SEARCH_DIRECTORY (directory)) {
		window = nemo_window_slot_get_window (slot);
		nemo_window_back_or_forward (window, TRUE, 0, 0);
	}

	parent = g_file_get_parent (slot->details->location);
	if (parent == NULL) {
		return;
	}

	nemo_window_slot_open_location (slot, parent, flags);
	g_object_unref (parent);
}

NemoFile *
nemo_window_slot_get_file (NemoWindowSlot *slot)
{
	return slot->details->viewed_file;
}

NemoBookmark *
nemo_window_slot_get_bookmark (NemoWindowSlot *slot)
{
	return slot->details->current_location_bookmark;
}

GList *
nemo_window_slot_get_back_history (NemoWindowSlot *slot)
{
	return slot->details->back_list;
}

GList *
nemo_window_slot_get_forward_history (NemoWindowSlot *slot)
{
	return slot->details->forward_list;
}

NemoWindowSlot *
nemo_window_slot_new (NemoWindowPane *pane)
{
	return g_object_new (NEMO_TYPE_WINDOW_SLOT,
			     "pane", pane,
			     NULL);
}

/* A view started to load the location its viewing, either due to
 * a load_location request, or some internal reason. Expect
 * a matching load_compete later
 */
void
nemo_window_report_load_underway (NemoWindow *window,
				      NemoView *view)
{
	NemoWindowSlot *slot;

	slot = nemo_window_get_slot_for_view (window, view);

	if (slot->details->temporarily_ignore_view_signals) {
		return;
	}

	if (view == slot->details->new_content_view) {
		location_has_really_changed (slot);
	} else {
		nemo_window_slot_set_allow_stop (slot, TRUE);
	}
}

const char *
nemo_window_slot_get_content_view_id (NemoWindowSlot *slot)
{
	NemoView *view = nemo_window_slot_get_view (slot);

	if (view == NULL) {
		return NULL;
	}
	return nemo_view_get_view_id (view);
}

void
nemo_window_slot_check_bad_cache_bar (NemoWindowSlot *slot)
{
    if (NEMO_IS_DESKTOP_WINDOW (nemo_window_slot_get_window (slot)))
        return;

	NemoApplication *app = NEMO_APPLICATION (g_application_get_default ());

    if (nemo_application_get_cache_bad (app) &&
        !nemo_application_get_cache_problem_ignored (app)) {
        if (slot->details->cache_bar != NULL) {
            gtk_widget_show (slot->details->cache_bar);
        } else {
            GtkWidget *bad_bar = nemo_thumbnail_problem_bar_new (nemo_window_slot_get_current_view (slot));
            if (bad_bar) {
                gtk_widget_show (bad_bar);
                nemo_window_slot_add_extra_location_widget (slot, bad_bar);
                slot->details->cache_bar = bad_bar;
            }
        }
    } else {
        if (slot->details->cache_bar != NULL) {
            gtk_widget_hide (slot->details->cache_bar);
        }
    }
}

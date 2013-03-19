/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-window-slot.c: Nautilus window slot
 
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
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Christian Neumair <cneumair@gnome.org>
*/

#include "config.h"

#include "nautilus-window-slot.h"

#include "nautilus-actions.h"
#include "nautilus-application.h"
#include "nautilus-canvas-view.h"
#include "nautilus-desktop-window.h"
#include "nautilus-floating-bar.h"
#include "nautilus-list-view.h"
#include "nautilus-special-location-bar.h"
#include "nautilus-toolbar.h"
#include "nautilus-trash-bar.h"
#include "nautilus-view-factory.h"
#include "nautilus-window-private.h"
#include "nautilus-x-content-bar.h"

#include <glib/gi18n.h>
#include <eel/eel-stock-dialogs.h>

#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-monitor.h>
#include <libnautilus-private/nautilus-profile.h>
#include <libnautilus-extension/nautilus-location-widget-provider.h>

G_DEFINE_TYPE (NautilusWindowSlot, nautilus_window_slot, GTK_TYPE_BOX);

enum {
	ACTIVE,
	INACTIVE,
	LOCATION_CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_WINDOW = 1,
	NUM_PROPERTIES
};

struct NautilusWindowSlotDetails {
	NautilusWindow *window;

	/* floating bar */
	guint set_status_timeout_id;
	guint loading_timeout_id;
	GtkWidget *floating_bar;
	GtkWidget *view_overlay;

	/* slot contains
	 *  1) an vbox containing extra_location_widgets
	 *  2) the view
	 */
	GtkWidget *extra_location_widgets;

	/* Current location. */
	GFile *location;
	gchar *title;

	/* Viewed file */
	NautilusView *content_view;
	NautilusView *new_content_view;
	NautilusFile *viewed_file;
	gboolean viewed_file_seen;
	gboolean viewed_file_in_trash;

	/* Information about bookmarks and history list */
	NautilusBookmark *current_location_bookmark;
	NautilusBookmark *last_location_bookmark;
	GList *back_list;
	GList *forward_list;

	/* Query editor */
	NautilusQueryEditor *query_editor;
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
	NautilusLocationChangeType location_change_type;
	guint location_change_distance;
	char *pending_scroll_to;
	GList *pending_selection;
	gboolean pending_use_default_location;
	NautilusFile *determine_view_file;
	GCancellable *mount_cancellable;
	GError *mount_error;
	gboolean tried_mount;
	NautilusWindowGoToCallback open_callback;
	gpointer open_callback_user_data;
};

static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static void nautilus_window_slot_force_reload (NautilusWindowSlot *slot);
static void location_has_really_changed (NautilusWindowSlot *slot);
static void nautilus_window_slot_connect_new_content_view (NautilusWindowSlot *slot);
static void nautilus_window_slot_emit_location_change (NautilusWindowSlot *slot, GFile *from, GFile *to);

static void
nautilus_window_slot_sync_search_widgets (NautilusWindowSlot *slot)
{
	NautilusDirectory *directory;
	gboolean toggle;

	if (slot != nautilus_window_get_active_slot (slot->details->window)) {
		return;
	}

	toggle = slot->details->search_visible;

	if (slot->details->content_view != NULL) {
		directory = nautilus_view_get_model (slot->details->content_view);
		if (NAUTILUS_IS_SEARCH_DIRECTORY (directory)) {
			toggle = TRUE;
		}
	}

	nautilus_window_slot_set_search_visible (slot, toggle);
}

static gboolean
nautilus_window_slot_content_view_matches_iid (NautilusWindowSlot *slot,
					       const char *iid)
{
	if (slot->details->content_view == NULL) {
		return FALSE;
	}
	return g_strcmp0 (nautilus_view_get_view_id (slot->details->content_view), iid) == 0;
}

static void
nautilus_window_slot_sync_view_as_menus (NautilusWindowSlot *slot)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	if (slot != nautilus_window_get_active_slot (slot->details->window)) {
		return;
	}

	if (slot->details->content_view == NULL || slot->details->new_content_view != NULL) {
		return;
	}

	action_group = nautilus_window_get_main_action_group (slot->details->window);

	if (nautilus_window_slot_content_view_matches_iid (slot, NAUTILUS_LIST_VIEW_ID)) {
		action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_VIEW_LIST);
	} else if (nautilus_window_slot_content_view_matches_iid (slot, NAUTILUS_CANVAS_VIEW_ID)) {
		action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_VIEW_GRID);
	} else {
		action = NULL;
	}

	if (action != NULL) {
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
	}
}

static void
sync_search_directory (NautilusWindowSlot *slot)
{
	NautilusDirectory *directory;
	NautilusQuery *query;
	gchar *text;
	GFile *location;

	g_assert (NAUTILUS_IS_FILE (slot->details->viewed_file));

	directory = nautilus_directory_get_for_file (slot->details->viewed_file);
	g_assert (NAUTILUS_IS_SEARCH_DIRECTORY (directory));

	query = nautilus_query_editor_get_query (slot->details->query_editor);
	text = nautilus_query_get_text (query);

	if (!strlen (text)) {
		/* Prevent the location change from hiding the query editor in this case */
		slot->details->load_with_search = TRUE;
		location = nautilus_query_editor_get_location (slot->details->query_editor);
		nautilus_window_slot_open_location (slot, location, 0);
		g_object_unref (location);
	} else {
		nautilus_search_directory_set_query (NAUTILUS_SEARCH_DIRECTORY (directory),
						     query);
		nautilus_window_slot_force_reload (slot);
	}

	g_free (text);
	g_object_unref (query);
	nautilus_directory_unref (directory);
}

static void
create_new_search (NautilusWindowSlot *slot)
{
	char *uri;
	NautilusDirectory *directory;
	GFile *location;
	NautilusQuery *query;

	uri = nautilus_search_directory_generate_new_uri ();
	location = g_file_new_for_uri (uri);

	directory = nautilus_directory_get (location);
	g_assert (NAUTILUS_IS_SEARCH_DIRECTORY (directory));

	query = nautilus_query_editor_get_query (slot->details->query_editor);
	nautilus_search_directory_set_query (NAUTILUS_SEARCH_DIRECTORY (directory), query);

	nautilus_window_slot_open_location (slot, location, 0);

	nautilus_directory_unref (directory);
	g_object_unref (query);
	g_object_unref (location);
	g_free (uri);
}

static void
query_editor_cancel_callback (NautilusQueryEditor *editor,
			      NautilusWindowSlot *slot)
{
	nautilus_window_slot_set_search_visible (slot, FALSE);
}

static void
query_editor_activated_callback (NautilusQueryEditor *editor,
				 NautilusWindowSlot *slot)
{
	if (slot->details->content_view != NULL) {
		nautilus_view_activate_selection (slot->details->content_view);
	}
}

static void
query_editor_changed_callback (NautilusQueryEditor *editor,
			       NautilusQuery *query,
			       gboolean reload,
			       NautilusWindowSlot *slot)
{
	NautilusDirectory *directory;

	g_assert (NAUTILUS_IS_FILE (slot->details->viewed_file));

	directory = nautilus_directory_get_for_file (slot->details->viewed_file);
	if (!NAUTILUS_IS_SEARCH_DIRECTORY (directory)) {
		/* this is the first change from the query editor. we
		   ask for a location change to the search directory,
		   indicate the directory needs to be sync'd with the
		   current query. */
		create_new_search (slot);
	} else {
		sync_search_directory (slot);
	}

	nautilus_directory_unref (directory);
}

static void
hide_query_editor (NautilusWindowSlot *slot)
{
	gtk_widget_hide (GTK_WIDGET (slot->details->query_editor));

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

	nautilus_query_editor_set_query (slot->details->query_editor, NULL);
}

static void
show_query_editor (NautilusWindowSlot *slot)
{
	NautilusDirectory *directory;
	NautilusSearchDirectory *search_directory;
	GFile *location;

	if (slot->details->location) {
		location = slot->details->location;
	} else {
		location = slot->details->pending_location;
	}

	directory = nautilus_directory_get (location);

	if (NAUTILUS_IS_SEARCH_DIRECTORY (directory)) {
		NautilusQuery *query;
		search_directory = NAUTILUS_SEARCH_DIRECTORY (directory);
		query = nautilus_search_directory_get_query (search_directory);
		if (query != NULL) {
			nautilus_query_editor_set_query (slot->details->query_editor,
							 query);
			g_object_unref (query);
		}
	} else {
		nautilus_query_editor_set_location (slot->details->query_editor, location);
	}

	nautilus_directory_unref (directory);

	gtk_widget_show (GTK_WIDGET (slot->details->query_editor));
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
nautilus_window_slot_set_search_visible (NautilusWindowSlot *slot,
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
	active_slot = (slot == nautilus_window_get_active_slot (slot->details->window));

	if (visible) {
		show_query_editor (slot);
	} else {
		/* If search was active on this slot and became inactive, change
		 * the slot location to the real directory.
		 */
		if (old_visible && active_slot) {
			/* Use the query editor search root if possible */
			return_location = nautilus_window_slot_get_query_editor_location (slot);

			/* Use the home directory as a fallback */
			if (return_location == NULL) {
				return_location = g_file_new_for_path (g_get_home_dir ());
			}
		}

		if (active_slot) {
			nautilus_window_grab_focus (slot->details->window);
		}

		/* Now hide the editor and clear its state */
		hide_query_editor (slot);
	}

	if (!active_slot) {
		return;
	}

	/* also synchronize the window action state */
	action_group = nautilus_window_get_main_action_group (slot->details->window);
	action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_SEARCH);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);

	/* If search was active on this slot and became inactive, change
	 * the slot location to the real directory.
	 */
	if (return_location != NULL) {
		nautilus_window_slot_open_location (slot, return_location, 0);
		g_object_unref (return_location);
	}
}

GFile *
nautilus_window_slot_get_query_editor_location (NautilusWindowSlot *slot)
{
	return nautilus_query_editor_get_location (slot->details->query_editor);
}

gboolean
nautilus_window_slot_handle_event (NautilusWindowSlot *slot,
				   GdkEventKey        *event)
{
	NautilusWindow *window;
	gboolean retval;

	retval = FALSE;
	window = nautilus_window_slot_get_window (slot);
	if (!NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		retval = nautilus_query_editor_handle_event (slot->details->query_editor, event);
	}

	if (retval) {
		nautilus_window_slot_set_search_visible (slot, TRUE);
	}

	return retval;
}

static void
real_active (NautilusWindowSlot *slot)
{
	NautilusWindow *window;
	int page_num;

	window = slot->details->window;
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->details->notebook),
					  GTK_WIDGET (slot));
	g_assert (page_num >= 0);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (window->details->notebook), page_num);

	/* sync window to new slot */
	nautilus_window_sync_allow_stop (window, slot);
	nautilus_window_sync_title (window, slot);
	nautilus_window_sync_zoom_widgets (window);
	nautilus_window_sync_location_widgets (window);
	nautilus_window_slot_sync_search_widgets (slot);

	if (slot->details->viewed_file != NULL) {
		nautilus_window_slot_sync_view_as_menus (slot);
		nautilus_window_load_extension_menus (window);
	}
}

static void
real_inactive (NautilusWindowSlot *slot)
{
	NautilusWindow *window;

	window = nautilus_window_slot_get_window (slot);
	g_assert (slot == nautilus_window_get_active_slot (window));
}

static void
floating_bar_action_cb (NautilusFloatingBar *floating_bar,
			gint action,
			NautilusWindowSlot *slot)
{
	if (action == NAUTILUS_FLOATING_BAR_ACTION_ID_STOP) {
		nautilus_window_slot_stop_loading (slot);
	}
}

static void
remove_all_extra_location_widgets (GtkWidget *widget,
				   gpointer data)
{
	NautilusWindowSlot *slot = data;
	NautilusDirectory *directory;

	directory = nautilus_directory_get (slot->details->location);
	if (widget != GTK_WIDGET (slot->details->query_editor)) {
		gtk_container_remove (GTK_CONTAINER (slot->details->extra_location_widgets), widget);
	}

	nautilus_directory_unref (directory);
}

static void
nautilus_window_slot_remove_extra_location_widgets (NautilusWindowSlot *slot)
{
	gtk_container_foreach (GTK_CONTAINER (slot->details->extra_location_widgets),
			       remove_all_extra_location_widgets,
			       slot);
}

static void
nautilus_window_slot_add_extra_location_widget (NautilusWindowSlot *slot,
						GtkWidget *widget)
{
	gtk_box_pack_start (GTK_BOX (slot->details->extra_location_widgets),
			    widget, TRUE, TRUE, 0);
	gtk_widget_show (slot->details->extra_location_widgets);
}

static void
nautilus_window_slot_set_property (GObject *object,
				   guint property_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (object);

	switch (property_id) {
	case PROP_WINDOW:
		nautilus_window_slot_set_window (slot, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nautilus_window_slot_get_property (GObject *object,
				   guint property_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (object);

	switch (property_id) {
	case PROP_WINDOW:
		g_value_set_object (value, slot->details->window);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nautilus_window_slot_constructed (GObject *object)
{
	NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (object);
	GtkWidget *extras_vbox;

	G_OBJECT_CLASS (nautilus_window_slot_parent_class)->constructed (object);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (slot),
					GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (GTK_WIDGET (slot));

	extras_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	slot->details->extra_location_widgets = extras_vbox;
	gtk_box_pack_start (GTK_BOX (slot), extras_vbox, FALSE, FALSE, 0);
	gtk_widget_show (extras_vbox);

	slot->details->query_editor = NAUTILUS_QUERY_EDITOR (nautilus_query_editor_new ());
	nautilus_window_slot_add_extra_location_widget (slot, GTK_WIDGET (slot->details->query_editor));
	g_object_add_weak_pointer (G_OBJECT (slot->details->query_editor),
				   (gpointer *) &slot->details->query_editor);

	slot->details->view_overlay = gtk_overlay_new ();
	gtk_widget_add_events (slot->details->view_overlay,
			       GDK_ENTER_NOTIFY_MASK |
			       GDK_LEAVE_NOTIFY_MASK);
	gtk_box_pack_start (GTK_BOX (slot), slot->details->view_overlay, TRUE, TRUE, 0);
	gtk_widget_show (slot->details->view_overlay);

	slot->details->floating_bar = nautilus_floating_bar_new (NULL, NULL, FALSE);
	gtk_widget_set_halign (slot->details->floating_bar, GTK_ALIGN_END);
	gtk_widget_set_valign (slot->details->floating_bar, GTK_ALIGN_END);
	gtk_overlay_add_overlay (GTK_OVERLAY (slot->details->view_overlay),
				 slot->details->floating_bar);

	g_signal_connect (slot->details->floating_bar, "action",
			  G_CALLBACK (floating_bar_action_cb), slot);

	slot->details->title = g_strdup (_("Loading…"));
}

static void
nautilus_window_slot_init (NautilusWindowSlot *slot)
{
	slot->details = G_TYPE_INSTANCE_GET_PRIVATE
		(slot, NAUTILUS_TYPE_WINDOW_SLOT, NautilusWindowSlotDetails);
}

static void
remove_loading_floating_bar (NautilusWindowSlot *slot)
{
	if (slot->details->loading_timeout_id != 0) {
		g_source_remove (slot->details->loading_timeout_id);
		slot->details->loading_timeout_id = 0;
	}

	gtk_widget_hide (slot->details->floating_bar);
	nautilus_floating_bar_cleanup_actions (NAUTILUS_FLOATING_BAR (slot->details->floating_bar));
}

#define DEBUG_FLAG NAUTILUS_DEBUG_WINDOW
#include <libnautilus-private/nautilus-debug.h>

/* FIXME bugzilla.gnome.org 41243:
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "nautilus-desktop-window.h"

/* This number controls a maximum character count for a URL that is
 * displayed as part of a dialog. It's fairly arbitrary -- big enough
 * to allow most "normal" URIs to display in full, but small enough to
 * prevent the dialog from getting insanely wide.
 */
#define MAX_URI_IN_DIALOG_LENGTH 60

static void begin_location_change                     (NautilusWindowSlot         *slot,
						       GFile                      *location,
						       GFile                      *previous_location,
						       GList                      *new_selection,
						       NautilusLocationChangeType  type,
						       guint                       distance,
						       const char                 *scroll_pos,
						       NautilusWindowGoToCallback  callback,
						       gpointer                    user_data);
static void free_location_change                      (NautilusWindowSlot         *slot);
static void end_location_change                       (NautilusWindowSlot         *slot);
static void cancel_location_change                    (NautilusWindowSlot         *slot);
static void got_file_info_for_view_selection_callback (NautilusFile               *file,
						       gpointer                    callback_data);
static gboolean create_content_view                   (NautilusWindowSlot         *slot,
						       const char                 *view_id,
						       GError                    **error);
static void display_view_selection_failure            (NautilusWindow             *window,
						       NautilusFile               *file,
						       GFile                      *location,
						       GError                     *error);
static void load_new_location                         (NautilusWindowSlot         *slot,
						       GFile                      *location,
						       GList                      *selection,
						       gboolean                    tell_current_content_view,
						       gboolean                    tell_new_content_view);

static void
new_window_show_callback (GtkWidget *widget,
			  gpointer user_data){
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (user_data);
	nautilus_window_close (window);

	g_signal_handlers_disconnect_by_func (widget,
					      G_CALLBACK (new_window_show_callback),
					      user_data);
}

void
nautilus_window_slot_open_location_full (NautilusWindowSlot *slot,
					 GFile *location,
					 NautilusWindowOpenFlags flags,
					 GList *new_selection,
					 NautilusWindowGoToCallback callback,
					 gpointer user_data)
{
	NautilusWindow *window;
        NautilusWindow *target_window;
        NautilusWindowSlot *target_slot;
	NautilusWindowOpenFlags slot_flags;
	GFile *old_location;
	GList *old_selection;
	char *old_uri, *new_uri;
	int new_slot_position;
	gboolean use_same;
	gboolean is_desktop;

	window = nautilus_window_slot_get_window (slot);

        target_window = NULL;
	target_slot = NULL;
	use_same = TRUE;

	/* this happens at startup */
	old_uri = nautilus_window_slot_get_location_uri (slot);
	if (old_uri == NULL) {
		old_uri = g_strdup ("(none)");
		use_same = TRUE;
	}
	new_uri = g_file_get_uri (location);

	DEBUG ("Opening location, old: %s, new: %s", old_uri, new_uri);
	nautilus_profile_start ("Opening location, old: %s, new: %s", old_uri, new_uri);

	g_free (old_uri);
	g_free (new_uri);

	is_desktop = NAUTILUS_IS_DESKTOP_WINDOW (window);

	if (is_desktop) {
		use_same = !nautilus_desktop_window_loaded (NAUTILUS_DESKTOP_WINDOW (window));

		/* if we're requested to open a new tab on the desktop, open a window
		 * instead.
		 */
		if (flags & NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB) {
			flags ^= NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB;
			flags |= NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW;
		}
	}

	g_assert (!((flags & NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW) != 0 &&
		    (flags & NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB) != 0));

	/* and if the flags specify so, this is overridden */
	if ((flags & NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW) != 0) {
		use_same = FALSE;
	}

	old_location = nautilus_window_slot_get_location (slot);

	/* now get/create the window */
	if (use_same) {
		target_window = window;
	} else {
		target_window = nautilus_application_create_window
			(NAUTILUS_APPLICATION (g_application_get_default ()),
			 gtk_window_get_screen (GTK_WINDOW (window)));
	}

        g_assert (target_window != NULL);

	/* if the flags say we want a new tab, open a slot in the current window */
	if ((flags & NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB) != 0) {
		g_assert (target_window == window);

		slot_flags = 0;

		new_slot_position = g_settings_get_enum (nautilus_preferences, NAUTILUS_PREFERENCES_NEW_TAB_POSITION);
		if (new_slot_position == NAUTILUS_NEW_TAB_POSITION_END) {
			slot_flags = NAUTILUS_WINDOW_OPEN_SLOT_APPEND;
		}

		target_slot = nautilus_window_open_slot (window,
							 slot_flags);
	}

	/* close the current window if the flags say so */
	if ((flags & NAUTILUS_WINDOW_OPEN_FLAG_CLOSE_BEHIND) != 0) {
		if (!is_desktop) {
			if (gtk_widget_get_visible (GTK_WIDGET (target_window))) {
				nautilus_window_close (window);
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
			target_slot = nautilus_window_get_active_slot (target_window);
		}
	}

	old_selection = NULL;
	if (slot->details->content_view != NULL) {
		old_selection = nautilus_view_get_selection (slot->details->content_view);
	}

	if (target_window == window && target_slot == slot && !is_desktop &&
	    old_location && g_file_equal (old_location, location) &&
	    nautilus_file_selection_equal (old_selection, new_selection)) {

		if (callback != NULL) {
			callback (window, location, NULL, user_data);
		}

		goto done;
	}

	slot->details->pending_use_default_location = ((flags & NAUTILUS_WINDOW_OPEN_FLAG_USE_DEFAULT_LOCATION) != 0);
	begin_location_change (target_slot, location, old_location, new_selection,
			       NAUTILUS_LOCATION_CHANGE_STANDARD, 0, NULL, callback, user_data);

 done:
	nautilus_file_list_free (old_selection);
	nautilus_profile_end (NULL);
}

static gboolean
report_callback (NautilusWindowSlot *slot,
		 GError *error)
{
	if (slot->details->open_callback != NULL) {
		gboolean res;
		res = slot->details->open_callback (nautilus_window_slot_get_window (slot),
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
 * @window: The NautilusWindow whose location should be changed.
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
begin_location_change (NautilusWindowSlot *slot,
                       GFile *location,
                       GFile *previous_location,
		       GList *new_selection,
                       NautilusLocationChangeType type,
                       guint distance,
                       const char *scroll_pos,
		       NautilusWindowGoToCallback callback,
		       gpointer user_data)
{
        NautilusDirectory *directory;
        NautilusFile *file;
	gboolean force_reload;
        char *current_pos;
	GFile *from_folder, *parent;
	GList *parent_selection = NULL;

	g_assert (slot != NULL);
        g_assert (location != NULL);
        g_assert (type == NAUTILUS_LOCATION_CHANGE_BACK
                  || type == NAUTILUS_LOCATION_CHANGE_FORWARD
                  || distance == 0);

	nautilus_profile_start (NULL);

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
				g_list_prepend (NULL, nautilus_file_get (from_folder));
			g_object_unref (parent);
		}

		g_object_unref (from_folder);
	}

	end_location_change (slot);

	nautilus_window_slot_set_allow_stop (slot, TRUE);
	nautilus_window_slot_set_status (slot, NULL, NULL);

	g_assert (slot->details->pending_location == NULL);
	g_assert (slot->details->pending_selection == NULL);

	slot->details->pending_location = g_object_ref (location);
	slot->details->location_change_type = type;
	slot->details->location_change_distance = distance;
	slot->details->tried_mount = FALSE;
	slot->details->pending_selection = g_list_copy_deep (new_selection, (GCopyFunc) g_object_ref, NULL);

	slot->details->pending_scroll_to = g_strdup (scroll_pos);

	slot->details->open_callback = callback;
	slot->details->open_callback_user_data = user_data;

        directory = nautilus_directory_get (location);

	/* The code to force a reload is here because if we do it
	 * after determining an initial view (in the components), then
	 * we end up fetching things twice.
	 */
	if (type == NAUTILUS_LOCATION_CHANGE_RELOAD) {
		force_reload = TRUE;
	} else if (!nautilus_monitor_active ()) {
		force_reload = TRUE;
	} else {
		force_reload = !nautilus_directory_is_local (directory);
	}

	if (force_reload) {
		nautilus_directory_force_reload (directory);
		file = nautilus_directory_get_corresponding_file (directory);
		nautilus_file_invalidate_all_attributes (file);
		nautilus_file_unref (file);
	}

        nautilus_directory_unref (directory);

	if (parent_selection != NULL) {
		g_list_free_full (parent_selection, g_object_unref);
	}

        /* Set current_bookmark scroll pos */
        if (slot->details->current_location_bookmark != NULL &&
            slot->details->content_view != NULL) {
                current_pos = nautilus_view_get_first_visible_file (slot->details->content_view);
                nautilus_bookmark_set_scroll_pos (slot->details->current_location_bookmark, current_pos);
                g_free (current_pos);
        }

	/* Get the info needed for view selection */
	slot->details->determine_view_file = nautilus_file_get (location);
	g_assert (slot->details->determine_view_file != NULL);

	nautilus_file_call_when_ready (slot->details->determine_view_file,
				       NAUTILUS_FILE_ATTRIBUTE_INFO |
				       NAUTILUS_FILE_ATTRIBUTE_MOUNT,
                                       got_file_info_for_view_selection_callback,
				       slot);

	nautilus_profile_end (NULL);
}

static void
nautilus_window_slot_set_location (NautilusWindowSlot *slot,
				   GFile *location)
{
	GFile *old_location;

	if (slot->details->location &&
	    g_file_equal (location, slot->details->location)) {
		return;
	}

	old_location = slot->details->location;
	slot->details->location = g_object_ref (location);

	if (slot == nautilus_window_get_active_slot (slot->details->window)) {
		nautilus_window_sync_location_widgets (slot->details->window);
		nautilus_window_slot_update_title (slot);
	}

	nautilus_window_slot_emit_location_change (slot, old_location, location);

	if (old_location) {
		g_object_unref (old_location);
	}
}

static void
viewed_file_changed_callback (NautilusFile *file,
                              NautilusWindowSlot *slot)
{
        GFile *new_location;
	gboolean is_in_trash, was_in_trash;

        g_assert (NAUTILUS_IS_FILE (file));
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));
	g_assert (file == slot->details->viewed_file);

        if (!nautilus_file_is_not_yet_confirmed (file)) {
                slot->details->viewed_file_seen = TRUE;
        }

	was_in_trash = slot->details->viewed_file_in_trash;

	slot->details->viewed_file_in_trash = is_in_trash = nautilus_file_is_in_trash (file);

	if (nautilus_file_is_gone (file) || (is_in_trash && !was_in_trash)) {
                if (slot->details->viewed_file_seen) {
			GFile *go_to_file;
			GFile *parent;
			GFile *location;
			GMount *mount;

			parent = NULL;
			location = nautilus_file_get_location (file);

			if (g_file_is_native (location)) {
				mount = nautilus_get_mounted_mount_for_root (location);

				if (mount == NULL) {
					parent = g_file_get_parent (location);
				}

				g_clear_object (&mount);
			}

			if (parent != NULL) {
				/* auto-show existing parent */
				go_to_file = nautilus_find_existing_uri_in_hierarchy (parent);
			} else {
				go_to_file = g_file_new_for_path (g_get_home_dir ());
			}

			nautilus_window_slot_open_location (slot, go_to_file, 0);

			g_clear_object (&parent);
			g_object_unref (go_to_file);
			g_object_unref (location);
                }
	} else {
                new_location = nautilus_file_get_location (file);
		nautilus_window_slot_set_location (slot, new_location);
		g_object_unref (new_location);
        }
}

static void
nautilus_window_slot_set_viewed_file (NautilusWindowSlot *slot,
				      NautilusFile *file)
{
	NautilusFileAttributes attributes;

	if (slot->details->viewed_file == file) {
		return;
	}

	nautilus_file_ref (file);

	if (slot->details->viewed_file != NULL) {
		g_signal_handlers_disconnect_by_func (slot->details->viewed_file,
						      G_CALLBACK (viewed_file_changed_callback),
						      slot);
		nautilus_file_monitor_remove (slot->details->viewed_file,
					      slot);
	}

	if (file != NULL) {
		attributes =
			NAUTILUS_FILE_ATTRIBUTE_INFO |
			NAUTILUS_FILE_ATTRIBUTE_LINK_INFO;
		nautilus_file_monitor_add (file, slot, attributes);

		g_signal_connect_object (file, "changed",
					 G_CALLBACK (viewed_file_changed_callback), slot, 0);
	}

	nautilus_file_unref (slot->details->viewed_file);
	slot->details->viewed_file = file;
}

typedef struct {
	GCancellable *cancellable;
	NautilusWindowSlot *slot;
} MountNotMountedData;

static void
mount_not_mounted_callback (GObject *source_object,
			    GAsyncResult *res,
			    gpointer user_data)
{
	MountNotMountedData *data;
	NautilusWindowSlot *slot;
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

	slot->details->determine_view_file = nautilus_file_get (slot->details->pending_location);

	error = NULL;
	if (!g_file_mount_enclosing_volume_finish (G_FILE (source_object), res, &error)) {
		slot->details->mount_error = error;
		got_file_info_for_view_selection_callback (slot->details->determine_view_file, slot);
		slot->details->mount_error = NULL;
		g_error_free (error);
	} else {
		nautilus_file_invalidate_all_attributes (slot->details->determine_view_file);
		nautilus_file_call_when_ready (slot->details->determine_view_file,
					       NAUTILUS_FILE_ATTRIBUTE_INFO |
					       NAUTILUS_FILE_ATTRIBUTE_MOUNT,
					       got_file_info_for_view_selection_callback,
					       slot);
	}

	g_object_unref (cancellable);
}

static void
got_file_info_for_view_selection_callback (NautilusFile *file,
					   gpointer callback_data)
{
        GError *error = NULL;
	char *view_id;
	char *mimetype;
	NautilusWindow *window;
	NautilusWindowSlot *slot;
	NautilusFile *viewed_file, *parent_file;
	GFile *location, *default_location;
	GMountOperation *mount_op;
	MountNotMountedData *data;
	GtkApplication *app;
	GMount *mount;

	slot = callback_data;
	window = nautilus_window_slot_get_window (slot);

	g_assert (slot->details->determine_view_file == file);
	slot->details->determine_view_file = NULL;

	nautilus_profile_start (NULL);

	if (slot->details->mount_error) {
		error = g_error_copy (slot->details->mount_error);
	} else if (nautilus_file_get_file_info_error (file) != NULL) {
		error = g_error_copy (nautilus_file_get_file_info_error (file));
	}

	if (error && error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_MOUNTED &&
	    !slot->details->tried_mount) {
		slot->details->tried_mount = TRUE;

		mount_op = gtk_mount_operation_new (GTK_WINDOW (window));
		g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
		location = nautilus_file_get_location (file);
		data = g_new0 (MountNotMountedData, 1);
		data->cancellable = g_cancellable_new ();
		data->slot = slot;
		slot->details->mount_cancellable = data->cancellable;
		g_file_mount_enclosing_volume (location, 0, mount_op, slot->details->mount_cancellable,
					       mount_not_mounted_callback, data);
		g_object_unref (location);
		g_object_unref (mount_op);

		goto done;
	}

	mount = NULL;
	default_location = NULL;

	if (slot->details->pending_use_default_location) {
		mount = nautilus_file_get_mount (file);
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
		slot->details->determine_view_file = nautilus_file_get (default_location);

		nautilus_file_invalidate_all_attributes (slot->details->determine_view_file);
		nautilus_file_call_when_ready (slot->details->determine_view_file,
					       NAUTILUS_FILE_ATTRIBUTE_INFO |
					       NAUTILUS_FILE_ATTRIBUTE_MOUNT,
					       got_file_info_for_view_selection_callback,
					       slot);

		goto done;
	}

	parent_file = nautilus_file_get_parent (file);
	if ((parent_file != NULL) &&
	    nautilus_file_get_file_type (file) == G_FILE_TYPE_REGULAR) {
		if (slot->details->pending_selection != NULL) {
			g_list_free_full (slot->details->pending_selection, (GDestroyNotify) nautilus_file_unref);
		}

		g_clear_object (&slot->details->pending_location);
		g_free (slot->details->pending_scroll_to);

		slot->details->pending_location = nautilus_file_get_parent_location (file);
		slot->details->pending_selection = g_list_prepend (NULL, nautilus_file_ref (file));
		slot->details->determine_view_file = parent_file;
		slot->details->pending_scroll_to = nautilus_file_get_uri (file);

		nautilus_file_invalidate_all_attributes (slot->details->determine_view_file);
		nautilus_file_call_when_ready (slot->details->determine_view_file,
					       NAUTILUS_FILE_ATTRIBUTE_INFO |
					       NAUTILUS_FILE_ATTRIBUTE_MOUNT,
					       got_file_info_for_view_selection_callback,
					       slot);

		goto done;
	}

	nautilus_file_unref (parent_file);
	location = slot->details->pending_location;

	view_id = NULL;

        if (error == NULL ||
	    (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_SUPPORTED)) {
		/* We got the information we need, now pick what view to use: */

		mimetype = nautilus_file_get_mime_type (file);

		/* Otherwise, use default */
		if (slot->details->content_view != NULL) {
			view_id = g_strdup (nautilus_view_get_view_id (slot->details->content_view));
		}

		if (view_id == NULL) {
			view_id = nautilus_global_preferences_get_default_folder_viewer_preference_as_iid ();

			if (view_id != NULL &&
			    !nautilus_view_factory_view_supports_uri (view_id,
								      location,
								      nautilus_file_get_file_type (file),
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

				if (!nautilus_is_root_directory (location)) {
					if (!nautilus_is_home_directory (location)) {
						nautilus_window_slot_go_home (slot, FALSE);
					} else {
						GFile *root;

						root = g_file_new_for_path ("/");
						/* the last fallback is to go to a known place that can't be deleted! */
						nautilus_window_slot_open_location (slot, location, 0);
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
			slot_location = nautilus_window_slot_get_location (slot);

			/* We're missing a previous location (if opened location
			 * in a new tab) so close it and return */
			if (slot_location == NULL) {
				nautilus_window_slot_close (window, slot);
			} else {
				/* We disconnected this, so we need to re-connect it */
				viewed_file = nautilus_file_get (slot_location);
				nautilus_window_slot_set_viewed_file (slot, viewed_file);
				nautilus_file_unref (viewed_file);

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

	nautilus_file_unref (file);

	nautilus_profile_end (NULL);
}

/* Load a view into the window, either reusing the old one or creating
 * a new one. This happens when you want to load a new location, or just
 * switch to a different view.
 * If pending_location is set we're loading a new location and
 * pending_location/selection will be used. If not, we're just switching
 * view, and the current location will be used.
 */
static gboolean
create_content_view (NautilusWindowSlot *slot,
		     const char *view_id,
		     GError **error_out)
{
	NautilusWindow *window;
        NautilusView *view;
	GList *selection;
	gboolean ret = TRUE;
	GError *error = NULL;
	NautilusDirectory *old_directory, *new_directory;
	GFile *old_location;

	window = nautilus_window_slot_get_window (slot);

	nautilus_profile_start (NULL);

	/* FIXME bugzilla.gnome.org 41243:
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
	if (NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		/* We force the desktop to use a desktop_icon_view. It's simpler
		 * to fix it here than trying to make it pick the right view in
		 * the first place.
		 */
		view_id = NAUTILUS_DESKTOP_ICON_VIEW_IID;
	}

        if (slot->details->content_view != NULL &&
	    g_strcmp0 (nautilus_view_get_view_id (slot->details->content_view),
			view_id) == 0) {
                /* reuse existing content view */
                view = slot->details->content_view;
                slot->details->new_content_view = view;
		g_object_ref (view);
        } else {
                /* create a new content view */
		view = nautilus_view_factory_create (view_id, slot);

                slot->details->new_content_view = view;
		nautilus_window_slot_connect_new_content_view (slot);
        }

	/* Forward search selection and state before loading the new model */
	old_location = nautilus_window_slot_get_location (slot);
	old_directory = nautilus_directory_get (old_location);
	new_directory = nautilus_directory_get (slot->details->pending_location);

	if (NAUTILUS_IS_SEARCH_DIRECTORY (new_directory) &&
	    !NAUTILUS_IS_SEARCH_DIRECTORY (old_directory)) {
		nautilus_search_directory_set_base_model (NAUTILUS_SEARCH_DIRECTORY (new_directory), old_directory);
	}

	if (NAUTILUS_IS_SEARCH_DIRECTORY (old_directory) &&
	    !NAUTILUS_IS_SEARCH_DIRECTORY (new_directory)) {
		/* Reset the search_active state when going out of a search directory,
		 * before nautilus_window_slot_sync_search_widgets() is called
		 * if we're not being loaded with search visible.
		 */
		if (!slot->details->load_with_search) {
			slot->details->search_visible = FALSE;
		}

		slot->details->load_with_search = FALSE;

		if (slot->details->pending_selection == NULL) {
			slot->details->pending_selection = nautilus_view_get_selection (slot->details->content_view);
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
		selection = nautilus_view_get_selection (slot->details->content_view);
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

	nautilus_profile_end (NULL);

	return ret;
}

static void
load_new_location (NautilusWindowSlot *slot,
		   GFile *location,
		   GList *selection,
		   gboolean tell_current_content_view,
		   gboolean tell_new_content_view)
{
	GList *selection_copy;
	NautilusView *view;

	g_assert (slot != NULL);
	g_assert (location != NULL);

	selection_copy = g_list_copy_deep (selection, (GCopyFunc) g_object_ref, NULL);
	view = NULL;

	nautilus_profile_start (NULL);
	/* Note, these may recurse into report_load_underway */
        if (slot->details->content_view != NULL && tell_current_content_view) {
		view = slot->details->content_view;
		nautilus_view_load_location (slot->details->content_view, location);
        }

        if (slot->details->new_content_view != NULL && tell_new_content_view &&
	    (!tell_current_content_view ||
	     slot->details->new_content_view != slot->details->content_view) ) {
		view = slot->details->new_content_view;
		nautilus_view_load_location (slot->details->new_content_view, location);
        }
	if (view != NULL) {
		/* new_content_view might have changed here if
		   report_load_underway was called from load_location */
		nautilus_view_set_selection (view, selection_copy);
	}

	g_list_free_full (selection_copy, g_object_unref);

	nautilus_profile_end (NULL);
}

static void
end_location_change (NautilusWindowSlot *slot)
{
	char *uri;

	uri = nautilus_window_slot_get_location_uri (slot);
	if (uri) {
		DEBUG ("Finished loading window for uri %s", uri);
		g_free (uri);
	}

	nautilus_window_slot_set_allow_stop (slot, FALSE);

        /* Now we can free details->pending_scroll_to, since the load_complete
         * callback already has been emitted.
         */
	g_free (slot->details->pending_scroll_to);
	slot->details->pending_scroll_to = NULL;

	free_location_change (slot);
}

static void
free_location_change (NautilusWindowSlot *slot)
{
	NautilusWindow *window;

	window = nautilus_window_slot_get_window (slot);

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
		nautilus_file_cancel_call_when_ready
			(slot->details->determine_view_file,
			 got_file_info_for_view_selection_callback, slot);
                slot->details->determine_view_file = NULL;
        }

        if (slot->details->new_content_view != NULL) {
		slot->details->temporarily_ignore_view_signals = TRUE;
		nautilus_view_stop_loading (slot->details->new_content_view);
		slot->details->temporarily_ignore_view_signals = FALSE;

		nautilus_window_disconnect_content_view (window, slot->details->new_content_view);
		g_object_unref (slot->details->new_content_view);
		slot->details->new_content_view = NULL;
        }
}

static void
cancel_location_change (NautilusWindowSlot *slot)
{
	GList *selection;
	GFile *location;

	location = nautilus_window_slot_get_location (slot);

        if (slot->details->pending_location != NULL
            && location != NULL
            && slot->details->content_view != NULL) {

                /* No need to tell the new view - either it is the
                 * same as the old view, in which case it will already
                 * be told, or it is the very pending change we wish
                 * to cancel.
                 */
		selection = nautilus_view_get_selection (slot->details->content_view);
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
display_view_selection_failure (NautilusWindow *window, NautilusFile *file,
				GFile *location, GError *error)
{
	char *error_message;
	char *detail_message;
	char *scheme_string;

	/* Some sort of failure occurred. How 'bout we tell the user? */

	error_message = g_strdup (_("Oops! Something went wrong."));
	detail_message = NULL;
	if (error == NULL) {
		if (nautilus_file_is_directory (file)) {
			detail_message = g_strdup (_("Unable to display the contents of this folder."));
		} else {
			detail_message = g_strdup (_("This location doesn't appear to be a folder."));
		}
	} else if (error->domain == G_IO_ERROR) {
		switch (error->code) {
		case G_IO_ERROR_NOT_FOUND:
			error_message = g_strdup (_("Unable to find the requested file. Please check the spelling and try again."));
			break;
		case G_IO_ERROR_NOT_SUPPORTED:
			scheme_string = g_file_get_uri_scheme (location);
			if (scheme_string != NULL) {
				detail_message = g_strdup_printf (_("“%s” locations are not supported."),
								  scheme_string);
			} else {
				detail_message = g_strdup (_("Unable to handle this kind of location."));
			}
			g_free (scheme_string);
			break;
		case G_IO_ERROR_NOT_MOUNTED:
			detail_message = g_strdup (_("Unable to access the requested location."));
			break;
		case G_IO_ERROR_PERMISSION_DENIED:
			detail_message = g_strdup (_("Don't have permission to access the requested location."));
			break;
		case G_IO_ERROR_HOST_NOT_FOUND:
			/* This case can be hit for user-typed strings like "foo" due to
			 * the code that guesses web addresses when there's no initial "/".
			 * But this case is also hit for legitimate web addresses when
			 * the proxy is set up wrong.
			 */
			detail_message = g_strdup (_("Unable to find the requested location. Please check the spelling or the network settings."));
			break;
		case G_IO_ERROR_CANCELLED:
		case G_IO_ERROR_FAILED_HANDLED:
			goto done;
		default:
			break;
		}
	}

	if (detail_message == NULL) {
		detail_message = g_strdup_printf (_("Unhandled error message: %s"), error->message);
	}

	eel_show_error_dialog (error_message, detail_message, GTK_WINDOW (window));
 done:
	g_free (error_message);
	g_free (detail_message);
}

void
nautilus_window_slot_set_content_view (NautilusWindowSlot *slot,
				       const char *id)
{
	char *uri;

	g_assert (slot != NULL);
	g_assert (id != NULL);

	uri = nautilus_window_slot_get_location_uri (slot);
	DEBUG ("Change view of window %s to %s", uri, id);
	g_free (uri);

	if (nautilus_window_slot_content_view_matches_iid (slot, id)) {
		return;
        }

        end_location_change (slot);

        nautilus_window_slot_set_allow_stop (slot, TRUE);

        if (nautilus_view_get_selection_count (slot->details->content_view) == 0) {
                /* If there is no selection, queue a scroll to the same icon that
                 * is currently visible */
                slot->details->pending_scroll_to = nautilus_view_get_first_visible_file (slot->details->content_view);
        }
	slot->details->location_change_type = NAUTILUS_LOCATION_CHANGE_RELOAD;

        if (!create_content_view (slot, id, NULL)) {
		/* Just load the homedir. */
		nautilus_window_slot_go_home (slot, FALSE);
	}
}

void
nautilus_window_back_or_forward (NautilusWindow *window,
				 gboolean back,
				 guint distance,
				 NautilusWindowOpenFlags flags)
{
	NautilusWindowSlot *slot;
	GList *list;
	GFile *location;
	guint len;
	NautilusBookmark *bookmark;
	GFile *old_location;

	slot = nautilus_window_get_active_slot (window);
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
	location = nautilus_bookmark_get_location (bookmark);

	if (flags != 0) {
		nautilus_window_slot_open_location (slot, location, flags);
	} else {
		char *scroll_pos;

		old_location = nautilus_window_slot_get_location (slot);
		scroll_pos = nautilus_bookmark_get_scroll_pos (bookmark);
		begin_location_change
			(slot,
			 location, old_location, NULL,
			 back ? NAUTILUS_LOCATION_CHANGE_BACK : NAUTILUS_LOCATION_CHANGE_FORWARD,
			 distance,
			 scroll_pos,
			 NULL, NULL);

		g_free (scroll_pos);
	}

	g_object_unref (location);
}

/* reload the contents of the window */
static void
nautilus_window_slot_force_reload (NautilusWindowSlot *slot)
{
	GFile *location;
        char *current_pos;
	GList *selection;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	location = nautilus_window_slot_get_location (slot);
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
		current_pos = nautilus_view_get_first_visible_file (slot->details->content_view);
		selection = nautilus_view_get_selection (slot->details->content_view);
	}
	begin_location_change
		(slot, location, location, selection,
		 NAUTILUS_LOCATION_CHANGE_RELOAD, 0, current_pos,
		 NULL, NULL);
        g_free (current_pos);
	g_object_unref (location);
	g_list_free_full (selection, g_object_unref);
}

void
nautilus_window_slot_queue_reload (NautilusWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	if (nautilus_window_slot_get_location (slot) == NULL) {
		return;
	}

	if (slot->details->pending_location != NULL
	    || slot->details->content_view == NULL
	    || nautilus_view_get_loading (slot->details->content_view)) {
		/* there is a reload in flight */
		slot->details->needs_reload = TRUE;
		return;
	}

	nautilus_window_slot_force_reload (slot);
}

static void
nautilus_window_slot_clear_forward_list (NautilusWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	g_list_free_full (slot->details->forward_list, g_object_unref);
	slot->details->forward_list = NULL;
}

static void
nautilus_window_slot_clear_back_list (NautilusWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	g_list_free_full (slot->details->back_list, g_object_unref);
	slot->details->back_list = NULL;
}

static void
nautilus_window_slot_update_bookmark (NautilusWindowSlot *slot, NautilusFile *file)
{
        gboolean recreate;
	GFile *new_location;

	new_location = nautilus_file_get_location (file);

	if (slot->details->current_location_bookmark == NULL) {
		recreate = TRUE;
	} else {
		GFile *bookmark_location;
		bookmark_location = nautilus_bookmark_get_location (slot->details->current_location_bookmark);
		recreate = !g_file_equal (bookmark_location, new_location);
		g_object_unref (bookmark_location);
        }

	if (recreate) {
		char *display_name = NULL;

		/* We've changed locations, must recreate bookmark for current location. */
		g_clear_object (&slot->details->last_location_bookmark);
		slot->details->last_location_bookmark = slot->details->current_location_bookmark;

		display_name = nautilus_file_get_display_name (file);
		slot->details->current_location_bookmark = nautilus_bookmark_new (new_location, display_name);
		g_free (display_name);
        }

	g_object_unref (new_location);
}

static void
check_bookmark_location_matches (NautilusBookmark *bookmark, GFile *location)
{
        GFile *bookmark_location;
        char *bookmark_uri, *uri;

	bookmark_location = nautilus_bookmark_get_location (bookmark);
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
check_last_bookmark_location_matches_slot (NautilusWindowSlot *slot)
{
	check_bookmark_location_matches (slot->details->last_location_bookmark,
					 nautilus_window_slot_get_location (slot));
}

static void
handle_go_direction (NautilusWindowSlot *slot,
		     GFile              *location,
		     gboolean            forward)
{
	GList **list_ptr, **other_list_ptr;
	GList *list, *other_list, *link;
	NautilusBookmark *bookmark;
	gint i;

	list_ptr = (forward) ? (&slot->details->forward_list) : (&slot->details->back_list);
	other_list_ptr = (forward) ? (&slot->details->back_list) : (&slot->details->forward_list);
	list = *list_ptr;
	other_list = *other_list_ptr;

	/* Move items from the list to the other list. */
	g_assert (g_list_length (list) > slot->details->location_change_distance);
	check_bookmark_location_matches (g_list_nth_data (list, slot->details->location_change_distance),
					 location);
	g_assert (nautilus_window_slot_get_location (slot) != NULL);

	/* Move current location to list */
	check_last_bookmark_location_matches_slot (slot);

	/* Use the first bookmark in the history list rather than creating a new one. */
	other_list = g_list_prepend (other_list, slot->details->last_location_bookmark);
	g_object_ref (other_list->data);

	/* Move extra links from the list to the other list */
	for (i = 0; i < slot->details->location_change_distance; ++i) {
		bookmark = NAUTILUS_BOOKMARK (list->data);
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
handle_go_elsewhere (NautilusWindowSlot *slot,
		     GFile *location)
{
	GFile *slot_location;

	/* Clobber the entire forward list, and move displayed location to back list */
	nautilus_window_slot_clear_forward_list (slot);
	slot_location = nautilus_window_slot_get_location (slot);

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
update_history (NautilusWindowSlot *slot,
                NautilusLocationChangeType type,
                GFile *new_location)
{
        switch (type) {
        case NAUTILUS_LOCATION_CHANGE_STANDARD:
		handle_go_elsewhere (slot, new_location);
                return;
        case NAUTILUS_LOCATION_CHANGE_RELOAD:
                /* for reload there is no work to do */
                return;
        case NAUTILUS_LOCATION_CHANGE_BACK:
                handle_go_direction (slot, new_location, FALSE);
                return;
        case NAUTILUS_LOCATION_CHANGE_FORWARD:
                handle_go_direction (slot, new_location, TRUE);
                return;
        }
	g_return_if_fail (FALSE);
}

typedef struct {
	NautilusWindowSlot *slot;
	GCancellable *cancellable;
	GMount *mount;
} FindMountData;

static void
nautilus_window_slot_show_x_content_bar (NautilusWindowSlot *slot, GMount *mount, const char **x_content_types)
{
	GtkWidget *bar;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	bar = nautilus_x_content_bar_new (mount, x_content_types);
	gtk_widget_show (bar);
	nautilus_window_slot_add_extra_location_widget (slot, bar);
}

static void
found_content_type_cb (const char **x_content_types,
		       gpointer user_data)
{
	NautilusWindowSlot *slot;
	FindMountData *data = user_data;

	if (g_cancellable_is_cancelled (data->cancellable)) {
		goto out;
	}

	slot = data->slot;

	if (x_content_types != NULL && x_content_types[0] != NULL) {
		nautilus_window_slot_show_x_content_bar (slot, data->mount, x_content_types);
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
		nautilus_get_x_content_types_for_mount_async (mount,
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
nautilus_window_slot_emit_location_change (NautilusWindowSlot *slot,
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
nautilus_window_slot_show_trash_bar (NautilusWindowSlot *slot)
{
	GtkWidget *bar;
	NautilusView *view;

	view = nautilus_window_slot_get_current_view (slot);
	bar = nautilus_trash_bar_new (view);
	gtk_widget_show (bar);

	nautilus_window_slot_add_extra_location_widget (slot, bar);
}

static void
nautilus_window_slot_show_special_location_bar (NautilusWindowSlot     *slot,
						NautilusSpecialLocation special_location)
{
	GtkWidget *bar;

	bar = nautilus_special_location_bar_new (special_location);
	gtk_widget_show (bar);

	nautilus_window_slot_add_extra_location_widget (slot, bar);
}

static void
slot_add_extension_extra_widgets (NautilusWindowSlot *slot)
{
	GList *providers, *l;
	GtkWidget *widget;
	char *uri;
	NautilusWindow *window;

	providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_LOCATION_WIDGET_PROVIDER);
	window = nautilus_window_slot_get_window (slot);

	uri = nautilus_window_slot_get_location_uri (slot);
	for (l = providers; l != NULL; l = l->next) {
		NautilusLocationWidgetProvider *provider;

		provider = NAUTILUS_LOCATION_WIDGET_PROVIDER (l->data);
		widget = nautilus_location_widget_provider_get_widget (provider, uri, GTK_WIDGET (window));
		if (widget != NULL) {
			nautilus_window_slot_add_extra_location_widget (slot, widget);
		}
	}
	g_free (uri);

	nautilus_module_extension_list_free (providers);
}

static void
nautilus_window_slot_update_for_new_location (NautilusWindowSlot *slot)
{
	NautilusWindow *window;
        GFile *new_location, *old_location;
        NautilusFile *file;
	NautilusDirectory *directory;
	gboolean location_really_changed;
	FindMountData *data;

	window = nautilus_window_slot_get_window (slot);
	new_location = slot->details->pending_location;
	slot->details->pending_location = NULL;

	file = nautilus_file_get (new_location);
	nautilus_window_slot_update_bookmark (slot, file);

	update_history (slot, slot->details->location_change_type, new_location);
	old_location = nautilus_window_slot_get_location (slot);

	location_really_changed =
		old_location == NULL ||
		!g_file_equal (old_location, new_location);

        /* Create a NautilusFile for this location, so we can catch it
         * if it goes away.
         */
	nautilus_window_slot_set_viewed_file (slot, file);
	slot->details->viewed_file_seen = !nautilus_file_is_not_yet_confirmed (file);
	slot->details->viewed_file_in_trash = nautilus_file_is_in_trash (file);
        nautilus_file_unref (file);

	nautilus_window_slot_set_location (slot, new_location);

	if (slot == nautilus_window_get_active_slot (window)) {
		/* Sync up and zoom action states */
		nautilus_window_sync_up_button (window);
		nautilus_window_sync_zoom_widgets (window);

		/* Sync the content view menu for this new location. */
		nautilus_window_slot_sync_view_as_menus (slot);

		/* Load menus from nautilus extensions for this location */
		nautilus_window_load_extension_menus (window);
	}

	if (location_really_changed) {
		nautilus_window_slot_remove_extra_location_widgets (slot);

		directory = nautilus_directory_get (new_location);

		if (nautilus_directory_is_in_trash (directory)) {
			nautilus_window_slot_show_trash_bar (slot);
		} else {
			GFile *scripts_file;
			char *scripts_path = nautilus_get_scripts_directory_path ();
			scripts_file = g_file_new_for_path (scripts_path);
			g_free (scripts_path);
			if (nautilus_should_use_templates_directory () &&
			    nautilus_file_is_user_special_directory (file, G_USER_DIRECTORY_TEMPLATES)) {
				nautilus_window_slot_show_special_location_bar (slot, NAUTILUS_SPECIAL_LOCATION_TEMPLATES);
			} else if (g_file_equal (new_location, scripts_file)) {
				nautilus_window_slot_show_special_location_bar (slot, NAUTILUS_SPECIAL_LOCATION_SCRIPTS);
			}
			g_object_unref (scripts_file);
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

		nautilus_directory_unref (directory);

		slot_add_extension_extra_widgets (slot);
	}

	if (slot == nautilus_window_get_active_slot (window) &&
	    location_really_changed) {
		nautilus_window_slot_sync_search_widgets (slot);
	}
}

static void
view_end_loading_cb (NautilusView       *view,
		     gboolean            all_files_seen,
		     NautilusWindowSlot *slot)
{
	if (slot->details->temporarily_ignore_view_signals) {
		return;
	}

	/* Only handle this if we're expecting it.
	 * Don't handle it if its from an old view we've switched from */
	if (view == slot->details->content_view && all_files_seen) {
		if (slot->details->pending_scroll_to != NULL) {
			nautilus_view_scroll_to_file (slot->details->content_view,
						      slot->details->pending_scroll_to);
		}
		end_location_change (slot);
	}

	if (slot->details->needs_reload) {
		nautilus_window_slot_queue_reload (slot);
		slot->details->needs_reload = FALSE;
	}

	remove_loading_floating_bar (slot);
}

static void
real_setup_loading_floating_bar (NautilusWindowSlot *slot)
{
	gboolean disable_chrome;

	g_object_get (nautilus_window_slot_get_window (slot),
		      "disable-chrome", &disable_chrome,
		      NULL);

	if (disable_chrome) {
		gtk_widget_hide (slot->details->floating_bar);
		return;
	}

	nautilus_floating_bar_cleanup_actions (NAUTILUS_FLOATING_BAR (slot->details->floating_bar));
	nautilus_floating_bar_set_primary_label (NAUTILUS_FLOATING_BAR (slot->details->floating_bar),
						 NAUTILUS_IS_SEARCH_DIRECTORY (nautilus_view_get_model (slot->details->content_view)) ?
						 _("Searching…") : _("Loading…"));
	nautilus_floating_bar_set_details_label (NAUTILUS_FLOATING_BAR (slot->details->floating_bar), NULL);
	nautilus_floating_bar_set_show_spinner (NAUTILUS_FLOATING_BAR (slot->details->floating_bar),
						TRUE);
	nautilus_floating_bar_add_action (NAUTILUS_FLOATING_BAR (slot->details->floating_bar),
					  GTK_STOCK_STOP,
					  NAUTILUS_FLOATING_BAR_ACTION_ID_STOP);

	gtk_widget_set_halign (slot->details->floating_bar, GTK_ALIGN_END);
	gtk_widget_show (slot->details->floating_bar);
}

static gboolean
setup_loading_floating_bar_timeout_cb (gpointer user_data)
{
	NautilusWindowSlot *slot = user_data;

	slot->details->loading_timeout_id = 0;
	real_setup_loading_floating_bar (slot);

	return FALSE;
}

static void
setup_loading_floating_bar (NautilusWindowSlot *slot)
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
view_begin_loading_cb (NautilusView       *view,
		       NautilusWindowSlot *slot)
{
	if (slot->details->temporarily_ignore_view_signals) {
		return;
	}

	nautilus_profile_start (NULL);

	if (view == slot->details->new_content_view) {
		location_has_really_changed (slot);
	} else {
		nautilus_window_slot_set_allow_stop (slot, TRUE);
	}

	setup_loading_floating_bar (slot);

	nautilus_profile_end (NULL);
}

static void
nautilus_window_slot_connect_new_content_view (NautilusWindowSlot *slot)
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
nautilus_window_slot_switch_new_content_view (NautilusWindowSlot *slot)
{
	NautilusWindow *window;
	GtkWidget *widget;

	if ((slot->details->new_content_view == NULL) ||
	    gtk_widget_get_parent (GTK_WIDGET (slot->details->new_content_view)) != NULL) {
		return;
	}

	window = nautilus_window_slot_get_window (slot);

	if (slot->details->content_view != NULL) {
		nautilus_window_disconnect_content_view (window, slot->details->content_view);

		widget = GTK_WIDGET (slot->details->content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->details->content_view);
		slot->details->content_view = NULL;
	}

	if (slot->details->new_content_view != NULL) {
		slot->details->content_view = slot->details->new_content_view;
		slot->details->new_content_view = NULL;

		widget = GTK_WIDGET (slot->details->content_view);
		gtk_container_add (GTK_CONTAINER (slot->details->view_overlay), widget);
		gtk_widget_show (widget);

		/* connect new view */
		nautilus_window_connect_content_view (slot->details->window, slot->details->content_view);

		if (!NAUTILUS_IS_SEARCH_DIRECTORY (nautilus_view_get_model (slot->details->content_view)) &&
		    slot == nautilus_window_get_active_slot (window)) {
			nautilus_view_grab_focus (slot->details->content_view);
		}
	}
}

/* This is called when we have decided we can actually change to the new view/location situation. */
static void
location_has_really_changed (NautilusWindowSlot *slot)
{
	NautilusWindow *window;
	GFile *location;

	window = nautilus_window_slot_get_window (slot);

	/* Switch to the new content view. */
	nautilus_window_slot_switch_new_content_view (slot);

	if (slot->details->pending_location != NULL) {
		/* Tell the window we are finished. */
		nautilus_window_slot_update_for_new_location (slot);
	}

	location = nautilus_window_slot_get_location (slot);
	if (location != NULL) {
		g_object_ref (location);
	}

	if (location != NULL) {
		if (slot == nautilus_window_get_active_slot (window)) {
			char *uri;

			uri = g_file_get_uri (location);
			g_signal_emit_by_name (window, "loading-uri", uri);
			g_free (uri);
		}

		g_object_unref (location);
	}
}

static void
nautilus_window_slot_dispose (GObject *object)
{
	NautilusWindowSlot *slot;
	GtkWidget *widget;

	slot = NAUTILUS_WINDOW_SLOT (object);

	nautilus_window_slot_clear_forward_list (slot);
	nautilus_window_slot_clear_back_list (slot);

	if (slot->details->content_view) {
		nautilus_window_disconnect_content_view (nautilus_window_slot_get_window (slot),
							 slot->details->content_view);

		widget = GTK_WIDGET (slot->details->content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->details->content_view);
		slot->details->content_view = NULL;
	}

	if (slot->details->new_content_view) {
		widget = GTK_WIDGET (slot->details->new_content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->details->new_content_view);
		slot->details->new_content_view = NULL;
	}

	if (slot->details->set_status_timeout_id != 0) {
		g_source_remove (slot->details->set_status_timeout_id);
		slot->details->set_status_timeout_id = 0;
	}

	if (slot->details->loading_timeout_id != 0) {
		g_source_remove (slot->details->loading_timeout_id);
		slot->details->loading_timeout_id = 0;
	}

	nautilus_window_slot_set_viewed_file (slot, NULL);
	/* TODO? why do we unref here? the file is NULL.
	 * It was already here before the slot move, though */
	nautilus_file_unref (slot->details->viewed_file);

	if (slot->details->location) {
		/* TODO? why do we ref here, instead of unreffing?
		 * It was already here before the slot migration, though */
		g_object_ref (slot->details->location);
	}

	g_list_free_full (slot->details->pending_selection, g_object_unref);
	slot->details->pending_selection = NULL;

	g_clear_object (&slot->details->current_location_bookmark);
	g_clear_object (&slot->details->last_location_bookmark);

	if (slot->details->find_mount_cancellable != NULL) {
		g_cancellable_cancel (slot->details->find_mount_cancellable);
		slot->details->find_mount_cancellable = NULL;
	}

	slot->details->window = NULL;

	g_free (slot->details->title);
	slot->details->title = NULL;

	free_location_change (slot);

	G_OBJECT_CLASS (nautilus_window_slot_parent_class)->dispose (object);
}

static void
nautilus_window_slot_class_init (NautilusWindowSlotClass *klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	klass->active = real_active;
	klass->inactive = real_inactive;

	oclass->dispose = nautilus_window_slot_dispose;
	oclass->constructed = nautilus_window_slot_constructed;
	oclass->set_property = nautilus_window_slot_set_property;
	oclass->get_property = nautilus_window_slot_get_property;

	signals[ACTIVE] =
		g_signal_new ("active",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NautilusWindowSlotClass, active),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[INACTIVE] =
		g_signal_new ("inactive",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NautilusWindowSlotClass, inactive),
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

	properties[PROP_WINDOW] =
		g_param_spec_object ("window",
				     "The NautilusWindow",
				     "The NautilusWindow this slot is part of",
				     NAUTILUS_TYPE_WINDOW,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
	g_type_class_add_private (klass, sizeof (NautilusWindowSlotDetails));
}

GFile *
nautilus_window_slot_get_location (NautilusWindowSlot *slot)
{
	g_assert (slot != NULL);

	return slot->details->location;
}

const gchar *
nautilus_window_slot_get_title (NautilusWindowSlot *slot)
{
	return slot->details->title;
}

char *
nautilus_window_slot_get_location_uri (NautilusWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	if (slot->details->location) {
		return g_file_get_uri (slot->details->location);
	}
	return NULL;
}

NautilusWindow *
nautilus_window_slot_get_window (NautilusWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));
	return slot->details->window;
}

void
nautilus_window_slot_set_window (NautilusWindowSlot *slot,
				 NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));
	g_assert (NAUTILUS_IS_WINDOW (window));

	if (slot->details->window != window) {
		slot->details->window = window;
		g_object_notify_by_pspec (G_OBJECT (slot), properties[PROP_WINDOW]);
	}
}

NautilusView *
nautilus_window_slot_get_view (NautilusWindowSlot *slot)
{
	return slot->details->content_view;
}

/* nautilus_window_slot_update_title:
 * 
 * Re-calculate the slot title.
 * Called when the location or view has changed.
 * @slot: The NautilusWindowSlot in question.
 * 
 */
void
nautilus_window_slot_update_title (NautilusWindowSlot *slot)
{
	NautilusWindow *window;
	char *title;
	gboolean do_sync = FALSE;

	title = nautilus_compute_title_for_location (slot->details->location);
	window = nautilus_window_slot_get_window (slot);

	if (g_strcmp0 (title, slot->details->title) != 0) {
		do_sync = TRUE;

		g_free (slot->details->title);
		slot->details->title = title;
		title = NULL;
	}

	if (strlen (slot->details->title) > 0) {
		do_sync = TRUE;
	}

	if (do_sync) {
		nautilus_window_sync_title (window, slot);
	}

	if (title != NULL) {
		g_free (title);
	}
}

gboolean
nautilus_window_slot_get_allow_stop (NautilusWindowSlot *slot)
{
	return slot->details->allow_stop;
}

void
nautilus_window_slot_set_allow_stop (NautilusWindowSlot *slot,
				     gboolean allow)
{
	NautilusWindow *window;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	slot->details->allow_stop = allow;

	window = nautilus_window_slot_get_window (slot);
	nautilus_window_sync_allow_stop (window, slot);
}

void
nautilus_window_slot_stop_loading (NautilusWindowSlot *slot)
{
	nautilus_view_stop_loading (slot->details->content_view);

	if (slot->details->new_content_view != NULL) {
		slot->details->temporarily_ignore_view_signals = TRUE;
		nautilus_view_stop_loading (slot->details->new_content_view);
		slot->details->temporarily_ignore_view_signals = FALSE;
	}

        cancel_location_change (slot);
}

static void
real_slot_set_short_status (NautilusWindowSlot *slot,
			    const gchar *primary_status,
			    const gchar *detail_status)
{
	gboolean disable_chrome;

	nautilus_floating_bar_cleanup_actions (NAUTILUS_FLOATING_BAR (slot->details->floating_bar));
	nautilus_floating_bar_set_show_spinner (NAUTILUS_FLOATING_BAR (slot->details->floating_bar),
						FALSE);

	g_object_get (nautilus_window_slot_get_window (slot),
		      "disable-chrome", &disable_chrome,
		      NULL);

	if ((primary_status == NULL && detail_status == NULL) || disable_chrome) {
		gtk_widget_hide (slot->details->floating_bar);
		return;
	}

	nautilus_floating_bar_set_labels (NAUTILUS_FLOATING_BAR (slot->details->floating_bar),
					  primary_status, detail_status);
	gtk_widget_show (slot->details->floating_bar);
}

typedef struct {
	gchar *primary_status;
	gchar *detail_status;
	NautilusWindowSlot *slot;
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
set_floating_bar_status (NautilusWindowSlot *slot,
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
nautilus_window_slot_set_status (NautilusWindowSlot *slot,
				 const char *primary_status,
				 const char *detail_status)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	if (slot->details->content_view != NULL) {
		set_floating_bar_status (slot, primary_status, detail_status);
	}
}

/* returns either the pending or the actual current uri */
char *
nautilus_window_slot_get_current_uri (NautilusWindowSlot *slot)
{
	if (slot->details->pending_location != NULL) {
		return g_file_get_uri (slot->details->pending_location);
	}

	if (slot->details->location != NULL) {
		return g_file_get_uri (slot->details->location);
	}

	return NULL;
}

NautilusView *
nautilus_window_slot_get_current_view (NautilusWindowSlot *slot)
{
	if (slot->details->content_view != NULL) {
		return slot->details->content_view;
	} else if (slot->details->new_content_view) {
		return slot->details->new_content_view;
	}

	return NULL;
}

void
nautilus_window_slot_go_home (NautilusWindowSlot *slot,
			      NautilusWindowOpenFlags flags)
{			      
	GFile *home;

	g_return_if_fail (NAUTILUS_IS_WINDOW_SLOT (slot));

	home = g_file_new_for_path (g_get_home_dir ());
	nautilus_window_slot_open_location (slot, home, flags);
	g_object_unref (home);
}

void
nautilus_window_slot_go_up (NautilusWindowSlot *slot,
			    NautilusWindowOpenFlags flags)
{
	GFile *parent;

	if (slot->details->location == NULL) {
		return;
	}

	parent = g_file_get_parent (slot->details->location);
	if (parent == NULL) {
		return;
	}

	nautilus_window_slot_open_location (slot, parent, flags);
	g_object_unref (parent);
}

NautilusFile *
nautilus_window_slot_get_file (NautilusWindowSlot *slot)
{
	return slot->details->viewed_file;
}

NautilusBookmark *
nautilus_window_slot_get_bookmark (NautilusWindowSlot *slot)
{
	return slot->details->current_location_bookmark;
}

GList *
nautilus_window_slot_get_back_history (NautilusWindowSlot *slot)
{
	return slot->details->back_list;
}

GList *
nautilus_window_slot_get_forward_history (NautilusWindowSlot *slot)
{
	return slot->details->forward_list;
}

NautilusWindowSlot *
nautilus_window_slot_new (NautilusWindow *window)
{
	return g_object_new (NAUTILUS_TYPE_WINDOW_SLOT,
			     "window", window,
			     NULL);
}

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
#include "nemo-floating-bar.h"
#include "nemo-window-private.h"
#include "nemo-window-manage-views.h"
#include "nemo-desktop-window.h"

#include <glib/gi18n.h>

#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-global-preferences.h>

#include <eel/eel-string.h>

G_DEFINE_TYPE (NemoWindowSlot, nemo_window_slot, GTK_TYPE_BOX);

enum {
	ACTIVE,
	INACTIVE,
	CHANGED_PANE,
	LOCATION_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

gboolean
nemo_window_slot_handle_event (NemoWindowSlot *slot,
				   GdkEventKey        *event)
{
	NemoWindow *window;

	window = nemo_window_slot_get_window (slot);
	if (NEMO_IS_DESKTOP_WINDOW (window))
		return FALSE;
	return nemo_query_editor_handle_event (slot->query_editor, event);
}

static void
sync_search_directory (NemoWindowSlot *slot)
{
	NemoDirectory *directory;
	NemoQuery *query;
	gchar *text;
	GFile *location;

	g_assert (NEMO_IS_FILE (slot->viewed_file));

	directory = nemo_directory_get_for_file (slot->viewed_file);
	g_assert (NEMO_IS_SEARCH_DIRECTORY (directory));

	query = nemo_query_editor_get_query (slot->query_editor);
	text = nemo_query_get_text (query);

	if (!strlen (text)) {
		location = nemo_query_editor_get_location (slot->query_editor);
		slot->load_with_search = TRUE;
		nemo_window_slot_open_location (slot, location, 0);
		g_object_unref (location);
	} else {
		nemo_search_directory_set_query (NEMO_SEARCH_DIRECTORY (directory),
						     query);
		nemo_window_slot_reload (slot);
	}

	g_free (text);
	g_object_unref (query);
	nemo_directory_unref (directory);
}

static void
sync_search_location_cb (NemoWindow *window,
			 GError *error,
			 gpointer user_data)
{
	NemoWindowSlot *slot = user_data;

	sync_search_directory (slot);
}

static void
create_new_search (NemoWindowSlot *slot)
{
	char *uri;
	NemoDirectory *directory;
	GFile *location;

	uri = nemo_search_directory_generate_new_uri ();
	location = g_file_new_for_uri (uri);

	directory = nemo_directory_get (location);
	g_assert (NEMO_IS_SEARCH_DIRECTORY (directory));

	nemo_window_slot_open_location_full (slot, location, 0, NULL, sync_search_location_cb, slot);

	nemo_directory_unref (directory);
	g_object_unref (location);
	g_free (uri);
}

static void
query_editor_cancel_callback (NemoQueryEditor *editor,
			      NemoWindowSlot *slot)
{
	GtkAction *search;

	search = gtk_action_group_get_action (slot->pane->toolbar_action_group,
					      NEMO_ACTION_SEARCH);

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (search), FALSE);
}

static void
query_editor_changed_callback (NemoQueryEditor *editor,
			       NemoQuery *query,
			       gboolean reload,
			       NemoWindowSlot *slot)
{
	NemoDirectory *directory;

	g_assert (NEMO_IS_FILE (slot->viewed_file));

	directory = nemo_directory_get_for_file (slot->viewed_file);
	if (!NEMO_IS_SEARCH_DIRECTORY (directory)) {
		/* this is the first change from the query editor. we
		   ask for a location change to the search directory,
		   indicate the directory needs to be sync'd with the
		   current query. */
		create_new_search (slot);
		/* Focus is now on the new slot, move it back to query_editor */
		gtk_widget_grab_focus (GTK_WIDGET (slot->query_editor));
	} else {
		sync_search_directory (slot);
	}

	nemo_directory_unref (directory);
}

static void
update_query_editor (NemoWindowSlot *slot)
{
	NemoDirectory *directory;
	NemoSearchDirectory *search_directory;

	directory = nemo_directory_get (slot->location);

	if (NEMO_IS_SEARCH_DIRECTORY (directory)) {
		NemoQuery *query;
		search_directory = NEMO_SEARCH_DIRECTORY (directory);
		query = nemo_search_directory_get_query (search_directory);
		if (query != NULL) {
			nemo_query_editor_set_query (slot->query_editor,
							 query);
			g_object_unref (query);
		}
	} else {
		nemo_query_editor_set_location (slot->query_editor, slot->location);
	}

	nemo_directory_unref (directory);
}

static void
ensure_query_editor (NemoWindowSlot *slot)
{
	g_assert (slot->query_editor != NULL);

	update_query_editor (slot);

	gtk_widget_show (GTK_WIDGET (slot->query_editor));
	gtk_widget_grab_focus (GTK_WIDGET (slot->query_editor));
}

void
nemo_window_slot_set_query_editor_visible (NemoWindowSlot *slot,
					       gboolean            visible)
{
	if (visible) {
		ensure_query_editor (slot);

		if (slot->qe_changed_id == 0)
			slot->qe_changed_id = g_signal_connect (slot->query_editor, "changed",
								G_CALLBACK (query_editor_changed_callback), slot);
		if (slot->qe_cancel_id == 0)
			slot->qe_cancel_id = g_signal_connect (slot->query_editor, "cancel",
							       G_CALLBACK (query_editor_cancel_callback), slot);

	} else {
		gtk_widget_hide (GTK_WIDGET (slot->query_editor));
		g_signal_handler_disconnect (slot->query_editor, slot->qe_changed_id);
		slot->qe_changed_id = 0;
		g_signal_handler_disconnect (slot->query_editor, slot->qe_cancel_id);
		slot->qe_cancel_id = 0;
		nemo_query_editor_set_query (slot->query_editor, NULL);
	}
}

static void
real_active (NemoWindowSlot *slot)
{
	NemoWindow *window;
	NemoWindowPane *pane;
	int page_num;

	window = nemo_window_slot_get_window (slot);
	pane = slot->pane;
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (pane->notebook),
					  GTK_WIDGET (slot));
	g_assert (page_num >= 0);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (pane->notebook), page_num);

	/* sync window to new slot */
	nemo_window_push_status (window, slot->status_text);
	nemo_window_sync_allow_stop (window, slot);
	nemo_window_sync_title (window, slot);
	nemo_window_sync_zoom_widgets (window);
	nemo_window_pane_sync_location_widgets (slot->pane);
	nemo_window_pane_sync_search_widgets (slot->pane);

	if (slot->viewed_file != NULL) {
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
	}
}

static void
nemo_window_slot_init (NemoWindowSlot *slot)
{
	GtkWidget *extras_vbox;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (slot),
					GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (GTK_WIDGET (slot));

	extras_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	slot->extra_location_widgets = extras_vbox;
	gtk_box_pack_start (GTK_BOX (slot), extras_vbox, FALSE, FALSE, 0);
	gtk_widget_show (extras_vbox);

	slot->query_editor = NEMO_QUERY_EDITOR (nemo_query_editor_new ());
	nemo_window_slot_add_extra_location_widget (slot, GTK_WIDGET (slot->query_editor));
	g_object_add_weak_pointer (G_OBJECT (slot->query_editor),
				   (gpointer *) &slot->query_editor);

	slot->view_overlay = gtk_overlay_new ();
	gtk_widget_add_events (slot->view_overlay,
			       GDK_ENTER_NOTIFY_MASK |
			       GDK_LEAVE_NOTIFY_MASK);
	gtk_box_pack_start (GTK_BOX (slot), slot->view_overlay, TRUE, TRUE, 0);
	gtk_widget_show (slot->view_overlay);

	slot->floating_bar = nemo_floating_bar_new ("", FALSE);
	gtk_widget_set_halign (slot->floating_bar, GTK_ALIGN_END);
	gtk_widget_set_valign (slot->floating_bar, GTK_ALIGN_END);
	gtk_overlay_add_overlay (GTK_OVERLAY (slot->view_overlay),
				 slot->floating_bar);

	g_signal_connect (slot->floating_bar, "action",
			  G_CALLBACK (floating_bar_action_cb), slot);

	slot->title = g_strdup (_("Loading..."));
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

	if (slot->content_view) {
		widget = GTK_WIDGET (slot->content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->content_view);
		slot->content_view = NULL;
	}

	if (slot->new_content_view) {
		widget = GTK_WIDGET (slot->new_content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->new_content_view);
		slot->new_content_view = NULL;
	}

	if (slot->set_status_timeout_id != 0) {
		g_source_remove (slot->set_status_timeout_id);
		slot->set_status_timeout_id = 0;
	}

	if (slot->loading_timeout_id != 0) {
		g_source_remove (slot->loading_timeout_id);
		slot->loading_timeout_id = 0;
	}

	nemo_window_slot_set_viewed_file (slot, NULL);
	/* TODO? why do we unref here? the file is NULL.
	 * It was already here before the slot move, though */
	nemo_file_unref (slot->viewed_file);

	if (slot->location) {
		/* TODO? why do we ref here, instead of unreffing?
		 * It was already here before the slot migration, though */
		g_object_ref (slot->location);
	}

	g_list_free_full (slot->pending_selection, g_object_unref);
	slot->pending_selection = NULL;

	g_clear_object (&slot->current_location_bookmark);
	g_clear_object (&slot->last_location_bookmark);

	if (slot->find_mount_cancellable != NULL) {
		g_cancellable_cancel (slot->find_mount_cancellable);
		slot->find_mount_cancellable = NULL;
	}

	slot->pane = NULL;

	g_free (slot->title);
	slot->title = NULL;

	g_free (slot->status_text);
	slot->status_text = NULL;

	G_OBJECT_CLASS (nemo_window_slot_parent_class)->dispose (object);
}

static void
nemo_window_slot_class_init (NemoWindowSlotClass *klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	klass->active = real_active;
	klass->inactive = real_inactive;

	oclass->dispose = nemo_window_slot_dispose;

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
}

GFile *
nemo_window_slot_get_location (NemoWindowSlot *slot)
{
	g_assert (slot != NULL);

	if (slot->location != NULL) {
		return g_object_ref (slot->location);
	}
	return NULL;
}

char *
nemo_window_slot_get_location_uri (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	if (slot->location) {
		return g_file_get_uri (slot->location);
	}
	return NULL;
}

void
nemo_window_slot_make_hosting_pane_active (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_PANE (slot->pane));
	
	nemo_window_set_active_slot (nemo_window_slot_get_window (slot),
					 slot);
}

NemoWindow *
nemo_window_slot_get_window (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_SLOT (slot));
	return slot->pane->window;
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

	title = nemo_compute_title_for_location (slot->location);
	window = nemo_window_slot_get_window (slot);

	if (g_strcmp0 (title, slot->title) != 0) {
		do_sync = TRUE;

		g_free (slot->title);
		slot->title = title;
		title = NULL;
	}

	if (strlen (slot->title) > 0 &&
	    slot->current_location_bookmark != NULL) {
		do_sync = TRUE;
	}

	if (do_sync) {
		nemo_window_sync_title (window, slot);
	}

	if (title != NULL) {
		g_free (title);
	}
}

/* nemo_window_slot_update_icon:
 * 
 * Re-calculate the slot icon
 * Called when the location or view or icon set has changed.
 * @slot: The NemoWindowSlot in question.
 */
void
nemo_window_slot_update_icon (NemoWindowSlot *slot)
{
	NemoWindow *window;
	NemoIconInfo *info;
	const char *icon_name;
	GdkPixbuf *pixbuf;

	window = nemo_window_slot_get_window (slot);
	info = NEMO_WINDOW_CLASS (G_OBJECT_GET_CLASS (window))->get_icon (window, slot);

	icon_name = NULL;
	if (info) {
		icon_name = nemo_icon_info_get_used_name (info);
		if (icon_name != NULL) {
			/* Gtk+ doesn't short circuit this (yet), so avoid lots of work
			 * if we're setting to the same icon. This happens a lot e.g. when
			 * the trash directory changes due to the file count changing.
			 */
			if (g_strcmp0 (icon_name, gtk_window_get_icon_name (GTK_WINDOW (window))) != 0) {			
				gtk_window_set_icon_name (GTK_WINDOW (window), icon_name);
			}
		} else {
			pixbuf = nemo_icon_info_get_pixbuf_nodefault (info);
			
			if (pixbuf) {
				gtk_window_set_icon (GTK_WINDOW (window), pixbuf);
				g_object_unref (pixbuf);
			} 
		}
		
		g_object_unref (info);
	}
}

void
nemo_window_slot_set_content_view_widget (NemoWindowSlot *slot,
					      NemoView *new_view)
{
	NemoWindow *window;
	GtkWidget *widget;

	window = nemo_window_slot_get_window (slot);

	if (slot->content_view != NULL) {
		/* disconnect old view */
		nemo_window_disconnect_content_view (window, slot->content_view);

		widget = GTK_WIDGET (slot->content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->content_view);
		slot->content_view = NULL;
	}

	if (new_view != NULL) {
		widget = GTK_WIDGET (new_view);
		gtk_container_add (GTK_CONTAINER (slot->view_overlay), widget);
		gtk_widget_show (widget);

		slot->content_view = new_view;
		g_object_ref (slot->content_view);

		/* connect new view */
		nemo_window_connect_content_view (window, new_view);
	}
}

void
nemo_window_slot_set_allow_stop (NemoWindowSlot *slot,
				     gboolean allow)
{
	NemoWindow *window;

	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	slot->allow_stop = allow;

	window = nemo_window_slot_get_window (slot);
	nemo_window_sync_allow_stop (window, slot);
}

static void
real_slot_set_short_status (NemoWindowSlot *slot,
			    const gchar *status)
{
	
	gboolean show_statusbar;
	gboolean disable_chrome;

	nemo_floating_bar_cleanup_actions (NEMO_FLOATING_BAR (slot->floating_bar));
	nemo_floating_bar_set_show_spinner (NEMO_FLOATING_BAR (slot->floating_bar),
						FALSE);

	show_statusbar = g_settings_get_boolean (nemo_window_state,
						 NEMO_WINDOW_STATE_START_WITH_STATUS_BAR);

	g_object_get (nemo_window_slot_get_window (slot),
		      "disable-chrome", &disable_chrome,
		      NULL);

	if (status == NULL || show_statusbar || disable_chrome) {
		gtk_widget_hide (slot->floating_bar);
		return;
	}

	nemo_floating_bar_set_label (NEMO_FLOATING_BAR (slot->floating_bar), status);
	gtk_widget_show (slot->floating_bar);
}

typedef struct {
	gchar *status;
	NemoWindowSlot *slot;
} SetStatusData;

static void
set_status_data_free (gpointer data)
{
	SetStatusData *status_data = data;

	g_free (status_data->status);

	g_slice_free (SetStatusData, data);
}

static gboolean
set_status_timeout_cb (gpointer data)
{
	SetStatusData *status_data = data;

	status_data->slot->set_status_timeout_id = 0;
	real_slot_set_short_status (status_data->slot, status_data->status);

	return FALSE;
}

static void
set_floating_bar_status (NemoWindowSlot *slot,
			 const gchar *status)
{
	GtkSettings *settings;
	gint double_click_time;
	SetStatusData *status_data;

	if (slot->set_status_timeout_id != 0) {
		g_source_remove (slot->set_status_timeout_id);
		slot->set_status_timeout_id = 0;
	}

	settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (slot->content_view)));
	g_object_get (settings,
		      "gtk-double-click-time", &double_click_time,
		      NULL);

	status_data = g_slice_new0 (SetStatusData);
	status_data->status = g_strdup (status);
	status_data->slot = slot;

	/* waiting for half of the double-click-time before setting
	 * the status seems to be a good approximation of not setting it
	 * too often and not delaying the statusbar too much.
	 */
	slot->set_status_timeout_id =
		g_timeout_add_full (G_PRIORITY_DEFAULT,
				    (guint) (double_click_time / 2),
				    set_status_timeout_cb,
				    status_data,
				    set_status_data_free);
}

void
nemo_window_slot_set_status (NemoWindowSlot *slot,
				 const char *status,
				 const char *short_status)
{
	NemoWindow *window;

	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	g_free (slot->status_text);
	slot->status_text = g_strdup (status);

	if (slot->content_view != NULL) {
		set_floating_bar_status (slot, short_status);
	}

	window = nemo_window_slot_get_window (slot);
	if (slot == nemo_window_get_active_slot (window)) {
		nemo_window_push_status (window, slot->status_text);
	}
}

static void
remove_all_extra_location_widgets (GtkWidget *widget,
				   gpointer data)
{
	NemoWindowSlot *slot = data;
	NemoDirectory *directory;

	directory = nemo_directory_get (slot->location);
	if (widget != GTK_WIDGET (slot->query_editor)) {
		gtk_container_remove (GTK_CONTAINER (slot->extra_location_widgets), widget);
	}

	nemo_directory_unref (directory);
}

void
nemo_window_slot_remove_extra_location_widgets (NemoWindowSlot *slot)
{
	gtk_container_foreach (GTK_CONTAINER (slot->extra_location_widgets),
			       remove_all_extra_location_widgets,
			       slot);
}

void
nemo_window_slot_add_extra_location_widget (NemoWindowSlot *slot,
						GtkWidget *widget)
{
	gtk_box_pack_start (GTK_BOX (slot->extra_location_widgets),
			    widget, TRUE, TRUE, 0);
	gtk_widget_show (slot->extra_location_widgets);
}

/* returns either the pending or the actual current uri */
char *
nemo_window_slot_get_current_uri (NemoWindowSlot *slot)
{
	if (slot->pending_location != NULL) {
		return g_file_get_uri (slot->pending_location);
	}

	if (slot->location != NULL) {
		return g_file_get_uri (slot->location);
	}

	g_assert_not_reached ();
	return NULL;
}

NemoView *
nemo_window_slot_get_current_view (NemoWindowSlot *slot)
{
	if (slot->content_view != NULL) {
		return slot->content_view;
	} else if (slot->new_content_view) {
		return slot->new_content_view;
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

	if (slot->location == NULL) {
		return;
	}

	parent = g_file_get_parent (slot->location);
	if (parent == NULL) {
		return;
	}

	nemo_window_slot_open_location (slot, parent, flags);
	g_object_unref (parent);
}

void
nemo_window_slot_clear_forward_list (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	g_list_free_full (slot->forward_list, g_object_unref);
	slot->forward_list = NULL;
}

void
nemo_window_slot_clear_back_list (NemoWindowSlot *slot)
{
	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	g_list_free_full (slot->back_list, g_object_unref);
	slot->back_list = NULL;
}

gboolean
nemo_window_slot_should_close_with_mount (NemoWindowSlot *slot,
					      GMount *mount)
{
	GFile *mount_location;
	gboolean close_with_mount;

	mount_location = g_mount_get_root (mount);
	close_with_mount = 
		g_file_has_prefix (NEMO_WINDOW_SLOT (slot)->location, mount_location) ||
		g_file_equal (NEMO_WINDOW_SLOT (slot)->location, mount_location);

	g_object_unref (mount_location);

	return close_with_mount;
}

NemoWindowSlot *
nemo_window_slot_new (NemoWindowPane *pane)
{
	NemoWindowSlot *slot;

	slot = g_object_new (NEMO_TYPE_WINDOW_SLOT, NULL);
	slot->pane = pane;

	return slot;
}

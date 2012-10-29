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
#include "nautilus-toolbar.h"
#include "nautilus-floating-bar.h"
#include "nautilus-window-private.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-desktop-window.h"

#include <glib/gi18n.h>

#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>

#include <eel/eel-string.h>

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
};

static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

gboolean
nautilus_window_slot_handle_event (NautilusWindowSlot *slot,
				   GdkEventKey        *event)
{
	NautilusWindow *window;

	window = nautilus_window_slot_get_window (slot);
	if (NAUTILUS_IS_DESKTOP_WINDOW (window))
		return FALSE;
	return nautilus_query_editor_handle_event (slot->query_editor, event);
}

static void
sync_search_directory (NautilusWindowSlot *slot)
{
	NautilusDirectory *directory;
	NautilusQuery *query;
	gchar *text;
	GFile *location;

	g_assert (NAUTILUS_IS_FILE (slot->viewed_file));

	directory = nautilus_directory_get_for_file (slot->viewed_file);
	g_assert (NAUTILUS_IS_SEARCH_DIRECTORY (directory));

	query = nautilus_query_editor_get_query (slot->query_editor);
	text = nautilus_query_get_text (query);

	if (!strlen (text)) {
		/* Prevent the location change from hiding the query editor in this case */
		slot->load_with_search = TRUE;
		location = nautilus_query_editor_get_location (slot->query_editor);
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

	query = nautilus_query_editor_get_query (slot->query_editor);
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
	if (slot->content_view != NULL) {
		nautilus_view_activate_selection (slot->content_view);
	}
}

static void
query_editor_changed_callback (NautilusQueryEditor *editor,
			       NautilusQuery *query,
			       gboolean reload,
			       NautilusWindowSlot *slot)
{
	NautilusDirectory *directory;

	g_assert (NAUTILUS_IS_FILE (slot->viewed_file));

	directory = nautilus_directory_get_for_file (slot->viewed_file);
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
	gtk_widget_hide (GTK_WIDGET (slot->query_editor));

	if (slot->qe_changed_id > 0) {
		g_signal_handler_disconnect (slot->query_editor, slot->qe_changed_id);
		slot->qe_changed_id = 0;
	}
	if (slot->qe_cancel_id > 0) {
		g_signal_handler_disconnect (slot->query_editor, slot->qe_cancel_id);
		slot->qe_cancel_id = 0;
	}
	if (slot->qe_activated_id > 0) {
		g_signal_handler_disconnect (slot->query_editor, slot->qe_activated_id);
		slot->qe_activated_id = 0;
	}

	nautilus_query_editor_set_query (slot->query_editor, NULL);
}

static void
show_query_editor (NautilusWindowSlot *slot)
{
	NautilusDirectory *directory;
	NautilusSearchDirectory *search_directory;
	GFile *location;

	if (slot->location) {
		location = slot->location;
	} else {
		location = slot->pending_location;
	}

	directory = nautilus_directory_get (location);

	if (NAUTILUS_IS_SEARCH_DIRECTORY (directory)) {
		NautilusQuery *query;
		search_directory = NAUTILUS_SEARCH_DIRECTORY (directory);
		query = nautilus_search_directory_get_query (search_directory);
		if (query != NULL) {
			nautilus_query_editor_set_query (slot->query_editor,
							 query);
			g_object_unref (query);
		}
	} else {
		nautilus_query_editor_set_location (slot->query_editor, location);
	}

	nautilus_directory_unref (directory);

	gtk_widget_show (GTK_WIDGET (slot->query_editor));
	gtk_widget_grab_focus (GTK_WIDGET (slot->query_editor));

	if (slot->qe_changed_id == 0) {
		slot->qe_changed_id = g_signal_connect (slot->query_editor, "changed",
							G_CALLBACK (query_editor_changed_callback), slot);
	}
	if (slot->qe_cancel_id == 0) {
		slot->qe_cancel_id = g_signal_connect (slot->query_editor, "cancel",
						       G_CALLBACK (query_editor_cancel_callback), slot);
	}
	if (slot->qe_activated_id == 0) {
		slot->qe_activated_id = g_signal_connect (slot->query_editor, "activated",
							  G_CALLBACK (query_editor_activated_callback), slot);
	}
}

void
nautilus_window_slot_set_search_visible (NautilusWindowSlot *slot,
					 gboolean            visible)
{
	gboolean old_visible;
	GFile *return_location;
	GtkAction *action;
	gboolean active_slot;

	old_visible = slot->search_visible;
	slot->search_visible = visible;

	return_location = NULL;
	active_slot = (slot == nautilus_window_get_active_slot (slot->details->window));

	if (visible) {
		show_query_editor (slot);
	} else {
		if (old_visible && active_slot && slot->query_editor != NULL) {
			/* Use the location bar as the return location */
			return_location = nautilus_query_editor_get_location (slot->query_editor);

			/* Last try: use the home directory as the return location */
			if (return_location == NULL) {
				return_location = g_file_new_for_path (g_get_home_dir ());
			}
		}

		if (active_slot) {
			nautilus_window_grab_focus (slot->details->window);
		}

		hide_query_editor (slot);
	}

	if (!active_slot) {
		return;
	}

	action = gtk_action_group_get_action (nautilus_window_get_main_action_group (slot->details->window),
					      NAUTILUS_ACTION_SEARCH);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), visible);

	if (return_location != NULL) {
		nautilus_window_go_to (slot->details->window, return_location);
		g_object_unref (return_location);
	}
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
	nautilus_window_sync_search_widgets (window);

	if (slot->viewed_file != NULL) {
		nautilus_window_sync_view_as_menus (window);
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
	slot->extra_location_widgets = extras_vbox;
	gtk_box_pack_start (GTK_BOX (slot), extras_vbox, FALSE, FALSE, 0);
	gtk_widget_show (extras_vbox);

	slot->query_editor = NAUTILUS_QUERY_EDITOR (nautilus_query_editor_new ());
	nautilus_window_slot_add_extra_location_widget (slot, GTK_WIDGET (slot->query_editor));
	g_object_add_weak_pointer (G_OBJECT (slot->query_editor),
				   (gpointer *) &slot->query_editor);

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

	slot->title = g_strdup (_("Loading..."));
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

static void
view_end_loading_cb (NautilusView       *view,
		     gboolean            all_files_seen,
		     NautilusWindowSlot *slot)
{
	if (slot->needs_reload) {
		nautilus_window_slot_queue_reload (slot);
		slot->needs_reload = FALSE;
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

	nautilus_floating_bar_set_primary_label (NAUTILUS_FLOATING_BAR (slot->details->floating_bar),
						 NAUTILUS_IS_SEARCH_DIRECTORY (nautilus_view_get_model (slot->content_view)) ?
						 _("Searching...") : _("Loading..."));
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
	setup_loading_floating_bar (slot);
}

static void
nautilus_window_slot_dispose (GObject *object)
{
	NautilusWindowSlot *slot;
	GtkWidget *widget;

	slot = NAUTILUS_WINDOW_SLOT (object);

	nautilus_window_slot_clear_forward_list (slot);
	nautilus_window_slot_clear_back_list (slot);

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
	nautilus_file_unref (slot->viewed_file);

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

	slot->details->window = NULL;

	g_free (slot->title);
	slot->title = NULL;

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

	if (slot->location != NULL) {
		return g_object_ref (slot->location);
	}
	return NULL;
}

char *
nautilus_window_slot_get_location_uri (NautilusWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	if (slot->location) {
		return g_file_get_uri (slot->location);
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

	title = nautilus_compute_title_for_location (slot->location);
	window = nautilus_window_slot_get_window (slot);

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
		nautilus_window_sync_title (window, slot);
	}

	if (title != NULL) {
		g_free (title);
	}
}

void
nautilus_window_slot_set_content_view_widget (NautilusWindowSlot *slot,
					      NautilusView *new_view)
{
	NautilusWindow *window;
	GtkWidget *widget;

	window = nautilus_window_slot_get_window (slot);

	if (slot->content_view != NULL) {
		/* disconnect old view */
		g_signal_handlers_disconnect_by_func (slot->content_view, G_CALLBACK (view_end_loading_cb), slot);
		g_signal_handlers_disconnect_by_func (slot->content_view, G_CALLBACK (view_begin_loading_cb), slot);

		nautilus_window_disconnect_content_view (window, slot->content_view);

		widget = GTK_WIDGET (slot->content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->content_view);
		slot->content_view = NULL;
	}

	if (new_view != NULL) {
		widget = GTK_WIDGET (new_view);
		gtk_container_add (GTK_CONTAINER (slot->details->view_overlay), widget);
		gtk_widget_show (widget);

		slot->content_view = new_view;
		g_object_ref (slot->content_view);

		g_signal_connect (new_view, "begin_loading", G_CALLBACK (view_begin_loading_cb), slot);
		g_signal_connect (new_view, "end_loading", G_CALLBACK (view_end_loading_cb), slot);

		/* connect new view */
		nautilus_window_connect_content_view (window, new_view);
	}
}

void
nautilus_window_slot_set_allow_stop (NautilusWindowSlot *slot,
				     gboolean allow)
{
	NautilusWindow *window;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	slot->allow_stop = allow;

	window = nautilus_window_slot_get_window (slot);
	nautilus_window_sync_allow_stop (window, slot);
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

	settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (slot->content_view)));
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

	if (slot->content_view != NULL) {
		set_floating_bar_status (slot, primary_status, detail_status);
	}
}

static void
remove_all_extra_location_widgets (GtkWidget *widget,
				   gpointer data)
{
	NautilusWindowSlot *slot = data;
	NautilusDirectory *directory;

	directory = nautilus_directory_get (slot->location);
	if (widget != GTK_WIDGET (slot->query_editor)) {
		gtk_container_remove (GTK_CONTAINER (slot->extra_location_widgets), widget);
	}

	nautilus_directory_unref (directory);
}

void
nautilus_window_slot_remove_extra_location_widgets (NautilusWindowSlot *slot)
{
	gtk_container_foreach (GTK_CONTAINER (slot->extra_location_widgets),
			       remove_all_extra_location_widgets,
			       slot);
}

void
nautilus_window_slot_add_extra_location_widget (NautilusWindowSlot *slot,
						GtkWidget *widget)
{
	gtk_box_pack_start (GTK_BOX (slot->extra_location_widgets),
			    widget, TRUE, TRUE, 0);
	gtk_widget_show (slot->extra_location_widgets);
}

/* returns either the pending or the actual current uri */
char *
nautilus_window_slot_get_current_uri (NautilusWindowSlot *slot)
{
	if (slot->pending_location != NULL) {
		return g_file_get_uri (slot->pending_location);
	}

	if (slot->location != NULL) {
		return g_file_get_uri (slot->location);
	}

	return NULL;
}

NautilusView *
nautilus_window_slot_get_current_view (NautilusWindowSlot *slot)
{
	if (slot->content_view != NULL) {
		return slot->content_view;
	} else if (slot->new_content_view) {
		return slot->new_content_view;
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

	if (slot->location == NULL) {
		return;
	}

	parent = g_file_get_parent (slot->location);
	if (parent == NULL) {
		return;
	}

	nautilus_window_slot_open_location (slot, parent, flags);
	g_object_unref (parent);
}

void
nautilus_window_slot_clear_forward_list (NautilusWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	g_list_free_full (slot->forward_list, g_object_unref);
	slot->forward_list = NULL;
}

void
nautilus_window_slot_clear_back_list (NautilusWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	g_list_free_full (slot->back_list, g_object_unref);
	slot->back_list = NULL;
}

NautilusWindowSlot *
nautilus_window_slot_new (NautilusWindow *window)
{
	return g_object_new (NAUTILUS_TYPE_WINDOW_SLOT,
			     "window", window,
			     NULL);
}

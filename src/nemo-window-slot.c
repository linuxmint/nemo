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
#include "nemo-desktop-window.h"
#include "nemo-floating-bar.h"
#include "nemo-window-private.h"
#include "nemo-window-manage-views.h"
#include "nemo-window-types.h"
#include "nemo-window-slot-dnd.h"
#include "nemo-terminal-widget.h"

#include <glib/gi18n.h>

#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-global-preferences.h>

#define DEBUG_FLAG NEMO_DEBUG_WINDOW
#include <libnemo-private/nemo-debug.h>

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

static void
sync_search_directory (NemoWindowSlot *slot)
{
	NemoDirectory *directory;
	NemoQuery *query;

	g_assert (NEMO_IS_FILE (slot->viewed_file));

	directory = nemo_directory_get_for_file (slot->viewed_file);
	g_assert (NEMO_IS_SEARCH_DIRECTORY (directory));

	query = nemo_query_editor_get_query (slot->query_editor);

    if (query) {
        nemo_query_set_show_hidden (query,
                                    g_settings_get_boolean (nemo_preferences,
                                                            NEMO_PREFERENCES_SHOW_HIDDEN_FILES));
    }

	nemo_search_directory_set_query (NEMO_SEARCH_DIRECTORY (directory),
					     query);

    g_clear_object (&query);

	nemo_window_slot_force_reload (slot);

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

	nemo_window_slot_open_location_full (slot, location, NEMO_WINDOW_OPEN_FLAG_SEARCH, NULL, sync_search_location_cb, slot);

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

    gtk_widget_hide (slot->no_search_results_box);
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

    nemo_query_editor_set_active (NEMO_QUERY_EDITOR (slot->query_editor),
                                  nemo_window_slot_get_location_uri (slot),
                                  TRUE);

	gtk_widget_grab_focus (GTK_WIDGET (slot->query_editor));
}

void
nemo_window_slot_set_query_editor_visible (NemoWindowSlot *slot,
					       gboolean            visible)
{
    gtk_widget_hide (slot->no_search_results_box);

	if (visible) {
		ensure_query_editor (slot);

		if (slot->qe_changed_id == 0)
			slot->qe_changed_id = g_signal_connect (slot->query_editor, "changed",
								G_CALLBACK (query_editor_changed_callback), slot);
		if (slot->qe_cancel_id == 0)
			slot->qe_cancel_id = g_signal_connect (slot->query_editor, "cancel",
							       G_CALLBACK (query_editor_cancel_callback), slot);

	} else {
        nemo_query_editor_set_active (NEMO_QUERY_EDITOR (slot->query_editor), NULL, FALSE);

        if (slot->qe_changed_id > 0) {
            g_signal_handler_disconnect (slot->query_editor, slot->qe_changed_id);
            slot->qe_changed_id = 0;
        }

        if (slot->qe_cancel_id > 0) {
            g_signal_handler_disconnect (slot->query_editor, slot->qe_cancel_id);
            slot->qe_cancel_id = 0;
        }

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
    nemo_window_sync_bookmark_action (window);
	nemo_window_pane_sync_location_widgets (slot->pane);
	nemo_window_pane_sync_search_widgets (slot->pane);
	nemo_window_sync_thumbnail_action(window);

	if (slot->viewed_file != NULL) {
		nemo_window_sync_view_type (window);
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

static GtkWidget *
create_nsr_box (void)
{
    GtkWidget *box;
    GtkWidget *widget;
    PangoAttrList *attrs;

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);

    widget = gtk_image_new_from_icon_name ("system-search-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

    widget = gtk_label_new (_("No files found"));
    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_size_new (20 * PANGO_SCALE));
    gtk_label_set_attributes (GTK_LABEL (widget), attrs);
    pango_attr_list_unref (attrs);
    gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

    gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (box, GTK_ALIGN_CENTER);

    gtk_widget_show_all (box);
    gtk_widget_set_no_show_all (box, TRUE);
    gtk_widget_hide (box);
    return box;
}

static void
nemo_window_slot_init (NemoWindowSlot *slot)
{
	GtkWidget *extras_vbox;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (slot),
					GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (GTK_WIDGET (slot));

	extras_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	slot->extra_location_widgets = extras_vbox;
	gtk_box_pack_start (GTK_BOX (slot), extras_vbox, FALSE, FALSE, 0);
	gtk_widget_show (extras_vbox);

	slot->query_editor = NEMO_QUERY_EDITOR (nemo_query_editor_new ());

	nemo_window_slot_add_extra_location_widget (slot, GTK_WIDGET (slot->query_editor));

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

    slot->no_search_results_box = create_nsr_box ();
    gtk_overlay_add_overlay (GTK_OVERLAY (slot->view_overlay),
                             slot->no_search_results_box);

	g_signal_connect (slot->floating_bar, "action",
			  G_CALLBACK (floating_bar_action_cb), slot);

    slot->cache_bar = NULL;
    slot->terminal_widget = NULL;
    slot->terminal_visible = FALSE;

	slot->title = g_strdup (_("Loading..."));
}

static void
view_end_loading_cb (NemoView       *view,
		     		 gboolean        all_files_seen,
		     		 NemoWindowSlot *slot)
{
	if (slot->needs_reload) {
		nemo_window_slot_queue_reload (slot, FALSE);
		slot->needs_reload = FALSE;
	} else if (all_files_seen) {
        NemoDirectory *directory;

        directory = nemo_directory_get_for_file (slot->viewed_file);

        if (NEMO_IS_SEARCH_DIRECTORY (directory)) {
            if (!nemo_directory_is_not_empty (directory)) {
                gtk_widget_show (slot->no_search_results_box);
            } else {
                gtk_widget_hide (slot->no_search_results_box);

            }
        }

        nemo_directory_unref (directory);
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

        nemo_icon_info_unref (info);
	}
}

void
nemo_window_slot_set_show_thumbnails (NemoWindowSlot *slot,
                                      gboolean show_thumbnails)
{
  NemoDirectory *directory;

  directory = nemo_directory_get (slot->location);
  nemo_directory_set_show_thumbnails(directory, show_thumbnails);
  nemo_directory_unref (directory);
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
        g_signal_handlers_disconnect_by_func (slot->content_view, G_CALLBACK (view_end_loading_cb), slot);

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

		g_signal_connect (new_view, "end_loading", G_CALLBACK (view_end_loading_cb), slot);

		/* connect new view */
		nemo_window_connect_content_view (window, new_view);

        /* If terminal-visible is enabled in config, ensure terminal is initialized and visible */
        gboolean terminal_should_be_visible = g_settings_get_boolean(nemo_window_state, "terminal-visible");
        if (terminal_should_be_visible) {
            /* Defer terminal initialization to an idle callback. This ensures that the main
             * window and its widgets have been allocated sizes before we attempt to create
             * and size the terminal pane, preventing race conditions and sizing issues on startup. */
            g_idle_add((GSourceFunc)nemo_window_slot_ensure_terminal_state, slot);
        }
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

	g_free (data);
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

	status_data = g_new0 (SetStatusData, 1);
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
                             const char *short_status,
                             gboolean    location_loading)
{
	NemoWindow *window;

	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	g_free (slot->status_text);
	slot->status_text = g_strdup (status);

	if (slot->content_view != NULL && !location_loading) {
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
	char * uri;

	if (slot->location == NULL) {
		return;
	}

	parent = g_file_get_parent (slot->location);
	if (parent == NULL) {
		if (g_file_has_uri_scheme (slot->location, "smb")) {
			uri = g_file_get_uri (slot->location);

            DEBUG ("Starting samba URI for navigation: %s", uri);

			if (g_strcmp0 ("smb:///", uri) == 0) {
				parent = g_file_new_for_uri ("network:///");
			}
			else {
                GString *gstr;
                char * temp;

                gstr = g_string_new (uri);

				// Remove last /
                if (g_str_has_suffix (gstr->str, "/")) {
                    gstr = g_string_set_size (gstr, gstr->len - 1);
                }

				// Remove last part of string after last remaining /
				temp = g_strrstr (gstr->str, "/") + 1;
				if (temp != NULL) {
                    gstr = g_string_set_size (gstr, temp - gstr->str);
				}

                // if we're going to end up with smb://, redirect it to network instead.
                if (g_strcmp0 ("smb://", gstr->str) == 0) {
                    gstr = g_string_assign (gstr, "network:///");
                }

                uri = g_string_free (gstr, FALSE);

				parent = g_file_new_for_uri (uri);

                DEBUG ("Ending samba URI for navigation: %s", uri);
			}
			g_free (uri);
		}
		else {
			return;
		}
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

static void
on_terminal_visibility_changed(NemoTerminalWidget *terminal,
                               gboolean visible,
                               NemoWindowSlot *slot)
{
    slot->terminal_visible = visible;
}

static void
on_terminal_directory_changed(NemoTerminalWidget *terminal,
                              GFile *location,
                              NemoWindowSlot *slot)
{
    if (location != NULL) {
        nemo_window_slot_open_location(slot, location, 0);
    }
}

static void
on_paned_size_allocated (GtkWidget *paned, GtkAllocation *allocation, gpointer user_data)
{
    NemoTerminalWidget *terminal = NEMO_TERMINAL_WIDGET (user_data);
    nemo_terminal_widget_apply_new_size (terminal);
    /* Disconnect after the first call to avoid re-applying the size on every allocation. */
    g_signal_handlers_disconnect_by_func (paned, on_paned_size_allocated, terminal);
}

static gboolean
on_paned_button_release (GtkWidget *paned, GdkEventButton *event, gpointer user_data)
{
    int position = gtk_paned_get_position (GTK_PANED (paned));
    int total_height = gtk_widget_get_allocated_height (paned);

    if (total_height > 0)
    {
        int height = total_height - position;
        g_settings_set_int (nemo_window_state, "terminal-pane-size", height);
    }

    return FALSE;
}

/*
 * _initialize_terminal_in_paned:
 * @slot: The #NemoWindowSlot to add the terminal to.
 *
 * This function performs a delicate re-parenting of widgets to insert the
 * terminal. It takes the existing view_overlay, removes it from its parent,
 * creates a new GtkPaned, and places the view_overlay in the top pane and
 * the new terminal widget in the bottom pane. This new GtkPaned is then
 * inserted back into the original parent of the view_overlay.
 */
static void
_initialize_terminal_in_paned(NemoWindowSlot *slot)
{
    GtkWidget *paned_container;
    GtkWidget *parent_of_overlay;
    gint position;
    GList *children;

    parent_of_overlay = gtk_widget_get_parent(slot->view_overlay);
    if (!GTK_IS_BOX(parent_of_overlay)) {
        g_warning("Cannot initialize terminal in paned: parent of view_overlay is not a GtkBox.");
        return;
    }

    children = gtk_container_get_children(GTK_CONTAINER(parent_of_overlay));
    position = g_list_index(children, slot->view_overlay);
    g_list_free(children);

    paned_container = gtk_paned_new(GTK_ORIENTATION_VERTICAL);

    g_object_ref(slot->view_overlay);
    gtk_container_remove(GTK_CONTAINER(parent_of_overlay), slot->view_overlay);

    gtk_paned_pack1(GTK_PANED(paned_container), slot->view_overlay, TRUE, TRUE);
    g_object_unref(slot->view_overlay);

    gtk_paned_pack2(GTK_PANED(paned_container), GTK_WIDGET(slot->terminal_widget), FALSE, TRUE);

    gtk_box_pack_start(GTK_BOX(parent_of_overlay), paned_container, TRUE, TRUE, 0);
    if (position != -1) {
        gtk_box_reorder_child(GTK_BOX(parent_of_overlay), paned_container, position);
    }

    g_signal_connect (paned_container, "size-allocate", G_CALLBACK (on_paned_size_allocated), slot->terminal_widget);
    g_signal_connect (paned_container, "button-release-event", G_CALLBACK (on_paned_button_release), NULL);

    gtk_widget_show_all(paned_container);

    nemo_terminal_widget_set_container_paned(slot->terminal_widget, paned_container);
}

void
nemo_window_slot_init_terminal (NemoWindowSlot *slot)
{
    if (slot->terminal_widget != NULL) {
        return;
    }

    slot->terminal_widget = nemo_terminal_widget_new_with_location(slot->location);

    g_signal_connect(slot->terminal_widget, "toggle-visibility",
                     G_CALLBACK(on_terminal_visibility_changed), slot);
    g_signal_connect(slot->terminal_widget, "change-directory",
                     G_CALLBACK(on_terminal_directory_changed), slot);

    _initialize_terminal_in_paned(slot);
}

void
nemo_window_slot_toggle_terminal (NemoWindowSlot *slot)
{
    if (slot->terminal_widget == NULL) {
        nemo_window_slot_init_terminal(slot);
    }

    if (slot->terminal_widget != NULL) {
        nemo_terminal_widget_toggle_visible(slot->terminal_widget);
    }
}

void
nemo_window_slot_update_terminal_location (NemoWindowSlot *slot)
{
    if (slot->terminal_widget != NULL && slot->location != NULL) {
        nemo_terminal_widget_set_current_location(slot->terminal_widget, slot->location);
    }
}

gboolean
nemo_window_slot_ensure_terminal_state (gpointer user_data)
{
    NemoWindowSlot *slot = user_data;
    gboolean terminal_should_be_visible = g_settings_get_boolean(nemo_window_state, "terminal-visible");

    if (terminal_should_be_visible) {
        if (slot->terminal_widget == NULL) {
            nemo_window_slot_init_terminal(slot);
        }
        nemo_terminal_widget_ensure_state(slot->terminal_widget);
        nemo_window_slot_update_terminal_location(slot);
    } else {
        if (slot->terminal_widget != NULL) {
            nemo_terminal_widget_ensure_state(slot->terminal_widget);
        }
    }
    return G_SOURCE_REMOVE;
}
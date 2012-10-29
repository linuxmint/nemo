/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           John Sullivan <sullivan@eazel.com>
 *           Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "nautilus-window-manage-views.h"

#include "nautilus-actions.h"
#include "nautilus-application.h"
#include "nautilus-pathbar.h"
#include "nautilus-window-private.h"
#include "nautilus-window-slot.h"
#include "nautilus-special-location-bar.h"
#include "nautilus-trash-bar.h"
#include "nautilus-toolbar.h"
#include "nautilus-view-factory.h"
#include "nautilus-x-content-bar.h"
#include <eel/eel-accessibility.h>
#include <eel/eel-debug.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <libnautilus-extension/nautilus-location-widget-provider.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-monitor.h>
#include <libnautilus-private/nautilus-profile.h>
#include <libnautilus-private/nautilus-search-directory.h>

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
static void location_has_really_changed               (NautilusWindowSlot         *slot);
static void update_for_new_location                   (NautilusWindowSlot         *slot);

static void
set_displayed_file (NautilusWindowSlot *slot, NautilusFile *file)
{
        gboolean recreate;
	GFile *new_location = NULL;

	if (file != NULL) {
		new_location = nautilus_file_get_location (file);
	}

        if (slot->current_location_bookmark == NULL || file == NULL) {
                recreate = TRUE;
        } else {
		GFile *bookmark_location;
                bookmark_location = nautilus_bookmark_get_location (slot->current_location_bookmark);
                recreate = !g_file_equal (bookmark_location, new_location);
                g_object_unref (bookmark_location);
        }

        if (recreate) {
		char *display_name = NULL;

                /* We've changed locations, must recreate bookmark for current location. */
		g_clear_object (&slot->last_location_bookmark);

		if (file != NULL) {
			display_name = nautilus_file_get_display_name (file);
		}
		slot->last_location_bookmark = slot->current_location_bookmark;
		if (new_location == NULL) {
			slot->current_location_bookmark = NULL;
		} else {
			slot->current_location_bookmark = nautilus_bookmark_new (new_location, display_name);
		}
		g_free (display_name);
        }

	g_clear_object (&new_location);
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
	check_bookmark_location_matches (slot->last_location_bookmark,
					 slot->location);
}

static void
handle_go_back (NautilusWindowSlot *slot,
		GFile *location)
{
	guint i;
	GList *link;
	NautilusBookmark *bookmark;

	/* Going back. Move items from the back list to the forward list. */
	g_assert (g_list_length (slot->back_list) > slot->location_change_distance);
	check_bookmark_location_matches (NAUTILUS_BOOKMARK (g_list_nth_data (slot->back_list,
									     slot->location_change_distance)),
					 location);
	g_assert (slot->location != NULL);
	
	/* Move current location to Forward list */

	check_last_bookmark_location_matches_slot (slot);

	/* Use the first bookmark in the history list rather than creating a new one. */
	slot->forward_list = g_list_prepend (slot->forward_list,
					     slot->last_location_bookmark);
	g_object_ref (slot->forward_list->data);
				
	/* Move extra links from Back to Forward list */
	for (i = 0; i < slot->location_change_distance; ++i) {
		bookmark = NAUTILUS_BOOKMARK (slot->back_list->data);
		slot->back_list =
			g_list_remove (slot->back_list, bookmark);
		slot->forward_list =
			g_list_prepend (slot->forward_list, bookmark);
	}
	
	/* One bookmark falls out of back/forward lists and becomes viewed location */
	link = slot->back_list;
	slot->back_list = g_list_remove_link (slot->back_list, link);
	g_object_unref (link->data);
	g_list_free_1 (link);
}

static void
handle_go_forward (NautilusWindowSlot *slot,
		   GFile *location)
{
	guint i;
	GList *link;
	NautilusBookmark *bookmark;

	/* Going forward. Move items from the forward list to the back list. */
	g_assert (g_list_length (slot->forward_list) > slot->location_change_distance);
	check_bookmark_location_matches (NAUTILUS_BOOKMARK (g_list_nth_data (slot->forward_list,
									     slot->location_change_distance)),
					 location);
	g_assert (slot->location != NULL);
				
	/* Move current location to Back list */
	check_last_bookmark_location_matches_slot (slot);
	
	/* Use the first bookmark in the history list rather than creating a new one. */
	slot->back_list = g_list_prepend (slot->back_list,
						     slot->last_location_bookmark);
	g_object_ref (slot->back_list->data);
	
	/* Move extra links from Forward to Back list */
	for (i = 0; i < slot->location_change_distance; ++i) {
		bookmark = NAUTILUS_BOOKMARK (slot->forward_list->data);
		slot->forward_list =
			g_list_remove (slot->back_list, bookmark);
		slot->back_list =
			g_list_prepend (slot->forward_list, bookmark);
	}
	
	/* One bookmark falls out of back/forward lists and becomes viewed location */
	link = slot->forward_list;
	slot->forward_list = g_list_remove_link (slot->forward_list, link);
	g_object_unref (link->data);
	g_list_free_1 (link);
}

static void
handle_go_elsewhere (NautilusWindowSlot *slot,
		     GFile *location)
{
	/* Clobber the entire forward list, and move displayed location to back list */
	nautilus_window_slot_clear_forward_list (slot);
		
	if (slot->location != NULL) {
		/* If we're returning to the same uri somehow, don't put this uri on back list. 
		 * This also avoids a problem where set_displayed_location
		 * didn't update last_location_bookmark since the uri didn't change.
		 */
		if (!g_file_equal (slot->location, location)) {
			/* Store bookmark for current location in back list, unless there is no current location */
			check_last_bookmark_location_matches_slot (slot);
			/* Use the first bookmark in the history list rather than creating a new one. */
			slot->back_list = g_list_prepend (slot->back_list,
							  slot->last_location_bookmark);
			g_object_ref (slot->back_list->data);
		}
	}
}

static void
viewed_file_changed_callback (NautilusFile *file,
                              NautilusWindowSlot *slot)
{
        GFile *new_location;
	gboolean is_in_trash, was_in_trash;
	NautilusWindow *window;

        g_assert (NAUTILUS_IS_FILE (file));
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));
	g_assert (file == slot->viewed_file);

	window = nautilus_window_slot_get_window (slot);

        if (!nautilus_file_is_not_yet_confirmed (file)) {
                slot->viewed_file_seen = TRUE;
        }

	was_in_trash = slot->viewed_file_in_trash;

	slot->viewed_file_in_trash = is_in_trash = nautilus_file_is_in_trash (file);

	if (nautilus_file_is_gone (file) || (is_in_trash && !was_in_trash)) {

                if (slot->viewed_file_seen) {
			/* auto-show existing parent. */
			GFile *go_to_file;
			GFile *parent;
			GFile *location;
			GMount *mount;

                        /* Detecting a file is gone may happen in the
                         * middle of a pending location change, we
                         * need to cancel it before closing the window
                         * or things break.
                         */
                        /* FIXME: It makes no sense that this call is
                         * needed. When the window is destroyed, it
                         * calls nautilus_window_manage_views_destroy,
                         * which calls free_location_change, which
                         * should be sufficient. Also, if this was
                         * really needed, wouldn't it be needed for
                         * all other nautilus_window_close callers?
                         */
			end_location_change (slot);

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

                /* If the file was renamed, update location and/or
                 * title. */
                if (!g_file_equal (new_location,
				   slot->location)) {
                        g_object_unref (slot->location);
                        slot->location = new_location;
			if (slot == nautilus_window_get_active_slot (window)) {
				nautilus_window_sync_location_widgets (window);
			}
                } else {
			/* TODO?
 			 *   why do we update title at all in this case? */
                        g_object_unref (new_location);
                }

                nautilus_window_slot_update_title (slot);
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
                handle_go_back (slot, new_location);
                return;
        case NAUTILUS_LOCATION_CHANGE_FORWARD:
                handle_go_forward (slot, new_location);
                return;
        }
	g_return_if_fail (FALSE);
}

static void
cancel_viewed_file_changed_callback (NautilusWindowSlot *slot)
{
        NautilusFile *file;

        file = slot->viewed_file;
        if (file != NULL) {
                g_signal_handlers_disconnect_by_func (G_OBJECT (file),
                                                      G_CALLBACK (viewed_file_changed_callback),
						      slot);
                nautilus_file_monitor_remove (file, &slot->viewed_file);
        }
}

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

        if (target_window == window && target_slot == slot &&
	    old_location && g_file_equal (old_location, location) &&
	    !is_desktop) {

		if (callback != NULL) {
			callback (window, location, NULL, user_data);
		}

		goto done;
        }

	slot->pending_use_default_location = ((flags & NAUTILUS_WINDOW_OPEN_FLAG_USE_DEFAULT_LOCATION) != 0);
        begin_location_change (target_slot, location, old_location, new_selection,
			       NAUTILUS_LOCATION_CHANGE_STANDARD, 0, NULL, callback, user_data);

 done:
	g_clear_object (&old_location);

	nautilus_profile_end (NULL);
}

const char *
nautilus_window_slot_get_content_view_id (NautilusWindowSlot *slot)
{
	if (slot->content_view == NULL) {
		return NULL;
	}
	return nautilus_view_get_view_id (slot->content_view);
}

gboolean
nautilus_window_slot_content_view_matches_iid (NautilusWindowSlot *slot, 
					       const char *iid)
{
	if (slot->content_view == NULL) {
		return FALSE;
	}
	return g_strcmp0 (nautilus_view_get_view_id (slot->content_view), iid) == 0;
}

static gboolean
report_callback (NautilusWindowSlot *slot,
		 GError *error)
{
	if (slot->open_callback != NULL) {
		gboolean res;
		res = slot->open_callback (nautilus_window_slot_get_window (slot),
					   slot->pending_location,
					   error, slot->open_callback_user_data);
		slot->open_callback = NULL;
		slot->open_callback_user_data = NULL;

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

	g_assert (slot->pending_location == NULL);
	g_assert (slot->pending_selection == NULL);
	
	slot->pending_location = g_object_ref (location);
        slot->location_change_type = type;
        slot->location_change_distance = distance;
	slot->tried_mount = FALSE;
	slot->pending_selection = eel_g_object_list_copy (new_selection);

	slot->pending_scroll_to = g_strdup (scroll_pos);

	slot->open_callback = callback;
	slot->open_callback_user_data = user_data;

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
        if (slot->current_location_bookmark != NULL &&
            slot->content_view != NULL) {
                current_pos = nautilus_view_get_first_visible_file (slot->content_view);
                nautilus_bookmark_set_scroll_pos (slot->current_location_bookmark, current_pos);
                g_free (current_pos);
        }

	/* Get the info needed for view selection */
	
        slot->determine_view_file = nautilus_file_get (location);
	g_assert (slot->determine_view_file != NULL);

	/* if the currently viewed file is marked gone while loading the new location,
	 * this ensures that the window isn't destroyed */
        cancel_viewed_file_changed_callback (slot);

	nautilus_file_call_when_ready (slot->determine_view_file,
				       NAUTILUS_FILE_ATTRIBUTE_INFO |
				       NAUTILUS_FILE_ATTRIBUTE_MOUNT,
                                       got_file_info_for_view_selection_callback,
				       slot);

	nautilus_profile_end (NULL);
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

	slot->mount_cancellable = NULL;

	slot->determine_view_file = nautilus_file_get (slot->pending_location);
	
	error = NULL;
	if (!g_file_mount_enclosing_volume_finish (G_FILE (source_object), res, &error)) {
		slot->mount_error = error;
		got_file_info_for_view_selection_callback (slot->determine_view_file, slot);
		slot->mount_error = NULL;
		g_error_free (error);
	} else {
		nautilus_file_invalidate_all_attributes (slot->determine_view_file);
		nautilus_file_call_when_ready (slot->determine_view_file,
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

	g_assert (slot->determine_view_file == file);
	slot->determine_view_file = NULL;

	nautilus_profile_start (NULL);

	if (slot->mount_error) {
		error = g_error_copy (slot->mount_error);
	} else if (nautilus_file_get_file_info_error (file) != NULL) {
		error = g_error_copy (nautilus_file_get_file_info_error (file));
	}

	if (error && error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_MOUNTED &&
	    !slot->tried_mount) {
		slot->tried_mount = TRUE;

		mount_op = gtk_mount_operation_new (GTK_WINDOW (window));
		g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
		location = nautilus_file_get_location (file);
		data = g_new0 (MountNotMountedData, 1);
		data->cancellable = g_cancellable_new ();
		data->slot = slot;
		slot->mount_cancellable = data->cancellable;
		g_file_mount_enclosing_volume (location, 0, mount_op, slot->mount_cancellable,
					       mount_not_mounted_callback, data);
		g_object_unref (location);
		g_object_unref (mount_op);

		goto done;
	}

	mount = NULL;
	default_location = NULL;

	if (slot->pending_use_default_location) {
		mount = nautilus_file_get_mount (file);
		slot->pending_use_default_location = FALSE;
	}

	if (mount != NULL) {
		default_location = g_mount_get_default_location (mount);
		g_object_unref (mount);
	}

	if (default_location != NULL &&
	    !g_file_equal (slot->pending_location, default_location)) {
		g_clear_object (&slot->pending_location);
		slot->pending_location = default_location;
		slot->determine_view_file = nautilus_file_get (default_location);

		nautilus_file_invalidate_all_attributes (slot->determine_view_file);
		nautilus_file_call_when_ready (slot->determine_view_file,
					       NAUTILUS_FILE_ATTRIBUTE_INFO |
					       NAUTILUS_FILE_ATTRIBUTE_MOUNT,
					       got_file_info_for_view_selection_callback,
					       slot);

		goto done;
	}

	parent_file = nautilus_file_get_parent (file);
	if ((parent_file != NULL) &&
	    nautilus_file_get_file_type (file) == G_FILE_TYPE_REGULAR) {
		if (slot->pending_selection != NULL) {
			g_list_free_full (slot->pending_selection, (GDestroyNotify) nautilus_file_unref);
		}

		g_clear_object (&slot->pending_location);
		g_free (slot->pending_scroll_to);
	
		slot->pending_location = nautilus_file_get_parent_location (file);
		slot->pending_selection = g_list_prepend (NULL, nautilus_file_ref (file));
		slot->determine_view_file = parent_file;
		slot->pending_scroll_to = nautilus_file_get_uri (file);

		nautilus_file_invalidate_all_attributes (slot->determine_view_file);
		nautilus_file_call_when_ready (slot->determine_view_file,
					       NAUTILUS_FILE_ATTRIBUTE_INFO |
					       NAUTILUS_FILE_ATTRIBUTE_MOUNT,
					       got_file_info_for_view_selection_callback,
					       slot);		

		goto done;
	}

	nautilus_file_unref (parent_file);
	location = slot->pending_location;
	
	view_id = NULL;
	
        if (error == NULL ||
	    (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_SUPPORTED)) {
		/* We got the information we need, now pick what view to use: */

		mimetype = nautilus_file_get_mime_type (file);

		/* Try to use the existing view */
		view_id = g_strdup (nautilus_window_slot_get_content_view_id (slot));

		/* Otherwise, use default */
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
			/* Clean up state of already-showing window */
			end_location_change (slot);

			/* TODO? shouldn't we call
			 *   cancel_viewed_file_changed_callback (slot);
			 * at this point, or in end_location_change()
			 */
			/* We're missing a previous location (if opened location
			 * in a new tab) so close it and return */
			if (slot->location == NULL) {
				nautilus_window_slot_close (window, slot);
			} else {
				/* We disconnected this, so we need to re-connect it */
				viewed_file = nautilus_file_get (slot->location);
				nautilus_window_slot_set_viewed_file (slot, viewed_file);
				nautilus_file_monitor_add (viewed_file, &slot->viewed_file, 0);
				g_signal_connect_object (viewed_file, "changed",
							 G_CALLBACK (viewed_file_changed_callback), slot, 0);
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
        
        if (slot->content_view != NULL &&
	    g_strcmp0 (nautilus_view_get_view_id (slot->content_view),
			view_id) == 0) {
                /* reuse existing content view */
                view = slot->content_view;
                slot->new_content_view = view;
        	g_object_ref (view);
        } else {
                /* create a new content view */
		view = nautilus_view_factory_create (view_id, slot);

                slot->new_content_view = view;
		nautilus_window_connect_content_view (window, slot->new_content_view);
        }

	/* Forward search selection and state before loading the new model */
	new_directory = nautilus_directory_get (slot->pending_location);
	old_directory = nautilus_directory_get (slot->location);

	if (NAUTILUS_IS_SEARCH_DIRECTORY (new_directory) &&
	    !NAUTILUS_IS_SEARCH_DIRECTORY (old_directory)) {
		nautilus_search_directory_set_base_model (NAUTILUS_SEARCH_DIRECTORY (new_directory), old_directory);
	}

	if (NAUTILUS_IS_SEARCH_DIRECTORY (old_directory) &&
	    !NAUTILUS_IS_SEARCH_DIRECTORY (new_directory)) {
		/* Reset the search_visible state when going out of a search directory,
		 * before nautilus_window_slot_sync_search_widgets() is called
		 * if we're not being loaded with search visible.
		 */
		if (!slot->load_with_search) {
			slot->search_visible = FALSE;
		}

		slot->load_with_search = FALSE;

		if (slot->pending_selection == NULL) {
			slot->pending_selection = nautilus_view_get_selection (slot->content_view);
		}
	}

	/* Actually load the pending location and selection: */

        if (slot->pending_location != NULL) {
		load_new_location (slot,
				   slot->pending_location,
				   slot->pending_selection,
				   FALSE,
				   TRUE);

		g_list_free_full (slot->pending_selection, g_object_unref);
		slot->pending_selection = NULL;
	} else if (slot->location != NULL) {
		selection = nautilus_view_get_selection (slot->content_view);
		load_new_location (slot,
				   slot->location,
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

	selection_copy = eel_g_object_list_copy (selection);
	view = NULL;

	nautilus_profile_start (NULL);
	/* Note, these may recurse into report_load_underway */
        if (slot->content_view != NULL && tell_current_content_view) {
		view = slot->content_view;
		nautilus_view_load_location (slot->content_view, location);
        }
	
        if (slot->new_content_view != NULL && tell_new_content_view &&
	    (!tell_current_content_view ||
	     slot->new_content_view != slot->content_view) ) {
		view = slot->new_content_view;
		nautilus_view_load_location (slot->new_content_view, location);
        }
	if (view != NULL) {
		/* slot->new_content_view might have changed here if
		   report_load_underway was called from load_location */
		nautilus_view_set_selection (view, selection_copy);
	}

	g_list_free_full (selection_copy, g_object_unref);

	nautilus_profile_end (NULL);
}

/* A view started to load the location its viewing, either due to
 * a load_location request, or some internal reason. Expect
 * a matching load_compete later
 */
void
nautilus_window_report_load_underway (NautilusWindow *window,
				      NautilusView *view)
{
	NautilusWindowSlot *slot;

	g_assert (NAUTILUS_IS_WINDOW (window));

	nautilus_profile_start (NULL);

	if (window->details->temporarily_ignore_view_signals) {
		return;
	}

	slot = nautilus_window_get_slot_for_view (window, view);
	g_assert (slot != NULL);

	if (view == slot->new_content_view) {
		location_has_really_changed (slot);
	} else {
		nautilus_window_slot_set_allow_stop (slot, TRUE);
	}

	nautilus_profile_end (NULL);
}

static void
nautilus_window_emit_location_change (NautilusWindow *window,
				      GFile *location)
{
	char *uri;

	uri = g_file_get_uri (location);
	g_signal_emit_by_name (window, "loading_uri", uri);
	g_free (uri);
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

/* reports location change to window's "loading-uri" clients, i.e.
 * sidebar panels [used when switching tabs]. It will emit the pending
 * location, or the existing location if none is pending.
 */
void
nautilus_window_report_location_change (NautilusWindow *window)
{
	NautilusWindowSlot *slot;
	GFile *location;

	slot = nautilus_window_get_active_slot (window);
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	location = NULL;

	if (slot->pending_location != NULL) {
		location = slot->pending_location;
	}

	if (location == NULL && slot->location != NULL) {
		location = slot->location;
	}

	if (location != NULL) {
		nautilus_window_emit_location_change (window, location);
	}
}

/* This is called when we have decided we can actually change to the new view/location situation. */
static void
location_has_really_changed (NautilusWindowSlot *slot)
{
	NautilusWindow *window;
	GtkWidget *widget;
	GFile *location_copy;

	window = nautilus_window_slot_get_window (slot);

	if (slot->new_content_view != NULL) {
		widget = GTK_WIDGET (slot->new_content_view);
		/* Switch to the new content view. */
		if (gtk_widget_get_parent (widget) == NULL) {
			nautilus_window_slot_set_content_view_widget (slot, slot->new_content_view);
		}
		g_object_unref (slot->new_content_view);
		slot->new_content_view = NULL;
	}

      if (slot->pending_location != NULL) {
		/* Tell the window we are finished. */
		update_for_new_location (slot);
	}

	location_copy = NULL;
	if (slot->location != NULL) {
		location_copy = g_object_ref (slot->location);
	}

	free_location_change (slot);

	if (location_copy != NULL) {
		if (slot == nautilus_window_get_active_slot (window)) {
			nautilus_window_emit_location_change (window, location_copy);
		}

		g_object_unref (location_copy);
	}
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

	uri = g_file_get_uri (slot->location);
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
nautilus_window_slot_show_x_content_bar (NautilusWindowSlot *slot, GMount *mount, const char **x_content_types)
{
	GtkWidget *bar;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	bar = nautilus_x_content_bar_new (mount, x_content_types);
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
nautilus_window_slot_show_trash_bar (NautilusWindowSlot *slot)
{
	GtkWidget *bar;
	NautilusView *view;

	view = nautilus_window_slot_get_current_view (slot);
	bar = nautilus_trash_bar_new (view);
	gtk_widget_show (bar);

	nautilus_window_slot_add_extra_location_widget (slot, bar);
}

typedef struct {
	NautilusWindowSlot *slot;
	GCancellable *cancellable;
	GMount *mount;
} FindMountData;

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

	slot->find_mount_cancellable = NULL;

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
	
	data->slot->find_mount_cancellable = NULL;

 out:
	g_object_unref (data->cancellable);
	g_free (data);
}

/* Handle the changes for the NautilusWindow itself. */
static void
update_for_new_location (NautilusWindowSlot *slot)
{
	NautilusWindow *window;
        GFile *new_location;
        NautilusFile *file;
	NautilusDirectory *directory;
	gboolean location_really_changed;
	FindMountData *data;

	window = nautilus_window_slot_get_window (slot);
	new_location = slot->pending_location;
	slot->pending_location = NULL;

	file = nautilus_file_get (new_location);
	set_displayed_file (slot, file);

	update_history (slot, slot->location_change_type, new_location);

	location_really_changed =
		slot->location == NULL ||
		!g_file_equal (slot->location, new_location);

	nautilus_window_slot_emit_location_change (slot, slot->location, new_location);

        /* Set the new location. */
	g_clear_object (&slot->location);
	slot->location = new_location;

        /* Create a NautilusFile for this location, so we can catch it
         * if it goes away.
         */
	cancel_viewed_file_changed_callback (slot);
	nautilus_window_slot_set_viewed_file (slot, file);
	slot->viewed_file_seen = !nautilus_file_is_not_yet_confirmed (file);
	slot->viewed_file_in_trash = nautilus_file_is_in_trash (file);
	nautilus_file_monitor_add (file, &slot->viewed_file, 0);
	g_signal_connect_object (file, "changed",
				 G_CALLBACK (viewed_file_changed_callback), slot, 0);
        nautilus_file_unref (file);

	if (slot == nautilus_window_get_active_slot (window)) {
		/* Sync up and zoom action states */
		nautilus_window_sync_up_button (window);
		nautilus_window_sync_zoom_widgets (window);

		/* Sync the content view menu for this new location. */
		nautilus_window_sync_view_as_menus (window);

		/* Load menus from nautilus extensions for this location */
		nautilus_window_load_extension_menus (window);
	}

	if (location_really_changed) {
		nautilus_window_slot_remove_extra_location_widgets (slot);
		
		directory = nautilus_directory_get (slot->location);

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
			} else if (g_file_equal (slot->location, scripts_file)) {
				nautilus_window_slot_show_special_location_bar (slot, NAUTILUS_SPECIAL_LOCATION_SCRIPTS);
			}
			g_object_unref (scripts_file);
		}

		/* need the mount to determine if we should put up the x-content cluebar */
		if (slot->find_mount_cancellable != NULL) {
			g_cancellable_cancel (slot->find_mount_cancellable);
			slot->find_mount_cancellable = NULL;
		}
		
		data = g_new (FindMountData, 1);
		data->slot = slot;
		data->cancellable = g_cancellable_new ();
		data->mount = NULL;

		slot->find_mount_cancellable = data->cancellable;
		g_file_find_enclosing_mount_async (slot->location,
						   G_PRIORITY_DEFAULT, 
						   data->cancellable,
						   found_mount_cb,
						   data);

		nautilus_directory_unref (directory);

		slot_add_extension_extra_widgets (slot);
	}

	nautilus_window_slot_update_title (slot);

	if (slot == nautilus_window_get_active_slot (window)) {
		nautilus_window_sync_location_widgets (window);

		if (location_really_changed) {
			nautilus_window_sync_search_widgets (window);
		}
	}
}

/* A location load previously announced by load_underway
 * has been finished */
void
nautilus_window_report_load_complete (NautilusWindow *window,
				      NautilusView *view)
{
	NautilusWindowSlot *slot;

	g_assert (NAUTILUS_IS_WINDOW (window));

	if (window->details->temporarily_ignore_view_signals) {
		return;
	}

	slot = nautilus_window_get_slot_for_view (window, view);
	g_assert (slot != NULL);

	/* Only handle this if we're expecting it.
	 * Don't handle it if its from an old view we've switched from */
	if (view == slot->content_view) {
		if (slot->pending_scroll_to != NULL) {
			nautilus_view_scroll_to_file (slot->content_view,
						      slot->pending_scroll_to);
		}
		end_location_change (slot);
	}
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

        /* Now we can free pending_scroll_to, since the load_complete
         * callback already has been emitted.
         */
	g_free (slot->pending_scroll_to);
	slot->pending_scroll_to = NULL;

	free_location_change (slot);
}

static void
free_location_change (NautilusWindowSlot *slot)
{
	NautilusWindow *window;

	window = nautilus_window_slot_get_window (slot);

	g_clear_object (&slot->pending_location);
	g_list_free_full (slot->pending_selection, g_object_unref);
	slot->pending_selection = NULL;

        /* Don't free pending_scroll_to, since thats needed until
         * the load_complete callback.
         */

	if (slot->mount_cancellable != NULL) {
		g_cancellable_cancel (slot->mount_cancellable);
		slot->mount_cancellable = NULL;
	}

        if (slot->determine_view_file != NULL) {
		nautilus_file_cancel_call_when_ready
			(slot->determine_view_file,
			 got_file_info_for_view_selection_callback, slot);
                slot->determine_view_file = NULL;
        }

        if (slot->new_content_view != NULL) {
		window->details->temporarily_ignore_view_signals = TRUE;
		nautilus_view_stop_loading (slot->new_content_view);
		window->details->temporarily_ignore_view_signals = FALSE;

		nautilus_window_disconnect_content_view (window, slot->new_content_view);
        	g_object_unref (slot->new_content_view);
                slot->new_content_view = NULL;
        }
}

static void
cancel_location_change (NautilusWindowSlot *slot)
{
	GList *selection;
	
        if (slot->pending_location != NULL
            && slot->location != NULL
            && slot->content_view != NULL) {

                /* No need to tell the new view - either it is the
                 * same as the old view, in which case it will already
                 * be told, or it is the very pending change we wish
                 * to cancel.
                 */
		selection = nautilus_view_get_selection (slot->content_view);
                load_new_location (slot,
				   slot->location,
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
nautilus_window_slot_stop_loading (NautilusWindowSlot *slot)
{
	NautilusWindow *window;

	window = nautilus_window_slot_get_window (slot);

	nautilus_view_stop_loading (slot->content_view);
	
	if (slot->new_content_view != NULL) {
		window->details->temporarily_ignore_view_signals = TRUE;
		nautilus_view_stop_loading (slot->new_content_view);
		window->details->temporarily_ignore_view_signals = FALSE;
	}

        cancel_location_change (slot);
}

void
nautilus_window_slot_set_content_view (NautilusWindowSlot *slot,
				       const char *id)
{
	char *uri;

	g_assert (slot != NULL);
	g_assert (slot->location != NULL);
	g_assert (id != NULL);

	uri = nautilus_window_slot_get_location_uri (slot);
	DEBUG ("Change view of window %s to %s", uri, id);
	g_free (uri);

	if (nautilus_window_slot_content_view_matches_iid (slot, id)) {
        	return;
        }

        end_location_change (slot);

        nautilus_window_slot_set_allow_stop (slot, TRUE);

        if (nautilus_view_get_selection_count (slot->content_view) == 0) {
                /* If there is no selection, queue a scroll to the same icon that
                 * is currently visible */
                slot->pending_scroll_to = nautilus_view_get_first_visible_file (slot->content_view);
        }
	slot->location_change_type = NAUTILUS_LOCATION_CHANGE_RELOAD;
	
        if (!create_content_view (slot, id, NULL)) {
		/* Just load the homedir. */
		nautilus_window_slot_go_home (slot, FALSE);
	}
}

void
nautilus_window_manage_views_close_slot (NautilusWindowSlot *slot)
{
	if (slot->content_view != NULL) {
		nautilus_window_disconnect_content_view (nautilus_window_slot_get_window (slot), 
							 slot->content_view);
	}

	free_location_change (slot);
	cancel_viewed_file_changed_callback (slot);
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
	list = back ? slot->back_list : slot->forward_list;

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

		g_clear_object (&old_location);
		g_free (scroll_pos);
	}

	g_object_unref (location);
}

/* reload the contents of the window */
void
nautilus_window_slot_force_reload (NautilusWindowSlot *slot)
{
	GFile *location;
        char *current_pos;
	GList *selection;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	if (slot->location == NULL) {
		return;
	}

	/* peek_slot_field (window, location) can be free'd during the processing
	 * of begin_location_change, so make a copy
	 */
	location = g_object_ref (slot->location);
	current_pos = NULL;
	selection = NULL;
	if (slot->content_view != NULL) {
		current_pos = nautilus_view_get_first_visible_file (slot->content_view);
		selection = nautilus_view_get_selection (slot->content_view);
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

	if (slot->location == NULL) {
		return;
	}

	if (slot->pending_location != NULL
	    || slot->content_view == NULL
	    || nautilus_view_get_loading (slot->content_view)) {
		/* there is a reload in flight */
		slot->needs_reload = TRUE;
		return;
	}

	nautilus_window_slot_force_reload (slot);
}

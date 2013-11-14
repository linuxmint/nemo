/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2005 Red Hat, Inc.
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 * Author: John Sullivan <sullivan@eazel.com>
 *         Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include <locale.h> 

#include "nemo-actions.h"
#include "nemo-bookmark-list.h"
#include "nemo-bookmarks-window.h"
#include "nemo-window-bookmarks.h"
#include "nemo-window-private.h"
#include <libnemo-private/nemo-ui-utilities.h>
#include <eel/eel-debug.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <glib/gi18n.h>

#define MENU_ITEM_MAX_WIDTH_CHARS 32
#define MENU_PATH_BOOKMARKS_PLACEHOLDER	 "/MenuBar/Other Menus/Bookmarks/Bookmarks Placeholder"

static GtkWindow *bookmarks_window = NULL;

static void refresh_bookmarks_menu (NemoWindow *window);

static void
remove_bookmarks_for_uri_if_yes (GtkDialog *dialog, int response, gpointer callback_data)
{
	const char *uri;
	NemoWindow *window;

	g_assert (GTK_IS_DIALOG (dialog));
	g_assert (callback_data != NULL);

	window = callback_data;

	if (response == GTK_RESPONSE_YES) {
		uri = g_object_get_data (G_OBJECT (dialog), "uri");
		nemo_bookmark_list_delete_items_with_uri (window->details->bookmark_list, uri);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
show_bogus_bookmark_window (NemoWindow *window,
			    NemoBookmark *bookmark)
{
	GtkDialog *dialog;
	GFile *location;
	char *uri_for_display;
	char *prompt;
	char *detail;

	location = nemo_bookmark_get_location (bookmark);
	uri_for_display = g_file_get_parse_name (location);
	
	prompt = _("Do you want to remove any bookmarks with the "
		   "non-existing location from your list?");
	detail = g_strdup_printf (_("The location \"%s\" does not exist."), uri_for_display);
	
	dialog = eel_show_yes_no_dialog (prompt, detail,
					 _("Bookmark for Nonexistent Location"),
					 GTK_STOCK_CANCEL,
					 GTK_WINDOW (window));

	g_signal_connect (dialog, "response",
	                  G_CALLBACK (remove_bookmarks_for_uri_if_yes), window);
	g_object_set_data_full (G_OBJECT (dialog), "uri", g_file_get_uri (location), g_free);

	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_NO);

	g_object_unref (location);
	g_free (uri_for_display);
	g_free (detail);
}

static GtkWindow *
get_or_create_bookmarks_window (NemoWindow *window)
{
	GObject *undo_manager_source;

	undo_manager_source = G_OBJECT (window);

	if (bookmarks_window == NULL) {
		bookmarks_window = create_bookmarks_window (window->details->bookmark_list,
		                                            undo_manager_source);
	} else {
		edit_bookmarks_dialog_set_signals (undo_manager_source);
	}

	return bookmarks_window;
}

/**
 * nemo_bookmarks_exiting:
 * 
 * Last chance to save state before app exits.
 * Called when application exits; don't call from anywhere else.
 **/
void
nemo_bookmarks_exiting (void)
{
	if (bookmarks_window != NULL) {
		nemo_bookmarks_window_save_geometry (bookmarks_window);
		gtk_widget_destroy (GTK_WIDGET (bookmarks_window));
	}
}

/**
 * add_bookmark_for_current_location
 * 
 * Add a bookmark for the displayed location to the bookmarks menu.
 * Does nothing if there's already a bookmark for the displayed location.
 */
void
nemo_window_add_bookmark_for_current_location (NemoWindow *window)
{
	NemoBookmark *bookmark;
	NemoWindowSlot *slot;
	NemoBookmarkList *list;

	slot = nemo_window_get_active_slot (window);
	bookmark = slot->current_location_bookmark;
	list = window->details->bookmark_list;

	if (!nemo_bookmark_list_contains (list, bookmark)) {
		nemo_bookmark_list_append (list, bookmark); 
	}
}

void
nemo_window_edit_bookmarks (NemoWindow *window)
{
	GtkWindow *dialog;

	dialog = get_or_create_bookmarks_window (window);

	gtk_window_set_screen (
		dialog, gtk_window_get_screen (GTK_WINDOW (window)));
        gtk_window_present (dialog);
}

static void
remove_bookmarks_menu_items (NemoWindow *window)
{
	GtkUIManager *ui_manager;
	
	ui_manager = nemo_window_get_ui_manager (window);
	if (window->details->bookmarks_merge_id != 0) {
		gtk_ui_manager_remove_ui (ui_manager,
					  window->details->bookmarks_merge_id);
		window->details->bookmarks_merge_id = 0;
	}
	if (window->details->bookmarks_action_group != NULL) {
		gtk_ui_manager_remove_action_group (ui_manager,
						    window->details->bookmarks_action_group);
		window->details->bookmarks_action_group = NULL;
	}
}

static void
connect_proxy_cb (GtkActionGroup *action_group,
                  GtkAction *action,
                  GtkWidget *proxy,
                  gpointer dummy)
{
	GtkLabel *label;
	GIcon *icon;

	if (!GTK_IS_MENU_ITEM (proxy))
		return;

	label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (proxy)));

	gtk_label_set_use_underline (label, FALSE);
	gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (label, MENU_ITEM_MAX_WIDTH_CHARS);

	icon = g_object_get_data (G_OBJECT (action), "menu-icon");

	if (icon != NULL) {
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (proxy),
					       gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU));
	}
}

/* Struct that stores all the info necessary to activate a bookmark. */
typedef struct {
        NemoBookmark *bookmark;
        NemoWindow *window;
	GCallback refresh_callback;
	NemoBookmarkFailedCallback failed_callback;
} BookmarkHolder;

static BookmarkHolder *
bookmark_holder_new (NemoBookmark *bookmark, 
		     NemoWindow *window,
		     GCallback refresh_callback,
		     NemoBookmarkFailedCallback failed_callback)
{
	BookmarkHolder *new_bookmark_holder;

	new_bookmark_holder = g_new (BookmarkHolder, 1);
	new_bookmark_holder->window = window;
	new_bookmark_holder->bookmark = bookmark;
	new_bookmark_holder->failed_callback = failed_callback;
	new_bookmark_holder->refresh_callback = refresh_callback;
	/* Ref the bookmark because it might be unreffed away while 
	 * we're holding onto it (not an issue for window).
	 */
	g_object_ref (bookmark);
	g_signal_connect_object (bookmark, "notify::icon",
				 refresh_callback,
				 window, G_CONNECT_SWAPPED);
	g_signal_connect_object (bookmark, "notify::name",
				 refresh_callback,
				 window, G_CONNECT_SWAPPED);

	return new_bookmark_holder;
}

static void
bookmark_holder_free (BookmarkHolder *bookmark_holder)
{
	g_signal_handlers_disconnect_by_func (bookmark_holder->bookmark,
					      bookmark_holder->refresh_callback, bookmark_holder->window);
	g_object_unref (bookmark_holder->bookmark);
	g_free (bookmark_holder);
}

static void
bookmark_holder_free_cover (gpointer callback_data, GClosure *closure)
{
	bookmark_holder_free (callback_data);
}

static void
activate_bookmark_in_menu_item (GtkAction *action, gpointer user_data)
{
	NemoWindowSlot *slot;
        BookmarkHolder *holder;
        GFile *location;

        holder = (BookmarkHolder *)user_data;

	if (nemo_bookmark_uri_known_not_to_exist (holder->bookmark)) {
		holder->failed_callback (holder->window, holder->bookmark);
	} else {
	        location = nemo_bookmark_get_location (holder->bookmark);
		slot = nemo_window_get_active_slot (holder->window);
	        nemo_window_slot_open_location (slot, location, 
						    nemo_event_get_window_open_flags ());
	        g_object_unref (location);
        }
}

void
nemo_menus_append_bookmark_to_menu (NemoWindow *window, 
					NemoBookmark *bookmark, 
					const char *parent_path,
					const char *parent_id,
					guint index_in_parent,
					GtkActionGroup *action_group,
					guint merge_id,
					GCallback refresh_callback,
					NemoBookmarkFailedCallback failed_callback)
{
	BookmarkHolder *bookmark_holder;
	char action_name[128];
	const char *name;
	char *path;
	GIcon *icon;
	GtkAction *action;
	GtkWidget *menuitem;

	g_assert (NEMO_IS_WINDOW (window));
	g_assert (NEMO_IS_BOOKMARK (bookmark));

	bookmark_holder = bookmark_holder_new (bookmark, window, refresh_callback, failed_callback);
	name = nemo_bookmark_get_name (bookmark);

	/* Create menu item with pixbuf */
	icon = nemo_bookmark_get_icon (bookmark);

	g_snprintf (action_name, sizeof (action_name), "%s%d", parent_id, index_in_parent);

	action = gtk_action_new (action_name,
				 name,
				 _("Go to the location specified by this bookmark"),
				 NULL);
	
	g_object_set_data_full (G_OBJECT (action), "menu-icon",
				icon,
				g_object_unref);

	g_signal_connect_data (action, "activate",
			       G_CALLBACK (activate_bookmark_in_menu_item),
			       bookmark_holder, 
			       bookmark_holder_free_cover, 0);

	gtk_action_group_add_action (action_group,
				     GTK_ACTION (action));

	g_object_unref (action);

	gtk_ui_manager_add_ui (window->details->ui_manager,
			       merge_id,
			       parent_path,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	path = g_strdup_printf ("%s/%s", parent_path, action_name);
	menuitem = gtk_ui_manager_get_widget (window->details->ui_manager,
					      path);
	gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (menuitem),
						   TRUE);

	g_free (path);
}

static void
update_bookmarks (NemoWindow *window)
{
        NemoBookmarkList *bookmarks;
	NemoBookmark *bookmark;
	guint bookmark_count;
	guint index;
	GtkUIManager *ui_manager;

	g_assert (NEMO_IS_WINDOW (window));
	g_assert (window->details->bookmarks_merge_id == 0);
	g_assert (window->details->bookmarks_action_group == NULL);

	if (window->details->bookmark_list == NULL) {
		window->details->bookmark_list = nemo_bookmark_list_new ();
	}

	bookmarks = window->details->bookmark_list;

	ui_manager = nemo_window_get_ui_manager (NEMO_WINDOW (window));
	
	window->details->bookmarks_merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	window->details->bookmarks_action_group = gtk_action_group_new ("BookmarksGroup");
	g_signal_connect (window->details->bookmarks_action_group, "connect-proxy",
			  G_CALLBACK (connect_proxy_cb), NULL);

	gtk_ui_manager_insert_action_group (ui_manager,
					    window->details->bookmarks_action_group,
					    -1);
	g_object_unref (window->details->bookmarks_action_group);

	/* append new set of bookmarks */
	bookmark_count = nemo_bookmark_list_length (bookmarks);
	for (index = 0; index < bookmark_count; ++index) {
		bookmark = nemo_bookmark_list_item_at (bookmarks, index);

		if (nemo_bookmark_uri_known_not_to_exist (bookmark)) {
			continue;
		}

		nemo_menus_append_bookmark_to_menu
			(NEMO_WINDOW (window),
			 bookmark,
			 MENU_PATH_BOOKMARKS_PLACEHOLDER,
			 "dynamic",
			 index,
			 window->details->bookmarks_action_group,
			 window->details->bookmarks_merge_id,
			 G_CALLBACK (refresh_bookmarks_menu), 
			 show_bogus_bookmark_window);
	}
}

static void
refresh_bookmarks_menu (NemoWindow *window)
{
	g_assert (NEMO_IS_WINDOW (window));

	remove_bookmarks_menu_items (window);
	update_bookmarks (window);
}

/**
 * nemo_window_initialize_bookmarks_menu
 * 
 * Fill in bookmarks menu with stored bookmarks, and wire up signals
 * so we'll be notified when bookmark list changes.
 */
void 
nemo_window_initialize_bookmarks_menu (NemoWindow *window)
{
	g_assert (NEMO_IS_WINDOW (window));

	refresh_bookmarks_menu (window);

	/* Recreate dynamic part of menu if bookmark list changes */
	g_signal_connect_object (window->details->bookmark_list, "changed",
				 G_CALLBACK (refresh_bookmarks_menu),
				 window, G_CONNECT_SWAPPED);
}

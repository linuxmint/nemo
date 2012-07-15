/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2005 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: John Sullivan <sullivan@eazel.com>
 *         Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include <locale.h> 

#include "nautilus-actions.h"
#include "nautilus-bookmark-list.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-window-bookmarks.h"
#include "nautilus-window-private.h"
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <eel/eel-debug.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <glib/gi18n.h>

#define MENU_ITEM_MAX_WIDTH_CHARS 32

static GtkWindow *bookmarks_window = NULL;

static void refresh_bookmarks_menu (NautilusWindow *window);

/**
 * add_bookmark_for_current_location
 * 
 * Add a bookmark for the displayed location to the bookmarks menu.
 * Does nothing if there's already a bookmark for the displayed location.
 */
void
nautilus_window_add_bookmark_for_current_location (NautilusWindow *window)
{
	NautilusBookmark *bookmark;
	NautilusWindowSlot *slot;
	NautilusBookmarkList *list;

	slot = nautilus_window_get_active_slot (window);
	bookmark = slot->current_location_bookmark;
	list = window->details->bookmark_list;

	if (!nautilus_bookmark_list_contains (list, bookmark)) {
		nautilus_bookmark_list_append (list, bookmark); 
	}
}

void
nautilus_window_edit_bookmarks (NautilusWindow *window)
{
	if (bookmarks_window == NULL) {
		bookmarks_window = nautilus_bookmarks_window_new (window, window->details->bookmark_list);
		g_object_add_weak_pointer (G_OBJECT (bookmarks_window), (gpointer *) &bookmarks_window);
	}

	gtk_window_set_transient_for (bookmarks_window, GTK_WINDOW (window));
	gtk_window_set_screen (GTK_WINDOW (bookmarks_window), gtk_window_get_screen (GTK_WINDOW (window)));
        gtk_window_present (bookmarks_window);
}

static void
remove_bookmarks_menu_items (NautilusWindow *window)
{
	GtkUIManager *ui_manager;
	
	ui_manager = nautilus_window_get_ui_manager (window);
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
update_bookmarks (NautilusWindow *window)
{
	GtkUIManager *ui_manager;

	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (window->details->bookmarks_merge_id == 0);
	g_assert (window->details->bookmarks_action_group == NULL);

	if (window->details->bookmark_list == NULL) {
		window->details->bookmark_list = nautilus_bookmark_list_new ();
	}

	ui_manager = nautilus_window_get_ui_manager (NAUTILUS_WINDOW (window));
	
	window->details->bookmarks_merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	window->details->bookmarks_action_group = gtk_action_group_new ("BookmarksGroup");

	gtk_ui_manager_insert_action_group (ui_manager,
					    window->details->bookmarks_action_group,
					    -1);
	g_object_unref (window->details->bookmarks_action_group);
}

static void
refresh_bookmarks_menu (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	remove_bookmarks_menu_items (window);
	update_bookmarks (window);
}

/**
 * nautilus_window_initialize_bookmarks_menu
 * 
 * Fill in bookmarks menu with stored bookmarks, and wire up signals
 * so we'll be notified when bookmark list changes.
 */
void 
nautilus_window_initialize_bookmarks_menu (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	refresh_bookmarks_menu (window);
}

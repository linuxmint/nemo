/*unity-bookmarks-handler.c: handle Unity bookmark for quicklist
 *
 * Copyright (C) 2012 Canonical
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Didier Roche <didrocks@ubuntu.com>
 *
 */

#include <config.h>

#include "unity-bookmarks-handler.h"

#include <libdbusmenu-glib/dbusmenu-glib.h>
#include "unity-quicklist-handler.h"

#include "nemo-application.h"
#include "nemo-window-private.h"

#include <eel/eel-string.h>

static UnityQuicklistHandler* unity_quicklist_handler = NULL;
static NemoBookmarkList* bookmarks = NULL;

static void
activate_bookmark_by_quicklist (DbusmenuMenuitem *menu,
								guint timestamp,
								NemoBookmark *bookmark)
{
	g_assert (NEMO_IS_BOOKMARK (bookmark));

	GFile *location;
	NemoApplication *application;
	NemoWindow *new_window;

	location = nemo_bookmark_get_location (bookmark);

	application = nemo_application_get_singleton ();
	new_window = nemo_application_create_window (application, gdk_screen_get_default ());
	nemo_window_slot_go_to (nemo_window_get_active_slot (new_window), location, FALSE);

	g_object_unref (location);
}

static void
unity_bookmarks_handler_remove_bookmark_quicklists () {

	GList *children, *l;

	/* remove unity quicklist bookmarks to launcher entries */
	for (l = unity_quicklist_get_launcher_entries (unity_quicklist_handler); l; l = l->next) {
		UnityLauncherEntry *entry = l->data;
		DbusmenuMenuitem *ql = unity_launcher_entry_get_quicklist (entry);
		if (!ql)
			break;

		children = dbusmenu_menuitem_get_children (ql);
		while (children) {
			DbusmenuMenuitem *child = children->data;
			children = children->next;
			if (unity_quicklist_handler_menuitem_is_bookmark_item (child)) {
				g_signal_handlers_disconnect_matched (child, G_SIGNAL_MATCH_FUNC, 0, 0, 0, (GCallback) activate_bookmark_by_quicklist, 0);
				dbusmenu_menuitem_child_delete (ql, child);
				g_object_unref(child);
			}
		}
	}
}

static void
unity_bookmarks_handler_update_bookmarks () {

	NemoBookmark *bookmark;
	guint bookmark_count;
	guint index;
	GList *l;

	/* append new set of bookmarks */
	bookmark_count = nemo_bookmark_list_length (bookmarks);
	for (index = 0; index < bookmark_count; ++index) {

		bookmark = nemo_bookmark_list_item_at (bookmarks, index);

		if (nemo_bookmark_uri_known_not_to_exist (bookmark)) {
			continue;
		}

		for (l = unity_quicklist_get_launcher_entries (unity_quicklist_handler); l; l = l->next) {
			UnityLauncherEntry *entry = l->data;

			DbusmenuMenuitem* menuitem = dbusmenu_menuitem_new();
			gchar *bookmark_name_dbusmenu = eel_str_replace_substring (nemo_bookmark_get_name (bookmark), "_", "__");
			dbusmenu_menuitem_property_set (menuitem, "label", bookmark_name_dbusmenu);
			g_free (bookmark_name_dbusmenu);
			g_signal_connect (menuitem, DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED,
										 (GCallback) activate_bookmark_by_quicklist,
										 bookmark);

			unity_quicklist_handler_append_menuitem (entry, menuitem);
		}
	}
}

static void
unity_bookmarks_handler_refresh_bookmarks ()
{
	unity_bookmarks_handler_remove_bookmark_quicklists ();
	unity_bookmarks_handler_update_bookmarks ();
}

void
unity_bookmarks_handler_initialize ()
{
	unity_quicklist_handler = unity_quicklist_handler_get_singleton ();
	// get the singleton
	bookmarks = nemo_bookmark_list_new ();
	unity_bookmarks_handler_refresh_bookmarks ();

    /* Recreate dynamic part of menu if bookmark list changes */
	g_signal_connect (bookmarks, "changed",
						G_CALLBACK (unity_bookmarks_handler_refresh_bookmarks), 0);
}


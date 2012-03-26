/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-empty-view.c - implementation of empty view of directory.

   Copyright (C) 2006 Free Software Foundation, Inc.
   
   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Christian Neumair <chris@gnome-de.org>
*/

#include <config.h>

#include "nautilus-empty-view.h"

#include "nautilus-view.h"
#include "nautilus-view-factory.h"

#include <string.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <eel/eel-vfs-extensions.h>

struct NautilusEmptyViewDetails {
	int number_of_files;
};

static GList *nautilus_empty_view_get_selection                   (NautilusView   *view);
static GList *nautilus_empty_view_get_selection_for_file_transfer (NautilusView   *view);
static void   nautilus_empty_view_scroll_to_file                  (NautilusView      *view,
								   const char        *uri);

G_DEFINE_TYPE (NautilusEmptyView, nautilus_empty_view, NAUTILUS_TYPE_VIEW)

static void
nautilus_empty_view_add_file (NautilusView *view, NautilusFile *file, NautilusDirectory *directory)
{
	static GTimer *timer = NULL;
	static gdouble cumu = 0, elaps;
	NAUTILUS_EMPTY_VIEW (view)->details->number_of_files++;
	GdkPixbuf *icon;

	if (!timer) timer = g_timer_new ();

	g_timer_start (timer);
	icon = nautilus_file_get_icon_pixbuf (file, nautilus_get_icon_size_for_zoom_level (NAUTILUS_ZOOM_LEVEL_STANDARD), TRUE, 0);

	elaps = g_timer_elapsed (timer, NULL);
	g_timer_stop (timer);

	g_object_unref (icon);
	
	cumu += elaps;
	g_message ("entire loading: %.3f, cumulative %.3f", elaps, cumu);
}


static void
nautilus_empty_view_begin_loading (NautilusView *view)
{
}

static void
nautilus_empty_view_clear (NautilusView *view)
{
}


static void
nautilus_empty_view_file_changed (NautilusView *view, NautilusFile *file, NautilusDirectory *directory)
{
}

static GList *
nautilus_empty_view_get_selection (NautilusView *view)
{
	return NULL;
}


static GList *
nautilus_empty_view_get_selection_for_file_transfer (NautilusView *view)
{
	return NULL;
}

static guint
nautilus_empty_view_get_item_count (NautilusView *view)
{
	return NAUTILUS_EMPTY_VIEW (view)->details->number_of_files;
}

static gboolean
nautilus_empty_view_is_empty (NautilusView *view)
{
	return NAUTILUS_EMPTY_VIEW (view)->details->number_of_files == 0;
}

static void
nautilus_empty_view_end_file_changes (NautilusView *view)
{
}

static void
nautilus_empty_view_remove_file (NautilusView *view, NautilusFile *file, NautilusDirectory *directory)
{
	NAUTILUS_EMPTY_VIEW (view)->details->number_of_files--;
	g_assert (NAUTILUS_EMPTY_VIEW (view)->details->number_of_files >= 0);
}

static void
nautilus_empty_view_set_selection (NautilusView *view, GList *selection)
{
	nautilus_view_notify_selection_changed (view);
}

static void
nautilus_empty_view_select_all (NautilusView *view)
{
}

static void
nautilus_empty_view_reveal_selection (NautilusView *view)
{
}

static void
nautilus_empty_view_merge_menus (NautilusView *view)
{
	NAUTILUS_VIEW_CLASS (nautilus_empty_view_parent_class)->merge_menus (view);
}

static void
nautilus_empty_view_update_menus (NautilusView *view)
{
	NAUTILUS_VIEW_CLASS (nautilus_empty_view_parent_class)->update_menus (view);
}

/* Reset sort criteria and zoom level to match defaults */
static void
nautilus_empty_view_reset_to_defaults (NautilusView *view)
{
}

static void
nautilus_empty_view_bump_zoom_level (NautilusView *view, int zoom_increment)
{
}

static NautilusZoomLevel
nautilus_empty_view_get_zoom_level (NautilusView *view)
{
	return NAUTILUS_ZOOM_LEVEL_STANDARD;
}

static void
nautilus_empty_view_zoom_to_level (NautilusView *view,
			    NautilusZoomLevel zoom_level)
{
}

static void
nautilus_empty_view_restore_default_zoom_level (NautilusView *view)
{
}

static gboolean 
nautilus_empty_view_can_zoom_in (NautilusView *view) 
{
	return FALSE;
}

static gboolean 
nautilus_empty_view_can_zoom_out (NautilusView *view) 
{
	return FALSE;
}

static void
nautilus_empty_view_start_renaming_file (NautilusView *view,
				  NautilusFile *file,
				  gboolean select_all)
{
}

static void
nautilus_empty_view_click_policy_changed (NautilusView *directory_view)
{
}


static int
nautilus_empty_view_compare_files (NautilusView *view, NautilusFile *file1, NautilusFile *file2)
{
	if (file1 < file2) {
		return -1;
	}

	if (file1 > file2) {
		return +1;
	}

	return 0;
}

static gboolean
nautilus_empty_view_using_manual_layout (NautilusView *view)
{
	return FALSE;
}

static void
nautilus_empty_view_end_loading (NautilusView *view,
			   gboolean all_files_seen)
{
}

static char *
nautilus_empty_view_get_first_visible_file (NautilusView *view)
{
	return NULL;
}

static void
nautilus_empty_view_scroll_to_file (NautilusView *view,
			      const char *uri)
{
}

static void
nautilus_empty_view_sort_directories_first_changed (NautilusView *view)
{
}

static const char *
nautilus_empty_view_get_id (NautilusView *view)
{
	return NAUTILUS_EMPTY_VIEW_ID;
}

static void
nautilus_empty_view_class_init (NautilusEmptyViewClass *class)
{
	NautilusViewClass *nautilus_view_class;

	g_type_class_add_private (class, sizeof (NautilusEmptyViewDetails));

	nautilus_view_class = NAUTILUS_VIEW_CLASS (class);

	nautilus_view_class->add_file = nautilus_empty_view_add_file;
	nautilus_view_class->begin_loading = nautilus_empty_view_begin_loading;
	nautilus_view_class->bump_zoom_level = nautilus_empty_view_bump_zoom_level;
	nautilus_view_class->can_zoom_in = nautilus_empty_view_can_zoom_in;
	nautilus_view_class->can_zoom_out = nautilus_empty_view_can_zoom_out;
        nautilus_view_class->click_policy_changed = nautilus_empty_view_click_policy_changed;
	nautilus_view_class->clear = nautilus_empty_view_clear;
	nautilus_view_class->file_changed = nautilus_empty_view_file_changed;
	nautilus_view_class->get_selection = nautilus_empty_view_get_selection;
	nautilus_view_class->get_selection_for_file_transfer = nautilus_empty_view_get_selection_for_file_transfer;
	nautilus_view_class->get_item_count = nautilus_empty_view_get_item_count;
	nautilus_view_class->is_empty = nautilus_empty_view_is_empty;
	nautilus_view_class->remove_file = nautilus_empty_view_remove_file;
	nautilus_view_class->merge_menus = nautilus_empty_view_merge_menus;
	nautilus_view_class->update_menus = nautilus_empty_view_update_menus;
	nautilus_view_class->reset_to_defaults = nautilus_empty_view_reset_to_defaults;
	nautilus_view_class->restore_default_zoom_level = nautilus_empty_view_restore_default_zoom_level;
	nautilus_view_class->reveal_selection = nautilus_empty_view_reveal_selection;
	nautilus_view_class->select_all = nautilus_empty_view_select_all;
	nautilus_view_class->set_selection = nautilus_empty_view_set_selection;
	nautilus_view_class->compare_files = nautilus_empty_view_compare_files;
	nautilus_view_class->sort_directories_first_changed = nautilus_empty_view_sort_directories_first_changed;
	nautilus_view_class->start_renaming_file = nautilus_empty_view_start_renaming_file;
	nautilus_view_class->get_zoom_level = nautilus_empty_view_get_zoom_level;
	nautilus_view_class->zoom_to_level = nautilus_empty_view_zoom_to_level;
	nautilus_view_class->end_file_changes = nautilus_empty_view_end_file_changes;
	nautilus_view_class->using_manual_layout = nautilus_empty_view_using_manual_layout;
	nautilus_view_class->end_loading = nautilus_empty_view_end_loading;
	nautilus_view_class->get_view_id = nautilus_empty_view_get_id;
	nautilus_view_class->get_first_visible_file = nautilus_empty_view_get_first_visible_file;
	nautilus_view_class->scroll_to_file = nautilus_empty_view_scroll_to_file;
}

static void
nautilus_empty_view_init (NautilusEmptyView *empty_view)
{
	empty_view->details = G_TYPE_INSTANCE_GET_PRIVATE (empty_view, NAUTILUS_TYPE_EMPTY_VIEW,
							   NautilusEmptyViewDetails);
}

static NautilusView *
nautilus_empty_view_create (NautilusWindowSlot *slot)
{
	NautilusEmptyView *view;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	view = g_object_new (NAUTILUS_TYPE_EMPTY_VIEW,
			     "window-slot", slot,
			     NULL);

	return NAUTILUS_VIEW (view);
}

static gboolean
nautilus_empty_view_supports_uri (const char *uri,
				  GFileType file_type,
				  const char *mime_type)
{
	if (file_type == G_FILE_TYPE_DIRECTORY) {
		return TRUE;
	}
	if (strcmp (mime_type, NAUTILUS_SAVED_SEARCH_MIMETYPE) == 0){
		return TRUE;
	}
	if (g_str_has_prefix (uri, "trash:")) {
		return TRUE;
	}
	if (g_str_has_prefix (uri, EEL_SEARCH_URI)) {
		return TRUE;
	}

	return FALSE;
}

static NautilusViewInfo nautilus_empty_view = {
	NAUTILUS_EMPTY_VIEW_ID,
	"Empty",
	"Empty View",
	"_Empty View",
	"The empty view encountered an error.",
	"Display this location with the empty view.",
	nautilus_empty_view_create,
	nautilus_empty_view_supports_uri
};

void
nautilus_empty_view_register (void)
{
	nautilus_empty_view.id = nautilus_empty_view.id;
	nautilus_empty_view.view_combo_label = nautilus_empty_view.view_combo_label;
	nautilus_empty_view.view_menu_label_with_mnemonic = nautilus_empty_view.view_menu_label_with_mnemonic;
	nautilus_empty_view.error_label = nautilus_empty_view.error_label;
	nautilus_empty_view.display_location_label = nautilus_empty_view.display_location_label;

	nautilus_view_factory_register (&nautilus_empty_view);
}

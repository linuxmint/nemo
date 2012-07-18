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

#include "nemo-empty-view.h"

#include "nemo-view.h"
#include "nemo-view-factory.h"

#include <string.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <eel/eel-vfs-extensions.h>

struct NemoEmptyViewDetails {
	int number_of_files;
};

static GList *nemo_empty_view_get_selection                   (NemoView   *view);
static GList *nemo_empty_view_get_selection_for_file_transfer (NemoView   *view);
static void   nemo_empty_view_scroll_to_file                  (NemoView      *view,
								   const char        *uri);

G_DEFINE_TYPE (NemoEmptyView, nemo_empty_view, NEMO_TYPE_VIEW)

static void
nemo_empty_view_add_file (NemoView *view, NemoFile *file, NemoDirectory *directory)
{
	static GTimer *timer = NULL;
	static gdouble cumu = 0, elaps;
	NEMO_EMPTY_VIEW (view)->details->number_of_files++;
	GdkPixbuf *icon;

	if (!timer) timer = g_timer_new ();

	g_timer_start (timer);
	icon = nemo_file_get_icon_pixbuf (file, nemo_get_icon_size_for_zoom_level (NEMO_ZOOM_LEVEL_STANDARD), TRUE, 0);

	elaps = g_timer_elapsed (timer, NULL);
	g_timer_stop (timer);

	g_object_unref (icon);
	
	cumu += elaps;
	g_message ("entire loading: %.3f, cumulative %.3f", elaps, cumu);
}


static void
nemo_empty_view_begin_loading (NemoView *view)
{
}

static void
nemo_empty_view_clear (NemoView *view)
{
}


static void
nemo_empty_view_file_changed (NemoView *view, NemoFile *file, NemoDirectory *directory)
{
}

static GList *
nemo_empty_view_get_selection (NemoView *view)
{
	return NULL;
}


static GList *
nemo_empty_view_get_selection_for_file_transfer (NemoView *view)
{
	return NULL;
}

static guint
nemo_empty_view_get_item_count (NemoView *view)
{
	return NEMO_EMPTY_VIEW (view)->details->number_of_files;
}

static gboolean
nemo_empty_view_is_empty (NemoView *view)
{
	return NEMO_EMPTY_VIEW (view)->details->number_of_files == 0;
}

static void
nemo_empty_view_end_file_changes (NemoView *view)
{
}

static void
nemo_empty_view_remove_file (NemoView *view, NemoFile *file, NemoDirectory *directory)
{
	NEMO_EMPTY_VIEW (view)->details->number_of_files--;
	g_assert (NEMO_EMPTY_VIEW (view)->details->number_of_files >= 0);
}

static void
nemo_empty_view_set_selection (NemoView *view, GList *selection)
{
	nemo_view_notify_selection_changed (view);
}

static void
nemo_empty_view_select_all (NemoView *view)
{
}

static void
nemo_empty_view_reveal_selection (NemoView *view)
{
}

static void
nemo_empty_view_merge_menus (NemoView *view)
{
	NEMO_VIEW_CLASS (nemo_empty_view_parent_class)->merge_menus (view);
}

static void
nemo_empty_view_update_menus (NemoView *view)
{
	NEMO_VIEW_CLASS (nemo_empty_view_parent_class)->update_menus (view);
}

/* Reset sort criteria and zoom level to match defaults */
static void
nemo_empty_view_reset_to_defaults (NemoView *view)
{
}

static void
nemo_empty_view_bump_zoom_level (NemoView *view, int zoom_increment)
{
}

static NemoZoomLevel
nemo_empty_view_get_zoom_level (NemoView *view)
{
	return NEMO_ZOOM_LEVEL_STANDARD;
}

static void
nemo_empty_view_zoom_to_level (NemoView *view,
			    NemoZoomLevel zoom_level)
{
}

static void
nemo_empty_view_restore_default_zoom_level (NemoView *view)
{
}

static gboolean 
nemo_empty_view_can_zoom_in (NemoView *view) 
{
	return FALSE;
}

static gboolean 
nemo_empty_view_can_zoom_out (NemoView *view) 
{
	return FALSE;
}

static void
nemo_empty_view_start_renaming_file (NemoView *view,
				  NemoFile *file,
				  gboolean select_all)
{
}

static void
nemo_empty_view_click_policy_changed (NemoView *directory_view)
{
}


static int
nemo_empty_view_compare_files (NemoView *view, NemoFile *file1, NemoFile *file2)
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
nemo_empty_view_using_manual_layout (NemoView *view)
{
	return FALSE;
}

static void
nemo_empty_view_end_loading (NemoView *view,
			   gboolean all_files_seen)
{
}

static char *
nemo_empty_view_get_first_visible_file (NemoView *view)
{
	return NULL;
}

static void
nemo_empty_view_scroll_to_file (NemoView *view,
			      const char *uri)
{
}

static void
nemo_empty_view_sort_directories_first_changed (NemoView *view)
{
}

static const char *
nemo_empty_view_get_id (NemoView *view)
{
	return NEMO_EMPTY_VIEW_ID;
}

static void
nemo_empty_view_class_init (NemoEmptyViewClass *class)
{
	NemoViewClass *nemo_view_class;

	g_type_class_add_private (class, sizeof (NemoEmptyViewDetails));

	nemo_view_class = NEMO_VIEW_CLASS (class);

	nemo_view_class->add_file = nemo_empty_view_add_file;
	nemo_view_class->begin_loading = nemo_empty_view_begin_loading;
	nemo_view_class->bump_zoom_level = nemo_empty_view_bump_zoom_level;
	nemo_view_class->can_zoom_in = nemo_empty_view_can_zoom_in;
	nemo_view_class->can_zoom_out = nemo_empty_view_can_zoom_out;
        nemo_view_class->click_policy_changed = nemo_empty_view_click_policy_changed;
	nemo_view_class->clear = nemo_empty_view_clear;
	nemo_view_class->file_changed = nemo_empty_view_file_changed;
	nemo_view_class->get_selection = nemo_empty_view_get_selection;
	nemo_view_class->get_selection_for_file_transfer = nemo_empty_view_get_selection_for_file_transfer;
	nemo_view_class->get_item_count = nemo_empty_view_get_item_count;
	nemo_view_class->is_empty = nemo_empty_view_is_empty;
	nemo_view_class->remove_file = nemo_empty_view_remove_file;
	nemo_view_class->merge_menus = nemo_empty_view_merge_menus;
	nemo_view_class->update_menus = nemo_empty_view_update_menus;
	nemo_view_class->reset_to_defaults = nemo_empty_view_reset_to_defaults;
	nemo_view_class->restore_default_zoom_level = nemo_empty_view_restore_default_zoom_level;
	nemo_view_class->reveal_selection = nemo_empty_view_reveal_selection;
	nemo_view_class->select_all = nemo_empty_view_select_all;
	nemo_view_class->set_selection = nemo_empty_view_set_selection;
	nemo_view_class->compare_files = nemo_empty_view_compare_files;
	nemo_view_class->sort_directories_first_changed = nemo_empty_view_sort_directories_first_changed;
	nemo_view_class->start_renaming_file = nemo_empty_view_start_renaming_file;
	nemo_view_class->get_zoom_level = nemo_empty_view_get_zoom_level;
	nemo_view_class->zoom_to_level = nemo_empty_view_zoom_to_level;
	nemo_view_class->end_file_changes = nemo_empty_view_end_file_changes;
	nemo_view_class->using_manual_layout = nemo_empty_view_using_manual_layout;
	nemo_view_class->end_loading = nemo_empty_view_end_loading;
	nemo_view_class->get_view_id = nemo_empty_view_get_id;
	nemo_view_class->get_first_visible_file = nemo_empty_view_get_first_visible_file;
	nemo_view_class->scroll_to_file = nemo_empty_view_scroll_to_file;
}

static void
nemo_empty_view_init (NemoEmptyView *empty_view)
{
	empty_view->details = G_TYPE_INSTANCE_GET_PRIVATE (empty_view, NEMO_TYPE_EMPTY_VIEW,
							   NemoEmptyViewDetails);
}

static NemoView *
nemo_empty_view_create (NemoWindowSlot *slot)
{
	NemoEmptyView *view;

	g_assert (NEMO_IS_WINDOW_SLOT (slot));

	view = g_object_new (NEMO_TYPE_EMPTY_VIEW,
			     "window-slot", slot,
			     NULL);

	return NEMO_VIEW (view);
}

static gboolean
nemo_empty_view_supports_uri (const char *uri,
				  GFileType file_type,
				  const char *mime_type)
{
	if (file_type == G_FILE_TYPE_DIRECTORY) {
		return TRUE;
	}
	if (strcmp (mime_type, NEMO_SAVED_SEARCH_MIMETYPE) == 0){
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

static NemoViewInfo nemo_empty_view = {
	NEMO_EMPTY_VIEW_ID,
	"Empty",
	"Empty View",
	"_Empty View",
	"The empty view encountered an error.",
	"Display this location with the empty view.",
	nemo_empty_view_create,
	nemo_empty_view_supports_uri
};

void
nemo_empty_view_register (void)
{
	nemo_empty_view.id = nemo_empty_view.id;
	nemo_empty_view.view_combo_label = nemo_empty_view.view_combo_label;
	nemo_empty_view.view_menu_label_with_mnemonic = nemo_empty_view.view_menu_label_with_mnemonic;
	nemo_empty_view.error_label = nemo_empty_view.error_label;
	nemo_empty_view.display_location_label = nemo_empty_view.display_location_label;

	nemo_view_factory_register (&nemo_empty_view);
}

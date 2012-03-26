/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-search-directory-file.c: Subclass of NautilusFile to help implement the
   searches
 
   Copyright (C) 2005 Novell, Inc.
  
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
  
   Author: Anders Carlsson <andersca@imendio.com>
*/

#include <config.h>
#include "nautilus-search-directory-file.h"

#include "nautilus-directory-notify.h"
#include "nautilus-directory-private.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include <eel/eel-glib-extensions.h>
#include "nautilus-search-directory.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

struct NautilusSearchDirectoryFileDetails {
	NautilusSearchDirectory *search_directory;
};

G_DEFINE_TYPE(NautilusSearchDirectoryFile, nautilus_search_directory_file, NAUTILUS_TYPE_FILE);


static void
search_directory_file_monitor_add (NautilusFile *file,
				   gconstpointer client,
				   NautilusFileAttributes attributes)
{
	/* No need for monitoring, we always emit changed when files
	   are added/removed, and no other metadata changes */

	/* Update display name, in case this didn't happen yet */
	nautilus_search_directory_file_update_display_name (NAUTILUS_SEARCH_DIRECTORY_FILE (file));
}

static void
search_directory_file_monitor_remove (NautilusFile *file,
				      gconstpointer client)
{
	/* Do nothing here, we don't have any monitors */
}

static void
search_directory_file_call_when_ready (NautilusFile *file,
				       NautilusFileAttributes file_attributes,
				       NautilusFileCallback callback,
				       gpointer callback_data)

{
	/* Update display name, in case this didn't happen yet */
	nautilus_search_directory_file_update_display_name (NAUTILUS_SEARCH_DIRECTORY_FILE (file));
	
	/* All data for directory-as-file is always uptodate */
	(* callback) (file, callback_data);
}
 
static void
search_directory_file_cancel_call_when_ready (NautilusFile *file,
					       NautilusFileCallback callback,
					       gpointer callback_data)
{
	/* Do nothing here, we don't have any pending calls */
}

static gboolean
search_directory_file_check_if_ready (NautilusFile *file,
				      NautilusFileAttributes attributes)
{
	return TRUE;
}

static gboolean
search_directory_file_get_item_count (NautilusFile *file, 
				      guint *count,
				      gboolean *count_unreadable)
{
	GList *file_list;

	if (count) {
		file_list = nautilus_directory_get_file_list (file->details->directory);

		*count = g_list_length (file_list);

		nautilus_file_list_free (file_list);
	}

	return TRUE;
}

static NautilusRequestStatus
search_directory_file_get_deep_counts (NautilusFile *file,
				       guint *directory_count,
				       guint *file_count,
				       guint *unreadable_directory_count,
				       goffset *total_size)
{
	NautilusFile *dir_file;
	GList *file_list, *l;
	guint dirs, files;
	GFileType type;

	file_list = nautilus_directory_get_file_list (file->details->directory);

	dirs = files = 0;
	for (l = file_list; l != NULL; l = l->next) {
		dir_file = NAUTILUS_FILE (l->data);
		type = nautilus_file_get_file_type (dir_file);
		if (type == G_FILE_TYPE_DIRECTORY) {
			dirs++;
		} else {
			files++;
		}
	}

	if (directory_count != NULL) {
		*directory_count = dirs;
	}
	if (file_count != NULL) {
		*file_count = files;
	}
	if (unreadable_directory_count != NULL) {
		*unreadable_directory_count = 0;
	}
	if (total_size != NULL) {
		/* FIXME: Maybe we want to calculate this? */
		*total_size = 0;
	}
	
	nautilus_file_list_free (file_list);
	
	return NAUTILUS_REQUEST_DONE;
}

static char *
search_directory_file_get_where_string (NautilusFile *file)
{
	return g_strdup (_("Search"));
}

void
nautilus_search_directory_file_update_display_name (NautilusSearchDirectoryFile *search_file)
{
	NautilusFile *file;
	NautilusSearchDirectory *search_dir;
	NautilusQuery *query;
	char *display_name;
	gboolean changed;

	
	display_name = NULL;
	file = NAUTILUS_FILE (search_file);
	if (file->details->directory) {
		search_dir = NAUTILUS_SEARCH_DIRECTORY (file->details->directory);
		query = nautilus_search_directory_get_query (search_dir);
	
		if (query != NULL) {
			display_name = nautilus_query_to_readable_string (query);
			g_object_unref (query);
		} 
	}

	if (display_name == NULL) {
		display_name = g_strdup (_("Search"));
	}

	changed = nautilus_file_set_display_name (file, display_name, NULL, TRUE);
	if (changed) {
		nautilus_file_emit_changed (file);
	}
}

static void
nautilus_search_directory_file_init (NautilusSearchDirectoryFile *search_file)
{
	NautilusFile *file;

	file = NAUTILUS_FILE (search_file);

	file->details->got_file_info = TRUE;
	file->details->mime_type = eel_ref_str_get_unique ("x-directory/normal");
	file->details->type = G_FILE_TYPE_DIRECTORY;
	file->details->size = 0;

	file->details->file_info_is_up_to_date = TRUE;

	file->details->custom_icon = NULL;
	file->details->activation_uri = NULL;
	file->details->got_link_info = TRUE;
	file->details->link_info_is_up_to_date = TRUE;

	file->details->directory_count = 0;
	file->details->got_directory_count = TRUE;
	file->details->directory_count_is_up_to_date = TRUE;

	nautilus_file_set_display_name (file, _("Search"), NULL, TRUE);
}

static void
nautilus_search_directory_file_class_init (NautilusSearchDirectoryFileClass *klass)
{
	NautilusFileClass *file_class;

	file_class = NAUTILUS_FILE_CLASS (klass);

	file_class->default_file_type = G_FILE_TYPE_DIRECTORY;

	file_class->monitor_add = search_directory_file_monitor_add;
	file_class->monitor_remove = search_directory_file_monitor_remove;
	file_class->call_when_ready = search_directory_file_call_when_ready;
	file_class->cancel_call_when_ready = search_directory_file_cancel_call_when_ready;
	file_class->check_if_ready = search_directory_file_check_if_ready;
	file_class->get_item_count = search_directory_file_get_item_count;
	file_class->get_deep_counts = search_directory_file_get_deep_counts;
	file_class->get_where_string = search_directory_file_get_where_string;
}

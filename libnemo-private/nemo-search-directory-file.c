/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-search-directory-file.c: Subclass of NemoFile to help implement the
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
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
  
   Author: Anders Carlsson <andersca@imendio.com>
*/

#include <config.h>
#include "nemo-search-directory-file.h"

#include "nemo-directory-notify.h"
#include "nemo-directory-private.h"
#include "nemo-file-attributes.h"
#include "nemo-file-private.h"
#include "nemo-file-utilities.h"
#include <eel/eel-glib-extensions.h>
#include "nemo-search-directory.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

struct NemoSearchDirectoryFileDetails {
	NemoSearchDirectory *search_directory;
};

G_DEFINE_TYPE(NemoSearchDirectoryFile, nemo_search_directory_file, NEMO_TYPE_FILE);


static void
search_directory_file_monitor_add (NemoFile *file,
				   gconstpointer client,
				   NemoFileAttributes attributes)
{
	/* No need for monitoring, we always emit changed when files
	   are added/removed, and no other metadata changes */

	/* Update display name, in case this didn't happen yet */
	nemo_search_directory_file_update_display_name (NEMO_SEARCH_DIRECTORY_FILE (file));
}

static void
search_directory_file_monitor_remove (NemoFile *file,
				      gconstpointer client)
{
	/* Do nothing here, we don't have any monitors */
}

static void
search_directory_file_call_when_ready (NemoFile *file,
				       NemoFileAttributes file_attributes,
				       NemoFileCallback callback,
				       gpointer callback_data)

{
	/* Update display name, in case this didn't happen yet */
	nemo_search_directory_file_update_display_name (NEMO_SEARCH_DIRECTORY_FILE (file));
	
	/* All data for directory-as-file is always uptodate */
	(* callback) (file, callback_data);
}
 
static void
search_directory_file_cancel_call_when_ready (NemoFile *file,
					       NemoFileCallback callback,
					       gpointer callback_data)
{
	/* Do nothing here, we don't have any pending calls */
}

static gboolean
search_directory_file_check_if_ready (NemoFile *file,
				      NemoFileAttributes attributes)
{
	return TRUE;
}

static gboolean
search_directory_file_get_item_count (NemoFile *file, 
				      guint *count,
				      gboolean *count_unreadable)
{
	GList *file_list;

	if (count) {
		file_list = nemo_directory_get_file_list (file->details->directory);

		*count = g_list_length (file_list);

		nemo_file_list_free (file_list);
	}

	return TRUE;
}

static NemoRequestStatus
search_directory_file_get_deep_counts (NemoFile *file,
				       guint *directory_count,
				       guint *file_count,
				       guint *unreadable_directory_count,
                       guint *hidden_count,
				       goffset *total_size)
{
	NemoFile *dir_file;
	GList *file_list, *l;
	guint dirs, files;
	GFileType type;

	file_list = nemo_directory_get_file_list (file->details->directory);

	dirs = files = 0;
	for (l = file_list; l != NULL; l = l->next) {
		dir_file = NEMO_FILE (l->data);
		type = nemo_file_get_file_type (dir_file);
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
    if (hidden_count != NULL) {
        *hidden_count = 0;
    }

	nemo_file_list_free (file_list);
	
	return NEMO_REQUEST_DONE;
}

static char *
search_directory_file_get_where_string (NemoFile *file)
{
	return g_strdup (_("Search"));
}

void
nemo_search_directory_file_update_display_name (NemoSearchDirectoryFile *search_file)
{
	NemoFile *file;
	NemoSearchDirectory *search_dir;
	NemoQuery *query;
	char *display_name;
	gboolean changed;

	
	display_name = NULL;
	file = NEMO_FILE (search_file);
	if (file->details->directory) {
		search_dir = NEMO_SEARCH_DIRECTORY (file->details->directory);
		query = nemo_search_directory_get_query (search_dir);
	
		if (query != NULL) {
			display_name = nemo_query_to_readable_string (query);
			g_object_unref (query);
		} 
	}

	if (display_name == NULL) {
		display_name = g_strdup (_("Search"));
	}

	changed = nemo_file_set_display_name (file, display_name, NULL, TRUE);
	if (changed) {
		nemo_file_emit_changed (file);
	}

	g_free (display_name);
}

static void
nemo_search_directory_file_init (NemoSearchDirectoryFile *search_file)
{
	NemoFile *file;

	file = NEMO_FILE (search_file);

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

	nemo_file_set_display_name (file, _("Search"), NULL, TRUE);
}

static void
nemo_search_directory_file_class_init (NemoSearchDirectoryFileClass *klass)
{
	NemoFileClass *file_class;

	file_class = NEMO_FILE_CLASS (klass);

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

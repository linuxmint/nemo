/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-desktop-directory-file.c: Subclass of NemoFile to help implement the
   virtual desktop.
 
   Copyright (C) 2003 Red Hat, Inc.
  
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
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "nemo-desktop-directory-file.h"

#include "nemo-desktop-metadata.h"
#include "nemo-directory-notify.h"
#include "nemo-directory-private.h"
#include "nemo-file-attributes.h"
#include "nemo-file-private.h"
#include "nemo-file-utilities.h"
#include <eel/eel-glib-extensions.h>
#include "nemo-desktop-directory.h"
#include "nemo-metadata.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>

struct NemoDesktopDirectoryFileDetails {
	NemoDesktopDirectory *desktop_directory;

  	NemoFile *real_dir_file;

	GHashTable *callbacks;
	GHashTable *monitors;
};

typedef struct {
	NemoDesktopDirectoryFile *desktop_file;
	NemoFileCallback callback;
	gpointer callback_data;

	NemoFileAttributes delegated_attributes;
	NemoFileAttributes non_delegated_attributes;

	GList *non_ready_files;

	gboolean initializing;
} DesktopCallback;

typedef struct {
	NemoDesktopDirectoryFile *desktop_file;

	NemoFileAttributes delegated_attributes;
	NemoFileAttributes non_delegated_attributes;
} DesktopMonitor;

G_DEFINE_TYPE (NemoDesktopDirectoryFile, nemo_desktop_directory_file,
	       NEMO_TYPE_FILE);

static guint
desktop_callback_hash (gconstpointer desktop_callback_as_pointer)
{
	const DesktopCallback *desktop_callback;

	desktop_callback = desktop_callback_as_pointer;
	return GPOINTER_TO_UINT (desktop_callback->callback)
		^ GPOINTER_TO_UINT (desktop_callback->callback_data);
}

static gboolean
desktop_callback_equal (gconstpointer desktop_callback_as_pointer,
		      gconstpointer desktop_callback_as_pointer_2)
{
	const DesktopCallback *desktop_callback, *desktop_callback_2;

	desktop_callback = desktop_callback_as_pointer;
	desktop_callback_2 = desktop_callback_as_pointer_2;

	return desktop_callback->callback == desktop_callback_2->callback
		&& desktop_callback->callback_data == desktop_callback_2->callback_data;
}

     
static void
real_file_changed_callback (NemoFile *real_file,
			    gpointer callback_data)
{
	NemoDesktopDirectoryFile *desktop_file;
	
	desktop_file = NEMO_DESKTOP_DIRECTORY_FILE (callback_data);
	nemo_file_changed (NEMO_FILE (desktop_file));
}

static NemoFileAttributes 
get_delegated_attributes_mask (void)
{
	return NEMO_FILE_ATTRIBUTE_DEEP_COUNTS |
		NEMO_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
		NEMO_FILE_ATTRIBUTE_DIRECTORY_ITEM_MIME_TYPES;
}

static void
partition_attributes (NemoFileAttributes attributes,
		      NemoFileAttributes *delegated_attributes,
		      NemoFileAttributes *non_delegated_attributes)
{
	NemoFileAttributes mask;

	mask = get_delegated_attributes_mask ();

	*delegated_attributes = attributes & mask;
	*non_delegated_attributes = attributes & ~mask;
}

static void
desktop_directory_file_monitor_add (NemoFile *file,
				    gconstpointer client,
				    NemoFileAttributes attributes)
{
	NemoDesktopDirectoryFile *desktop_file;
	DesktopMonitor *monitor;

	desktop_file = NEMO_DESKTOP_DIRECTORY_FILE (file);

	/* Map the client to a unique value so this doesn't interfere
	 * with direct monitoring of the file by the same client.
	 */
	monitor = g_hash_table_lookup (desktop_file->details->monitors, client);
	if (monitor != NULL) {
		g_assert (monitor->desktop_file == desktop_file);
	} else {
		monitor = g_new0 (DesktopMonitor, 1);
		monitor->desktop_file = desktop_file;
		g_hash_table_insert (desktop_file->details->monitors,
				     (gpointer) client, monitor);
	}

	partition_attributes (attributes,
			      &monitor->delegated_attributes,
			      &monitor->non_delegated_attributes);

	/* Pawn off partioned attributes to real dir file */
	nemo_file_monitor_add (desktop_file->details->real_dir_file,
				   monitor, monitor->delegated_attributes);

	/* Do the rest ourself */
	nemo_directory_monitor_add_internal
		(file->details->directory, file,
		 client, TRUE,
		 monitor->non_delegated_attributes,
		 NULL, NULL);
}

static void
desktop_directory_file_monitor_remove (NemoFile *file,
				       gconstpointer client)
{
	NemoDesktopDirectoryFile *desktop_file;
	DesktopMonitor *monitor;
	
	desktop_file = NEMO_DESKTOP_DIRECTORY_FILE (file);
	
	/* Map the client to the value used by the earlier add call. */
        monitor = g_hash_table_lookup (desktop_file->details->monitors, client);
	if (monitor == NULL) {
		return;
	}

	/* Call through to the real file remove calls. */
	g_hash_table_remove (desktop_file->details->monitors, client);

	/* Remove the locally handled parts */
	nemo_directory_monitor_remove_internal
		(file->details->directory, file, client);
}

static void
desktop_callback_destroy (DesktopCallback *desktop_callback)
{
	g_assert (desktop_callback != NULL);
	g_assert (NEMO_IS_DESKTOP_DIRECTORY_FILE (desktop_callback->desktop_file));

	nemo_file_unref (NEMO_FILE (desktop_callback->desktop_file));
	g_list_free (desktop_callback->non_ready_files);
	g_free (desktop_callback);
}

static void
desktop_callback_check_done (DesktopCallback *desktop_callback)
{
	/* Check if we are ready. */
	if (desktop_callback->initializing ||
	    desktop_callback->non_ready_files != NULL) {
		return;
	}

	/* Ensure our metadata is updated before calling back */
	nemo_desktop_update_metadata_from_keyfile (NEMO_FILE (desktop_callback->desktop_file), "directory");

	/* Remove from the hash table before sending it. */
	g_hash_table_remove (desktop_callback->desktop_file->details->callbacks,
			     desktop_callback);

	/* We are ready, so do the real callback. */
	(* desktop_callback->callback) (NEMO_FILE (desktop_callback->desktop_file),
					desktop_callback->callback_data);

	/* And we are done. */
	desktop_callback_destroy (desktop_callback);
}

static void
desktop_callback_remove_file (DesktopCallback *desktop_callback,
			      NemoFile *file)
{
	desktop_callback->non_ready_files = g_list_remove
		(desktop_callback->non_ready_files, file);
	desktop_callback_check_done (desktop_callback);
}

static void
ready_callback (NemoFile *file,
		gpointer callback_data)
{
	DesktopCallback *desktop_callback;

	g_assert (NEMO_IS_FILE (file));
	g_assert (callback_data != NULL);

	desktop_callback = callback_data;
	g_assert (g_list_find (desktop_callback->non_ready_files, file) != NULL);

	desktop_callback_remove_file (desktop_callback, file);
}

static void
desktop_directory_file_call_when_ready (NemoFile *file,
					NemoFileAttributes attributes,
					NemoFileCallback callback,
					gpointer callback_data)

{
	NemoDesktopDirectoryFile *desktop_file;
	DesktopCallback search_key, *desktop_callback;

	desktop_file = NEMO_DESKTOP_DIRECTORY_FILE (file);

	/* Check to be sure we aren't overwriting. */
	search_key.callback = callback;
	search_key.callback_data = callback_data;
	if (g_hash_table_lookup (desktop_file->details->callbacks, &search_key) != NULL) {
		g_warning ("tried to add a new callback while an old one was pending");
		return;
	}

	/* Create a desktop_callback record. */
	desktop_callback = g_new0 (DesktopCallback, 1);
	nemo_file_ref (file);
	desktop_callback->desktop_file = desktop_file;
	desktop_callback->callback = callback;
	desktop_callback->callback_data = callback_data;
	desktop_callback->initializing = TRUE;

	partition_attributes (attributes,
			      &desktop_callback->delegated_attributes,
			      &desktop_callback->non_delegated_attributes);

	desktop_callback->non_ready_files = g_list_prepend
		(desktop_callback->non_ready_files, file);
	desktop_callback->non_ready_files = g_list_prepend
		(desktop_callback->non_ready_files, desktop_file->details->real_dir_file);
	
	/* Put it in the hash table. */
	g_hash_table_insert (desktop_file->details->callbacks,
			     desktop_callback, desktop_callback);

	/* Now connect to each file's call_when_ready. */
	nemo_directory_call_when_ready_internal
		(file->details->directory, file,
		 desktop_callback->non_delegated_attributes,
		 FALSE, NULL, ready_callback, desktop_callback);
	nemo_file_call_when_ready
			(desktop_file->details->real_dir_file,
			 desktop_callback->delegated_attributes,
			 ready_callback, desktop_callback);

	desktop_callback->initializing = FALSE;

	/* Check if any files became read while we were connecting up
	 * the call_when_ready callbacks (also handles the pathological
	 * case where there are no files at all).
	 */
	desktop_callback_check_done (desktop_callback);

}

static void
desktop_directory_file_cancel_call_when_ready (NemoFile *file,
					       NemoFileCallback callback,
					       gpointer callback_data)
{
	NemoDesktopDirectoryFile *desktop_file;
	DesktopCallback search_key, *desktop_callback;

	desktop_file = NEMO_DESKTOP_DIRECTORY_FILE (file);

	/* Find the entry in the table. */
	search_key.callback = callback;
	search_key.callback_data = callback_data;
	desktop_callback = g_hash_table_lookup (desktop_file->details->callbacks, &search_key);
	if (desktop_callback == NULL) {
		return;
	}

	/* Remove from the hash table before working with it. */
	g_hash_table_remove (desktop_callback->desktop_file->details->callbacks, desktop_callback);

	/* Tell the real directory to cancel the call. */
	nemo_directory_cancel_callback_internal
		(file->details->directory, file,
		 NULL, ready_callback, desktop_callback);
	
	nemo_file_cancel_call_when_ready
		(desktop_file->details->real_dir_file,
		 ready_callback, desktop_callback);
	
	desktop_callback_destroy (desktop_callback);
}

static gboolean
real_check_if_ready (NemoFile *file,
		     NemoFileAttributes attributes)
{
	return nemo_directory_check_if_ready_internal
		(file->details->directory, file,
		 attributes);
}

static gboolean
desktop_directory_file_check_if_ready (NemoFile *file,
				       NemoFileAttributes attributes)
{
	NemoFileAttributes delegated_attributes, non_delegated_attributes;
	NemoDesktopDirectoryFile *desktop_file;

	desktop_file = NEMO_DESKTOP_DIRECTORY_FILE (file);

	partition_attributes (attributes,
			      &delegated_attributes,
			      &non_delegated_attributes);

	return real_check_if_ready (file, non_delegated_attributes) &&
		nemo_file_check_if_ready (desktop_file->details->real_dir_file,
					      delegated_attributes);
}

static gboolean
desktop_directory_file_get_item_count (NemoFile *file, 
				       guint *count,
				       gboolean *count_unreadable)
{
	NemoDesktopDirectoryFile *desktop_file;
	gboolean got_count;
	
	desktop_file = NEMO_DESKTOP_DIRECTORY_FILE (file);
	
	got_count = nemo_file_get_directory_item_count (desktop_file->details->real_dir_file,
							    count,
							    count_unreadable);

	if (count) {
		*count += g_list_length (file->details->directory->details->file_list);
	}
	
	return got_count;
}

static NemoRequestStatus
desktop_directory_file_get_deep_counts (NemoFile *file,
					guint *directory_count,
					guint *file_count,
					guint *unreadable_directory_count,
					goffset *total_size)
{
	NemoDesktopDirectoryFile *desktop_file;
	NemoRequestStatus status;

	desktop_file = NEMO_DESKTOP_DIRECTORY_FILE (file);
	
	status = nemo_file_get_deep_counts (desktop_file->details->real_dir_file,
						directory_count,
						file_count,
						unreadable_directory_count,
						total_size,
						TRUE);

	if (file_count) {
		*file_count += g_list_length (file->details->directory->details->file_list);
	}
	
	return status;
}

static gboolean
desktop_directory_file_get_date (NemoFile *file,
				 NemoDateType date_type,
				 time_t *date)
{
	NemoDesktopDirectoryFile *desktop_file;

	desktop_file = NEMO_DESKTOP_DIRECTORY_FILE (file);

	return nemo_file_get_date (desktop_file->details->real_dir_file,
				       date_type,
				       date);
}

static char *
desktop_directory_file_get_where_string (NemoFile *file)
{
	return g_strdup (_("on the desktop"));
}


static void
monitor_destroy (gpointer data)
{
	DesktopMonitor *monitor = data;
	
	nemo_file_monitor_remove
		(NEMO_FILE (monitor->desktop_file->details->real_dir_file), monitor);
	g_free (monitor);
}

static void
nemo_desktop_directory_file_set_metadata (NemoFile           *file,
					      const char             *key,
					      const char             *value)
{
	nemo_desktop_set_metadata_string (file, "directory", key, value);
}

static void
nemo_desktop_directory_file_set_metadata_as_list (NemoFile           *file,
						      const char             *key,
						      char                  **value)
{
	nemo_desktop_set_metadata_stringv (file, "directory", key, (const gchar **) value);
}

static void
nemo_desktop_directory_file_init (NemoDesktopDirectoryFile *desktop_file)
{
	NemoDesktopDirectory *desktop_directory;
	NemoDirectory *real_dir;
	NemoFile *real_dir_file;

	desktop_file->details = G_TYPE_INSTANCE_GET_PRIVATE (desktop_file,
							     NEMO_TYPE_DESKTOP_DIRECTORY_FILE,
							     NemoDesktopDirectoryFileDetails);

	desktop_directory = NEMO_DESKTOP_DIRECTORY (nemo_directory_get_by_uri (EEL_DESKTOP_URI));
	desktop_file->details->desktop_directory = desktop_directory;

	desktop_file->details->callbacks = g_hash_table_new
		(desktop_callback_hash, desktop_callback_equal);
	desktop_file->details->monitors = g_hash_table_new_full (NULL, NULL,
								 NULL, monitor_destroy);

	real_dir = nemo_desktop_directory_get_real_directory (desktop_directory);
	real_dir_file = nemo_directory_get_corresponding_file (real_dir);
	nemo_directory_unref (real_dir);
	
	desktop_file->details->real_dir_file = real_dir_file;
	g_signal_connect_object (real_dir_file, "changed",
				 G_CALLBACK (real_file_changed_callback), desktop_file, 0);
}


static void
desktop_callback_remove_file_cover (gpointer key,
				    gpointer value,
				    gpointer callback_data)
{
	desktop_callback_remove_file
		(value, NEMO_FILE (callback_data));
}


static void
desktop_finalize (GObject *object)
{
	NemoDesktopDirectoryFile *desktop_file;
	NemoDesktopDirectory *desktop_directory;

	desktop_file = NEMO_DESKTOP_DIRECTORY_FILE (object);
	desktop_directory = desktop_file->details->desktop_directory;

	/* Todo: ghash now safe? */
	eel_g_hash_table_safe_for_each
		(desktop_file->details->callbacks,
		 desktop_callback_remove_file_cover,
		 desktop_file->details->real_dir_file);
	
	if (g_hash_table_size (desktop_file->details->callbacks) != 0) {
		g_warning ("call_when_ready still pending when desktop virtual file is destroyed");
	}

	g_hash_table_destroy (desktop_file->details->callbacks);
	g_hash_table_destroy (desktop_file->details->monitors);

	nemo_file_unref (desktop_file->details->real_dir_file);
	nemo_directory_unref (NEMO_DIRECTORY (desktop_directory));

	G_OBJECT_CLASS (nemo_desktop_directory_file_parent_class)->finalize (object);
}

static void
nemo_desktop_directory_file_class_init (NemoDesktopDirectoryFileClass *klass)
{
	GObjectClass *object_class;
	NemoFileClass *file_class;

	object_class = G_OBJECT_CLASS (klass);
	file_class = NEMO_FILE_CLASS (klass);
	
	object_class->finalize = desktop_finalize;

	file_class->default_file_type = G_FILE_TYPE_DIRECTORY;
	
	file_class->monitor_add = desktop_directory_file_monitor_add;
	file_class->monitor_remove = desktop_directory_file_monitor_remove;
	file_class->call_when_ready = desktop_directory_file_call_when_ready;
	file_class->cancel_call_when_ready = desktop_directory_file_cancel_call_when_ready;
	file_class->check_if_ready = desktop_directory_file_check_if_ready;
	file_class->get_item_count = desktop_directory_file_get_item_count;
	file_class->get_deep_counts = desktop_directory_file_get_deep_counts;
	file_class->get_date = desktop_directory_file_get_date;
	file_class->get_where_string = desktop_directory_file_get_where_string;
	file_class->set_metadata = nemo_desktop_directory_file_set_metadata;
	file_class->set_metadata_as_list = nemo_desktop_directory_file_set_metadata_as_list;

	g_type_class_add_private (klass, sizeof (NemoDesktopDirectoryFileDetails));
}

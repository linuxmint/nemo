/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-merged-directory.c: Subclass of NautilusDirectory to implement the
   virtual merged directory.
 
   Copyright (C) 1999, 2000 Eazel, Inc.
  
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
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#include <config.h>
#include "nautilus-merged-directory.h"

#include "nautilus-directory-private.h"
#include "nautilus-directory-notify.h"
#include "nautilus-file.h"
#include <eel/eel-glib-extensions.h>
#include <gtk/gtk.h>

struct NautilusMergedDirectoryDetails {
	GList *directories;
	GList *directories_not_done_loading;
	GHashTable *callbacks;
	GHashTable *monitors;
};

typedef struct {
	NautilusMergedDirectory *merged;
	NautilusDirectoryCallback callback;
	gpointer callback_data;

	NautilusFileAttributes wait_for_attributes;
	gboolean wait_for_file_list;

	GList *non_ready_directories;
	GList *merged_file_list;
} MergedCallback;

typedef struct {
	NautilusMergedDirectory *merged;

	gboolean monitor_hidden_files;
	NautilusFileAttributes monitor_attributes;
} MergedMonitor;

enum {
	ADD_REAL_DIRECTORY,
	REMOVE_REAL_DIRECTORY,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (NautilusMergedDirectory, nautilus_merged_directory,
	       NAUTILUS_TYPE_DIRECTORY);

static guint
merged_callback_hash (gconstpointer merged_callback_as_pointer)
{
	const MergedCallback *merged_callback;

	merged_callback = merged_callback_as_pointer;
	return GPOINTER_TO_UINT (merged_callback->callback)
		^ GPOINTER_TO_UINT (merged_callback->callback_data);
}

static gboolean
merged_callback_equal (gconstpointer merged_callback_as_pointer,
		       gconstpointer merged_callback_as_pointer_2)
{
	const MergedCallback *merged_callback, *merged_callback_2;

	merged_callback = merged_callback_as_pointer;
	merged_callback_2 = merged_callback_as_pointer_2;

	return merged_callback->callback == merged_callback_2->callback
		&& merged_callback->callback_data == merged_callback_2->callback_data;
}

static void
merged_callback_destroy (MergedCallback *merged_callback)
{
	g_assert (merged_callback != NULL);
	g_assert (NAUTILUS_IS_MERGED_DIRECTORY (merged_callback->merged));

	g_list_free (merged_callback->non_ready_directories);
	nautilus_file_list_free (merged_callback->merged_file_list);
	g_free (merged_callback);
}

static void
merged_callback_check_done (MergedCallback *merged_callback)
{
	/* Check if we are ready. */
	if (merged_callback->non_ready_directories != NULL) {
		return;
	}

	/* Remove from the hash table before sending it. */
	g_hash_table_remove (merged_callback->merged->details->callbacks, merged_callback);

	/* We are ready, so do the real callback. */
	(* merged_callback->callback) (NAUTILUS_DIRECTORY (merged_callback->merged),
				       merged_callback->merged_file_list,
				       merged_callback->callback_data);

	/* And we are done. */
	merged_callback_destroy (merged_callback);
}

static void
merged_callback_remove_directory (MergedCallback *merged_callback,
				  NautilusDirectory *directory)
{
	merged_callback->non_ready_directories = g_list_remove
		(merged_callback->non_ready_directories, directory);
	merged_callback_check_done (merged_callback);
}

static void
directory_ready_callback (NautilusDirectory *directory,
			  GList *files,
			  gpointer callback_data)
{
	MergedCallback *merged_callback;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (callback_data != NULL);

	merged_callback = callback_data;
	g_assert (g_list_find (merged_callback->non_ready_directories, directory) != NULL);

	/* Update based on this call. */
	merged_callback->merged_file_list = g_list_concat
		(merged_callback->merged_file_list,
		 nautilus_file_list_copy (files));

	/* Check if we are ready. */
	merged_callback_remove_directory (merged_callback, directory);
}

static void
merged_call_when_ready (NautilusDirectory *directory,
			NautilusFileAttributes file_attributes,
			gboolean wait_for_file_list,
			NautilusDirectoryCallback callback,
			gpointer callback_data)
{
	NautilusMergedDirectory *merged;
	MergedCallback search_key, *merged_callback;
	GList *node;

	merged = NAUTILUS_MERGED_DIRECTORY (directory);

	/* Check to be sure we aren't overwriting. */
	search_key.callback = callback;
	search_key.callback_data = callback_data;
	if (g_hash_table_lookup (merged->details->callbacks, &search_key) != NULL) {
		g_warning ("tried to add a new callback while an old one was pending");
		return;
	}

	/* Create a merged_callback record. */
	merged_callback = g_new0 (MergedCallback, 1);
	merged_callback->merged = merged;
	merged_callback->callback = callback;
	merged_callback->callback_data = callback_data;
	merged_callback->wait_for_attributes = file_attributes;
	merged_callback->wait_for_file_list = wait_for_file_list;
	for (node = merged->details->directories; node != NULL; node = node->next) {
		merged_callback->non_ready_directories = g_list_prepend
			(merged_callback->non_ready_directories, node->data);
	}

	/* Put it in the hash table. */
	g_hash_table_insert (merged->details->callbacks,
			     merged_callback, merged_callback);

	/* Handle the pathological case where there are no directories. */
	if (merged->details->directories == NULL) {
		merged_callback_check_done (merged_callback);
	}

	/* Now tell all the directories about it. */
	for (node = merged->details->directories; node != NULL; node = node->next) {
		nautilus_directory_call_when_ready
			(node->data,
			 merged_callback->wait_for_attributes,
			 merged_callback->wait_for_file_list,
			 directory_ready_callback, merged_callback);
	}
}

static void
merged_cancel_callback (NautilusDirectory *directory,
			NautilusDirectoryCallback callback,
			gpointer callback_data)
{
	NautilusMergedDirectory *merged;
	MergedCallback search_key, *merged_callback;
	GList *node;

	merged = NAUTILUS_MERGED_DIRECTORY (directory);

	/* Find the entry in the table. */
	search_key.callback = callback;
	search_key.callback_data = callback_data;
	merged_callback = g_hash_table_lookup (merged->details->callbacks, &search_key);
	if (merged_callback == NULL) {
		return;
	}

	/* Remove from the hash table before working with it. */
	g_hash_table_remove (merged_callback->merged->details->callbacks, merged_callback);

	/* Tell all the directories to cancel the call. */
	for (node = merged_callback->non_ready_directories; node != NULL; node = node->next) {
		nautilus_directory_cancel_callback
			(node->data,
			 directory_ready_callback, merged_callback);
	}
	merged_callback_destroy (merged_callback);
}

static void
build_merged_callback_list (NautilusDirectory *directory,
			    GList *file_list,
			    gpointer callback_data)
{
	GList **merged_list;

	merged_list = callback_data;
	*merged_list = g_list_concat (*merged_list,
				      nautilus_file_list_copy (file_list));
}

/* Create a monitor on each of the directories in the list. */
static void
merged_monitor_add (NautilusDirectory *directory,
		    gconstpointer client,
		    gboolean monitor_hidden_files,
		    NautilusFileAttributes file_attributes,
		    NautilusDirectoryCallback callback,
		    gpointer callback_data)
{
	NautilusMergedDirectory *merged;
	MergedMonitor *monitor;
	GList *node;
	GList *merged_callback_list;

	merged = NAUTILUS_MERGED_DIRECTORY (directory);

	/* Map the client to a unique value so this doesn't interfere
	 * with direct monitoring of the directory by the same client.
	 */
	monitor = g_hash_table_lookup (merged->details->monitors, client);
	if (monitor != NULL) {
		g_assert (monitor->merged == merged);
	} else {
		monitor = g_new0 (MergedMonitor, 1);
		monitor->merged = merged;
		g_hash_table_insert (merged->details->monitors,
				     (gpointer) client, monitor);
	}
	monitor->monitor_hidden_files = monitor_hidden_files;
	monitor->monitor_attributes = file_attributes;

	/* Call through to the real directory add calls. */
	merged_callback_list = NULL;
	for (node = merged->details->directories; node != NULL; node = node->next) {
		nautilus_directory_file_monitor_add
			(node->data, monitor,
			 monitor_hidden_files,
			 file_attributes,
			 build_merged_callback_list, &merged_callback_list);
	}
	if (callback != NULL) {
		(* callback) (directory, merged_callback_list, callback_data);
	}
	nautilus_file_list_free (merged_callback_list);
}

static void
merged_monitor_destroy (NautilusMergedDirectory *merged, MergedMonitor *monitor)
{
	GList *node;

	/* Call through to the real directory remove calls. */
	for (node = merged->details->directories; node != NULL; node = node->next) {
		nautilus_directory_file_monitor_remove (node->data, monitor);
	}

	g_free (monitor);
}

/* Remove the monitor from each of the directories in the list. */
static void
merged_monitor_remove (NautilusDirectory *directory,
		       gconstpointer client)
{
	NautilusMergedDirectory *merged;
	MergedMonitor *monitor;
	
	merged = NAUTILUS_MERGED_DIRECTORY (directory);
	
	/* Map the client to the value used by the earlier add call. */
        monitor = g_hash_table_lookup (merged->details->monitors, client);
	if (monitor == NULL) {
		return;
	}
	g_hash_table_remove (merged->details->monitors, client);

	merged_monitor_destroy (merged, monitor);
}

static void
merged_force_reload (NautilusDirectory *directory)
{
	NautilusMergedDirectory *merged;
	GList *node;

	merged = NAUTILUS_MERGED_DIRECTORY (directory);

	/* Call through to the real force_reload calls. */
	for (node = merged->details->directories; node != NULL; node = node->next) {
		nautilus_directory_force_reload (node->data);
	}
}

/* Return true if any directory in the list does. */
static gboolean
merged_contains_file (NautilusDirectory *directory,
		      NautilusFile *file)
{
	NautilusMergedDirectory *merged;
	GList *node;

	merged = NAUTILUS_MERGED_DIRECTORY (directory);

	for (node = merged->details->directories; node != NULL; node = node->next) {
		if (nautilus_directory_contains_file (node->data, file)) {
			return TRUE;
		}
	}
	return FALSE;
}

/* Return true only if all directories in the list do. */
static gboolean
merged_are_all_files_seen (NautilusDirectory *directory)
{
	NautilusMergedDirectory *merged;
	GList *node;

	merged = NAUTILUS_MERGED_DIRECTORY (directory);

	for (node = merged->details->directories; node != NULL; node = node->next) {
		if (!nautilus_directory_are_all_files_seen (node->data)) {
			return FALSE;
		}
	}
	return TRUE;
}

/* Return true if any directory in the list does. */
static gboolean
merged_is_not_empty (NautilusDirectory *directory)
{
	NautilusMergedDirectory *merged;
	GList *node;

	merged = NAUTILUS_MERGED_DIRECTORY (directory);

	for (node = merged->details->directories; node != NULL; node = node->next) {
		if (nautilus_directory_is_not_empty (node->data)) {
			return TRUE;
		}
	}
	return FALSE;
}

static GList *
merged_get_file_list (NautilusDirectory *directory)
{
	GList *dirs_file_list, *merged_dir_file_list = NULL;
	GList *dir_list;
	GList *cur_node;

	dirs_file_list = NULL;
	dir_list = NAUTILUS_MERGED_DIRECTORY (directory)->details->directories;

	for (cur_node = dir_list; cur_node != NULL; cur_node = cur_node->next) {
		NautilusDirectory *cur_dir;

		cur_dir = NAUTILUS_DIRECTORY (cur_node->data);
		dirs_file_list = g_list_concat (dirs_file_list,
						 nautilus_directory_get_file_list (cur_dir));
	}

	merged_dir_file_list = NAUTILUS_DIRECTORY_CLASS 
		(nautilus_merged_directory_parent_class)->get_file_list (directory);

	return g_list_concat (dirs_file_list, merged_dir_file_list);
}

static void
forward_files_added_cover (NautilusDirectory *real_directory,
			   GList *files,
			   gpointer callback_data)
{
	nautilus_directory_emit_files_added (NAUTILUS_DIRECTORY (callback_data), files);
}

static void
forward_files_changed_cover (NautilusDirectory *real_directory,
			     GList *files,
			     gpointer callback_data)
{
	nautilus_directory_emit_files_changed (NAUTILUS_DIRECTORY (callback_data), files);
}

static void
done_loading_callback (NautilusDirectory *real_directory,
		       NautilusMergedDirectory *merged)
{
	merged->details->directories_not_done_loading = g_list_remove
		(merged->details->directories_not_done_loading, real_directory);
	if (merged->details->directories_not_done_loading == NULL) {
		nautilus_directory_emit_done_loading (NAUTILUS_DIRECTORY (merged));
	}
}

static void
monitor_add_directory (gpointer key,
		       gpointer value,
		       gpointer callback_data)
{
	MergedMonitor *monitor;
	
	monitor = value;
	nautilus_directory_file_monitor_add
		(NAUTILUS_DIRECTORY (callback_data), monitor,
		 monitor->monitor_hidden_files,
		 monitor->monitor_attributes,
		 forward_files_added_cover, monitor->merged);
}

static void
merged_add_real_directory (NautilusMergedDirectory *merged,
			   NautilusDirectory *real_directory)
{
	g_return_if_fail (NAUTILUS_IS_MERGED_DIRECTORY (merged));
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (real_directory));
	g_return_if_fail (!NAUTILUS_IS_MERGED_DIRECTORY (real_directory));
	g_return_if_fail (g_list_find (merged->details->directories, real_directory) == NULL);

	/* Add to our list of directories. */
	nautilus_directory_ref (real_directory);
	merged->details->directories = g_list_prepend
		(merged->details->directories, real_directory);
	merged->details->directories_not_done_loading = g_list_prepend
		(merged->details->directories_not_done_loading, real_directory);

	g_signal_connect_object (real_directory, "done_loading",
				 G_CALLBACK (done_loading_callback), merged, 0);

	/* FIXME bugzilla.gnome.org 45084: The done_loading part won't work for the case where
         * we have no directories in our list.
	 */

	/* Add the directory to any extant monitors. */
	g_hash_table_foreach (merged->details->monitors,
			      monitor_add_directory,
			      real_directory);
	/* FIXME bugzilla.gnome.org 42541: Do we need to add the directory to callbacks too? */

	g_signal_connect_object (real_directory, "files_added",
				 G_CALLBACK (forward_files_added_cover), merged, 0);
	g_signal_connect_object (real_directory, "files_changed",
				 G_CALLBACK (forward_files_changed_cover), merged, 0);
}

void
nautilus_merged_directory_add_real_directory (NautilusMergedDirectory *merged,
					      NautilusDirectory *real_directory)
{
	g_return_if_fail (NAUTILUS_IS_MERGED_DIRECTORY (merged));
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (real_directory));
	g_return_if_fail (!NAUTILUS_IS_MERGED_DIRECTORY (real_directory));

	/* Quietly do nothing if asked to add something that's already there. */
	if (g_list_find (merged->details->directories, real_directory) != NULL) {
		return;
	}

	g_signal_emit (merged, signals[ADD_REAL_DIRECTORY], 0, real_directory);
}

GList *
nautilus_merged_directory_get_real_directories (NautilusMergedDirectory *merged)
{
	return g_list_copy (merged->details->directories);
}

static void
merged_callback_remove_directory_cover (gpointer key,
					gpointer value,
					gpointer callback_data)
{
	merged_callback_remove_directory
		(value, NAUTILUS_DIRECTORY (callback_data));
}

static void
monitor_remove_directory (gpointer key,
			  gpointer value,
			  gpointer callback_data)
{
	nautilus_directory_file_monitor_remove
		(NAUTILUS_DIRECTORY (callback_data), value);
}

static void
real_directory_notify_files_removed (NautilusDirectory *real_directory)
{
	GList *files, *l;

	files = nautilus_directory_get_file_list (real_directory);

	for (l = files; l; l = l->next) {
		NautilusFile *file;
		char *uri;

		file = NAUTILUS_FILE (l->data);
		uri = nautilus_file_get_uri (file);
		nautilus_file_unref (file);

		l->data = uri;
	}

	if (files) {
		nautilus_directory_notify_files_removed_by_uri (files);
	}

	g_list_free_full (files, g_free);
}

static void
merged_remove_real_directory (NautilusMergedDirectory *merged,
			      NautilusDirectory *real_directory)
{
	g_return_if_fail (NAUTILUS_IS_MERGED_DIRECTORY (merged));
	g_return_if_fail (NAUTILUS_IS_DIRECTORY (real_directory));
	g_return_if_fail (g_list_find (merged->details->directories, real_directory) != NULL);

	/* Since the real directory will be going away, act as if files were removed */
	real_directory_notify_files_removed (real_directory);

	/* Remove this directory from callbacks and monitors. */
	eel_g_hash_table_safe_for_each (merged->details->callbacks,
					merged_callback_remove_directory_cover,
					real_directory);
	g_hash_table_foreach (merged->details->monitors,
			      monitor_remove_directory,
			      real_directory);

	/* Disconnect all the signals. */
	g_signal_handlers_disconnect_matched
		(real_directory, G_SIGNAL_MATCH_DATA,
		 0, 0, NULL, NULL, merged);

	/* Remove from our list of directories. */
	merged->details->directories = g_list_remove
		(merged->details->directories, real_directory);
	merged->details->directories_not_done_loading = g_list_remove
		(merged->details->directories_not_done_loading, real_directory);
	nautilus_directory_unref (real_directory);
}

void
nautilus_merged_directory_remove_real_directory (NautilusMergedDirectory *merged,
						 NautilusDirectory *real_directory)
{
	g_return_if_fail (NAUTILUS_IS_MERGED_DIRECTORY (merged));

	/* Quietly do nothing if asked to remove something that's not there. */
	if (g_list_find (merged->details->directories, real_directory) == NULL) {
		return;
	}

	g_signal_emit (merged, signals[REMOVE_REAL_DIRECTORY], 0, real_directory);
}

static void
merged_monitor_destroy_cover (gpointer key,
			      gpointer value,
			      gpointer callback_data)
{
	merged_monitor_destroy (callback_data, value);
}

static void
merged_callback_destroy_cover (gpointer key,
			       gpointer value,
			       gpointer callback_data)
{
	merged_callback_destroy (value);
}

static void
merged_finalize (GObject *object)
{
	NautilusMergedDirectory *merged;

	merged = NAUTILUS_MERGED_DIRECTORY (object);

	g_hash_table_foreach (merged->details->monitors,
			      merged_monitor_destroy_cover, merged);
	g_hash_table_foreach (merged->details->callbacks,
			      merged_callback_destroy_cover, NULL);

	g_hash_table_destroy (merged->details->callbacks);
	g_hash_table_destroy (merged->details->monitors);
	nautilus_directory_list_free (merged->details->directories);
	g_list_free (merged->details->directories_not_done_loading);

	G_OBJECT_CLASS (nautilus_merged_directory_parent_class)->finalize (object);
}

static void
nautilus_merged_directory_init (NautilusMergedDirectory *merged)
{
	merged->details = G_TYPE_INSTANCE_GET_PRIVATE (merged, NAUTILUS_TYPE_MERGED_DIRECTORY,
						       NautilusMergedDirectoryDetails);
	merged->details->callbacks = g_hash_table_new
		(merged_callback_hash, merged_callback_equal);
	merged->details->monitors = g_hash_table_new (NULL, NULL);
}

static void
nautilus_merged_directory_class_init (NautilusMergedDirectoryClass *class)
{
	NautilusDirectoryClass *directory_class;

	directory_class = NAUTILUS_DIRECTORY_CLASS (class);
	
	G_OBJECT_CLASS (class)->finalize = merged_finalize;

	directory_class->contains_file = merged_contains_file;
	directory_class->call_when_ready = merged_call_when_ready;
	directory_class->cancel_callback = merged_cancel_callback;
	directory_class->file_monitor_add = merged_monitor_add;
	directory_class->file_monitor_remove = merged_monitor_remove;
	directory_class->force_reload = merged_force_reload;
 	directory_class->are_all_files_seen = merged_are_all_files_seen;
	directory_class->is_not_empty = merged_is_not_empty;
	/* Override get_file_list so that we can return a list that includes
	 * the files from each of the directories in NautilusMergedDirectory->details->directories.
         */
	directory_class->get_file_list = merged_get_file_list;

	class->add_real_directory = merged_add_real_directory;
	class->remove_real_directory = merged_remove_real_directory;

	g_type_class_add_private (class, sizeof (NautilusMergedDirectoryDetails));

	signals[ADD_REAL_DIRECTORY] 
		= g_signal_new ("add_real_directory",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusMergedDirectoryClass, 
						 add_real_directory),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[REMOVE_REAL_DIRECTORY] 
		= g_signal_new ("remove_real_directory",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NautilusMergedDirectoryClass, 
						 remove_real_directory),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1, G_TYPE_POINTER);
}

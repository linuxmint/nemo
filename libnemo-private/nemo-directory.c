/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-directory.c: Nemo directory model.
 
   Copyright (C) 1999, 2000, 2001 Eazel, Inc.
   
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
   
   Author: Darin Adler <darin@bentspoon.com>
*/

#include <config.h>
#include "nemo-directory-private.h"

#include "nemo-directory-notify.h"
#include "nemo-file-attributes.h"
#include "nemo-file-private.h"
#include "nemo-file-utilities.h"
#include "nemo-search-directory.h"
#include "nemo-global-preferences.h"
#include "nemo-lib-self-check-functions.h"
#include "nemo-metadata.h"
#include "nemo-desktop-directory.h"
#include "nemo-vfs-directory.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>
#include <gtk/gtk.h>

enum {
	FILES_ADDED,
	FILES_CHANGED,
	DONE_LOADING,
	LOAD_ERROR,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GHashTable *directories;

static void               nemo_directory_finalize         (GObject                *object);
static NemoDirectory *nemo_directory_new              (GFile                  *location);
static GList *            real_get_file_list                  (NemoDirectory      *directory);
static gboolean	  real_is_editable                    (NemoDirectory      *directory);
static void               set_directory_location              (NemoDirectory      *directory,
					       GFile                  *location);

G_DEFINE_TYPE (NemoDirectory, nemo_directory, G_TYPE_OBJECT);

static void
nemo_directory_class_init (NemoDirectoryClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = nemo_directory_finalize;

	signals[FILES_ADDED] =
		g_signal_new ("files_added",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NemoDirectoryClass, files_added),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__POINTER,
		              G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[FILES_CHANGED] =
		g_signal_new ("files_changed",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NemoDirectoryClass, files_changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__POINTER,
		              G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[DONE_LOADING] =
		g_signal_new ("done_loading",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NemoDirectoryClass, done_loading),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[LOAD_ERROR] =
		g_signal_new ("load_error",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NemoDirectoryClass, load_error),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__POINTER,
		              G_TYPE_NONE, 1, G_TYPE_POINTER);

	klass->get_file_list = real_get_file_list;
	klass->is_editable = real_is_editable;

	g_type_class_add_private (klass, sizeof (NemoDirectoryDetails));
}

static void
nemo_directory_init (NemoDirectory *directory)
{
	directory->details = G_TYPE_INSTANCE_GET_PRIVATE ((directory), NEMO_TYPE_DIRECTORY, NemoDirectoryDetails);
	directory->details->file_hash = g_hash_table_new (g_str_hash, g_str_equal);
	directory->details->high_priority_queue = nemo_file_queue_new ();
	directory->details->low_priority_queue = nemo_file_queue_new ();
	directory->details->extension_queue = nemo_file_queue_new ();
    directory->details->max_deferred_file_count = g_settings_get_int (nemo_preferences,
                                                                      NEMO_PREFERENCES_DEFERRED_ATTR_PRELOAD_LIMIT);
}

NemoDirectory *
nemo_directory_ref (NemoDirectory *directory)
{
	if (directory == NULL) {
		return directory;
	}

	g_return_val_if_fail (NEMO_IS_DIRECTORY (directory), NULL);

	g_object_ref (directory);
	return directory;
}

void
nemo_directory_unref (NemoDirectory *directory)
{
	if (directory == NULL) {
		return;
	}

	g_return_if_fail (NEMO_IS_DIRECTORY (directory));

	g_object_unref (directory);
}

static void
nemo_directory_finalize (GObject *object)
{
	NemoDirectory *directory;

	directory = NEMO_DIRECTORY (object);

	g_hash_table_remove (directories, directory->details->location);

	nemo_directory_cancel (directory);
	g_assert (directory->details->count_in_progress == NULL);

	if (directory->details->monitor_list != NULL) {
		g_warning ("destroying a NemoDirectory while it's being monitored");
		g_list_free_full (directory->details->monitor_list, g_free);
	}

	if (directory->details->monitor != NULL) {
		nemo_monitor_cancel (directory->details->monitor);
	}

	if (directory->details->dequeue_pending_idle_id != 0) {
		g_source_remove (directory->details->dequeue_pending_idle_id);
	}

	if (directory->details->call_ready_idle_id != 0) {
		g_source_remove (directory->details->call_ready_idle_id);
	}

	if (directory->details->location) {
		g_object_unref (directory->details->location);
	}

	g_assert (directory->details->file_list == NULL);
	g_hash_table_destroy (directory->details->file_hash);

	nemo_file_queue_destroy (directory->details->high_priority_queue);
	nemo_file_queue_destroy (directory->details->low_priority_queue);
	nemo_file_queue_destroy (directory->details->extension_queue);
	g_assert (directory->details->directory_load_in_progress == NULL);
	g_assert (directory->details->count_in_progress == NULL);
	g_assert (directory->details->dequeue_pending_idle_id == 0);
	g_list_free_full (directory->details->pending_file_info, g_object_unref);

	G_OBJECT_CLASS (nemo_directory_parent_class)->finalize (object);
}

static void
invalidate_one_count (gpointer key, gpointer value, gpointer user_data)
{
	NemoDirectory *directory;

	g_assert (key != NULL);
	g_assert (NEMO_IS_DIRECTORY (value));
	g_assert (user_data == NULL);

	directory = NEMO_DIRECTORY (value);	
	nemo_directory_invalidate_count_and_mime_list (directory);
}

static void
filtering_changed_callback (gpointer callback_data)
{
	g_assert (callback_data == NULL);

	g_hash_table_foreach (directories, invalidate_one_count, NULL);
}

void
emit_change_signals_for_all_files (NemoDirectory *directory)
{
	GList *files;

	files = g_list_copy (directory->details->file_list);
	if (directory->details->as_file != NULL) {
		files = g_list_prepend (files, directory->details->as_file);
	}

	nemo_file_list_ref (files);
	nemo_directory_emit_change_signals (directory, files);

	nemo_file_list_free (files);
}

static void
collect_all_directories (gpointer key, gpointer value, gpointer callback_data)
{
	NemoDirectory *directory;
	GList **dirs;

	directory = NEMO_DIRECTORY (value);
	dirs = callback_data;

	*dirs = g_list_prepend (*dirs, nemo_directory_ref (directory));
}

void
emit_change_signals_for_all_files_in_all_directories (void)
{
	GList *dirs, *l;
	NemoDirectory *directory;

	dirs = NULL;
	g_hash_table_foreach (directories,
			      collect_all_directories,
			      &dirs);

	for (l = dirs; l != NULL; l = l->next) {
		directory = NEMO_DIRECTORY (l->data);
		emit_change_signals_for_all_files (directory);
		nemo_directory_unref (directory);
	}

	g_list_free (dirs);
}

static void
async_state_changed_one (gpointer key, gpointer value, gpointer user_data)
{
	NemoDirectory *directory;

	g_assert (key != NULL);
	g_assert (NEMO_IS_DIRECTORY (value));
	g_assert (user_data == NULL);

	directory = NEMO_DIRECTORY (value);	
	nemo_directory_async_state_changed (directory);
	emit_change_signals_for_all_files (directory);
}

static void
async_data_preference_changed_callback (gpointer callback_data)
{
	g_assert (callback_data == NULL);

	g_hash_table_foreach (directories, async_state_changed_one, NULL);
}

static void
add_preferences_callbacks (void)
{
	nemo_global_preferences_init ();

	g_signal_connect_swapped (nemo_preferences,
			  "changed::" NEMO_PREFERENCES_SHOW_HIDDEN_FILES,
			  G_CALLBACK (filtering_changed_callback), NULL);
	g_signal_connect_swapped (nemo_preferences,
			  "changed::" NEMO_PREFERENCES_SHOW_BACKUP_FILES,
			  G_CALLBACK (filtering_changed_callback), NULL);
	g_signal_connect_swapped (nemo_preferences,
			  "changed::" NEMO_PREFERENCES_DATE_FORMAT,
			  G_CALLBACK (async_data_preference_changed_callback), NULL);
	g_signal_connect_swapped (nemo_preferences,
			  "changed::" NEMO_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
			  G_CALLBACK (async_data_preference_changed_callback), NULL);
	g_signal_connect_swapped (nemo_preferences,
			  "changed::" NEMO_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
			  G_CALLBACK (async_data_preference_changed_callback), NULL);
}

NemoDirectory *
nemo_directory_get (GFile *location)
{
	NemoDirectory *directory;

	g_return_val_if_fail (G_IS_FILE (location), NULL);

	if (directories == NULL) {
		directories = g_hash_table_new_full (g_direct_hash, g_direct_equal,
			   NULL, g_object_unref);
		add_preferences_callbacks ();
	}

	directory = g_hash_table_lookup (directories, location);
	if (directory == NULL) {
		directory = nemo_directory_new (location);
		g_hash_table_insert (directories, g_object_ref (location), directory);
	}

	return nemo_directory_ref (directory);
}

void
nemo_directory_force_reload (NemoDirectory *directory)
{
	g_return_if_fail (NEMO_IS_DIRECTORY (directory));

	g_hash_table_remove_all (directory->details->file_hash);

	if (directory->details->file_list != NULL) {
		nemo_file_list_free (directory->details->file_list);
		directory->details->file_list = NULL;
	}

	directory->details->loading = FALSE;
	directory->details->done_loading = FALSE;
	directory->details->load_error = FALSE;

	nemo_directory_cancel (directory);
	nemo_directory_load (directory);
}

void
nemo_directory_invalidate_file_hash (NemoDirectory *directory)
{
	g_return_if_fail (NEMO_IS_DIRECTORY (directory));

	g_hash_table_remove_all (directory->details->file_hash);

	if (directory->details->file_list != NULL) {
		nemo_file_list_free (directory->details->file_list);
		directory->details->file_list = NULL;
	}

	directory->details->done_loading = FALSE;
	directory->details->loading = FALSE;
	nemo_directory_load (directory);
}

static NemoDirectory *
nemo_directory_new (GFile *location)
{
	NemoDirectory *directory;

	directory = g_object_new (NEMO_TYPE_DIRECTORY, NULL);
	set_directory_location (directory, location);

	return directory;
}

static void
set_directory_location (NemoDirectory *directory,
			    GFile *location)
{
	g_return_if_fail (NEMO_IS_DIRECTORY (directory));
	g_return_if_fail (G_IS_FILE (location));

	if (directory->details->location != NULL) {
		g_object_unref (directory->details->location);
	}

	directory->details->location = g_object_ref (location);
}

GList *
nemo_directory_get_file_list (NemoDirectory *directory)
{
	g_return_val_if_fail (NEMO_IS_DIRECTORY (directory), NULL);

	return NEMO_DIRECTORY_GET_CLASS (directory)->get_file_list (directory);
}

static GList *
real_get_file_list (NemoDirectory *directory)
{
	GList *list;

	list = directory->details->file_list;
	nemo_file_list_ref (list);

	return list;
}

gboolean
nemo_directory_is_editable (NemoDirectory *directory)
{
	g_return_val_if_fail (NEMO_IS_DIRECTORY (directory), FALSE);

	return NEMO_DIRECTORY_GET_CLASS (directory)->is_editable (directory);
}

static gboolean
real_is_editable (NemoDirectory *directory)
{
	return TRUE;
}

gboolean
nemo_directory_are_files_pending (NemoDirectory *directory)
{
	g_return_val_if_fail (NEMO_IS_DIRECTORY (directory), FALSE);

	return directory->details->pending_file_info != NULL;
}

void
nemo_directory_cancel (NemoDirectory *directory)
{
	g_return_if_fail (NEMO_IS_DIRECTORY (directory));

	if (directory->details->directory_load_in_progress != NULL) {
		g_cancellable_cancel (directory->details->directory_load_in_progress);
		g_clear_object (&directory->details->directory_load_in_progress);
	}

	nemo_file_queue_clear (directory->details->high_priority_queue);
	nemo_file_queue_clear (directory->details->low_priority_queue);
	nemo_file_queue_clear (directory->details->extension_queue);

	if (directory->details->dequeue_pending_idle_id != 0) {
		g_source_remove (directory->details->dequeue_pending_idle_id);
		directory->details->dequeue_pending_idle_id = 0;
	}

	if (directory->details->call_ready_idle_id != 0) {
		g_source_remove (directory->details->call_ready_idle_id);
		directory->details->call_ready_idle_id = 0;
	}
}

void
nemo_directory_load (NemoDirectory *directory)
{
	g_return_if_fail (NEMO_IS_DIRECTORY (directory));

	if (directory->details->loading || directory->details->done_loading) {
		return;
	}

	directory->details->loading = TRUE;
	nemo_vfs_directory_load (directory);
}

void
nemo_directory_add_file (NemoDirectory *directory,
			   NemoFile *file)
{
	const char *name;

	g_return_if_fail (NEMO_IS_DIRECTORY (directory));
	g_return_if_fail (NEMO_IS_FILE (file));

	name = nemo_file_get_name (file);
	if (name == NULL) {
		return;
	}

	if (g_hash_table_lookup (directory->details->file_hash, name) != NULL) {
		g_hash_table_remove (directory->details->file_hash, name);
	}

	g_hash_table_insert (directory->details->file_hash, g_strdup (name), nemo_file_ref (file));
	directory->details->file_list = g_list_prepend (directory->details->file_list, nemo_file_ref (file));
}

void
nemo_directory_remove_file (NemoDirectory *directory,
			    NemoFile *file)
{
	const char *name;

	g_return_if_fail (NEMO_IS_DIRECTORY (directory));
	g_return_if_fail (NEMO_IS_FILE (file));

	name = nemo_file_get_name (file);
	if (name == NULL) {
		return;
	}

	g_hash_table_remove (directory->details->file_hash, name);
	directory->details->file_list = g_list_remove (directory->details->file_list, file);
}

NemoFile *
nemo_directory_get_file_by_name (NemoDirectory *directory,
				   const char *name)
{
	NemoFile *file;

	g_return_val_if_fail (NEMO_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	file = g_hash_table_lookup (directory->details->file_hash, name);
	if (file != NULL) {
		return nemo_file_ref (file);
	}

	return NULL;
}

void
nemo_directory_emit_change_signals (NemoDirectory *directory,
			      GList *files)
{
	g_return_if_fail (NEMO_IS_DIRECTORY (directory));
	g_return_if_fail (files != NULL);

	g_signal_emit (directory, signals[FILES_CHANGED], 0, files);
}

void
nemo_directory_emit_files_added (NemoDirectory *directory,
			   GList *files)
{
	g_return_if_fail (NEMO_IS_DIRECTORY (directory));
	g_return_if_fail (files != NULL);

	g_signal_emit (directory, signals[FILES_ADDED], 0, files);
}

void
nemo_directory_emit_done_loading (NemoDirectory *directory)
{
	g_return_if_fail (NEMO_IS_DIRECTORY (directory));

	g_signal_emit (directory, signals[DONE_LOADING], 0);
}

void
nemo_directory_emit_load_error (NemoDirectory *directory,
			  GError *error)
{
	g_return_if_fail (NEMO_IS_DIRECTORY (directory));
	g_return_if_fail (error != NULL);

	g_signal_emit (directory, signals[LOAD_ERROR], 0, error);
}

NemoMonitor *
nemo_directory_get_monitor (NemoDirectory *directory)
{
	g_return_val_if_fail (NEMO_IS_DIRECTORY (directory), NULL);

	if (directory->details->monitor == NULL) {
		directory->details->monitor = nemo_monitor_new (directory->details->location);
	}

	return g_object_ref (directory->details->monitor);
}

void
nemo_directory_add_monitor (NemoDirectory *directory,
			  NemoMonitor *monitor)
{
	g_return_if_fail (NEMO_IS_DIRECTORY (directory));
	g_return_if_fail (NEMO_IS_MONITOR (monitor));

	directory->details->monitor_list = g_list_prepend (directory->details->monitor_list,
			    g_object_ref (monitor));
}

void
nemo_directory_remove_monitor (NemoDirectory *directory,
			   NemoMonitor *monitor)
{
	g_return_if_fail (NEMO_IS_DIRECTORY (directory));
	g_return_if_fail (NEMO_IS_MONITOR (monitor));

	directory->details->monitor_list = g_list_remove (directory->details->monitor_list, monitor);
}

void
nemo_directory_invalidate_count_and_mime_list (NemoDirectory *directory)
{
	g_return_if_fail (NEMO_IS_DIRECTORY (directory));

	directory->details->count_valid = FALSE;
	directory->details->mime_list_valid = FALSE;
}

void
nemo_directory_async_state_changed (NemoDirectory *directory)
{
	g_return_if_fail (NEMO_IS_DIRECTORY (directory));

	if (directory->details->file_list != NULL) {
		GList *l;
		for (l = directory->details->file_list; l != NULL; l = l->next) {
			nemo_file_async_state_changed (NEMO_FILE (l->data));
		}
	}
}

GFile *
nemo_directory_get_location (NemoDirectory *directory)
{
	g_return_val_if_fail (NEMO_IS_DIRECTORY (directory), NULL);

	return g_object_ref (directory->details->location);
}

void
nemo_directory_set_as_file (NemoDirectory *directory,
			   NemoFile *file)
{
	g_return_if_fail (NEMO_IS_DIRECTORY (directory));

	if (directory->details->as_file != NULL) {
		nemo_file_unref (directory->details->as_file);
	}

	directory->details->as_file = nemo_file_ref (file);
}

NemoFile *
nemo_directory_get_as_file (NemoDirectory *directory)
{
	g_return_val_if_fail (NEMO_IS_DIRECTORY (directory), NULL);

	if (directory->details->as_file != NULL) {
		return nemo_file_ref (directory->details->as_file);
	}

	return NULL;
}

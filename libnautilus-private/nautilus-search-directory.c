/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
   Copyright (C) 2005 Novell, Inc
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
  
   Author: Anders Carlsson <andersca@imendio.com>
*/

#include <config.h>
#include "nautilus-search-directory.h"
#include "nautilus-search-directory-file.h"

#include "nautilus-directory-private.h"
#include "nautilus-file.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-search-provider.h"
#include "nautilus-search-engine.h"
#include "nautilus-search-engine-model.h"

#include <eel/eel-glib-extensions.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <string.h>
#include <sys/time.h>

struct NautilusSearchDirectoryDetails {
	NautilusQuery *query;
	char *saved_search_uri;
	gboolean modified;

	NautilusSearchEngine *engine;

	gboolean search_running;
	gboolean search_loaded;

	GList *files;
	GHashTable *files_hash;

	GList *monitor_list;
	GList *callback_list;
	GList *pending_callback_list;

	NautilusDirectory *base_model;
};

typedef struct {
	gboolean monitor_hidden_files;
	NautilusFileAttributes monitor_attributes;

	gconstpointer client;
} SearchMonitor;

typedef struct {
	NautilusSearchDirectory *search_directory;

	NautilusDirectoryCallback callback;
	gpointer callback_data;

	NautilusFileAttributes wait_for_attributes;
	gboolean wait_for_file_list;
	GList *file_list;
	GHashTable *non_ready_hash;
} SearchCallback;

enum {
	PROP_0,
	PROP_BASE_MODEL,
	PROP_QUERY,
	NUM_PROPERTIES
};

G_DEFINE_TYPE (NautilusSearchDirectory, nautilus_search_directory,
	       NAUTILUS_TYPE_DIRECTORY);

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static void search_engine_hits_added (NautilusSearchEngine *engine, GList *hits, NautilusSearchDirectory *search);
static void search_engine_error (NautilusSearchEngine *engine, const char *error, NautilusSearchDirectory *search);
static void search_callback_file_ready_callback (NautilusFile *file, gpointer data);
static void file_changed (NautilusFile *file, NautilusSearchDirectory *search);

static void
reset_file_list (NautilusSearchDirectory *search)
{
	GList *list, *monitor_list;
	NautilusFile *file;
	SearchMonitor *monitor;

	/* Remove file connections */
	for (list = search->details->files; list != NULL; list = list->next) {
		file = list->data;

		/* Disconnect change handler */
		g_signal_handlers_disconnect_by_func (file, file_changed, search);

		/* Remove monitors */
		for (monitor_list = search->details->monitor_list; monitor_list; 
		     monitor_list = monitor_list->next) {
			monitor = monitor_list->data;
			nautilus_file_monitor_remove (file, monitor);
		}
	}
	
	nautilus_file_list_free (search->details->files);
	search->details->files = NULL;

	g_hash_table_remove_all (search->details->files_hash);
}

static void
set_hidden_files (NautilusSearchDirectory *search)
{
	GList *l;
	SearchMonitor *monitor;
	gboolean monitor_hidden = FALSE;

	for (l = search->details->monitor_list; l != NULL; l = l->next) {
		monitor = l->data;
		monitor_hidden |= monitor->monitor_hidden_files;

		if (monitor_hidden) {
			break;
		}
	}

	nautilus_query_set_show_hidden_files (search->details->query, monitor_hidden);
}

static void
start_search (NautilusSearchDirectory *search)
{
	NautilusSearchEngineModel *model_provider;
	NautilusSearchEngineSimple *simple_provider;

	if (!search->details->query) {
		return;
	}

	if (search->details->search_running) {
		return;
	}

	if (!search->details->monitor_list && !search->details->pending_callback_list) {
		return;
	}

	/* We need to start the search engine */
	search->details->search_running = TRUE;
	search->details->search_loaded = FALSE;

	set_hidden_files (search);
	nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (search->details->engine),
					    search->details->query);

	model_provider = nautilus_search_engine_get_model_provider (search->details->engine);
	nautilus_search_engine_model_set_model (model_provider, search->details->base_model);

	simple_provider = nautilus_search_engine_get_simple_provider (search->details->engine);
	g_object_set (simple_provider, "recursive", TRUE, NULL);

	reset_file_list (search);

	nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (search->details->engine));
}

static void
stop_search (NautilusSearchDirectory *search)
{
	if (!search->details->search_running) {
		return;
	}

	search->details->search_running = FALSE;
	nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (search->details->engine));

	reset_file_list (search);
}

static void
file_changed (NautilusFile *file, NautilusSearchDirectory *search)
{
	GList list;

	list.data = file;
	list.next = NULL;

	nautilus_directory_emit_files_changed (NAUTILUS_DIRECTORY (search), &list);
}

static void
search_monitor_add (NautilusDirectory *directory,
		    gconstpointer client,
		    gboolean monitor_hidden_files,
		    NautilusFileAttributes file_attributes,
		    NautilusDirectoryCallback callback,
		    gpointer callback_data)
{
	GList *list;
	SearchMonitor *monitor;
	NautilusSearchDirectory *search;
	NautilusFile *file;

	search = NAUTILUS_SEARCH_DIRECTORY (directory);

	monitor = g_new0 (SearchMonitor, 1);
	monitor->monitor_hidden_files = monitor_hidden_files;
	monitor->monitor_attributes = file_attributes;
	monitor->client = client;

	search->details->monitor_list = g_list_prepend (search->details->monitor_list, monitor);
	
	if (callback != NULL) {
		(* callback) (directory, search->details->files, callback_data);
	}
	
	for (list = search->details->files; list != NULL; list = list->next) {
		file = list->data;

		/* Add monitors */
		nautilus_file_monitor_add (file, monitor, file_attributes);
	}

	start_search (search);
}

static void
search_monitor_remove_file_monitors (SearchMonitor *monitor, NautilusSearchDirectory *search)
{
	GList *list;
	NautilusFile *file;
	
	for (list = search->details->files; list != NULL; list = list->next) {
		file = list->data;

		nautilus_file_monitor_remove (file, monitor);
	}
}

static void
search_monitor_destroy (SearchMonitor *monitor, NautilusSearchDirectory *search)
{
	search_monitor_remove_file_monitors (monitor, search);

	g_free (monitor);
}

static void
search_monitor_remove (NautilusDirectory *directory,
		       gconstpointer client)
{
	NautilusSearchDirectory *search;
	SearchMonitor *monitor;
	GList *list;

	search = NAUTILUS_SEARCH_DIRECTORY (directory);

	for (list = search->details->monitor_list; list != NULL; list = list->next) {
		monitor = list->data;

		if (monitor->client == client) {
			search->details->monitor_list = g_list_delete_link (search->details->monitor_list, list);

			search_monitor_destroy (monitor, search);

			break;
		}
	}

	if (!search->details->monitor_list) {
		stop_search (search);
	}
}

static void
cancel_call_when_ready (gpointer key, gpointer value, gpointer user_data)
{
	SearchCallback *search_callback;
	NautilusFile *file;

	file = key;
	search_callback = user_data;

	nautilus_file_cancel_call_when_ready (file, search_callback_file_ready_callback,
					      search_callback);
}

static void
search_callback_destroy (SearchCallback *search_callback)
{
	if (search_callback->non_ready_hash) {
		g_hash_table_foreach (search_callback->non_ready_hash, cancel_call_when_ready, search_callback);
		g_hash_table_destroy (search_callback->non_ready_hash);
	}

	nautilus_file_list_free (search_callback->file_list);

	g_free (search_callback);
}

static void
search_callback_invoke_and_destroy (SearchCallback *search_callback)
{
	search_callback->callback (NAUTILUS_DIRECTORY (search_callback->search_directory),
				   search_callback->file_list,
				   search_callback->callback_data);

	search_callback->search_directory->details->callback_list = 
		g_list_remove (search_callback->search_directory->details->callback_list, search_callback);

	search_callback_destroy (search_callback);
}

static void
search_callback_file_ready_callback (NautilusFile *file, gpointer data)
{
	SearchCallback *search_callback = data;
	
	g_hash_table_remove (search_callback->non_ready_hash, file);

	if (g_hash_table_size (search_callback->non_ready_hash) == 0) {
		search_callback_invoke_and_destroy (search_callback);
	}
}

static void
search_callback_add_file_callbacks (SearchCallback *callback)
{
	GList *file_list_copy, *list;
	NautilusFile *file;

	file_list_copy = g_list_copy (callback->file_list);

	for (list = file_list_copy; list != NULL; list = list->next) {
		file = list->data;

		nautilus_file_call_when_ready (file,
					       callback->wait_for_attributes,
					       search_callback_file_ready_callback,
					       callback);
	}
	g_list_free (file_list_copy);
}
	 
static SearchCallback *
search_callback_find (NautilusSearchDirectory *search, NautilusDirectoryCallback callback, gpointer callback_data)
{
	SearchCallback *search_callback;
	GList *list;

	for (list = search->details->callback_list; list != NULL; list = list->next) {
		search_callback = list->data;

		if (search_callback->callback == callback &&
		    search_callback->callback_data == callback_data) {
			return search_callback;
		}
	}
	
	return NULL;
}

static SearchCallback *
search_callback_find_pending (NautilusSearchDirectory *search, NautilusDirectoryCallback callback, gpointer callback_data)
{
	SearchCallback *search_callback;
	GList *list;

	for (list = search->details->pending_callback_list; list != NULL; list = list->next) {
		search_callback = list->data;

		if (search_callback->callback == callback &&
		    search_callback->callback_data == callback_data) {
			return search_callback;
		}
	}
	
	return NULL;
}

static GHashTable *
file_list_to_hash_table (GList *file_list)
{
	GList *list;
	GHashTable *table;

	if (!file_list)
		return NULL;

	table = g_hash_table_new (NULL, NULL);

	for (list = file_list; list != NULL; list = list->next) {
		g_hash_table_insert (table, list->data, list->data);
	}

	return table;
}

static void
search_call_when_ready (NautilusDirectory *directory,
			NautilusFileAttributes file_attributes,
			gboolean wait_for_file_list,
			NautilusDirectoryCallback callback,
			gpointer callback_data)
{
	NautilusSearchDirectory *search;
	SearchCallback *search_callback;

	search = NAUTILUS_SEARCH_DIRECTORY (directory);

	search_callback = search_callback_find (search, callback, callback_data);
	if (search_callback == NULL) {
		search_callback = search_callback_find_pending (search, callback, callback_data);
	}
	
	if (search_callback) {
		g_warning ("tried to add a new callback while an old one was pending");
		return;
	}

	search_callback = g_new0 (SearchCallback, 1);
	search_callback->search_directory = search;
	search_callback->callback = callback;
	search_callback->callback_data = callback_data;
	search_callback->wait_for_attributes = file_attributes;
	search_callback->wait_for_file_list = wait_for_file_list;

	if (wait_for_file_list && !search->details->search_loaded) {
		/* Add it to the pending callback list, which will be
		 * processed when the directory has finished loading
		 */
		search->details->pending_callback_list = 
			g_list_prepend (search->details->pending_callback_list, search_callback);

		/* We might need to start the search engine */
		start_search (search);
	} else {
		search_callback->file_list = nautilus_file_list_copy (search->details->files);
		search_callback->non_ready_hash = file_list_to_hash_table (search->details->files);

		if (!search_callback->non_ready_hash) {
			/* If there are no ready files, we invoke the callback
			   with an empty list.
			*/
			search_callback_invoke_and_destroy (search_callback);
		} else {
			search->details->callback_list = g_list_prepend (search->details->callback_list, search_callback);
			search_callback_add_file_callbacks (search_callback);
		}
	}
}

static void
search_cancel_callback (NautilusDirectory *directory,
			NautilusDirectoryCallback callback,
			gpointer callback_data)
{
	NautilusSearchDirectory *search;
	SearchCallback *search_callback;

	search = NAUTILUS_SEARCH_DIRECTORY (directory);
	search_callback = search_callback_find (search, callback, callback_data);
	
	if (search_callback) {
		search->details->callback_list = g_list_remove (search->details->callback_list, search_callback);
		
		search_callback_destroy (search_callback);
		
		return;
	} 

	/* Check for a pending callback */
	search_callback = search_callback_find_pending (search, callback, callback_data);

	if (search_callback) {
		search->details->pending_callback_list = g_list_remove (search->details->pending_callback_list, search_callback);

		search_callback_destroy (search_callback);

		if (!search->details->pending_callback_list) {
			stop_search (search);
		}
	}
}

static void
search_callback_add_pending_file_callbacks (SearchCallback *callback)
{
	callback->file_list = nautilus_file_list_copy (callback->search_directory->details->files);
	callback->non_ready_hash = file_list_to_hash_table (callback->search_directory->details->files);

	search_callback_add_file_callbacks (callback);
}

static void
search_directory_ensure_loaded (NautilusSearchDirectory *search)
{
	if (search->details->search_loaded) {
		return;
	}

	search->details->search_loaded = TRUE;
	nautilus_directory_emit_done_loading (NAUTILUS_DIRECTORY (search));

	/* Add all file callbacks */
	g_list_foreach (search->details->pending_callback_list,
			(GFunc)search_callback_add_pending_file_callbacks, NULL);
	search->details->callback_list = g_list_concat (search->details->callback_list,
							search->details->pending_callback_list);

	g_list_free (search->details->pending_callback_list);
	search->details->pending_callback_list = NULL;
}

static void
search_engine_hits_added (NautilusSearchEngine *engine, GList *hits, 
			  NautilusSearchDirectory *search)
{
	GList *hit_list;
	GList *file_list;
	NautilusFile *file;
	SearchMonitor *monitor;
	GList *monitor_list;

	file_list = NULL;

	for (hit_list = hits; hit_list != NULL; hit_list = hit_list->next) {
		NautilusSearchHit *hit = hit_list->data;
		const char *uri;

		uri = nautilus_search_hit_get_uri (hit);
		if (g_str_has_suffix (uri, NAUTILUS_SAVED_SEARCH_EXTENSION)) {
			/* Never return saved searches themselves as hits */
			continue;
		}

		nautilus_search_hit_compute_scores (hit, search->details->query);

		file = nautilus_file_get_by_uri (uri);
		nautilus_file_set_search_relevance (file, nautilus_search_hit_get_relevance (hit));

		for (monitor_list = search->details->monitor_list; monitor_list; monitor_list = monitor_list->next) {
			monitor = monitor_list->data;

			/* Add monitors */
			nautilus_file_monitor_add (file, monitor, monitor->monitor_attributes);
		}

		g_signal_connect (file, "changed", G_CALLBACK (file_changed), search),

		file_list = g_list_prepend (file_list, file);
		g_hash_table_add (search->details->files_hash, file);
	}
	
	search->details->files = g_list_concat (search->details->files, file_list);

	nautilus_directory_emit_files_added (NAUTILUS_DIRECTORY (search), file_list);

	file = nautilus_directory_get_corresponding_file (NAUTILUS_DIRECTORY (search));
	nautilus_file_emit_changed (file);
	nautilus_file_unref (file);

	search_directory_ensure_loaded (search);
}

static void
search_engine_error (NautilusSearchEngine *engine, const char *error_message, NautilusSearchDirectory *search)
{
	GError *error;

	error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
				     error_message);
	nautilus_directory_emit_load_error (NAUTILUS_DIRECTORY (search),
					    error);
	g_error_free (error);
}

static void
search_engine_finished (NautilusSearchEngine *engine, NautilusSearchDirectory *search)
{
	search_directory_ensure_loaded (search);
}

static void
search_force_reload (NautilusDirectory *directory)
{
	NautilusSearchDirectory *search;

	search = NAUTILUS_SEARCH_DIRECTORY (directory);

	if (!search->details->query) {
		return;
	}
	
	search->details->search_loaded = FALSE;

	/* Remove file monitors */
	reset_file_list (search);
	stop_search (search);
}

static gboolean
search_are_all_files_seen (NautilusDirectory *directory)
{
	NautilusSearchDirectory *search;

	search = NAUTILUS_SEARCH_DIRECTORY (directory);

	return (!search->details->query ||
		search->details->search_loaded);
}

static gboolean
search_contains_file (NautilusDirectory *directory,
		      NautilusFile *file)
{
	NautilusSearchDirectory *search;

	search = NAUTILUS_SEARCH_DIRECTORY (directory);
	return (g_hash_table_lookup (search->details->files_hash, file) != NULL);
}

static GList *
search_get_file_list (NautilusDirectory *directory)
{
	NautilusSearchDirectory *search;

	search = NAUTILUS_SEARCH_DIRECTORY (directory);

	return nautilus_file_list_copy (search->details->files);
}


static gboolean
search_is_editable (NautilusDirectory *directory)
{
	return FALSE;
}

static void
search_set_property (GObject *object,
		     guint property_id,
		     const GValue *value,
		     GParamSpec *pspec)
{
	NautilusSearchDirectory *search = NAUTILUS_SEARCH_DIRECTORY (object);

	switch (property_id) {
	case PROP_BASE_MODEL:
		nautilus_search_directory_set_base_model (search, g_value_get_object (value));
		break;
	case PROP_QUERY:
		nautilus_search_directory_set_query (search, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
search_get_property (GObject *object,
		     guint property_id,
		     GValue *value,
		     GParamSpec *pspec)
{
	NautilusSearchDirectory *search = NAUTILUS_SEARCH_DIRECTORY (object);

	switch (property_id) {
	case PROP_BASE_MODEL:
		g_value_set_object (value, nautilus_search_directory_get_base_model (search));
		break;
	case PROP_QUERY:
		g_value_take_object (value, nautilus_search_directory_get_query (search));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
clear_base_model (NautilusSearchDirectory *search)
{
	if (search->details->base_model != NULL) {
		nautilus_directory_file_monitor_remove (search->details->base_model,
							&search->details->base_model);
		g_clear_object (&search->details->base_model);
	}
}

static void
search_dispose (GObject *object)
{
	NautilusSearchDirectory *search;
	GList *list;

	search = NAUTILUS_SEARCH_DIRECTORY (object);

	clear_base_model (search);

	/* Remove search monitors */
	if (search->details->monitor_list) {
		for (list = search->details->monitor_list; list != NULL; list = list->next) {
			search_monitor_destroy ((SearchMonitor *)list->data, search);
		}

		g_list_free (search->details->monitor_list);
		search->details->monitor_list = NULL;
	}

	reset_file_list (search);
	
	if (search->details->callback_list) {
		/* Remove callbacks */
		g_list_foreach (search->details->callback_list,
				(GFunc)search_callback_destroy, NULL);
		g_list_free (search->details->callback_list);
		search->details->callback_list = NULL;
	}

	if (search->details->pending_callback_list) {
		g_list_foreach (search->details->pending_callback_list,
				(GFunc)search_callback_destroy, NULL);
		g_list_free (search->details->pending_callback_list);
		search->details->pending_callback_list = NULL;
	}

	g_clear_object (&search->details->query);
	stop_search (search);

	g_clear_object (&search->details->engine);
	
	G_OBJECT_CLASS (nautilus_search_directory_parent_class)->dispose (object);
}

static void
search_finalize (GObject *object)
{
	NautilusSearchDirectory *search;

	search = NAUTILUS_SEARCH_DIRECTORY (object);
	g_free (search->details->saved_search_uri);

	g_hash_table_destroy (search->details->files_hash);

	G_OBJECT_CLASS (nautilus_search_directory_parent_class)->finalize (object);
}

static void
nautilus_search_directory_init (NautilusSearchDirectory *search)
{
	search->details = G_TYPE_INSTANCE_GET_PRIVATE (search, NAUTILUS_TYPE_SEARCH_DIRECTORY,
						       NautilusSearchDirectoryDetails);

	search->details->files_hash = g_hash_table_new (g_direct_hash, g_direct_equal);

	search->details->engine = nautilus_search_engine_new ();
	g_signal_connect (search->details->engine, "hits-added",
			  G_CALLBACK (search_engine_hits_added),
			  search);
	g_signal_connect (search->details->engine, "error",
			  G_CALLBACK (search_engine_error),
			  search);
	g_signal_connect (search->details->engine, "finished",
			  G_CALLBACK (search_engine_finished),
			  search);
}

static void
nautilus_search_directory_class_init (NautilusSearchDirectoryClass *class)
{
	NautilusDirectoryClass *directory_class = NAUTILUS_DIRECTORY_CLASS (class);
	GObjectClass *oclass = G_OBJECT_CLASS (class);

	oclass->dispose = search_dispose;
	oclass->finalize = search_finalize;
	oclass->get_property = search_get_property;
	oclass->set_property = search_set_property;

 	directory_class->are_all_files_seen = search_are_all_files_seen;
	directory_class->contains_file = search_contains_file;
	directory_class->force_reload = search_force_reload;
	directory_class->call_when_ready = search_call_when_ready;
	directory_class->cancel_callback = search_cancel_callback;

	directory_class->file_monitor_add = search_monitor_add;
	directory_class->file_monitor_remove = search_monitor_remove;
	
	directory_class->get_file_list = search_get_file_list;
	directory_class->is_editable = search_is_editable;

	properties[PROP_BASE_MODEL] =
		g_param_spec_object ("base-model",
				     "The base model",
				     "The base directory model for this directory",
				     NAUTILUS_TYPE_DIRECTORY,
				     G_PARAM_READWRITE);
	properties[PROP_QUERY] =
		g_param_spec_object ("query",
				     "The query",
				     "The query for this search directory",
				     NAUTILUS_TYPE_QUERY,
				     G_PARAM_READWRITE);

	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
	g_type_class_add_private (class, sizeof (NautilusSearchDirectoryDetails));
}

void
nautilus_search_directory_set_base_model (NautilusSearchDirectory *search,
					  NautilusDirectory *base_model)
{
	if (search->details->base_model == base_model) {
		return;
	}

	if (search->details->query != NULL) {
		gchar *uri;
		GFile *query_location, *model_location;
		gboolean is_equal;

		uri = nautilus_query_get_location (search->details->query);
		query_location = g_file_new_for_uri (uri);
		model_location = nautilus_directory_get_location (base_model);

		is_equal = g_file_equal (model_location, query_location);

		g_object_unref (model_location);
		g_object_unref (query_location);
		g_free (uri);

		if (!is_equal) {
			return;
		}
	}

	clear_base_model (search);
	search->details->base_model = nautilus_directory_ref (base_model);

	if (search->details->base_model != NULL) {
		nautilus_directory_file_monitor_add (base_model, &search->details->base_model,
						     TRUE, NAUTILUS_FILE_ATTRIBUTE_INFO,
						     NULL, NULL);
	}

	g_object_notify_by_pspec (G_OBJECT (search), properties[PROP_BASE_MODEL]);
}

NautilusDirectory *
nautilus_search_directory_get_base_model (NautilusSearchDirectory *search)
{
	return search->details->base_model;
}

char *
nautilus_search_directory_generate_new_uri (void)
{
	static int counter = 0;
	char *uri;

	uri = g_strdup_printf (EEL_SEARCH_URI"//%d/", counter++);

	return uri;
}

void
nautilus_search_directory_set_query (NautilusSearchDirectory *search,
				     NautilusQuery *query)
{
	NautilusFile *file;

	if (search->details->query != query) {
		search->details->modified = TRUE;

		g_object_ref (query);
		g_clear_object (&search->details->query);
		search->details->query = query;

		g_object_notify_by_pspec (G_OBJECT (search), properties[PROP_QUERY]);
	}

	file = nautilus_directory_get_existing_corresponding_file (NAUTILUS_DIRECTORY (search));
	if ((file != NULL) && (search->details->saved_search_uri == NULL)) {
		nautilus_search_directory_file_update_display_name (NAUTILUS_SEARCH_DIRECTORY_FILE (file));
	}
	nautilus_file_unref (file);
}

NautilusQuery *
nautilus_search_directory_get_query (NautilusSearchDirectory *search)
{
	if (search->details->query != NULL) {
		return g_object_ref (search->details->query);
	}
					   
	return NULL;
}

void
nautilus_search_directory_set_saved_search (NautilusSearchDirectory *search,
					    GFile *saved_search)
{
	NautilusQuery *query;
	char *file;

	search->details->saved_search_uri = g_file_get_uri (saved_search);
	file = g_file_get_path (saved_search);

	if (file != NULL) {
		query = nautilus_query_load (file);
		if (query != NULL) {
			nautilus_search_directory_set_query (search, query);
			g_object_unref (query);
		}
		g_free (file);
	} else {
		g_warning ("Non-local saved searches not supported");
	}

	search->details->modified = FALSE;
}

gboolean
nautilus_search_directory_is_saved_search (NautilusSearchDirectory *search)
{
	return search->details->saved_search_uri != NULL;
}

gboolean
nautilus_search_directory_is_modified (NautilusSearchDirectory *search)
{
	return search->details->modified;
}

void
nautilus_search_directory_save_to_file (NautilusSearchDirectory *search,
					const char              *save_file_uri)
{
	char *file;
	
	file = g_filename_from_uri (save_file_uri, NULL, NULL);
	if (file == NULL) {
		return;
	}

	if (search->details->query != NULL) {
		nautilus_query_save (search->details->query, file);
	}
	
	g_free (file);
}

void
nautilus_search_directory_save_search (NautilusSearchDirectory *search)
{
	if (search->details->saved_search_uri == NULL) {
		return;
	}

	nautilus_search_directory_save_to_file (search,
						search->details->saved_search_uri);
	search->details->modified = FALSE;
}

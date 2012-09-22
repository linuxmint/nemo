/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Red Hat, Inc
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#include <config.h>
#include "nemo-search-engine-simple.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#define BATCH_SIZE 500

typedef struct {
	NemoSearchEngineSimple *engine;
	GCancellable *cancellable;

	GList *mime_types;
	char **words;
	GList *found_list;

	GQueue *directories; /* GFiles */

	GHashTable *visited;
	
	gint n_processed_files;
	GList *uri_hits;
} SearchThreadData;


struct NemoSearchEngineSimpleDetails {
	NemoQuery *query;

	SearchThreadData *active_search;
	
	gboolean query_finished;
};

G_DEFINE_TYPE (NemoSearchEngineSimple, nemo_search_engine_simple,
	       NEMO_TYPE_SEARCH_ENGINE);

static void
finalize (GObject *object)
{
	NemoSearchEngineSimple *simple;

	simple = NEMO_SEARCH_ENGINE_SIMPLE (object);
	
	if (simple->details->query) {
		g_object_unref (simple->details->query);
		simple->details->query = NULL;
	}

	G_OBJECT_CLASS (nemo_search_engine_simple_parent_class)->finalize (object);
}

static SearchThreadData *
search_thread_data_new (NemoSearchEngineSimple *engine,
			NemoQuery *query)
{
	SearchThreadData *data;
	char *text, *lower, *normalized, *uri;
	GFile *location;
	
	data = g_new0 (SearchThreadData, 1);

	data->engine = engine;
	data->directories = g_queue_new ();
	data->visited = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	uri = nemo_query_get_location (query);
	location = NULL;
	if (uri != NULL) {
		location = g_file_new_for_uri (uri);
		g_free (uri);
	}
	if (location == NULL) {
		location = g_file_new_for_path ("/");
	}
	g_queue_push_tail (data->directories, location);
	
	text = nemo_query_get_text (query);
	normalized = g_utf8_normalize (text, -1, G_NORMALIZE_NFD);
	lower = g_utf8_strdown (normalized, -1);
	data->words = g_strsplit (lower, " ", -1);
	g_free (text);
	g_free (lower);
	g_free (normalized);

	data->mime_types = nemo_query_get_mime_types (query);

	data->cancellable = g_cancellable_new ();
	
	return data;
}

static void 
search_thread_data_free (SearchThreadData *data)
{
	g_queue_foreach (data->directories,
			 (GFunc)g_object_unref, NULL);
	g_queue_free (data->directories);
	g_hash_table_destroy (data->visited);
	g_object_unref (data->cancellable);
	g_strfreev (data->words);	
	g_list_free_full (data->mime_types, g_free);
	g_list_free_full (data->uri_hits, g_free);
	g_free (data);
}

static gboolean
search_thread_done_idle (gpointer user_data)
{
	SearchThreadData *data;

	data = user_data;

	if (!g_cancellable_is_cancelled (data->cancellable)) {
		nemo_search_engine_finished (NEMO_SEARCH_ENGINE (data->engine));
		data->engine->details->active_search = NULL;
	}
	
	search_thread_data_free (data);
	
	return FALSE;
}

typedef struct {
	GList *uris;
	SearchThreadData *thread_data;
} SearchHits;


static gboolean
search_thread_add_hits_idle (gpointer user_data)
{
	SearchHits *hits;

	hits = user_data;

	if (!g_cancellable_is_cancelled (hits->thread_data->cancellable)) {
		nemo_search_engine_hits_added (NEMO_SEARCH_ENGINE (hits->thread_data->engine),
						   hits->uris);
	}

	g_list_free_full (hits->uris, g_free);
	g_free (hits);
	
	return FALSE;
}

static void
send_batch (SearchThreadData *data)
{
	SearchHits *hits;
	
	data->n_processed_files = 0;
	
	if (data->uri_hits) {
		hits = g_new (SearchHits, 1);
		hits->uris = data->uri_hits;
		hits->thread_data = data;
		g_idle_add (search_thread_add_hits_idle, hits);
	}
	data->uri_hits = NULL;
}

#define STD_ATTRIBUTES \
	G_FILE_ATTRIBUTE_STANDARD_NAME "," \
	G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME "," \
	G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," \
	G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
	G_FILE_ATTRIBUTE_ID_FILE

static void
visit_directory (GFile *dir, SearchThreadData *data)
{
	GFileEnumerator *enumerator;
	GFileInfo *info;
	GFile *child;
	const char *mime_type, *display_name;
	char *lower_name, *normalized;
	gboolean hit;
	int i;
	GList *l;
	const char *id;
	gboolean visited;

	enumerator = g_file_enumerate_children (dir,
						data->mime_types != NULL ?
						STD_ATTRIBUTES ","
						G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE
						:
						STD_ATTRIBUTES
						,
						0, data->cancellable, NULL);
	
	if (enumerator == NULL) {
		return;
	}

	while ((info = g_file_enumerator_next_file (enumerator, data->cancellable, NULL)) != NULL) {
		if (g_file_info_get_is_hidden (info)) {
			goto next;
		}
		
		display_name = g_file_info_get_display_name (info);
		if (display_name == NULL) {
			goto next;
		}
		
		normalized = g_utf8_normalize (display_name, -1, G_NORMALIZE_NFD);
		lower_name = g_utf8_strdown (normalized, -1);
		g_free (normalized);
		
		hit = TRUE;
		for (i = 0; data->words[i] != NULL; i++) {
			if (strstr (lower_name, data->words[i]) == NULL) {
				hit = FALSE;
				break;
			}
		}
		g_free (lower_name);
		
		if (hit && data->mime_types) {
			mime_type = g_file_info_get_content_type (info);
			hit = FALSE;
			
			for (l = data->mime_types; mime_type != NULL && l != NULL; l = l->next) {
				if (g_content_type_equals (mime_type, l->data)) {
					hit = TRUE;
					break;
				}
			}
		}
		
		child = g_file_get_child (dir, g_file_info_get_name (info));
		
		if (hit) {
			data->uri_hits = g_list_prepend (data->uri_hits, g_file_get_uri (child));
		}
		
		data->n_processed_files++;
		if (data->n_processed_files > BATCH_SIZE) {
			send_batch (data);
		}

		if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
			id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILE);
			visited = FALSE;
			if (id) {
				if (g_hash_table_lookup_extended (data->visited,
								  id, NULL, NULL)) {
					visited = TRUE;
				} else {
					g_hash_table_insert (data->visited, g_strdup (id), NULL);
				}
			}
			
			if (!visited) {
				g_queue_push_tail (data->directories, g_object_ref (child));
			}
		}
		
		g_object_unref (child);
	next:
		g_object_unref (info);
	}

	g_object_unref (enumerator);
}


static gpointer 
search_thread_func (gpointer user_data)
{
	SearchThreadData *data;
	GFile *dir;
	GFileInfo *info;
	const char *id;

	data = user_data;

	/* Insert id for toplevel directory into visited */
	dir = g_queue_peek_head (data->directories);
	info = g_file_query_info (dir, G_FILE_ATTRIBUTE_ID_FILE, 0, data->cancellable, NULL);
	if (info) {
		id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILE);
		if (id) {
			g_hash_table_insert (data->visited, g_strdup (id), NULL);
		}
		g_object_unref (info);
	}
	
	while (!g_cancellable_is_cancelled (data->cancellable) &&
	       (dir = g_queue_pop_head (data->directories)) != NULL) {
		visit_directory (dir, data);
		g_object_unref (dir);
	}
	send_batch (data);

	g_idle_add (search_thread_done_idle, data);
	
	return NULL;
}

static void
nemo_search_engine_simple_start (NemoSearchEngine *engine)
{
	NemoSearchEngineSimple *simple;
	SearchThreadData *data;
	GThread *thread;
	
	simple = NEMO_SEARCH_ENGINE_SIMPLE (engine);

	if (simple->details->active_search != NULL) {
		return;
	}

	if (simple->details->query == NULL) {
		return;
	}
	
	data = search_thread_data_new (simple, simple->details->query);

	thread = g_thread_new ("nemo-search-simple", search_thread_func, data);
	simple->details->active_search = data;

	g_thread_unref (thread);
}

static void
nemo_search_engine_simple_stop (NemoSearchEngine *engine)
{
	NemoSearchEngineSimple *simple;

	simple = NEMO_SEARCH_ENGINE_SIMPLE (engine);

	if (simple->details->active_search != NULL) {
		g_cancellable_cancel (simple->details->active_search->cancellable);
		simple->details->active_search = NULL;
	}
}

static void
nemo_search_engine_simple_set_query (NemoSearchEngine *engine, NemoQuery *query)
{
	NemoSearchEngineSimple *simple;

	simple = NEMO_SEARCH_ENGINE_SIMPLE (engine);

	if (query) {
		g_object_ref (query);
	}

	if (simple->details->query) {
		g_object_unref (simple->details->query);
	}

	simple->details->query = query;
}

static void
nemo_search_engine_simple_class_init (NemoSearchEngineSimpleClass *class)
{
	GObjectClass *gobject_class;
	NemoSearchEngineClass *engine_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	engine_class = NEMO_SEARCH_ENGINE_CLASS (class);
	engine_class->set_query = nemo_search_engine_simple_set_query;
	engine_class->start = nemo_search_engine_simple_start;
	engine_class->stop = nemo_search_engine_simple_stop;

	g_type_class_add_private (class, sizeof (NemoSearchEngineSimpleDetails));
}

static void
nemo_search_engine_simple_init (NemoSearchEngineSimple *engine)
{
	engine->details = G_TYPE_INSTANCE_GET_PRIVATE (engine, NEMO_TYPE_SEARCH_ENGINE_SIMPLE,
						       NemoSearchEngineSimpleDetails);
}

NemoSearchEngine *
nemo_search_engine_simple_new (void)
{
	NemoSearchEngine *engine;

	engine = g_object_new (NEMO_TYPE_SEARCH_ENGINE_SIMPLE, NULL);

	return engine;
}

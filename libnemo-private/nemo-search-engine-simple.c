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
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#include <config.h>
#include "nemo-search-hit.h"
#include "nemo-search-provider.h"
#include "nemo-search-engine-simple.h"
#include "nemo-ui-utilities.h"
#define DEBUG_FLAG NEMO_DEBUG_SEARCH
#include "nemo-debug.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#define BATCH_SIZE 500

enum {
	PROP_RECURSIVE = 1,
	NUM_PROPERTIES
};

typedef struct {
	NemoSearchEngineSimple *engine;
	GCancellable *cancellable;

	GList *mime_types;
	GList *found_list;

	GQueue *directories; /* GFiles */

	GHashTable *visited;

	gboolean recursive;
	gint n_processed_files;
	GList *hits;

	NemoQuery *query;
} SearchThreadData;


struct NemoSearchEngineSimpleDetails {
	NemoQuery *query;

	SearchThreadData *active_search;

	gboolean recursive;
	gboolean query_finished;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static void nemo_search_provider_init (NemoSearchProviderIface  *iface);

G_DEFINE_TYPE_WITH_CODE (NemoSearchEngineSimple,
			 nemo_search_engine_simple,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (NEMO_TYPE_SEARCH_PROVIDER,
						nemo_search_provider_init))

static void
finalize (GObject *object)
{
	NemoSearchEngineSimple *simple;

	simple = NEMO_SEARCH_ENGINE_SIMPLE (object);
	g_clear_object (&simple->details->query);

	G_OBJECT_CLASS (nemo_search_engine_simple_parent_class)->finalize (object);
}

static SearchThreadData *
search_thread_data_new (NemoSearchEngineSimple *engine,
			NemoQuery *query)
{
	SearchThreadData *data;
	char *uri;
	GFile *location;
	
	data = g_new0 (SearchThreadData, 1);

	data->engine = g_object_ref (engine);
	data->directories = g_queue_new ();
	data->visited = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	data->query = g_object_ref (query);

	uri = nemo_query_get_location (query);
	location = g_file_new_for_uri (uri);
	g_free (uri);

	g_queue_push_tail (data->directories, location);
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
	g_object_unref (data->query);
	g_list_free_full (data->mime_types, g_free);
	g_list_free_full (data->hits, g_object_unref);
	g_object_unref (data->engine);

	g_free (data);
}

static gboolean
search_thread_done_idle (gpointer user_data)
{
	SearchThreadData *data = user_data;
	NemoSearchEngineSimple *engine = data->engine;

	DEBUG ("Simple engine done");

	engine->details->active_search = NULL;
	nemo_search_provider_finished (NEMO_SEARCH_PROVIDER (engine));

	search_thread_data_free (data);

	return FALSE;
}

typedef struct {
	GList *hits;
	SearchThreadData *thread_data;
} SearchHitsData;


static gboolean
search_thread_add_hits_idle (gpointer user_data)
{
	SearchHitsData *data = user_data;

	DEBUG ("Simple engine add hits");

	if (!g_cancellable_is_cancelled (data->thread_data->cancellable)) {
		nemo_search_provider_hits_added (NEMO_SEARCH_PROVIDER (data->thread_data->engine),
						     data->hits);
	}

	g_list_free_full (data->hits, g_object_unref);
	g_free (data);
	
	return FALSE;
}

static void
send_batch (SearchThreadData *thread_data)
{
	SearchHitsData *data;
	
	thread_data->n_processed_files = 0;
	
	if (thread_data->hits) {
		data = g_new (SearchHitsData, 1);
		data->hits = thread_data->hits;
		data->thread_data = thread_data;
		g_idle_add (search_thread_add_hits_idle, data);
	}
	thread_data->hits = NULL;
}

#define STD_ATTRIBUTES \
	G_FILE_ATTRIBUTE_STANDARD_NAME "," \
	G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME "," \
	G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP "," \
	G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," \
	G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
	G_FILE_ATTRIBUTE_TIME_MODIFIED "," \
	G_FILE_ATTRIBUTE_ID_FILE

static void
visit_directory (GFile *dir, SearchThreadData *data)
{
	GFileEnumerator *enumerator;
	GFileInfo *info;
	GFile *child;
	const char *mime_type, *display_name;
	gdouble match;
	gboolean is_hidden, found;
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
						0 /* G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS */,
						data->cancellable, NULL);
	
	if (enumerator == NULL) {
		return;
	}

	while ((info = g_file_enumerator_next_file (enumerator, data->cancellable, NULL)) != NULL) {
		display_name = g_file_info_get_display_name (info);
		if (display_name == NULL) {
			goto next;
		}

		is_hidden = g_file_info_get_is_hidden (info) || g_file_info_get_is_backup (info);
		if (is_hidden && !nemo_query_get_show_hidden_files (data->query)) {
			goto next;
		}

		child = g_file_get_child (dir, g_file_info_get_name (info));
		match = nemo_query_matches_string (data->query, display_name);
		found = (match > -1);

		if (found && data->mime_types) {
			mime_type = g_file_info_get_content_type (info);
			found = FALSE;
			
			for (l = data->mime_types; mime_type != NULL && l != NULL; l = l->next) {
				if (g_content_type_is_a (mime_type, l->data)) {
					found = TRUE;
					break;
				}
			}
		}
		
		if (found) {
			NemoSearchHit *hit;
			GTimeVal tv;
			GDateTime *dt;
			char *uri;

			uri = g_file_get_uri (child);
			hit = nemo_search_hit_new (uri);
			g_free (uri);
			nemo_search_hit_set_fts_rank (hit, match);
			g_file_info_get_modification_time (info, &tv);
			dt = g_date_time_new_from_timeval_local (&tv);
			nemo_search_hit_set_modification_time (hit, dt);
			g_date_time_unref (dt);

			data->hits = g_list_prepend (data->hits, hit);
		}
		
		data->n_processed_files++;
		if (data->n_processed_files > BATCH_SIZE) {
			send_batch (data);
		}

		if (data->engine->details->recursive && g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
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

	if (!g_cancellable_is_cancelled (data->cancellable)) {
		send_batch (data);
	}

	g_idle_add (search_thread_done_idle, data);
	
	return NULL;
}

static void
nemo_search_engine_simple_start (NemoSearchProvider *provider)
{
	NemoSearchEngineSimple *simple;
	SearchThreadData *data;
	GThread *thread;
	
	simple = NEMO_SEARCH_ENGINE_SIMPLE (provider);

	if (simple->details->active_search != NULL) {
		return;
	}

	DEBUG ("Simple engine start");
	
	data = search_thread_data_new (simple, simple->details->query);

	thread = g_thread_new ("nemo-search-simple", search_thread_func, data);
	simple->details->active_search = data;

	g_thread_unref (thread);
}

static void
nemo_search_engine_simple_stop (NemoSearchProvider *provider)
{
	NemoSearchEngineSimple *simple;

	simple = NEMO_SEARCH_ENGINE_SIMPLE (provider);

	if (simple->details->active_search != NULL) {
		DEBUG ("Simple engine stop");
		g_cancellable_cancel (simple->details->active_search->cancellable);
		simple->details->active_search = NULL;
	}
}

static void
nemo_search_engine_simple_set_query (NemoSearchProvider *provider,
					 NemoQuery          *query)
{
	NemoSearchEngineSimple *simple;

	simple = NEMO_SEARCH_ENGINE_SIMPLE (provider);

	g_object_ref (query);
	g_clear_object (&simple->details->query);
	simple->details->query = query;
}

static void
nemo_search_engine_simple_set_property (GObject *object,
					    guint arg_id,
					    const GValue *value,
					    GParamSpec *pspec)
{
	NemoSearchEngineSimple *engine;

	engine = NEMO_SEARCH_ENGINE_SIMPLE (object);

	switch (arg_id) {
	case PROP_RECURSIVE:
		engine->details->recursive = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
		break;
	}
}

static void
nemo_search_engine_simple_get_property (GObject *object,
					    guint arg_id,
					    GValue *value,
					    GParamSpec *pspec)
{
	NemoSearchEngineSimple *engine;

	engine = NEMO_SEARCH_ENGINE_SIMPLE (object);

	switch (arg_id) {
	case PROP_RECURSIVE:
		g_value_set_boolean (value, engine->details->recursive);
		break;
	}
}

static void
nemo_search_provider_init (NemoSearchProviderIface *iface)
{
	iface->set_query = nemo_search_engine_simple_set_query;
	iface->start = nemo_search_engine_simple_start;
	iface->stop = nemo_search_engine_simple_stop;
}

static void
nemo_search_engine_simple_class_init (NemoSearchEngineSimpleClass *class)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;
	gobject_class->get_property = nemo_search_engine_simple_get_property;
	gobject_class->set_property = nemo_search_engine_simple_set_property;

	properties[PROP_RECURSIVE] = g_param_spec_boolean ("recursive",
							   "recursive",
							   "recursive",
							   TRUE,
							   G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
	g_type_class_add_private (class, sizeof (NemoSearchEngineSimpleDetails));
}

static void
nemo_search_engine_simple_init (NemoSearchEngineSimple *engine)
{
	engine->details = G_TYPE_INSTANCE_GET_PRIVATE (engine, NEMO_TYPE_SEARCH_ENGINE_SIMPLE,
						       NemoSearchEngineSimpleDetails);
}

NemoSearchEngineSimple *
nemo_search_engine_simple_new (void)
{
	NemoSearchEngineSimple *engine;

	engine = g_object_new (NEMO_TYPE_SEARCH_ENGINE_SIMPLE, NULL);

	return engine;
}

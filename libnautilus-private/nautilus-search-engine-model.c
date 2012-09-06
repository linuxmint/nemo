/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Red Hat, Inc
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#include <config.h>
#include "nautilus-search-hit.h"
#include "nautilus-search-provider.h"
#include "nautilus-search-engine-model.h"
#include "nautilus-directory.h"
#include "nautilus-directory-private.h"
#include "nautilus-file.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#define BATCH_SIZE 500

typedef struct {
	NautilusSearchEngineModel *engine;
	GCancellable *cancellable;

	GList *mime_types;
	char **words;
	GList *found_list;

	GQueue *directories; /* GFiles */

	GHashTable *visited;

	gint n_processed_files;
	GList *hits;
} SearchData;


struct NautilusSearchEngineModelDetails {
	NautilusQuery *query;

	SearchData *active_search;

	gboolean query_finished;
};

static void nautilus_search_provider_init (NautilusSearchProviderIface  *iface);

G_DEFINE_TYPE_WITH_CODE (NautilusSearchEngineModel,
			 nautilus_search_engine_model,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SEARCH_PROVIDER,
						nautilus_search_provider_init))

static void
finalize (GObject *object)
{
	NautilusSearchEngineModel *model;

	model = NAUTILUS_SEARCH_ENGINE_MODEL (object);

	if (model->details->query) {
		g_object_unref (model->details->query);
		model->details->query = NULL;
	}

	G_OBJECT_CLASS (nautilus_search_engine_model_parent_class)->finalize (object);
}

static SearchData *
search_data_new (NautilusSearchEngineModel *engine,
		 NautilusQuery *query)
{
	SearchData *data;
	char *text, *lower, *normalized, *uri;
	GFile *location;

	data = g_new0 (SearchData, 1);

	data->engine = g_object_ref (engine);
	data->directories = g_queue_new ();
	data->visited = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	uri = nautilus_query_get_location (query);
	location = NULL;
	if (uri != NULL) {
		location = g_file_new_for_uri (uri);
		g_free (uri);
	}
	if (location == NULL) {
		location = g_file_new_for_path ("/");
	}
	g_queue_push_tail (data->directories, location);

	text = nautilus_query_get_text (query);
	g_message ("text %s", text);
	normalized = g_utf8_normalize (text, -1, G_NORMALIZE_NFD);
	lower = g_utf8_strdown (normalized, -1);
	data->words = g_strsplit (lower, " ", -1);
	g_free (text);
	g_free (lower);
	g_free (normalized);

	data->mime_types = nautilus_query_get_mime_types (query);

	data->cancellable = g_cancellable_new ();

	return data;
}

static void
search_data_free (SearchData *data)
{
	g_queue_foreach (data->directories,
			 (GFunc)g_object_unref, NULL);
	g_queue_free (data->directories);
	g_hash_table_destroy (data->visited);
	g_object_unref (data->cancellable);
	g_strfreev (data->words);
	g_list_free_full (data->mime_types, g_free);
	g_list_free_full (data->hits, g_object_unref);
	g_object_unref (data->engine);

	g_free (data);
}

static gboolean
search_done_idle (gpointer user_data)
{
	SearchData *data = user_data;

	if (!g_cancellable_is_cancelled (data->cancellable)) {
		nautilus_search_provider_finished (NAUTILUS_SEARCH_PROVIDER (data->engine));
		data->engine->details->active_search = NULL;
	}

	search_data_free (data);

	return FALSE;
}

typedef struct {
	GList *hits;
	SearchData *thread_data;
} SearchHitsData;

static gboolean
search_add_hits_idle (gpointer user_data)
{
	SearchHitsData *data = user_data;

	if (!g_cancellable_is_cancelled (data->thread_data->cancellable)) {
		nautilus_search_provider_hits_added (NAUTILUS_SEARCH_PROVIDER (data->thread_data->engine),
						     data->hits);
	}

	g_list_free_full (data->hits, g_object_unref);
	g_free (data);

	return FALSE;
}

static void
send_batch (SearchData *thread_data)
{
	SearchHitsData *data;

	thread_data->n_processed_files = 0;

	if (thread_data->hits) {
		data = g_new (SearchHitsData, 1);
		data->hits = thread_data->hits;
		data->thread_data = thread_data;
		g_idle_add (search_add_hits_idle, data);
	}
	thread_data->hits = NULL;
}

static void
got_files_callback (NautilusDirectory *directory,
		    GList             *files,
		    SearchData        *data)
{
	GList *node;

	for (node = files; node != NULL; node = node->next) {
		NautilusFile *file = node->data;
		char *display_name;
		char *normalized;
		char *lower_name;
		gboolean found;
		int i;

		display_name = nautilus_file_get_display_name (file);
		g_message ("got %s", display_name);
		if (display_name == NULL) {
			continue;
		}

		normalized = g_utf8_normalize (display_name, -1, G_NORMALIZE_NFD);
		lower_name = g_utf8_strdown (normalized, -1);
		g_free (normalized);

		found = TRUE;
		for (i = 0; data->words[i] != NULL; i++) {
			if (strstr (lower_name, data->words[i]) == NULL) {
				found = FALSE;
				break;
			}
		}
		g_free (lower_name);

		if (found && data->mime_types) {
			GList *l;

			found = FALSE;

			for (l = data->mime_types; l != NULL; l = l->next) {
				if (nautilus_file_is_mime_type (file, l->data)) {
					found = TRUE;
					break;
				}
			}
		}

		if (found) {
			NautilusSearchHit *hit;
			GDateTime *dt;
			char *uri;
			time_t t;

			uri = nautilus_file_get_uri (file);
			hit = nautilus_search_hit_new (uri);
			g_message ("FOUND %s", uri);
			g_free (uri);
			nautilus_search_hit_set_fts_rank (hit, 10.0);
			t = nautilus_file_get_mtime (file);
			dt = g_date_time_new_from_unix_local (t);
			nautilus_search_hit_set_modification_time (hit, dt);
			g_date_time_unref (dt);

			data->hits = g_list_prepend (data->hits, hit);
		}

		data->n_processed_files++;
		if (data->n_processed_files > BATCH_SIZE) {
			send_batch (data);
		}
	}
	send_batch (data);
	g_idle_add (search_done_idle, data);
}

static void
visit_directory (GFile *dir, SearchData *data)
{
	NautilusDirectory *directory;

	g_message ("searching %s", g_file_get_uri (dir));
	directory = nautilus_directory_get (dir);
	nautilus_directory_call_when_ready (directory,
					    NAUTILUS_FILE_ATTRIBUTE_INFO,
					    TRUE,
					    (NautilusDirectoryCallback)got_files_callback,
					    data);
	g_object_unref (directory);
}

static gpointer
search_thread_func (gpointer user_data)
{
	SearchData *data = user_data;
	GFile *dir;
	GFileInfo *info;
	const char *id;

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

	return NULL;
}

static void
nautilus_search_engine_model_start (NautilusSearchProvider *provider)
{
	NautilusSearchEngineModel *model;
	SearchData *data;
	GThread *thread;

	model = NAUTILUS_SEARCH_ENGINE_MODEL (provider);

	if (model->details->active_search != NULL) {
		return;
	}

	if (model->details->query == NULL) {
		return;
	}

	data = search_data_new (model, model->details->query);

	thread = g_thread_new ("nautilus-search-model", search_thread_func, data);
	model->details->active_search = data;

	g_thread_unref (thread);
}

static void
nautilus_search_engine_model_stop (NautilusSearchProvider *provider)
{
	NautilusSearchEngineModel *model;

	model = NAUTILUS_SEARCH_ENGINE_MODEL (provider);

	if (model->details->active_search != NULL) {
		g_cancellable_cancel (model->details->active_search->cancellable);
		model->details->active_search = NULL;
	}
}

static void
nautilus_search_engine_model_set_query (NautilusSearchProvider *provider,
					 NautilusQuery          *query)
{
	NautilusSearchEngineModel *model;

	model = NAUTILUS_SEARCH_ENGINE_MODEL (provider);

	if (query) {
		g_object_ref (query);
	}

	if (model->details->query) {
		g_object_unref (model->details->query);
	}

	model->details->query = query;
}

static void
nautilus_search_provider_init (NautilusSearchProviderIface *iface)
{
	iface->set_query = nautilus_search_engine_model_set_query;
	iface->start = nautilus_search_engine_model_start;
	iface->stop = nautilus_search_engine_model_stop;
}

static void
nautilus_search_engine_model_class_init (NautilusSearchEngineModelClass *class)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	g_type_class_add_private (class, sizeof (NautilusSearchEngineModelDetails));
}

static void
nautilus_search_engine_model_init (NautilusSearchEngineModel *engine)
{
	engine->details = G_TYPE_INSTANCE_GET_PRIVATE (engine, NAUTILUS_TYPE_SEARCH_ENGINE_MODEL,
						       NautilusSearchEngineModelDetails);
}

NautilusSearchEngineModel *
nautilus_search_engine_model_new (void)
{
	NautilusSearchEngineModel *engine;

	engine = g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE_MODEL, NULL);

	return engine;
}

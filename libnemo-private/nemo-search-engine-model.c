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
#include "nemo-search-engine-model.h"
#include "nemo-directory.h"
#include "nemo-directory-private.h"
#include "nemo-file.h"
#include "nemo-ui-utilities.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

struct NemoSearchEngineModelDetails {
	NemoQuery *query;

	GList *hits;
	NemoDirectory *directory;

	gboolean query_pending;
	guint finished_id;
};

static void nemo_search_provider_init (NemoSearchProviderIface  *iface);

G_DEFINE_TYPE_WITH_CODE (NemoSearchEngineModel,
			 nemo_search_engine_model,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (NEMO_TYPE_SEARCH_PROVIDER,
						nemo_search_provider_init))

static void
finalize (GObject *object)
{
	NemoSearchEngineModel *model;

	model = NEMO_SEARCH_ENGINE_MODEL (object);

	if (model->details->hits != NULL) {
		g_list_free_full (model->details->hits, g_object_unref);
		model->details->hits = NULL;
	}

	if (model->details->finished_id != 0) {
		g_source_remove (model->details->finished_id);
		model->details->finished_id = 0;
	}

	g_clear_object (&model->details->directory);
	g_clear_object (&model->details->query);

	G_OBJECT_CLASS (nemo_search_engine_model_parent_class)->finalize (object);
}

static gboolean
search_finished (NemoSearchEngineModel *model)
{
	if (model->details->hits != NULL) {
		nemo_search_provider_hits_added (NEMO_SEARCH_PROVIDER (model),
						     model->details->hits);
		g_list_free_full (model->details->hits, g_object_unref);
		model->details->hits = NULL;
	}

	model->details->query_pending = FALSE;
	nemo_search_provider_finished (NEMO_SEARCH_PROVIDER (model));
	g_object_unref (model);

	return FALSE;
}

static void
search_finished_idle (NemoSearchEngineModel *model)
{
	if (model->details->finished_id != 0) {
		return;
	}

	model->details->finished_id = g_idle_add ((GSourceFunc) search_finished, model);
}

static void
model_directory_ready_cb (NemoDirectory	*directory,
			  GList			*list,
			  gpointer		 user_data)
{
	NemoSearchEngineModel *model = user_data;
	gchar *uri, *display_name;
	GList *files, *hits, *mime_types, *l, *m;
	NemoFile *file;
	gdouble match;
	gboolean found;
	NemoSearchHit *hit;

	files = nemo_directory_get_file_list (directory);
	mime_types = nemo_query_get_mime_types (model->details->query);
	hits = NULL;

	for (l = files; l != NULL; l = l->next) {
		file = l->data;

		display_name = nemo_file_get_display_name (file);
		match = nemo_query_matches_string (model->details->query, display_name);
		found = (match > -1);

		if (found && mime_types) {
			found = FALSE;

			for (m = mime_types; m != NULL; m = m->next) {
				if (nemo_file_is_mime_type (file, m->data)) {
					found = TRUE;
					break;
				}
			}
		}

		if (found) {
			uri = nemo_file_get_uri (file);
			hit = nemo_search_hit_new (uri);
			nemo_search_hit_set_fts_rank (hit, match);
			hits = g_list_prepend (hits, hit);
			g_free (uri);
		}

		g_free (display_name);
	}

	g_list_free_full (mime_types, g_free);
	nemo_file_list_free (files);
	model->details->hits = hits;

	search_finished (model);
}

static void
nemo_search_engine_model_start (NemoSearchProvider *provider)
{
	NemoSearchEngineModel *model;

	model = NEMO_SEARCH_ENGINE_MODEL (provider);

	if (model->details->query_pending) {
		return;
	}

	g_object_ref (model);
	model->details->query_pending = TRUE;

	if (model->details->directory == NULL) {
		search_finished_idle (model);
		return;
	}

	nemo_directory_call_when_ready (model->details->directory,
					    NEMO_FILE_ATTRIBUTE_INFO,
					    TRUE, model_directory_ready_cb, model);
}

static void
nemo_search_engine_model_stop (NemoSearchProvider *provider)
{
	NemoSearchEngineModel *model;

	model = NEMO_SEARCH_ENGINE_MODEL (provider);

	if (model->details->query_pending) {
		nemo_directory_cancel_callback (model->details->directory,
						    model_directory_ready_cb, model);
		search_finished_idle (model);
	}

	g_clear_object (&model->details->directory);
}

static void
nemo_search_engine_model_set_query (NemoSearchProvider *provider,
					 NemoQuery          *query)
{
	NemoSearchEngineModel *model;

	model = NEMO_SEARCH_ENGINE_MODEL (provider);

	g_object_ref (query);
	g_clear_object (&model->details->query);
	model->details->query = query;
}

static void
nemo_search_provider_init (NemoSearchProviderIface *iface)
{
	iface->set_query = nemo_search_engine_model_set_query;
	iface->start = nemo_search_engine_model_start;
	iface->stop = nemo_search_engine_model_stop;
}

static void
nemo_search_engine_model_class_init (NemoSearchEngineModelClass *class)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	g_type_class_add_private (class, sizeof (NemoSearchEngineModelDetails));
}

static void
nemo_search_engine_model_init (NemoSearchEngineModel *engine)
{
	engine->details = G_TYPE_INSTANCE_GET_PRIVATE (engine, NEMO_TYPE_SEARCH_ENGINE_MODEL,
						       NemoSearchEngineModelDetails);
}

NemoSearchEngineModel *
nemo_search_engine_model_new (void)
{
	NemoSearchEngineModel *engine;

	engine = g_object_new (NEMO_TYPE_SEARCH_ENGINE_MODEL, NULL);

	return engine;
}

void
nemo_search_engine_model_set_model (NemoSearchEngineModel *model,
					NemoDirectory         *directory)
{
	g_clear_object (&model->details->directory);
	model->details->directory = nemo_directory_ref (directory);
}

NemoDirectory *
nemo_search_engine_model_get_model (NemoSearchEngineModel *model)
{
	return model->details->directory;
}

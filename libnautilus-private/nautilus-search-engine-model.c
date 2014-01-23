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
 * see <http://www.gnu.org/licenses/>.
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
#include "nautilus-ui-utilities.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

struct NautilusSearchEngineModelDetails {
	NautilusQuery *query;

	GList *hits;
	NautilusDirectory *directory;

	gboolean query_pending;
	guint finished_id;
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

	G_OBJECT_CLASS (nautilus_search_engine_model_parent_class)->finalize (object);
}

static gboolean
search_finished (NautilusSearchEngineModel *model)
{
	if (model->details->hits != NULL) {
		nautilus_search_provider_hits_added (NAUTILUS_SEARCH_PROVIDER (model),
						     model->details->hits);
		g_list_free_full (model->details->hits, g_object_unref);
		model->details->hits = NULL;
	}

	model->details->query_pending = FALSE;
	nautilus_search_provider_finished (NAUTILUS_SEARCH_PROVIDER (model));
	g_object_unref (model);

	return FALSE;
}

static void
search_finished_idle (NautilusSearchEngineModel *model)
{
	if (model->details->finished_id != 0) {
		return;
	}

	model->details->finished_id = g_idle_add ((GSourceFunc) search_finished, model);
}

static void
model_directory_ready_cb (NautilusDirectory	*directory,
			  GList			*list,
			  gpointer		 user_data)
{
	NautilusSearchEngineModel *model = user_data;
	gchar *uri, *display_name;
	GList *files, *hits, *mime_types, *l, *m;
	NautilusFile *file;
	gdouble match;
	gboolean found;
	NautilusSearchHit *hit;

	files = nautilus_directory_get_file_list (directory);
	mime_types = nautilus_query_get_mime_types (model->details->query);
	hits = NULL;

	for (l = files; l != NULL; l = l->next) {
		file = l->data;

		display_name = nautilus_file_get_display_name (file);
		match = nautilus_query_matches_string (model->details->query, display_name);
		found = (match > -1);

		if (found && mime_types) {
			found = FALSE;

			for (m = mime_types; m != NULL; m = m->next) {
				if (nautilus_file_is_mime_type (file, m->data)) {
					found = TRUE;
					break;
				}
			}
		}

		if (found) {
			uri = nautilus_file_get_uri (file);
			hit = nautilus_search_hit_new (uri);
			nautilus_search_hit_set_fts_rank (hit, match);
			hits = g_list_prepend (hits, hit);
			g_free (uri);
		}

		g_free (display_name);
	}

	g_list_free_full (mime_types, g_free);
	nautilus_file_list_free (files);
	model->details->hits = hits;

	search_finished (model);
}

static void
nautilus_search_engine_model_start (NautilusSearchProvider *provider)
{
	NautilusSearchEngineModel *model;

	model = NAUTILUS_SEARCH_ENGINE_MODEL (provider);

	if (model->details->query_pending) {
		return;
	}

	g_object_ref (model);
	model->details->query_pending = TRUE;

	if (model->details->directory == NULL) {
		search_finished_idle (model);
		return;
	}

	nautilus_directory_call_when_ready (model->details->directory,
					    NAUTILUS_FILE_ATTRIBUTE_INFO,
					    TRUE, model_directory_ready_cb, model);
}

static void
nautilus_search_engine_model_stop (NautilusSearchProvider *provider)
{
	NautilusSearchEngineModel *model;

	model = NAUTILUS_SEARCH_ENGINE_MODEL (provider);

	if (model->details->query_pending) {
		nautilus_directory_cancel_callback (model->details->directory,
						    model_directory_ready_cb, model);
		search_finished_idle (model);
	}

	g_clear_object (&model->details->directory);
}

static void
nautilus_search_engine_model_set_query (NautilusSearchProvider *provider,
					 NautilusQuery          *query)
{
	NautilusSearchEngineModel *model;

	model = NAUTILUS_SEARCH_ENGINE_MODEL (provider);

	g_object_ref (query);
	g_clear_object (&model->details->query);
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

void
nautilus_search_engine_model_set_model (NautilusSearchEngineModel *model,
					NautilusDirectory         *directory)
{
	g_clear_object (&model->details->directory);
	model->details->directory = nautilus_directory_ref (directory);
}

NautilusDirectory *
nautilus_search_engine_model_get_model (NautilusSearchEngineModel *model)
{
	return model->details->directory;
}

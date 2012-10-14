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
#include "nautilus-ui-utilities.h"

#include <string.h>
#include <glib.h>
#include <gio/gio.h>

struct NautilusSearchEngineModelDetails {
	NautilusQuery *query;

	GList *hits;
	NautilusDirectory *directory;

	gboolean query_pending;
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

	g_clear_object (&model->details->directory);
	g_clear_object (&model->details->query);

	G_OBJECT_CLASS (nautilus_search_engine_model_parent_class)->finalize (object);
}

static gboolean
emit_finished_idle_cb (gpointer user_data)
{
	NautilusSearchEngineModel *model = user_data;

	if (model->details->hits != NULL) {
		nautilus_search_provider_hits_added (NAUTILUS_SEARCH_PROVIDER (model),
						     model->details->hits);
		g_list_free_full (model->details->hits, g_object_unref);
		model->details->hits = NULL;
	}

	nautilus_search_provider_finished (NAUTILUS_SEARCH_PROVIDER (model));
	model->details->query_pending = FALSE;
	g_object_unref (model);

	return FALSE;
}

static void
model_directory_ready_cb (NautilusDirectory	*directory,
			  GList			*list,
			  gpointer		 user_data)
{
	NautilusSearchEngineModel *model = user_data;
	gchar *uri, *display_name;
	GList *files, *l, *hits;
	NautilusFile *file;
	gdouble match;
	NautilusSearchHit *hit;

	files = nautilus_directory_get_file_list (directory);
	hits = NULL;

	for (l = files; l != NULL; l = l->next) {
		file = l->data;
		display_name = nautilus_file_get_display_name (file);
		match = nautilus_query_matches_string (model->details->query, display_name);

		if (match > -1) {
			uri = nautilus_file_get_uri (file);
			hit = nautilus_search_hit_new (uri);
			nautilus_search_hit_set_fts_rank (hit, match);
			hits = g_list_prepend (hits, hit);
			g_free (uri);
		}

		g_free (display_name);
	}

	nautilus_file_list_free (files);
	model->details->hits = hits;

	emit_finished_idle_cb (model);
}

static void
nautilus_search_engine_model_start (NautilusSearchProvider *provider)
{
	NautilusSearchEngineModel *model;

	model = NAUTILUS_SEARCH_ENGINE_MODEL (provider);
	g_object_ref (model);

	if (model->details->query_pending) {
		return;
	}

	if (model->details->directory == NULL) {
		g_idle_add (emit_finished_idle_cb, model);
		return;
	}

	model->details->query_pending = TRUE;
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
		model->details->query_pending = FALSE;
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

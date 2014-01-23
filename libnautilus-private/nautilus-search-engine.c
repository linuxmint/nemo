/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Novell, Inc.
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
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#include <config.h>

#include <glib/gi18n.h>
#include "nautilus-search-provider.h"
#include "nautilus-search-engine.h"
#include "nautilus-search-engine-simple.h"
#include "nautilus-search-engine-model.h"
#define DEBUG_FLAG NAUTILUS_DEBUG_SEARCH
#include "nautilus-debug.h"

#ifdef ENABLE_TRACKER
#include "nautilus-search-engine-tracker.h"
#endif

struct NautilusSearchEngineDetails
{
#ifdef ENABLE_TRACKER
	NautilusSearchEngineTracker *tracker;
#endif
	NautilusSearchEngineSimple *simple;
	NautilusSearchEngineModel *model;

	GHashTable *uris;
	guint providers_running;
	guint providers_finished;
	guint providers_error;

	gboolean running;
	gboolean restart;
};

static void nautilus_search_provider_init (NautilusSearchProviderIface  *iface);

G_DEFINE_TYPE_WITH_CODE (NautilusSearchEngine,
			 nautilus_search_engine,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_SEARCH_PROVIDER,
						nautilus_search_provider_init))

static void
nautilus_search_engine_set_query (NautilusSearchProvider *provider,
				  NautilusQuery          *query)
{
	NautilusSearchEngine *engine = NAUTILUS_SEARCH_ENGINE (provider);
#ifdef ENABLE_TRACKER
	nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (engine->details->tracker), query);
#endif
	nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (engine->details->model), query);
	nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (engine->details->simple), query);
}

static void
search_engine_start_real (NautilusSearchEngine *engine)
{
	engine->details->providers_running = 0;
	engine->details->providers_finished = 0;
	engine->details->providers_error = 0;

	engine->details->restart = FALSE;

	DEBUG ("Search engine start real");

	g_object_ref (engine);

#ifdef ENABLE_TRACKER
	nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (engine->details->tracker));
	engine->details->providers_running++;
#endif
	if (nautilus_search_engine_model_get_model (engine->details->model)) {
		nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (engine->details->model));
		engine->details->providers_running++;
	}

	nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (engine->details->simple));
	engine->details->providers_running++;
}

static void
nautilus_search_engine_start (NautilusSearchProvider *provider)
{
	NautilusSearchEngine *engine = NAUTILUS_SEARCH_ENGINE (provider);
	gint num_finished;

	DEBUG ("Search engine start");

	num_finished = engine->details->providers_error + engine->details->providers_finished;

	if (engine->details->running) {
		if (num_finished == engine->details->providers_running &&
		    engine->details->restart) {
			search_engine_start_real (engine);
		}

		return;
	}

	engine->details->running = TRUE;

	if (num_finished < engine->details->providers_running) {
		engine->details->restart = TRUE;
	} else {
		search_engine_start_real (engine);
	}
}

static void
nautilus_search_engine_stop (NautilusSearchProvider *provider)
{
	NautilusSearchEngine *engine = NAUTILUS_SEARCH_ENGINE (provider);

	DEBUG ("Search engine stop");

#ifdef ENABLE_TRACKER
	nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (engine->details->tracker));
#endif
	nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (engine->details->model));
	nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (engine->details->simple));

	engine->details->running = FALSE;
	engine->details->restart = FALSE;
}

static void
search_provider_hits_added (NautilusSearchProvider *provider,
			    GList                  *hits,
			    NautilusSearchEngine   *engine)
{
	GList *added = NULL;
	GList *l;

	if (!engine->details->running || engine->details->restart) {
		DEBUG ("Ignoring hits-added, since engine is %s",
		       !engine->details->running ? "not running" : "waiting to restart");
		return;
	}

	for (l = hits; l != NULL; l = l->next) {
		NautilusSearchHit *hit = l->data;
		int count;
		const char *uri;

		uri = nautilus_search_hit_get_uri (hit);
		count = GPOINTER_TO_INT (g_hash_table_lookup (engine->details->uris, uri));
		if (count == 0)
			added = g_list_prepend (added, hit);
		g_hash_table_replace (engine->details->uris, g_strdup (uri), GINT_TO_POINTER (++count));
	}
	if (added != NULL) {
		added = g_list_reverse (added);
		nautilus_search_provider_hits_added (NAUTILUS_SEARCH_PROVIDER (engine), added);
		g_list_free (added);
	}
}

static void
check_providers_status (NautilusSearchEngine *engine)
{
	gint num_finished = engine->details->providers_error + engine->details->providers_finished;

	if (num_finished < engine->details->providers_running) {
		return;
	}

	if (num_finished == engine->details->providers_error) {
		DEBUG ("Search engine error");
		nautilus_search_provider_error (NAUTILUS_SEARCH_PROVIDER (engine),
						_("Unable to complete the requested search"));
	} else {
		DEBUG ("Search engine finished");
		nautilus_search_provider_finished (NAUTILUS_SEARCH_PROVIDER (engine));
	}

	engine->details->running = FALSE;
	g_hash_table_remove_all (engine->details->uris);

	if (engine->details->restart) {
		DEBUG ("Restarting engine");
		nautilus_search_engine_start (NAUTILUS_SEARCH_PROVIDER (engine));
	}

	g_object_unref (engine);
}

static void
search_provider_error (NautilusSearchProvider *provider,
		       const char             *error_message,
		       NautilusSearchEngine   *engine)

{
	DEBUG ("Search provider error: %s", error_message);
	engine->details->providers_error++;

	check_providers_status (engine);
}

static void
search_provider_finished (NautilusSearchProvider *provider,
			  NautilusSearchEngine   *engine)

{
	DEBUG ("Search provider finished");
	engine->details->providers_finished++;

	check_providers_status (engine);
}

static void
connect_provider_signals (NautilusSearchEngine   *engine,
			  NautilusSearchProvider *provider)
{
	g_signal_connect (provider, "hits-added",
			  G_CALLBACK (search_provider_hits_added),
			  engine);
	g_signal_connect (provider, "finished",
			  G_CALLBACK (search_provider_finished),
			  engine);
	g_signal_connect (provider, "error",
			  G_CALLBACK (search_provider_error),
			  engine);
}

static void
nautilus_search_provider_init (NautilusSearchProviderIface *iface)
{
	iface->set_query = nautilus_search_engine_set_query;
	iface->start = nautilus_search_engine_start;
	iface->stop = nautilus_search_engine_stop;
}

static void
nautilus_search_engine_finalize (GObject *object)
{
	NautilusSearchEngine *engine = NAUTILUS_SEARCH_ENGINE (object);

	g_hash_table_destroy (engine->details->uris);

#ifdef ENABLE_TRACKER
	g_clear_object (&engine->details->tracker);
#endif
	g_clear_object (&engine->details->model);
	g_clear_object (&engine->details->simple);

	G_OBJECT_CLASS (nautilus_search_engine_parent_class)->finalize (object);
}

static void
nautilus_search_engine_class_init (NautilusSearchEngineClass *class)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) class;

	object_class->finalize = nautilus_search_engine_finalize;

	g_type_class_add_private (class, sizeof (NautilusSearchEngineDetails));
}

static void
nautilus_search_engine_init (NautilusSearchEngine *engine)
{
	engine->details = G_TYPE_INSTANCE_GET_PRIVATE (engine,
						       NAUTILUS_TYPE_SEARCH_ENGINE,
						       NautilusSearchEngineDetails);

	engine->details->uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

#ifdef ENABLE_TRACKER
	engine->details->tracker = nautilus_search_engine_tracker_new ();
	connect_provider_signals (engine, NAUTILUS_SEARCH_PROVIDER (engine->details->tracker));
#endif
	engine->details->model = nautilus_search_engine_model_new ();
	connect_provider_signals (engine, NAUTILUS_SEARCH_PROVIDER (engine->details->model));

	engine->details->simple = nautilus_search_engine_simple_new ();
	connect_provider_signals (engine, NAUTILUS_SEARCH_PROVIDER (engine->details->simple));
}

NautilusSearchEngine *
nautilus_search_engine_new (void)
{
	NautilusSearchEngine *engine;

	engine = g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE, NULL);

	return engine;
}

NautilusSearchEngineModel *
nautilus_search_engine_get_model_provider (NautilusSearchEngine *engine)
{
	return engine->details->model;
}

NautilusSearchEngineSimple *
nautilus_search_engine_get_simple_provider (NautilusSearchEngine *engine)
{
	return engine->details->simple;
}

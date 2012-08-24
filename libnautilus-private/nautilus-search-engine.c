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
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#include <config.h>

#include <glib/gi18n.h>
#include "nautilus-search-provider.h"
#include "nautilus-search-engine.h"
#include "nautilus-search-engine-simple.h"
#define DEBUG_FLAG NAUTILUS_DEBUG_SEARCH
#include "nautilus-debug.h"

#ifdef ENABLE_TRACKER
#include "nautilus-search-engine-tracker.h"
#endif

struct NautilusSearchEngineDetails
{
	NautilusSearchEngineSimple *simple;
#ifdef ENABLE_TRACKER
	NautilusSearchEngineTracker *tracker;
#endif
	GHashTable *uris;
	guint num_providers;
	guint providers_finished;
	guint providers_error;
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
	nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (engine->details->simple), query);
}

static void
nautilus_search_engine_start (NautilusSearchProvider *provider)
{
	NautilusSearchEngine *engine = NAUTILUS_SEARCH_ENGINE (provider);
	engine->details->providers_finished = 0;
	engine->details->providers_error = 0;
#ifdef ENABLE_TRACKER
	nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (engine->details->tracker));
#endif
	nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (engine->details->simple));
}

static void
nautilus_search_engine_stop (NautilusSearchProvider *provider)
{
	NautilusSearchEngine *engine = NAUTILUS_SEARCH_ENGINE (provider);
#ifdef ENABLE_TRACKER
	nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (engine->details->tracker));
#endif
	nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (engine->details->simple));
}

static void
search_provider_hits_added (NautilusSearchProvider *provider,
			    GList                  *hits,
			    NautilusSearchEngine   *engine)
{
	GList *added = NULL;
	GList *l;

	for (l = hits; l != NULL; l = l->next) {
		NautilusSearchHit *hit = l->data;
		int count;
		const char *uri;

		uri = nautilus_search_hit_get_uri (hit);
		count = GPOINTER_TO_INT (g_hash_table_lookup (engine->details->uris, uri));
		if (count == 0)
			added = g_list_prepend (added, hit);
		g_hash_table_replace (engine->details->uris, g_strdup (uri), GINT_TO_POINTER (count++));
	}
	if (added != NULL) {
		added = g_list_reverse (added);
		nautilus_search_provider_hits_added (NAUTILUS_SEARCH_PROVIDER (engine), added);
		g_list_free (added);
	}
}

static void
search_provider_hits_subtracted (NautilusSearchProvider *provider,
				 GList                  *hits,
				 NautilusSearchEngine   *engine)
{
	GList *removed = NULL;
	GList *l;

	for (l = hits; l != NULL; l = l->next) {
		NautilusSearchHit *hit = l->data;
		int count;
		const char *uri;

		uri = nautilus_search_hit_get_uri (hit);
		count = GPOINTER_TO_INT (g_hash_table_lookup (engine->details->uris, uri));
		g_assert (count > 0);
		if (count == 1) {
			removed = g_list_prepend (removed, hit);
			g_hash_table_remove (engine->details->uris, uri);
		} else {
			g_hash_table_replace (engine->details->uris, g_strdup (uri), GINT_TO_POINTER (count--));
		}
	}
	if (removed != NULL) {
		nautilus_search_provider_hits_subtracted (NAUTILUS_SEARCH_PROVIDER (engine), g_list_reverse (removed));
		g_list_free (removed);
	}
}

static void
search_provider_error (NautilusSearchProvider *provider,
		       const char             *error_message,
		       NautilusSearchEngine   *engine)

{
	DEBUG ("Search provider error: %s", error_message);
	engine->details->providers_error++;
	if (engine->details->providers_error == engine->details->num_providers) {
		nautilus_search_provider_error (NAUTILUS_SEARCH_PROVIDER (engine),
						_("Unable to complete the requested search"));
	}
}

static void
search_provider_finished (NautilusSearchProvider *provider,
			  NautilusSearchEngine   *engine)

{
	engine->details->providers_finished++;
	if (engine->details->providers_finished == engine->details->num_providers)
		nautilus_search_provider_finished (NAUTILUS_SEARCH_PROVIDER (engine));
}

static void
connect_provider_signals (NautilusSearchEngine   *engine,
			  NautilusSearchProvider *provider)
{
	g_signal_connect (provider, "hits-added",
			  G_CALLBACK (search_provider_hits_added),
			  engine);
	g_signal_connect (provider, "hits-subtracted",
			  G_CALLBACK (search_provider_hits_subtracted),
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
	engine->details->num_providers++;
#endif

	engine->details->simple = nautilus_search_engine_simple_new ();
	connect_provider_signals (engine, NAUTILUS_SEARCH_PROVIDER (engine->details->simple));
	engine->details->num_providers++;
}

NautilusSearchEngine *
nautilus_search_engine_new (void)
{
	NautilusSearchEngine *engine;

	engine = g_object_new (NAUTILUS_TYPE_SEARCH_ENGINE, NULL);

	return engine;
}

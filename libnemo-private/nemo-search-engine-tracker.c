/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Mr Jamie McCracken
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
 * Author: Jamie McCracken <jamiemcc@gnome.org>
 *
 */

#include <config.h>
#include "nemo-search-hit.h"
#include "nemo-search-provider.h"
#include "nemo-search-engine-tracker.h"
#include <string.h>
#include <gio/gio.h>

#include <libtracker-sparql/tracker-sparql.h>

struct NemoSearchEngineTrackerDetails {
	TrackerSparqlConnection *connection;
	NemoQuery *query;

	gboolean       query_pending;
	GCancellable  *cancellable;
};

static void nemo_search_provider_init (NemoSearchProviderIface  *iface);

G_DEFINE_TYPE_WITH_CODE (NemoSearchEngineTracker,
			 nemo_search_engine_tracker,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (NEMO_TYPE_SEARCH_PROVIDER,
						nemo_search_provider_init))

static void
finalize (GObject *object)
{
	NemoSearchEngineTracker *tracker;

	tracker = NEMO_SEARCH_ENGINE_TRACKER (object);

	if (tracker->details->cancellable) {
		g_cancellable_cancel (tracker->details->cancellable);
		g_clear_object (&tracker->details->cancellable);
	}

	g_clear_object (&tracker->details->query);
	g_clear_object (&tracker->details->connection);

	G_OBJECT_CLASS (nemo_search_engine_tracker_parent_class)->finalize (object);
}

static void cursor_callback (GObject      *object,
			     GAsyncResult *result,
			     gpointer      user_data);

static void
cursor_next (NemoSearchEngineTracker *tracker,
             TrackerSparqlCursor    *cursor)
{
	tracker_sparql_cursor_next_async (cursor,
	                                  tracker->details->cancellable,
	                                  cursor_callback,
	                                  tracker);
}

static void
cursor_callback (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
	NemoSearchEngineTracker *tracker;
	GError *error = NULL;
	TrackerSparqlCursor *cursor;
	NemoSearchHit *hit;
	const char *uri;
	const char *mtime_str;
	const char *atime_str;
	GTimeVal tv;
	gdouble rank;
	GList *hits;
	gboolean success;

	tracker = NEMO_SEARCH_ENGINE_TRACKER (user_data);

	cursor = TRACKER_SPARQL_CURSOR (object);
	success = tracker_sparql_cursor_next_finish (cursor, result, &error);

	if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		nemo_search_provider_error (NEMO_SEARCH_PROVIDER (tracker), error->message);
	}

	g_clear_error (&error);

	if (!success) {
		tracker->details->query_pending = FALSE;
		nemo_search_provider_finished (NEMO_SEARCH_PROVIDER (tracker));
		g_object_unref (cursor);

		return;
	}

	/* We iterate result by result, not n at a time. */
	uri = tracker_sparql_cursor_get_string (cursor, 0, NULL);
	rank = tracker_sparql_cursor_get_double (cursor, 1);
	mtime_str = tracker_sparql_cursor_get_string (cursor, 2, NULL);
	atime_str = tracker_sparql_cursor_get_string (cursor, 3, NULL);

	hit = nemo_search_hit_new (uri);
	nemo_search_hit_set_fts_rank (hit, rank);
	if (g_time_val_from_iso8601 (mtime_str, &tv)) {
		GDateTime *dt;
		dt = g_date_time_new_from_timeval_local (&tv);
		nemo_search_hit_set_modification_time (hit, dt);
		g_date_time_unref (dt);
	} else {
		g_warning ("unable to parse mtime: %s", mtime_str);
	}
	if (g_time_val_from_iso8601 (atime_str, &tv)) {
		GDateTime *dt;
		dt = g_date_time_new_from_timeval_local (&tv);
		nemo_search_hit_set_access_time (hit, dt);
		g_date_time_unref (dt);
	} else {
		g_warning ("unable to parse atime: %s", atime_str);
	}

	hits = g_list_append (NULL, hit);
	nemo_search_provider_hits_added (NEMO_SEARCH_PROVIDER (tracker), hits);
	g_list_free (hits);
	g_object_unref (hit);

	/* Get next */
	cursor_next (tracker, cursor);
}

static void
query_callback (GObject      *object,
                GAsyncResult *result,
                gpointer      user_data)
{
	NemoSearchEngineTracker *tracker;
	TrackerSparqlConnection *connection;
	TrackerSparqlCursor *cursor;
	GError *error = NULL;

	tracker = NEMO_SEARCH_ENGINE_TRACKER (user_data);

	connection = TRACKER_SPARQL_CONNECTION (object);
	cursor = tracker_sparql_connection_query_finish (connection,
	                                                 result,
	                                                 &error);

	if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		nemo_search_provider_error (NEMO_SEARCH_PROVIDER (tracker), error->message);
	}

	g_clear_error (&error);

	if (!cursor) {
		tracker->details->query_pending = FALSE;
		nemo_search_provider_finished (NEMO_SEARCH_PROVIDER (tracker));
		return;
	}

	cursor_next (tracker, cursor);
}

static void
nemo_search_engine_tracker_start (NemoSearchProvider *provider)
{
	NemoSearchEngineTracker *tracker;
	gchar	*query_text, *search_text, *location_uri, *downcase;
	GString *sparql;
	GList *mimetypes, *l;
	gint mime_count;

	tracker = NEMO_SEARCH_ENGINE_TRACKER (provider);

	if (tracker->details->query_pending) {
		return;
	}

	if (tracker->details->query == NULL) {
		return;
	}

	query_text = nemo_query_get_text (tracker->details->query);
	downcase = g_utf8_strdown (query_text, -1);
	search_text = tracker_sparql_escape_string (downcase);
	g_free (query_text);
	g_free (downcase);

	location_uri = nemo_query_get_location (tracker->details->query);
	mimetypes = nemo_query_get_mime_types (tracker->details->query);

	mime_count = g_list_length (mimetypes);

	sparql = g_string_new ("SELECT DISTINCT nie:url(?urn) fts:rank(?urn) tracker:coalesce(nfo:fileLastModified(?urn), nie:contentLastModified(?urn)) AS ?mtime tracker:coalesce(nfo:fileLastAccessed(?urn), nie:contentAccessed(?urn)) AS ?atime "
			       "WHERE {"
			       "  ?urn a nfo:FileDataObject ;"
			       "  tracker:available true ; ");

	if (mime_count > 0) {
		g_string_append (sparql, "nie:mimeType ?mime ;");
	}

	g_string_append_printf (sparql, " fts:match '%s*'", search_text);
	g_string_append (sparql, " . FILTER (");
	
	if (location_uri)  {
		g_string_append_printf (sparql, " fn:starts-with(nie:url(?urn), '%s')",
					location_uri);
	}

	g_string_append_printf (sparql, " && fn:contains(fn:lower-case(nfo:fileName(?urn)), '%s')",
				search_text);

	if (mime_count > 0) {
		g_string_append (sparql, " && (");

		for (l = mimetypes; l != NULL; l = l->next) {
			if (l != mimetypes) {
				g_string_append (sparql, " || ");
			}

			g_string_append_printf (sparql, "fn:contains(?mime, '%s')",
						(gchar *) l->data);
		}
		g_string_append (sparql, ")");
	}

	g_string_append (sparql, ")} ORDER BY DESC (fts:rank(?urn))");

	tracker->details->cancellable = g_cancellable_new ();
	tracker->details->query_pending = TRUE;
	tracker_sparql_connection_query_async (tracker->details->connection,
					       sparql->str,
					       tracker->details->cancellable,
					       query_callback,
					       tracker);
	g_string_free (sparql, TRUE);

	g_free (search_text);
	g_free (location_uri);
	g_list_free_full (mimetypes, g_free);
}

static void
nemo_search_engine_tracker_stop (NemoSearchProvider *provider)
{
	NemoSearchEngineTracker *tracker;

	tracker = NEMO_SEARCH_ENGINE_TRACKER (provider);
	
	if (tracker->details->query && tracker->details->query_pending) {
		g_cancellable_cancel (tracker->details->cancellable);
		g_clear_object (&tracker->details->cancellable);
		tracker->details->query_pending = FALSE;
	}
}

static void
nemo_search_engine_tracker_set_query (NemoSearchProvider *provider,
					  NemoQuery *query)
{
	NemoSearchEngineTracker *tracker;

	tracker = NEMO_SEARCH_ENGINE_TRACKER (provider);

	if (query) {
		g_object_ref (query);
	}

	if (tracker->details->query) {
		g_object_unref (tracker->details->query);
	}

	tracker->details->query = query;
}

static void
nemo_search_provider_init (NemoSearchProviderIface *iface)
{
	iface->set_query = nemo_search_engine_tracker_set_query;
	iface->start = nemo_search_engine_tracker_start;
	iface->stop = nemo_search_engine_tracker_stop;
}

static void
nemo_search_engine_tracker_class_init (NemoSearchEngineTrackerClass *class)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	g_type_class_add_private (class, sizeof (NemoSearchEngineTrackerDetails));
}

static void
nemo_search_engine_tracker_init (NemoSearchEngineTracker *engine)
{
	engine->details = G_TYPE_INSTANCE_GET_PRIVATE (engine, NEMO_TYPE_SEARCH_ENGINE_TRACKER,
						       NemoSearchEngineTrackerDetails);
}


NemoSearchEngineTracker *
nemo_search_engine_tracker_new (void)
{
	NemoSearchEngineTracker *engine;
	TrackerSparqlConnection *connection;
	GError *error = NULL;

	connection = tracker_sparql_connection_get (NULL, &error);

	if (error) {
		g_warning ("Could not establish a connection to Tracker: %s", error->message);
		g_error_free (error);
		return NULL;
	}

	engine = g_object_new (NEMO_TYPE_SEARCH_ENGINE_TRACKER, NULL);
	engine->details->connection = connection;

	return engine;
}

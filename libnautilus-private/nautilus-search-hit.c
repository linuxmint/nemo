/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
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
 */

#include <config.h>

#include <string.h>
#include <gio/gio.h>

#include "nautilus-search-hit.h"
#include "nautilus-query.h"
#define DEBUG_FLAG NAUTILUS_DEBUG_SEARCH_HIT
#include "nautilus-debug.h"

struct NautilusSearchHitDetails
{
	char      *uri;

	GDateTime *modification_time;
	GDateTime *access_time;
	gdouble    fts_rank;

	gdouble    relevance;
};

enum {
	PROP_URI = 1,
	PROP_RELEVANCE,
	PROP_MODIFICATION_TIME,
	PROP_ACCESS_TIME,
	PROP_FTS_RANK,
	NUM_PROPERTIES
};

G_DEFINE_TYPE (NautilusSearchHit, nautilus_search_hit, G_TYPE_OBJECT)

void
nautilus_search_hit_compute_scores (NautilusSearchHit *hit,
				    NautilusQuery     *query)
{
	GDateTime *now;
	char *query_uri;
	GFile *query_location;
	GFile *hit_location;
	GTimeSpan m_diff = G_MAXINT64;
	GTimeSpan a_diff = G_MAXINT64;
	GTimeSpan t_diff = G_MAXINT64;
	gdouble recent_bonus = 0.0;
	gdouble proximity_bonus = 0.0;
	gdouble match_bonus = 0.0;

	query_uri = nautilus_query_get_location (query);
	query_location = g_file_new_for_uri (query_uri);
	hit_location = g_file_new_for_uri (hit->details->uri);

	if (g_file_has_prefix (hit_location, query_location)) {
		GFile *parent, *location;
		guint dir_count = 0;

		parent = g_file_get_parent (hit_location);

		while (!g_file_equal (parent, query_location)) {
			dir_count++;
			location = parent;
			parent = g_file_get_parent (location);
			g_object_unref (location);
		}
		g_object_unref (parent);

		if (dir_count < 10) {
			proximity_bonus = 10000.0 - 1000.0 * dir_count;
		}
	}
	g_object_unref (hit_location);

	now = g_date_time_new_now_local ();
	if (hit->details->modification_time != NULL)
		m_diff = g_date_time_difference (now, hit->details->modification_time);
	if (hit->details->access_time != NULL)
		a_diff = g_date_time_difference (now, hit->details->access_time);
	m_diff /= G_TIME_SPAN_DAY;
	a_diff /= G_TIME_SPAN_DAY;
	t_diff = MIN (m_diff, a_diff);
	if (t_diff > 90) {
		recent_bonus = 0.0;
	} else if (t_diff > 30) {
		recent_bonus = 10.0;
	} else if (t_diff > 14) {
		recent_bonus = 30.0;
	} else if (t_diff > 7) {
		recent_bonus = 50.0;
	} else if (t_diff > 1) {
		recent_bonus = 70.0;
	} else {
		recent_bonus = 100.0;
	}

	if (hit->details->fts_rank > 0) {
		match_bonus = MIN (500, 10.0 * hit->details->fts_rank);
	} else {
		match_bonus = 0.0;
	}

	hit->details->relevance = recent_bonus + proximity_bonus + match_bonus;
	DEBUG ("Hit %s computed relevance %.2f (%.2f + %.2f + %.2f)", hit->details->uri, hit->details->relevance,
	       proximity_bonus, recent_bonus, match_bonus);

	g_date_time_unref (now);
	g_free (query_uri);
	g_object_unref (query_location);
}

const char *
nautilus_search_hit_get_uri (NautilusSearchHit *hit)
{
	return hit->details->uri;
}

gdouble
nautilus_search_hit_get_relevance (NautilusSearchHit *hit)
{
	return hit->details->relevance;
}

static void
nautilus_search_hit_set_uri (NautilusSearchHit *hit,
			     const char        *uri)
{
	g_free (hit->details->uri);
	hit->details->uri = g_strdup (uri);
}

void
nautilus_search_hit_set_fts_rank (NautilusSearchHit *hit,
				  gdouble            rank)
{
	hit->details->fts_rank = rank;
}

void
nautilus_search_hit_set_modification_time (NautilusSearchHit *hit,
					   GDateTime         *date)
{
	if (hit->details->modification_time != NULL)
		g_date_time_unref (hit->details->modification_time);
	if (date != NULL)
		hit->details->modification_time = g_date_time_ref (date);
	else
		hit->details->modification_time = NULL;
}

void
nautilus_search_hit_set_access_time (NautilusSearchHit *hit,
				     GDateTime         *date)
{
	if (hit->details->access_time != NULL)
		g_date_time_unref (hit->details->access_time);
	if (date != NULL)
		hit->details->access_time = g_date_time_ref (date);
	else
		hit->details->access_time = NULL;
}

static void
nautilus_search_hit_set_property (GObject *object,
				  guint arg_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	NautilusSearchHit *hit;

	hit = NAUTILUS_SEARCH_HIT (object);

	switch (arg_id) {
	case PROP_RELEVANCE:
		hit->details->relevance = g_value_get_double (value);
		break;
	case PROP_FTS_RANK:
		nautilus_search_hit_set_fts_rank (hit, g_value_get_double (value));
		break;
	case PROP_URI:
		nautilus_search_hit_set_uri (hit, g_value_get_string (value));
		break;
	case PROP_MODIFICATION_TIME:
		nautilus_search_hit_set_modification_time (hit, g_value_get_boxed (value));
		break;
	case PROP_ACCESS_TIME:
		nautilus_search_hit_set_access_time (hit, g_value_get_boxed (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
		break;
	}
}

static void
nautilus_search_hit_get_property (GObject *object,
				  guint arg_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	NautilusSearchHit *hit;

	hit = NAUTILUS_SEARCH_HIT (object);

	switch (arg_id) {
	case PROP_RELEVANCE:
		g_value_set_double (value, hit->details->relevance);
		break;
	case PROP_FTS_RANK:
		g_value_set_double (value, hit->details->fts_rank);
		break;
	case PROP_URI:
		g_value_set_string (value, hit->details->uri);
		break;
	case PROP_MODIFICATION_TIME:
		g_value_set_boxed (value, hit->details->modification_time);
		break;
	case PROP_ACCESS_TIME:
		g_value_set_boxed (value, hit->details->access_time);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
		break;
	}
}

static void
nautilus_search_hit_finalize (GObject *object)
{
	NautilusSearchHit *hit = NAUTILUS_SEARCH_HIT (object);

	g_free (hit->details->uri);

	if (hit->details->access_time != NULL) {
		g_date_time_unref (hit->details->access_time);
	}
	if (hit->details->modification_time != NULL) {
		g_date_time_unref (hit->details->modification_time);
	}

	G_OBJECT_CLASS (nautilus_search_hit_parent_class)->finalize (object);
}

static void
nautilus_search_hit_class_init (NautilusSearchHitClass *class)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) class;

	object_class->finalize = nautilus_search_hit_finalize;
	object_class->get_property = nautilus_search_hit_get_property;
	object_class->set_property = nautilus_search_hit_set_property;

	g_object_class_install_property (object_class,
					 PROP_URI,
					 g_param_spec_string ("uri",
							      "URI",
							      "URI",
							      NULL,
							      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_READABLE));
	g_object_class_install_property (object_class,
					 PROP_MODIFICATION_TIME,
					 g_param_spec_boxed ("modification-time",
							     "Modification time",
							     "Modification time",
							     G_TYPE_DATE_TIME,
							     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_ACCESS_TIME,
					 g_param_spec_boxed ("access-time",
							     "acess time",
							     "access time",
							     G_TYPE_DATE_TIME,
							     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class,
					 PROP_RELEVANCE,
					 g_param_spec_double ("relevance",
							      NULL,
							      NULL,
							      -G_MAXDOUBLE, G_MAXDOUBLE,
							      0,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_FTS_RANK,
					 g_param_spec_double ("fts-rank",
							      NULL,
							      NULL,
							      -G_MAXDOUBLE, G_MAXDOUBLE,
							      0,
							      G_PARAM_READWRITE));

	g_type_class_add_private (class, sizeof (NautilusSearchHitDetails));
}

static void
nautilus_search_hit_init (NautilusSearchHit *hit)
{
	hit->details = G_TYPE_INSTANCE_GET_PRIVATE (hit,
						    NAUTILUS_TYPE_SEARCH_HIT,
						    NautilusSearchHitDetails);
}

NautilusSearchHit *
nautilus_search_hit_new (const char *uri)
{
	NautilusSearchHit *hit;

	hit = g_object_new (NAUTILUS_TYPE_SEARCH_HIT,
			    "uri", uri,
			    NULL);

	return hit;
}

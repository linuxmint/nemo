/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
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
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <config.h>

#include <string.h>

#include "nemo-search-hit.h"
#include "nemo-query.h"

struct NemoSearchHitDetails
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

G_DEFINE_TYPE (NemoSearchHit, nemo_search_hit, G_TYPE_OBJECT)

void
nemo_search_hit_compute_scores (NemoSearchHit *hit,
				    NemoQuery     *query)
{
	GDateTime *now;
	char *query_uri;
	char *query_path;
	int i;
	GTimeSpan m_diff = G_MAXINT64;
	GTimeSpan a_diff = G_MAXINT64;
	GTimeSpan t_diff = G_MAXINT64;
	gdouble recent_bonus = 0.0;
	gdouble proximity_bonus = 0.0;
	gdouble match_bonus = 0.0;

	query_uri = nemo_query_get_location (query);
	query_path = g_filename_from_uri (query_uri, NULL, NULL);
	g_free (query_uri);
	if (query_path != NULL) {
		char *hit_path;
		char *hit_parent;
		guint dir_count;

		hit_path = g_filename_from_uri (hit->details->uri, NULL, NULL);
		hit_parent = g_path_get_dirname (hit_path);
		g_free (hit_path);

		dir_count = 0;
		for (i = strlen (query_path); hit_parent[i] != '\0'; i++) {
			if (G_IS_DIR_SEPARATOR (hit_parent[i]))
				dir_count++;
		}
		g_free (hit_parent);

		if (dir_count < 10) {
			proximity_bonus = 100.0 - 10 * dir_count;
		} else {
			proximity_bonus = 0.0;
		}
	}
	g_free (query_path);

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
		match_bonus = 10.0 * hit->details->fts_rank;
	} else {
		match_bonus = 0.0;
	}

	hit->details->relevance = recent_bonus + proximity_bonus + match_bonus;

	g_date_time_unref (now);
}

const char *
nemo_search_hit_get_uri (NemoSearchHit *hit)
{
	return hit->details->uri;
}

gdouble
nemo_search_hit_get_relevance (NemoSearchHit *hit)
{
	return hit->details->relevance;
}

static void
nemo_search_hit_set_uri (NemoSearchHit *hit,
			     const char        *uri)
{
	g_free (hit->details->uri);
	hit->details->uri = g_strdup (uri);
}

void
nemo_search_hit_set_fts_rank (NemoSearchHit *hit,
				  gdouble            rank)
{
	hit->details->fts_rank = rank;
}

void
nemo_search_hit_set_modification_time (NemoSearchHit *hit,
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
nemo_search_hit_set_access_time (NemoSearchHit *hit,
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
nemo_search_hit_set_property (GObject *object,
				  guint arg_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
	NemoSearchHit *hit;

	hit = NEMO_SEARCH_HIT (object);

	switch (arg_id) {
	case PROP_RELEVANCE:
		hit->details->relevance = g_value_get_double (value);
	case PROP_FTS_RANK:
		nemo_search_hit_set_fts_rank (hit, g_value_get_double (value));
		break;
	case PROP_URI:
		nemo_search_hit_set_uri (hit, g_value_get_string (value));
		break;
	case PROP_MODIFICATION_TIME:
		nemo_search_hit_set_modification_time (hit, g_value_get_boxed (value));
		break;
	case PROP_ACCESS_TIME:
		nemo_search_hit_set_access_time (hit, g_value_get_boxed (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
		break;
	}
}

static void
nemo_search_hit_get_property (GObject *object,
				  guint arg_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	NemoSearchHit *hit;

	hit = NEMO_SEARCH_HIT (object);

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
nemo_search_hit_finalize (GObject *object)
{
	NemoSearchHit *hit = NEMO_SEARCH_HIT (object);

	g_free (hit->details->uri);

	if (hit->details->access_time != NULL) {
		g_date_time_unref (hit->details->access_time);
	}
	if (hit->details->modification_time != NULL) {
		g_date_time_unref (hit->details->modification_time);
	}

	G_OBJECT_CLASS (nemo_search_hit_parent_class)->finalize (object);
}

static void
nemo_search_hit_class_init (NemoSearchHitClass *class)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) class;

	object_class->finalize = nemo_search_hit_finalize;
	object_class->get_property = nemo_search_hit_get_property;
	object_class->set_property = nemo_search_hit_set_property;

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

	g_type_class_add_private (class, sizeof (NemoSearchHitDetails));
}

static void
nemo_search_hit_init (NemoSearchHit *hit)
{
	hit->details = G_TYPE_INSTANCE_GET_PRIVATE (hit,
						    NEMO_TYPE_SEARCH_HIT,
						    NemoSearchHitDetails);
}

NemoSearchHit *
nemo_search_hit_new (const char *uri)
{
	NemoSearchHit *hit;

	hit = g_object_new (NEMO_TYPE_SEARCH_HIT,
			    "uri", uri,
			    NULL);

	return hit;
}

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

#ifndef NEMO_SEARCH_HIT_H
#define NEMO_SEARCH_HIT_H

#include <glib-object.h>
#include "nemo-query.h"

#define NEMO_TYPE_SEARCH_HIT		(nemo_search_hit_get_type ())
#define NEMO_SEARCH_HIT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_SEARCH_HIT, NemoSearchHit))
#define NEMO_SEARCH_HIT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_SEARCH_HIT, NemoSearchHitClass))
#define NEMO_IS_SEARCH_HIT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_SEARCH_HIT))
#define NEMO_IS_SEARCH_HIT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_SEARCH_HIT))
#define NEMO_SEARCH_HIT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_SEARCH_HIT, NemoSearchHitClass))

typedef struct NemoSearchHitDetails NemoSearchHitDetails;

typedef struct NemoSearchHit {
	GObject parent;
	NemoSearchHitDetails *details;
} NemoSearchHit;

typedef struct {
	GObjectClass parent_class;
} NemoSearchHitClass;

GType               nemo_search_hit_get_type      (void);

NemoSearchHit * nemo_search_hit_new                   (const char        *uri);

void                nemo_search_hit_set_fts_rank          (NemoSearchHit *hit,
							       gdouble            fts_rank);
void                nemo_search_hit_set_modification_time (NemoSearchHit *hit,
							       GDateTime         *date);
void                nemo_search_hit_set_access_time       (NemoSearchHit *hit,
							       GDateTime         *date);

void                nemo_search_hit_compute_scores        (NemoSearchHit *hit,
							       NemoQuery     *query);

const char *        nemo_search_hit_get_uri               (NemoSearchHit *hit);
gdouble             nemo_search_hit_get_relevance         (NemoSearchHit *hit);

#endif /* NEMO_SEARCH_HIT_H */

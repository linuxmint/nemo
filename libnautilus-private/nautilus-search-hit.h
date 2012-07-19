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
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef NAUTILUS_SEARCH_HIT_H
#define NAUTILUS_SEARCH_HIT_H

#include <glib-object.h>
#include "nautilus-query.h"

#define NAUTILUS_TYPE_SEARCH_HIT		(nautilus_search_hit_get_type ())
#define NAUTILUS_SEARCH_HIT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_SEARCH_HIT, NautilusSearchHit))
#define NAUTILUS_SEARCH_HIT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SEARCH_HIT, NautilusSearchHitClass))
#define NAUTILUS_IS_SEARCH_HIT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_SEARCH_HIT))
#define NAUTILUS_IS_SEARCH_HIT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SEARCH_HIT))
#define NAUTILUS_SEARCH_HIT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_SEARCH_HIT, NautilusSearchHitClass))

typedef struct NautilusSearchHitDetails NautilusSearchHitDetails;

typedef struct NautilusSearchHit {
	GObject parent;
	NautilusSearchHitDetails *details;
} NautilusSearchHit;

typedef struct {
	GObjectClass parent_class;
} NautilusSearchHitClass;

GType               nautilus_search_hit_get_type      (void);

NautilusSearchHit * nautilus_search_hit_new                   (const char        *uri);

void                nautilus_search_hit_set_fts_rank          (NautilusSearchHit *hit,
							       gdouble            fts_rank);
void                nautilus_search_hit_set_modification_time (NautilusSearchHit *hit,
							       GDateTime         *date);
void                nautilus_search_hit_set_access_time       (NautilusSearchHit *hit,
							       GDateTime         *date);

void                nautilus_search_hit_compute_scores        (NautilusSearchHit *hit,
							       NautilusQuery     *query);

const char *        nautilus_search_hit_get_uri               (NautilusSearchHit *hit);
gdouble             nautilus_search_hit_get_relevance         (NautilusSearchHit *hit);

#endif /* NAUTILUS_SEARCH_HIT_H */

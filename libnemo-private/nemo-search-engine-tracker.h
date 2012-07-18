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
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Jamie McCracken (jamiemcc@gnome.org)
 *
 */

#ifndef NEMO_SEARCH_ENGINE_TRACKER_H
#define NEMO_SEARCH_ENGINE_TRACKER_H

#include <libnemo-private/nemo-search-engine.h>

#define NEMO_TYPE_SEARCH_ENGINE_TRACKER		(nemo_search_engine_tracker_get_type ())
#define NEMO_SEARCH_ENGINE_TRACKER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_SEARCH_ENGINE_TRACKER, NemoSearchEngineTracker))
#define NEMO_SEARCH_ENGINE_TRACKER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_SEARCH_ENGINE_TRACKER, NemoSearchEngineTrackerClass))
#define NEMO_IS_SEARCH_ENGINE_TRACKER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_SEARCH_ENGINE_TRACKER))
#define NEMO_IS_SEARCH_ENGINE_TRACKER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_SEARCH_ENGINE_TRACKER))
#define NEMO_SEARCH_ENGINE_TRACKER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_SEARCH_ENGINE_TRACKER, NemoSearchEngineTrackerClass))

typedef struct NemoSearchEngineTrackerDetails NemoSearchEngineTrackerDetails;

typedef struct NemoSearchEngineTracker {
	NemoSearchEngine parent;
	NemoSearchEngineTrackerDetails *details;
} NemoSearchEngineTracker;

typedef struct {
	NemoSearchEngineClass parent_class;
} NemoSearchEngineTrackerClass;

GType nemo_search_engine_tracker_get_type (void);

NemoSearchEngine* nemo_search_engine_tracker_new (void);

#endif /* NEMO_SEARCH_ENGINE_TRACKER_H */

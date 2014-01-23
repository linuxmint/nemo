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

#ifndef NAUTILUS_SEARCH_ENGINE_H
#define NAUTILUS_SEARCH_ENGINE_H

#include <glib-object.h>

#include <libnautilus-private/nautilus-directory.h>
#include <libnautilus-private/nautilus-search-engine-model.h>
#include <libnautilus-private/nautilus-search-engine-simple.h>

#define NAUTILUS_TYPE_SEARCH_ENGINE		(nautilus_search_engine_get_type ())
#define NAUTILUS_SEARCH_ENGINE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_SEARCH_ENGINE, NautilusSearchEngine))
#define NAUTILUS_SEARCH_ENGINE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SEARCH_ENGINE, NautilusSearchEngineClass))
#define NAUTILUS_IS_SEARCH_ENGINE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_SEARCH_ENGINE))
#define NAUTILUS_IS_SEARCH_ENGINE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SEARCH_ENGINE))
#define NAUTILUS_SEARCH_ENGINE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_SEARCH_ENGINE, NautilusSearchEngineClass))

typedef struct NautilusSearchEngineDetails NautilusSearchEngineDetails;

typedef struct NautilusSearchEngine {
	GObject parent;
	NautilusSearchEngineDetails *details;
} NautilusSearchEngine;

typedef struct {
	GObjectClass parent_class;
} NautilusSearchEngineClass;

GType                 nautilus_search_engine_get_type           (void);

NautilusSearchEngine *nautilus_search_engine_new                (void);
NautilusSearchEngineModel *
                      nautilus_search_engine_get_model_provider (NautilusSearchEngine *engine);
NautilusSearchEngineSimple *
                      nautilus_search_engine_get_simple_provider (NautilusSearchEngine *engine);

#endif /* NAUTILUS_SEARCH_ENGINE_H */

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

#ifndef NAUTILUS_SEARCH_ENGINE_H
#define NAUTILUS_SEARCH_ENGINE_H

#include <glib-object.h>
#include <libnautilus-private/nautilus-query.h>

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
	
	/* VTable */
	void (*set_query) (NautilusSearchEngine *engine, NautilusQuery *query);
	void (*start) (NautilusSearchEngine *engine);
	void (*stop) (NautilusSearchEngine *engine);

	/* Signals */
	void (*hits_added) (NautilusSearchEngine *engine, GList *hits);
	void (*hits_subtracted) (NautilusSearchEngine *engine, GList *hits);
	void (*finished) (NautilusSearchEngine *engine);
	void (*error) (NautilusSearchEngine *engine, const char *error_message);
} NautilusSearchEngineClass;

GType          nautilus_search_engine_get_type  (void);
gboolean       nautilus_search_engine_enabled (void);

NautilusSearchEngine* nautilus_search_engine_new       (void);

void           nautilus_search_engine_set_query (NautilusSearchEngine *engine, NautilusQuery *query);
void	       nautilus_search_engine_start (NautilusSearchEngine *engine);
void	       nautilus_search_engine_stop (NautilusSearchEngine *engine);

void	       nautilus_search_engine_hits_added (NautilusSearchEngine *engine, GList *hits);
void	       nautilus_search_engine_hits_subtracted (NautilusSearchEngine *engine, GList *hits);
void	       nautilus_search_engine_finished (NautilusSearchEngine *engine);
void	       nautilus_search_engine_error (NautilusSearchEngine *engine, const char *error_message);

#endif /* NAUTILUS_SEARCH_ENGINE_H */

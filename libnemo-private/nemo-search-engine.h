/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Novell, Inc.
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
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#ifndef NEMO_SEARCH_ENGINE_H
#define NEMO_SEARCH_ENGINE_H

#include <glib-object.h>
#include <libnemo-private/nemo-query.h>

#define NEMO_TYPE_SEARCH_ENGINE		(nemo_search_engine_get_type ())
#define NEMO_SEARCH_ENGINE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_SEARCH_ENGINE, NemoSearchEngine))
#define NEMO_SEARCH_ENGINE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_SEARCH_ENGINE, NemoSearchEngineClass))
#define NEMO_IS_SEARCH_ENGINE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_SEARCH_ENGINE))
#define NEMO_IS_SEARCH_ENGINE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_SEARCH_ENGINE))
#define NEMO_SEARCH_ENGINE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_SEARCH_ENGINE, NemoSearchEngineClass))

typedef struct NemoSearchEngineDetails NemoSearchEngineDetails;

typedef struct NemoSearchEngine {
	GObject parent;
	NemoSearchEngineDetails *details;
} NemoSearchEngine;

typedef struct {
	GObjectClass parent_class;
	
	/* VTable */
	void (*set_query) (NemoSearchEngine *engine, NemoQuery *query);
	void (*start) (NemoSearchEngine *engine);
	void (*stop) (NemoSearchEngine *engine);

	/* Signals */
	void (*hits_added) (NemoSearchEngine *engine, GList *hits);
	void (*hits_subtracted) (NemoSearchEngine *engine, GList *hits);
	void (*finished) (NemoSearchEngine *engine);
	void (*error) (NemoSearchEngine *engine, const char *error_message);
} NemoSearchEngineClass;

GType          nemo_search_engine_get_type  (void);
gboolean       nemo_search_engine_enabled (void);

NemoSearchEngine* nemo_search_engine_new       (void);

void           nemo_search_engine_set_query (NemoSearchEngine *engine, NemoQuery *query);
void	       nemo_search_engine_start (NemoSearchEngine *engine);
void	       nemo_search_engine_stop (NemoSearchEngine *engine);

void	       nemo_search_engine_hits_added (NemoSearchEngine *engine, GList *hits);
void	       nemo_search_engine_hits_subtracted (NemoSearchEngine *engine, GList *hits);
void	       nemo_search_engine_finished (NemoSearchEngine *engine);
void	       nemo_search_engine_error (NemoSearchEngine *engine, const char *error_message);

#endif /* NEMO_SEARCH_ENGINE_H */

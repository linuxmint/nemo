/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Red Hat, Inc
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
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef NEMO_SEARCH_ENGINE_SIMPLE_H
#define NEMO_SEARCH_ENGINE_SIMPLE_H

#define NEMO_TYPE_SEARCH_ENGINE_SIMPLE		(nemo_search_engine_simple_get_type ())
#define NEMO_SEARCH_ENGINE_SIMPLE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_SEARCH_ENGINE_SIMPLE, NemoSearchEngineSimple))
#define NEMO_SEARCH_ENGINE_SIMPLE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_SEARCH_ENGINE_SIMPLE, NemoSearchEngineSimpleClass))
#define NEMO_IS_SEARCH_ENGINE_SIMPLE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_SEARCH_ENGINE_SIMPLE))
#define NEMO_IS_SEARCH_ENGINE_SIMPLE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_SEARCH_ENGINE_SIMPLE))
#define NEMO_SEARCH_ENGINE_SIMPLE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_SEARCH_ENGINE_SIMPLE, NemoSearchEngineSimpleClass))

typedef struct NemoSearchEngineSimpleDetails NemoSearchEngineSimpleDetails;

typedef struct NemoSearchEngineSimple {
	GObject parent;
	NemoSearchEngineSimpleDetails *details;
} NemoSearchEngineSimple;

typedef struct {
	GObjectClass parent_class;
} NemoSearchEngineSimpleClass;

GType          nemo_search_engine_simple_get_type  (void);

NemoSearchEngineSimple* nemo_search_engine_simple_new       (void);

#endif /* NEMO_SEARCH_ENGINE_SIMPLE_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Red Hat, Inc
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
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef NAUTILUS_SEARCH_ENGINE_MODEL_H
#define NAUTILUS_SEARCH_ENGINE_MODEL_H

#include <libnautilus-private/nautilus-directory.h>

#define NAUTILUS_TYPE_SEARCH_ENGINE_MODEL		(nautilus_search_engine_model_get_type ())
#define NAUTILUS_SEARCH_ENGINE_MODEL(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_SEARCH_ENGINE_MODEL, NautilusSearchEngineModel))
#define NAUTILUS_SEARCH_ENGINE_MODEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SEARCH_ENGINE_MODEL, NautilusSearchEngineModelClass))
#define NAUTILUS_IS_SEARCH_ENGINE_MODEL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_SEARCH_ENGINE_MODEL))
#define NAUTILUS_IS_SEARCH_ENGINE_MODEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SEARCH_ENGINE_MODEL))
#define NAUTILUS_SEARCH_ENGINE_MODEL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_SEARCH_ENGINE_MODEL, NautilusSearchEngineModelClass))

typedef struct NautilusSearchEngineModelDetails NautilusSearchEngineModelDetails;

typedef struct NautilusSearchEngineModel {
	GObject parent;
	NautilusSearchEngineModelDetails *details;
} NautilusSearchEngineModel;

typedef struct {
	GObjectClass parent_class;
} NautilusSearchEngineModelClass;

GType          nautilus_search_engine_model_get_type  (void);

NautilusSearchEngineModel* nautilus_search_engine_model_new       (void);
void                       nautilus_search_engine_model_set_model (NautilusSearchEngineModel *model,
								   NautilusDirectory         *directory);
NautilusDirectory *        nautilus_search_engine_model_get_model (NautilusSearchEngineModel *model);

#endif /* NAUTILUS_SEARCH_ENGINE_MODEL_H */

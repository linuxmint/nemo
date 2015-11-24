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
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef NEMO_SEARCH_ENGINE_MODEL_H
#define NEMO_SEARCH_ENGINE_MODEL_H

#include <libnemo-private/nemo-directory.h>

#define NEMO_TYPE_SEARCH_ENGINE_MODEL		(nemo_search_engine_model_get_type ())
#define NEMO_SEARCH_ENGINE_MODEL(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_SEARCH_ENGINE_MODEL, NemoSearchEngineModel))
#define NEMO_SEARCH_ENGINE_MODEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_SEARCH_ENGINE_MODEL, NemoSearchEngineModelClass))
#define NEMO_IS_SEARCH_ENGINE_MODEL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_SEARCH_ENGINE_MODEL))
#define NEMO_IS_SEARCH_ENGINE_MODEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_SEARCH_ENGINE_MODEL))
#define NEMO_SEARCH_ENGINE_MODEL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_SEARCH_ENGINE_MODEL, NemoSearchEngineModelClass))

typedef struct NemoSearchEngineModelDetails NemoSearchEngineModelDetails;

typedef struct NemoSearchEngineModel {
	GObject parent;
	NemoSearchEngineModelDetails *details;
} NemoSearchEngineModel;

typedef struct {
	GObjectClass parent_class;
} NemoSearchEngineModelClass;

GType          nemo_search_engine_model_get_type  (void);

NemoSearchEngineModel* nemo_search_engine_model_new       (void);
void                       nemo_search_engine_model_set_model (NemoSearchEngineModel *model,
								   NemoDirectory         *directory);
NemoDirectory *        nemo_search_engine_model_get_model (NemoSearchEngineModel *model);

#endif /* NEMO_SEARCH_ENGINE_MODEL_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Red Hat, Inc.
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

#ifndef NEMO_QUERY_EDITOR_H
#define NEMO_QUERY_EDITOR_H

#include <gtk/gtk.h>

#include <libnemo-private/nemo-query.h>

#define NEMO_TYPE_QUERY_EDITOR nemo_query_editor_get_type()
#define NEMO_QUERY_EDITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_QUERY_EDITOR, NemoQueryEditor))
#define NEMO_QUERY_EDITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_QUERY_EDITOR, NemoQueryEditorClass))
#define NEMO_IS_QUERY_EDITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_QUERY_EDITOR))
#define NEMO_IS_QUERY_EDITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_QUERY_EDITOR))
#define NEMO_QUERY_EDITOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_QUERY_EDITOR, NemoQueryEditorClass))

typedef struct NemoQueryEditorDetails NemoQueryEditorDetails;

typedef struct NemoQueryEditor {
	GtkBox parent;
	NemoQueryEditorDetails *details;
} NemoQueryEditor;

typedef struct {
	GtkBoxClass parent_class;

	void (* changed) (NemoQueryEditor  *editor,
			  NemoQuery        *query,
			  gboolean              reload);
	void (* cancel)   (NemoQueryEditor *editor);
	void (* activated) (NemoQueryEditor *editor);
} NemoQueryEditorClass;

#include "nemo-window-slot.h"

GType      nemo_query_editor_get_type     	   (void);
GtkWidget* nemo_query_editor_new          	   (void);

gboolean       nemo_query_editor_handle_event (NemoQueryEditor *editor,
						   GdkEventKey         *event);

NemoQuery *nemo_query_editor_get_query   (NemoQueryEditor *editor);
void           nemo_query_editor_set_query   (NemoQueryEditor *editor,
						  NemoQuery       *query);
GFile *        nemo_query_editor_get_location (NemoQueryEditor *editor);
void           nemo_query_editor_set_location (NemoQueryEditor *editor,
						   GFile               *location);
#endif /* NEMO_QUERY_EDITOR_H */

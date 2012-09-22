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
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef NEMO_QUERY_EDITOR_H
#define NEMO_QUERY_EDITOR_H

#include <gtk/gtk.h>

#include <libnemo-private/nemo-query.h>

#include "nemo-search-bar.h"

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
} NemoQueryEditorClass;

#include "nemo-window-slot.h"

GType      nemo_query_editor_get_type     	   (void);
GtkWidget* nemo_query_editor_new          	   (gboolean start_hidden);
GtkWidget* nemo_query_editor_new_with_bar      (gboolean start_hidden,
						    gboolean start_attached,
						    NemoSearchBar *bar,
						    NemoWindowSlot *slot);
void       nemo_query_editor_set_default_query (NemoQueryEditor *editor);

void	   nemo_query_editor_grab_focus (NemoQueryEditor *editor);
void       nemo_query_editor_clear_query (NemoQueryEditor *editor);

NemoQuery *nemo_query_editor_get_query   (NemoQueryEditor *editor);
void           nemo_query_editor_set_query   (NemoQueryEditor *editor,
						  NemoQuery       *query);
void           nemo_query_editor_set_visible (NemoQueryEditor *editor,
						  gboolean             visible);

#endif /* NEMO_QUERY_EDITOR_H */

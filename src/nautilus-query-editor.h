/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Red Hat, Inc.
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
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef NAUTILUS_QUERY_EDITOR_H
#define NAUTILUS_QUERY_EDITOR_H

#include <gtk/gtk.h>

#include <libnautilus-private/nautilus-query.h>

#include "nautilus-search-bar.h"

#define NAUTILUS_TYPE_QUERY_EDITOR nautilus_query_editor_get_type()
#define NAUTILUS_QUERY_EDITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_QUERY_EDITOR, NautilusQueryEditor))
#define NAUTILUS_QUERY_EDITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_QUERY_EDITOR, NautilusQueryEditorClass))
#define NAUTILUS_IS_QUERY_EDITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_QUERY_EDITOR))
#define NAUTILUS_IS_QUERY_EDITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_QUERY_EDITOR))
#define NAUTILUS_QUERY_EDITOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_QUERY_EDITOR, NautilusQueryEditorClass))

typedef struct NautilusQueryEditorDetails NautilusQueryEditorDetails;

typedef struct NautilusQueryEditor {
	GtkBox parent;
	NautilusQueryEditorDetails *details;
} NautilusQueryEditor;

typedef struct {
	GtkBoxClass parent_class;

	void (* changed) (NautilusQueryEditor  *editor,
			  NautilusQuery        *query,
			  gboolean              reload);
	void (* cancel)   (NautilusQueryEditor *editor);
} NautilusQueryEditorClass;

#include "nautilus-window-slot.h"

GType      nautilus_query_editor_get_type     	   (void);
GtkWidget* nautilus_query_editor_new          	   (gboolean start_hidden);
GtkWidget* nautilus_query_editor_new_with_bar      (gboolean start_hidden,
						    gboolean start_attached,
						    NautilusSearchBar *bar,
						    NautilusWindowSlot *slot);
void       nautilus_query_editor_set_default_query (NautilusQueryEditor *editor);

void	   nautilus_query_editor_grab_focus (NautilusQueryEditor *editor);
void       nautilus_query_editor_clear_query (NautilusQueryEditor *editor);

NautilusQuery *nautilus_query_editor_get_query   (NautilusQueryEditor *editor);
void           nautilus_query_editor_set_query   (NautilusQueryEditor *editor,
						  NautilusQuery       *query);
void           nautilus_query_editor_set_visible (NautilusQueryEditor *editor,
						  gboolean             visible);

#endif /* NAUTILUS_QUERY_EDITOR_H */

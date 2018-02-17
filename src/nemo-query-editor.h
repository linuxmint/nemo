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

G_BEGIN_DECLS

#define NEMO_TYPE_QUERY_EDITOR (nemo_query_editor_get_type ())

G_DECLARE_FINAL_TYPE (NemoQueryEditor, nemo_query_editor, NEMO, QUERY_EDITOR, GtkBox)

GtkWidget* nemo_query_editor_new                (void);

NemoQuery   *nemo_query_editor_get_query          (NemoQueryEditor *editor);
void         nemo_query_editor_set_query          (NemoQueryEditor *editor,
                                                   NemoQuery       *query);
GFile       *nemo_query_editor_get_location       (NemoQueryEditor *editor);
void         nemo_query_editor_set_location       (NemoQueryEditor *editor,
                                                   GFile           *location);
void         nemo_query_editor_set_active         (NemoQueryEditor *editor,
                                                   gchar           *base_uri,
                                                   gboolean         active);
gboolean     nemo_query_editor_get_active         (NemoQueryEditor *editor);
const gchar *nemo_query_editor_get_base_uri       (NemoQueryEditor *editor);

G_END_DECLS

#endif /* NEMO_QUERY_EDITOR_H */

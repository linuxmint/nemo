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

#ifndef NAUTILUS_QUERY_H
#define NAUTILUS_QUERY_H

#include <glib-object.h>

#define NAUTILUS_TYPE_QUERY		(nautilus_query_get_type ())
#define NAUTILUS_QUERY(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_QUERY, NautilusQuery))
#define NAUTILUS_QUERY_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_QUERY, NautilusQueryClass))
#define NAUTILUS_IS_QUERY(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_QUERY))
#define NAUTILUS_IS_QUERY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_QUERY))
#define NAUTILUS_QUERY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_QUERY, NautilusQueryClass))

typedef struct NautilusQueryDetails NautilusQueryDetails;

typedef struct NautilusQuery {
	GObject parent;
	NautilusQueryDetails *details;
} NautilusQuery;

typedef struct {
	GObjectClass parent_class;
} NautilusQueryClass;

GType          nautilus_query_get_type (void);
gboolean       nautilus_query_enabled  (void);

NautilusQuery* nautilus_query_new      (void);

char *         nautilus_query_get_text           (NautilusQuery *query);
void           nautilus_query_set_text           (NautilusQuery *query, const char *text);

char *         nautilus_query_get_location       (NautilusQuery *query);
void           nautilus_query_set_location       (NautilusQuery *query, const char *uri);

GList *        nautilus_query_get_mime_types     (NautilusQuery *query);
void           nautilus_query_set_mime_types     (NautilusQuery *query, GList *mime_types);
void           nautilus_query_add_mime_type      (NautilusQuery *query, const char *mime_type);

char *         nautilus_query_to_readable_string (NautilusQuery *query);
NautilusQuery *nautilus_query_load               (char *file);
gboolean       nautilus_query_save               (NautilusQuery *query, char *file);

#endif /* NAUTILUS_QUERY_H */

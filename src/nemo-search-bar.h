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

#ifndef NEMO_SEARCH_BAR_H
#define NEMO_SEARCH_BAR_H

#include <gtk/gtk.h>
#include <libnemo-private/nemo-query.h>

#define NEMO_TYPE_SEARCH_BAR nemo_search_bar_get_type()
#define NEMO_SEARCH_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_SEARCH_BAR, NemoSearchBar))
#define NEMO_SEARCH_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_SEARCH_BAR, NemoSearchBarClass))
#define NEMO_IS_SEARCH_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_SEARCH_BAR))
#define NEMO_IS_SEARCH_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_SEARCH_BAR))
#define NEMO_SEARCH_BAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_SEARCH_BAR, NemoSearchBarClass))

typedef struct NemoSearchBarDetails NemoSearchBarDetails;

typedef struct NemoSearchBar {
	GtkBox parent;
	NemoSearchBarDetails *details;
} NemoSearchBar;

typedef struct {
	GtkBoxClass parent_class;

	void (* activate) (NemoSearchBar *bar);
	void (* cancel)   (NemoSearchBar *bar);
} NemoSearchBarClass;

GType      nemo_search_bar_get_type     	(void);
GtkWidget* nemo_search_bar_new          	(void);

GtkWidget *    nemo_search_bar_get_entry     (NemoSearchBar *bar);
GtkWidget *    nemo_search_bar_borrow_entry  (NemoSearchBar *bar);
void           nemo_search_bar_return_entry  (NemoSearchBar *bar);
void           nemo_search_bar_grab_focus    (NemoSearchBar *bar);
NemoQuery *nemo_search_bar_get_query     (NemoSearchBar *bar);
void           nemo_search_bar_clear         (NemoSearchBar *bar);

#endif /* NEMO_SEARCH_BAR_H */

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

#ifndef NAUTILUS_SEARCH_BAR_H
#define NAUTILUS_SEARCH_BAR_H

#include <gtk/gtk.h>
#include <libnautilus-private/nautilus-query.h>

#define NAUTILUS_TYPE_SEARCH_BAR nautilus_search_bar_get_type()
#define NAUTILUS_SEARCH_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_SEARCH_BAR, NautilusSearchBar))
#define NAUTILUS_SEARCH_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SEARCH_BAR, NautilusSearchBarClass))
#define NAUTILUS_IS_SEARCH_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_SEARCH_BAR))
#define NAUTILUS_IS_SEARCH_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SEARCH_BAR))
#define NAUTILUS_SEARCH_BAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_SEARCH_BAR, NautilusSearchBarClass))

typedef struct NautilusSearchBarDetails NautilusSearchBarDetails;

typedef struct NautilusSearchBar {
	GtkBox parent;
	NautilusSearchBarDetails *details;
} NautilusSearchBar;

typedef struct {
	GtkBoxClass parent_class;

	void (* activate) (NautilusSearchBar *bar);
	void (* cancel)   (NautilusSearchBar *bar);
} NautilusSearchBarClass;

GType      nautilus_search_bar_get_type     	(void);
GtkWidget* nautilus_search_bar_new          	(void);

GtkWidget *    nautilus_search_bar_get_entry     (NautilusSearchBar *bar);
GtkWidget *    nautilus_search_bar_borrow_entry  (NautilusSearchBar *bar);
void           nautilus_search_bar_return_entry  (NautilusSearchBar *bar);
void           nautilus_search_bar_grab_focus    (NautilusSearchBar *bar);
NautilusQuery *nautilus_search_bar_get_query     (NautilusSearchBar *bar);
void           nautilus_search_bar_clear         (NautilusSearchBar *bar);

#endif /* NAUTILUS_SEARCH_BAR_H */

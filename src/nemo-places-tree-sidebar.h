

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Copyright (C) 2000, 2001 Eazel, Inc
 * Copyright (C) 2002 Anders Carlsson
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Klaus-Dieter Menk <kd@kmenk.de>
 */

/* fm-tree-view.h - tree view. */

#ifndef NEMO_PLACES_TREE_SIDEBAR_H
#define NEMO_PLACES_TREE_SIDEBAR_H

#include "nemo-window.h"

#include <gtk/gtk.h>

#define NEMO_PLACES_TREE_SIDEBAR_ID    "placestree"

#define NEMO_TYPE_PLACES_TREE_SIDEBAR nemo_places_tree_sidebar_get_type()
#define NEMO_PLACES_TREE_SIDEBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_PLACES_TREE_SIDEBAR, NemoPlacesTreeSidebar))
#define NEMO_PLACES_TREE_SIDEBAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_PLACES_TREE_SIDEBAR, NemoPlacesTreeSidebarClass))
#define NEMO_IS_PLACES_TREE_SIDEBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_PLACES_TREE_SIDEBAR))
#define NEMO_IS_PLACES_TREE_SIDEBAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_PLACES_TREE_SIDEBAR))
#define NEMO_PLACES_TREE_SIDEBAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_PLACES_TREE_SIDEBAR, NemoPlacesTreeSidebarClass))


GType nemo_places_tree_sidebar_get_type (void);
GtkWidget * nemo_places_tree_sidebar_new (NemoWindow *window);

#endif /* NEMO_PLACES_TREE_SIDEBAR_H */



/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-view.h - interface for list view of directory.

   Copyright (C) 2000 Eazel, Inc.
   Copyright (C) 2001 Anders Carlsson <andersca@gnu.org>
   
   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: John Sullivan <sullivan@eazel.com>
            Anders Carlsson <andersca@gnu.org>
*/

#ifndef NEMO_LIST_VIEW_H
#define NEMO_LIST_VIEW_H

#include "nemo-view.h"

#define NEMO_TYPE_LIST_VIEW nemo_list_view_get_type()
#define NEMO_LIST_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_LIST_VIEW, NemoListView))
#define NEMO_LIST_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_LIST_VIEW, NemoListViewClass))
#define NEMO_IS_LIST_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_LIST_VIEW))
#define NEMO_IS_LIST_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_LIST_VIEW))
#define NEMO_LIST_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_LIST_VIEW, NemoListViewClass))

#define NEMO_LIST_VIEW_ID "OAFIID:Nemo_File_Manager_List_View"

typedef struct NemoListViewDetails NemoListViewDetails;

typedef struct {
	NemoView parent_instance;
	NemoListViewDetails *details;
} NemoListView;

typedef struct {
	NemoViewClass parent_class;
} NemoListViewClass;

GType nemo_list_view_get_type (void);
void  nemo_list_view_register (void);
GtkTreeView* nemo_list_view_get_tree_view (NemoListView *list_view);

#endif /* NEMO_LIST_VIEW_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-empty-view.h - interface for empty view of directory.

   Copyright (C) 2006 Free Software Foundation, Inc.
   
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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Christian Neumair <chris@gnome-de.org>
*/

#ifndef NEMO_EMPTY_VIEW_H
#define NEMO_EMPTY_VIEW_H

#include "nemo-view.h"

#define NEMO_TYPE_EMPTY_VIEW nemo_empty_view_get_type()
#define NEMO_EMPTY_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_EMPTY_VIEW, NemoEmptyView))
#define NEMO_EMPTY_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_EMPTY_VIEW, NemoEmptyViewClass))
#define NEMO_IS_EMPTY_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_EMPTY_VIEW))
#define NEMO_IS_EMPTY_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_EMPTY_VIEW))
#define NEMO_EMPTY_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_EMPTY_VIEW, NemoEmptyViewClass))

#define NEMO_EMPTY_VIEW_ID "OAFIID:Nemo_File_Manager_Empty_View"

typedef struct NemoEmptyViewDetails NemoEmptyViewDetails;

typedef struct {
	NemoView parent_instance;
	NemoEmptyViewDetails *details;
} NemoEmptyView;

typedef struct {
	NemoViewClass parent_class;
} NemoEmptyViewClass;

GType nemo_empty_view_get_type (void);
void  nemo_empty_view_register (void);

#endif /* NEMO_EMPTY_VIEW_H */

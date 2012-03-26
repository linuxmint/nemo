/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-empty-view.h - interface for empty view of directory.

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

#ifndef NAUTILUS_EMPTY_VIEW_H
#define NAUTILUS_EMPTY_VIEW_H

#include "nautilus-view.h"

#define NAUTILUS_TYPE_EMPTY_VIEW nautilus_empty_view_get_type()
#define NAUTILUS_EMPTY_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_EMPTY_VIEW, NautilusEmptyView))
#define NAUTILUS_EMPTY_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_EMPTY_VIEW, NautilusEmptyViewClass))
#define NAUTILUS_IS_EMPTY_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_EMPTY_VIEW))
#define NAUTILUS_IS_EMPTY_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_EMPTY_VIEW))
#define NAUTILUS_EMPTY_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_EMPTY_VIEW, NautilusEmptyViewClass))

#define NAUTILUS_EMPTY_VIEW_ID "OAFIID:Nautilus_File_Manager_Empty_View"

typedef struct NautilusEmptyViewDetails NautilusEmptyViewDetails;

typedef struct {
	NautilusView parent_instance;
	NautilusEmptyViewDetails *details;
} NautilusEmptyView;

typedef struct {
	NautilusViewClass parent_class;
} NautilusEmptyViewClass;

GType nautilus_empty_view_get_type (void);
void  nautilus_empty_view_register (void);

#endif /* NAUTILUS_EMPTY_VIEW_H */

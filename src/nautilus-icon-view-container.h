/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-icon-container.h - the container widget for file manager icons

   Copyright (C) 2002 Sun Microsystems, Inc.

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

   Author: Michael Meeks <michael@ximian.com>
*/

#ifndef NAUTILUS_ICON_VIEW_CONTAINER_H
#define NAUTILUS_ICON_VIEW_CONTAINER_H

#include "nautilus-icon-view.h"

#include <libnautilus-private/nautilus-icon-container.h>

typedef struct NautilusIconViewContainer NautilusIconViewContainer;
typedef struct NautilusIconViewContainerClass NautilusIconViewContainerClass;

#define NAUTILUS_TYPE_ICON_VIEW_CONTAINER nautilus_icon_view_container_get_type()
#define NAUTILUS_ICON_VIEW_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_ICON_VIEW_CONTAINER, NautilusIconViewContainer))
#define NAUTILUS_ICON_VIEW_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ICON_VIEW_CONTAINER, NautilusIconViewContainerClass))
#define NAUTILUS_IS_ICON_VIEW_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_ICON_VIEW_CONTAINER))
#define NAUTILUS_IS_ICON_VIEW_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ICON_VIEW_CONTAINER))
#define NAUTILUS_ICON_VIEW_CONTAINER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_ICON_VIEW_CONTAINER, NautilusIconViewContainerClass))

typedef struct NautilusIconViewContainerDetails NautilusIconViewContainerDetails;

struct NautilusIconViewContainer {
	NautilusIconContainer parent;

	NautilusIconView *view;
	gboolean    sort_for_desktop;
};

struct NautilusIconViewContainerClass {
	NautilusIconContainerClass parent_class;
};

GType                  nautilus_icon_view_container_get_type         (void);
NautilusIconContainer *nautilus_icon_view_container_construct        (NautilusIconViewContainer *icon_container,
								      NautilusIconView      *view);
NautilusIconContainer *nautilus_icon_view_container_new              (NautilusIconView      *view);
void                   nautilus_icon_view_container_set_sort_desktop (NautilusIconViewContainer *container,
								      gboolean         desktop);

#endif /* NAUTILUS_ICON_VIEW_CONTAINER_H */

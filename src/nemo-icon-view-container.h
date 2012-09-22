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
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Author: Michael Meeks <michael@ximian.com>
*/

#ifndef NEMO_ICON_VIEW_CONTAINER_H
#define NEMO_ICON_VIEW_CONTAINER_H

#include "nemo-icon-view.h"

#include <libnemo-private/nemo-icon-container.h>

typedef struct NemoIconViewContainer NemoIconViewContainer;
typedef struct NemoIconViewContainerClass NemoIconViewContainerClass;

#define NEMO_TYPE_ICON_VIEW_CONTAINER nemo_icon_view_container_get_type()
#define NEMO_ICON_VIEW_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_ICON_VIEW_CONTAINER, NemoIconViewContainer))
#define NEMO_ICON_VIEW_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_ICON_VIEW_CONTAINER, NemoIconViewContainerClass))
#define NEMO_IS_ICON_VIEW_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_ICON_VIEW_CONTAINER))
#define NEMO_IS_ICON_VIEW_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_ICON_VIEW_CONTAINER))
#define NEMO_ICON_VIEW_CONTAINER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_ICON_VIEW_CONTAINER, NemoIconViewContainerClass))

typedef struct NemoIconViewContainerDetails NemoIconViewContainerDetails;

struct NemoIconViewContainer {
	NemoIconContainer parent;

	NemoIconView *view;
	gboolean    sort_for_desktop;
};

struct NemoIconViewContainerClass {
	NemoIconContainerClass parent_class;
};

GType                  nemo_icon_view_container_get_type         (void);
NemoIconContainer *nemo_icon_view_container_construct        (NemoIconViewContainer *icon_container,
								      NemoIconView      *view);
NemoIconContainer *nemo_icon_view_container_new              (NemoIconView      *view);
void                   nemo_icon_view_container_set_sort_desktop (NemoIconViewContainer *container,
								      gboolean         desktop);

#endif /* NEMO_ICON_VIEW_CONTAINER_H */

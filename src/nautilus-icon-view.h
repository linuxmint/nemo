/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-icon-view.h - interface for icon view of directory.
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 *
 */

#ifndef NAUTILUS_ICON_VIEW_H
#define NAUTILUS_ICON_VIEW_H

#include "nautilus-view.h"

typedef struct NautilusIconView NautilusIconView;
typedef struct NautilusIconViewClass NautilusIconViewClass;

#define NAUTILUS_TYPE_ICON_VIEW nautilus_icon_view_get_type()
#define NAUTILUS_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_ICON_VIEW, NautilusIconView))
#define NAUTILUS_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ICON_VIEW, NautilusIconViewClass))
#define NAUTILUS_IS_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_ICON_VIEW))
#define NAUTILUS_IS_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ICON_VIEW))
#define NAUTILUS_ICON_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_ICON_VIEW, NautilusIconViewClass))

#define NAUTILUS_ICON_VIEW_ID "OAFIID:Nautilus_File_Manager_Icon_View"
#define FM_COMPACT_VIEW_ID "OAFIID:Nautilus_File_Manager_Compact_View"

typedef struct NautilusIconViewDetails NautilusIconViewDetails;

struct NautilusIconView {
	NautilusView parent;
	NautilusIconViewDetails *details;
};

struct NautilusIconViewClass {
	NautilusViewClass parent_class;
};

/* GObject support */
GType   nautilus_icon_view_get_type      (void);
int     nautilus_icon_view_compare_files (NautilusIconView   *icon_view,
					  NautilusFile *a,
					  NautilusFile *b);
void    nautilus_icon_view_filter_by_screen (NautilusIconView *icon_view,
					     gboolean filter);
gboolean nautilus_icon_view_is_compact   (NautilusIconView *icon_view);

void    nautilus_icon_view_register         (void);
void    nautilus_icon_view_compact_register (void);

NautilusIconContainer * nautilus_icon_view_get_icon_container (NautilusIconView *view);

#endif /* NAUTILUS_ICON_VIEW_H */

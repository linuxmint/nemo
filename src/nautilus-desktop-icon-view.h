/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-icon-view.h - interface for icon view of directory.

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: Mike Engber <engber@eazel.com>
*/

#ifndef NAUTILUS_DESKTOP_ICON_VIEW_H
#define NAUTILUS_DESKTOP_ICON_VIEW_H

#include "nautilus-icon-view.h"

#define NAUTILUS_TYPE_DESKTOP_ICON_VIEW nautilus_desktop_icon_view_get_type()
#define NAUTILUS_DESKTOP_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_DESKTOP_ICON_VIEW, NautilusDesktopIconView))
#define NAUTILUS_DESKTOP_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DESKTOP_ICON_VIEW, NautilusDesktopIconViewClass))
#define NAUTILUS_IS_DESKTOP_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_DESKTOP_ICON_VIEW))
#define NAUTILUS_IS_DESKTOP_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DESKTOP_ICON_VIEW))
#define NAUTILUS_DESKTOP_ICON_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_DESKTOP_ICON_VIEW, NautilusDesktopIconViewClass))

#define NAUTILUS_DESKTOP_ICON_VIEW_ID "OAFIID:Nautilus_File_Manager_Desktop_Icon_View"

typedef struct NautilusDesktopIconViewDetails NautilusDesktopIconViewDetails;
typedef struct {
	NautilusIconView parent;
	NautilusDesktopIconViewDetails *details;
} NautilusDesktopIconView;

typedef struct {
	NautilusIconViewClass parent_class;
} NautilusDesktopIconViewClass;

/* GObject support */
GType   nautilus_desktop_icon_view_get_type (void);
void nautilus_desktop_icon_view_register (void);

#endif /* NAUTILUS_DESKTOP_ICON_VIEW_H */

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
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Mike Engber <engber@eazel.com>
*/

#ifndef NEMO_DESKTOP_ICON_GRID_VIEW_H
#define NEMO_DESKTOP_ICON_GRID_VIEW_H

#include "nemo-icon-view.h"

#define NEMO_TYPE_DESKTOP_ICON_GRID_VIEW nemo_desktop_icon_grid_view_get_type()
#define NEMO_DESKTOP_ICON_GRID_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_DESKTOP_ICON_GRID_VIEW, NemoDesktopIconGridView))
#define NEMO_DESKTOP_ICON_GRID_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_DESKTOP_ICON_GRID_VIEW, NemoDesktopIconGridViewClass))
#define NEMO_IS_DESKTOP_ICON_GRID_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_DESKTOP_ICON_GRID_VIEW))
#define NEMO_IS_DESKTOP_ICON_GRID_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_DESKTOP_ICON_GRID_VIEW))
#define NEMO_DESKTOP_ICON_GRID_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_DESKTOP_ICON_GRID_VIEW, NemoDesktopIconGridViewClass))

typedef struct NemoDesktopIconGridViewDetails NemoDesktopIconGridViewDetails;
typedef struct {
	NemoIconView parent;
	NemoDesktopIconGridViewDetails *details;
} NemoDesktopIconGridView;

typedef struct {
	NemoIconViewClass parent_class;
} NemoDesktopIconGridViewClass;

/* GObject support */
GType   nemo_desktop_icon_grid_view_get_type (void);
void nemo_desktop_icon_grid_view_register (void);
GtkActionGroup *nemo_desktop_icon_grid_view_get_action_group (NemoDesktopIconGridView *view);
void nemo_desktop_icon_grid_view_set_grid_adjusts (NemoDesktopIconGridView *view,
                                                   gint                     h_adjust,
                                                   gint                     v_adjust);

#endif /* NEMO_DESKTOP_ICON_GRID_VIEW_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-icon-view.h - interface for icon view of directory.
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
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 *
 */

#ifndef NEMO_ICON_VIEW_H
#define NEMO_ICON_VIEW_H

#include "nemo-view.h"

typedef struct NemoIconView NemoIconView;
typedef struct NemoIconViewClass NemoIconViewClass;

#define NEMO_TYPE_ICON_VIEW nemo_icon_view_get_type()
#define NEMO_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_ICON_VIEW, NemoIconView))
#define NEMO_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_ICON_VIEW, NemoIconViewClass))
#define NEMO_IS_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_ICON_VIEW))
#define NEMO_IS_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_ICON_VIEW))
#define NEMO_ICON_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_ICON_VIEW, NemoIconViewClass))

#define NEMO_ICON_VIEW_ID "OAFIID:Nemo_File_Manager_Icon_View"
#define FM_COMPACT_VIEW_ID "OAFIID:Nemo_File_Manager_Compact_View"

typedef struct NemoIconViewDetails NemoIconViewDetails;

struct NemoIconView {
	NemoView parent;
	NemoIconViewDetails *details;
};

struct NemoIconViewClass {
	NemoViewClass parent_class;
};

/* GObject support */
GType   nemo_icon_view_get_type      (void);
int     nemo_icon_view_compare_files (NemoIconView   *icon_view,
					  NemoFile *a,
					  NemoFile *b);
void    nemo_icon_view_filter_by_screen (NemoIconView *icon_view,
					     gboolean filter);
gboolean nemo_icon_view_is_compact   (NemoIconView *icon_view);

void    nemo_icon_view_register         (void);
void    nemo_icon_view_compact_register (void);

NemoIconContainer * nemo_icon_view_get_icon_container (NemoIconView *view);

#endif /* NEMO_ICON_VIEW_H */

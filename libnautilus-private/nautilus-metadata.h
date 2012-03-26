/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-metadata.h: #defines and other metadata-related info
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: John Sullivan <sullivan@eazel.com>
*/

#ifndef NAUTILUS_METADATA_H
#define NAUTILUS_METADATA_H

/* Keys for getting/setting Nautilus metadata. All metadata used in Nautilus
 * should define its key here, so we can keep track of the whole set easily.
 * Any updates here needs to be added in nautilus-metadata.c too.
 */

#include <glib.h>

/* Per-file */

#define NAUTILUS_METADATA_KEY_DEFAULT_VIEW		 	"nautilus-default-view"

#define NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_COLOR 	"folder-background-color"
#define NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_IMAGE 	"folder-background-image"

#define NAUTILUS_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL       	"nautilus-icon-view-zoom-level"
#define NAUTILUS_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT      	"nautilus-icon-view-auto-layout"
#define NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY          	"nautilus-icon-view-sort-by"
#define NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED    	"nautilus-icon-view-sort-reversed"
#define NAUTILUS_METADATA_KEY_ICON_VIEW_KEEP_ALIGNED            "nautilus-icon-view-keep-aligned"
#define NAUTILUS_METADATA_KEY_ICON_VIEW_LAYOUT_TIMESTAMP	"nautilus-icon-view-layout-timestamp"

#define NAUTILUS_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL       	"nautilus-list-view-zoom-level"
#define NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN      	"nautilus-list-view-sort-column"
#define NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED    	"nautilus-list-view-sort-reversed"
#define NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS    	"nautilus-list-view-visible-columns"
#define NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER    	"nautilus-list-view-column-order"

#define NAUTILUS_METADATA_KEY_COMPACT_VIEW_ZOOM_LEVEL		"nautilus-compact-view-zoom-level"

#define NAUTILUS_METADATA_KEY_WINDOW_GEOMETRY			"nautilus-window-geometry"
#define NAUTILUS_METADATA_KEY_WINDOW_SCROLL_POSITION		"nautilus-window-scroll-position"
#define NAUTILUS_METADATA_KEY_WINDOW_SHOW_HIDDEN_FILES		"nautilus-window-show-hidden-files"
#define NAUTILUS_METADATA_KEY_WINDOW_MAXIMIZED			"nautilus-window-maximized"
#define NAUTILUS_METADATA_KEY_WINDOW_STICKY			"nautilus-window-sticky"
#define NAUTILUS_METADATA_KEY_WINDOW_KEEP_ABOVE			"nautilus-window-keep-above"

#define NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR   	"nautilus-sidebar-background-color"
#define NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE   	"nautilus-sidebar-background-image"
#define NAUTILUS_METADATA_KEY_SIDEBAR_BUTTONS			"nautilus-sidebar-buttons"

#define NAUTILUS_METADATA_KEY_ICON_POSITION              	"nautilus-icon-position"
#define NAUTILUS_METADATA_KEY_ICON_POSITION_TIMESTAMP		"nautilus-icon-position-timestamp"
#define NAUTILUS_METADATA_KEY_ANNOTATION                 	"annotation"
#define NAUTILUS_METADATA_KEY_ICON_SCALE                 	"icon-scale"
#define NAUTILUS_METADATA_KEY_CUSTOM_ICON                	"custom-icon"
#define NAUTILUS_METADATA_KEY_CUSTOM_ICON_NAME                	"custom-icon-name"
#define NAUTILUS_METADATA_KEY_SCREEN				"screen"
#define NAUTILUS_METADATA_KEY_EMBLEMS				"emblems"

guint nautilus_metadata_get_id (const char *metadata);

#endif /* NAUTILUS_METADATA_H */

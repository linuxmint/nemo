/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-metadata.h: #defines and other metadata-related info
 
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
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
  
   Author: John Sullivan <sullivan@eazel.com>
*/

#ifndef NEMO_METADATA_H
#define NEMO_METADATA_H

/* Keys for getting/setting Nemo metadata. All metadata used in Nemo
 * should define its key here, so we can keep track of the whole set easily.
 * Any updates here needs to be added in nemo-metadata.c too.
 */

#include <glib.h>

/* Per-file */

#define NEMO_METADATA_KEY_DEFAULT_VIEW		 	"nemo-default-view"

#define NEMO_METADATA_KEY_LOCATION_BACKGROUND_COLOR 	"folder-background-color"
#define NEMO_METADATA_KEY_LOCATION_BACKGROUND_IMAGE 	"folder-background-image"

#define NEMO_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL       	"nemo-icon-view-zoom-level"
#define NEMO_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT      	"nemo-icon-view-auto-layout"
#define NEMO_METADATA_KEY_ICON_VIEW_SORT_BY          	"nemo-icon-view-sort-by"
#define NEMO_METADATA_KEY_ICON_VIEW_SORT_REVERSED    	"nemo-icon-view-sort-reversed"
#define NEMO_METADATA_KEY_ICON_VIEW_KEEP_ALIGNED            "nemo-icon-view-keep-aligned"
#define NEMO_METADATA_KEY_ICON_VIEW_LAYOUT_TIMESTAMP	"nemo-icon-view-layout-timestamp"

#define NEMO_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL       	"nemo-list-view-zoom-level"
#define NEMO_METADATA_KEY_LIST_VIEW_SORT_COLUMN      	"nemo-list-view-sort-column"
#define NEMO_METADATA_KEY_LIST_VIEW_SORT_REVERSED    	"nemo-list-view-sort-reversed"
#define NEMO_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS    	"nemo-list-view-visible-columns"
#define NEMO_METADATA_KEY_LIST_VIEW_COLUMN_ORDER    	"nemo-list-view-column-order"

#define NEMO_METADATA_KEY_COMPACT_VIEW_ZOOM_LEVEL		"nemo-compact-view-zoom-level"

#define NEMO_METADATA_KEY_WINDOW_GEOMETRY			"nemo-window-geometry"
#define NEMO_METADATA_KEY_WINDOW_SCROLL_POSITION		"nemo-window-scroll-position"
#define NEMO_METADATA_KEY_WINDOW_SHOW_HIDDEN_FILES		"nemo-window-show-hidden-files"
#define NEMO_METADATA_KEY_WINDOW_MAXIMIZED			"nemo-window-maximized"
#define NEMO_METADATA_KEY_WINDOW_STICKY			"nemo-window-sticky"
#define NEMO_METADATA_KEY_WINDOW_KEEP_ABOVE			"nemo-window-keep-above"

#define NEMO_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR   	"nemo-sidebar-background-color"
#define NEMO_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE   	"nemo-sidebar-background-image"
#define NEMO_METADATA_KEY_SIDEBAR_BUTTONS			"nemo-sidebar-buttons"

#define NEMO_METADATA_KEY_ICON_POSITION              	"nemo-icon-position"
#define NEMO_METADATA_KEY_ICON_POSITION_TIMESTAMP		"nemo-icon-position-timestamp"
#define NEMO_METADATA_KEY_ANNOTATION                 	"annotation"
#define NEMO_METADATA_KEY_ICON_SCALE                 	"icon-scale"
#define NEMO_METADATA_KEY_CUSTOM_ICON                	"custom-icon"
#define NEMO_METADATA_KEY_CUSTOM_ICON_NAME                	"custom-icon-name"
#define NEMO_METADATA_KEY_SCREEN				"screen"
#define NEMO_METADATA_KEY_EMBLEMS				"emblems"

guint nemo_metadata_get_id (const char *metadata);

#endif /* NEMO_METADATA_H */

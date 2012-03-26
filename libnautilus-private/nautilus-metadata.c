/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-*/

/* nautilus-metadata.c - metadata utils
 *
 * Copyright (C) 2009 Red Hatl, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "nautilus-metadata.h"
#include <glib.h>

static char *used_metadata_names[] = {
  NAUTILUS_METADATA_KEY_DEFAULT_VIEW,
  NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_COLOR,
  NAUTILUS_METADATA_KEY_LOCATION_BACKGROUND_IMAGE,
  NAUTILUS_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL,
  NAUTILUS_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT,
  NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY,
  NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
  NAUTILUS_METADATA_KEY_ICON_VIEW_KEEP_ALIGNED,
  NAUTILUS_METADATA_KEY_ICON_VIEW_LAYOUT_TIMESTAMP,
  NAUTILUS_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL,
  NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
  NAUTILUS_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
  NAUTILUS_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS,
  NAUTILUS_METADATA_KEY_LIST_VIEW_COLUMN_ORDER,
  NAUTILUS_METADATA_KEY_COMPACT_VIEW_ZOOM_LEVEL,
  NAUTILUS_METADATA_KEY_WINDOW_GEOMETRY,
  NAUTILUS_METADATA_KEY_WINDOW_SCROLL_POSITION,
  NAUTILUS_METADATA_KEY_WINDOW_SHOW_HIDDEN_FILES,
  NAUTILUS_METADATA_KEY_WINDOW_MAXIMIZED,
  NAUTILUS_METADATA_KEY_WINDOW_STICKY,
  NAUTILUS_METADATA_KEY_WINDOW_KEEP_ABOVE,
  NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR,
  NAUTILUS_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE,
  NAUTILUS_METADATA_KEY_SIDEBAR_BUTTONS,
  NAUTILUS_METADATA_KEY_ANNOTATION,
  NAUTILUS_METADATA_KEY_ICON_POSITION,
  NAUTILUS_METADATA_KEY_ICON_POSITION_TIMESTAMP,
  NAUTILUS_METADATA_KEY_ICON_SCALE,
  NAUTILUS_METADATA_KEY_CUSTOM_ICON,
  NAUTILUS_METADATA_KEY_CUSTOM_ICON_NAME,
  NAUTILUS_METADATA_KEY_SCREEN,
  NAUTILUS_METADATA_KEY_EMBLEMS,
  NULL
};

guint
nautilus_metadata_get_id (const char *metadata)
{
  static GHashTable *hash;
  int i;

  if (hash == NULL)
    {
      hash = g_hash_table_new (g_str_hash, g_str_equal);
      for (i = 0; used_metadata_names[i] != NULL; i++)
	g_hash_table_insert (hash,
			     used_metadata_names[i],
			     GINT_TO_POINTER (i + 1));
    }

  return GPOINTER_TO_INT (g_hash_table_lookup (hash, metadata));
}

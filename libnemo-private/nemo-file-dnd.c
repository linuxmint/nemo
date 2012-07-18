/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-file-drag.c - Drag & drop handling code that operated on 
   NemoFile objects.

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

   Authors: Pavel Cisler <pavel@eazel.com>,
*/

#include <config.h>
#include "nemo-file-dnd.h"
#include "nemo-desktop-icon-file.h"

#include "nemo-dnd.h"
#include "nemo-directory.h"
#include "nemo-file-utilities.h"
#include <string.h>

static gboolean
nemo_drag_can_accept_files (NemoFile *drop_target_item)
{
	NemoDirectory *directory;

	if (nemo_file_is_directory (drop_target_item)) {
		gboolean res;

		/* target is a directory, accept if editable */
		directory = nemo_directory_get_for_file (drop_target_item);
		res = nemo_directory_is_editable (directory);
		nemo_directory_unref (directory);
		return res;
	}
	
	if (NEMO_IS_DESKTOP_ICON_FILE (drop_target_item)) {
		return TRUE;
	}
	
	/* Launchers are an acceptable drop target */
	if (nemo_file_is_launcher (drop_target_item)) {
		return TRUE;
	}

	if (nemo_is_file_roller_installed () &&
	    nemo_file_is_archive (drop_target_item)) {
		return TRUE;
	}
	
	return FALSE;
}

gboolean
nemo_drag_can_accept_item (NemoFile *drop_target_item,
			       const char *item_uri)
{
	if (nemo_file_matches_uri (drop_target_item, item_uri)) {
		/* can't accept itself */
		return FALSE;
	}

	return nemo_drag_can_accept_files (drop_target_item);
}
				       
gboolean
nemo_drag_can_accept_items (NemoFile *drop_target_item,
				const GList *items)
{
	int max;

	if (drop_target_item == NULL)
		return FALSE;

	g_assert (NEMO_IS_FILE (drop_target_item));

	/* Iterate through selection checking if item will get accepted by the
	 * drop target. If more than 100 items selected, return an over-optimisic
	 * result
	 */
	for (max = 100; items != NULL && max >= 0; items = items->next, max--) {
		if (!nemo_drag_can_accept_item (drop_target_item, 
			((NemoDragSelectionItem *)items->data)->uri)) {
			return FALSE;
		}
	}
	
	return TRUE;
}

gboolean
nemo_drag_can_accept_info (NemoFile *drop_target_item,
			       NemoIconDndTargetType drag_type,
			       const GList *items)
{
	switch (drag_type) {
		case NEMO_ICON_DND_GNOME_ICON_LIST:
			return nemo_drag_can_accept_items (drop_target_item, items);

		case NEMO_ICON_DND_URI_LIST:
		case NEMO_ICON_DND_NETSCAPE_URL:
		case NEMO_ICON_DND_TEXT:
			return nemo_drag_can_accept_files (drop_target_item);

		case NEMO_ICON_DND_XDNDDIRECTSAVE:
		case NEMO_ICON_DND_RAW:
			return nemo_drag_can_accept_files (drop_target_item); /* Check if we can accept files at this location */

		case NEMO_ICON_DND_ROOTWINDOW_DROP:
			return FALSE;

		default:
			g_assert_not_reached ();
			return FALSE;
	}
}


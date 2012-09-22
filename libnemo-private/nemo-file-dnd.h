/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* nemo-file-drag.h - Drag & drop handling code that operated on 
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
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Pavel Cisler <pavel@eazel.com>,
*/

#ifndef NEMO_FILE_DND_H
#define NEMO_FILE_DND_H

#include <libnemo-private/nemo-dnd.h>
#include <libnemo-private/nemo-file.h>

#define NEMO_FILE_DND_ERASE_KEYWORD "erase"

gboolean nemo_drag_can_accept_item              (NemoFile *drop_target_item,
						     const char   *item_uri);
gboolean nemo_drag_can_accept_items             (NemoFile *drop_target_item,
						     const GList  *items);
gboolean nemo_drag_can_accept_info              (NemoFile *drop_target_item,
						     NemoIconDndTargetType drag_type,
						     const GList *items);

#endif /* NEMO_FILE_DND_H */


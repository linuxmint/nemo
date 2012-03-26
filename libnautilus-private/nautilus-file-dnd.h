/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* nautilus-file-drag.h - Drag & drop handling code that operated on 
   NautilusFile objects.

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

#ifndef NAUTILUS_FILE_DND_H
#define NAUTILUS_FILE_DND_H

#include <libnautilus-private/nautilus-dnd.h>
#include <libnautilus-private/nautilus-file.h>

#define NAUTILUS_FILE_DND_ERASE_KEYWORD "erase"

gboolean nautilus_drag_can_accept_item              (NautilusFile *drop_target_item,
						     const char   *item_uri);
gboolean nautilus_drag_can_accept_items             (NautilusFile *drop_target_item,
						     const GList  *items);
gboolean nautilus_drag_can_accept_info              (NautilusFile *drop_target_item,
						     NautilusIconDndTargetType drag_type,
						     const GList *items);

#endif /* NAUTILUS_FILE_DND_H */


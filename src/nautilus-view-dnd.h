/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nautilus-view-dnd.h: DnD helpers for NautilusView
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000, 2001  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Ettore Perazzoli
 * 	    Darin Adler <darin@bentspoon.com>
 * 	    John Sullivan <sullivan@eazel.com>
 *          Pavel Cisler <pavel@eazel.com>
 */

#ifndef __NAUTILUS_VIEW_DND_H__
#define __NAUTILUS_VIEW_DND_H__

#include "nautilus-view.h"

void nautilus_view_handle_netscape_url_drop (NautilusView  *view,
					     const char    *encoded_url,
					     const char    *target_uri,
					     GdkDragAction  action,
					     int            x,
					     int            y);
void nautilus_view_handle_uri_list_drop     (NautilusView  *view,
					     const char    *item_uris,
					     const char    *target_uri,
					     GdkDragAction  action,
					     int            x,
					     int            y);
void nautilus_view_handle_text_drop         (NautilusView  *view,
					     const char    *text,
					     const char    *target_uri,
					     GdkDragAction  action,
					     int            x,
					     int            y);
void nautilus_view_handle_raw_drop          (NautilusView  *view,
					     const char    *raw_data,
					     int            length,
					     const char    *target_uri,
					     const char    *direct_save_uri,
					     GdkDragAction  action,
					     int            x,
					     int            y);

void nautilus_view_drop_proxy_received_uris (NautilusView  *view,
					     const GList   *uris,
					     const char    *target_location,
					     GdkDragAction  action);


#endif /* __NAUTILUS_VIEW_DND_H__ */

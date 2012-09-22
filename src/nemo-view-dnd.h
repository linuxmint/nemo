/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-view-dnd.h: DnD helpers for NemoView
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
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Ettore Perazzoli
 * 	    Darin Adler <darin@bentspoon.com>
 * 	    John Sullivan <sullivan@eazel.com>
 *          Pavel Cisler <pavel@eazel.com>
 */

#ifndef __NEMO_VIEW_DND_H__
#define __NEMO_VIEW_DND_H__

#include "nemo-view.h"

void nemo_view_handle_netscape_url_drop (NemoView  *view,
					     const char    *encoded_url,
					     const char    *target_uri,
					     GdkDragAction  action,
					     int            x,
					     int            y);
void nemo_view_handle_uri_list_drop     (NemoView  *view,
					     const char    *item_uris,
					     const char    *target_uri,
					     GdkDragAction  action,
					     int            x,
					     int            y);
void nemo_view_handle_text_drop         (NemoView  *view,
					     const char    *text,
					     const char    *target_uri,
					     GdkDragAction  action,
					     int            x,
					     int            y);
void nemo_view_handle_raw_drop          (NemoView  *view,
					     const char    *raw_data,
					     int            length,
					     const char    *target_uri,
					     const char    *direct_save_uri,
					     GdkDragAction  action,
					     int            x,
					     int            y);

void nemo_view_drop_proxy_received_uris (NemoView  *view,
					     const GList   *uris,
					     const char    *target_location,
					     GdkDragAction  action);


#endif /* __NEMO_VIEW_DND_H__ */

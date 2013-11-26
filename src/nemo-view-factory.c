/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-view-factory.c: register and create NemoViews
 
   Copyright (C) 2004 Red Hat Inc.
  
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
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#include "config.h"

#include "nemo-view-factory.h"

static GList *registered_views;

void
nemo_view_factory_register (NemoViewInfo *view_info)
{
	g_return_if_fail (view_info != NULL);
	g_return_if_fail (view_info->id != NULL);
	g_return_if_fail (nemo_view_factory_lookup (view_info->id) == NULL);
	
	registered_views = g_list_append (registered_views, view_info);
}

const NemoViewInfo *
nemo_view_factory_lookup (const char *id)
{
	GList *l;
	NemoViewInfo *view_info;

	g_return_val_if_fail (id != NULL, NULL);

	
	for (l = registered_views; l != NULL; l = l->next) {
		view_info = l->data;
		
		if (strcmp (view_info->id, id) == 0) {
			return view_info;
		}
	}
	return NULL;
}

NemoView *
nemo_view_factory_create (const char *id,
			      NemoWindowSlot *slot)
{
	const NemoViewInfo *view_info;
	NemoView *view;

	view_info = nemo_view_factory_lookup (id);
	if (view_info == NULL) {
		return NULL;
	}

	view = view_info->create (slot);
	if (g_object_is_floating (view)) {
		g_object_ref_sink (view);
	}
	return view;
}

gboolean
nemo_view_factory_view_supports_uri (const char *id,
					 GFile *location,
					 GFileType file_type,
					 const char *mime_type)
{
	const NemoViewInfo *view_info;
	char *uri;
	gboolean res;

	view_info = nemo_view_factory_lookup (id);
	if (view_info == NULL) {
		return FALSE;
	}
	uri = g_file_get_uri (location);
	res = view_info->supports_uri (uri, file_type, mime_type);
	g_free (uri);
	return res;
	
}

GList *
nemo_view_factory_get_views_for_uri (const char *uri,
					 GFileType file_type,
					 const char *mime_type)
{
	GList *l, *res;
	const NemoViewInfo *view_info;

	res = NULL;
	
	for (l = registered_views; l != NULL; l = l->next) {
		view_info = l->data;

		if (view_info->supports_uri (uri, file_type, mime_type)) {
			res = g_list_prepend (res, g_strdup (view_info->id));
		}
	}
	
	return g_list_reverse (res);
}



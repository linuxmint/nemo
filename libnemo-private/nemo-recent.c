/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 James Willcox
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, 
 * Boston, MA 02110-1335, USA. 
 */

#include "config.h"
#include "nemo-recent.h"

#include <eel/eel-vfs-extensions.h>

#define DEFAULT_APP_EXEC "gnome-open %u"

static GtkRecentManager *
nemo_recent_get_manager (void)
{
	static GtkRecentManager *manager = NULL;

	if (manager == NULL) {
		manager = gtk_recent_manager_get_default ();
	}

	return manager;
}

void
nemo_recent_add_file (NemoFile *file,
			  GAppInfo *application)
{
	GtkRecentData recent_data;
	char *uri;

	uri = nemo_file_get_activation_uri (file);
	if (uri == NULL) {
		uri = nemo_file_get_uri (file);
	}

	/* do not add trash:// etc */
	if (eel_uri_is_trash (uri)  ||
	    eel_uri_is_search (uri) ||
	    eel_uri_is_recent (uri) ||
	    eel_uri_is_desktop (uri)) {
		g_free (uri);
		return;
	}

	recent_data.display_name = NULL;
	recent_data.description = NULL;

	recent_data.mime_type = nemo_file_get_mime_type (file);
	recent_data.app_name = g_strdup (g_get_application_name ());

	if (application != NULL)
		recent_data.app_exec = g_strdup (g_app_info_get_commandline (application));
	else
		recent_data.app_exec = g_strdup (DEFAULT_APP_EXEC);

	recent_data.groups = NULL;
	recent_data.is_private = FALSE;

	gtk_recent_manager_add_full (nemo_recent_get_manager (),
				     uri, &recent_data);

	g_free (recent_data.mime_type);
	g_free (recent_data.app_name);
	g_free (recent_data.app_exec);
	
	g_free (uri);
}

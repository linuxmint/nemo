/* -*- Mode: C; indent-tabs-mode: f; c-basic-offset: 4; tab-width: 4 -*- */

/* nemo-global-preferences.c - Nemo specific preference keys and
                                   functions.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "nemo-global-preferences.h"

#include "nemo-file-utilities.h"
#include "nemo-file.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <glib/gi18n.h>
#include <gio/gdesktopappinfo.h>


static gboolean ignore_view_metadata = FALSE;

/*
 * Public functions
 */
char *
nemo_global_preferences_get_default_folder_viewer_preference_as_iid (void)
{
	int preference_value;
	const char *viewer_iid;

	preference_value =
		g_settings_get_enum (nemo_preferences, NEMO_PREFERENCES_DEFAULT_FOLDER_VIEWER);

	if (preference_value == NEMO_DEFAULT_FOLDER_VIEWER_LIST_VIEW) {
		viewer_iid = NEMO_LIST_VIEW_IID;
	} else if (preference_value == NEMO_DEFAULT_FOLDER_VIEWER_COMPACT_VIEW) {
		viewer_iid = NEMO_COMPACT_VIEW_IID;
	} else {
		viewer_iid = NEMO_ICON_VIEW_IID;
	}

	return g_strdup (viewer_iid);
}

char *
nemo_global_preferences_get_desktop_iid (void)
{
    gboolean use_grid;
    const char *viewer_iid;

    use_grid = g_settings_get_boolean (nemo_desktop_preferences, NEMO_PREFERENCES_USE_DESKTOP_GRID);

    if (use_grid) {
        viewer_iid = NEMO_DESKTOP_ICON_GRID_VIEW_IID;
    } else {
        viewer_iid = NEMO_DESKTOP_ICON_VIEW_IID;
    }

    return g_strdup (viewer_iid);
}

gboolean
nemo_global_preferences_get_ignore_view_metadata (void)
{
    return ignore_view_metadata;
}

gint
nemo_global_preferences_get_tooltip_flags (void)
{
    NemoFileTooltipFlags flags = NEMO_FILE_TOOLTIP_FLAGS_NONE;

    if (g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_TOOLTIP_FILE_TYPE))
        flags |= NEMO_FILE_TOOLTIP_FLAGS_FILE_TYPE;
    if (g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_TOOLTIP_MOD_DATE))
        flags |= NEMO_FILE_TOOLTIP_FLAGS_MOD_DATE;
    if (g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_TOOLTIP_ACCESS_DATE))
        flags |= NEMO_FILE_TOOLTIP_FLAGS_ACCESS_DATE;
    if (g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_TOOLTIP_FULL_PATH))
        flags |= NEMO_FILE_TOOLTIP_FLAGS_PATH;
    if (g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_TOOLTIP_CREATED_DATE))
        flags |= NEMO_FILE_TOOLTIP_FLAGS_CREATED_DATE;
    return flags;
}

gboolean
nemo_global_preferences_should_load_plugin (const gchar *name, const gchar *key)
{
    gchar **disabled_list = g_settings_get_strv (nemo_plugin_preferences, key);

    gboolean ret = TRUE;
    guint i = 0;

    for (i = 0; i < g_strv_length (disabled_list); i++) {
        if (g_strcmp0 (disabled_list[i], name) == 0)
            ret = FALSE;
    }

    g_strfreev (disabled_list);
    return ret;
}

static void
ignore_view_metadata_cb (GSettings *settings,
                         gchar *key,
                         gpointer user_data)
{
    ignore_view_metadata = g_settings_get_boolean (settings, key);
}

static void
cache_fileroller_mimetypes (void)
{
    if (nemo_is_file_roller_installed ()) {
        GAppInfo *app_info;
        gchar ***results;
        gchar **result;
        gint i;

        results = g_desktop_app_info_search ("file-roller");

        if (results != NULL && results[0] != NULL) {
            const gchar *best;

            best = results[0][0];

            app_info = G_APP_INFO (g_desktop_app_info_new (best));

            if (app_info == NULL) {
                g_warning ("Unable to retrieve list of file-roller mimetypes");
                file_roller_mimetypes = NULL;
                return;
            }

            file_roller_mimetypes = g_strdupv ((gchar **) g_app_info_get_supported_types (app_info));

            g_object_unref (app_info);
        }

        i = 0;
        result = results[i];

        while (result != NULL) {
            g_strfreev (result);
            result = results[++i];
        }

        g_free (results);
    } else {
        file_roller_mimetypes = NULL;
    }
}

void
nemo_global_preferences_init (void)
{
	static gboolean initialized = FALSE;

	if (initialized) {
		return;
	}

	initialized = TRUE;

	nemo_preferences = g_settings_new("org.nemo.preferences");
	nemo_window_state = g_settings_new("org.nemo.window-state");
	nemo_icon_view_preferences = g_settings_new("org.nemo.icon-view");
	nemo_list_view_preferences = g_settings_new("org.nemo.list-view");
	nemo_compact_view_preferences = g_settings_new("org.nemo.compact-view");
	nemo_desktop_preferences = g_settings_new("org.nemo.desktop");
	nemo_tree_sidebar_preferences = g_settings_new("org.nemo.sidebar-panels.tree");
    nemo_plugin_preferences = g_settings_new("org.nemo.plugins");
	gnome_lockdown_preferences = g_settings_new("org.cinnamon.desktop.lockdown");
	gnome_background_preferences = g_settings_new("org.cinnamon.desktop.background");
	gnome_media_handling_preferences = g_settings_new("org.cinnamon.desktop.media-handling");
	gnome_terminal_preferences = g_settings_new("org.cinnamon.desktop.default-applications.terminal");
    cinnamon_privacy_preferences = g_settings_new("org.cinnamon.desktop.privacy");
	cinnamon_interface_preferences = g_settings_new ("org.cinnamon.desktop.interface");

    ignore_view_metadata = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_IGNORE_VIEW_METADATA);

    g_signal_connect (nemo_preferences,
                      "changed::" NEMO_PREFERENCES_IGNORE_VIEW_METADATA,
                      G_CALLBACK (ignore_view_metadata_cb), NULL);

    cache_fileroller_mimetypes ();
}

void
nemo_global_preferences_finalize (void)
{
    g_strfreev (file_roller_mimetypes);
}

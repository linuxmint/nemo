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
#include <eel/eel-debug.h>
#include <glib/gi18n.h>
#include <gio/gdesktopappinfo.h>

GSettings *nemo_preferences;
GSettings *nemo_icon_view_preferences;
GSettings *nemo_list_view_preferences;
GSettings *nemo_compact_view_preferences;
GSettings *nemo_desktop_preferences;
GSettings *nemo_tree_sidebar_preferences;
GSettings *nemo_window_state;
GSettings *nemo_plugin_preferences;
GSettings *nemo_menu_config_preferences;
GSettings *nemo_search_preferences;
GSettings *gnome_lockdown_preferences;
GSettings *gnome_background_preferences;
GSettings *gnome_media_handling_preferences;
GSettings *gnome_terminal_preferences;
GSettings *cinnamon_privacy_preferences;
GSettings *cinnamon_interface_preferences;

GTimeZone      *prefs_current_timezone;
gboolean        prefs_current_24h_time_format;
NemoDateFormat  prefs_current_date_format;

GTimer    *nemo_startup_timer;

static gboolean ignore_view_metadata = FALSE;
static gboolean inherit_folder_view_preference = FALSE;
static gboolean inherit_show_thumbnails_preference = FALSE;
static int      size_prefixes_preference = 0;

static gchar **file_roller_mimetypes = NULL;


GFileMonitor *tz_mon;

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

gboolean
nemo_global_preferences_get_inherit_folder_viewer_preference (void)
{
    return inherit_folder_view_preference;
}

gboolean
nemo_global_preferences_get_ignore_view_metadata (void)
{
    return ignore_view_metadata;
}

gboolean
nemo_global_preferences_get_inherit_show_thumbnails_preference (void)
{
    return inherit_show_thumbnails_preference;
}

int
nemo_global_preferences_get_size_prefix_preference (void)
{
    switch (size_prefixes_preference) {
        case 0: // base-10
            return G_FORMAT_SIZE_DEFAULT;
        case 1: // base-10 full
            return G_FORMAT_SIZE_DEFAULT |                           G_FORMAT_SIZE_LONG_FORMAT;
        case 2: // base-2
            return G_FORMAT_SIZE_DEFAULT | G_FORMAT_SIZE_IEC_UNITS;
        case 3: // base-2 full
            return G_FORMAT_SIZE_DEFAULT | G_FORMAT_SIZE_IEC_UNITS | G_FORMAT_SIZE_LONG_FORMAT;
    }

    return 0;
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
boolean_changed_cb (GSettings *settings,
                    gchar     *key,
                    gboolean  *user_data)
{
    *user_data = g_settings_get_boolean (settings, key);
}

static void
enum_changed_cb (GSettings *settings,
                 gchar     *key,
                 int       *user_data)
{
    *user_data = g_settings_get_enum (settings, key);
}

static void
setup_cached_pref_keys (void)
{
    g_signal_connect (nemo_preferences,
                      "changed::" NEMO_PREFERENCES_IGNORE_VIEW_METADATA,
                      G_CALLBACK (boolean_changed_cb), &ignore_view_metadata);

    boolean_changed_cb (nemo_preferences, NEMO_PREFERENCES_IGNORE_VIEW_METADATA, &ignore_view_metadata);

    g_signal_connect (nemo_preferences,
                      "changed::" NEMO_PREFERENCES_INHERIT_FOLDER_VIEWER,
                      G_CALLBACK (boolean_changed_cb), &inherit_folder_view_preference);

    boolean_changed_cb (nemo_preferences, NEMO_PREFERENCES_INHERIT_FOLDER_VIEWER, &inherit_folder_view_preference);

    g_signal_connect (nemo_preferences,
                      "changed::" NEMO_PREFERENCES_INHERIT_SHOW_THUMBNAILS,
                      G_CALLBACK (boolean_changed_cb), &inherit_show_thumbnails_preference);

    boolean_changed_cb (nemo_preferences, NEMO_PREFERENCES_INHERIT_SHOW_THUMBNAILS, &inherit_show_thumbnails_preference);

    g_signal_connect (nemo_preferences,
                      "changed::" NEMO_PREFERENCES_SIZE_PREFIXES,
                      G_CALLBACK (enum_changed_cb), &size_prefixes_preference);

    enum_changed_cb (nemo_preferences, NEMO_PREFERENCES_SIZE_PREFIXES, &size_prefixes_preference);
}


gchar **
nemo_global_preferences_get_fileroller_mimetypes (void)
{
    static gsize once_init = 0;

    if (g_once_init_enter (&once_init)) {
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

                if (app_info) {
                    file_roller_mimetypes = g_strdupv ((gchar **) g_app_info_get_supported_types (app_info));
                    g_object_unref (app_info);
                }

                if (app_info == NULL) {
                    g_warning ("Unable to retrieve list of file-roller mimetypes");
                }

                i = 0;
                result = results[i];

                while (result != NULL) {
                    g_strfreev (result);
                    result = results[++i];
                }

                g_free (results);
            }
        }

        g_once_init_leave (&once_init, 1);
    }

    return file_roller_mimetypes;
}

static void
on_time_data_changed (gpointer user_data)
{
    prefs_current_date_format = g_settings_get_enum (nemo_preferences, NEMO_PREFERENCES_DATE_FORMAT);
    prefs_current_24h_time_format = g_settings_get_boolean (cinnamon_interface_preferences, "clock-use-24h");

    if (prefs_current_timezone != NULL) {
        g_time_zone_unref (prefs_current_timezone);
    }

    prefs_current_timezone = g_time_zone_new_local ();
}

static void
setup_cached_time_data (void)
{
    GFile *tz;

    prefs_current_timezone = NULL;

    g_signal_connect_swapped (nemo_preferences,
                              "changed::" NEMO_PREFERENCES_DATE_FORMAT,
                              G_CALLBACK (on_time_data_changed), NULL);

    g_signal_connect_swapped (cinnamon_interface_preferences,
                              "changed::clock-use-24h",
                              G_CALLBACK (on_time_data_changed), NULL);


    tz = g_file_new_for_path ("/etc/localtime");

    tz_mon = g_file_monitor_file (tz, 0, NULL, NULL);
    g_object_unref (tz);

    g_signal_connect_swapped (tz_mon,
                              "changed",
                              G_CALLBACK (on_time_data_changed), NULL);

    on_time_data_changed (NULL);
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
    nemo_menu_config_preferences = g_settings_new("org.nemo.preferences.menu-config");
    nemo_search_preferences = g_settings_new("org.nemo.search");
	gnome_lockdown_preferences = g_settings_new("org.cinnamon.desktop.lockdown");
	gnome_background_preferences = g_settings_new("org.cinnamon.desktop.background");
	gnome_media_handling_preferences = g_settings_new("org.cinnamon.desktop.media-handling");
	gnome_terminal_preferences = g_settings_new("org.cinnamon.desktop.default-applications.terminal");
    cinnamon_privacy_preferences = g_settings_new("org.cinnamon.desktop.privacy");
	cinnamon_interface_preferences = g_settings_new ("org.cinnamon.desktop.interface");

    setup_cached_pref_keys ();
    setup_cached_time_data ();

    eel_debug_call_at_shutdown (nemo_global_preferences_finalize);
}

void
nemo_global_preferences_finalize (void)
{
    g_strfreev (file_roller_mimetypes);
    g_object_unref (tz_mon);

    g_object_unref (nemo_preferences);
    g_object_unref (nemo_window_state);
    g_object_unref (nemo_icon_view_preferences);
    g_object_unref (nemo_list_view_preferences);
    g_object_unref (nemo_compact_view_preferences);
    g_object_unref (nemo_desktop_preferences);
    g_object_unref (nemo_tree_sidebar_preferences);
    g_object_unref (nemo_plugin_preferences);
    g_object_unref (nemo_menu_config_preferences);
    g_object_unref (nemo_search_preferences);
    g_object_unref (gnome_lockdown_preferences);
    g_object_unref (gnome_background_preferences);
    g_object_unref (gnome_media_handling_preferences);
    g_object_unref (gnome_terminal_preferences);
    g_object_unref (cinnamon_privacy_preferences);
    g_object_unref (cinnamon_interface_preferences);
}

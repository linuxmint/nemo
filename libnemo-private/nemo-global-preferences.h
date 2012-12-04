/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-global-preferences.h - Nemo specific preference keys and
                                   functions.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NEMO_GLOBAL_PREFERENCES_H
#define NEMO_GLOBAL_PREFERENCES_H

#include <libnemo-private/nemo-global-preferences.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/* Trash options */
#define NEMO_PREFERENCES_CONFIRM_TRASH			"confirm-trash"
#define NEMO_PREFERENCES_ENABLE_DELETE			"enable-delete"

/* Desktop options */
#define NEMO_PREFERENCES_DESKTOP_IS_HOME_DIR                "desktop-is-home-dir"

/* Display  */
#define NEMO_PREFERENCES_SHOW_HIDDEN_FILES			"show-hidden-files"
#define NEMO_PREFERENCES_SHOW_ADVANCED_PERMISSIONS		"show-advanced-permissions"
#define NEMO_PREFERENCES_DATE_FORMAT			"date-format"

/* Mouse */
#define NEMO_PREFERENCES_MOUSE_USE_EXTRA_BUTTONS		"mouse-use-extra-buttons"
#define NEMO_PREFERENCES_MOUSE_FORWARD_BUTTON		"mouse-forward-button"
#define NEMO_PREFERENCES_MOUSE_BACK_BUTTON			"mouse-back-button"

typedef enum
{
	NEMO_DATE_FORMAT_LOCALE,
	NEMO_DATE_FORMAT_ISO,
	NEMO_DATE_FORMAT_INFORMAL
} NemoDateFormat;

typedef enum
{
	NEMO_NEW_TAB_POSITION_AFTER_CURRENT_TAB,
	NEMO_NEW_TAB_POSITION_END,
} NemoNewTabPosition;

/* Sidebar panels  */
#define NEMO_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES         "show-only-directories"

/* Single/Double click preference  */
#define NEMO_PREFERENCES_CLICK_POLICY			"click-policy"

/* Activating executable text files */
#define NEMO_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION		"executable-text-activation"

/* Installing new packages when unknown mime type activated */
#define NEMO_PREFERENCES_INSTALL_MIME_ACTIVATION		"install-mime-activation"

/* Spatial or browser mode */
#define NEMO_PREFERENCES_ALWAYS_USE_BROWSER			"always-use-browser"
#define NEMO_PREFERENCES_NEW_TAB_POSITION			"tabs-open-position"

#define NEMO_PREFERENCES_SHOW_LOCATION_ENTRY		"show-location-entry"
#define NEMO_PREFERENCES_SHOW_UP_ICON_TOOLBAR		"show-up-icon-toolbar"
#define NEMO_PREFERENCES_SHOW_EDIT_ICON_TOOLBAR		"show-edit-icon-toolbar"
#define NEMO_PREFERENCES_SHOW_RELOAD_ICON_TOOLBAR		"show-reload-icon-toolbar"
#define NEMO_PREFERENCES_SHOW_HOME_ICON_TOOLBAR		"show-home-icon-toolbar"
#define NEMO_PREFERENCES_SHOW_COMPUTER_ICON_TOOLBAR		"show-computer-icon-toolbar"
#define NEMO_PREFERENCES_SHOW_SEARCH_ICON_TOOLBAR		"show-search-icon-toolbar"
#define NEMO_PREFERENCES_SHOW_LABEL_SEARCH_ICON_TOOLBAR	"show-label-search-icon-toolbar"

/* Which views should be displayed for new windows */
#define NEMO_WINDOW_STATE_START_WITH_STATUS_BAR		"start-with-status-bar"
#define NEMO_WINDOW_STATE_START_WITH_SIDEBAR		"start-with-sidebar"
#define NEMO_WINDOW_STATE_START_WITH_TOOLBAR		"start-with-toolbar"
#define NEMO_WINDOW_STATE_START_WITH_MENU_BAR           "start-with-menu-bar"
#define NEMO_WINDOW_STATE_SIDE_PANE_VIEW                    "side-pane-view"
#define NEMO_WINDOW_STATE_GEOMETRY				"geometry"
#define NEMO_WINDOW_STATE_MAXIMIZED				"maximized"
#define NEMO_WINDOW_STATE_SIDEBAR_WIDTH			"sidebar-width"
#define NEMO_WINDOW_STATE_MY_COMPUTER_EXPANDED  "my-computer-expanded"
#define NEMO_WINDOW_STATE_DEVICES_EXPANDED      "devices-expanded"
#define NEMO_WINDOW_STATE_NETWORK_EXPANDED      "network-expanded"

/* Sorting order */
#define NEMO_PREFERENCES_SORT_DIRECTORIES_FIRST		"sort-directories-first"
#define NEMO_PREFERENCES_DEFAULT_SORT_ORDER			"default-sort-order"
#define NEMO_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER	"default-sort-in-reverse-order"

/* The default folder viewer - one of the two enums below */
#define NEMO_PREFERENCES_DEFAULT_FOLDER_VIEWER		"default-folder-viewer"

#define NEMO_PREFERENCES_SHOW_FULL_PATH_TITLES      "show-full-path-titles"


enum
{
	NEMO_DEFAULT_FOLDER_VIEWER_ICON_VIEW,
	NEMO_DEFAULT_FOLDER_VIEWER_COMPACT_VIEW,
	NEMO_DEFAULT_FOLDER_VIEWER_LIST_VIEW,
	NEMO_DEFAULT_FOLDER_VIEWER_OTHER
};

/* These IIDs are used by the preferences code and in nemo-application.c */
#define NEMO_ICON_VIEW_IID		"OAFIID:Nemo_File_Manager_Icon_View"
#define NEMO_COMPACT_VIEW_IID	"OAFIID:Nemo_File_Manager_Compact_View"
#define NEMO_LIST_VIEW_IID		"OAFIID:Nemo_File_Manager_List_View"


/* Icon View */
#define NEMO_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL		"default-zoom-level"
#define NEMO_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT   "default-use-tighter-layout"
#define NEMO_PREFERENCES_ICON_VIEW_LABELS_BESIDE_ICONS		"labels-beside-icons"

/* Which text attributes appear beneath icon names */
#define NEMO_PREFERENCES_ICON_VIEW_CAPTIONS				"captions"

/* The default size for thumbnail icons */
#define NEMO_PREFERENCES_ICON_VIEW_THUMBNAIL_SIZE			"thumbnail-size"

/* ellipsization preferences */
#define NEMO_PREFERENCES_ICON_VIEW_TEXT_ELLIPSIS_LIMIT		"text-ellipsis-limit"
#define NEMO_PREFERENCES_DESKTOP_TEXT_ELLIPSIS_LIMIT		"text-ellipsis-limit"

/* Compact View */
#define NEMO_PREFERENCES_COMPACT_VIEW_DEFAULT_ZOOM_LEVEL		"default-zoom-level"
#define NEMO_PREFERENCES_COMPACT_VIEW_ALL_COLUMNS_SAME_WIDTH	"all-columns-have-same-width"

/* List View */
#define NEMO_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL		"default-zoom-level"
#define NEMO_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS		"default-visible-columns"
#define NEMO_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER		"default-column-order"

enum
{
	NEMO_CLICK_POLICY_SINGLE,
	NEMO_CLICK_POLICY_DOUBLE
};

enum
{
	NEMO_EXECUTABLE_TEXT_LAUNCH,
	NEMO_EXECUTABLE_TEXT_DISPLAY,
	NEMO_EXECUTABLE_TEXT_ASK
};

typedef enum
{
	NEMO_SPEED_TRADEOFF_ALWAYS,
	NEMO_SPEED_TRADEOFF_LOCAL_ONLY,
	NEMO_SPEED_TRADEOFF_NEVER
} NemoSpeedTradeoffValue;

#define NEMO_PREFERENCES_SHOW_TEXT_IN_ICONS		"show-icon-text"
#define NEMO_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS "show-directory-item-counts"
#define NEMO_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS	"show-image-thumbnails"
#define NEMO_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT	"thumbnail-limit"

typedef enum
{
	NEMO_COMPLEX_SEARCH_BAR,
	NEMO_SIMPLE_SEARCH_BAR
} NemoSearchBarMode;

#define NEMO_PREFERENCES_DESKTOP_FONT		   "font"
#define NEMO_PREFERENCES_DESKTOP_HOME_VISIBLE          "home-icon-visible"
#define NEMO_PREFERENCES_DESKTOP_HOME_NAME             "home-icon-name"
#define NEMO_PREFERENCES_DESKTOP_COMPUTER_VISIBLE      "computer-icon-visible"
#define NEMO_PREFERENCES_DESKTOP_COMPUTER_NAME         "computer-icon-name"
#define NEMO_PREFERENCES_DESKTOP_TRASH_VISIBLE         "trash-icon-visible"
#define NEMO_PREFERENCES_DESKTOP_TRASH_NAME            "trash-icon-name"
#define NEMO_PREFERENCES_DESKTOP_VOLUMES_VISIBLE	   "volumes-visible"
#define NEMO_PREFERENCES_DESKTOP_NETWORK_VISIBLE       "network-icon-visible"
#define NEMO_PREFERENCES_DESKTOP_NETWORK_NAME          "network-icon-name"
#define NEMO_PREFERENCES_DESKTOP_BACKGROUND_FADE       "background-fade"

/* bulk rename utility */
#define NEMO_PREFERENCES_BULK_RENAME_TOOL              "bulk-rename-tool"

/* Lockdown */
#define NEMO_PREFERENCES_LOCKDOWN_COMMAND_LINE         "disable-command-line"

/* Desktop background */
#define NEMO_PREFERENCES_SHOW_DESKTOP		   "show-desktop-icons"

/* File size unit prefix */
#define NEMO_PREFERENCES_SIZE_PREFIXES			"size-prefixes"


void nemo_global_preferences_init                      (void);
char *nemo_global_preferences_get_default_folder_viewer_preference_as_iid (void);

GSettings *nemo_preferences;
GSettings *nemo_icon_view_preferences;
GSettings *nemo_list_view_preferences;
GSettings *nemo_compact_view_preferences;
GSettings *nemo_desktop_preferences;
GSettings *nemo_tree_sidebar_preferences;
GSettings *nemo_window_state;
GSettings *gnome_lockdown_preferences;
GSettings *gnome_background_preferences;

G_END_DECLS

#endif /* NEMO_GLOBAL_PREFERENCES_H */

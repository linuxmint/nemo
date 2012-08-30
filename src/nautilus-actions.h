/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef NAUTILUS_ACTIONS_H
#define NAUTILUS_ACTIONS_H

#define NAUTILUS_ACTION_STOP "Stop"
#define NAUTILUS_ACTION_RELOAD "Reload"
#define NAUTILUS_ACTION_BACK "Back"
#define NAUTILUS_ACTION_UP "Up"
#define NAUTILUS_ACTION_FORWARD "Forward"
#define NAUTILUS_ACTION_SHOW_HIDE_SIDEBAR "Show Hide Sidebar"
#define NAUTILUS_ACTION_SHOW_HIDE_LOCATION_BAR "Show Hide Location Bar"
#define NAUTILUS_ACTION_GO_TO_BURN_CD "Go to Burn CD"
#define NAUTILUS_ACTION_ENTER_LOCATION "Enter Location"
#define NAUTILUS_ACTION_GO_HOME "Home"
#define NAUTILUS_ACTION_ADD_BOOKMARK "Add Bookmark"
#define NAUTILUS_ACTION_EDIT_BOOKMARKS "Edit Bookmarks"
#define NAUTILUS_ACTION_HOME "Home"
#define NAUTILUS_ACTION_ZOOM_IN "Zoom In"
#define NAUTILUS_ACTION_ZOOM_OUT "Zoom Out"
#define NAUTILUS_ACTION_ZOOM_NORMAL "Zoom Normal"
#define NAUTILUS_ACTION_SHOW_HIDDEN_FILES "Show Hidden Files"
#define NAUTILUS_ACTION_CLOSE "Close"
#define NAUTILUS_ACTION_CLOSE_ALL_WINDOWS "Close All Windows"
#define NAUTILUS_ACTION_SEARCH "Search"
#define NAUTILUS_ACTION_VIEW_LIST "View List"
#define NAUTILUS_ACTION_VIEW_GRID "View Grid"
#define NAUTILUS_ACTION_FOLDER_WINDOW "Folder Window"
#define NAUTILUS_ACTION_NEW_TAB "New Tab"
#define NAUTILUS_ACTION_NEW_WINDOW "New Window"
#define NAUTILUS_ACTION_PREFERENCES "Preferences"
#define NAUTILUS_ACTION_ABOUT "About Nautilus"
#define NAUTILUS_ACTION_HELP "NautilusHelp"

#define NAUTILUS_ACTION_OPEN "Open"
#define NAUTILUS_ACTION_OPEN_WITH "Open With"
#define NAUTILUS_ACTION_OPEN_ALTERNATE "OpenAlternate"
#define NAUTILUS_ACTION_OPEN_IN_NEW_TAB "OpenInNewTab"
#define NAUTILUS_ACTION_LOCATION_OPEN_ALTERNATE "LocationOpenAlternate"
#define NAUTILUS_ACTION_LOCATION_OPEN_IN_NEW_TAB "LocationOpenInNewTab"
#define NAUTILUS_ACTION_OTHER_APPLICATION1 "OtherApplication1"
#define NAUTILUS_ACTION_OTHER_APPLICATION2 "OtherApplication2"
#define NAUTILUS_ACTION_NEW_FOLDER "New Folder"
#define NAUTILUS_ACTION_NEW_FOLDER_WITH_SELECTION "New Folder with Selection"
#define NAUTILUS_ACTION_PROPERTIES "Properties"
#define NAUTILUS_ACTION_PROPERTIES_ACCEL "PropertiesAccel"
#define NAUTILUS_ACTION_LOCATION_PROPERTIES "LocationProperties"
#define NAUTILUS_ACTION_EMPTY_TRASH "Empty Trash"
#define NAUTILUS_ACTION_SAVE_SEARCH "Save Search"
#define NAUTILUS_ACTION_SAVE_SEARCH_AS "Save Search As"
#define NAUTILUS_ACTION_CUT "Cut"
#define NAUTILUS_ACTION_LOCATION_CUT "LocationCut"
#define NAUTILUS_ACTION_COPY "Copy"
#define NAUTILUS_ACTION_LOCATION_COPY "LocationCopy"
#define NAUTILUS_ACTION_PASTE "Paste"
#define NAUTILUS_ACTION_PASTE_FILES_INTO "Paste Files Into"
#define NAUTILUS_ACTION_LOCATION_PASTE_FILES_INTO "LocationPasteFilesInto"
#define NAUTILUS_ACTION_MOVE_TO "Move To"
#define NAUTILUS_ACTION_COPY_TO "Copy To"
#define NAUTILUS_ACTION_RENAME "Rename"
#define NAUTILUS_ACTION_DUPLICATE "Duplicate"
#define NAUTILUS_ACTION_CREATE_LINK "Create Link"
#define NAUTILUS_ACTION_SET_AS_WALLPAPER "Set As Wallpaper"
#define NAUTILUS_ACTION_SELECT_ALL "Select All"
#define NAUTILUS_ACTION_INVERT_SELECTION "Invert Selection"
#define NAUTILUS_ACTION_SELECT_PATTERN "Select Pattern"
#define NAUTILUS_ACTION_TRASH "Trash"
#define NAUTILUS_ACTION_LOCATION_TRASH "LocationTrash"
#define NAUTILUS_ACTION_DELETE "Delete"
#define NAUTILUS_ACTION_LOCATION_DELETE "LocationDelete"
#define NAUTILUS_ACTION_RESTORE_FROM_TRASH "Restore From Trash"
#define NAUTILUS_ACTION_LOCATION_RESTORE_FROM_TRASH "LocationRestoreFromTrash"
#define NAUTILUS_ACTION_UNDO "Undo"
#define NAUTILUS_ACTION_REDO "Redo"
#define NAUTILUS_ACTION_RESET_TO_DEFAULTS "Reset to Defaults"
#define NAUTILUS_ACTION_CONNECT_TO_SERVER "Connect to Server"
#define NAUTILUS_ACTION_MOUNT_VOLUME "Mount Volume"
#define NAUTILUS_ACTION_UNMOUNT_VOLUME "Unmount Volume"
#define NAUTILUS_ACTION_EJECT_VOLUME "Eject Volume"
#define NAUTILUS_ACTION_START_VOLUME "Start Volume"
#define NAUTILUS_ACTION_STOP_VOLUME "Stop Volume"
#define NAUTILUS_ACTION_POLL "Poll"
#define NAUTILUS_ACTION_SELF_MOUNT_VOLUME "Self Mount Volume"
#define NAUTILUS_ACTION_SELF_UNMOUNT_VOLUME "Self Unmount Volume"
#define NAUTILUS_ACTION_SELF_EJECT_VOLUME "Self Eject Volume"
#define NAUTILUS_ACTION_SELF_START_VOLUME "Self Start Volume"
#define NAUTILUS_ACTION_SELF_STOP_VOLUME "Self Stop Volume"
#define NAUTILUS_ACTION_SELF_POLL "Self Poll"
#define NAUTILUS_ACTION_LOCATION_MOUNT_VOLUME "Location Mount Volume"
#define NAUTILUS_ACTION_LOCATION_UNMOUNT_VOLUME "Location Unmount Volume"
#define NAUTILUS_ACTION_LOCATION_EJECT_VOLUME "Location Eject Volume"
#define NAUTILUS_ACTION_LOCATION_START_VOLUME "Location Start Volume"
#define NAUTILUS_ACTION_LOCATION_STOP_VOLUME "Location Stop Volume"
#define NAUTILUS_ACTION_LOCATION_POLL "Location Poll"
#define NAUTILUS_ACTION_SCRIPTS "Scripts"
#define NAUTILUS_ACTION_OPEN_SCRIPTS_FOLDER "Open Scripts Folder"
#define NAUTILUS_ACTION_NEW_DOCUMENTS "New Documents"
#define NAUTILUS_ACTION_NEW_EMPTY_DOCUMENT "New Empty Document"
#define NAUTILUS_ACTION_EMPTY_TRASH_CONDITIONAL "Empty Trash Conditional"
#define NAUTILUS_ACTION_MANUAL_LAYOUT "Manual Layout"
#define NAUTILUS_ACTION_REVERSED_ORDER "Reversed Order"
#define NAUTILUS_ACTION_CLEAN_UP "Clean Up"
#define NAUTILUS_ACTION_KEEP_ALIGNED "Keep Aligned"
#define NAUTILUS_ACTION_ARRANGE_ITEMS "Arrange Items"
#define NAUTILUS_ACTION_STRETCH "Stretch"
#define NAUTILUS_ACTION_UNSTRETCH "Unstretch"
#define NAUTILUS_ACTION_ZOOM_ITEMS "Zoom Items"
#define NAUTILUS_ACTION_SORT_TRASH_TIME "Sort by Trash Time"

#endif /* NAUTILUS_ACTIONS_H */

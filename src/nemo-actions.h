/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nemo
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *
 *  Nemo is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Nemo is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef NEMO_ACTIONS_H
#define NEMO_ACTIONS_H

#define NEMO_ACTION_STOP "Stop"
#define NEMO_ACTION_RELOAD "Reload"
#define NEMO_ACTION_BACK "Back"
#define NEMO_ACTION_COMPUTER "Computer"
#define NEMO_ACTION_UP "Up"
#define NEMO_ACTION_UP_ACCEL "UpAccel"
#define NEMO_ACTION_UP_ACCEL "UpAccel"
#define NEMO_ACTION_FORWARD "Forward"
#define NEMO_ACTION_SHOW_HIDE_TOOLBAR "Show Hide Toolbar"
#define NEMO_ACTION_SHOW_HIDE_SIDEBAR "Show Hide Sidebar"
#define NEMO_ACTION_SHOW_HIDE_STATUSBAR "Show Hide Statusbar"
#define NEMO_ACTION_SHOW_HIDE_LOCATION_BAR "Show Hide Location Bar"
#define NEMO_ACTION_SHOW_HIDE_EXTRA_PANE "Show Hide Extra Pane"
#define NEMO_ACTION_SHOW_HIDE_LOCATION_ENTRY "Show Hide Location Entry"
#define NEMO_ACTION_GO_TO_BURN_CD "Go to Burn CD"
#define NEMO_ACTION_EDIT_LOCATION "Edit Location"
#define NEMO_ACTION_GO_HOME "Home"
#define NEMO_ACTION_ADD_BOOKMARK "Add Bookmark"
#define NEMO_ACTION_EDIT_BOOKMARKS "Edit Bookmarks"
#define NEMO_ACTION_HOME "Home"
#define NEMO_ACTION_ZOOM_IN "Zoom In"
#define NEMO_ACTION_ZOOM_OUT "Zoom Out"
#define NEMO_ACTION_ZOOM_NORMAL "Zoom Normal"
#define NEMO_ACTION_SHOW_HIDDEN_FILES "Show Hidden Files"
#define NEMO_ACTION_CLOSE "Close"
#define NEMO_ACTION_SEARCH "Search"
#define NEMO_ACTION_FOLDER_WINDOW "Folder Window"
#define NEMO_ACTION_NEW_TAB "New Tab"

#define NEMO_ACTION_OPEN "Open"
#define NEMO_ACTION_OPEN_ALTERNATE "OpenAlternate"
#define NEMO_ACTION_OPEN_IN_NEW_TAB "OpenInNewTab"
#define NEMO_ACTION_LOCATION_OPEN_ALTERNATE "LocationOpenAlternate"
#define NEMO_ACTION_LOCATION_OPEN_IN_NEW_TAB "LocationOpenInNewTab"
#define NEMO_ACTION_OTHER_APPLICATION1 "OtherApplication1"
#define NEMO_ACTION_OTHER_APPLICATION2 "OtherApplication2"
#define NEMO_ACTION_NEW_FOLDER "New Folder"
#define NEMO_ACTION_PROPERTIES "Properties"
#define NEMO_ACTION_PROPERTIES_ACCEL "PropertiesAccel"
#define NEMO_ACTION_LOCATION_PROPERTIES "LocationProperties"
#define NEMO_ACTION_NO_TEMPLATES "No Templates"
#define NEMO_ACTION_EMPTY_TRASH "Empty Trash"
#define NEMO_ACTION_SAVE_SEARCH "Save Search"
#define NEMO_ACTION_SAVE_SEARCH_AS "Save Search As"
#define NEMO_ACTION_CUT "Cut"
#define NEMO_ACTION_LOCATION_CUT "LocationCut"
#define NEMO_ACTION_COPY "Copy"
#define NEMO_ACTION_LOCATION_COPY "LocationCopy"
#define NEMO_ACTION_PASTE "Paste"
#define NEMO_ACTION_PASTE_FILES_INTO "Paste Files Into"
#define NEMO_ACTION_COPY_TO_NEXT_PANE "Copy to next pane"
#define NEMO_ACTION_MOVE_TO_NEXT_PANE "Move to next pane"
#define NEMO_ACTION_COPY_TO_HOME "Copy to Home"
#define NEMO_ACTION_MOVE_TO_HOME "Move to Home"
#define NEMO_ACTION_COPY_TO_DESKTOP "Copy to Desktop"
#define NEMO_ACTION_MOVE_TO_DESKTOP "Move to Desktop"
#define NEMO_ACTION_LOCATION_PASTE_FILES_INTO "LocationPasteFilesInto"
#define NEMO_ACTION_RENAME "Rename"
#define NEMO_ACTION_DUPLICATE "Duplicate"
#define NEMO_ACTION_CREATE_LINK "Create Link"
#define NEMO_ACTION_SELECT_ALL "Select All"
#define NEMO_ACTION_INVERT_SELECTION "Invert Selection"
#define NEMO_ACTION_SELECT_PATTERN "Select Pattern"
#define NEMO_ACTION_TRASH "Trash"
#define NEMO_ACTION_LOCATION_TRASH "LocationTrash"
#define NEMO_ACTION_DELETE "Delete"
#define NEMO_ACTION_LOCATION_DELETE "LocationDelete"
#define NEMO_ACTION_RESTORE_FROM_TRASH "Restore From Trash"
#define NEMO_ACTION_LOCATION_RESTORE_FROM_TRASH "LocationRestoreFromTrash"
#define NEMO_ACTION_SHOW_HIDDEN_FILES "Show Hidden Files"
#define NEMO_ACTION_CONNECT_TO_SERVER_LINK "Connect To Server Link"
#define NEMO_ACTION_MOUNT_VOLUME "Mount Volume"
#define NEMO_ACTION_UNMOUNT_VOLUME "Unmount Volume"
#define NEMO_ACTION_EJECT_VOLUME "Eject Volume"
#define NEMO_ACTION_START_VOLUME "Start Volume"
#define NEMO_ACTION_STOP_VOLUME "Stop Volume"
#define NEMO_ACTION_POLL "Poll"
#define NEMO_ACTION_SELF_MOUNT_VOLUME "Self Mount Volume"
#define NEMO_ACTION_SELF_UNMOUNT_VOLUME "Self Unmount Volume"
#define NEMO_ACTION_SELF_EJECT_VOLUME "Self Eject Volume"
#define NEMO_ACTION_SELF_START_VOLUME "Self Start Volume"
#define NEMO_ACTION_SELF_STOP_VOLUME "Self Stop Volume"
#define NEMO_ACTION_SELF_POLL "Self Poll"
#define NEMO_ACTION_LOCATION_MOUNT_VOLUME "Location Mount Volume"
#define NEMO_ACTION_LOCATION_UNMOUNT_VOLUME "Location Unmount Volume"
#define NEMO_ACTION_LOCATION_EJECT_VOLUME "Location Eject Volume"
#define NEMO_ACTION_LOCATION_START_VOLUME "Location Start Volume"
#define NEMO_ACTION_LOCATION_STOP_VOLUME "Location Stop Volume"
#define NEMO_ACTION_LOCATION_POLL "Location Poll"
#define NEMO_ACTION_SCRIPTS "Scripts"
#define NEMO_ACTION_NEW_LAUNCHER "New Launcher"
#define NEMO_ACTION_NEW_LAUNCHER_DESKTOP "New Launcher"
#define NEMO_ACTION_NEW_DOCUMENTS "New Documents"
#define NEMO_ACTION_NEW_EMPTY_DOCUMENT "New Empty Document"
#define NEMO_ACTION_EMPTY_TRASH_CONDITIONAL "Empty Trash Conditional"
#define NEMO_ACTION_MANUAL_LAYOUT "Manual Layout"
#define NEMO_ACTION_REVERSED_ORDER "Reversed Order"
#define NEMO_ACTION_CLEAN_UP "Clean Up"
#define NEMO_ACTION_KEEP_ALIGNED "Keep Aligned"
#define NEMO_ACTION_ARRANGE_ITEMS "Arrange Items"
#define NEMO_ACTION_STRETCH "Stretch"
#define NEMO_ACTION_UNSTRETCH "Unstretch"
#define NEMO_ACTION_ZOOM_ITEMS "Zoom Items"
#define NEMO_ACTION_SORT_TRASH_TIME "Sort by Trash Time"
#define NEMO_ACTION_MAILTO_THUNDERBIRD "MailToThunderbird"
#define NEMO_ACTION_MAILTO_OTHER "MailToOther"

#endif /* NEMO_ACTIONS_H */

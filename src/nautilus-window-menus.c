/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: John Sullivan <sullivan@eazel.com>
 */

/* nautilus-window-menus.h - implementation of nautilus window menu operations,
 *                           split into separate file just for convenience.
 */
#include <config.h>

#include <locale.h> 

#include "nautilus-actions.h"
#include "nautilus-application.h"
#include "nautilus-canvas-view.h"
#include "nautilus-connect-server-dialog.h"
#include "nautilus-file-management-properties.h"
#include "nautilus-list-view.h"
#include "nautilus-notebook.h"
#include "nautilus-window-private.h"
#include "nautilus-desktop-window.h"
#include "nautilus-properties-window.h"

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>

#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-icon-names.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-program-choosing.h>
#include <libnautilus-private/nautilus-search-directory.h>
#include <libnautilus-private/nautilus-search-engine.h>
#include <libnautilus-private/nautilus-signaller.h>
#include <libnautilus-private/nautilus-trash-monitor.h>
#include <string.h>

#define MENU_PATH_EXTENSION_ACTIONS                     "/ActionMenu/Extension Actions"
#define POPUP_PATH_EXTENSION_ACTIONS                     "/background/Before Zoom Items/Extension Actions"

#define NETWORK_URI          "network:"

static void
action_close_window_slot_callback (GtkAction *action,
				   gpointer user_data)
{
	NautilusWindow *window;
	NautilusWindowSlot *slot;

	window = NAUTILUS_WINDOW (user_data);
	slot = nautilus_window_get_active_slot (window);

	nautilus_window_slot_close (window, slot);
}

static void
action_connect_to_server_callback (GtkAction *action, 
				   gpointer user_data)
{
	g_action_group_activate_action (G_ACTION_GROUP (g_application_get_default ()),
					"connect-to-server", NULL);
}

static void
action_bookmarks_callback (GtkAction *action, 
			   gpointer user_data)
{
	g_action_group_activate_action (G_ACTION_GROUP (g_application_get_default ()),
					"bookmarks", NULL);
}

static void
action_stop_callback (GtkAction *action, 
		      gpointer user_data)
{
	NautilusWindow *window;
	NautilusWindowSlot *slot;

	window = NAUTILUS_WINDOW (user_data);
	slot = nautilus_window_get_active_slot (window);

	nautilus_window_slot_stop_loading (slot);
}

static void
action_home_callback (GtkAction *action, 
		      gpointer user_data) 
{
	NautilusWindow *window;
	NautilusWindowSlot *slot;

	window = NAUTILUS_WINDOW (user_data);
	slot = nautilus_window_get_active_slot (window);

	nautilus_window_slot_go_home (slot,
				      nautilus_event_get_window_open_flags ());
}

static void
action_reload_callback (GtkAction *action, 
			gpointer user_data) 
{
	NautilusWindowSlot *slot;

	slot = nautilus_window_get_active_slot (NAUTILUS_WINDOW (user_data));
	nautilus_window_slot_queue_reload (slot);
}

static void
action_location_properties_callback (GtkAction *action,
				     gpointer   user_data)
{
	NautilusWindowSlot *slot;
	GList              *files;
	NautilusView       *view;
	NautilusFile       *file;

	slot = nautilus_window_get_active_slot (NAUTILUS_WINDOW (user_data));
	view = nautilus_window_slot_get_current_view (slot);
	file = nautilus_view_get_directory_as_file (view);

	files = g_list_append (NULL, file);

	nautilus_properties_window_present (files, GTK_WIDGET (view), NULL);

	g_list_free (files);
}

static NautilusView *
get_current_view (NautilusWindow *window)
{
	NautilusWindowSlot *slot;
	NautilusView *view;

	slot = nautilus_window_get_active_slot (window);
	view = nautilus_window_slot_get_current_view (slot);

	return view;
}

static void
action_zoom_in_callback (GtkAction *action, 
			 gpointer user_data) 
{

	nautilus_view_bump_zoom_level (get_current_view (user_data), 1);
}

static void
action_zoom_out_callback (GtkAction *action, 
			  gpointer user_data) 
{
	nautilus_view_bump_zoom_level (get_current_view (user_data), -1);
}

static void
action_zoom_normal_callback (GtkAction *action, 
			     gpointer user_data) 
{
	nautilus_view_restore_default_zoom_level (get_current_view (user_data));
}

static void
action_preferences_callback (GtkAction *action, 
			     gpointer user_data)
{
	g_action_group_activate_action (G_ACTION_GROUP (g_application_get_default ()),
					"preferences", NULL);
}

static void
action_about_nautilus_callback (GtkAction *action,
				gpointer user_data)
{
	g_action_group_activate_action (G_ACTION_GROUP (g_application_get_default ()),
					"about", NULL);
}

static void
action_up_callback (GtkAction *action, 
		     gpointer user_data) 
{
	NautilusWindow *window = user_data;
	NautilusWindowSlot *slot;

	slot = nautilus_window_get_active_slot (window);
	nautilus_window_slot_go_up (slot, nautilus_event_get_window_open_flags ());
}

static void
action_nautilus_manual_callback (GtkAction *action, 
				 gpointer user_data)
{
	NautilusWindow *window;
	GError *error;
	GtkWidget *dialog;
	const char* helpuri;
	const char* name = gtk_action_get_name (action);

	error = NULL;
	window = NAUTILUS_WINDOW (user_data);

	if (g_str_equal (name, "NautilusHelpSearch")) {
		helpuri = "help:gnome-help/files-search";
	} else if (g_str_equal (name,"NautilusHelpSort")) {
		helpuri = "help:gnome-help/files-sort";
	} else if (g_str_equal (name, "NautilusHelpLost")) {
		helpuri = "help:gnome-help/files-lost";
	} else if (g_str_equal (name, "NautilusHelpShare")) {
		helpuri = "help:gnome-help/files-share";
	} else {
		helpuri = "help:gnome-help/files";
	}

	if (NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		nautilus_launch_application_from_command (gtk_window_get_screen (GTK_WINDOW (window)), "gnome-help", FALSE, NULL);
	} else {
		gtk_show_uri (gtk_window_get_screen (GTK_WINDOW (window)),
			      helpuri,
			      gtk_get_current_event_time (), &error);
	}

	if (error) {
		dialog = gtk_message_dialog_new (GTK_WINDOW (window),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("There was an error displaying help: \n%s"),
						 error->message);
		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}

#define MENU_ITEM_MAX_WIDTH_CHARS 32

static void
action_close_all_windows_callback (GtkAction *action, 
				   gpointer user_data)
{
	g_action_group_activate_action (G_ACTION_GROUP (g_application_get_default ()),
					"quit", NULL);
}

static void
action_back_callback (GtkAction *action, 
		      gpointer user_data) 
{
	nautilus_window_back_or_forward (NAUTILUS_WINDOW (user_data), 
					 TRUE, 0, nautilus_event_get_window_open_flags ());
}

static void
action_forward_callback (GtkAction *action, 
			 gpointer user_data) 
{
	nautilus_window_back_or_forward (NAUTILUS_WINDOW (user_data), 
					 FALSE, 0, nautilus_event_get_window_open_flags ());
}

static void
action_show_hide_sidebar_callback (GtkAction *action, 
                                  gpointer user_data)
{
       NautilusWindow *window;

       window = NAUTILUS_WINDOW (user_data);

       if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
               nautilus_window_show_sidebar (window);
       } else {
               nautilus_window_hide_sidebar (window);
       }
}

static void
action_add_bookmark_callback (GtkAction *action,
			      gpointer user_data)
{
	NautilusWindow *window = user_data;
	NautilusApplication *app = NAUTILUS_APPLICATION (g_application_get_default ());
	NautilusWindowSlot *slot;

	slot = nautilus_window_get_active_slot (window);
	nautilus_bookmark_list_append (nautilus_application_get_bookmarks (app),
				       nautilus_window_slot_get_bookmark (slot));
}

static void
connect_proxy_cb (GtkActionGroup *action_group,
                  GtkAction *action,
                  GtkWidget *proxy,
                  NautilusWindow *window)
{
	GtkLabel *label;

	if (!GTK_IS_MENU_ITEM (proxy))
		return;

	label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (proxy)));

	gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (label, MENU_ITEM_MAX_WIDTH_CHARS);
}

static void
action_new_window_callback (GtkAction *action,
			    gpointer user_data)
{
	g_action_group_activate_action (G_ACTION_GROUP (g_application_get_default ()),
					"new-window", NULL);
}

static void
action_new_tab_callback (GtkAction *action,
			 gpointer user_data)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (user_data);
	nautilus_window_new_tab (window);
}

static void
action_enter_location_callback (GtkAction *action,
				gpointer user_data)
{
	g_action_group_activate_action (G_ACTION_GROUP (g_application_get_default ()),
					"enter-location", NULL);
}

static void
action_tabs_previous_callback (GtkAction *action,
			       gpointer user_data)
{
	NautilusWindow *window = user_data;

	nautilus_notebook_prev_page (NAUTILUS_NOTEBOOK (window->details->notebook));
}

static void
action_tabs_next_callback (GtkAction *action,
			   gpointer user_data)
{
	NautilusWindow *window = user_data;

	nautilus_notebook_next_page (NAUTILUS_NOTEBOOK (window->details->notebook));
}

static void
action_tabs_move_left_callback (GtkAction *action,
				gpointer user_data)
{
	NautilusWindow *window = user_data;

	nautilus_notebook_reorder_current_child_relative (NAUTILUS_NOTEBOOK (window->details->notebook), -1);
}

static void
action_tabs_move_right_callback (GtkAction *action,
				 gpointer user_data)
{
	NautilusWindow *window = user_data;

	nautilus_notebook_reorder_current_child_relative (NAUTILUS_NOTEBOOK (window->details->notebook), 1);
}

static void
action_tab_change_action_activate_callback (GtkAction *action, 
					    gpointer user_data)
{
	NautilusWindow *window = user_data;
	GtkNotebook *notebook;
	int num;

	notebook = GTK_NOTEBOOK (window->details->notebook);

	num = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action), "num"));
	if (num < gtk_notebook_get_n_pages (notebook)) {
		gtk_notebook_set_current_page (notebook, num);
	}
}

static void
action_show_hide_search_callback (GtkAction *action,
				  NautilusWindow *window)
{
	gboolean active;
	NautilusWindowSlot *slot;

	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	slot = nautilus_window_get_active_slot (window);
	nautilus_window_slot_set_search_visible (slot, active);
}

static void
action_prompt_for_location_callback (GtkAction *action,
				     NautilusWindow *window)
{
	GFile *location;

	location = g_file_new_for_path ("/");
	nautilus_window_prompt_for_location (window, location);
	g_object_unref (location);
}

static void
action_view_radio_changed (GtkRadioAction *action,
			   GtkRadioAction *current,
			   NautilusWindow *window)
{
	const gchar *name;
	NautilusWindowSlot *slot;

	name = gtk_action_get_name (GTK_ACTION (current));
	slot = nautilus_window_get_active_slot (window);

	if (g_strcmp0 (name, NAUTILUS_ACTION_VIEW_LIST) == 0) {
		nautilus_window_slot_set_content_view (slot, NAUTILUS_LIST_VIEW_ID);
	} else if (g_strcmp0 (name, NAUTILUS_ACTION_VIEW_GRID) == 0) {
		nautilus_window_slot_set_content_view (slot, NAUTILUS_CANVAS_VIEW_ID);
	}
}

static const GtkActionEntry main_entries[] = {
  /* name, stock id, label */  { "Help", NULL, N_("_Help") },
  /* name, stock id */         { NAUTILUS_ACTION_CLOSE, GTK_STOCK_CLOSE,
  /* label, accelerator */       N_("_Close"), "<control>W",
  /* tooltip */                  N_("Close this folder"),
                                 G_CALLBACK (action_close_window_slot_callback) },
                               { NAUTILUS_ACTION_PREFERENCES, GTK_STOCK_PREFERENCES,
                                 N_("Prefere_nces"),               
                                 NULL, N_("Edit Nautilus preferences"),
                                 G_CALLBACK (action_preferences_callback) },
  /* name, stock id, label */  { NAUTILUS_ACTION_UP, GTK_STOCK_GO_UP, N_("Open _Parent"),
                                 "<alt>Up", N_("Open the parent folder"),
                                 G_CALLBACK (action_up_callback) },
  /* name, stock id */         { NAUTILUS_ACTION_STOP, GTK_STOCK_STOP,
  /* label, accelerator */       N_("_Stop"), NULL,
  /* tooltip */                  N_("Stop loading the current location"),
                                 G_CALLBACK (action_stop_callback) },
  /* name, stock id */         { NAUTILUS_ACTION_RELOAD, GTK_STOCK_REFRESH,
  /* label, accelerator */       N_("_Reload"), "<control>R",
  /* tooltip */                  N_("Reload the current location"),
                                 G_CALLBACK (action_reload_callback) },
  /* name, stock id */         { "ReloadAccel", NULL,
  /* label, accelerator */       "ReloadAccel", "F5",
  /* tooltip */                  NULL,
                                 G_CALLBACK (action_reload_callback) },
  /* name, stock id */         { NAUTILUS_ACTION_HELP, GTK_STOCK_HELP,
  /* label, accelerator */       N_("_All Topics"), "F1",
  /* tooltip */                  N_("Display Nautilus help"),
                                 G_CALLBACK (action_nautilus_manual_callback) },
  /* name, stock id */         { "NautilusHelpSearch", NULL,
  /* label, accelerator */       N_("Search for files"), NULL,
  /* tooltip */                  N_("Locate files based on file name and type. Save your searches for later use."),
                                 G_CALLBACK (action_nautilus_manual_callback) },
  /* name, stock id */         { "NautilusHelpSort", NULL,
  /* label, accelerator */       N_("Sort files and folders"), NULL,
  /* tooltip */                  N_("Arrange files by name, size, type, or when they were changed."),
                                 G_CALLBACK (action_nautilus_manual_callback) },
  /* name, stock id */         { "NautilusHelpLost", NULL,
  /* label, accelerator */       N_("Find a lost file"), NULL,
  /* tooltip */                  N_("Follow these tips if you can't find a file you created or downloaded."),
                                 G_CALLBACK (action_nautilus_manual_callback) },
  /* name, stock id */         { "NautilusHelpShare", NULL,
  /* label, accelerator */       N_("Share and transfer files"), NULL,
  /* tooltip */                  N_("Easily transfer files to your contacts and devices from the file manager."),
                                 G_CALLBACK (action_nautilus_manual_callback) },
  /* name, stock id */         { NAUTILUS_ACTION_ABOUT, GTK_STOCK_ABOUT,
  /* label, accelerator */       N_("_About"), NULL,
  /* tooltip */                  N_("Display credits for the creators of Nautilus"),
                                 G_CALLBACK (action_about_nautilus_callback) },
  /* name, stock id */         { NAUTILUS_ACTION_ZOOM_IN, GTK_STOCK_ZOOM_IN,
  /* label, accelerator */       N_("Zoom _In"), "<control>plus",
  /* tooltip */                  N_("Increase the view size"),
                                 G_CALLBACK (action_zoom_in_callback) },
  /* name, stock id */         { "ZoomInAccel", NULL,
  /* label, accelerator */       "ZoomInAccel", "<control>equal",
  /* tooltip */                  NULL,
                                 G_CALLBACK (action_zoom_in_callback) },
  /* name, stock id */         { "ZoomInAccel2", NULL,
  /* label, accelerator */       "ZoomInAccel2", "<control>KP_Add",
  /* tooltip */                  NULL,
                                 G_CALLBACK (action_zoom_in_callback) },
  /* name, stock id */         { NAUTILUS_ACTION_ZOOM_OUT, GTK_STOCK_ZOOM_OUT,
  /* label, accelerator */       N_("Zoom _Out"), "<control>minus",
  /* tooltip */                  N_("Decrease the view size"),
                                 G_CALLBACK (action_zoom_out_callback) },
  /* name, stock id */         { "ZoomOutAccel", NULL,
  /* label, accelerator */       "ZoomOutAccel", "<control>KP_Subtract",
  /* tooltip */                  NULL,
                                 G_CALLBACK (action_zoom_out_callback) },
  /* name, stock id */         { NAUTILUS_ACTION_ZOOM_NORMAL, GTK_STOCK_ZOOM_100,
  /* label, accelerator */       N_("Normal Si_ze"), "<control>0",
  /* tooltip */                  N_("Use the normal view size"),
                                 G_CALLBACK (action_zoom_normal_callback) },
  /* name, stock id */         { NAUTILUS_ACTION_CONNECT_TO_SERVER, NULL, 
  /* label, accelerator */       N_("Connect to _Server…"), NULL,
  /* tooltip */                  N_("Connect to a remote computer or shared disk"),
                                 G_CALLBACK (action_connect_to_server_callback) },
  /* name, stock id */         { NAUTILUS_ACTION_GO_HOME, NAUTILUS_ICON_HOME,
  /* label, accelerator */       N_("_Home"), "<alt>Home",
  /* tooltip */                  N_("Open your personal folder"),
                                 G_CALLBACK (action_home_callback) },
  /* name, stock id, label */  { NAUTILUS_ACTION_NEW_WINDOW, "window-new", N_("New _Window"),
                                 "<control>N", N_("Open another Nautilus window for the displayed location"),
                                 G_CALLBACK (action_new_window_callback) },
  /* name, stock id, label */  { NAUTILUS_ACTION_NEW_TAB, "tab-new", N_("New _Tab"),
                                 "<control>T", N_("Open another tab for the displayed location"),
                                 G_CALLBACK (action_new_tab_callback) },
  /* name, stock id, label */  { NAUTILUS_ACTION_CLOSE_ALL_WINDOWS, NULL, N_("Close _All Windows"),
                                 "<control>Q", N_("Close all Navigation windows"),
                                 G_CALLBACK (action_close_all_windows_callback) },
  /* name, stock id, label */  { NAUTILUS_ACTION_BACK, "go-previous-symbolic", N_("_Back"),
				 "<alt>Left", N_("Go to the previous visited location"),
				 G_CALLBACK (action_back_callback) },
  /* name, stock id, label */  { NAUTILUS_ACTION_FORWARD, "go-next-symbolic", N_("_Forward"),
				 "<alt>Right", N_("Go to the next visited location"),
				 G_CALLBACK (action_forward_callback) },
  /* name, stock id, label */  { NAUTILUS_ACTION_ENTER_LOCATION, NULL, N_("Enter _Location…"),
                                 "<control>L", N_("Specify a location to open"),
                                 G_CALLBACK (action_enter_location_callback) },
  /* name, stock id, label */  { NAUTILUS_ACTION_ADD_BOOKMARK, GTK_STOCK_ADD, N_("Bookmark this Location"),
                                 "<control>d", N_("Add a bookmark for the current location"),
                                 G_CALLBACK (action_add_bookmark_callback) },
  /* name, stock id, label */  { NAUTILUS_ACTION_EDIT_BOOKMARKS, NULL, N_("_Bookmarks…"),
                                 "<control>b", N_("Display and edit bookmarks"),
                                 G_CALLBACK (action_bookmarks_callback) },
  { "TabsPrevious", NULL, N_("_Previous Tab"), "<control>Page_Up",
    N_("Activate previous tab"),
    G_CALLBACK (action_tabs_previous_callback) },
  { "TabsNext", NULL, N_("_Next Tab"), "<control>Page_Down",
    N_("Activate next tab"),
    G_CALLBACK (action_tabs_next_callback) },
  { "TabsMoveLeft", NULL, N_("Move Tab _Left"), "<shift><control>Page_Up",
    N_("Move current tab to left"),
    G_CALLBACK (action_tabs_move_left_callback) },
  { "TabsMoveRight", NULL, N_("Move Tab _Right"), "<shift><control>Page_Down",
    N_("Move current tab to right"),
    G_CALLBACK (action_tabs_move_right_callback) },
  /* name, stock id */         { NAUTILUS_ACTION_LOCATION_PROPERTIES, GTK_STOCK_PROPERTIES,
  /* label, accelerator */       N_("P_roperties"), NULL,
  /* tooltip */                  N_("View or modify the properties of this folder"),
				 G_CALLBACK (action_location_properties_callback) },
  /* name, stock id */         { "PromptLocationAccel", NULL,
  /* label, accelerator */       "PromptLocationAccel", "slash",
  /* tooltip */                  NULL,
				 G_CALLBACK (action_prompt_for_location_callback) },
};

static const GtkToggleActionEntry main_toggle_entries[] = {
  /* name, stock id */     { NAUTILUS_ACTION_SHOW_HIDE_SIDEBAR, NULL,
  /* label, accelerator */   N_("_Show Sidebar"), "F9",
  /* tooltip */              N_("Change the visibility of this window's side pane"),
                             G_CALLBACK (action_show_hide_sidebar_callback),
  /* is_active */            TRUE },
  /* name, stock id */     { NAUTILUS_ACTION_SEARCH, "edit-find-symbolic",
  /* label, accelerator */   N_("_Search for Files…"), "<control>f",
  /* tooltip */              N_("Search documents and folders by name"),
			     G_CALLBACK (action_show_hide_search_callback),
  /* is_active */            FALSE },
};

static const GtkRadioActionEntry view_radio_entries[] = {
	{ NAUTILUS_ACTION_VIEW_LIST, "view-list-symbolic", N_("List"),
	  "<control>1", N_("View items as a list"), 0 },
	{ NAUTILUS_ACTION_VIEW_GRID, "view-grid-symbolic", N_("List"),
	  "<control>2", N_("View items as a grid of icons"), 1 }
};

static const gchar* app_actions[] = {
	NAUTILUS_ACTION_NEW_WINDOW,
	NAUTILUS_ACTION_CONNECT_TO_SERVER,
	NAUTILUS_ACTION_ENTER_LOCATION,
	NAUTILUS_ACTION_EDIT_BOOKMARKS,
	NAUTILUS_ACTION_PREFERENCES,
	NAUTILUS_ACTION_HELP,
	NAUTILUS_ACTION_ABOUT,
	NAUTILUS_ACTION_CLOSE_ALL_WINDOWS,

	/* also hide the help menu entirely when using an app menu */
	"Help"
};

static void
action_toggle_state (GSimpleAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	GVariant *state;

	state = g_action_get_state (G_ACTION (action));
	g_action_change_state (G_ACTION (action),
			       g_variant_new_boolean (!g_variant_get_boolean (state)));
	g_variant_unref (state);
}

const GActionEntry win_entries[] = {
	{ "gear-menu", action_toggle_state, NULL, "false", NULL },
};

void 
nautilus_window_initialize_actions (NautilusWindow *window)
{
	g_action_map_add_action_entries (G_ACTION_MAP (window),
					 win_entries, G_N_ELEMENTS (win_entries),
					 window);
}

static void
nautilus_window_menus_set_visibility_for_app_menu (NautilusWindow *window)
{
	const gchar *action_name;
	gboolean shows_app_menu;
	GtkSettings *settings;
	GtkAction *action;
	gint idx;

	settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (window)));
	g_object_get (settings,
		      "gtk-shell-shows-app-menu", &shows_app_menu,
		      NULL);

	for (idx = 0; idx < G_N_ELEMENTS (app_actions); idx++) {
		action_name = app_actions[idx];
		action = gtk_action_group_get_action (window->details->main_action_group, action_name);

		gtk_action_set_visible (action, !shows_app_menu);
	}
}

/**
 * nautilus_window_initialize_menus
 * 
 * Create and install the set of menus for this window.
 * @window: A recently-created NautilusWindow.
 */
void 
nautilus_window_initialize_menus (NautilusWindow *window)
{
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkAction *action;
	gint i;

	window->details->ui_manager = gtk_ui_manager_new ();
	ui_manager = window->details->ui_manager;

	/* shell actions */
	action_group = gtk_action_group_new ("ShellActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	window->details->main_action_group = action_group;
	gtk_action_group_add_actions (action_group,
				      main_entries, G_N_ELEMENTS (main_entries),
				      window);
	gtk_action_group_add_toggle_actions (action_group, 
					     main_toggle_entries, G_N_ELEMENTS (main_toggle_entries),
					     window);
	gtk_action_group_add_radio_actions (action_group,
					    view_radio_entries, G_N_ELEMENTS (view_radio_entries),
					    -1, G_CALLBACK (action_view_radio_changed),
					    window);

	nautilus_window_menus_set_visibility_for_app_menu (window);
	window->details->app_menu_visibility_id =
		g_signal_connect_swapped (gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (window))),
					  "notify::gtk-shell-shows-app-menu",
					  G_CALLBACK (nautilus_window_menus_set_visibility_for_app_menu), window);

	action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_UP);
	g_object_set (action, "short_label", _("_Up"), NULL);

	action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_HOME);
	g_object_set (action, "short_label", _("_Home"), NULL);

	/* Alt+N for the first 10 tabs */
	for (i = 0; i < 10; ++i) {
		gchar action_name[80];
		gchar accelerator[80];

		snprintf(action_name, sizeof (action_name), "Tab%d", i);
		action = gtk_action_new (action_name, NULL, NULL, NULL);
		g_object_set_data (G_OBJECT (action), "num", GINT_TO_POINTER (i));
		g_signal_connect (action, "activate",
				G_CALLBACK (action_tab_change_action_activate_callback), window);
		snprintf(accelerator, sizeof (accelerator), "<alt>%d", (i+1)%10);
		gtk_action_group_add_action_with_accel (action_group, action, accelerator);
		g_object_unref (action);
		gtk_ui_manager_add_ui (ui_manager,
				gtk_ui_manager_new_merge_id (ui_manager),
				"/",
				action_name,
				action_name,
				GTK_UI_MANAGER_ACCELERATOR,
				FALSE);

	}

	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui_manager */

	gtk_window_add_accel_group (GTK_WINDOW (window),
				    gtk_ui_manager_get_accel_group (ui_manager));
	
	g_signal_connect (ui_manager, "connect-proxy",
			  G_CALLBACK (connect_proxy_cb), window);

	/* add the UI */
	gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/gnome/nautilus/nautilus-shell-ui.xml", NULL);
}

void
nautilus_window_finalize_menus (NautilusWindow *window)
{
	if (window->details->app_menu_visibility_id != 0) {
		g_signal_handler_disconnect (gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (window))),
					     window->details->app_menu_visibility_id);
		window->details->app_menu_visibility_id = 0;
	}
}

static GList *
get_extension_menus (NautilusWindow *window)
{
	NautilusFile *file;
	NautilusWindowSlot *slot;
	GList *providers;
	GList *items;
	GList *l;
	
	providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);
	items = NULL;

	slot = nautilus_window_get_active_slot (window);
	file = nautilus_window_slot_get_file (slot);

	for (l = providers; l != NULL; l = l->next) {
		NautilusMenuProvider *provider;
		GList *file_items;
		
		provider = NAUTILUS_MENU_PROVIDER (l->data);
		file_items = nautilus_menu_provider_get_background_items (provider,
									  GTK_WIDGET (window),
									  file);
		items = g_list_concat (items, file_items);
	}

	nautilus_module_extension_list_free (providers);

	return items;
}

static void
add_extension_menu_items (NautilusWindow *window,
			  guint merge_id,
			  GtkActionGroup *action_group,
			  GList *menu_items,
			  const char *subdirectory)
{
	GtkUIManager *ui_manager;
	GList *l;

	ui_manager = window->details->ui_manager;
	
	for (l = menu_items; l; l = l->next) {
		NautilusMenuItem *item;
		NautilusMenu *menu;
		GtkAction *action;
		char *path;
		
		item = NAUTILUS_MENU_ITEM (l->data);
		
		g_object_get (item, "menu", &menu, NULL);
		
		action = nautilus_action_from_menu_item (item);
		gtk_action_group_add_action_with_accel (action_group, action, NULL);
		
		path = g_build_path ("/", POPUP_PATH_EXTENSION_ACTIONS, subdirectory, NULL);
		gtk_ui_manager_add_ui (ui_manager,
				       merge_id,
				       path,
				       gtk_action_get_name (action),
				       gtk_action_get_name (action),
				       (menu != NULL) ? GTK_UI_MANAGER_MENU : GTK_UI_MANAGER_MENUITEM,
				       FALSE);
		g_free (path);

		path = g_build_path ("/", MENU_PATH_EXTENSION_ACTIONS, subdirectory, NULL);
		gtk_ui_manager_add_ui (ui_manager,
				       merge_id,
				       path,
				       gtk_action_get_name (action),
				       gtk_action_get_name (action),
				       (menu != NULL) ? GTK_UI_MANAGER_MENU : GTK_UI_MANAGER_MENUITEM,
				       FALSE);
		g_free (path);

		/* recursively fill the menu */		       
		if (menu != NULL) {
			char *subdir;
			GList *children;
			
			children = nautilus_menu_get_items (menu);
			
			subdir = g_build_path ("/", subdirectory, "/", gtk_action_get_name (action), NULL);
			add_extension_menu_items (window,
						  merge_id,
						  action_group,
						  children,
						  subdir);

			nautilus_menu_item_list_free (children);
			g_free (subdir);
		}			
	}
}

void
nautilus_window_load_extension_menus (NautilusWindow *window)
{
	GtkActionGroup *action_group;
	GList *items;
	guint merge_id;

	if (window->details->extensions_menu_merge_id != 0) {
		gtk_ui_manager_remove_ui (window->details->ui_manager,
					  window->details->extensions_menu_merge_id);
		window->details->extensions_menu_merge_id = 0;
	}

	if (window->details->extensions_menu_action_group != NULL) {
		gtk_ui_manager_remove_action_group (window->details->ui_manager,
						    window->details->extensions_menu_action_group);
		window->details->extensions_menu_action_group = NULL;
	}
	
	merge_id = gtk_ui_manager_new_merge_id (window->details->ui_manager);
	window->details->extensions_menu_merge_id = merge_id;
	action_group = gtk_action_group_new ("ExtensionsMenuGroup");
	window->details->extensions_menu_action_group = action_group;
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_ui_manager_insert_action_group (window->details->ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui manager */

	items = get_extension_menus (window);

	if (items != NULL) {
		add_extension_menu_items (window, merge_id, action_group, items, "");

		g_list_foreach (items, (GFunc) g_object_unref, NULL);
		g_list_free (items);
	}
}

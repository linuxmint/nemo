/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 * Author: John Sullivan <sullivan@eazel.com>
 */

/* nemo-window-menus.h - implementation of nemo window menu operations,
 *                           split into separate file just for convenience.
 */
#include <config.h>

#include <locale.h> 

#include "nemo-actions.h"
#include "nemo-application.h"
#include "nemo-connect-server-dialog.h"
#include "nemo-file-management-properties.h"
#include "nemo-navigation-action.h"
#include "nemo-notebook.h"
#include "nemo-window-manage-views.h"
#include "nemo-window-bookmarks.h"
#include "nemo-window-private.h"
#include "nemo-desktop-window.h"
#include "nemo-search-bar.h"
#include "nemo-icon-view.h"
#include "nemo-list-view.h"
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>

#include <libnemo-extension/nemo-menu-provider.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-icon-names.h>
#include <libnemo-private/nemo-ui-utilities.h>
#include <libnemo-private/nemo-module.h>
#include <libnemo-private/nemo-undo-manager.h>
#include <libnemo-private/nemo-program-choosing.h>
#include <libnemo-private/nemo-search-directory.h>
#include <libnemo-private/nemo-search-engine.h>
#include <libnemo-private/nemo-signaller.h>
#include <libnemo-private/nemo-trash-monitor.h>
#include <string.h>

#define MENU_PATH_EXTENSION_ACTIONS                     "/MenuBar/File/Extension Actions"
#define POPUP_PATH_EXTENSION_ACTIONS                     "/background/Before Zoom Items/Extension Actions"

#define NETWORK_URI          "network:"
#define COMPUTER_URI         "computer:"
#define BURN_CD_URI          "burn:"

static void
action_close_window_slot_callback (GtkAction *action,
				   gpointer user_data)
{
	NemoWindow *window;
	NemoWindowSlot *slot;

	window = NEMO_WINDOW (user_data);
	slot = nemo_window_get_active_slot (window);

	nemo_window_pane_slot_close (slot->pane, slot);
}

static void
action_connect_to_server_callback (GtkAction *action, 
				   gpointer user_data)
{
	NemoWindow *window = NEMO_WINDOW (user_data);
	GtkWidget *dialog;

	dialog = nemo_connect_server_dialog_new (window);

	gtk_widget_show (dialog);
}

static void
action_stop_callback (GtkAction *action, 
		      gpointer user_data)
{
	NemoWindow *window;
	NemoWindowSlot *slot;

	window = NEMO_WINDOW (user_data);
	slot = nemo_window_get_active_slot (window);

	nemo_window_slot_stop_loading (slot);
}

#ifdef TEXT_CHANGE_UNDO
static void
action_undo_callback (GtkAction *action, 
		      gpointer user_data) 
{
	NemoApplication *app;

	app = nemo_application_get_singleton ();
	nemo_undo_manager_undo (app->undo_manager);
}
#endif

static void
action_home_callback (GtkAction *action, 
		      gpointer user_data) 
{
	NemoWindow *window;
	NemoWindowSlot *slot;

	window = NEMO_WINDOW (user_data);
	slot = nemo_window_get_active_slot (window);

	nemo_window_slot_go_home (slot, 
				      nemo_event_should_open_in_new_tab ());
}

static void
action_go_to_computer_callback (GtkAction *action, 
				gpointer user_data) 
{
	NemoWindow *window;
	NemoWindowSlot *slot;
	GFile *computer;

	window = NEMO_WINDOW (user_data);
	slot = nemo_window_get_active_slot (window);

	computer = g_file_new_for_uri (COMPUTER_URI);
	nemo_window_slot_go_to (slot,
				    computer,
				    nemo_event_should_open_in_new_tab ());
	g_object_unref (computer);
}

static void
action_go_to_network_callback (GtkAction *action, 
				gpointer user_data) 
{
	NemoWindow *window;
	NemoWindowSlot *slot;
	GFile *network;

	window = NEMO_WINDOW (user_data);
	slot = nemo_window_get_active_slot (window);

	network = g_file_new_for_uri (NETWORK_URI);
	nemo_window_slot_go_to (slot,
				    network,
				    nemo_event_should_open_in_new_tab ());
	g_object_unref (network);
}

static void
action_go_to_templates_callback (GtkAction *action,
				 gpointer user_data) 
{
	NemoWindow *window;
	NemoWindowSlot *slot;
	char *path;
	GFile *location;

	window = NEMO_WINDOW (user_data);
	slot = nemo_window_get_active_slot (window);

	path = nemo_get_templates_directory ();
	location = g_file_new_for_path (path);
	g_free (path);
	nemo_window_slot_go_to (slot,
				    location,
				    nemo_event_should_open_in_new_tab ());
	g_object_unref (location);
}

static void
action_go_to_trash_callback (GtkAction *action, 
			     gpointer user_data) 
{
	NemoWindow *window;
	NemoWindowSlot *slot;
	GFile *trash;

	window = NEMO_WINDOW (user_data);
	slot = nemo_window_get_active_slot (window);

	trash = g_file_new_for_uri ("trash:///");
	nemo_window_slot_go_to (slot,
				    trash,
				    nemo_event_should_open_in_new_tab ());
	g_object_unref (trash);
}

static void
action_reload_callback (GtkAction *action, 
			gpointer user_data) 
{
	NemoWindowSlot *slot;

	slot = nemo_window_get_active_slot (NEMO_WINDOW (user_data));
	nemo_window_slot_reload (slot);
}

static NemoView *
get_current_view (NemoWindow *window)
{
	NemoWindowSlot *slot;
	NemoView *view;

	slot = nemo_window_get_active_slot (window);
	view = nemo_window_slot_get_current_view (slot);

	return view;
}

static void
action_zoom_in_callback (GtkAction *action, 
			 gpointer user_data) 
{

	nemo_view_bump_zoom_level (get_current_view (user_data), 1);
}

static void
action_zoom_out_callback (GtkAction *action, 
			  gpointer user_data) 
{
	nemo_view_bump_zoom_level (get_current_view (user_data), -1);
}

static void
action_zoom_normal_callback (GtkAction *action, 
			     gpointer user_data) 
{
	nemo_view_restore_default_zoom_level (get_current_view (user_data));
}

static void
action_show_hidden_files_callback (GtkAction *action, 
				   gpointer callback_data)
{
	NemoWindow *window;
	NemoWindowShowHiddenFilesMode mode;

	window = NEMO_WINDOW (callback_data);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		mode = NEMO_WINDOW_SHOW_HIDDEN_FILES_ENABLE;
	} else {
		mode = NEMO_WINDOW_SHOW_HIDDEN_FILES_DISABLE;
	}

	nemo_window_set_hidden_files_mode (window, mode);
}

static void
show_hidden_files_preference_callback (gpointer callback_data)
{
	NemoWindow *window;
	GtkAction *action;

	window = NEMO_WINDOW (callback_data);

	if (window->details->show_hidden_files_mode == NEMO_WINDOW_SHOW_HIDDEN_FILES_DEFAULT) {
		action = gtk_action_group_get_action (nemo_window_get_main_action_group (window),
						      NEMO_ACTION_SHOW_HIDDEN_FILES);

		/* update button */
		g_signal_handlers_block_by_func (action, action_show_hidden_files_callback, window);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
					      g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_HIDDEN_FILES));
		g_signal_handlers_unblock_by_func (action, action_show_hidden_files_callback, window);

		/* inform views */
		nemo_window_set_hidden_files_mode (window, NEMO_WINDOW_SHOW_HIDDEN_FILES_DEFAULT);

	}
}

static void
action_preferences_callback (GtkAction *action, 
			     gpointer user_data)
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);

	nemo_file_management_properties_dialog_show (window);
}

static void
action_about_nemo_callback (GtkAction *action,
				gpointer user_data)
{	
	const gchar *license[] = {
		N_("Nemo is free software; you can redistribute it and/or modify "
		   "it under the terms of the GNU General Public License as published by "
		   "the Free Software Foundation; either version 2 of the License, or "
		   "(at your option) any later version."),
		N_("Nemo is distributed in the hope that it will be useful, "
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
		   "GNU General Public License for more details."),
		N_("You should have received a copy of the GNU General Public License "
		   "along with Nemo; if not, write to the Free Software Foundation, Inc., "
		   "51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA")
	};
	gchar *license_trans, *copyright_str;
	GDateTime *date;

	license_trans = g_strjoin ("\n\n", _(license[0]), _(license[1]),
					     _(license[2]), NULL);

	date = g_date_time_new_now_local ();	

	gtk_show_about_dialog (GTK_WINDOW (user_data),
			       "program-name", _("Nemo"),
			       "version", VERSION,
			       "comments", _("Nemo lets you organize "
					     "files and folders, both on "
					     "your computer and online."),			       
			       "license", license_trans,
			       "wrap-license", TRUE,			       				
			      "logo-icon-name", "folder",			      
			      NULL);

	g_free (license_trans);
	g_free (copyright_str);
	g_date_time_unref (date);
}

static void
action_up_callback (GtkAction *action, 
		     gpointer user_data) 
{
	NemoWindow *window = user_data;
	NemoWindowSlot *slot;

	slot = nemo_window_get_active_slot (window);
	nemo_window_slot_go_up (slot, FALSE, nemo_event_should_open_in_new_tab ());
}

static void
action_nemo_manual_callback (GtkAction *action, 
				 gpointer user_data)
{
	NemoWindow *window;
	GError *error;
	GtkWidget *dialog;
	const char* helpuri;
	const char* name = gtk_action_get_name (action);

	error = NULL;
	window = NEMO_WINDOW (user_data);

	if (g_str_equal (name, "NemoHelpSearch")) {
		helpuri = "help:gnome-help/files-search";
	} else if (g_str_equal (name,"NemoHelpSort")) {
		helpuri = "help:gnome-help/files-sort";
	} else if (g_str_equal (name, "NemoHelpLost")) {
		helpuri = "help:gnome-help/files-lost";
	} else if (g_str_equal (name, "NemoHelpShare")) {
		helpuri = "help:gnome-help/files-share";
	} else {
		helpuri = "help:gnome-help/files";
	}

	if (NEMO_IS_DESKTOP_WINDOW (window)) {
		nemo_launch_application_from_command (gtk_window_get_screen (GTK_WINDOW (window)), "gnome-help", FALSE, NULL);
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

static void
menu_item_select_cb (GtkMenuItem *proxy,
		     NemoWindow *window)
{
	GtkAction *action;
	char *message;

	action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (proxy));
	g_return_if_fail (action != NULL);
	
	g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
	if (message) {
		gtk_statusbar_push (GTK_STATUSBAR (window->details->statusbar),
				    window->details->help_message_cid, message);
		g_free (message);
	}
}

static void
menu_item_deselect_cb (GtkMenuItem *proxy,
		       NemoWindow *window)
{
	gtk_statusbar_pop (GTK_STATUSBAR (window->details->statusbar),
			   window->details->help_message_cid);
}

static void
disconnect_proxy_cb (GtkUIManager *manager,
		     GtkAction *action,
		     GtkWidget *proxy,
		     NemoWindow *window)
{
	if (GTK_IS_MENU_ITEM (proxy)) {
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_select_cb), window);
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_deselect_cb), window);
	}
}

static void
trash_state_changed_cb (NemoTrashMonitor *monitor,
			gboolean state,
			NemoWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	GIcon *gicon;

	action_group = nemo_window_get_main_action_group (window);
	action = gtk_action_group_get_action (action_group, "Go to Trash");

	gicon = nemo_trash_monitor_get_icon ();

	if (gicon) {
		g_object_set (action, "gicon", gicon, NULL);
		g_object_unref (gicon);
	}
}

static void
nemo_window_initialize_trash_icon_monitor (NemoWindow *window)
{
	NemoTrashMonitor *monitor;

	monitor = nemo_trash_monitor_get ();

	trash_state_changed_cb (monitor, TRUE, window);

	g_signal_connect (monitor, "trash_state_changed",
			  G_CALLBACK (trash_state_changed_cb), window);
}

#define MENU_ITEM_MAX_WIDTH_CHARS 32

enum {
	SIDEBAR_PLACES,
	SIDEBAR_TREE
};

static void
action_close_all_windows_callback (GtkAction *action, 
				   gpointer user_data)
{
	nemo_application_close_all_windows (nemo_application_get_singleton ());
}

static void
action_back_callback (GtkAction *action, 
		      gpointer user_data) 
{
	nemo_window_back_or_forward (NEMO_WINDOW (user_data), 
					 TRUE, 0, nemo_event_should_open_in_new_tab ());
}

static void
action_forward_callback (GtkAction *action, 
			 gpointer user_data) 
{
	nemo_window_back_or_forward (NEMO_WINDOW (user_data), 
					 FALSE, 0, nemo_event_should_open_in_new_tab ());
}

static void
action_split_view_switch_next_pane_callback(GtkAction *action,
					    gpointer user_data)
{
	nemo_window_pane_grab_focus (nemo_window_get_next_pane (NEMO_WINDOW (user_data)));
}

static void
action_split_view_same_location_callback (GtkAction *action,
					  gpointer user_data)
{
	NemoWindow *window;
	NemoWindowPane *next_pane;
	GFile *location;

	window = NEMO_WINDOW (user_data);
	next_pane = nemo_window_get_next_pane (window);

	if (!next_pane) {
		return;
	}
	location = nemo_window_slot_get_location (next_pane->active_slot);
	if (location) {
		nemo_window_slot_go_to (nemo_window_get_active_slot (window), location, FALSE);
		g_object_unref (location);
	}
}

static void
action_show_hide_sidebar_callback (GtkAction *action, 
				   gpointer user_data)
{
	NemoWindow *window;

	window = NEMO_WINDOW (user_data);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		nemo_window_show_sidebar (window);
	} else {
		nemo_window_hide_sidebar (window);
	}
}

static void
action_split_view_callback (GtkAction *action,
			    gpointer user_data)
{
	NemoWindow *window;
	gboolean is_active;

	window = NEMO_WINDOW (user_data);

	is_active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	if (is_active != nemo_window_split_view_showing (window)) {
		NemoWindowSlot *slot;

		if (is_active) {
			nemo_window_split_view_on (window);
		} else {
			nemo_window_split_view_off (window);
		}

		slot = nemo_window_get_active_slot (window);
		if (slot != NULL) {
			nemo_view_update_menus (slot->content_view);
		}
	}
}

static void
nemo_window_update_split_view_actions_sensitivity (NemoWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	gboolean have_multiple_panes;
	gboolean next_pane_is_in_same_location;
	GFile *active_pane_location;
	GFile *next_pane_location;
	NemoWindowPane *next_pane;
	NemoWindowSlot *active_slot;

	active_slot = nemo_window_get_active_slot (window);
	action_group = nemo_window_get_main_action_group (window);

	/* collect information */
	have_multiple_panes = nemo_window_split_view_showing (window);
	if (active_slot != NULL) {
		active_pane_location = nemo_window_slot_get_location (active_slot);
	} else {
		active_pane_location = NULL;
	}

	next_pane = nemo_window_get_next_pane (window);
	if (next_pane && next_pane->active_slot) {
		next_pane_location = nemo_window_slot_get_location (next_pane->active_slot);
		next_pane_is_in_same_location = (active_pane_location && next_pane_location &&
						 g_file_equal (active_pane_location, next_pane_location));
	} else {
		next_pane_location = NULL;
		next_pane_is_in_same_location = FALSE;
	}

	/* switch to next pane */
	action = gtk_action_group_get_action (action_group, "SplitViewNextPane");
	gtk_action_set_sensitive (action, have_multiple_panes);

	/* same location */
	action = gtk_action_group_get_action (action_group, "SplitViewSameLocation");
	gtk_action_set_sensitive (action, have_multiple_panes && !next_pane_is_in_same_location);

	/* clean up */
	g_clear_object (&active_pane_location);
	g_clear_object (&next_pane_location);
}

/* TODO: bind all of this with g_settings_bind and GBinding */
static guint
sidebar_id_to_value (const gchar *sidebar_id)
{
	guint retval = SIDEBAR_PLACES;

	if (g_strcmp0 (sidebar_id, NEMO_WINDOW_SIDEBAR_TREE) == 0)
		retval = SIDEBAR_TREE;

	return retval;
}

void
nemo_window_update_show_hide_menu_items (NemoWindow *window) 
{
	GtkActionGroup *action_group;
	GtkAction *action;
	guint current_value;

	action_group = nemo_window_get_main_action_group (window);

	action = gtk_action_group_get_action (action_group,
					      NEMO_ACTION_SHOW_HIDE_EXTRA_PANE);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      nemo_window_split_view_showing (window));
	nemo_window_update_split_view_actions_sensitivity (window);

	action = gtk_action_group_get_action (action_group,
					      "Sidebar Places");
	current_value = sidebar_id_to_value (window->details->sidebar_id);
	gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), current_value);
}

static void
action_add_bookmark_callback (GtkAction *action,
			      gpointer user_data)
{
        nemo_window_add_bookmark_for_current_location (NEMO_WINDOW (user_data));
}

static void
action_edit_bookmarks_callback (GtkAction *action, 
				gpointer user_data)
{
        nemo_window_edit_bookmarks (NEMO_WINDOW (user_data));
}

static void
connect_proxy_cb (GtkActionGroup *action_group,
                  GtkAction *action,
                  GtkWidget *proxy,
                  NemoWindow *window)
{
	GtkLabel *label;

	if (!GTK_IS_MENU_ITEM (proxy))
		return;

	label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (proxy)));

	gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (label, MENU_ITEM_MAX_WIDTH_CHARS);

	g_signal_connect (proxy, "select",
			  G_CALLBACK (menu_item_select_cb), window);
	g_signal_connect (proxy, "deselect",
			  G_CALLBACK (menu_item_deselect_cb), window);
}

static const char* icon_entries[] = {
	"/MenuBar/Other Menus/Go/Home",
	"/MenuBar/Other Menus/Go/Computer",
	"/MenuBar/Other Menus/Go/Go to Templates",
	"/MenuBar/Other Menus/Go/Go to Trash",
	"/MenuBar/Other Menus/Go/Go to Network",
	"/MenuBar/Other Menus/Go/Edit Location"
};

/**
 * refresh_go_menu:
 * 
 * Refresh list of bookmarks at end of Go menu to match centralized history list.
 * @window: The NemoWindow whose Go menu will be refreshed.
 **/
static void
nemo_window_initialize_go_menu (NemoWindow *window)
{
	GtkUIManager *ui_manager;
	GtkWidget *menuitem;
	int i;

	ui_manager = nemo_window_get_ui_manager (NEMO_WINDOW (window));

	for (i = 0; i < G_N_ELEMENTS (icon_entries); i++) {
		menuitem = gtk_ui_manager_get_widget (
				ui_manager,
				icon_entries[i]);

		gtk_image_menu_item_set_always_show_image (
				GTK_IMAGE_MENU_ITEM (menuitem), TRUE);
	}
}

static void
action_new_window_callback (GtkAction *action,
			    gpointer user_data)
{
	NemoApplication *application;
	NemoWindow *current_window, *new_window;

	current_window = NEMO_WINDOW (user_data);
	application = nemo_application_get_singleton ();

	new_window = nemo_application_create_window (
				application,
				gtk_window_get_screen (GTK_WINDOW (current_window)));
	nemo_window_slot_go_home (nemo_window_get_active_slot (new_window), FALSE);
}

static void
action_new_tab_callback (GtkAction *action,
			 gpointer user_data)
{
	NemoWindow *window;

	window = NEMO_WINDOW (user_data);
	nemo_window_new_tab (window);
}

static void
action_edit_location_callback (GtkAction *action,
				gpointer user_data)
{
	NemoWindow *window = user_data;
	NemoWindowPane *pane;

	pane = nemo_window_get_active_pane (window);
	nemo_window_pane_ensure_location_bar (pane);
}

static void
action_icon_view_callback (GtkAction *action,
                           gpointer user_data)
{
    NemoWindow *window;
    NemoWindowSlot *slot;
    window = NEMO_WINDOW (user_data);
    slot = nemo_window_get_active_slot (window);
    nemo_window_slot_set_content_view (slot, NEMO_ICON_VIEW_ID);
    toolbar_set_view_button (NEMO_ACTION_ICON_VIEW, nemo_window_get_active_pane(window));
}


static void
action_list_view_callback (GtkAction *action,
                           gpointer user_data)
{
    NemoWindow *window;
    NemoWindowSlot *slot;
    window = NEMO_WINDOW (user_data);
    slot = nemo_window_get_active_slot (window);
    nemo_window_slot_set_content_view (slot, NEMO_LIST_VIEW_ID);
    toolbar_set_view_button (NEMO_ACTION_LIST_VIEW, nemo_window_get_active_pane(window));
}


static void
action_compact_view_callback (GtkAction *action,
                           gpointer user_data)
{
    NemoWindow *window;
    NemoWindowSlot *slot;
    window = NEMO_WINDOW (user_data);
    slot = nemo_window_get_active_slot (window);
    nemo_window_slot_set_content_view (slot, FM_COMPACT_VIEW_ID);
    toolbar_set_view_button (NEMO_ACTION_COMPACT_VIEW, nemo_window_get_active_pane(window));
}


gchar *
toolbar_action_for_view_id (gchar *view_id)
{
    if (g_strcmp0(view_id, NEMO_ICON_VIEW_ID) == 0) {
        return NEMO_ACTION_ICON_VIEW;
    } else if (g_strcmp0(view_id, NEMO_LIST_VIEW_ID) == 0) {
        return NEMO_ACTION_LIST_VIEW;
    } else if (g_strcmp0(view_id, FM_COMPACT_VIEW_ID) == 0) {
        return NEMO_ACTION_COMPACT_VIEW;
    } else {
        return NULL;
    }
}

void
toolbar_set_view_button (gchar *action_id, NemoWindowPane *pane)
{
    GtkAction *action, *action1, *action2;
    GtkActionGroup *action_group;
    if (action_id == NULL) {
        return;
    }
    action_group = nemo_window_pane_get_toolbar_action_group (pane);


    action = gtk_action_group_get_action(action_group,
                                         NEMO_ACTION_ICON_VIEW);
    action1 = gtk_action_group_get_action(action_group,
                                         NEMO_ACTION_LIST_VIEW);
    action2 = gtk_action_group_get_action(action_group,
                                         NEMO_ACTION_COMPACT_VIEW);

    g_signal_handlers_block_matched (action,
                         G_SIGNAL_MATCH_FUNC,
                         0, 0,
                         NULL,
                         action_icon_view_callback,
                         NULL);

    g_signal_handlers_block_matched (action1,
                         G_SIGNAL_MATCH_FUNC,
                         0, 0,
                         NULL,
                         action_list_view_callback,
                         NULL);
    g_signal_handlers_block_matched (action2,
                         G_SIGNAL_MATCH_FUNC,
                         0, 0,
                         NULL,
                         action_compact_view_callback,
                         NULL);

    if (g_strcmp0(action_id, NEMO_ACTION_ICON_VIEW) != 0) {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), FALSE);
    } else {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), TRUE);
    }

    if (g_strcmp0(action_id, NEMO_ACTION_LIST_VIEW) != 0) {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action1), FALSE);
    } else {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action1), TRUE);
    }

    if (g_strcmp0(action_id, NEMO_ACTION_COMPACT_VIEW) != 0) {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action2), FALSE);
    } else {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action2), TRUE);
    }

    g_signal_handlers_unblock_matched (action,
                           G_SIGNAL_MATCH_FUNC,
                           0, 0,
                           NULL,
                           action_icon_view_callback,
                           NULL);


    g_signal_handlers_unblock_matched (action1,
                           G_SIGNAL_MATCH_FUNC,
                           0, 0,
                           NULL,
                           action_list_view_callback,
                           NULL);


    g_signal_handlers_unblock_matched (action2,
                           G_SIGNAL_MATCH_FUNC,
                           0, 0,
                           NULL,
                           action_compact_view_callback,
                           NULL);

}

static void
action_tabs_previous_callback (GtkAction *action,
			       gpointer user_data)
{
	NemoWindowPane *pane;
	NemoWindow *window = user_data;

	pane = nemo_window_get_active_pane (window);
	nemo_notebook_set_current_page_relative (NEMO_NOTEBOOK (pane->notebook), -1);
}

static void
action_tabs_next_callback (GtkAction *action,
			   gpointer user_data)
{
	NemoWindowPane *pane;
	NemoWindow *window = user_data;

	pane = nemo_window_get_active_pane (window);
	nemo_notebook_set_current_page_relative (NEMO_NOTEBOOK (pane->notebook), 1);
}

static void
action_tabs_move_left_callback (GtkAction *action,
				gpointer user_data)
{
	NemoWindowPane *pane;
	NemoWindow *window = user_data;

	pane = nemo_window_get_active_pane (window);
	nemo_notebook_reorder_current_child_relative (NEMO_NOTEBOOK (pane->notebook), -1);
}

static void
action_tabs_move_right_callback (GtkAction *action,
				 gpointer user_data)
{
	NemoWindowPane *pane;
	NemoWindow *window = user_data;

	pane = nemo_window_get_active_pane (window);
	nemo_notebook_reorder_current_child_relative (NEMO_NOTEBOOK (pane->notebook), 1);
}

static void
action_tab_change_action_activate_callback (GtkAction *action, 
					    gpointer user_data)
{
	NemoWindowPane *pane;
	NemoWindow *window = user_data;
	GtkNotebook *notebook;
	int num;

	pane = nemo_window_get_active_pane (window);
	notebook = GTK_NOTEBOOK (pane->notebook);

	num = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action), "num"));
	if (num < gtk_notebook_get_n_pages (notebook)) {
		gtk_notebook_set_current_page (notebook, num);
	}
}

static void
sidebar_radio_entry_changed_cb (GtkAction *action,
				GtkRadioAction *current,
				gpointer user_data)
{
	gint current_value;

	current_value = gtk_radio_action_get_current_value (current);

	if (current_value == SIDEBAR_PLACES) {
		g_settings_set_string (nemo_window_state,
				       NEMO_WINDOW_STATE_SIDE_PANE_VIEW,
				       NEMO_WINDOW_SIDEBAR_PLACES);
	} else if (current_value == SIDEBAR_TREE) {
		g_settings_set_string (nemo_window_state,
				       NEMO_WINDOW_STATE_SIDE_PANE_VIEW,
				       NEMO_WINDOW_SIDEBAR_TREE);
	}
}

static const GtkActionEntry main_entries[] = {
  /* name, stock id, label */  { "File", NULL, N_("_File") },
  /* name, stock id, label */  { "Edit", NULL, N_("_Edit") },
  /* name, stock id, label */  { "View", NULL, N_("_View") },
  /* name, stock id, label */  { "Help", NULL, N_("_Help") },
  /* name, stock id */         { "Close", GTK_STOCK_CLOSE,
  /* label, accelerator */       N_("_Close"), "<control>W",
  /* tooltip */                  N_("Close this folder"),
                                 G_CALLBACK (action_close_window_slot_callback) },
                               { "Preferences", GTK_STOCK_PREFERENCES,
                                 N_("Prefere_nces"),               
                                 NULL, N_("Edit Nemo preferences"),
                                 G_CALLBACK (action_preferences_callback) },
#ifdef TEXT_CHANGE_UNDO
  /* name, stock id, label */  { "Undo", NULL, N_("_Undo"),
                                 "<control>Z", N_("Undo the last text change"),
                                 G_CALLBACK (action_undo_callback) },
#endif
  /* name, stock id, label */  { "Up", GTK_STOCK_GO_UP, N_("Open _Parent"),
                                 "<alt>Up", N_("Open the parent folder"),
                                 G_CALLBACK (action_up_callback) },
  /* name, stock id, label */  { "UpAccel", NULL, "UpAccel",
                                 "", NULL,
                                 G_CALLBACK (action_up_callback) },
  /* name, stock id */         { "Stop", GTK_STOCK_STOP,
  /* label, accelerator */       N_("_Stop"), NULL,
  /* tooltip */                  N_("Stop loading the current location"),
                                 G_CALLBACK (action_stop_callback) },
  /* name, stock id */         { "Reload", GTK_STOCK_REFRESH,
  /* label, accelerator */       N_("_Reload"), "<control>R",
  /* tooltip */                  N_("Reload the current location"),
                                 G_CALLBACK (action_reload_callback) },
  /* name, stock id */         { "NemoHelp", GTK_STOCK_HELP,
  /* label, accelerator */       N_("_All Topics"), "F1",
  /* tooltip */                  N_("Display Nemo help"),
                                 G_CALLBACK (action_nemo_manual_callback) },
  /* name, stock id */         { "NemoHelpSearch", NULL,
  /* label, accelerator */       N_("Search for files"), NULL,
  /* tooltip */                  N_("Locate files based on file name and type. Save your searches for later use."),
                                 G_CALLBACK (action_nemo_manual_callback) },
  /* name, stock id */         { "NemoHelpSort", NULL,
  /* label, accelerator */       N_("Sort files and folders"), NULL,
  /* tooltip */                  N_("Arrange files by name, size, type, or when they were changed."),
                                 G_CALLBACK (action_nemo_manual_callback) },
  /* name, stock id */         { "NemoHelpLost", NULL,
  /* label, accelerator */       N_("Find a lost file"), NULL,
  /* tooltip */                  N_("Follow these tips if you can't find a file you created or downloaded."),
                                 G_CALLBACK (action_nemo_manual_callback) },
  /* name, stock id */         { "NemoHelpShare", NULL,
  /* label, accelerator */       N_("Share and transfer files"), NULL,
  /* tooltip */                  N_("Easily transfer files to your contacts and devices from the file manager."),
                                 G_CALLBACK (action_nemo_manual_callback) },
  /* name, stock id */         { "About Nemo", GTK_STOCK_ABOUT,
  /* label, accelerator */       N_("_About"), NULL,
  /* tooltip */                  N_("Display credits for the creators of Nemo"),
                                 G_CALLBACK (action_about_nemo_callback) },
  /* name, stock id */         { "Zoom In", GTK_STOCK_ZOOM_IN,
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
  /* name, stock id */         { "Zoom Out", GTK_STOCK_ZOOM_OUT,
  /* label, accelerator */       N_("Zoom _Out"), "<control>minus",
  /* tooltip */                  N_("Decrease the view size"),
                                 G_CALLBACK (action_zoom_out_callback) },
  /* name, stock id */         { "ZoomOutAccel", NULL,
  /* label, accelerator */       "ZoomOutAccel", "<control>KP_Subtract",
  /* tooltip */                  NULL,
                                 G_CALLBACK (action_zoom_out_callback) },
  /* name, stock id */         { "Zoom Normal", GTK_STOCK_ZOOM_100,
  /* label, accelerator */       N_("Normal Si_ze"), "<control>0",
  /* tooltip */                  N_("Use the normal view size"),
                                 G_CALLBACK (action_zoom_normal_callback) },
  /* name, stock id */         { "Connect to Server", NULL, 
  /* label, accelerator */       N_("Connect to _Server..."), NULL,
  /* tooltip */                  N_("Connect to a remote computer or shared disk"),
                                 G_CALLBACK (action_connect_to_server_callback) },
  /* name, stock id */         { "Home", NEMO_ICON_HOME,
  /* label, accelerator */       N_("_Home"), "<alt>Home",
  /* tooltip */                  N_("Open your personal folder"),
                                 G_CALLBACK (action_home_callback) },
  /* name, stock id */         { "Go to Computer", NEMO_ICON_COMPUTER,
  /* label, accelerator */       N_("_Computer"), NULL,
  /* tooltip */                  N_("Browse all local and remote disks and folders accessible from this computer"),
                                 G_CALLBACK (action_go_to_computer_callback) },
  /* name, stock id */         { "Go to Network", NEMO_ICON_NETWORK,
  /* label, accelerator */       N_("_Network"), NULL,
  /* tooltip */                  N_("Browse bookmarked and local network locations"),
                                 G_CALLBACK (action_go_to_network_callback) },
  /* name, stock id */         { "Go to Templates", NEMO_ICON_TEMPLATE,
  /* label, accelerator */       N_("T_emplates"), NULL,
  /* tooltip */                  N_("Open your personal templates folder"),
                                 G_CALLBACK (action_go_to_templates_callback) },
  /* name, stock id */         { "Go to Trash", NEMO_ICON_TRASH,
  /* label, accelerator */       N_("_Trash"), NULL,
  /* tooltip */                  N_("Open your personal trash folder"),
                                 G_CALLBACK (action_go_to_trash_callback) },
  /* name, stock id, label */  { "Go", NULL, N_("_Go") },
  /* name, stock id, label */  { "Bookmarks", NULL, N_("_Bookmarks") },
  /* name, stock id, label */  { "Tabs", NULL, N_("_Tabs") },
  /* name, stock id, label */  { "New Window", "window-new", N_("New _Window"),
                                 "<control>N", N_("Open another Nemo window for the displayed location"),
                                 G_CALLBACK (action_new_window_callback) },
  /* name, stock id, label */  { "New Tab", "tab-new", N_("New _Tab"),
                                 "<control>T", N_("Open another tab for the displayed location"),
                                 G_CALLBACK (action_new_tab_callback) },
  /* name, stock id, label */  { "Close All Windows", NULL, N_("Close _All Windows"),
                                 "<control>Q", N_("Close all Navigation windows"),
                                 G_CALLBACK (action_close_all_windows_callback) },
  /* name, stock id, label */  { NEMO_ACTION_BACK, GTK_STOCK_GO_BACK, N_("_Back"),
				 "<alt>Left", N_("Go to the previous visited location"),
				 G_CALLBACK (action_back_callback) },
  /* name, stock id, label */  { NEMO_ACTION_FORWARD, GTK_STOCK_GO_FORWARD, N_("_Forward"),
				 "<alt>Right", N_("Go to the next visited location"),
				 G_CALLBACK (action_forward_callback) },
  /* name, stock id, label */  { NEMO_ACTION_EDIT_LOCATION, NULL, N_("_Location..."),
                                 "<control>L", N_("Specify a location to open"),
                                 G_CALLBACK (action_edit_location_callback) },
  /* name, stock id, label */  { "SplitViewNextPane", NULL, N_("S_witch to Other Pane"),
				 "F6", N_("Move focus to the other pane in a split view window"),
				 G_CALLBACK (action_split_view_switch_next_pane_callback) },
  /* name, stock id, label */  { "SplitViewSameLocation", NULL, N_("Sa_me Location as Other Pane"),
				 NULL, N_("Go to the same location as in the extra pane"),
				 G_CALLBACK (action_split_view_same_location_callback) },
  /* name, stock id, label */  { "Add Bookmark", GTK_STOCK_ADD, N_("_Add Bookmark"),
                                 "<control>d", N_("Add a bookmark for the current location to this menu"),
                                 G_CALLBACK (action_add_bookmark_callback) },
  /* name, stock id, label */  { "Edit Bookmarks", NULL, N_("_Edit Bookmarks..."),
                                 "<control>b", N_("Display a window that allows editing the bookmarks in this menu"),
                                 G_CALLBACK (action_edit_bookmarks_callback) },
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
  { "Sidebar List", NULL, N_("Sidebar") }
};

static const GtkToggleActionEntry main_toggle_entries[] = {
  /* name, stock id */         { "Show Hidden Files", NULL,
  /* label, accelerator */       N_("Show _Hidden Files"), "<control>H",
  /* tooltip */                  N_("Toggle the display of hidden files in the current window"),
                                 G_CALLBACK (action_show_hidden_files_callback),
                                 TRUE },
  /* name, stock id */     { "Show Hide Toolbar", NULL,
  /* label, accelerator */   N_("_Main Toolbar"), NULL,
  /* tooltip */              N_("Change the visibility of this window's main toolbar"),
			     NULL,
  /* is_active */            TRUE }, 
  /* name, stock id */     { "Show Hide Sidebar", NULL,
  /* label, accelerator */   N_("_Show Sidebar"), "F9",
  /* tooltip */              N_("Change the visibility of this window's side pane"),
                             G_CALLBACK (action_show_hide_sidebar_callback),
  /* is_active */            TRUE }, 
  /* name, stock id */     { "Show Hide Statusbar", NULL,
  /* label, accelerator */   N_("St_atusbar"), NULL,
  /* tooltip */              N_("Change the visibility of this window's statusbar"),
                             NULL,
  /* is_active */            TRUE },
  /* name, stock id */     { "Search", "edit-find",
  /* label, accelerator */   N_("_Search for Files..."), "<control>f",
  /* tooltip */              N_("Search documents and folders by name"),
			     NULL,
  /* is_active */            FALSE },
  /* name, stock id */     { NEMO_ACTION_SHOW_HIDE_EXTRA_PANE, NULL,
  /* label, accelerator */   N_("E_xtra Pane"), "F3",
  /* tooltip */              N_("Open an extra folder view side-by-side"),
                             G_CALLBACK (action_split_view_callback),
  /* is_active */            FALSE },
  /* name, stock id */     { "Show Hide Location Entry", NULL,
  /* label, accelerator */   N_("Loca_tion entry"), "<control>l",
  /* tooltip */              N_("Change the visibility of this window's location entry"),
                             NULL,
  /* is_active */            TRUE },
};

static const GtkRadioActionEntry main_radio_entries[] = {
	{ "Sidebar Places", NULL,
	  N_("Places"), NULL, N_("Select Places as the default sidebar"),
	  SIDEBAR_PLACES },
	{ "Sidebar Tree", NULL,
	  N_("Tree"), NULL, N_("Select Tree as the default sidebar"),
	  SIDEBAR_TREE }
};

GtkActionGroup *
nemo_window_create_toolbar_action_group (NemoWindow *window)
{
	gboolean show_label_search_icon_toolbar;

	NemoNavigationState *navigation_state;
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = gtk_action_group_new ("ToolbarActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);

	action = g_object_new (NEMO_TYPE_NAVIGATION_ACTION,
			       "name", NEMO_ACTION_BACK,
			       "label", _("_Back"),
			       "stock_id", GTK_STOCK_GO_BACK,
			       "tooltip", _("Go to the previous visited location"),
			       "arrow-tooltip", _("Back history"),
			       "window", window,
			       "direction", NEMO_NAVIGATION_DIRECTION_BACK,
			       "sensitive", FALSE,
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (action_back_callback), window);
	gtk_action_group_add_action (action_group, action);

	g_object_unref (action);

	action = g_object_new (NEMO_TYPE_NAVIGATION_ACTION,
			       "name", NEMO_ACTION_FORWARD,
			       "label", _("_Forward"),
			       "stock_id", GTK_STOCK_GO_FORWARD,
			       "tooltip", _("Go to the next visited location"),
			       "arrow-tooltip", _("Forward history"),
			       "window", window,
			       "direction", NEMO_NAVIGATION_DIRECTION_FORWARD,
			       "sensitive", FALSE,
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (action_forward_callback), window);
	gtk_action_group_add_action (action_group, action);

	g_object_unref (action);

	/**
	 * Nemo 2.30/2.32 type actions
	 */
   	action = g_object_new (NEMO_TYPE_NAVIGATION_ACTION,
   			       "name", NEMO_ACTION_UP,
   			       "label", _("_Up"),
   			       "stock_id", GTK_STOCK_GO_UP,
   			       "tooltip", _("Go to parent folder"),
   			       "arrow-tooltip", _("Forward history"),
   			       "window", window,
   			       "direction", NEMO_NAVIGATION_DIRECTION_UP,
   			       NULL);
   	g_signal_connect (action, "activate",
   			  G_CALLBACK (action_up_callback), window);
   	gtk_action_group_add_action (action_group, action);
   
   	g_object_unref (action);
  
   	action = g_object_new (NEMO_TYPE_NAVIGATION_ACTION,
   			       "name", NEMO_ACTION_RELOAD,
   			       "label", _("_Reload"),
   			       "stock_id", GTK_STOCK_REFRESH,
   			       "tooltip", _("Reload the current location"),
   			       "window", window,
   			       "direction", NEMO_NAVIGATION_DIRECTION_RELOAD,
   			       NULL);
   	g_signal_connect (action, "activate",
   			  G_CALLBACK (action_reload_callback), window);
   	gtk_action_group_add_action (action_group, action);
   	
   	g_object_unref (action);
   
   	action = g_object_new (NEMO_TYPE_NAVIGATION_ACTION,
   			       "name", NEMO_ACTION_HOME,
   			       "label", _("_Home"),
   			       "stock_id", GTK_STOCK_HOME,
   			       "tooltip", _("Go to home directory"),
   			       "window", window,
   			       "direction", NEMO_NAVIGATION_DIRECTION_HOME,
   			       NULL);
   	g_signal_connect (action, "activate",
   			  G_CALLBACK (action_home_callback), window);
   	gtk_action_group_add_action (action_group, action);
   
   	g_object_unref (action);
   
   	action = g_object_new (NEMO_TYPE_NAVIGATION_ACTION,
   			       "name", NEMO_ACTION_COMPUTER,
   			       "label", _("_Computer"),
   			       "icon_name", "computer",
   			       "tooltip", _("Go to Computer"),
   			       "window", window,
   			       "direction", NEMO_NAVIGATION_DIRECTION_COMPUTER,
   			       NULL);
   	g_signal_connect (action, "activate",
   			  G_CALLBACK (action_go_to_computer_callback), window);
   	gtk_action_group_add_action (action_group, action);
   
   	g_object_unref (action);
 
   	action = g_object_new (NEMO_TYPE_NAVIGATION_ACTION,
   			       "name", NEMO_ACTION_EDIT_LOCATION,
   			       "label", _("Location"),
   			       "icon_name", "location-symbolic",
   			       "tooltip", _("Toggle Location bar / Path bar"),
   			       "window", window,
    			       "direction", NEMO_NAVIGATION_DIRECTION_EDIT,
   			       NULL);
   	g_signal_connect (action, "activate",
   			  G_CALLBACK (action_edit_location_callback), window);
   	gtk_action_group_add_action (action_group, action);
  
   	g_object_unref (action);

    action = GTK_ACTION (gtk_toggle_action_new (NEMO_ACTION_ICON_VIEW,
                         _("Icons"),
                         _("Icon View"),
                         NULL));
    g_signal_connect (action, "activate",
                      G_CALLBACK (action_icon_view_callback),
                      window);
   	gtk_action_group_add_action (action_group, action);
    gtk_action_set_icon_name (GTK_ACTION (action), "view-grid-symbolic");
   	g_object_unref (action);

    action = GTK_ACTION (gtk_toggle_action_new (NEMO_ACTION_LIST_VIEW,
                         _("List"),
                         _("List View"),
                         NULL));
    g_signal_connect (action, "activate",
                      G_CALLBACK (action_list_view_callback),
                      window);
   	gtk_action_group_add_action (action_group, action);
    gtk_action_set_icon_name (GTK_ACTION (action), "view-list-symbolic");

   	g_object_unref (action);

    action = GTK_ACTION (gtk_toggle_action_new (NEMO_ACTION_COMPACT_VIEW,
                         _("Compact"),
                         _("Compact View"),
                         NULL));
   	g_signal_connect (action, "activate",
                      G_CALLBACK (action_compact_view_callback),
                      window);
   	gtk_action_group_add_action (action_group, action);
    gtk_action_set_icon_name (GTK_ACTION (action), "view-compact-symbolic");

   	g_object_unref (action);

 	action = GTK_ACTION (gtk_toggle_action_new (NEMO_ACTION_SEARCH,
 				_("Search"),_("Search documents and folders by name"),
 				NULL));
 
  	gtk_action_group_add_action (action_group, action);
    gtk_action_set_icon_name (GTK_ACTION (action), "edit-find-symbolic");
 
 
 	show_label_search_icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_LABEL_SEARCH_ICON_TOOLBAR);
 	gtk_action_set_is_important (GTK_ACTION (action), show_label_search_icon_toolbar);
  
  	g_object_unref (action);

	navigation_state = nemo_window_get_navigation_state (window);
	nemo_navigation_state_add_group (navigation_state, action_group);

	return action_group;
}

static void
window_menus_set_bindings (NemoWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = nemo_window_get_main_action_group (window);

	action = gtk_action_group_get_action (action_group,
					      NEMO_ACTION_SHOW_HIDE_TOOLBAR);

	g_settings_bind (nemo_window_state,
			 NEMO_WINDOW_STATE_START_WITH_TOOLBAR,
			 action,
			 "active",
			 G_SETTINGS_BIND_DEFAULT);

	action = gtk_action_group_get_action (action_group,
					      NEMO_ACTION_SHOW_HIDE_STATUSBAR);

	g_settings_bind (nemo_window_state,
			 NEMO_WINDOW_STATE_START_WITH_STATUS_BAR,
			 action,
			 "active",
			 G_SETTINGS_BIND_DEFAULT);

	action = gtk_action_group_get_action (action_group,
					      NEMO_ACTION_SHOW_HIDE_SIDEBAR);	

	g_settings_bind (nemo_window_state,
			 NEMO_WINDOW_STATE_START_WITH_SIDEBAR,
			 action,
			 "active",
			 G_SETTINGS_BIND_DEFAULT);
    
    action = gtk_action_group_get_action (action_group,
					      NEMO_ACTION_SHOW_HIDE_LOCATION_ENTRY);

	g_settings_bind (nemo_preferences,
             NEMO_PREFERENCES_SHOW_LOCATION_ENTRY,
			 action,
			 "active",
             G_SETTINGS_BIND_DEFAULT);
}

void 
nemo_window_initialize_actions (NemoWindow *window)
{
	GtkActionGroup *action_group;
	const gchar *nav_state_actions[] = {
		NEMO_ACTION_BACK, NEMO_ACTION_FORWARD, NEMO_ACTION_UP, NEMO_ACTION_RELOAD, NEMO_ACTION_COMPUTER, NEMO_ACTION_HOME, NEMO_ACTION_EDIT_LOCATION,
		NEMO_ACTION_SEARCH, NULL
	};

	action_group = nemo_window_get_main_action_group (window);
	window->details->nav_state = nemo_navigation_state_new (action_group,
								    nav_state_actions);

	window_menus_set_bindings (window);
	nemo_window_update_show_hide_menu_items (window);

	g_signal_connect (window, "loading_uri",
			  G_CALLBACK (nemo_window_update_split_view_actions_sensitivity),
			  NULL);
}

/**
 * nemo_window_initialize_menus
 * 
 * Create and install the set of menus for this window.
 * @window: A recently-created NemoWindow.
 */
void 
nemo_window_initialize_menus (NemoWindow *window)
{
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkAction *action;
	gint i;

	if (window->details->ui_manager == NULL){
        window->details->ui_manager = gtk_ui_manager_new ();
    }
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
					    main_radio_entries, G_N_ELEMENTS (main_radio_entries),
					    0, G_CALLBACK (sidebar_radio_entry_changed_cb),
					    window);

	action = gtk_action_group_get_action (action_group, NEMO_ACTION_UP);
	g_object_set (action, "short_label", _("_Up"), NULL);

	action = gtk_action_group_get_action (action_group, NEMO_ACTION_HOME);
	g_object_set (action, "short_label", _("_Home"), NULL);

  	action = gtk_action_group_get_action (action_group, NEMO_ACTION_EDIT_LOCATION);
  	g_object_set (action, "short_label", _("_Location"), NULL);

	action = gtk_action_group_get_action (action_group, NEMO_ACTION_SHOW_HIDDEN_FILES);
	g_signal_handlers_block_by_func (action, action_show_hidden_files_callback, window);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_HIDDEN_FILES));
	g_signal_handlers_unblock_by_func (action, action_show_hidden_files_callback, window);


	g_signal_connect_swapped (nemo_preferences, "changed::" NEMO_PREFERENCES_SHOW_HIDDEN_FILES,
				  G_CALLBACK(show_hidden_files_preference_callback),
				  window);

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
	
	g_signal_connect (ui_manager, "connect_proxy",
			  G_CALLBACK (connect_proxy_cb), window);
	g_signal_connect (ui_manager, "disconnect_proxy",
			  G_CALLBACK (disconnect_proxy_cb), window);

	/* add the UI */
	gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/nemo/nemo-shell-ui.xml", NULL);

	nemo_window_initialize_trash_icon_monitor (window);
	nemo_window_initialize_go_menu (window);
}

void
nemo_window_finalize_menus (NemoWindow *window)
{
	NemoTrashMonitor *monitor;

	monitor = nemo_trash_monitor_get ();

	g_signal_handlers_disconnect_by_func (monitor,
					      trash_state_changed_cb, window);

	g_signal_handlers_disconnect_by_func (nemo_preferences,
					      show_hidden_files_preference_callback, window);
}

static GList *
get_extension_menus (NemoWindow *window)
{
	NemoWindowSlot *slot;
	GList *providers;
	GList *items;
	GList *l;
	
	providers = nemo_module_get_extensions_for_type (NEMO_TYPE_MENU_PROVIDER);
	items = NULL;

	slot = nemo_window_get_active_slot (window);

	for (l = providers; l != NULL; l = l->next) {
		NemoMenuProvider *provider;
		GList *file_items;
		
		provider = NEMO_MENU_PROVIDER (l->data);
		file_items = nemo_menu_provider_get_background_items (provider,
									  GTK_WIDGET (window),
									  slot->viewed_file);
		items = g_list_concat (items, file_items);
	}

	nemo_module_extension_list_free (providers);

	return items;
}

static void
add_extension_menu_items (NemoWindow *window,
			  guint merge_id,
			  GtkActionGroup *action_group,
			  GList *menu_items,
			  const char *subdirectory)
{
	GtkUIManager *ui_manager;
	GList *l;

	ui_manager = window->details->ui_manager;
	
	for (l = menu_items; l; l = l->next) {
		NemoMenuItem *item;
		NemoMenu *menu;
		GtkAction *action;
		char *path;
		
		item = NEMO_MENU_ITEM (l->data);
		
		g_object_get (item, "menu", &menu, NULL);
		
		action = nemo_action_from_menu_item (item);
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
			
			children = nemo_menu_get_items (menu);
			
			subdir = g_build_path ("/", subdirectory, "/", gtk_action_get_name (action), NULL);
			add_extension_menu_items (window,
						  merge_id,
						  action_group,
						  children,
						  subdir);

			nemo_menu_item_list_free (children);
			g_free (subdir);
		}			
	}
}

void
nemo_window_load_extension_menus (NemoWindow *window)
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

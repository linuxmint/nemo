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

#include "nemo-window-menus.h"
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
#include "nemo-location-bar.h"
#include "nemo-icon-view.h"
#include "nemo-list-view.h"
#include "nemo-toolbar.h"

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include <eel/eel-vfs-extensions.h>
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
#define MENU_BAR_PATH                                    "/MenuBar"
#define NEMO_ACTION_SHOW_HIDE_TERMINAL                  "Show Hide Terminal"

#define NETWORK_URI          "network:"
#define COMPUTER_URI         "computer:"

static void set_content_view_type(NemoWindow *window,
                                  const gchar *view_id);

enum {
    NULL_VIEW,
    ICON_VIEW,
    LIST_VIEW,
    COMPACT_VIEW,
    SIDEBAR_PLACES,
    SIDEBAR_TREE,
    TOOLBAR_PATHBAR,
    TOOLBAR_ENTRY
};

static void
action_close_window_slot_callback (GtkAction *action,
				   gpointer user_data)
{
	NemoWindow *window;
	NemoWindowSlot *slot;

	window = NEMO_WINDOW (user_data);
	slot = nemo_window_get_active_slot (window);

	nemo_window_pane_close_slot (slot->pane, slot);
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
    if (!NEMO_IS_DESKTOP_WINDOW (user_data)) {
        NemoWindow *window;
        NemoWindowSlot *slot;

        window = NEMO_WINDOW (user_data);

        slot = nemo_window_get_active_slot (window);

        nemo_window_slot_go_home (slot, nemo_event_get_window_open_flags ());
    }
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
	nemo_window_slot_open_location (slot, computer,
					    nemo_event_get_window_open_flags ());
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
	nemo_window_slot_open_location (slot, network,
					    nemo_event_get_window_open_flags ());
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
	nemo_window_slot_open_location (slot, location,
					    nemo_event_get_window_open_flags ());
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
	nemo_window_slot_open_location (slot, trash,
					    nemo_event_get_window_open_flags ());
	g_object_unref (trash);
}

static void
action_reload_callback (GtkAction *action,
			gpointer user_data)
{
	NemoWindowSlot *slot;

	slot = nemo_window_get_active_slot (NEMO_WINDOW (user_data));
	nemo_window_slot_queue_reload (slot, TRUE);
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
    if (!NEMO_IS_DESKTOP_WINDOW (user_data)) {
        nemo_view_bump_zoom_level (get_current_view (user_data), 1);
    }
}

static void
action_zoom_out_callback (GtkAction *action,
			  gpointer user_data)
{
    if (!NEMO_IS_DESKTOP_WINDOW (user_data)) {
        nemo_view_bump_zoom_level (get_current_view (user_data), -1);
    }
}

static void
action_zoom_normal_callback (GtkAction *action,
			     gpointer user_data)
{
    if (!NEMO_IS_DESKTOP_WINDOW (user_data)) {
        nemo_view_restore_default_zoom_level (get_current_view (user_data));
    }
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
action_preferences_callback (GtkAction *action,
			     gpointer user_data)
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);

	nemo_file_management_properties_dialog_show (window, NULL);
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
	gchar *license_trans;
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
	g_date_time_unref (date);
}

static void
action_up_callback (GtkAction *action,
		     gpointer user_data)
{
	NemoWindow *window = user_data;
	NemoWindowSlot *slot;

	slot = nemo_window_get_active_slot (window);
	nemo_window_slot_go_up (slot, nemo_event_get_window_open_flags ());
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
action_show_shortcuts_window (GtkAction *action,
                              gpointer user_data)
{
    NemoWindow *window;
    static GtkWidget *shortcuts_window;

    window = NEMO_WINDOW (user_data);

    if (shortcuts_window == NULL)
    {
        GtkBuilder *builder;

        builder = gtk_builder_new_from_resource ("/org/nemo/nemo-shortcuts.ui");
        shortcuts_window = GTK_WIDGET (gtk_builder_get_object (builder, "keyboard_shortcuts"));

        gtk_window_set_position (GTK_WINDOW (shortcuts_window), GTK_WIN_POS_CENTER);

        g_signal_connect (shortcuts_window, "destroy",
                          G_CALLBACK (gtk_widget_destroyed), &shortcuts_window);

        g_object_unref (builder);
    }

    if (GTK_WINDOW (window) != gtk_window_get_transient_for (GTK_WINDOW (shortcuts_window)))
    {
        gtk_window_set_transient_for (GTK_WINDOW (shortcuts_window), GTK_WINDOW (window));
    }

    gtk_widget_show_all (shortcuts_window);
    gtk_window_present (GTK_WINDOW (shortcuts_window));
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
    gchar *icon_name;

	action_group = nemo_window_get_main_action_group (window);
	action = gtk_action_group_get_action (action_group, "Go to Trash");

    icon_name = nemo_trash_monitor_get_symbolic_icon_name ();

    if (icon_name) {
        g_object_set (action, "icon-name", icon_name, NULL);
        g_clear_pointer (&icon_name, g_free);
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

static void
action_close_all_windows_callback (GtkAction *action,
				   gpointer user_data)
{
	if (!NEMO_IS_DESKTOP_WINDOW (user_data)) {
		nemo_application_close_all_windows (nemo_application_get_singleton ());
	}
}

static void
action_back_callback (GtkAction *action,
		      gpointer user_data)
{
	nemo_window_back_or_forward (NEMO_WINDOW (user_data),
					 TRUE, 0, nemo_event_get_window_open_flags ());
}

static void
action_forward_callback (GtkAction *action,
			 gpointer user_data)
{
	nemo_window_back_or_forward (NEMO_WINDOW (user_data),
					 FALSE, 0, nemo_event_get_window_open_flags ());
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
		nemo_window_slot_open_location (nemo_window_get_active_slot (window),
						    location, 0);
		g_object_unref (location);
	}
}

static void
action_show_hide_sidebar_callback (GtkAction *action,
				   gpointer user_data)
{
	NemoWindow *window;

	if (!NEMO_IS_DESKTOP_WINDOW (user_data)) {
		window = NEMO_WINDOW (user_data);

		if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
			nemo_window_show_sidebar (window);
		} else {
			nemo_window_hide_sidebar (window);
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

static void
action_split_view_callback (GtkAction *action,
			    gpointer user_data)
{
	NemoWindow *window;
	gboolean is_active;

    if (NEMO_IS_DESKTOP_WINDOW (user_data)) {
        return;
    }

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

    nemo_window_update_split_view_actions_sensitivity (window);
}

static void
sidebar_radio_entry_changed_cb (GtkAction *action,
                GtkRadioAction *current,
                gpointer user_data)
{
    gint current_value;
    NemoWindow *window = NEMO_WINDOW (user_data);

    current_value = gtk_radio_action_get_current_value (current);

    switch (current_value) {
        case SIDEBAR_PLACES:
            nemo_window_set_sidebar_id (window, NEMO_WINDOW_SIDEBAR_PLACES);
            break;
        case SIDEBAR_TREE:
            nemo_window_set_sidebar_id (window, NEMO_WINDOW_SIDEBAR_TREE);
            break;
        default:
            ;
            break;
    }
}

static void
view_radio_entry_changed_cb (GtkAction *action,
                             GtkRadioAction *current,
                             gpointer user_data)
{
    gint current_value;
    NemoWindow *window = NEMO_WINDOW (user_data);

    if (NEMO_IS_DESKTOP_WINDOW (window)) {
        return;
    }

    current_value = gtk_radio_action_get_current_value (current);

    switch (current_value) {
        case ICON_VIEW:
            set_content_view_type (window, NEMO_ICON_VIEW_ID);
            break;
        case LIST_VIEW:
            set_content_view_type (window, NEMO_LIST_VIEW_ID);
            break;
        case COMPACT_VIEW:
            set_content_view_type (window, FM_COMPACT_VIEW_ID);
            break;
        default:
            ;
            break;
    }
}

static void
toolbar_radio_entry_changed_cb (GtkAction *action,
                                GtkRadioAction *current,
                                gpointer user_data)
{
    NemoWindow *window = NEMO_WINDOW (user_data);
    NemoWindowPane *pane;
    GtkAction *toggle_action;
    gint current_value;

    if (NEMO_IS_DESKTOP_WINDOW (window)) {
        return;
    }

    pane = nemo_window_get_active_pane (window);
    toggle_action = gtk_action_group_get_action (pane->action_group, NEMO_ACTION_TOGGLE_LOCATION);

    current_value = gtk_radio_action_get_current_value (current);
    switch (current_value) {
        case TOOLBAR_PATHBAR:
            g_settings_set_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_LOCATION_ENTRY, FALSE);
            gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (toggle_action), FALSE);
            break;
        case TOOLBAR_ENTRY:
            g_settings_set_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_LOCATION_ENTRY, TRUE);
            gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (toggle_action), TRUE);
            break;
        default:
            ;
            break;
    }
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

static void
update_side_bar_radio_buttons (NemoWindow *window)
{
    GtkActionGroup *action_group;
    GtkAction *action;
    guint current_value;

    action_group = nemo_window_get_main_action_group (window);

    action = gtk_action_group_get_action (action_group,
                          "Sidebar Places");
    current_value = sidebar_id_to_value (nemo_window_get_sidebar_id (window));

    g_signal_handlers_block_by_func (action, sidebar_radio_entry_changed_cb, window);
    gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), current_value);
    g_signal_handlers_unblock_by_func (action, sidebar_radio_entry_changed_cb, window);
}

void
nemo_window_update_show_hide_menu_items (NemoWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = nemo_window_get_main_action_group (window);

	action = gtk_action_group_get_action (action_group,
					      NEMO_ACTION_SHOW_HIDE_EXTRA_PANE);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      nemo_window_split_view_showing (window));
	nemo_window_update_split_view_actions_sensitivity (window);

    update_side_bar_radio_buttons (window);
}

static void
action_add_bookmark_callback (GtkAction *action,
			      gpointer user_data)
{
    if (!NEMO_IS_DESKTOP_WINDOW (user_data)) {
        nemo_window_add_bookmark_for_current_location (NEMO_WINDOW (user_data));
    }
}

static void
action_edit_bookmarks_callback (GtkAction *action,
				gpointer user_data)
{
    if (!NEMO_IS_DESKTOP_WINDOW (user_data)) {
        nemo_window_edit_bookmarks (NEMO_WINDOW (user_data));
    }
}

static void
connect_proxy_cb (GtkActionGroup *action_group,
                  GtkAction *action,
                  GtkWidget *proxy,
                  NemoWindow *window)
{
    GtkWidget *label;

	if (!GTK_IS_MENU_ITEM (proxy))
		return;

    label = gtk_bin_get_child (GTK_BIN (proxy));

    if (GTK_IS_LABEL (label)) {
       gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
       gtk_label_set_max_width_chars (GTK_LABEL (label), MENU_ITEM_MAX_WIDTH_CHARS);
    }

	g_signal_connect (proxy, "select",
			  G_CALLBACK (menu_item_select_cb), window);
	g_signal_connect (proxy, "deselect",
			  G_CALLBACK (menu_item_deselect_cb), window);
}

static void
action_new_window_callback (GtkAction *action,
                            gpointer user_data)
{
    NemoWindow *current_window;

    current_window = NEMO_WINDOW (user_data);

    if (NEMO_IS_DESKTOP_WINDOW (current_window)) {
        NemoFile *file;
        NemoView *view;
        gchar *desktop_uri;

        desktop_uri = nemo_get_desktop_directory_uri ();

        file = nemo_file_get_existing_by_uri (desktop_uri);

        view = nemo_window_slot_get_current_view (nemo_window_get_active_slot (current_window));
        nemo_view_activate_file (view, file, 0);

        g_free (desktop_uri);
        nemo_file_unref (file);
    } else {
        NemoApplication *application;
        NemoWindow *new_window;
        gchar *uri;
        GFile *loc;

        uri = nemo_window_slot_get_current_uri (nemo_window_get_active_slot (current_window));

        if (eel_uri_is_search (uri)) {
            NemoDirectory *dir;
            NemoQuery *query;

            dir = nemo_directory_get_by_uri (uri);
            query = nemo_search_directory_get_query (NEMO_SEARCH_DIRECTORY (dir));

            if (query != NULL) {
                g_free (uri);

                uri = nemo_query_get_location (query);
                g_object_unref (query);
            }

            nemo_directory_unref (dir);
        }

        loc = g_file_new_for_uri (uri);

        application = nemo_application_get_singleton ();

        new_window = nemo_application_create_window (application,
                                                     gtk_window_get_screen (GTK_WINDOW (current_window)));

        nemo_window_slot_open_location (nemo_window_get_active_slot (new_window), loc, 0);

        g_object_unref (loc);
        g_free (uri);
    }
}

static void
action_new_tab_callback (GtkAction *action,
			 gpointer user_data)
{
	NemoWindow *window;

	window = NEMO_WINDOW (user_data);
	nemo_window_new_tab (window);
}

void action_toggle_location_entry_callback (GtkToggleAction *action, gpointer user_data);

static void
toggle_location_entry (NemoWindow     *window,
                       NemoWindowPane *pane,
                       gboolean        from_accel_or_menu)
{
    gboolean current_view, temp_toolbar_visible, default_toolbar_visible, grab_focus_only, already_has_focus;
    GtkToggleAction *button_action;
    GtkActionGroup *action_group;

    current_view = nemo_toolbar_get_show_location_entry (NEMO_TOOLBAR (pane->tool_bar));
    temp_toolbar_visible = pane->temporary_navigation_bar;
    default_toolbar_visible = g_settings_get_boolean (nemo_window_state,
                                                      NEMO_WINDOW_STATE_START_WITH_TOOLBAR);
    already_has_focus = nemo_location_bar_has_focus (NEMO_LOCATION_BAR (pane->location_bar));

    grab_focus_only = from_accel_or_menu && (pane->last_focus_widget == NULL || !already_has_focus) && current_view;

    if ((temp_toolbar_visible || default_toolbar_visible) && !grab_focus_only) {
        nemo_toolbar_set_show_location_entry (NEMO_TOOLBAR (pane->tool_bar), !current_view);

        action_group = pane->toolbar_action_group;
        button_action = GTK_TOGGLE_ACTION (gtk_action_group_get_action (action_group, NEMO_ACTION_TOGGLE_LOCATION));

        g_signal_handlers_block_by_func (button_action, action_toggle_location_entry_callback, window);
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (button_action), !current_view);
        g_signal_handlers_unblock_by_func (button_action, action_toggle_location_entry_callback, window);
    } else {
        nemo_window_pane_ensure_location_bar (pane);
    }
}

void
action_toggle_location_entry_callback (GtkToggleAction *action,
                                        gpointer user_data)
{
    NemoWindow *window = user_data;
    NemoWindowPane *pane;

    pane = nemo_window_get_active_pane (window);
    toggle_location_entry (window, pane, FALSE);
}

void nemo_window_show_location_entry (NemoWindow *window) {
	NemoWindowPane *pane;

    pane = nemo_window_get_active_pane (window);
    toggle_location_entry (window, pane, TRUE);
}

static void
action_menu_edit_location_callback (GtkAction *action,
				gpointer user_data)
{
	NemoWindow *window = user_data;
	NemoWindowPane *pane;

    if (!NEMO_IS_DESKTOP_WINDOW (user_data)) {
        pane = nemo_window_get_active_pane (window);
        toggle_location_entry (window, pane, TRUE);
    }
}

static void
action_show_thumbnails_callback (GtkAction * action,
                                 gpointer user_data)
{
    NemoWindowSlot *slot;
    NemoWindowPane *pane;
    NemoWindow *window;
    gboolean value;

    window = NEMO_WINDOW (user_data);

    slot = nemo_window_get_active_slot (window);
    pane = nemo_window_get_active_pane(window);

    value = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
    nemo_window_slot_set_show_thumbnails(slot, value);

    toolbar_set_show_thumbnails_button (value, pane);
    menu_set_show_thumbnails_action(value, window);
}

static void
set_content_view_type(NemoWindow *window,
                      const gchar *view_id)
{
    NemoWindowSlot *slot;

    slot = nemo_window_get_active_slot (window);
    nemo_window_slot_set_content_view (slot, view_id);
}

static void
action_icon_view_callback (GtkAction *action,
                           gpointer user_data)
{
    NemoWindow *window;

    window = NEMO_WINDOW (user_data);

    set_content_view_type (window, NEMO_ICON_VIEW_ID);
}


static void
action_list_view_callback (GtkAction *action,
                           gpointer user_data)
{
    NemoWindow *window;

    window = NEMO_WINDOW (user_data);

    set_content_view_type (window, NEMO_LIST_VIEW_ID);
}


static void
action_compact_view_callback (GtkAction *action,
                           gpointer user_data)
{
    NemoWindow *window;

    window = NEMO_WINDOW (user_data);

    set_content_view_type (window, FM_COMPACT_VIEW_ID);
}

guint
action_for_view_id (const char *view_id)
{
    if (g_strcmp0(view_id, NEMO_ICON_VIEW_ID) == 0) {
        return ICON_VIEW;
    } else if (g_strcmp0(view_id, NEMO_LIST_VIEW_ID) == 0) {
        return LIST_VIEW;
    } else if (g_strcmp0(view_id, FM_COMPACT_VIEW_ID) == 0) {
        return COMPACT_VIEW;
    } else {
        return NULL_VIEW;
    }
}

void
toolbar_set_view_button (guint action_id, NemoWindow *window)
{
    GtkAction *action, *action1, *action2;
    GtkActionGroup *action_group;
    if (action_id == NULL_VIEW) {
        return;
    }
    action_group = nemo_window_pane_get_toolbar_action_group (nemo_window_get_active_pane (window));

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
                         window);

    g_signal_handlers_block_matched (action1,
                         G_SIGNAL_MATCH_FUNC,
                         0, 0,
                         NULL,
                         action_list_view_callback,
                         window);
    g_signal_handlers_block_matched (action2,
                         G_SIGNAL_MATCH_FUNC,
                         0, 0,
                         NULL,
                         action_compact_view_callback,
                         window);

    if (action_id != ICON_VIEW) {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), FALSE);
    } else {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), TRUE);
    }

    if (action_id != LIST_VIEW) {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action1), FALSE);
    } else {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action1), TRUE);
    }

    if (action_id != COMPACT_VIEW) {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action2), FALSE);
    } else {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action2), TRUE);
    }

    g_signal_handlers_unblock_matched (action,
                           G_SIGNAL_MATCH_FUNC,
                           0, 0,
                           NULL,
                           action_icon_view_callback,
                           window);


    g_signal_handlers_unblock_matched (action1,
                           G_SIGNAL_MATCH_FUNC,
                           0, 0,
                           NULL,
                           action_list_view_callback,
                           window);


    g_signal_handlers_unblock_matched (action2,
                           G_SIGNAL_MATCH_FUNC,
                           0, 0,
                           NULL,
                           action_compact_view_callback,
                           window);

}

void
toolbar_set_show_thumbnails_button (gboolean value, NemoWindowPane *pane)
{
    GtkAction *action;
    GtkActionGroup *action_group;

    action_group = nemo_window_pane_get_toolbar_action_group (pane);


    action = gtk_action_group_get_action(action_group,
                                         NEMO_ACTION_SHOW_THUMBNAILS);

    g_signal_handlers_block_matched (action,
                         G_SIGNAL_MATCH_FUNC,
                         0, 0,
                         NULL,
                         action_show_thumbnails_callback,
                         NULL);

    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), value);

    g_signal_handlers_unblock_matched (action,
                           G_SIGNAL_MATCH_FUNC,
                           0, 0,
                           NULL,
                           action_show_thumbnails_callback,
                           NULL);
}

void
menu_set_show_thumbnails_action (gboolean value, NemoWindow *window)
{
    GtkAction *action;

    action = gtk_action_group_get_action (window->details->main_action_group,
                                          NEMO_ACTION_SHOW_THUMBNAILS);

    g_signal_handlers_block_matched (action,
                         G_SIGNAL_MATCH_FUNC,
                         0, 0,
                         NULL,
                         action_show_thumbnails_callback,
                         NULL);

    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), value);

    g_signal_handlers_unblock_matched (action,
                           G_SIGNAL_MATCH_FUNC,
                           0, 0,
                           NULL,
                           action_show_thumbnails_callback,
                           NULL);
}

void
toolbar_set_create_folder_button (gboolean value, NemoWindowPane *pane)
{
    GtkActionGroup *action_group;
    GtkAction *action;

    action_group = nemo_window_pane_get_toolbar_action_group (pane);

    action = gtk_action_group_get_action(action_group,
                                         NEMO_ACTION_NEW_FOLDER);

    gtk_action_set_sensitive (action, value);
}

void
menu_set_view_selection (guint action_id,
                         NemoWindow *window)
{
    GtkAction *action;

    if (action_id == NULL_VIEW) {
        return;
    }

    g_signal_handlers_block_by_func (window->details->main_action_group,
                                     view_radio_entry_changed_cb,
                                     window);

    action = gtk_action_group_get_action (window->details->main_action_group,
                                          NEMO_ACTION_ICON_VIEW);

    gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), action_id);

    g_signal_handlers_unblock_by_func (window->details->main_action_group,
                                       view_radio_entry_changed_cb,
                                       window);
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
reorder_tab (NemoWindowPane *pane, int offset)
{
	int page_num;

	g_return_if_fail (pane != NULL);

	page_num = gtk_notebook_get_current_page (
		GTK_NOTEBOOK (pane->notebook));
	g_return_if_fail (page_num != -1);
	nemo_notebook_reorder_child_relative (
		NEMO_NOTEBOOK (pane->notebook), page_num, offset);
}

static void
action_tabs_move_left_callback (GtkAction *action,
				gpointer user_data)
{
	NemoWindow *window = user_data;
	reorder_tab (nemo_window_get_active_pane (window), -1);
}

static void
action_tabs_move_right_callback (GtkAction *action,
				 gpointer user_data)
{
	NemoWindow *window = user_data;
	reorder_tab (nemo_window_get_active_pane (window), 1);
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
action_new_folder_callback (GtkAction *action,
                            gpointer user_data)
{
    g_assert (NEMO_IS_WINDOW (user_data));
    NemoWindow *window = user_data;
    NemoView *view = get_current_view (window);

    nemo_view_new_folder (view);
}

static void
open_in_terminal_other (const gchar *path)
{
    gchar *gsetting_terminal;
    gchar **token;
    gchar **argv;
    gint i;

    gsetting_terminal = g_settings_get_string (gnome_terminal_preferences,
                                               GNOME_DESKTOP_TERMINAL_EXEC);

    token = g_strsplit (gsetting_terminal, " ", 0);
    argv = g_new (gchar *, g_strv_length (token) + 1);
    for (i = 0; token[i] != NULL; i++) {
        argv[i] = token[i];
    }
    argv[i] = NULL;

    g_spawn_async (path, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

    g_free (gsetting_terminal);
    g_strfreev (token);
    g_free (argv);
}

void
action_toggle_terminal_callback (GtkAction *action, gpointer callback_data)
{
    NemoWindow *window;
    NemoWindowSlot *slot;

    window = NEMO_WINDOW (callback_data);
    slot = nemo_window_get_active_slot (window);
    nemo_window_slot_toggle_terminal (slot);
}

static void
action_open_terminal_callback(GtkAction *action, gpointer callback_data)
{
    NemoWindow *window;
    NemoView *view;

    window = NEMO_WINDOW(callback_data);

    view = get_current_view (window);

    gchar *path;
    gchar *uri = nemo_view_get_uri (view);
    GFile *gfile = g_file_new_for_uri (uri);
    path = g_file_get_path (gfile);
    open_in_terminal_other (path);
    g_free (uri);
    g_free (path);
    g_object_unref (gfile);
}

#define NEMO_VIEW_MENUBAR_FILE_PATH                  "/MenuBar/File"

static void
on_file_menu_show (GtkWidget *widget, gpointer user_data)
{
    NemoWindow *window;
    NemoView *view;

    window = NEMO_WINDOW (user_data);
    view = get_current_view (window);

    nemo_view_update_actions_and_extensions (view);
}

static const GtkActionEntry main_entries[] = {
  /* name, stock id, label */  { "File", NULL, N_("_File") },
  /* name, stock id, label */  { "Edit", NULL, N_("_Edit") },
  /* name, stock id, label */  { "View", NULL, N_("_View") },
  /* name, stock id, label */  { "Help", NULL, N_("_Help") },
  /* name, stock id */         { "Close", "window-close-symbolic",
  /* label, accelerator */       N_("_Close"), "<control>W",
  /* tooltip */                  N_("Close this folder"),
                                 G_CALLBACK (action_close_window_slot_callback) },
                               { "Preferences", "preferences-other-symbolic",
                                 N_("Prefere_nces"),
                                 NULL, N_("Edit Nemo preferences"),
                                 G_CALLBACK (action_preferences_callback) },
#ifdef TEXT_CHANGE_UNDO
  /* name, stock id, label */  { "Undo", NULL, N_("_Undo"),
                                 "<control>Z", N_("Undo the last text change"),
                                 G_CALLBACK (action_undo_callback) },
#endif
  /* name, stock id, label */  { "Up", "go-up-symbolic", N_("Open _Parent"),
                                 "<alt>Up", N_("Open the parent folder"),
                                 G_CALLBACK (action_up_callback) },
  /* name, stock id, label */  { "UpAccel", NULL, "UpAccel",
                                 "", NULL,
                                 G_CALLBACK (action_up_callback) },
  /* name, stock id */         { "Stop", "process-stop-symbolic",
  /* label, accelerator */       N_("_Stop"), NULL,
  /* tooltip */                  N_("Stop loading the current location"),
                                 G_CALLBACK (action_stop_callback) },
  /* name, stock id */         { "Reload", "view-refresh-symbolic",
  /* label, accelerator */       N_("_Reload"), "<control>R",
  /* tooltip */                  N_("Reload the current location"),
                                 G_CALLBACK (action_reload_callback) },
  /* name, stock id */         { "NemoHelp", "help-contents-symbolic",
  /* label, accelerator */       N_("_All Topics"), "F1",
  /* tooltip */                  N_("Display Nemo help"),
                                 G_CALLBACK (action_nemo_manual_callback) },
                               { "NemoShortcuts", "preferences-desktop-keyboard-shortcuts-symbolic",
                                 N_("_Keyboard Shortcuts"), "<control>F1",
                                 N_("Display keyboard shortcuts"),
                                 G_CALLBACK (action_show_shortcuts_window) },
  /** name, stock id          { "NemoHelpSearch", NULL,
     label, accelerator        N_("Search for files"), NULL,
     tooltip                   N_("Locate files based on file name and type. Save your searches for later use."),
                                 G_CALLBACK (action_nemo_manual_callback) },
     name, stock id          { "NemoHelpSort", NULL,
     label, accelerator        N_("Sort files and folders"), NULL,
     tooltip                   N_("Arrange files by name, size, type, or when they were changed."),
                                 G_CALLBACK (action_nemo_manual_callback) },
     name, stock id          { "NemoHelpLost", NULL,
     label, accelerator        N_("Find a lost file"), NULL,
     tooltip                   N_("Follow these tips if you can't find a file you created or downloaded."),
                                 G_CALLBACK (action_nemo_manual_callback) },
     name, stock id          { "NemoHelpShare", NULL,
     label, accelerator        N_("Share and transfer files"), NULL,
     tooltip                   N_("Easily transfer files to your contacts and devices from the file manager."),
                                 G_CALLBACK (action_nemo_manual_callback) }, **/
  /* name, stock id */         { "About Nemo", "help-about-symbolic",
  /* label, accelerator */       N_("_About"), NULL,
  /* tooltip */                  N_("Display credits for the creators of Nemo"),
                                 G_CALLBACK (action_about_nemo_callback) },
  /* name, stock id */         { "Zoom In", "zoom-in-symbolic",
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
  /* name, stock id */         { "Zoom Out", "zoom-out-symbolic",
  /* label, accelerator */       N_("Zoom _Out"), "<control>minus",
  /* tooltip */                  N_("Decrease the view size"),
                                 G_CALLBACK (action_zoom_out_callback) },
  /* name, stock id */         { "ZoomOutAccel", NULL,
  /* label, accelerator */       "ZoomOutAccel", "<control>KP_Subtract",
  /* tooltip */                  NULL,
                                 G_CALLBACK (action_zoom_out_callback) },
  /* name, stock id */         { "Zoom Normal", "zoom-original-symbolic",
  /* label, accelerator */       N_("Normal Si_ze"), "<control>0",
  /* tooltip */                  N_("Use the normal view size"),
                                 G_CALLBACK (action_zoom_normal_callback) },
  /* name, stock id */         { "Connect to Server", NULL,
  /* label, accelerator */       N_("Connect to _Server..."), NULL,
  /* tooltip */                  N_("Connect to a remote computer or shared disk"),
                                 G_CALLBACK (action_connect_to_server_callback) },
  /* name, stock id */         { "Home", NEMO_ICON_SYMBOLIC_HOME,
  /* label, accelerator */       N_("_Home"), "<alt>Home",
  /* tooltip */                  N_("Open your personal folder"),
                                 G_CALLBACK (action_home_callback) },
  /* name, stock id */         { "Go to Computer", NEMO_ICON_SYMBOLIC_COMPUTER,
  /* label, accelerator */       N_("_Computer"), NULL,
  /* tooltip */                  N_("Browse all local and remote disks and folders accessible from this computer"),
                                 G_CALLBACK (action_go_to_computer_callback) },
  /* name, stock id */         { "Go to Network", NEMO_ICON_SYMBOLIC_NETWORK,
  /* label, accelerator */       N_("_Network"), NULL,
  /* tooltip */                  N_("Browse bookmarked and local network locations"),
                                 G_CALLBACK (action_go_to_network_callback) },
  /* name, stock id */         { "Go to Templates", NEMO_ICON_SYMBOLIC_FOLDER_TEMPLATES,
  /* label, accelerator */       N_("T_emplates"), NULL,
  /* tooltip */                  N_("Open your personal templates folder"),
                                 G_CALLBACK (action_go_to_templates_callback) },
  /* name, stock id */         { "Go to Trash", NEMO_ICON_SYMBOLIC_TRASH,
  /* label, accelerator */       N_("_Trash"), NULL,
  /* tooltip */                  N_("Open your personal trash folder"),
                                 G_CALLBACK (action_go_to_trash_callback) },
  /* name, stock id, label */  { "Go", NULL, N_("_Go") },
  /* name, stock id, label */  { "Bookmarks", NULL, N_("_Bookmarks") },
  /* name, stock id, label */  { "Tabs", NULL, N_("_Tabs") },
  /* name, stock id, label */  { "New Window", NULL, N_("New _Window"),
                                 "<control>N", N_("Open another Nemo window for the displayed location"),
                                 G_CALLBACK (action_new_window_callback) },
  /* name, stock id, label */  { "New Tab", "tab-new-symbolic", N_("New _Tab"),
                                 "<control>T", N_("Open another tab for the displayed location"),
                                 G_CALLBACK (action_new_tab_callback) },
  /* name, stock id, label */  { "Close All Windows", NULL, N_("Close _All Windows"),
                                 "<control>Q", N_("Close all Navigation windows"),
                                 G_CALLBACK (action_close_all_windows_callback) },
  /* name, stock id, label */  { NEMO_ACTION_BACK, "go-previous-symbolic", N_("_Back"),
				 "<alt>Left", N_("Go to the previous visited location"),
				 G_CALLBACK (action_back_callback) },
  /* name, stock id, label */  { NEMO_ACTION_FORWARD, "go-next-symbolic", N_("_Forward"),
				 "<alt>Right", N_("Go to the next visited location"),
				 G_CALLBACK (action_forward_callback) },
  /* name, stock id, label */  { NEMO_ACTION_EDIT_LOCATION, NULL, N_("Toggle _Location Entry"),
                                 "<control>L", N_("Switch between location entry and breadcrumbs"),
                                 G_CALLBACK (action_menu_edit_location_callback) },
  /* name, stock id, label */  { "SplitViewNextPane", NULL, N_("S_witch to Other Pane"),
				 "F6", N_("Move focus to the other pane in a split view window"),
				 G_CALLBACK (action_split_view_switch_next_pane_callback) },
  /* name, stock id, label */  { "SplitViewSameLocation", NULL, N_("Sa_me Location as Other Pane"),
				 "<alt>S", N_("Go to the same location as in the extra pane"),
				 G_CALLBACK (action_split_view_same_location_callback) },
  /* name, stock id, label */  { "Add Bookmark", "bookmark-new-symbolic", N_("_Add Bookmark"),
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
  { "Sidebar List", NULL, N_("Sidebar") },
  { "Toolbar List", NULL, N_("Toolbar") }
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
  /* name, stock id */     { NEMO_ACTION_SHOW_HIDE_MENUBAR, NULL,
  /* label, accelerator */   N_("M_enubar"), NULL,
  /* tooltip */              N_("Change the default visibility of the menubar"),
                             NULL,
  /* is_active */            TRUE },
  /* name, stock id */     { NEMO_ACTION_SHOW_HIDE_TERMINAL, NULL,
  /* label, accelerator */   N_("Show Hide _Terminal"), "F4",
  /* tooltip */              N_("Toggle the visibility of the embedded terminal"),
  /* callback */             G_CALLBACK (action_toggle_terminal_callback),
  /* default */              FALSE },
  /* name, stock id */     { "Search", "edit-find-symbolic",
  /* label, accelerator */   N_("_Search for Files..."), "<control>f",
  /* tooltip */              N_("Search documents and folders"),
			     NULL,
  /* is_active */            FALSE },
  /* name, stock id */     { NEMO_ACTION_SHOW_HIDE_EXTRA_PANE, NULL,
  /* label, accelerator */   N_("E_xtra Pane"), "F3",
  /* tooltip */              N_("Open an extra folder view side-by-side"),
                             G_CALLBACK (action_split_view_callback),
  /* is_active */            FALSE },
    /* name, stock id */         { NEMO_ACTION_SHOW_THUMBNAILS, NULL,
  /* label, accelerator */       N_("Show _Thumbnails"), NULL,
  /* tooltip */                  N_("Toggle the display of thumbnails in the current directory"),
  /* callback */                 G_CALLBACK (action_show_thumbnails_callback),
  /* default */                  FALSE },
};

static const GtkRadioActionEntry sidebar_radio_entries[] = {
	{ "Sidebar Places", NULL,
	  N_("Places"), NULL, N_("Select Places as the default sidebar"),
	  SIDEBAR_PLACES },
	{ "Sidebar Tree", NULL,
	  N_("Tree"), NULL, N_("Select Tree as the default sidebar"),
	  SIDEBAR_TREE }
};

static const GtkRadioActionEntry view_radio_entries[] = {
    { "IconView", NULL,
      N_("Icon View"), "<ctrl>1", N_("Icon View"),
      ICON_VIEW },
    { "ListView", NULL,
      N_("List View"), "<ctrl>2", N_("List View"),
      LIST_VIEW },
    { "CompactView", NULL,
      N_("Compact View"), "<ctrl>3", N_("Compact View"),
      COMPACT_VIEW }
};

static const GtkRadioActionEntry toolbar_radio_entries[] = {
    { NEMO_ACTION_TOOLBAR_ALWAYS_SHOW_PATHBAR, NULL,
      N_("Path Bar"), NULL, N_("Always prefer the path bar"),
      TOOLBAR_PATHBAR },
    { NEMO_ACTION_TOOLBAR_ALWAYS_SHOW_ENTRY, NULL,
      N_("Location Entry"), NULL, N_("Always prefer the location entry"),
      TOOLBAR_ENTRY }
};

GtkActionGroup *
nemo_window_create_toolbar_action_group (NemoWindow *window)
{
    gboolean show_location_entry_initially;

	NemoNavigationState *navigation_state;
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = gtk_action_group_new ("ToolbarActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);

	action = g_object_new (NEMO_TYPE_NAVIGATION_ACTION,
			       "name", NEMO_ACTION_BACK,
			       "label", _("_Back"),
			       "icon_name", "go-previous-symbolic",
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
			       "icon_name", "go-next-symbolic",
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
   			       "icon_name", "go-up-symbolic",
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
   			       "icon_name", "view-refresh-symbolic",
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
   			       "icon_name", "go-home-symbolic",
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
   			       "icon_name", "computer-symbolic",
   			       "tooltip", _("Go to Computer"),
   			       "window", window,
   			       "direction", NEMO_NAVIGATION_DIRECTION_COMPUTER,
   			       NULL);
   	g_signal_connect (action, "activate",
   			  G_CALLBACK (action_go_to_computer_callback), window);
   	gtk_action_group_add_action (action_group, action);

   	g_object_unref (action);

    action = GTK_ACTION (gtk_toggle_action_new (NEMO_ACTION_TOGGLE_LOCATION,
                                                _("Location"),
                                                _("Toggle Location Entry"),
                                                NULL));
    gtk_action_group_add_action (action_group, GTK_ACTION (action));
    show_location_entry_initially = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_LOCATION_ENTRY);
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), show_location_entry_initially);
    g_signal_connect (action, "activate",
                      G_CALLBACK (action_toggle_location_entry_callback), window);
    gtk_action_set_icon_name (GTK_ACTION (action), "location-symbolic");

    g_object_unref (action);

    action = GTK_ACTION (gtk_action_new (NEMO_ACTION_NEW_FOLDER,
                                                _("New folder"),
                                                _("Create a new folder"),
                                                NULL));
    gtk_action_group_add_action (action_group, GTK_ACTION (action));
    g_signal_connect (action, "activate",
                      G_CALLBACK (action_new_folder_callback), window);
    gtk_action_set_icon_name (GTK_ACTION (action), "folder-new-symbolic");
    g_object_unref (action);

    action = GTK_ACTION (gtk_action_new (NEMO_ACTION_OPEN_IN_TERMINAL,
                                                _("Open in Terminal"),
                                                _("Open a terminal in the active folder"),
                                                NULL));
    gtk_action_group_add_action (action_group, GTK_ACTION (action));
    g_signal_connect (action, "activate",
                      G_CALLBACK (action_open_terminal_callback), window);
    gtk_action_set_icon_name (GTK_ACTION (action), "utilities-terminal-symbolic");
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
 				_("Search"),_("Search documents and folders"),
 				NULL));

  	gtk_action_group_add_action (action_group, action);
    gtk_action_set_icon_name (GTK_ACTION (action), "edit-find-symbolic");

  	g_object_unref (action);
    
    action = GTK_ACTION (gtk_toggle_action_new (NEMO_ACTION_SHOW_THUMBNAILS,
                         _("Show Thumbnails"),
                         _("Show Thumbnails"),
                         NULL));
   	g_signal_connect (action, "activate",
                      G_CALLBACK (action_show_thumbnails_callback),
                      window);
   	gtk_action_group_add_action (action_group, action);
    gtk_action_set_icon_name (GTK_ACTION (action), "xapp-prefs-preview-symbolic");

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
                          NEMO_ACTION_SHOW_HIDE_MENUBAR);

    g_settings_bind (nemo_window_state,
             NEMO_WINDOW_STATE_START_WITH_MENU_BAR,
             action,
             "active",
             G_SETTINGS_BIND_DEFAULT);

	action = gtk_action_group_get_action (action_group,
					      NEMO_ACTION_SHOW_HIDE_SIDEBAR);

    g_object_bind_property (window,
                            "show-sidebar",
                            action,
                            "active",
                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}

void
nemo_window_initialize_actions (NemoWindow *window)
{
	GtkActionGroup *action_group;
	const gchar *nav_state_actions[] = {
		NEMO_ACTION_BACK, NEMO_ACTION_FORWARD, NEMO_ACTION_UP, NEMO_ACTION_RELOAD, NEMO_ACTION_COMPUTER, NEMO_ACTION_HOME, NEMO_ACTION_EDIT_LOCATION,
		NEMO_ACTION_TOGGLE_LOCATION, NEMO_ACTION_SEARCH, NULL
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
      GtkAction *action_to_hide;
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

      /* if root then hide menu items that do not work */
    if (nemo_user_is_root () && !nemo_treating_root_as_normal ()) {
        action_to_hide = gtk_action_group_get_action (action_group, "Go to Templates");
        gtk_action_set_visible (action_to_hide, FALSE);
    }

    /* hide menu items that are not currently supported by the vfs */
    action_to_hide = gtk_action_group_get_action (action_group, "Go to Computer");
    gtk_action_set_visible (action_to_hide, eel_vfs_supports_uri_scheme ("computer"));
    action_to_hide = gtk_action_group_get_action (action_group, "Go to Trash");
    gtk_action_set_visible (action_to_hide, eel_vfs_supports_uri_scheme ("trash"));
    action_to_hide = gtk_action_group_get_action (action_group, "Go to Network");
    gtk_action_set_visible (action_to_hide, eel_vfs_supports_uri_scheme ("network"));

	gtk_action_group_add_toggle_actions (action_group,
					     main_toggle_entries, G_N_ELEMENTS (main_toggle_entries),
					     window);
	gtk_action_group_add_radio_actions (action_group,
					    sidebar_radio_entries, G_N_ELEMENTS (sidebar_radio_entries),
					    0, G_CALLBACK (sidebar_radio_entry_changed_cb),
					    window);
    gtk_action_group_add_radio_actions (action_group,
                        view_radio_entries, G_N_ELEMENTS (view_radio_entries),
                        0, G_CALLBACK (view_radio_entry_changed_cb),
                        window);

    gboolean use_entry = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_LOCATION_ENTRY);
    gtk_action_group_add_radio_actions (action_group,
                                        toolbar_radio_entries, G_N_ELEMENTS (toolbar_radio_entries),
                                        use_entry ? TOOLBAR_ENTRY : TOOLBAR_PATHBAR,
                                        G_CALLBACK (toolbar_radio_entry_changed_cb),
                                        window);

	action = gtk_action_group_get_action (action_group, NEMO_ACTION_UP);
	g_object_set (action, "short_label", _("_Up"), NULL);

	action = gtk_action_group_get_action (action_group, NEMO_ACTION_HOME);
	g_object_set (action, "short_label", _("_Home"), NULL);

  	action = gtk_action_group_get_action (action_group, NEMO_ACTION_EDIT_LOCATION);
  	g_object_set (action, "short_label", _("_Location"), NULL);

	action = gtk_action_group_get_action (action_group, NEMO_ACTION_SHOW_HIDDEN_FILES);

    if (NEMO_IS_DESKTOP_WINDOW (window)) {
        gtk_action_set_visible (action, FALSE);
    } else {
        g_signal_handlers_block_by_func (action, action_show_hidden_files_callback, window);
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                                      g_settings_get_boolean (nemo_preferences,
                                      NEMO_PREFERENCES_SHOW_HIDDEN_FILES));
        g_signal_handlers_unblock_by_func (action, action_show_hidden_files_callback, window);
    }

    /* Initialize Show Embedded Terminal toggle state */
    action = gtk_action_group_get_action (action_group, NEMO_ACTION_SHOW_HIDE_TERMINAL);
    g_signal_handlers_block_by_func (action, action_toggle_terminal_callback, window);
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                                  g_settings_get_boolean (nemo_window_state,
                                                       "terminal-visible"));
    g_signal_handlers_unblock_by_func (action, action_toggle_terminal_callback, window);

    g_signal_connect_object ( NEMO_WINDOW (window), "notify::sidebar-view-id",
                             G_CALLBACK (update_side_bar_radio_buttons), window, 0);

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

    GtkWidget *menuitem, *submenu;
    menuitem = gtk_ui_manager_get_widget (nemo_window_get_ui_manager (window), NEMO_VIEW_MENUBAR_FILE_PATH);
    submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menuitem));
    g_signal_connect (submenu, "show", G_CALLBACK (on_file_menu_show), window);

	nemo_window_initialize_trash_icon_monitor (window);
}

void
nemo_window_finalize_menus (NemoWindow *window)
{
	g_signal_handlers_disconnect_by_func (nemo_trash_monitor_get(),
					      trash_state_changed_cb, window);
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

		action = nemo_action_from_menu_item (item, GTK_WIDGET (window));
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

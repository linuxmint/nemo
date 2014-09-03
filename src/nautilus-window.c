/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000, 2004 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *  	     John Sullivan <sullivan@eazel.com>
 *           Alexander Larsson <alexl@redhat.com>
 */

/* nautilus-window.c: Implementation of the main window object */

#include <config.h>

#include "nautilus-window-private.h"

#include "nautilus-actions.h"
#include "nautilus-application.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-desktop-window.h"
#include "nautilus-location-entry.h"
#include "nautilus-mime-actions.h"
#include "nautilus-notebook.h"
#include "nautilus-pathbar.h"
#include "nautilus-properties-window.h"
#include "nautilus-toolbar.h"
#include "nautilus-window-slot.h"
#include "nautilus-list-view.h"
#include "nautilus-canvas-view.h"

#include <eel/eel-debug.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#ifdef HAVE_X11_XF86KEYSYM_H
#include <X11/XF86keysym.h>
#endif
#include <libnautilus-private/nautilus-dnd.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-profile.h>
#include <libnautilus-private/nautilus-clipboard.h>
#include <libnautilus-private/nautilus-signaller.h>
#include <libnautilus-private/nautilus-trash-monitor.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_WINDOW
#include <libnautilus-private/nautilus-debug.h>

#include <math.h>
#include <sys/time.h>

/* Forward and back buttons on the mouse */
static gboolean mouse_extra_buttons = TRUE;
static int mouse_forward_button = 9;
static int mouse_back_button = 8;

static void mouse_back_button_changed		     (gpointer                  callback_data);
static void mouse_forward_button_changed	     (gpointer                  callback_data);
static void use_extra_mouse_buttons_changed          (gpointer                  callback_data);

/* Sanity check: highest mouse button value I could find was 14. 5 is our 
 * lower threshold (well-documented to be the one of the button events for the 
 * scrollwheel), so it's hardcoded in the functions below. However, if you have
 * a button that registers higher and want to map it, file a bug and 
 * we'll move the bar. Makes you wonder why the X guys don't have 
 * defined values for these like the XKB stuff, huh?
 */
#define UPPER_MOUSE_LIMIT 14

enum {
	PROP_DISABLE_CHROME = 1,
	NUM_PROPERTIES,
};

enum {
	LOADING_URI,
	SLOT_ADDED,
	SLOT_REMOVED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NautilusWindow, nautilus_window, GTK_TYPE_APPLICATION_WINDOW);

static const struct {
	unsigned int keyval;
	const char *action;
} extra_window_keybindings [] = {
#ifdef HAVE_X11_XF86KEYSYM_H
	{ XF86XK_AddFavorite,	NAUTILUS_ACTION_ADD_BOOKMARK },
	{ XF86XK_Favorites,	NAUTILUS_ACTION_EDIT_BOOKMARKS },
	{ XF86XK_Go,		NAUTILUS_ACTION_ENTER_LOCATION },
	{ XF86XK_HomePage,      NAUTILUS_ACTION_GO_HOME },
	{ XF86XK_OpenURL,	NAUTILUS_ACTION_ENTER_LOCATION },
	{ XF86XK_Refresh,	NAUTILUS_ACTION_RELOAD },
	{ XF86XK_Reload,	NAUTILUS_ACTION_RELOAD },
	{ XF86XK_Search,	NAUTILUS_ACTION_SEARCH },
	{ XF86XK_Start,		NAUTILUS_ACTION_GO_HOME },
	{ XF86XK_Stop,		NAUTILUS_ACTION_STOP },
	{ XF86XK_ZoomIn,	NAUTILUS_ACTION_ZOOM_IN },
	{ XF86XK_ZoomOut,	NAUTILUS_ACTION_ZOOM_OUT },
	{ XF86XK_Back,		NAUTILUS_ACTION_BACK },
	{ XF86XK_Forward,	NAUTILUS_ACTION_FORWARD }

#endif
};

void
nautilus_window_go_to (NautilusWindow *window, GFile *location)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_window_slot_open_location (nautilus_window_get_active_slot (window),
					    location, 0);
}

void
nautilus_window_go_to_full (NautilusWindow *window,
			    GFile          *location,
			    NautilusWindowGoToCallback callback,
			    gpointer        user_data)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_window_slot_open_location_full (nautilus_window_get_active_slot (window),
						 location, 0, NULL, callback, user_data);
}

static int
bookmark_list_get_uri_index (GList *list, GFile *location)
{
	NautilusBookmark *bookmark;
	GList *l;
	GFile *tmp;
	int i;

	g_return_val_if_fail (location != NULL, -1);

	for (i = 0, l = list; l != NULL; i++, l = l->next) {
		bookmark = NAUTILUS_BOOKMARK (l->data);

		tmp = nautilus_bookmark_get_location (bookmark);
		if (g_file_equal (location, tmp)) {
			g_object_unref (tmp);
			return i;
		}
		g_object_unref (tmp);
	}

	return -1;
}

static void
unset_focus_widget (NautilusWindow *window)
{
	if (window->details->last_focus_widget != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (window->details->last_focus_widget),
					      (gpointer *) &window->details->last_focus_widget);
		window->details->last_focus_widget = NULL;
	}
}

static void
remember_focus_widget (NautilusWindow *window)
{
	GtkWidget *focus_widget;

	focus_widget = gtk_window_get_focus (GTK_WINDOW (window));
	if (focus_widget != NULL) {
		unset_focus_widget (window);

		window->details->last_focus_widget = focus_widget;
		g_object_add_weak_pointer (G_OBJECT (focus_widget),
					   (gpointer *) &(window->details->last_focus_widget));
	}
}

void
nautilus_window_grab_focus (NautilusWindow *window)
{
	NautilusWindowSlot *slot;
	NautilusView *view;

	slot = nautilus_window_get_active_slot (window);
	view = nautilus_window_slot_get_view (slot);

	if (view) {
		nautilus_view_grab_focus (view);
	}
}

static void
restore_focus_widget (NautilusWindow *window)
{
	if (window->details->last_focus_widget != NULL) {
		if (NAUTILUS_IS_VIEW (window->details->last_focus_widget)) {
			nautilus_view_grab_focus (NAUTILUS_VIEW (window->details->last_focus_widget));
		} else {
			gtk_widget_grab_focus (window->details->last_focus_widget);
		}

		unset_focus_widget (window);
	}
}

static void
location_entry_cancel_callback (GtkWidget      *widget,
				NautilusWindow *window)
{
	nautilus_toolbar_set_show_location_entry (NAUTILUS_TOOLBAR (window->details->toolbar), FALSE);

	restore_focus_widget (window);
}

static void
location_entry_location_changed_callback (GtkWidget      *widget,
					  GFile          *location,
					  NautilusWindow *window)
{
	nautilus_toolbar_set_show_location_entry (NAUTILUS_TOOLBAR (window->details->toolbar), FALSE);

	restore_focus_widget (window);

	nautilus_window_slot_open_location (window->details->active_slot, location, 0);
}

static void
notebook_switch_page_cb (GtkNotebook    *notebook,
			 GtkWidget      *page,
			 unsigned int    page_num,
			 NautilusWindow *window)
{
	NautilusWindowSlot *slot;
	GtkWidget *widget;

	widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (window->details->notebook), page_num);
	g_assert (widget != NULL);

	/* find slot corresponding to the target page */
	slot = NAUTILUS_WINDOW_SLOT (widget);
	g_assert (slot != NULL);

	nautilus_window_set_active_slot (nautilus_window_slot_get_window (slot),
					 slot);
}

static void
close_slot (NautilusWindow     *window,
	    NautilusWindowSlot *slot,
	    gboolean            remove_from_notebook)
{
	int page_num;
	GtkNotebook *notebook;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	DEBUG ("Closing slot %p", slot);

	window->details->slots = g_list_remove (window->details->slots, slot);

	g_signal_emit (window, signals[SLOT_REMOVED], 0, slot);

	notebook = GTK_NOTEBOOK (window->details->notebook);

	if (remove_from_notebook) {
		page_num = gtk_notebook_page_num (notebook, GTK_WIDGET (slot));
		g_assert (page_num >= 0);

		/* this will call gtk_widget_destroy on the slot */
		gtk_notebook_remove_page (notebook, page_num);
	}
}

NautilusWindowSlot *
nautilus_window_open_slot (NautilusWindow             *window,
			   NautilusWindowOpenSlotFlags flags)
{
	NautilusWindowSlot *slot;

	g_assert (NAUTILUS_IS_WINDOW (window));

	slot = nautilus_window_slot_new (window);

	g_signal_handlers_block_by_func (window->details->notebook,
					 G_CALLBACK (notebook_switch_page_cb),
					 window);
	nautilus_notebook_add_tab (NAUTILUS_NOTEBOOK (window->details->notebook),
				   slot,
				   (flags & NAUTILUS_WINDOW_OPEN_SLOT_APPEND) != 0 ?
				   -1 :
				   gtk_notebook_get_current_page (GTK_NOTEBOOK (window->details->notebook)) + 1,
				   FALSE);
	g_signal_handlers_unblock_by_func (window->details->notebook,
					   G_CALLBACK (notebook_switch_page_cb),
					   window);

	window->details->slots = g_list_append (window->details->slots, slot);
	g_signal_emit (window, signals[SLOT_ADDED], 0, slot);

	return slot;
}

void
nautilus_window_new_tab (NautilusWindow *window)
{
	NautilusWindowSlot *current_slot;
	NautilusWindowSlot *new_slot;
	NautilusWindowOpenFlags flags;
	GFile *location;
	int new_slot_position;
	char *scheme;

	current_slot = nautilus_window_get_active_slot (window);
	location = nautilus_window_slot_get_location (current_slot);

	if (location != NULL) {
		flags = 0;

		new_slot_position = g_settings_get_enum (nautilus_preferences, NAUTILUS_PREFERENCES_NEW_TAB_POSITION);
		if (new_slot_position == NAUTILUS_NEW_TAB_POSITION_END) {
			flags = NAUTILUS_WINDOW_OPEN_SLOT_APPEND;
		}

		scheme = g_file_get_uri_scheme (location);
		if (strcmp (scheme, "x-nautilus-search") == 0) {
			location = g_file_new_for_path (g_get_home_dir ());
		} else {
			g_object_ref (location);
		}

		g_free (scheme);

		new_slot = nautilus_window_open_slot (window, flags);
		nautilus_window_set_active_slot (window, new_slot);
		nautilus_window_slot_open_location (new_slot, location, 0);
		g_object_unref (location);
	}
}

static void
update_cursor (NautilusWindow *window)
{
	NautilusWindowSlot *slot;
	GdkCursor *cursor;

	slot = nautilus_window_get_active_slot (window);

	if (nautilus_window_slot_get_allow_stop (slot)) {
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), cursor);
		g_object_unref (cursor);
	} else {
		gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), NULL);
	}
}

void
nautilus_window_sync_allow_stop (NautilusWindow *window,
				 NautilusWindowSlot *slot)
{
	GtkAction *stop_action;
	GtkAction *reload_action;
	gboolean allow_stop, slot_is_active, slot_allow_stop;

	stop_action = gtk_action_group_get_action (nautilus_window_get_main_action_group (window),
						   NAUTILUS_ACTION_STOP);
	reload_action = gtk_action_group_get_action (nautilus_window_get_main_action_group (window),
						     NAUTILUS_ACTION_RELOAD);
	allow_stop = gtk_action_get_sensitive (stop_action);

	slot_allow_stop = nautilus_window_slot_get_allow_stop (slot);
	slot_is_active = (slot == nautilus_window_get_active_slot (window));

	if (!slot_is_active ||
	    allow_stop != slot_allow_stop) {
		if (slot_is_active) {
			gtk_action_set_visible (stop_action, slot_allow_stop);
			gtk_action_set_visible (reload_action, !slot_allow_stop);
		}

		if (gtk_widget_get_realized (GTK_WIDGET (window))) {
			update_cursor (window);
		}

		nautilus_notebook_sync_loading (NAUTILUS_NOTEBOOK (window->details->notebook), slot);
	}
}

void
nautilus_window_prompt_for_location (NautilusWindow *window,
				     GFile          *location)
{
	GtkWidget *entry;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));
	g_return_if_fail (G_IS_FILE (location));

	entry = nautilus_window_ensure_location_entry (window);
	nautilus_location_entry_set_location (NAUTILUS_LOCATION_ENTRY (entry), location);
}

/* Code should never force the window taller than this size.
 * (The user can still stretch the window taller if desired).
 */
static guint
get_max_forced_height (GdkScreen *screen)
{
	return (gdk_screen_get_height (screen) * 90) / 100;
}

/* Code should never force the window wider than this size.
 * (The user can still stretch the window wider if desired).
 */
static guint
get_max_forced_width (GdkScreen *screen)
{
	return (gdk_screen_get_width (screen) * 90) / 100;
}

/* This must be called when construction of NautilusWindow is finished,
 * since it depends on the type of the argument, which isn't decided at
 * construction time.
 */
static void
nautilus_window_set_initial_window_geometry (NautilusWindow *window)
{
	GdkScreen *screen;
	guint max_width_for_screen, max_height_for_screen;
	guint default_width, default_height;
	gboolean show_sidebar;
	GtkAction *action;

	screen = gtk_window_get_screen (GTK_WINDOW (window));
	
	max_width_for_screen = get_max_forced_width (screen);
	max_height_for_screen = get_max_forced_height (screen);
	
	default_width = NAUTILUS_WINDOW_DEFAULT_WIDTH;
	default_height = NAUTILUS_WINDOW_DEFAULT_HEIGHT;

	gtk_window_set_default_size (GTK_WINDOW (window), 
				     MIN (default_width, 
				          max_width_for_screen), 
				     MIN (default_height, 
				          max_height_for_screen));

	show_sidebar = g_settings_get_boolean (nautilus_window_state, NAUTILUS_WINDOW_STATE_START_WITH_SIDEBAR);
	action = gtk_action_group_get_action (window->details->main_action_group,
					      NAUTILUS_ACTION_SHOW_HIDE_SIDEBAR);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), show_sidebar);

	if (show_sidebar) {
		nautilus_window_show_sidebar (window);
	} else {
		nautilus_window_hide_sidebar (window);
	}
}

static gboolean
save_sidebar_width_cb (gpointer user_data)
{
	NautilusWindow *window = user_data;

	window->details->sidebar_width_handler_id = 0;

	DEBUG ("Saving sidebar width: %d", window->details->side_pane_width);

	g_settings_set_int (nautilus_window_state,
			    NAUTILUS_WINDOW_STATE_SIDEBAR_WIDTH,
			    window->details->side_pane_width);

	return FALSE;
}

/* side pane helpers */
static void
side_pane_size_allocate_callback (GtkWidget *widget,
				  GtkAllocation *allocation,
				  gpointer user_data)
{
	NautilusWindow *window;

	window = user_data;

	if (window->details->sidebar_width_handler_id != 0) {
		g_source_remove (window->details->sidebar_width_handler_id);
		window->details->sidebar_width_handler_id = 0;
	}

	if (allocation->width != window->details->side_pane_width &&
	    allocation->width > 1) {
		window->details->side_pane_width = allocation->width;

		window->details->sidebar_width_handler_id =
			g_idle_add (save_sidebar_width_cb, window);
	}
}

static void
setup_side_pane_width (NautilusWindow *window)
{
	g_return_if_fail (window->details->sidebar != NULL);

	window->details->side_pane_width =
		g_settings_get_int (nautilus_window_state,
				    NAUTILUS_WINDOW_STATE_SIDEBAR_WIDTH);

	gtk_paned_set_position (GTK_PANED (window->details->content_paned),
				window->details->side_pane_width);
}

/* Callback used when the places sidebar changes location; we need to change the displayed folder */
static void
places_sidebar_open_location_cb (GtkPlacesSidebar	*sidebar,
				 GFile			*location,
				 GtkPlacesOpenFlags	 open_flags,
				 gpointer		 user_data)
{
	NautilusWindow *window = NAUTILUS_WINDOW (user_data);
	NautilusWindowOpenFlags flags;

	switch (open_flags) {
	case GTK_PLACES_OPEN_NEW_TAB:
		flags = NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB;
		break;

	case GTK_PLACES_OPEN_NEW_WINDOW:
		flags = NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW;
		break;

	case GTK_PLACES_OPEN_NORMAL: /* fall-through */
	default:
		flags = 0;
		break;
	}

	nautilus_window_slot_open_location (window->details->active_slot, location, flags);
}

/* Callback used when the places sidebar needs us to present an error message */
static void
places_sidebar_show_error_message_cb (GtkPlacesSidebar *sidebar,
				      const char       *primary,
				      const char       *secondary,
				      gpointer          user_data)
{
	NautilusWindow *window = NAUTILUS_WINDOW (user_data);

	eel_show_error_dialog (primary, secondary, GTK_WINDOW (window));
}

/* Callback used when the places sidebar needs us to present the Connect to Server dialog */
static void
places_sidebar_show_connect_to_server_cb (GtkPlacesSidebar *sidebar,
					  gpointer          user_data)
{
	NautilusWindow *window = NAUTILUS_WINDOW (user_data);
	NautilusApplication *application = NAUTILUS_APPLICATION (g_application_get_default ());
	GtkWidget *dialog;
 
	dialog = nautilus_application_connect_server (application, window);
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
}

static GList *
build_selection_list_from_gfile_list (GList *gfile_list)
{
	GList *result;
	GList *l;

	result = NULL;
	for (l = gfile_list; l; l = l->next) {
		GFile *file;
		NautilusDragSelectionItem *item;

		file = l->data;

		item = nautilus_drag_selection_item_new ();
		item->uri = g_file_get_uri (file);
		item->file = nautilus_file_get_existing (file);
		item->got_icon_position = FALSE;
		result = g_list_prepend (result, item);
	}

	return g_list_reverse (result);
}

/* Callback used when the places sidebar needs to know the drag action to suggest */
static GdkDragAction
places_sidebar_drag_action_requested_cb (GtkPlacesSidebar *sidebar,
					 GdkDragContext   *context,
					 GFile            *dest_file,
					 GList            *source_file_list,
					 gpointer          user_data)
{
	GList *items;
	char *uri;
	int action = 0;

	items = build_selection_list_from_gfile_list (source_file_list);
	uri = g_file_get_uri (dest_file);

	nautilus_drag_default_drop_action_for_icons (context, uri, items, &action);

	nautilus_drag_destroy_selection_list (items);
	g_free (uri);

	return action;
}

/* Callback used when the places sidebar needs us to pop up a menu with possible drag actions */
static GdkDragAction
places_sidebar_drag_action_ask_cb (GtkPlacesSidebar *sidebar,
				   GdkDragAction     actions,
				   gpointer          user_data)
{
	return nautilus_drag_drop_action_ask (GTK_WIDGET (sidebar), actions);
}

static GList *
build_uri_list_from_gfile_list (GList *file_list)
{
	GList *result;
	GList *l;

	result = NULL;

	for (l = file_list; l; l = l->next) {
		GFile *file = l->data;
		char *uri;

		uri = g_file_get_uri (file);
		result = g_list_prepend (result, uri);
	}

	return g_list_reverse (result);
	
}

/* Callback used when the places sidebar has URIs dropped into it.  We do a normal file operation for them. */
static void
places_sidebar_drag_perform_drop_cb (GtkPlacesSidebar *sidebar,
				     GFile            *dest_file,
				     GList            *source_file_list,
				     GdkDragAction     action,
				     gpointer          user_data)
{
	char *dest_uri;
	GList *source_uri_list;

	dest_uri = g_file_get_uri (dest_file);
	source_uri_list = build_uri_list_from_gfile_list (source_file_list);

	nautilus_file_operations_copy_move (source_uri_list, NULL, dest_uri, action, GTK_WIDGET (sidebar), NULL, NULL);

	g_free (dest_uri);
	g_list_free_full (source_uri_list, g_free);
}

/* Callback for our own loading_uri signal.  We update the sidebar's path. */
static void
window_loading_uri_cb (NautilusWindow *window,
		       char           *location,
		       gpointer        user_data)
{
	if (window->details->places_sidebar) {
		GFile *file;

		file = g_file_new_for_uri (location);
		gtk_places_sidebar_set_location (GTK_PLACES_SIDEBAR (window->details->places_sidebar), file);
		g_object_unref (file);
	}
}

/* Callback used in the "empty trash" menu item from the places sidebar */
static void
empty_trash_cb (GtkMenuItem *item,
		gpointer     user_data)
{
	NautilusWindow *window = NAUTILUS_WINDOW (user_data);

	nautilus_file_operations_empty_trash (GTK_WIDGET (window));
}

/* Callback used for the "properties" menu item from the places sidebar */
static void
properties_cb (GtkMenuItem *item,
	       gpointer     user_data)
{
	NautilusWindow *window = NAUTILUS_WINDOW (user_data);
	GFile *selected;
	GList *list;
	NautilusFile *file;

	selected = gtk_places_sidebar_get_location (GTK_PLACES_SIDEBAR (window->details->places_sidebar));
	file = nautilus_file_get (selected);
	g_object_unref (selected);

	list = g_list_append (NULL, file);
	nautilus_properties_window_present (list, GTK_WIDGET (window), NULL);
	nautilus_file_list_free (list);
}

static gboolean
check_have_gnome_disks (void)
{
	gchar *disks_path;
	gboolean res;

	disks_path = g_find_program_in_path ("gnome-disks");
	res = (disks_path != NULL);
	g_free (disks_path);

	return res;
}

static gboolean
should_show_format_command (GVolume *volume)
{
	gchar *unix_device_id;
	gboolean show_format;

	unix_device_id = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
	show_format = (unix_device_id != NULL) && check_have_gnome_disks ();
	g_free (unix_device_id);

	return show_format;
}

static void
format_cb (GtkMenuItem *item,
	   gpointer     user_data)
{
	NautilusWindow *window = NAUTILUS_WINDOW (user_data);
	GAppInfo *app_info;
	gchar *cmdline, *device_identifier, *xid_string;
	GVolume *volume;
	gint xid;

	volume = g_object_get_data (G_OBJECT (item), "nautilus-volume");

	g_assert (volume != NULL && G_IS_VOLUME (volume));

	device_identifier = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
	xid = (gint) gdk_x11_window_get_xid (gtk_widget_get_window (GTK_WIDGET (window)));
	xid_string = g_strdup_printf ("%d", xid);

	cmdline = g_strconcat ("gnome-disks ",
			       "--block-device ", device_identifier, " ",
			       "--format-device ",
			       "--xid ", xid_string,
			       NULL);
	app_info = g_app_info_create_from_commandline (cmdline, NULL, 0, NULL);
	g_app_info_launch (app_info, NULL, NULL, NULL);

	g_free (cmdline);
	g_free (device_identifier);
	g_free (xid_string);
	g_clear_object (&app_info);
}

/* Destroy notification function used from g_object_set_data_full() */
static void
menu_item_destroy_notify_cb (gpointer data)
{
	GVolume *volume;

	volume = G_VOLUME (data);
	g_object_unref (volume);
}

static void
places_sidebar_populate_popup_cb (GtkPlacesSidebar *sidebar,
				  GtkMenu          *menu,
				  GFile            *selected_item,
				  GVolume          *selected_volume,
				  gpointer          user_data)
{
	NautilusWindow *window = NAUTILUS_WINDOW (user_data);
	GtkWidget *item;
	GFile *trash;

	if (selected_item) {
		trash = g_file_new_for_uri ("trash:///");
		if (g_file_equal (trash, selected_item)) {
			eel_gtk_menu_append_separator (menu);

			item = gtk_menu_item_new_with_mnemonic (_("Empty _Trash"));
			g_signal_connect (item, "activate",
					  G_CALLBACK (empty_trash_cb), window);
			gtk_widget_show (item);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

			if (nautilus_trash_monitor_is_empty ())
				gtk_widget_set_sensitive (item, FALSE);
		}
		g_object_unref (trash);

		if (g_file_is_native (selected_item)) {
			eel_gtk_menu_append_separator (menu);

			item = gtk_menu_item_new_with_mnemonic (_("_Properties"));
			g_signal_connect (item, "activate",
					  G_CALLBACK (properties_cb), window);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
			gtk_widget_show (item);
		}
	}
	if (selected_volume) {
		if (should_show_format_command (selected_volume)) {
			item = gtk_menu_item_new_with_mnemonic (_("_Format…"));
			g_object_set_data_full (G_OBJECT (item), "nautilus-volume",
						g_object_ref (selected_volume),
						menu_item_destroy_notify_cb);
			g_signal_connect (item, "activate",
					  G_CALLBACK (format_cb), window);
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
			gtk_widget_show (item);
		}
	}
}

static void
nautilus_window_set_up_sidebar (NautilusWindow *window)
{
	window->details->sidebar = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_style_context_add_class (gtk_widget_get_style_context (window->details->sidebar),
				     GTK_STYLE_CLASS_SIDEBAR);

	gtk_paned_pack1 (GTK_PANED (window->details->content_paned),
			 GTK_WIDGET (window->details->sidebar),
			 FALSE, FALSE);

	setup_side_pane_width (window);
	g_signal_connect (window->details->sidebar, 
			  "size-allocate",
			  G_CALLBACK (side_pane_size_allocate_callback),
			  window);

	window->details->places_sidebar = gtk_places_sidebar_new ();
	gtk_places_sidebar_set_open_flags (GTK_PLACES_SIDEBAR (window->details->places_sidebar),
					   (GTK_PLACES_OPEN_NORMAL
					    | GTK_PLACES_OPEN_NEW_TAB
					    | GTK_PLACES_OPEN_NEW_WINDOW));
	gtk_places_sidebar_set_show_connect_to_server (GTK_PLACES_SIDEBAR (window->details->places_sidebar), TRUE);

	g_signal_connect (window->details->places_sidebar, "open-location",
			  G_CALLBACK (places_sidebar_open_location_cb), window);
	g_signal_connect (window->details->places_sidebar, "show-error-message",
			  G_CALLBACK (places_sidebar_show_error_message_cb), window);
	g_signal_connect (window->details->places_sidebar, "show-connect-to-server",
			  G_CALLBACK (places_sidebar_show_connect_to_server_cb), window);
	g_signal_connect (window->details->places_sidebar, "drag-action-requested",
			  G_CALLBACK (places_sidebar_drag_action_requested_cb), window);
	g_signal_connect (window->details->places_sidebar, "drag-action-ask",
			  G_CALLBACK (places_sidebar_drag_action_ask_cb), window);
	g_signal_connect (window->details->places_sidebar, "drag-perform-drop",
			  G_CALLBACK (places_sidebar_drag_perform_drop_cb), window);
	g_signal_connect (window->details->places_sidebar, "populate-popup",
			  G_CALLBACK (places_sidebar_populate_popup_cb), window);

	g_signal_connect (window, "loading-uri",
			  G_CALLBACK (window_loading_uri_cb), window);

	gtk_box_pack_start (GTK_BOX (window->details->sidebar), window->details->places_sidebar, TRUE, TRUE, 0);
	gtk_widget_show (window->details->places_sidebar);
	gtk_widget_show (window->details->sidebar);
}

static void
nautilus_window_tear_down_sidebar (NautilusWindow *window)
{
	DEBUG ("Destroying sidebar");

	if (window->details->sidebar != NULL) {
		gtk_widget_destroy (GTK_WIDGET (window->details->sidebar));
		window->details->sidebar = NULL;
		window->details->places_sidebar = NULL;
	}
}

void
nautilus_window_hide_sidebar (NautilusWindow *window)
{
	DEBUG ("Called hide_sidebar()");

	if (window->details->sidebar == NULL) {
		return;
	}

	nautilus_window_tear_down_sidebar (window);
}

void
nautilus_window_show_sidebar (NautilusWindow *window)
{
	DEBUG ("Called show_sidebar()");

	if (window->details->sidebar != NULL) {
		return;
	}

	if (window->details->disable_chrome) {
		return;
	}

	nautilus_window_set_up_sidebar (window);
}

gboolean
nautilus_window_disable_chrome_mapping (GValue *value,
					GVariant *variant,
					gpointer user_data)
{
	NautilusWindow *window = user_data;

	g_value_set_boolean (value,
			     g_variant_get_boolean (variant) &&
			     !window->details->disable_chrome);

	return TRUE;
}

static inline NautilusWindowSlot *
get_first_inactive_slot (NautilusWindow *window)
{
	GList *l;
	NautilusWindowSlot *slot;

	for (l = window->details->slots; l != NULL; l = l->next) {
		slot = NAUTILUS_WINDOW_SLOT (l->data);
		if (slot != window->details->active_slot) {
			return slot;
		}
	}

	return NULL;
}

void
nautilus_window_slot_close (NautilusWindow     *window,
			    NautilusWindowSlot *slot)
{
	NautilusWindowSlot *next_slot;

	DEBUG ("Requesting to remove slot %p from window %p", slot, window);
	if (window == NULL)
		return;

	if (window->details->active_slot == slot) {
		next_slot = get_first_inactive_slot (window);
		nautilus_window_set_active_slot (window, next_slot);
	}

	close_slot (window, slot, TRUE);

	/* If that was the last slot in the window, close the window. */
	if (window->details->slots == NULL) {
		DEBUG ("Last slot removed, closing the window");
		nautilus_window_close (window);
	}
}

static void
nautilus_window_sync_bookmarks (NautilusWindow *window)
{
	gboolean can_bookmark = FALSE;
	NautilusWindowSlot *slot;
	NautilusBookmarkList *bookmarks;
	GtkAction *action;
	GFile *location;

	slot = window->details->active_slot;
	location = nautilus_window_slot_get_location (slot);

	if (location != NULL) {
		bookmarks = nautilus_application_get_bookmarks
			(NAUTILUS_APPLICATION (gtk_window_get_application (GTK_WINDOW (window))));
		can_bookmark = nautilus_bookmark_list_can_bookmark_location (bookmarks, location);
	}

	action = gtk_action_group_get_action (nautilus_window_get_main_action_group (window),
					      NAUTILUS_ACTION_ADD_BOOKMARK);
	gtk_action_set_sensitive (action, can_bookmark);
}

void
nautilus_window_sync_location_widgets (NautilusWindow *window)
{
	NautilusWindowSlot *slot, *active_slot;
	GFile *location;
	GtkActionGroup *action_group;
	GtkAction *action;

	slot = window->details->active_slot;
	location = nautilus_window_slot_get_location (slot);

	/* Change the location bar and path bar to match the current location. */
	if (location != NULL) {
		GtkWidget *location_entry;
		GtkWidget *path_bar;

		location_entry = nautilus_toolbar_get_location_entry (NAUTILUS_TOOLBAR (window->details->toolbar));
		if (location_entry != NULL && GTK_IS_WIDGET (location_entry)) {
			nautilus_location_entry_set_location (NAUTILUS_LOCATION_ENTRY (location_entry), location);
		}

		path_bar = nautilus_toolbar_get_path_bar (NAUTILUS_TOOLBAR (window->details->toolbar));
		nautilus_path_bar_set_path (NAUTILUS_PATH_BAR (path_bar), location);
	}

	nautilus_window_sync_up_button (window);

	/* Check if the back and forward buttons need enabling or disabling. */
	active_slot = nautilus_window_get_active_slot (window);
	action_group = nautilus_window_get_main_action_group (window);

	action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_BACK);
	gtk_action_set_sensitive (action, nautilus_window_slot_get_back_history (active_slot) != NULL);

	action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_FORWARD);
	gtk_action_set_sensitive (action, nautilus_window_slot_get_forward_history (active_slot) != NULL);

	nautilus_window_sync_bookmarks (window);
}

GtkWidget *
nautilus_window_ensure_location_entry (NautilusWindow *window)
{
	GtkWidget *location_entry;

	remember_focus_widget (window);

	nautilus_toolbar_set_show_location_entry (NAUTILUS_TOOLBAR (window->details->toolbar), TRUE);

	location_entry = nautilus_toolbar_get_location_entry (NAUTILUS_TOOLBAR (window->details->toolbar));
	nautilus_location_entry_focus (NAUTILUS_LOCATION_ENTRY (location_entry));

	return location_entry;
}

static void
path_bar_location_changed_callback (GtkWidget      *widget,
				    GFile          *location,
				    NautilusWindow *window)
{
	NautilusWindowSlot *slot;
	int i;

	slot = window->details->active_slot;
	/* check whether we already visited the target location */
	i = bookmark_list_get_uri_index (nautilus_window_slot_get_back_history (slot), location);
	if (i >= 0) {
		nautilus_window_back_or_forward (window, TRUE, i, 0);
	} else {
		nautilus_window_slot_open_location (slot, location, 0);
	}
}

static gboolean
path_bar_path_event_callback (NautilusPathBar *path_bar,
			      GFile           *location,
			      GdkEventButton  *event,
			      NautilusWindow  *window)
{
	NautilusWindowSlot *slot;
	NautilusWindowOpenFlags flags;
	int mask;
	NautilusView *view;
	char *uri;

	if (event->type == GDK_BUTTON_RELEASE) {
		mask = event->state & gtk_accelerator_get_default_mod_mask ();
		flags = 0;

		if (event->button == 2 && mask == 0) {
			flags = NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB;
		} else if (event->button == 1 && mask == GDK_CONTROL_MASK) {
			flags = NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW;
		}

		if (flags != 0) {
			slot = nautilus_window_get_active_slot (window);
			nautilus_window_slot_open_location (slot, location, flags);
		}

		return FALSE;
	}

	if (event->button == 3) {
		slot = nautilus_window_get_active_slot (window);
		view = nautilus_window_slot_get_view (slot);
		if (view != NULL) {
			uri = g_file_get_uri (location);
			nautilus_view_pop_up_location_context_menu (view, event, uri);
			g_free (uri);
		}

		return TRUE;
	} else if (event->button == 2) {
		return TRUE;
	}

	return FALSE;
}

static void
notebook_popup_menu_new_tab_cb (GtkMenuItem *menuitem,
				gpointer user_data)
{
	NautilusWindow *window = user_data;

	nautilus_window_new_tab (window);
}

static void
notebook_popup_menu_move_left_cb (GtkMenuItem *menuitem,
				  gpointer user_data)
{
	NautilusWindow *window = user_data;

	nautilus_notebook_reorder_current_child_relative (NAUTILUS_NOTEBOOK (window->details->notebook), -1);
}

static void
notebook_popup_menu_move_right_cb (GtkMenuItem *menuitem,
				   gpointer user_data)
{
	NautilusWindow *window = user_data;

	nautilus_notebook_reorder_current_child_relative (NAUTILUS_NOTEBOOK (window->details->notebook), 1);
}

static void
notebook_popup_menu_close_cb (GtkMenuItem *menuitem,
			      gpointer user_data)
{
	NautilusWindow *window = user_data;
	NautilusWindowSlot *slot;

	slot = window->details->active_slot;
	nautilus_window_slot_close (window, slot);
}

static void
notebook_popup_menu_show (NautilusWindow *window,
			  GdkEventButton *event)
{
	GtkWidget *popup;
	GtkWidget *item;
	int button, event_time;
	gboolean can_move_left, can_move_right;
	NautilusNotebook *notebook;

	notebook = NAUTILUS_NOTEBOOK (window->details->notebook);

	can_move_left = nautilus_notebook_can_reorder_current_child_relative (notebook, -1);
	can_move_right = nautilus_notebook_can_reorder_current_child_relative (notebook, 1);

	popup = gtk_menu_new ();

	item = gtk_menu_item_new_with_mnemonic (_("_New Tab"));
	g_signal_connect (item, "activate",
			  G_CALLBACK (notebook_popup_menu_new_tab_cb),
			  window);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup),
			       item);

	gtk_menu_shell_append (GTK_MENU_SHELL (popup),
			       gtk_separator_menu_item_new ());

	item = gtk_menu_item_new_with_mnemonic (_("Move Tab _Left"));
	g_signal_connect (item, "activate",
			  G_CALLBACK (notebook_popup_menu_move_left_cb),
			  window);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup),
			       item);
	gtk_widget_set_sensitive (item, can_move_left);

	item = gtk_menu_item_new_with_mnemonic (_("Move Tab _Right"));
	g_signal_connect (item, "activate",
			  G_CALLBACK (notebook_popup_menu_move_right_cb),
			  window);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup),
			       item);
	gtk_widget_set_sensitive (item, can_move_right);

	gtk_menu_shell_append (GTK_MENU_SHELL (popup),
			       gtk_separator_menu_item_new ());

	item = gtk_menu_item_new_with_mnemonic (_("_Close Tab"));
	g_signal_connect (item, "activate",
			  G_CALLBACK (notebook_popup_menu_close_cb), window);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup),
			       item);

	gtk_widget_show_all (popup);

	if (event) {
		button = event->button;
		event_time = event->time;
	} else {
		button = 0;
		event_time = gtk_get_current_event_time ();
	}

	/* TODO is this correct? */
	gtk_menu_attach_to_widget (GTK_MENU (popup),
				   window->details->notebook,
				   NULL);

	gtk_menu_popup (GTK_MENU (popup), NULL, NULL, NULL, NULL,
			button, event_time);
}

/* emitted when the user clicks the "close" button of tabs */
static void
notebook_tab_close_requested (NautilusNotebook   *notebook,
			      NautilusWindowSlot *slot,
			      NautilusWindow     *window)
{
	nautilus_window_slot_close (window, slot);
}

static gboolean
notebook_button_press_cb (GtkWidget *widget,
			  GdkEventButton *event,
			  gpointer user_data)
{
	NautilusWindow *window = user_data;

	if (GDK_BUTTON_PRESS == event->type && 3 == event->button) {
		notebook_popup_menu_show (window, event);
		return TRUE;
	}

	return FALSE;
}

static gboolean
notebook_popup_menu_cb (GtkWidget *widget,
			gpointer user_data)
{
	NautilusWindow *window = user_data;
	notebook_popup_menu_show (window, NULL);
	return TRUE;
}

static GtkWidget *
create_toolbar (NautilusWindow *window)
{
	GtkWidget *toolbar;
	GtkWidget *path_bar;
	GtkWidget *location_entry;

	/* build the toolbar */
	toolbar = nautilus_toolbar_new (NAUTILUS_WINDOW (window));

	g_object_bind_property (window, "disable-chrome",
				toolbar, "visible",
				G_BINDING_INVERT_BOOLEAN);

	/* connect to the pathbar signals */
	path_bar = nautilus_toolbar_get_path_bar (NAUTILUS_TOOLBAR (toolbar));

	g_signal_connect_object (path_bar, "path-clicked",
				 G_CALLBACK (path_bar_location_changed_callback), window, 0);
	g_signal_connect_object (path_bar, "path-event",
				 G_CALLBACK (path_bar_path_event_callback), window, 0);

	/* connect to the location entry signals */
	location_entry = nautilus_toolbar_get_location_entry (NAUTILUS_TOOLBAR (toolbar));

	nautilus_clipboard_set_up_editable (GTK_EDITABLE (location_entry),
					    nautilus_window_get_ui_manager (NAUTILUS_WINDOW (window)),
					    TRUE);

	g_signal_connect_object (location_entry, "location-changed",
				 G_CALLBACK (location_entry_location_changed_callback), window, 0);
	g_signal_connect_object (location_entry, "cancel",
				 G_CALLBACK (location_entry_cancel_callback), window, 0);

	return toolbar;
}

static void
notebook_page_removed_cb (GtkNotebook *notebook,
			  GtkWidget *page,
			  guint page_num,
			  gpointer user_data)
{
	NautilusWindow *window = user_data;
	NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (page), *next_slot;
	gboolean dnd_slot;

	dnd_slot = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (slot), "dnd-window-slot"));
	if (!dnd_slot) {
		return;
	}

	if (window->details->active_slot == slot) {
		next_slot = get_first_inactive_slot (window);
		nautilus_window_set_active_slot (window, next_slot);
	}

	close_slot (window, slot, FALSE);
}

static void
notebook_page_added_cb (GtkNotebook *notebook,
			GtkWidget *page,
			guint page_num,
			gpointer user_data)
{
	NautilusWindow *window = user_data;
	NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (page);
	NautilusWindowSlot *dummy_slot;
	gboolean dnd_slot;

	dnd_slot = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (slot), "dnd-window-slot"));
	if (!dnd_slot) {
		return;
	}

	g_object_set_data (G_OBJECT (page), "dnd-window-slot",
			   GINT_TO_POINTER (FALSE));

	nautilus_window_slot_set_window (slot, window);
	window->details->slots = g_list_append (window->details->slots, slot);
	g_signal_emit (window, signals[SLOT_ADDED], 0, slot);

	nautilus_window_set_active_slot (window, slot);

	dummy_slot = g_list_nth_data (window->details->slots, 0);
	if (dummy_slot != NULL) {
		close_slot (window, dummy_slot, TRUE);
	}

	gtk_widget_show (GTK_WIDGET (window));
}

static GtkNotebook *
notebook_create_window_cb (GtkNotebook *notebook,
			   GtkWidget *page,
			   gint x,
			   gint y,
			   gpointer user_data)
{
	NautilusApplication *app;
	NautilusWindow *new_window;
	NautilusWindowSlot *slot;

	if (!NAUTILUS_IS_WINDOW_SLOT (page)) {
		return NULL;
	}

	app = NAUTILUS_APPLICATION (g_application_get_default ());
	new_window = nautilus_application_create_window
		(app, gtk_widget_get_screen (GTK_WIDGET (notebook)));

	slot = NAUTILUS_WINDOW_SLOT (page);
	g_object_set_data (G_OBJECT (slot), "dnd-window-slot",
			   GINT_TO_POINTER (TRUE));

	gtk_window_set_position (GTK_WINDOW (new_window), GTK_WIN_POS_MOUSE);

	return GTK_NOTEBOOK (new_window->details->notebook);
}

static GtkWidget *
create_notebook (NautilusWindow *window)
{
	GtkWidget *notebook;

	notebook = g_object_new (NAUTILUS_TYPE_NOTEBOOK, NULL);
	g_signal_connect (notebook, "tab-close-request",
			  G_CALLBACK (notebook_tab_close_requested),
			  window);
	g_signal_connect (notebook, "popup-menu",
			  G_CALLBACK (notebook_popup_menu_cb),
			  window);
	g_signal_connect (notebook, "switch-page",
			  G_CALLBACK (notebook_switch_page_cb),
			  window);
	g_signal_connect (notebook, "create-window",
			  G_CALLBACK (notebook_create_window_cb),
			  window);
	g_signal_connect (notebook, "page-added",
			  G_CALLBACK (notebook_page_added_cb),
			  window);
	g_signal_connect (notebook, "page-removed",
			  G_CALLBACK (notebook_page_removed_cb),
			  window);
	g_signal_connect_after (notebook, "button-press-event",
				G_CALLBACK (notebook_button_press_cb),
				window);

	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
	gtk_widget_show (notebook);
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 0);

	gtk_box_pack_start (GTK_BOX (window->details->main_view),
			    notebook,
			    TRUE, TRUE, 0);

	return notebook;
}

static void
nautilus_window_constructed (GObject *self)
{
	NautilusWindow *window;
	GtkWidget *grid;
	NautilusWindowSlot *slot;
	NautilusApplication *application;

	window = NAUTILUS_WINDOW (self);

	nautilus_profile_start (NULL);

	G_OBJECT_CLASS (nautilus_window_parent_class)->constructed (self);

	application = NAUTILUS_APPLICATION (g_application_get_default ());
	gtk_window_set_application (GTK_WINDOW (window), GTK_APPLICATION (application));

	/* disable automatic menubar handling, since we show our regular
	 * menubar together with the app menu.
	 */
	gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (self), FALSE);

	grid = gtk_grid_new ();
	gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (grid);
	gtk_container_add (GTK_CONTAINER (window), grid);

	nautilus_window_initialize_menus (window);
	nautilus_window_initialize_actions (window);

	/* Register to menu provider extension signal managing menu updates */
	g_signal_connect_object (nautilus_signaller_get_current (), "popup-menu-changed",
			 G_CALLBACK (nautilus_window_load_extension_menus), window, G_CONNECT_SWAPPED);

	window->details->toolbar = create_toolbar (window);
	gtk_window_set_titlebar (GTK_WINDOW (window), window->details->toolbar);

	window->details->content_paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_hexpand (window->details->content_paned, TRUE);
	gtk_widget_set_vexpand (window->details->content_paned, TRUE);

	gtk_container_add (GTK_CONTAINER (grid), window->details->content_paned);
	gtk_widget_show (window->details->content_paned);

	window->details->main_view = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_paned_pack2 (GTK_PANED (window->details->content_paned), window->details->main_view,
			 TRUE, FALSE);
	gtk_widget_show (window->details->main_view);

	window->details->notebook = create_notebook (window);
	nautilus_window_set_initial_window_geometry (window);

	slot = nautilus_window_open_slot (window, 0);
	nautilus_window_set_active_slot (window, slot);

	window->details->bookmarks_id =
		g_signal_connect_swapped (nautilus_application_get_bookmarks (application), "changed",
					  G_CALLBACK (nautilus_window_sync_bookmarks), window);

	nautilus_profile_end (NULL);
}

static void
nautilus_window_set_property (GObject *object,
			      guint arg_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (object);
	
	switch (arg_id) {
	case PROP_DISABLE_CHROME:
		window->details->disable_chrome = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
		break;
	}
}

static void
nautilus_window_get_property (GObject *object,
			      guint arg_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (object);

	switch (arg_id) {
	case PROP_DISABLE_CHROME:
		g_value_set_boolean (value, window->details->disable_chrome);
		break;
	}
}

static void
destroy_slots_foreach (gpointer data,
		       gpointer user_data)
{
	NautilusWindowSlot *slot = data;
	NautilusWindow *window = user_data;

	close_slot (window, slot, TRUE);
}

static void
nautilus_window_destroy (GtkWidget *object)
{
	NautilusWindow *window;
	NautilusApplication *application;
	GList *slots_copy;

	window = NAUTILUS_WINDOW (object);

	DEBUG ("Destroying window");

	/* close the sidebar first */
	nautilus_window_tear_down_sidebar (window);

	/* close all slots safely */
	slots_copy = g_list_copy (window->details->slots);
	g_list_foreach (slots_copy, (GFunc) destroy_slots_foreach, window);
	g_list_free (slots_copy);

	/* the slots list should now be empty */
	g_assert (window->details->slots == NULL);

	window->details->active_slot = NULL;

	if (window->details->bookmarks_id != 0) {
		application = NAUTILUS_APPLICATION (gtk_window_get_application (GTK_WINDOW (window)));
		g_signal_handler_disconnect (nautilus_application_get_bookmarks (application),
					     window->details->bookmarks_id);
		window->details->bookmarks_id = 0;
	}

	GTK_WIDGET_CLASS (nautilus_window_parent_class)->destroy (object);
}

static void
nautilus_window_finalize (GObject *object)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (object);

	if (window->details->sidebar_width_handler_id != 0) {
		g_source_remove (window->details->sidebar_width_handler_id);
		window->details->sidebar_width_handler_id = 0;
	}

	g_clear_object (&window->details->ui_manager);

	/* nautilus_window_close() should have run */
	g_assert (window->details->slots == NULL);

	G_OBJECT_CLASS (nautilus_window_parent_class)->finalize (object);
}

void
nautilus_window_view_visible (NautilusWindow *window,
			      NautilusView   *view)
{
	NautilusWindowSlot *slot;
	GList *l;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	/* FIXME: this code is odd and should not really be needed, but
	 * removing it causes bugs, see e.g.
	 * https://bugzilla.gnome.org/show_bug.cgi?id=679640
	 *
	 * Needs more investigation...
	 */
	slot = nautilus_view_get_nautilus_window_slot (view);
	if (g_object_get_data (G_OBJECT (slot), "nautilus-window-view-visible") != NULL) {
		return;
	}

	g_object_set_data (G_OBJECT (slot), "nautilus-window-view-visible", GINT_TO_POINTER (1));

	/* Look for other non-visible slots */
	for (l = window->details->slots; l != NULL; l = l->next) {
		slot = l->data;
		if (g_object_get_data (G_OBJECT (slot), "nautilus-window-view-visible") == NULL) {
			return;
		}
	}

	/* Look for other non-visible slots */
	for (l = window->details->slots; l != NULL; l = l->next) {
		slot = l->data;
		nautilus_window_slot_update_title (slot);
	}

	nautilus_window_grab_focus (window);

	/* All slots, show window */
	gtk_widget_show (GTK_WIDGET (window));
}

static void
nautilus_window_save_geometry (NautilusWindow *window)
{
	char *geometry_string;
	gboolean is_maximized;

	g_assert (NAUTILUS_IS_WINDOW (window));

	if (gtk_widget_get_window (GTK_WIDGET (window))) {
		geometry_string = eel_gtk_window_get_geometry_string (GTK_WINDOW (window));
		is_maximized = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window)))
				& GDK_WINDOW_STATE_MAXIMIZED;

		if (!is_maximized) {
			g_settings_set_string
				(nautilus_window_state, NAUTILUS_WINDOW_STATE_GEOMETRY,
				 geometry_string);
		}
		g_free (geometry_string);

		g_settings_set_boolean
			(nautilus_window_state, NAUTILUS_WINDOW_STATE_MAXIMIZED,
			 is_maximized);
	}
}

void
nautilus_window_close (NautilusWindow *window)
{
	NAUTILUS_WINDOW_CLASS (G_OBJECT_GET_CLASS (window))->close (window);
}

/* reports location change to window's "loading-uri" clients, i.e.
 * sidebar panels [used when switching tabs]. It will emit the pending
 * location, or the existing location if none is pending.
 */
static void
nautilus_window_report_location_change (NautilusWindow *window)
{
	NautilusWindowSlot *slot;
	gchar *uri;

	slot = nautilus_window_get_active_slot (window);
	uri = nautilus_window_slot_get_current_uri (slot);

	if (uri != NULL) {
		g_signal_emit (window, signals[LOADING_URI], 0, uri);
		g_free (uri);
	}
}

void
nautilus_window_set_active_slot (NautilusWindow *window, NautilusWindowSlot *new_slot)
{
	NautilusWindowSlot *old_slot;
	NautilusView *view;

	g_assert (NAUTILUS_IS_WINDOW (window));

	DEBUG ("Setting new slot %p as active", new_slot);

	if (new_slot) {
		g_assert ((window == nautilus_window_slot_get_window (new_slot)));
	}

	old_slot = nautilus_window_get_active_slot (window);

	if (old_slot == new_slot) {
		return;
	}

	/* make old slot inactive if it exists (may be NULL after init, for example) */
	if (old_slot != NULL) {
		view = nautilus_window_slot_get_view (old_slot);
		if (view != NULL) {
			/* inform window */
			nautilus_window_disconnect_content_view (window, view);
		}

		/* inform slot & view */
		g_signal_emit_by_name (old_slot, "inactive");
	}

	window->details->active_slot = new_slot;

	/* make new slot active, if it exists */
	if (new_slot) {
		view = nautilus_window_slot_get_view (new_slot);
		if (view != NULL) {
                        /* inform window */
                        nautilus_window_connect_content_view (window, view);
                }

		/* inform sidebar panels */
                nautilus_window_report_location_change (window);

		/* inform slot & view */
                g_signal_emit_by_name (new_slot, "active");
	}
}

static void
nautilus_window_realize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (nautilus_window_parent_class)->realize (widget);
	update_cursor (NAUTILUS_WINDOW (widget));
}

static gboolean
nautilus_window_key_press_event (GtkWidget *widget,
				 GdkEventKey *event)
{
	NautilusWindow *window;
	NautilusWindowSlot *active_slot;
	NautilusView *view;
	GtkWidget *focus_widget;
	int i;

	window = NAUTILUS_WINDOW (widget);

	active_slot = nautilus_window_get_active_slot (window);
	view =  nautilus_window_slot_get_view (active_slot);

	if (view != NULL && nautilus_view_get_is_renaming (view)) {
		/* if we're renaming, just forward the event to the
		 * focused widget and return. We don't want to process the window
		 * accelerator bindings, as they might conflict with the 
		 * editable widget bindings.
		 */
		if (gtk_window_propagate_key_event (GTK_WINDOW (window), event)) {
			return TRUE;
		}
	}

	focus_widget = gtk_window_get_focus (GTK_WINDOW (window));
	if (view != NULL && focus_widget != NULL &&
	    GTK_IS_EDITABLE (focus_widget)) {
		/* if we have input focus on a GtkEditable (e.g. a GtkEntry), forward
		 * the event to it before activating accelerator bindings too.
		 */
		if (gtk_window_propagate_key_event (GTK_WINDOW (window), event)) {
			return TRUE;
		}
	}

	for (i = 0; i < G_N_ELEMENTS (extra_window_keybindings); i++) {
		if (extra_window_keybindings[i].keyval == event->keyval) {
			const GList *action_groups;
			GtkAction *action;

			action = NULL;

			action_groups = gtk_ui_manager_get_action_groups (window->details->ui_manager);
			while (action_groups != NULL && action == NULL) {
				action = gtk_action_group_get_action (action_groups->data, extra_window_keybindings[i].action);
				action_groups = action_groups->next;
			}

			g_assert (action != NULL);
			if (gtk_action_is_sensitive (action)) {
				gtk_action_activate (action);
				return TRUE;
			}

			break;
		}
	}

	if (GTK_WIDGET_CLASS (nautilus_window_parent_class)->key_press_event (widget, event)) {
		return TRUE;
	}

	if (nautilus_window_slot_handle_event (window->details->active_slot, event)) {
		return TRUE;
	}

	return FALSE;
}

void
nautilus_window_sync_up_button (NautilusWindow *window)
{
	GtkAction *action;
	GtkActionGroup *action_group;
	NautilusWindowSlot *slot;
	gboolean allowed;
	GFile *parent, *location;

	slot = nautilus_window_get_active_slot (window);
	location = nautilus_window_slot_get_location (slot);

	allowed = FALSE;
	if (location != NULL) {
		parent = g_file_get_parent (location);
		allowed = parent != NULL;

		g_clear_object (&parent);
	}

	action_group = nautilus_window_get_main_action_group (window);

	action = gtk_action_group_get_action (action_group,
					      NAUTILUS_ACTION_UP);
	gtk_action_set_sensitive (action, allowed);
}

void
nautilus_window_sync_title (NautilusWindow *window,
			    NautilusWindowSlot *slot)
{
	if (NAUTILUS_WINDOW_CLASS (G_OBJECT_GET_CLASS (window))->sync_title != NULL) {
		NAUTILUS_WINDOW_CLASS (G_OBJECT_GET_CLASS (window))->sync_title (window, slot);

		return;
	}

	if (slot == nautilus_window_get_active_slot (window)) {
		gtk_window_set_title (GTK_WINDOW (window), nautilus_window_slot_get_title (slot));
	}

	nautilus_notebook_sync_tab_label (NAUTILUS_NOTEBOOK (window->details->notebook), slot);
}

void
nautilus_window_sync_zoom_widgets (NautilusWindow *window)
{
	NautilusWindowSlot *slot;
	NautilusView *view;
	GtkActionGroup *action_group;
	GtkAction *action;
	gboolean supports_zooming;
	gboolean can_zoom, can_zoom_in, can_zoom_out;
	NautilusZoomLevel zoom_level;

	slot = nautilus_window_get_active_slot (window);
	view = nautilus_window_slot_get_view (slot);

	if (view != NULL) {
		supports_zooming = nautilus_view_supports_zooming (view);
		zoom_level = nautilus_view_get_zoom_level (view);
		can_zoom = supports_zooming &&
			   zoom_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
			   zoom_level <= NAUTILUS_ZOOM_LEVEL_LARGEST;
		can_zoom_in = can_zoom && nautilus_view_can_zoom_in (view);
		can_zoom_out = can_zoom && nautilus_view_can_zoom_out (view);
	} else {
		supports_zooming = FALSE;
		can_zoom = FALSE;
		can_zoom_in = FALSE;
		can_zoom_out = FALSE;
	}

	action_group = nautilus_window_get_main_action_group (window);

	action = gtk_action_group_get_action (action_group,
					      NAUTILUS_ACTION_ZOOM_IN);
	gtk_action_set_visible (action, supports_zooming);
	gtk_action_set_sensitive (action, can_zoom_in);
	
	action = gtk_action_group_get_action (action_group,
					      NAUTILUS_ACTION_ZOOM_OUT);
	gtk_action_set_visible (action, supports_zooming);
	gtk_action_set_sensitive (action, can_zoom_out);

	action = gtk_action_group_get_action (action_group,
					      NAUTILUS_ACTION_ZOOM_NORMAL);
	gtk_action_set_visible (action, supports_zooming);
	gtk_action_set_sensitive (action, can_zoom);
}

static void
zoom_level_changed_callback (NautilusView *view,
                             NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	/* This is called each time the component in
	 * the active slot successfully completed
	 * a zooming operation.
	 */
	nautilus_window_sync_zoom_widgets (window);
}


/* These are called
 *   A) when switching the view within the active slot
 *   B) when switching the active slot
 *   C) when closing the active slot (disconnect)
*/
void
nautilus_window_connect_content_view (NautilusWindow *window,
				      NautilusView *view)
{
	NautilusWindowSlot *slot;

	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (NAUTILUS_IS_VIEW (view));

	slot = nautilus_view_get_nautilus_window_slot (view);

	if (slot != nautilus_window_get_active_slot (window)) {
		return;
	}

	g_signal_connect (view, "zoom-level-changed",
			  G_CALLBACK (zoom_level_changed_callback),
			  window);
}

void
nautilus_window_disconnect_content_view (NautilusWindow *window,
					 NautilusView *view)
{
	NautilusWindowSlot *slot;

	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (NAUTILUS_IS_VIEW (view));

	slot = nautilus_view_get_nautilus_window_slot (view);

	if (slot != nautilus_window_get_active_slot (window)) {
		return;
	}

	g_signal_handlers_disconnect_by_func (view, G_CALLBACK (zoom_level_changed_callback), window);
}

/**
 * nautilus_window_show:
 * @widget:	GtkWidget
 *
 * Call parent and then show/hide window items
 * base on user prefs.
 */
static void
nautilus_window_show (GtkWidget *widget)
{	
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (widget);

	GTK_WIDGET_CLASS (nautilus_window_parent_class)->show (widget);	

	gtk_ui_manager_ensure_update (window->details->ui_manager);
}

GtkUIManager *
nautilus_window_get_ui_manager (NautilusWindow *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW (window), NULL);

	return window->details->ui_manager;
}

GtkActionGroup *
nautilus_window_get_main_action_group (NautilusWindow *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW (window), NULL);

	return window->details->main_action_group;
}

NautilusWindowSlot *
nautilus_window_get_active_slot (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	return window->details->active_slot;
}

GList *
nautilus_window_get_slots (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	return window->details->slots;
}

static gboolean
nautilus_window_state_event (GtkWidget *widget,
			     GdkEventWindowState *event)
{
	if (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) {
		g_settings_set_boolean (nautilus_window_state, NAUTILUS_WINDOW_STATE_MAXIMIZED,
					event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED);
	}

	if (GTK_WIDGET_CLASS (nautilus_window_parent_class)->window_state_event != NULL) {
		return GTK_WIDGET_CLASS (nautilus_window_parent_class)->window_state_event (widget, event);
	}

	return FALSE;
}

static gboolean
nautilus_window_delete_event (GtkWidget *widget,
			      GdkEventAny *event)
{
	nautilus_window_close (NAUTILUS_WINDOW (widget));
	return FALSE;
}

static gboolean
nautilus_window_button_press_event (GtkWidget *widget,
				    GdkEventButton *event)
{
	NautilusWindow *window;
	gboolean handled;

	window = NAUTILUS_WINDOW (widget);

	if (mouse_extra_buttons && (event->button == mouse_back_button)) {
		nautilus_window_back_or_forward (window, TRUE, 0, 0);
		handled = TRUE; 
	} else if (mouse_extra_buttons && (event->button == mouse_forward_button)) {
		nautilus_window_back_or_forward (window, FALSE, 0, 0);
		handled = TRUE;
	} else if (GTK_WIDGET_CLASS (nautilus_window_parent_class)->button_press_event) {
		handled = GTK_WIDGET_CLASS (nautilus_window_parent_class)->button_press_event (widget, event);
	} else {
		handled = FALSE;
	}
	return handled;
}

static void
mouse_back_button_changed (gpointer callback_data)
{
	int new_back_button;

	new_back_button = g_settings_get_int (nautilus_preferences, NAUTILUS_PREFERENCES_MOUSE_BACK_BUTTON);

	/* Bounds checking */
	if (new_back_button < 6 || new_back_button > UPPER_MOUSE_LIMIT)
		return;

	mouse_back_button = new_back_button;
}

static void
mouse_forward_button_changed (gpointer callback_data)
{
	int new_forward_button;

	new_forward_button = g_settings_get_int (nautilus_preferences, NAUTILUS_PREFERENCES_MOUSE_FORWARD_BUTTON);

	/* Bounds checking */
	if (new_forward_button < 6 || new_forward_button > UPPER_MOUSE_LIMIT)
		return;

	mouse_forward_button = new_forward_button;
}

static void
use_extra_mouse_buttons_changed (gpointer callback_data)
{
	mouse_extra_buttons = g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_MOUSE_USE_EXTRA_BUTTONS);
}


/*
 * Main API
 */

static void
nautilus_window_init (NautilusWindow *window)
{
	GtkWindowGroup *window_group;

	window->details = G_TYPE_INSTANCE_GET_PRIVATE (window, NAUTILUS_TYPE_WINDOW, NautilusWindowDetails);

	window->details->slots = NULL;
	window->details->active_slot = NULL;

	window_group = gtk_window_group_new ();
	gtk_window_group_add_window (window_group, GTK_WINDOW (window));
	g_object_unref (window_group);

	/* Set initial window title */
	gtk_window_set_title (GTK_WINDOW (window), _("Files"));
	gtk_window_set_icon_name (GTK_WINDOW (window), "system-file-manager");
}

static void
real_window_close (NautilusWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_window_save_geometry (window);

	gtk_widget_destroy (GTK_WIDGET (window));
}

static void
nautilus_window_class_init (NautilusWindowClass *class)
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (class);

	oclass->finalize = nautilus_window_finalize;
	oclass->constructed = nautilus_window_constructed;
	oclass->get_property = nautilus_window_get_property;
	oclass->set_property = nautilus_window_set_property;

	wclass->destroy = nautilus_window_destroy;
	wclass->show = nautilus_window_show;
	wclass->realize = nautilus_window_realize;
	wclass->key_press_event = nautilus_window_key_press_event;
	wclass->window_state_event = nautilus_window_state_event;
	wclass->button_press_event = nautilus_window_button_press_event;
	wclass->delete_event = nautilus_window_delete_event;

	class->close = real_window_close;

	properties[PROP_DISABLE_CHROME] =
		g_param_spec_boolean ("disable-chrome",
				      "Disable chrome",
				      "Disable window chrome, for the desktop",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				      G_PARAM_STATIC_STRINGS);

	signals[LOADING_URI] =
		g_signal_new ("loading-uri",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1,
			      G_TYPE_STRING);
	signals[SLOT_ADDED] =
		g_signal_new ("slot-added",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, NAUTILUS_TYPE_WINDOW_SLOT);
	signals[SLOT_REMOVED] =
		g_signal_new ("slot-removed",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, NAUTILUS_TYPE_WINDOW_SLOT);

	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_MOUSE_BACK_BUTTON,
				  G_CALLBACK(mouse_back_button_changed),
				  NULL);

	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_MOUSE_FORWARD_BUTTON,
				  G_CALLBACK(mouse_forward_button_changed),
				  NULL);

	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_MOUSE_USE_EXTRA_BUTTONS,
				  G_CALLBACK(use_extra_mouse_buttons_changed),
				  NULL);

	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
	g_type_class_add_private (oclass, sizeof (NautilusWindowDetails));
}

NautilusWindow *
nautilus_window_new (GdkScreen *screen)
{
	return g_object_new (NAUTILUS_TYPE_WINDOW,
			     "screen", screen,
			     NULL);
}

NautilusWindowOpenFlags
nautilus_event_get_window_open_flags (void)
{
	NautilusWindowOpenFlags flags = 0;
	GdkEvent *event;

	event = gtk_get_current_event ();

	if (event == NULL) {
		return flags;
	}

	if ((event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) &&
	    (event->button.button == 2)) {
		flags |= NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB;
	}

	gdk_event_free (event);

	return flags;
}

void
nautilus_window_show_about_dialog (NautilusWindow *window)
{
	const gchar *authors[] = {
		"Alexander Larsson",
		"Ali Abdin",
		"Anders Carlsson",
		"Andrew Walton",
		"Andy Hertzfeld",
		"Arlo Rose",
		"Christian Neumair",
		"Cosimo Cecchi",
		"Darin Adler",
		"David Camp",
		"Eli Goldberg",
		"Elliot Lee",
		"Eskil Heyn Olsen",
		"Ettore Perazzoli",
		"Gene Z. Ragan",
		"George Lebl",
		"Ian McKellar",
		"J Shane Culpepper",
		"James Willcox",
		"Jan Arne Petersen",
		"John Harper",
		"John Sullivan",
		"Josh Barrow",
		"Maciej Stachowiak",
		"Mark McLoughlin",
		"Mathieu Lacage",
		"Mike Engber",
		"Mike Fleming",
		"Pavel Cisler",
		"Ramiro Estrugo",
		"Raph Levien",
		"Rebecca Schulman",
		"Robey Pointer",
		"Robin * Slomkowski",
		"Seth Nickell",
		"Susan Kare",
		"Tomas Bzatek",
		"William Jon McCann",
		NULL
	};
	const gchar *documenters[] = {
		"GNOME Documentation Team",
		"Sun Microsystems",
		NULL
	};

	gtk_show_about_dialog (window ? GTK_WINDOW (window) : NULL,
			       "program-name", _("Files"),
			       "version", VERSION,
			       "comments", _("Access and organize your files."),
			       "copyright", "Copyright © 1999–2014 The Files Authors",
			       "license-type", GTK_LICENSE_GPL_2_0,
			       "authors", authors,
			       "documenters", documenters,
				/* Translators should localize the following string
				 * which will be displayed at the bottom of the about
				 * box to give credit to the translator(s).
				 */
			      "translator-credits", _("translator-credits"),
			      "logo-icon-name", "system-file-manager",
			      NULL);
}

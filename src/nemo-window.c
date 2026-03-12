/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nemo
 *
 *  Copyright (C) 1999, 2000, 2004 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
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
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *  	     John Sullivan <sullivan@eazel.com>
 *           Alexander Larsson <alexl@redhat.com>
 */

/* nemo-window.c: Implementation of the main window object */

#include <config.h>

#include "nemo-window-private.h"

#include "nemo-actions.h"
#include "nemo-application.h"
#include "nemo-bookmarks-window.h"
#include "nemo-desktop-window.h"
#include "nemo-location-bar.h"
#include "nemo-mime-actions.h"
#include "nemo-notebook.h"
#include "nemo-places-sidebar.h"
#include "nemo-tree-sidebar.h"
#include "nemo-view-factory.h"
#include "nemo-window-manage-views.h"
#include "nemo-window-bookmarks.h"
#include "nemo-window-slot.h"
#include "nemo-window-menus.h"
#include "nemo-icon-view.h"
#include "nemo-list-view.h"
#include "nemo-statusbar.h"

#include <eel/eel-debug.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#ifdef HAVE_X11_XF86KEYSYM_H
#include <X11/XF86keysym.h>
#endif
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-file-attributes.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-metadata.h>
#include <libnemo-private/nemo-clipboard.h>
#include <libnemo-private/nemo-undo.h>
#include <libnemo-private/nemo-search-directory.h>
#include <libnemo-private/nemo-signaller.h>

#define DEBUG_FLAG NEMO_DEBUG_WINDOW
#include <libnemo-private/nemo-debug.h>

#include <math.h>
#include <sys/time.h>

#define MAX_TITLE_LENGTH 180

/* Forward and back buttons on the mouse */
static gboolean mouse_extra_buttons = TRUE;
static guint mouse_forward_button = 9;
static guint mouse_back_button = 8;

static void mouse_back_button_changed		     (gpointer                  callback_data);
static void mouse_forward_button_changed	     (gpointer                  callback_data);
static void use_extra_mouse_buttons_changed          (gpointer              callback_data);
static void side_pane_id_changed                    (NemoWindow            *window);
static void toggle_menubar                          (NemoWindow            *window,
                                                     gint                   action);
static void nemo_window_reload                      (NemoWindow            *window);
static void nemo_window_update_split_view_orientation (NemoWindow *window);
static void nemo_window_set_up_sidebar2             (NemoWindow *window);
static void nemo_window_tear_down_sidebar2          (NemoWindow *window);
static void nemo_window_set_up_pane1_wrapper        (NemoWindow *window);
static void nemo_window_refresh_sidebar1_pane_lock  (NemoWindow *window, gboolean lock_to_pane1);
static void nemo_window_tear_down_pane1_wrapper     (NemoWindow *window);
static void dual_pane_prefs_changed                 (gpointer callback_data);
static void nemo_window_refresh_sidebar_colours     (NemoWindow *window);

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
    PROP_SIDEBAR_VIEW_TYPE,
    PROP_SHOW_SIDEBAR,
	NUM_PROPERTIES,
};

enum {
	GO_UP,
	RELOAD,
	PROMPT_FOR_LOCATION,
	LOADING_URI,
	HIDDEN_FILES_MODE_CHANGED,
	SLOT_ADDED,
	SLOT_REMOVED,
	LAST_SIGNAL
};

enum {
    MENU_HIDE,
    MENU_SHOW,
    MENU_TOGGLE
};

static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NemoWindow, nemo_window, GTK_TYPE_APPLICATION_WINDOW);

static const struct {
	unsigned int keyval;
	const char *action;
} extra_window_keybindings [] = {
#ifdef HAVE_X11_XF86KEYSYM_H
	{ XF86XK_AddFavorite,	NEMO_ACTION_ADD_BOOKMARK },
	{ XF86XK_Favorites,	NEMO_ACTION_EDIT_BOOKMARKS },
	{ XF86XK_Go,		NEMO_ACTION_EDIT_LOCATION },
	{ XF86XK_HomePage,      NEMO_ACTION_GO_HOME },
	{ XF86XK_OpenURL,	NEMO_ACTION_EDIT_LOCATION },
	{ XF86XK_Refresh,	NEMO_ACTION_RELOAD },
	{ XF86XK_Reload,	NEMO_ACTION_RELOAD },
	{ XF86XK_Search,	NEMO_ACTION_SEARCH },
	{ XF86XK_Start,		NEMO_ACTION_GO_HOME },
	{ XF86XK_Stop,		NEMO_ACTION_STOP },
	{ XF86XK_ZoomIn,	NEMO_ACTION_ZOOM_IN },
	{ XF86XK_ZoomOut,	NEMO_ACTION_ZOOM_OUT },
	{ XF86XK_Back,		NEMO_ACTION_BACK },
	{ XF86XK_Forward,	NEMO_ACTION_FORWARD }

#endif
};

void
nemo_window_push_status (NemoWindow *window,
			     const char *text)
{
	g_return_if_fail (NEMO_IS_WINDOW (window));

	/* clear any previous message, underflow is allowed */
	gtk_statusbar_pop (GTK_STATUSBAR (window->details->statusbar), 0);

	if (text != NULL && text[0] != '\0') {
		gtk_statusbar_push (GTK_STATUSBAR (window->details->statusbar), 0, text);
	}
}

void
nemo_window_go_to (NemoWindow *window, GFile *location)
{
	g_return_if_fail (NEMO_IS_WINDOW (window));

	nemo_window_slot_open_location (nemo_window_get_active_slot (window),
					    location, 0);
}

void
nemo_window_go_to_tab (NemoWindow *window, GFile *location)
{
	g_return_if_fail (NEMO_IS_WINDOW (window));

	nemo_window_slot_open_location (nemo_window_get_active_slot (window),
					    location, NEMO_WINDOW_OPEN_FLAG_NEW_TAB);
}

void
nemo_window_go_to_full (NemoWindow *window,
			    GFile          *location,
			    NemoWindowGoToCallback callback,
			    gpointer        user_data)
{
	g_return_if_fail (NEMO_IS_WINDOW (window));

	nemo_window_slot_open_location_full (nemo_window_get_active_slot (window),
						 location, 0, NULL, callback, user_data);
}

static void
nemo_window_go_up_signal (NemoWindow *window)
{
	nemo_window_slot_go_up (nemo_window_get_active_slot (window), 0);
}

void
nemo_window_slot_removed (NemoWindow *window,  NemoWindowSlot *slot)
{
	g_signal_emit (window, signals[SLOT_REMOVED], 0, slot);
}

void
nemo_window_slot_added (NemoWindow *window,  NemoWindowSlot *slot)
{
    g_signal_emit (window, signals[SLOT_ADDED], 0, slot);
}

void
nemo_window_new_tab (NemoWindow *window)
{
	NemoWindowSlot *current_slot;
	NemoWindowSlot *new_slot;
	NemoWindowOpenFlags flags;
	GFile *location;
	int new_slot_position;
	char *scheme;

	current_slot = nemo_window_get_active_slot (window);
	location = nemo_window_slot_get_location (current_slot);

	if (location != NULL) {
		flags = 0;

		new_slot_position = g_settings_get_enum (nemo_preferences, NEMO_PREFERENCES_NEW_TAB_POSITION);
		if (new_slot_position == NEMO_NEW_TAB_POSITION_END) {
			flags = NEMO_WINDOW_OPEN_SLOT_APPEND;
		}

		scheme = g_file_get_uri_scheme (location);
		if (!strcmp (scheme, "x-nemo-search")) {
			g_object_unref (location);
			location = g_file_new_for_path (g_get_home_dir ());
		}
		g_free (scheme);

		new_slot = nemo_window_pane_open_slot (current_slot->pane, flags);
		nemo_window_set_active_slot (window, new_slot);
		nemo_window_slot_open_location (new_slot, location, 0);
		g_object_unref (location);
	}
}

static void
update_cursor (NemoWindow *window)
{
	NemoWindowSlot *slot;
	GdkCursor *cursor;

	slot = nemo_window_get_active_slot (window);

	if (slot && slot->allow_stop) {
		cursor = gdk_cursor_new (GDK_WATCH);
                gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), cursor);
		g_object_unref (cursor);
	} else {
                gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), NULL);
        }
}

void
nemo_window_sync_allow_stop (NemoWindow *window,
				 NemoWindowSlot *slot)
{
	GtkAction *stop_action;
	GtkAction *reload_action;
	gboolean allow_stop, slot_is_active;
	NemoNotebook *notebook;

	stop_action = gtk_action_group_get_action (nemo_window_get_main_action_group (window),
						   NEMO_ACTION_STOP);
	reload_action = gtk_action_group_get_action (nemo_window_get_main_action_group (window),
						     NEMO_ACTION_RELOAD);
	allow_stop = gtk_action_get_sensitive (stop_action);

	slot_is_active = (slot == nemo_window_get_active_slot (window));

	if (!slot_is_active ||
	    allow_stop != slot->allow_stop) {
		if (slot_is_active) {
			gtk_action_set_visible (stop_action, slot->allow_stop);
			gtk_action_set_visible (reload_action, !slot->allow_stop);
		}

		if (gtk_widget_get_realized (GTK_WIDGET (window))) {
			update_cursor (window);
		}

		notebook = NEMO_NOTEBOOK (slot->pane->notebook);
		nemo_notebook_sync_loading (notebook, slot);
	}
}

static void
nemo_window_prompt_for_location (NemoWindow *window,
                                 const char *initial)
{
    NemoWindowPane *pane;

    g_return_if_fail (NEMO_IS_WINDOW (window));

    if (!NEMO_IS_DESKTOP_WINDOW (window)) {
        if (initial) {
            NemoEntry *entry;
            nemo_window_show_location_entry(window);
            pane = window->details->active_pane;
            entry = nemo_location_bar_get_entry (NEMO_LOCATION_BAR (pane->location_bar));
            nemo_entry_set_text (entry, initial);
            gtk_editable_set_position (GTK_EDITABLE (entry), -1);
        }
    }
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

/* This must be called when construction of NemoWindow is finished,
 * since it depends on the type of the argument, which isn't decided at
 * construction time.
 */
static void
nemo_window_set_initial_window_geometry (NemoWindow *window)
{
	GdkScreen *screen;
	guint max_width_for_screen, max_height_for_screen;
	guint default_width, default_height;

	screen = gtk_window_get_screen (GTK_WINDOW (window));

	max_width_for_screen = get_max_forced_width (screen);
	max_height_for_screen = get_max_forced_height (screen);

	default_width = NEMO_WINDOW_DEFAULT_WIDTH;
	default_height = NEMO_WINDOW_DEFAULT_HEIGHT;

	gtk_window_set_default_size (GTK_WINDOW (window),
				     MIN (default_width,
				          max_width_for_screen),
				     MIN (default_height,
				          max_height_for_screen));
}

static gboolean
save_sidebar_width_cb (gpointer user_data)
{
	NemoWindow *window = user_data;

	window->details->sidebar_width_handler_id = 0;

	DEBUG ("Saving sidebar width: %d", window->details->side_pane_width);

	g_settings_set_int (nemo_window_state,
			    NEMO_WINDOW_STATE_SIDEBAR_WIDTH,
			    window->details->side_pane_width);

	return FALSE;
}

/* side pane helpers */
static void
side_pane_size_allocate_callback (GtkWidget *widget,
				  GtkAllocation *allocation,
				  gpointer user_data)
{
	NemoWindow *window;

	window = user_data;

	if (window->details->sidebar_width_handler_id != 0) {
		g_source_remove (window->details->sidebar_width_handler_id);
		window->details->sidebar_width_handler_id = 0;
	}

	if (allocation->width != window->details->side_pane_width &&
	    allocation->width > 1) {
		window->details->side_pane_width = allocation->width;

		window->details->sidebar_width_handler_id =
			g_timeout_add (100, save_sidebar_width_cb, window);
	}
}

static void
setup_side_pane_width (NemoWindow *window)
{
	g_return_if_fail (window->details->sidebar != NULL);

	window->details->side_pane_width =
		g_settings_get_int (nemo_window_state,
				    NEMO_WINDOW_STATE_SIDEBAR_WIDTH);

	gtk_paned_set_position (GTK_PANED (window->details->content_paned),
				window->details->side_pane_width);
}

static void
nemo_window_set_up_sidebar (NemoWindow *window)
{
	GtkWidget *sidebar;

	DEBUG ("Setting up sidebar id %s", window->details->sidebar_id);

	window->details->sidebar = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_style_context_add_class (gtk_widget_get_style_context (window->details->sidebar),
				     GTK_STYLE_CLASS_SIDEBAR);

	gtk_paned_pack1 (GTK_PANED (window->details->content_paned),
			 GTK_WIDGET (window->details->sidebar),
			 FALSE, FALSE);

	setup_side_pane_width (window);
	g_signal_connect (window->details->sidebar,
			  "size_allocate",
			  G_CALLBACK (side_pane_size_allocate_callback),
			  window);

    g_signal_connect_object (NEMO_WINDOW (window), "notify::sidebar-view-id",
                             G_CALLBACK (side_pane_id_changed), window, 0);

    if (g_strcmp0 (window->details->sidebar_id, NEMO_WINDOW_SIDEBAR_PLACES) == 0) {
        sidebar = nemo_places_sidebar_new (window);
    } else if (g_strcmp0 (window->details->sidebar_id, NEMO_WINDOW_SIDEBAR_TREE) == 0) {
        sidebar = nemo_tree_sidebar_new (window);
    } else {
        g_assert_not_reached ();
    }

	gtk_box_pack_start (GTK_BOX (window->details->sidebar), sidebar, TRUE, TRUE, 0);
	gtk_widget_show (sidebar);
	gtk_widget_show (GTK_WIDGET (window->details->sidebar));
}

static void
nemo_window_tear_down_sidebar (NemoWindow *window)
{
	DEBUG ("Destroying sidebar");

    g_signal_handlers_disconnect_by_func (NEMO_WINDOW (window), side_pane_id_changed, window);

	if (window->details->sidebar != NULL) {
		gtk_widget_destroy (GTK_WIDGET (window->details->sidebar));
		window->details->sidebar = NULL;
	}
}

void
nemo_window_hide_sidebar (NemoWindow *window)
{
	DEBUG ("Called hide_sidebar()");

	if (window->details->sidebar == NULL) {
		return;
	}

	nemo_window_tear_down_pane1_wrapper (window);
    nemo_window_tear_down_sidebar2 (window);
	nemo_window_tear_down_sidebar (window);
	nemo_window_update_show_hide_ui_elements (window);

    nemo_window_set_show_sidebar (window, FALSE);
}

void
nemo_window_show_sidebar (NemoWindow *window)
{
	DEBUG ("Called show_sidebar()");

	if (window->details->sidebar != NULL) {
		return;
	}

	if (window->details->disable_chrome) {
		return;
	}

	nemo_window_set_up_sidebar (window);

    /* Mark sidebar as shown BEFORE calling wrapper setup functions.
     * set_up_pane1_wrapper guards on show_sidebar being TRUE - if we
     * set it after, the guard silently skips the wrapper and sidebar1
     * stays full-height in content_paned, breaking the layout. */
    nemo_window_set_show_sidebar (window, TRUE);

    /* Restore per-pane layout if applicable */
    {
        gboolean split = nemo_window_split_view_showing (window);
        gboolean vertical = g_settings_get_boolean (nemo_preferences,
                                                    NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT);
        gboolean sep_sidebar = g_settings_get_boolean (nemo_preferences,
                                                       NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR);
        gboolean sep_nav = g_settings_get_boolean (nemo_preferences,
                                                   NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR);
        /* pane1 wrapper only when separate_sidebar ON (independent of sep_nav) */
        if (split && vertical && sep_sidebar) {
            nemo_window_set_up_pane1_wrapper (window);
            nemo_window_set_up_sidebar2 (window);
        }
    }

	nemo_window_update_show_hide_ui_elements (window);
    nemo_window_refresh_sidebar_colours (window);
}

static gboolean
sidebar_id_is_valid (const gchar *sidebar_id)
{
    return (g_strcmp0 (sidebar_id, NEMO_WINDOW_SIDEBAR_PLACES) == 0 ||
            g_strcmp0 (sidebar_id, NEMO_WINDOW_SIDEBAR_TREE) == 0);
}

static void
side_pane_id_changed (NemoWindow *window)
{

    if (!sidebar_id_is_valid (window->details->sidebar_id)) {
        return;
    }

    /* refresh the sidebar - tear down per-pane wrappers first so sidebar
     * can be safely moved back to content_paned before rebuild */
    nemo_window_tear_down_pane1_wrapper (window);
    nemo_window_tear_down_sidebar2 (window);
    nemo_window_tear_down_sidebar (window);
    nemo_window_set_up_sidebar (window);

    /* Restore per-pane layout if applicable */
    {
        gboolean split = nemo_window_split_view_showing (window);
        gboolean vertical = g_settings_get_boolean (nemo_preferences,
                                                    NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT);
        gboolean sep_sidebar = g_settings_get_boolean (nemo_preferences,
                                                       NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR);
        gboolean sep_nav = g_settings_get_boolean (nemo_preferences,
                                                   NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR);
        /* pane1 wrapper only when separate_sidebar ON (independent of sep_nav) */
        if (split && vertical && sep_sidebar && window->details->show_sidebar) {
            nemo_window_set_up_pane1_wrapper (window);
            nemo_window_set_up_sidebar2 (window);
        }
    }

    nemo_window_refresh_sidebar_colours (window);
}

gboolean
nemo_window_disable_chrome_mapping (GValue *value,
					GVariant *variant,
					gpointer user_data)
{
	NemoWindow *window = user_data;

	g_value_set_boolean (value,
			     g_variant_get_boolean (variant) &&
			     !window->details->disable_chrome);

	return TRUE;
}

static gboolean
on_button_press_callback (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    NemoWindow *window = NEMO_WINDOW (user_data);

    if (event->button == 3) {
        toggle_menubar (window, MENU_TOGGLE);
    }

    return GDK_EVENT_STOP;
}

static void
clear_menu_hide_delay (NemoWindow *window)
{
    if (window->details->menu_hide_delay_id > 0) {
        g_source_remove (window->details->menu_hide_delay_id);
    }

    window->details->menu_hide_delay_id = 0;
}

static gboolean
hide_menu_on_delay (NemoWindow *window)
{
    toggle_menubar (window, MENU_HIDE);

    window->details->menu_hide_delay_id = 0;
    return FALSE;
}

static gboolean
on_menu_focus_out (GtkMenuShell *widget,
                   GdkEvent  *event,
                   gpointer   user_data)
{
    NemoWindow *window = NEMO_WINDOW (user_data);

    /* The menu, when visible on demand, gets the keyboard grab.
     * If the user clicks on some element in the window,, we want the menu
     * to disappear, but if it's done immediately, everything shifts up the
     * height of the menu, and the user will more than likely end up clicking
     * in the wrong spot.  Delay the hide momentarily, to allow the user to
     * complete their click action. */
    clear_menu_hide_delay (window);

    /* When a submenu pops-up, the menu loses focus. The menu should disappear
     * only when none of its elements is selected. */
    if (!gtk_menu_shell_get_selected_item (widget)) {
        window->details->menu_hide_delay_id = g_timeout_add (200, (GSourceFunc) hide_menu_on_delay, window);
    }

    return GDK_EVENT_PROPAGATE;
}

void
on_menu_selection_done (GtkMenuShell *menushell,
                        gpointer      user_data)
{
	NemoWindow *window = NEMO_WINDOW (user_data);

	/* Remove the menu inmediately after selecting an item. */
	clear_menu_hide_delay (window);
	window->details->menu_hide_delay_id = g_timeout_add (0, (GSourceFunc) hide_menu_on_delay, window);
}

static void
nemo_window_constructed (GObject *self)
{
	NemoWindow *window;
	GtkWidget *grid;
	GtkWidget *menu;
	GtkWidget *hpaned;
	GtkWidget *vbox;
	GtkWidget *toolbar_holder;
    GtkWidget *nemo_statusbar;
	NemoWindowPane *pane;
	NemoWindowSlot *slot;
	NemoApplication *application;

	window = NEMO_WINDOW (self);
	application = nemo_application_get_singleton ();

	G_OBJECT_CLASS (nemo_window_parent_class)->constructed (self);
	gtk_window_set_application (GTK_WINDOW (window), GTK_APPLICATION (application));

	/* disable automatic menubar handling, since we show our regular
	 * menubar together with the app menu.
	 */
	gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (self), FALSE);

	grid = gtk_grid_new ();
	gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (grid);
	gtk_container_add (GTK_CONTAINER (window), grid);

	/* Statusbar is packed in the subclasses */

	nemo_window_initialize_menus (window);
	nemo_window_initialize_actions (window);

	menu = gtk_ui_manager_get_widget (window->details->ui_manager, "/MenuBar");
	window->details->menubar = menu;

    gtk_widget_set_can_focus (menu, TRUE);
	gtk_widget_set_hexpand (menu, TRUE);

    g_signal_connect_object (menu,
                             "focus-out-event",
                             G_CALLBACK (on_menu_focus_out),
                             window,
                             0);

    g_signal_connect_object (menu,
                             "selection-done",
                             G_CALLBACK (on_menu_selection_done),
                             window,
                             0);

	if (g_settings_get_boolean (nemo_window_state, NEMO_WINDOW_STATE_START_WITH_MENU_BAR)){
		gtk_widget_show (menu);
	} else {
		gtk_widget_hide (menu);
	}

    g_settings_bind_with_mapping (nemo_window_state,
                      NEMO_WINDOW_STATE_START_WITH_MENU_BAR,
                      window->details->menubar,
                      "visible",
                      G_SETTINGS_BIND_GET,
                      nemo_window_disable_chrome_mapping, NULL,
                      window, NULL);

	gtk_container_add (GTK_CONTAINER (grid), menu);

	/* Set up the toolbar place holder */
	toolbar_holder = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add (GTK_CONTAINER (grid), toolbar_holder);
	gtk_widget_show (toolbar_holder);

    g_signal_connect_object (toolbar_holder, "button-press-event",
                             G_CALLBACK (on_button_press_callback), window, 0);

	window->details->toolbar_holder = toolbar_holder;

	/* Register to menu provider extension signal managing menu updates */
	g_signal_connect_object (nemo_signaller_get_current (), "popup_menu_changed",
			 G_CALLBACK (nemo_window_load_extension_menus), window, G_CONNECT_SWAPPED);

	window->details->content_paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_hexpand (window->details->content_paned, TRUE);
	gtk_widget_set_vexpand (window->details->content_paned, TRUE);

	gtk_container_add (GTK_CONTAINER (grid), window->details->content_paned);
	gtk_widget_show (window->details->content_paned);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_paned_pack2 (GTK_PANED (window->details->content_paned), vbox,
			 TRUE, FALSE);
	gtk_widget_show (vbox);

	hpaned = gtk_paned_new (g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT)
                            ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start (GTK_BOX (vbox), hpaned, TRUE, TRUE, 0);
	gtk_widget_show (hpaned);
	window->details->split_view_hpane = hpaned;

	pane = nemo_window_pane_new (window);
	window->details->panes = g_list_prepend (window->details->panes, pane);

	gtk_paned_pack1 (GTK_PANED (hpaned), GTK_WIDGET (pane), TRUE, FALSE);

    nemo_statusbar = nemo_status_bar_new (window);
    window->details->nemo_status_bar = nemo_statusbar;

    GtkWidget *sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_container_add (GTK_CONTAINER (grid), sep);
    gtk_widget_show (sep);

    GtkWidget *eb;

    eb = gtk_event_box_new ();
    gtk_container_add (GTK_CONTAINER (eb), nemo_statusbar);
    gtk_container_add (GTK_CONTAINER (grid), eb);
    gtk_widget_show (eb);

    window->details->statusbar = nemo_status_bar_get_real_statusbar (NEMO_STATUS_BAR (nemo_statusbar));
    window->details->help_message_cid = gtk_statusbar_get_context_id (GTK_STATUSBAR (window->details->statusbar),
                                                                      "help_message");

    gtk_widget_add_events (GTK_WIDGET (eb), GDK_BUTTON_PRESS_MASK);

    g_signal_connect_object (GTK_WIDGET (eb), "button-press-event",
                             G_CALLBACK (on_button_press_callback), window, 0);

    g_settings_bind_with_mapping (nemo_window_state,
                      NEMO_WINDOW_STATE_START_WITH_STATUS_BAR,
                      window->details->nemo_status_bar,
                      "visible",
                      G_SETTINGS_BIND_DEFAULT,
                      nemo_window_disable_chrome_mapping, NULL,
                      window, NULL);

    g_object_bind_property (window->details->nemo_status_bar, "visible",
                            sep, "visible",
                            G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

	/* this has to be done after the location bar has been set up,
	 * but before menu stuff is being called */
	nemo_window_set_active_pane (window, pane);

	side_pane_id_changed (window);

	nemo_window_initialize_bookmarks_menu (window);
	nemo_window_set_initial_window_geometry (window);

	slot = nemo_window_pane_open_slot (window->details->active_pane, 0);
	nemo_window_set_active_slot (window, slot);

    if (g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_START_WITH_DUAL_PANE) &&
        !window->details->disable_chrome)
        nemo_window_split_view_on (window);

    g_signal_connect_swapped (GTK_WINDOW (window),
                              "notify::scale-factor",
                              G_CALLBACK (nemo_window_reload),
                              window);
}

static void
nemo_window_set_property (GObject *object,
			      guint arg_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	NemoWindow *window;

	window = NEMO_WINDOW (object);

	switch (arg_id) {
	case PROP_DISABLE_CHROME:
		window->details->disable_chrome = g_value_get_boolean (value);
		break;
    case PROP_SIDEBAR_VIEW_TYPE:
        window->details->sidebar_id = g_strdup (g_value_get_string (value));
        break;
    case PROP_SHOW_SIDEBAR:
        nemo_window_set_show_sidebar (window, g_value_get_boolean (value));
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
		break;
	}
}

static void
nemo_window_get_property (GObject *object,
			      guint arg_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	NemoWindow *window;

	window = NEMO_WINDOW (object);

	switch (arg_id) {
        case PROP_DISABLE_CHROME:
            g_value_set_boolean (value, window->details->disable_chrome);
            break;
        case PROP_SIDEBAR_VIEW_TYPE:
            g_value_set_string (value, window->details->sidebar_id);
            break;
        case PROP_SHOW_SIDEBAR:
            g_value_set_boolean (value, window->details->show_sidebar);
            break;
        default:
        	g_assert_not_reached ();
        	break;
	}
}

static void
destroy_panes_foreach (gpointer data,
		       gpointer user_data)
{
	NemoWindowPane *pane = data;
	NemoWindow *window = user_data;

	nemo_window_close_pane (window, pane);
}

static void
nemo_window_destroy (GtkWidget *object)
{
	NemoWindow *window;
	GList *panes_copy;

	window = NEMO_WINDOW (object);

	DEBUG ("Destroying window");

	/* Tear down per-pane wrappers before the sidebar and panes, so sidebar2
	 * and pri_paned are removed cleanly from the widget hierarchy before
	 * pane widgets are destroyed. */
	nemo_window_tear_down_pane1_wrapper (window);
	nemo_window_tear_down_sidebar2 (window);

	/* close the sidebar first */
	nemo_window_tear_down_sidebar (window);

	/* close all panes safely */
	panes_copy = g_list_copy (window->details->panes);
	g_list_foreach (panes_copy, (GFunc) destroy_panes_foreach, window);
	g_list_free (panes_copy);

	/* the panes list should now be empty */
	g_assert (window->details->panes == NULL);
	g_assert (window->details->active_pane == NULL);

	GTK_WIDGET_CLASS (nemo_window_parent_class)->destroy (object);
}

static void
nemo_window_finalize (GObject *object)
{
	NemoWindow *window;

	window = NEMO_WINDOW (object);

	if (window->details->sidebar_width_handler_id != 0) {
		g_source_remove (window->details->sidebar_width_handler_id);
		window->details->sidebar_width_handler_id = 0;
	}

    g_signal_handlers_disconnect_by_func (nemo_preferences,
                                          nemo_window_sync_thumbnail_action,
                                          window);

    clear_menu_hide_delay (window);

	nemo_window_finalize_menus (window);

	g_clear_object (&window->details->nav_state);
    g_clear_object (&window->details->secondary_pane_last_location);

	g_clear_object (&window->details->ui_manager);

	g_free (window->details->sidebar_id);

	/* nemo_window_close() should have run */
	g_assert (window->details->panes == NULL);

	G_OBJECT_CLASS (nemo_window_parent_class)->finalize (object);
}

void
nemo_window_view_visible (NemoWindow *window,
			      NemoView *view)
{
	NemoWindowSlot *slot;
	NemoWindowPane *pane;
	GList *l, *walk;

	g_return_if_fail (NEMO_IS_WINDOW (window));

	slot = nemo_window_get_slot_for_view (window, view);

	if (slot->visible) {
		return;
	}

	slot->visible = TRUE;
	pane = slot->pane;

	if (gtk_widget_get_visible (GTK_WIDGET (pane))) {
		return;
	}

	/* Look for other non-visible slots */
	for (l = pane->slots; l != NULL; l = l->next) {
		slot = l->data;

		if (!slot->visible) {
			return;
		}
	}

	/* None, this pane is visible */
	gtk_widget_show (GTK_WIDGET (pane));

	/* Look for other non-visible panes */
	for (walk = window->details->panes; walk; walk = walk->next) {
		pane = walk->data;

		if (!gtk_widget_get_visible (GTK_WIDGET (pane))) {
			return;
		}

		for (l = pane->slots; l != NULL; l = l->next) {
			slot = l->data;

			nemo_window_slot_update_title (slot);
			nemo_window_slot_update_icon (slot);
		}
	}

	nemo_window_pane_grab_focus (window->details->active_pane);

	/* All slots and panes visible, show window */
	gtk_widget_show (GTK_WIDGET (window));
}

static gboolean
nemo_window_is_desktop (NemoWindow *window)
{
    return window->details->disable_chrome;
}

static void
nemo_window_save_geometry (NemoWindow *window)
{
	char *geometry_string;
	gboolean is_maximized;

	g_assert (NEMO_IS_WINDOW (window));

	if (gtk_widget_get_window (GTK_WIDGET (window)) && !nemo_window_is_desktop (window)) {
        GdkWindowState state = gdk_window_get_state (gtk_widget_get_window (GTK_WIDGET (window)));

        if (state & GDK_WINDOW_STATE_TILED) {
            return;
        }

        geometry_string = eel_gtk_window_get_geometry_string (GTK_WINDOW (window));

		is_maximized = state & GDK_WINDOW_STATE_MAXIMIZED;

		if (!is_maximized) {
			g_settings_set_string
				(nemo_window_state, NEMO_WINDOW_STATE_GEOMETRY,
				 geometry_string);
		}
		g_free (geometry_string);

		g_settings_set_boolean
			(nemo_window_state, NEMO_WINDOW_STATE_MAXIMIZED,
			 is_maximized);
	}
}

void
nemo_window_close (NemoWindow *window)
{
	NEMO_WINDOW_CLASS (G_OBJECT_GET_CLASS (window))->close (window);
}

void
nemo_window_close_pane (NemoWindow *window,
			    NemoWindowPane *pane)
{
	g_assert (NEMO_IS_WINDOW_PANE (pane));

	while (pane->slots != NULL) {
		NemoWindowSlot *slot = pane->slots->data;

		nemo_window_pane_remove_slot_unsafe (pane, slot);
	}

	/* If the pane was active, set it to NULL. The caller is responsible
	 * for setting a new active pane with nemo_window_set_active_pane()
	 * if it wants to continue using the window. */
	if (window->details->active_pane == pane) {
		window->details->active_pane = NULL;
	}

	/* Required really. Destroying the NemoWindowPane still leaves behind the toolbar.
	 * This kills it off. Do it before we call gtk_widget_destroy for safety.
	 * The toolbar may have been reparented into the pane itself (embedded nav bar mode),
	 * in which case gtk_widget_destroy on the pane will handle it — but we remove it
	 * from toolbar_holder only if that's where it still lives. */
	if (gtk_widget_get_parent (GTK_WIDGET (pane->tool_bar)) == window->details->toolbar_holder) {
		gtk_container_remove (GTK_CONTAINER (window->details->toolbar_holder), GTK_WIDGET (pane->tool_bar));
	}

	window->details->panes = g_list_remove (window->details->panes, pane);

	gtk_widget_destroy (GTK_WIDGET (pane));
}

NemoWindowPane*
nemo_window_get_active_pane (NemoWindow *window)
{
	g_assert (NEMO_IS_WINDOW (window));
	return window->details->active_pane;
}

static void
real_set_active_pane (NemoWindow *window, NemoWindowPane *new_pane)
{
	/* make old pane inactive, and new one active.
	 * Currently active pane may be NULL (after init). */
	if (window->details->active_pane &&
	    window->details->active_pane != new_pane) {
		nemo_window_pane_set_active (window->details->active_pane, FALSE);
	}
	nemo_window_pane_set_active (new_pane, TRUE);

	window->details->active_pane = new_pane;
}

/* Make the given pane the active pane of its associated window. This
 * always implies making the containing active slot the active slot of
 * the window. */
void
nemo_window_set_active_pane (NemoWindow *window,
				 NemoWindowPane *new_pane)
{
	g_assert (NEMO_IS_WINDOW_PANE (new_pane));

	DEBUG ("Setting new pane %p as active", new_pane);

	if (new_pane->active_slot) {
		nemo_window_set_active_slot (window, new_pane->active_slot);
	} else if (new_pane != window->details->active_pane) {
		real_set_active_pane (window, new_pane);
	}
}

/* Make both, the given slot the active slot and its corresponding
 * pane the active pane of the associated window.
 * new_slot may be NULL. */
void
nemo_window_set_active_slot (NemoWindow *window, NemoWindowSlot *new_slot)
{
	NemoWindowSlot *old_slot;
	g_assert (NEMO_IS_WINDOW (window));

	DEBUG ("Setting new slot %p as active", new_slot);

	if (new_slot) {
		g_assert ((window == nemo_window_slot_get_window (new_slot)));
		g_assert (NEMO_IS_WINDOW_PANE (new_slot->pane));
		g_assert (g_list_find (new_slot->pane->slots, new_slot) != NULL);
	}

	old_slot = nemo_window_get_active_slot (window);

	if (old_slot == new_slot) {
		return;
	}

	/* make old slot inactive if it exists (may be NULL after init, for example) */
	if (old_slot != NULL) {
		/* inform window */
		if (old_slot->content_view != NULL) {
			nemo_window_disconnect_content_view (window, old_slot->content_view);
		}
		/* Only hide the toolbar if it is in the shared toolbar_holder.
		 * Embedded toolbars (separate nav bar mode) must always stay visible. */
		if (gtk_widget_get_parent (GTK_WIDGET (old_slot->pane->tool_bar)) ==
		    window->details->toolbar_holder) {
			gtk_widget_hide (GTK_WIDGET (old_slot->pane->tool_bar));
		}
		/* inform slot & view */
		g_signal_emit_by_name (old_slot, "inactive");
	}

	/* deal with panes */
	if (new_slot &&
	    new_slot->pane != window->details->active_pane) {
		real_set_active_pane (window, new_slot->pane);
	}

	window->details->active_pane->active_slot = new_slot;

	/* make new slot active, if it exists */
	if (new_slot) {
		/* inform sidebar panels */
                nemo_window_report_location_change (window);
		/* TODO decide whether "selection-changed" should be emitted */

		if (new_slot->content_view != NULL) {
                        /* inform window */
                        nemo_window_connect_content_view (window, new_slot->content_view);
                }

		/* Show active toolbar - but only if it is in toolbar_holder (not embedded).
		 * Embedded toolbars in separate-nav-bar mode are always visible. */
		if (gtk_widget_get_parent (GTK_WIDGET (new_slot->pane->tool_bar)) ==
		    window->details->toolbar_holder) {
			gboolean show_toolbar = g_settings_get_boolean (nemo_window_state,
			                                                NEMO_WINDOW_STATE_START_WITH_TOOLBAR);
			if (show_toolbar) {
				gtk_widget_show (GTK_WIDGET (new_slot->pane->tool_bar));
			}
		}

		/* inform slot & view */
                g_signal_emit_by_name (new_slot, "active");
	}
}

static void
nemo_window_realize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (nemo_window_parent_class)->realize (widget);
	update_cursor (NEMO_WINDOW (widget));
}

static void
toggle_menubar (NemoWindow *window, gint action)
{
    GtkWidget *menu;
    gboolean default_visible;

    default_visible = g_settings_get_boolean (nemo_window_state,
                                              NEMO_WINDOW_STATE_START_WITH_MENU_BAR);

    if (default_visible || window->details->disable_chrome) {
        return;
    }

    menu = window->details->menubar;

    if (action == MENU_TOGGLE) {
        action = gtk_widget_get_visible (menu) ? MENU_HIDE : MENU_SHOW;
    }

    if (action == MENU_HIDE) {
        gtk_widget_hide (menu);
    } else {
        gtk_widget_show (menu);

        /* When the menu is normally hidden, have an activation of it trigger a key grab.
         * For keyboard users, this is a natural progression, that they will type a mnemonic
         * next to open a menu.  Any loss of focus or click elsewhere will re-hide the menu
         * and cancel focus.
         */
        gtk_widget_grab_focus (menu);
        gtk_window_set_mnemonics_visible (GTK_WINDOW (window), TRUE);
    }

    return;
}

static gboolean
is_alt_key_event (GdkEventKey *event)
{
    GdkModifierType nominal_state;
    gboolean state_ok;

    nominal_state = event->state & gtk_accelerator_get_default_mod_mask();

    /* A key press of alt will show just the alt keyval (GDK_KEY_Alt_L/R).  A key release
     * of a single modifier is always modified by itself.  So a valid press state is 0 and
     * a valid release state is GDK_MOD1_MASK (alt modifier).
     */
    state_ok = (event->type == GDK_KEY_PRESS && nominal_state == 0) ||
               (event->type == GDK_KEY_RELEASE && nominal_state == GDK_MOD1_MASK);

    if (state_ok && (event->keyval == GDK_KEY_Alt_L || event->keyval == GDK_KEY_Alt_R)) {
        return TRUE;
    }

    return FALSE;
}

static gboolean
nemo_window_key_press_event (GtkWidget *widget,
				 GdkEventKey *event)
{
	NemoWindow *window;
	NemoWindowSlot *active_slot;
	NemoView *view;
	GtkWidget *focus_widget;
	size_t i;

	window = NEMO_WINDOW (widget);

	active_slot = nemo_window_get_active_slot (window);
	view = active_slot->content_view;

      /**
       * Disable the GTK Emoji Chooser
       */
      if ((event->keyval == GDK_KEY_semicolon || event->keyval == GDK_KEY_period) && (event->state & GDK_CONTROL_MASK)) {
          return FALSE;
      }

	if (view != NULL && nemo_view_get_is_renaming (view) && event->keyval != GDK_KEY_F2) {
		/* if we're renaming, just forward the event to the
		 * focused widget and return. We don't want to process the window
		 * accelerator bindings, as they might conflict with the
		 * editable widget bindings.
		 */
		if (gtk_window_propagate_key_event (GTK_WINDOW (window), event)) {
			return TRUE;
		}

               /* Do not allow for other accelerator bindings to fire off while
                *  renaming is in progress
                */
               return FALSE;
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

    /* An alt key press by itself will always hide the menu if it's visible.  We set a flag
     * to skip the subsequent release, otherwise we'll show the menu again.
     *
     * When alt is pressed and the menu is NOT visible, we flag that on release we'll show the
     * menu.  If any other keys are pressed between alt being pressed and released, we clear that
     * flag, because it was more than likely part of some other shortcut, and otherwise, depending
     * on the order the keys are released, if the alt key is last to be released, we don't want to
     * show the menu, as that was not the original intent.
     */

    if (is_alt_key_event (event)) {
        if (gtk_widget_get_visible (window->details->menubar)) {
            toggle_menubar (window, MENU_HIDE);
            window->details->menu_skip_release = TRUE;
        } else {
            window->details->menu_show_queued = TRUE;
        }
    } else {
        window->details->menu_show_queued = FALSE;
    }

	return GTK_WIDGET_CLASS (nemo_window_parent_class)->key_press_event (widget, event);
}

static gboolean
nemo_window_key_release_event (GtkWidget *widget,
                             GdkEventKey *event)
{
    NemoWindow *window = NEMO_WINDOW (widget);

    /* Conditions to show the menu via the alt key is that it must have been pressed and
     * released without any other key events in between, and we must not have hidden the
     * menu on the alt key press event.  Show we check both flags here, for opposing states.
     */

    if (is_alt_key_event (event)) {
        if (!window->details->menu_skip_release && window->details->menu_show_queued) {
            toggle_menubar (window, MENU_SHOW);
        }
    }

    window->details->menu_skip_release = FALSE;
    window->details->menu_show_queued = FALSE;

    return GTK_WIDGET_CLASS (nemo_window_parent_class)->key_release_event (widget, event);
}

/*
 * Main API
 */

static void
sync_view_type_callback (NemoFile *file,
                         gpointer callback_data)
{
    NemoWindow *window;
    NemoWindowSlot *slot;

    slot = callback_data;
    window = nemo_window_slot_get_window (slot);

    if (slot == nemo_window_get_active_slot (window)) {
        const gchar *view_id;

        if (slot->content_view == NULL) {
            return;
        }

        view_id = nemo_window_slot_get_content_view_id (slot);

        toolbar_set_view_button (action_for_view_id (view_id), window);
        menu_set_view_selection (action_for_view_id (view_id), window);
    }
}

static void
cancel_sync_view_type_callback (NemoWindowSlot *slot)
{
	nemo_file_cancel_call_when_ready (slot->viewed_file,
					      sync_view_type_callback,
					      slot);
}

void
nemo_window_sync_view_type (NemoWindow *window)
{
    NemoWindowSlot *slot;
    NemoFileAttributes attributes;

    g_return_if_fail (NEMO_IS_WINDOW (window));

    attributes = nemo_mime_actions_get_required_file_attributes ();

    slot = nemo_window_get_active_slot (window);

    cancel_sync_view_type_callback (slot);
    nemo_file_call_when_ready (slot->viewed_file,
                               attributes,
                               sync_view_type_callback,
                               slot);
}

void
nemo_window_sync_menu_bar (NemoWindow *window)
{
    GtkWidget *menu = window->details->menubar;

    if (g_settings_get_boolean (nemo_window_state, NEMO_WINDOW_STATE_START_WITH_MENU_BAR) &&
                                !window->details->disable_chrome) {
        gtk_widget_show (menu);
    } else {
        gtk_widget_hide (menu);
    }
}

void
nemo_window_sync_title (NemoWindow *window,
			    NemoWindowSlot *slot)
{
	NemoWindowPane *pane;
	NemoNotebook *notebook;
	char *full_title;
	char *window_title;

	if (NEMO_WINDOW_CLASS (G_OBJECT_GET_CLASS (window))->sync_title != NULL) {
		NEMO_WINDOW_CLASS (G_OBJECT_GET_CLASS (window))->sync_title (window, slot);

		return;
	}

	if (slot == nemo_window_get_active_slot (window)) {
		/* if spatial mode is default, we keep "File Browser" in the window title
		 * to recognize browser windows. Otherwise, we default to the directory name.
		 */
		if (!g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_ALWAYS_USE_BROWSER)) {
			full_title = g_strdup_printf (_("%s - File Browser"), slot->title);
			window_title = eel_str_middle_truncate (full_title, MAX_TITLE_LENGTH);
			g_free (full_title);
		} else {
			window_title = eel_str_middle_truncate (slot->title, MAX_TITLE_LENGTH);
		}

		gtk_window_set_title (GTK_WINDOW (window), window_title);
		g_free (window_title);
	}

	pane = slot->pane;
	notebook = NEMO_NOTEBOOK (pane->notebook);
	nemo_notebook_sync_tab_label (notebook, slot);
}

void
nemo_window_sync_zoom_widgets (NemoWindow *window)
{
	NemoWindowSlot *slot;
	NemoView *view;
	GtkActionGroup *action_group;
	GtkAction *action;
	gboolean supports_zooming;
	gboolean can_zoom, can_zoom_in, can_zoom_out;
	NemoZoomLevel zoom_level;

	slot = nemo_window_get_active_slot (window);
	view = slot->content_view;

	if (view != NULL) {
		supports_zooming = nemo_view_supports_zooming (view);
		zoom_level = nemo_view_get_zoom_level (view);
		can_zoom = supports_zooming &&
			   zoom_level >= NEMO_ZOOM_LEVEL_SMALLEST &&
			   zoom_level <= NEMO_ZOOM_LEVEL_LARGEST;
		can_zoom_in = can_zoom && nemo_view_can_zoom_in (view);
		can_zoom_out = can_zoom && nemo_view_can_zoom_out (view);
	} else {
		supports_zooming = FALSE;
		can_zoom = FALSE;
		can_zoom_in = FALSE;
		can_zoom_out = FALSE;
	}

	action_group = nemo_window_get_main_action_group (window);

	action = gtk_action_group_get_action (action_group,
					      NEMO_ACTION_ZOOM_IN);
	gtk_action_set_visible (action, supports_zooming);
	gtk_action_set_sensitive (action, can_zoom_in);

	action = gtk_action_group_get_action (action_group,
					      NEMO_ACTION_ZOOM_OUT);
	gtk_action_set_visible (action, supports_zooming);
	gtk_action_set_sensitive (action, can_zoom_out);

	action = gtk_action_group_get_action (action_group,
					      NEMO_ACTION_ZOOM_NORMAL);
	gtk_action_set_visible (action, supports_zooming);
	gtk_action_set_sensitive (action, can_zoom);

    nemo_status_bar_sync_zoom_widgets (NEMO_STATUS_BAR (window->details->nemo_status_bar));
}

void
nemo_window_sync_bookmark_action (NemoWindow *window)
{
    NemoWindowSlot *slot;
    GFile *location;
    GtkAction *action;
    gchar *uri;
    slot = nemo_window_get_active_slot (window);
    location = nemo_window_slot_get_location (slot);

    if (!location) {
        return;
    }

    uri = g_file_get_uri (location);

    action = gtk_action_group_get_action (nemo_window_get_main_action_group (window),
                                          NEMO_ACTION_ADD_BOOKMARK);

    gtk_action_set_sensitive (action, !eel_uri_is_search (uri));

    g_free (uri);
    g_object_unref (location);
}

void
sync_thumbnail_action_callback (NemoFile *file,
                       gpointer callback_data)
{
    NemoWindow *window;
    NemoWindowSlot *slot;

    slot = callback_data;
    window = nemo_window_slot_get_window (slot);

    if (slot == nemo_window_get_active_slot (window)) {
        NemoWindowPane *pane;
        gboolean show_thumbnails;

        pane = nemo_window_get_active_pane(window);
        show_thumbnails = nemo_file_should_show_thumbnail (file);

        toolbar_set_show_thumbnails_button (show_thumbnails, pane);
        menu_set_show_thumbnails_action (show_thumbnails, window);
    }
}

static void
cancel_sync_show_thumbnail_callback (NemoWindowSlot *slot)
{
	nemo_file_cancel_call_when_ready (slot->viewed_file,
					      sync_thumbnail_action_callback,
					      slot);
}

void
nemo_window_sync_thumbnail_action (NemoWindow *window)
{
    NemoWindowSlot *slot;
    NemoFileAttributes attributes;

    g_return_if_fail (NEMO_IS_WINDOW (window));

    attributes = nemo_mime_actions_get_required_file_attributes ();

    slot = nemo_window_get_active_slot (window);

    cancel_sync_show_thumbnail_callback (slot);
    nemo_file_call_when_ready (slot->viewed_file,
                               attributes,
                               sync_thumbnail_action_callback,
                               slot);
}

void
nemo_window_sync_create_folder_button (NemoWindow *window)
{
    NemoWindowSlot *slot;
    gboolean allow;

    slot = nemo_window_get_active_slot (window);

    allow = nemo_file_can_write (slot->viewed_file) &&
            !nemo_file_is_in_favorites (slot->viewed_file) &&
            !nemo_file_is_in_trash (slot->viewed_file);

    toolbar_set_create_folder_button (allow, slot->pane);
}

static void
zoom_level_changed_callback (NemoView *view,
                             NemoWindow *window)
{
	g_assert (NEMO_IS_WINDOW (window));

	/* This is called each time the component in
	 * the active slot successfully completed
	 * a zooming operation.
	 */
	nemo_window_sync_zoom_widgets (window);
}

/* These are called
 *   A) when switching the view within the active slot
 *   B) when switching the active slot
 *   C) when closing the active slot (disconnect)
*/
void
nemo_window_connect_content_view (NemoWindow *window,
				      NemoView *view)
{
	NemoWindowSlot *slot;

	g_assert (NEMO_IS_WINDOW (window));
	g_assert (NEMO_IS_VIEW (view));

	slot = nemo_window_get_slot_for_view (window, view);

	if (slot != nemo_window_get_active_slot (window)) {
		return;
	}

	g_signal_connect (view, "zoom-level-changed",
			  G_CALLBACK (zoom_level_changed_callback),
			  window);

    /* Update displayed the selected view type in the toolbar and menu. */
    if (slot->pending_location == NULL) {
        nemo_window_sync_view_type (window);
    }

	nemo_view_grab_focus (view);
}

void
nemo_window_disconnect_content_view (NemoWindow *window,
					 NemoView *view)
{
	NemoWindowSlot *slot;

	g_assert (NEMO_IS_WINDOW (window));
	g_assert (NEMO_IS_VIEW (view));

	slot = nemo_window_get_slot_for_view (window, view);

	if (slot != nemo_window_get_active_slot (window)) {
		return;
	}

	g_signal_handlers_disconnect_by_func (view, G_CALLBACK (zoom_level_changed_callback), window);
}

/**
 * nemo_window_show:
 * @widget:	GtkWidget
 *
 * Call parent and then show/hide window items
 * base on user prefs.
 */
static void
nemo_window_show (GtkWidget *widget)
{
	NemoWindow *window;

	window = NEMO_WINDOW (widget);

    window->details->sidebar_id = g_settings_get_string (nemo_window_state,
                                                         NEMO_WINDOW_STATE_SIDE_PANE_VIEW);

	if (g_settings_get_boolean (nemo_window_state, NEMO_WINDOW_STATE_START_WITH_SIDEBAR)) {
		nemo_window_show_sidebar (window);
	} else {
		nemo_window_hide_sidebar (window);
	}

	GTK_WIDGET_CLASS (nemo_window_parent_class)->show (widget);

	gtk_ui_manager_ensure_update (window->details->ui_manager);
}

GtkUIManager *
nemo_window_get_ui_manager (NemoWindow *window)
{
	g_return_val_if_fail (NEMO_IS_WINDOW (window), NULL);

	return window->details->ui_manager;
}

GtkActionGroup *
nemo_window_get_main_action_group (NemoWindow *window)
{
	g_return_val_if_fail (NEMO_IS_WINDOW (window), NULL);

	return window->details->main_action_group;
}

NemoNavigationState *
nemo_window_get_navigation_state (NemoWindow *window)
{
	g_return_val_if_fail (NEMO_IS_WINDOW (window), NULL);

	return window->details->nav_state;
}

NemoWindowPane *
nemo_window_get_next_pane (NemoWindow *window)
{
       NemoWindowPane *next_pane;
       GList *node;

       /* return NULL if there is only one pane */
       if (!window->details->panes || !window->details->panes->next) {
	       return NULL;
       }

       /* get next pane in the (wrapped around) list */
       node = g_list_find (window->details->panes, window->details->active_pane);
       g_return_val_if_fail (node, NULL);
       if (node->next) {
	       next_pane = node->next->data;
       } else {
	       next_pane =  window->details->panes->data;
       }

       return next_pane;
}

void
nemo_window_slot_set_viewed_file (NemoWindowSlot *slot,
				      NemoFile *file)
{
	NemoFileAttributes attributes;

	if (slot->viewed_file == file) {
		return;
	}

	nemo_file_ref (file);

	cancel_sync_view_type_callback (slot);
    cancel_sync_show_thumbnail_callback (slot);

	if (slot->viewed_file != NULL) {
		nemo_file_monitor_remove (slot->viewed_file,
					      slot);
	}

	if (file != NULL) {
		attributes =
			NEMO_FILE_ATTRIBUTE_INFO |
			NEMO_FILE_ATTRIBUTE_LINK_INFO;
		nemo_file_monitor_add (file, slot, attributes);
	}

	nemo_file_unref (slot->viewed_file);
	slot->viewed_file = file;
}

NemoWindowSlot *
nemo_window_get_slot_for_view (NemoWindow *window,
				   NemoView *view)
{
	NemoWindowSlot *slot;
	GList *l, *walk;

	for (walk = window->details->panes; walk; walk = walk->next) {
		NemoWindowPane *pane = walk->data;

		for (l = pane->slots; l != NULL; l = l->next) {
			slot = l->data;
			if (slot->content_view == view ||
			    slot->new_content_view == view) {
				return slot;
			}
		}
	}

	return NULL;
}

NemoWindowShowHiddenFilesMode
nemo_window_get_hidden_files_mode (NemoWindow *window)
{
	return window->details->show_hidden_files_mode;
}

void
nemo_window_set_hidden_files_mode (NemoWindow *window,
				       NemoWindowShowHiddenFilesMode  mode)
{
	window->details->show_hidden_files_mode = mode;
    g_settings_set_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_HIDDEN_FILES,
                            mode == NEMO_WINDOW_SHOW_HIDDEN_FILES_ENABLE);
	g_signal_emit_by_name (window, "hidden_files_mode_changed");
}

NemoWindowSlot *
nemo_window_get_active_slot (NemoWindow *window)
{
	g_assert (NEMO_IS_WINDOW (window));

	if (window->details->active_pane == NULL) {
		return NULL;
	}

	return window->details->active_pane->active_slot;
}

NemoWindowSlot *
nemo_window_get_extra_slot (NemoWindow *window)
{
	NemoWindowPane *extra_pane;
	GList *node;

	g_assert (NEMO_IS_WINDOW (window));

	/* return NULL if there is only one pane */
	if (window->details->panes == NULL ||
	    window->details->panes->next == NULL) {
		return NULL;
	}

	/* get next pane in the (wrapped around) list */
	node = g_list_find (window->details->panes,
			    window->details->active_pane);
	g_return_val_if_fail (node, FALSE);

	if (node->next) {
		extra_pane = node->next->data;
	}
	else {
		extra_pane =  window->details->panes->data;
	}

	return extra_pane->active_slot;
}

GList *
nemo_window_get_panes (NemoWindow *window)
{
	g_assert (NEMO_IS_WINDOW (window));

	return window->details->panes;
}

static void
window_set_search_action_text (NemoWindow *window,
			       gboolean setting)
{
	GtkAction *action;
	NemoWindowPane *pane;
	GList *l;

	for (l = window->details->panes; l != NULL; l = l->next) {
		pane = l->data;
		action = gtk_action_group_get_action (pane->action_group,
						      NEMO_ACTION_SEARCH);

		gtk_action_set_is_important (action, setting);
	}
}

static void
center_pane_divider (GtkWidget  *paned,
                     GParamSpec *pspec,
                     gpointer    user_data)
{
    /* Make the paned think it's been manually resized, otherwise
     * things like the trash bar will force unwanted resizes */
    GtkOrientation orientation = gtk_orientable_get_orientation (GTK_ORIENTABLE (paned));
    gint size = (orientation == GTK_ORIENTATION_VERTICAL)
        ? gtk_widget_get_allocated_height (paned)
        : gtk_widget_get_allocated_width (paned);

    g_object_set (G_OBJECT (paned),
                  "position", size / 2,
                  NULL);

    g_signal_handlers_disconnect_by_func (G_OBJECT (paned), center_pane_divider, NULL);
}

static NemoWindowSlot *
create_extra_pane (NemoWindow *window)
{
	NemoWindowPane *pane;
	NemoWindowSlot *slot;
	GtkPaned *paned;

	/* New pane */
	pane = nemo_window_pane_new (window);
	window->details->panes = g_list_append (window->details->panes, pane);

	paned = GTK_PANED (window->details->split_view_hpane);

    g_signal_connect_after (paned,
                            "notify::position",
                            G_CALLBACK(center_pane_divider),
                            NULL);

	if (gtk_paned_get_child1 (paned) == NULL) {
		gtk_paned_pack1 (paned, GTK_WIDGET (pane), TRUE, FALSE);
	} else {
		gtk_paned_pack2 (paned, GTK_WIDGET (pane), TRUE, FALSE);
	}

	/* Ensure the toolbar doesn't pop itself into existence (double toolbars suck.) */
	gtk_widget_hide (pane->tool_bar);

	/* slot */
	slot = nemo_window_pane_open_slot (NEMO_WINDOW_PANE (pane),
					       NEMO_WINDOW_OPEN_SLOT_APPEND);
	pane->active_slot = slot;

	return slot;
}

static void
nemo_window_reload (NemoWindow *window)
{
	NemoWindowSlot *active_slot;
	active_slot = nemo_window_get_active_slot (window);
	nemo_window_slot_queue_reload (active_slot, TRUE);
}

static gboolean
nemo_window_state_event (GtkWidget *widget,
			     GdkEventWindowState *event)
{
	if ((event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) && !nemo_window_is_desktop (NEMO_WINDOW (widget))) {
		g_settings_set_boolean (nemo_window_state, NEMO_WINDOW_STATE_MAXIMIZED,
					event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED);
	}

	if (GTK_WIDGET_CLASS (nemo_window_parent_class)->window_state_event != NULL) {
		return GTK_WIDGET_CLASS (nemo_window_parent_class)->window_state_event (widget, event);
	}

	return FALSE;
}

static gboolean
nemo_window_delete_event (GtkWidget *widget,
			      GdkEventAny *event)
{
	nemo_window_close (NEMO_WINDOW (widget));
	return FALSE;
}

static gboolean
nemo_window_button_press_event (GtkWidget *widget,
				    GdkEventButton *event)
{
	NemoWindow *window;
	gboolean handled;

	window = NEMO_WINDOW (widget);

	if (mouse_extra_buttons && (event->button == mouse_back_button)) {
		nemo_window_back_or_forward (window, TRUE, 0, 0);
		handled = TRUE;
	} else if (mouse_extra_buttons && (event->button == mouse_forward_button)) {
		nemo_window_back_or_forward (window, FALSE, 0, 0);
		handled = TRUE;
	} else if (GTK_WIDGET_CLASS (nemo_window_parent_class)->button_press_event) {
		handled = GTK_WIDGET_CLASS (nemo_window_parent_class)->button_press_event (widget, event);
	} else {
		handled = FALSE;
	}
	return handled;
}

static void
mouse_back_button_changed (gpointer callback_data)
{
	int new_back_button;

	new_back_button = g_settings_get_int (nemo_preferences, NEMO_PREFERENCES_MOUSE_BACK_BUTTON);

	/* Bounds checking */
	if (new_back_button < 6 || new_back_button > UPPER_MOUSE_LIMIT)
		return;

	mouse_back_button = new_back_button;
}

static void
mouse_forward_button_changed (gpointer callback_data)
{
	int new_forward_button;

	new_forward_button = g_settings_get_int (nemo_preferences, NEMO_PREFERENCES_MOUSE_FORWARD_BUTTON);

	/* Bounds checking */
	if (new_forward_button < 6 || new_forward_button > UPPER_MOUSE_LIMIT)
		return;

	mouse_forward_button = new_forward_button;
}

static void
use_extra_mouse_buttons_changed (gpointer callback_data)
{
	mouse_extra_buttons = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_MOUSE_USE_EXTRA_BUTTONS);
}

/*
 * Main API
 */

static void
nemo_window_init (NemoWindow *window)
{
    GtkWindowGroup *window_group;

	window->details = G_TYPE_INSTANCE_GET_PRIVATE (window, NEMO_TYPE_WINDOW, NemoWindowDetails);

	window->details->panes = NULL;
	window->details->active_pane = NULL;

    gboolean show_hidden = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_HIDDEN_FILES);

    window->details->show_hidden_files_mode = show_hidden ? NEMO_WINDOW_SHOW_HIDDEN_FILES_ENABLE :
                                                            NEMO_WINDOW_SHOW_HIDDEN_FILES_DISABLE;

    window->details->show_sidebar = g_settings_get_boolean (nemo_window_state,
                                                            NEMO_WINDOW_STATE_START_WITH_SIDEBAR);

    window->details->menu_skip_release = FALSE;
    window->details->menu_show_queued = FALSE;

    window->details->ignore_meta_view_id = NULL;
    window->details->ignore_meta_zoom_level = -1;
    window->details->ignore_meta_visible_columns = NULL;
    window->details->ignore_meta_column_order = NULL;
    window->details->ignore_meta_sort_column = NULL;
    window->details->ignore_meta_sort_direction = SORT_NULL;

	/* This makes it possible for GTK+ themes to apply styling that is specific to Nemo
	 * without affecting other GTK+ applications.
	 */
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (window)), "nemo-window");

	window_group = gtk_window_group_new ();
	gtk_window_group_add_window (window_group, GTK_WINDOW (window));
	g_object_unref (window_group);

	/* Set initial window title */
	gtk_window_set_title (GTK_WINDOW (window), _("Nemo"));

    g_signal_connect_swapped (nemo_preferences,
				  "changed::" NEMO_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
				  G_CALLBACK(nemo_window_sync_thumbnail_action),
				  window);
    g_signal_connect_swapped (nemo_preferences,
				  "changed::" NEMO_PREFERENCES_INHERIT_SHOW_THUMBNAILS,
				  G_CALLBACK(nemo_window_sync_thumbnail_action),
				  window);

    /* Respond to dual pane preference changes */
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT,
                  G_CALLBACK (dual_pane_prefs_changed),
                  window);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR,
                  G_CALLBACK (dual_pane_prefs_changed),
                  window);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR,
                  G_CALLBACK (dual_pane_prefs_changed),
                  window);
}

static NemoIconInfo *
real_get_icon (NemoWindow *window,
               NemoWindowSlot *slot)
{
        return nemo_file_get_icon (slot->viewed_file, 48, 0,
                       gtk_widget_get_scale_factor (GTK_WIDGET (window)),
				       NEMO_FILE_ICON_FLAGS_IGNORE_VISITING |
				       NEMO_FILE_ICON_FLAGS_USE_MOUNT_ICON);
}

static void
real_window_close (NemoWindow *window)
{
	g_return_if_fail (NEMO_IS_WINDOW (window));

	nemo_window_save_geometry (window);

	gtk_widget_destroy (GTK_WIDGET (window));
}

static void
nemo_window_class_init (NemoWindowClass *class)
{
	GtkBindingSet *binding_set;
	GObjectClass *oclass = G_OBJECT_CLASS (class);
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (class);

	oclass->finalize = nemo_window_finalize;
	oclass->constructed = nemo_window_constructed;
	oclass->get_property = nemo_window_get_property;
	oclass->set_property = nemo_window_set_property;

	wclass->destroy = nemo_window_destroy;
	wclass->show = nemo_window_show;
	wclass->realize = nemo_window_realize;
	wclass->key_press_event = nemo_window_key_press_event;
    wclass->key_release_event = nemo_window_key_release_event;
	wclass->window_state_event = nemo_window_state_event;
	wclass->button_press_event = nemo_window_button_press_event;
	wclass->delete_event = nemo_window_delete_event;

	class->get_icon = real_get_icon;
	class->close = real_window_close;

	properties[PROP_DISABLE_CHROME] =
		g_param_spec_boolean ("disable-chrome",
				      "Disable chrome",
				      "Disable window chrome, for the desktop",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				      G_PARAM_STATIC_STRINGS);

    properties[PROP_SIDEBAR_VIEW_TYPE] =
        g_param_spec_string ("sidebar-view-id",
                      "Sidebar view type",
                      "Sidebar view type",
                      NULL,
                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    properties[PROP_SHOW_SIDEBAR] =
        g_param_spec_boolean ("show-sidebar",
                              "Show the sidebar",
                              "Show the sidebar",
                              FALSE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	signals[GO_UP] =
		g_signal_new ("go-up",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (NemoWindowClass, go_up),
			      NULL, NULL,
			      g_cclosure_marshal_generic,
			      G_TYPE_NONE, 0);
	signals[RELOAD] =
		g_signal_new ("reload",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (NemoWindowClass, reload),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[PROMPT_FOR_LOCATION] =
		g_signal_new ("prompt-for-location",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (NemoWindowClass, prompt_for_location),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals[HIDDEN_FILES_MODE_CHANGED] =
		g_signal_new ("hidden_files_mode_changed",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[LOADING_URI] =
		g_signal_new ("loading_uri",
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
			      G_TYPE_NONE, 1, NEMO_TYPE_WINDOW_SLOT);
	signals[SLOT_REMOVED] =
		g_signal_new ("slot-removed",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, NEMO_TYPE_WINDOW_SLOT);

	binding_set = gtk_binding_set_by_class (class);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_BackSpace, 0,
				      "go-up", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_F5, 0,
				      "reload", 0);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_slash, 0,
				      "prompt-for-location", 1,
				      G_TYPE_STRING, "/");
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Divide, 0,
				      "prompt-for-location", 1,
				      G_TYPE_STRING, "/");
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_asciitilde, 0,
				      "prompt-for-location", 1,
				      G_TYPE_STRING, "~");

	class->reload = nemo_window_reload;
	class->go_up = nemo_window_go_up_signal;
	class->prompt_for_location = nemo_window_prompt_for_location;

	g_signal_connect_swapped (nemo_preferences,
				  "changed::" NEMO_PREFERENCES_MOUSE_BACK_BUTTON,
				  G_CALLBACK(mouse_back_button_changed),
				  NULL);

	g_signal_connect_swapped (nemo_preferences,
				  "changed::" NEMO_PREFERENCES_MOUSE_FORWARD_BUTTON,
				  G_CALLBACK(mouse_forward_button_changed),
				  NULL);

	g_signal_connect_swapped (nemo_preferences,
				  "changed::" NEMO_PREFERENCES_MOUSE_USE_EXTRA_BUTTONS,
				  G_CALLBACK(use_extra_mouse_buttons_changed),
				  NULL);

	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
	g_type_class_add_private (oclass, sizeof (NemoWindowDetails));
}

NemoWindow *
nemo_window_new (GtkApplication *application,
                 GdkScreen *screen)
{
	return g_object_new (NEMO_TYPE_WINDOW,
			     "application", application,
			     "screen", screen,
			     NULL);
}

/* ---- Dual pane helper functions ---- */

/*
 * Per-pane layout helpers.
 *
 * When "separate sidebar per pane" or "separate nav bar per pane" is active in
 * vertical (stacked) dual-pane mode, each pane column looks like:
 *
 *   pack_paned (HPaned)
 *     ├── sidebar_box  (pack1, fixed width)
 *     └── content_vbox (pack2, expanding)
 *           ├── toolbar   (optional, via embed_toolbar)
 *           └── pane      (NemoWindowPane)
 *
 * For pane1 we move the primary sidebar OUT of content_paned and INTO a new
 * inline HPaned that is then packed as child1 of split_view_hpane.
 * For pane2 we wrap it in the same structure as child2 of split_view_hpane.
 */

/* ---------- pane2 (secondary) wrapper ---------- */

static void
nemo_window_set_up_sidebar2 (NemoWindow *window)
{
    GtkWidget *sidebar2_widget;
    const gchar *sidebar_id;
    GList *last;
    NemoWindowPane *pane2;
    GtkWidget *pane2_widget;
    GtkPaned *hpane;
    GtkWidget *child2;
    GtkWidget *sec_paned;
    GtkWidget *content_vbox;

    if (!nemo_window_split_view_showing (window)) {
        return;
    }

    sidebar_id = window->details->sidebar_id;
    if (!sidebar_id || (g_strcmp0 (sidebar_id, NEMO_WINDOW_SIDEBAR_PLACES) != 0 &&
                        g_strcmp0 (sidebar_id, NEMO_WINDOW_SIDEBAR_TREE) != 0)) {
        return;
    }

    last = g_list_last (window->details->panes);
    if (!last || last->data == window->details->panes->data) {
        return;
    }
    pane2 = last->data;
    pane2_widget = GTK_WIDGET (pane2);

    if (window->details->secondary_pane_content_paned != NULL) {
        return;
    }

    /* pane2 may already be inside a content_vbox (from embed_toolbar).
     * Whichever widget is currently child2 of split_view_hpane is what we wrap. */
    hpane = GTK_PANED (window->details->split_view_hpane);
    child2 = gtk_paned_get_child2 (hpane);
    if (child2 == NULL) {
        return;
    }

    g_object_ref (child2);
    gtk_container_remove (GTK_CONTAINER (hpane), child2);

    /* If pane2 doesn't yet have a content_vbox (embed_toolbar not called), wrap it now */
    if (child2 == pane2_widget) {
        content_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_show (content_vbox);
        gtk_box_pack_start (GTK_BOX (content_vbox), pane2_widget, TRUE, TRUE, 0);
        g_object_unref (child2);
        child2 = content_vbox;
        g_object_ref (child2);
    }

    /* Create outer HPaned: sidebar2 | content_vbox */
    sec_paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
    window->details->secondary_pane_content_paned = sec_paned;
    gtk_widget_show (sec_paned);

    /* Create sidebar2 box locked to pane2 */
    window->details->sidebar2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class (gtk_widget_get_style_context (window->details->sidebar2),
                                 GTK_STYLE_CLASS_SIDEBAR);

    if (g_strcmp0 (sidebar_id, NEMO_WINDOW_SIDEBAR_PLACES) == 0) {
        sidebar2_widget = nemo_places_sidebar_new_for_pane (window, pane2);
    } else {
        sidebar2_widget = nemo_tree_sidebar_new_for_pane (window, pane2);
    }

    gtk_box_pack_start (GTK_BOX (window->details->sidebar2), sidebar2_widget, TRUE, TRUE, 0);
    gtk_widget_show (sidebar2_widget);
    gtk_widget_show (window->details->sidebar2);

    /* sidebar2 on left (fixed), content_vbox on right (expanding) */
    gtk_paned_pack1 (GTK_PANED (sec_paned), window->details->sidebar2, FALSE, FALSE);
    gtk_paned_pack2 (GTK_PANED (sec_paned), child2, TRUE, FALSE);
    g_object_unref (child2);

    gtk_paned_set_position (GTK_PANED (sec_paned),
                            g_settings_get_int (nemo_window_state, NEMO_WINDOW_STATE_SIDEBAR_WIDTH));

    gtk_paned_pack2 (GTK_PANED (hpane), sec_paned, TRUE, FALSE);

    /* Sync sidebar2 active/inactive CSS class to reflect current active pane */
    {
        GtkStyleContext *s2 = gtk_widget_get_style_context (window->details->sidebar2);
        gboolean pane2_active = (window->details->active_pane == pane2);
        if (pane2_active) {
            gtk_style_context_remove_class (s2, "nemo-inactive-pane");
            gtk_style_context_add_class    (s2, "nemo-active-sidebar");
        } else {
            gtk_style_context_remove_class (s2, "nemo-active-sidebar");
            gtk_style_context_add_class    (s2, "nemo-inactive-pane");
        }
        gtk_widget_reset_style (window->details->sidebar2);
    }
}

static void
nemo_window_tear_down_sidebar2 (NemoWindow *window)
{
    GList *last;
    NemoWindowPane *pane2;
    GtkWidget *pane2_widget;
    GtkPaned *hpane;
    GtkWidget *sec_paned;
    GtkWidget *restore_widget;  /* what gets packed back into hpane as child2 */

    if (window->details->secondary_pane_content_paned == NULL) {
        return;
    }

    sec_paned = window->details->secondary_pane_content_paned;
    window->details->secondary_pane_content_paned = NULL;
    window->details->sidebar2 = NULL;

    if (!nemo_window_split_view_showing (window)) {
        return;
    }

    last = g_list_last (window->details->panes);
    if (!last || last->data == window->details->panes->data) {
        return;
    }
    pane2 = last->data;
    pane2_widget = GTK_WIDGET (pane2);

    hpane = GTK_PANED (window->details->split_view_hpane);

    /* Walk up from pane2 to find the direct child of sec_paned that contains
     * it (may be pane2 itself, or a content_vbox wrapping it). */
    restore_widget = pane2_widget;
    {
        GtkWidget *p = gtk_widget_get_parent (pane2_widget);
        while (p != NULL && p != sec_paned) {
            restore_widget = p;
            p = gtk_widget_get_parent (p);
        }
        if (p == NULL) {
            return;
        }
    }

    /* Safely lift restore_widget out of sec_paned before destroying sec_paned */
    g_object_ref (restore_widget);
    gtk_container_remove (GTK_CONTAINER (sec_paned), restore_widget);

    /* Now sec_paned only has sidebar2 left; remove it from hpane and destroy it.
     * sidebar2 is destroyed with it - that is safe, it has no panes inside.
     * Hold a ref across the remove so the widget isn't freed before destroy. */
    g_object_ref (sec_paned);
    if (gtk_widget_get_parent (sec_paned) == GTK_WIDGET (hpane)) {
        gtk_container_remove (GTK_CONTAINER (hpane), sec_paned);
    }
    gtk_widget_destroy (sec_paned);
    g_object_unref (sec_paned);

    /* Put pane2 (or its vbox wrapper) back as child2 of hpane */
    gtk_paned_pack2 (GTK_PANED (hpane), restore_widget, TRUE, FALSE);
    g_object_unref (restore_widget);

}

/* Rebuild the inner sidebar1 widget with or without pane1 locking.
 * When per-pane mode is active, sidebar1 must be locked to pane1 so it only
 * updates when pane1 navigates, not when pane2 does. */
static void
nemo_window_refresh_sidebar1_pane_lock (NemoWindow *window, gboolean lock_to_pane1)
{
    GtkWidget *sidebar_box;
    GtkWidget *old_widget;
    GtkWidget *new_widget;
    GList *children;
    NemoWindowPane *pane1;
    const gchar *sidebar_id;

    if (window->details->sidebar == NULL) {
        return;
    }

    sidebar_id = window->details->sidebar_id;
    if (!sidebar_id || (g_strcmp0 (sidebar_id, NEMO_WINDOW_SIDEBAR_PLACES) != 0 &&
                        g_strcmp0 (sidebar_id, NEMO_WINDOW_SIDEBAR_TREE) != 0)) {
        return;
    }

    sidebar_box = window->details->sidebar;
    pane1 = window->details->panes ? window->details->panes->data : NULL;

    /* Remove the existing inner widget */
    children = gtk_container_get_children (GTK_CONTAINER (sidebar_box));
    if (children != NULL) {
        old_widget = GTK_WIDGET (children->data);
        gtk_container_remove (GTK_CONTAINER (sidebar_box), old_widget);
        g_list_free (children);
    }

    /* Create a replacement with or without pane lock */
    if (lock_to_pane1 && pane1 != NULL) {
        if (g_strcmp0 (sidebar_id, NEMO_WINDOW_SIDEBAR_PLACES) == 0) {
            new_widget = nemo_places_sidebar_new_for_pane (window, pane1);
        } else {
            new_widget = nemo_tree_sidebar_new_for_pane (window, pane1);
        }
    } else {
        if (g_strcmp0 (sidebar_id, NEMO_WINDOW_SIDEBAR_PLACES) == 0) {
            new_widget = nemo_places_sidebar_new (window);
        } else {
            new_widget = nemo_tree_sidebar_new (window);
        }
    }

    gtk_box_pack_start (GTK_BOX (sidebar_box), new_widget, TRUE, TRUE, 0);
    gtk_widget_show (new_widget);
}

/* ---------- pane1 (primary) wrapper ----------
 *
 * Moves the primary sidebar from content_paned (where it spans full height)
 * into an inline HPaned that lives as child1 of split_view_hpane, so that
 * pane1 has its own sidebar column just like pane2.
 */

static void
nemo_window_set_up_pane1_wrapper (NemoWindow *window)
{
    NemoWindowPane *pane1;
    GtkWidget *pane1_widget;
    GtkWidget *sidebar1;
    GtkPaned *split_hpane;
    GtkWidget *child1;
    GtkWidget *pri_paned;
    GtkWidget *content_vbox;

    if (window->details->primary_pane_content_paned != NULL) {
        return;  /* Already set up */
    }

    if (!window->details->show_sidebar || window->details->sidebar == NULL) {
        return;  /* No sidebar to move */
    }

    pane1 = window->details->panes->data;
    pane1_widget = GTK_WIDGET (pane1);
    split_hpane = GTK_PANED (window->details->split_view_hpane);

    /* child1 of split_view_hpane is either pane1 directly, or a content_vbox
     * containing toolbar + pane1 (if embed_toolbar was called for pane1). */
    child1 = gtk_paned_get_child1 (split_hpane);
    if (child1 == NULL) {
        return;
    }

    g_object_ref (child1);
    gtk_container_remove (GTK_CONTAINER (split_hpane), child1);

    /* If child1 is the bare pane, wrap it in a content_vbox so toolbar can
     * be added later consistently */
    if (child1 == pane1_widget) {
        content_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_show (content_vbox);
        gtk_box_pack_start (GTK_BOX (content_vbox), pane1_widget, TRUE, TRUE, 0);
        g_object_unref (child1);
        child1 = content_vbox;
        g_object_ref (child1);
    }

    /* Move the primary sidebar widget out of content_paned and into a new
     * inline HPaned.  We take the sidebar box (window->details->sidebar). */
    sidebar1 = window->details->sidebar;

    g_object_ref (sidebar1);
    gtk_container_remove (GTK_CONTAINER (window->details->content_paned), sidebar1);

    /* Hide content_paned's now-empty pack1 gap by setting position to 0 */
    gtk_paned_set_position (GTK_PANED (window->details->content_paned), 0);

    /* Create inline HPaned for pane1: sidebar1 | content_vbox */
    pri_paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
    window->details->primary_pane_content_paned = pri_paned;
    gtk_widget_show (pri_paned);

    gtk_paned_pack1 (GTK_PANED (pri_paned), sidebar1, FALSE, FALSE);
    g_object_unref (sidebar1);
    gtk_paned_pack2 (GTK_PANED (pri_paned), child1, TRUE, FALSE);
    g_object_unref (child1);

    gtk_paned_set_position (GTK_PANED (pri_paned),
                            g_settings_get_int (nemo_window_state, NEMO_WINDOW_STATE_SIDEBAR_WIDTH));

    gtk_paned_pack1 (GTK_PANED (split_hpane), pri_paned, TRUE, FALSE);

    /* Lock sidebar1 to pane1 now that we are in per-pane mode */
    nemo_window_refresh_sidebar1_pane_lock (window, TRUE);

    /* Add a top separator to sidebar1 to restore the border that was previously
     * provided by toolbar_holder sitting above it */
    if (sidebar1 != NULL) {
        GtkWidget *sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start (GTK_BOX (sidebar1), sep, FALSE, FALSE, 0);
        gtk_box_reorder_child (GTK_BOX (sidebar1), sep, 0);
        gtk_widget_show (sep);
        g_object_set_data (G_OBJECT (sidebar1), "pane-wrapper-top-sep", sep);
    }

    /* Sync sidebar1 active/inactive CSS class to reflect current active pane.
     * sidebar_set_active_style is defined in nemo-window-pane.c; we replicate
     * the same logic here to avoid adding a cross-file helper just for this. */
    {
        GtkStyleContext *s1 = gtk_widget_get_style_context (window->details->sidebar);
        gboolean pane1_active = (window->details->active_pane == pane1);
        if (pane1_active) {
            gtk_style_context_remove_class (s1, "nemo-inactive-pane");
            gtk_style_context_add_class    (s1, "nemo-active-sidebar");
        } else {
            gtk_style_context_remove_class (s1, "nemo-active-sidebar");
            gtk_style_context_add_class    (s1, "nemo-inactive-pane");
        }
        gtk_widget_reset_style (window->details->sidebar);
    }

}

static void
nemo_window_tear_down_pane1_wrapper (NemoWindow *window)
{
    NemoWindowPane *pane1;
    GtkWidget *pane1_widget;
    GtkWidget *sidebar1;
    GtkPaned *split_hpane;
    GtkWidget *pri_paned;
    GtkWidget *restore_widget;

    if (window->details->primary_pane_content_paned == NULL) {
        return;
    }

    pri_paned = window->details->primary_pane_content_paned;
    window->details->primary_pane_content_paned = NULL;

    pane1 = window->details->panes->data;
    pane1_widget = GTK_WIDGET (pane1);
    split_hpane = GTK_PANED (window->details->split_view_hpane);
    sidebar1 = window->details->sidebar;

    /* Walk up from pane1 to find the direct child of pri_paned that contains it */
    restore_widget = pane1_widget;
    {
        GtkWidget *p = gtk_widget_get_parent (pane1_widget);
        while (p != NULL && p != GTK_WIDGET (pri_paned)) {
            restore_widget = p;
            p = gtk_widget_get_parent (p);
        }
        if (p == NULL) {
            goto restore_sidebar;
        }
    }

    /* Step 1: Rescue sidebar1 from pri_paned BEFORE we touch anything else.
     * This must happen first so content_paned.pack1 is populated before
     * pri_paned is unparented — GTK may re-layout between operations. */
    if (sidebar1 != NULL && gtk_widget_get_parent (sidebar1) == GTK_WIDGET (pri_paned)) {
        g_object_ref (sidebar1);
        gtk_container_remove (GTK_CONTAINER (pri_paned), sidebar1);

        /* Remove the top separator we added when entering per-pane mode */
        {
            GtkWidget *sep = g_object_get_data (G_OBJECT (sidebar1), "pane-wrapper-top-sep");
            if (sep != NULL) {
                gtk_widget_destroy (sep);
                g_object_set_data (G_OBJECT (sidebar1), "pane-wrapper-top-sep", NULL);
            }
        }

        /* sidebar1 goes back into content_paned pack1 at full window height */
        gtk_paned_pack1 (GTK_PANED (window->details->content_paned),
                         sidebar1, FALSE, FALSE);
        g_object_unref (sidebar1);
        gtk_paned_set_position (GTK_PANED (window->details->content_paned),
                                g_settings_get_int (nemo_window_state,
                                                    NEMO_WINDOW_STATE_SIDEBAR_WIDTH));
    } else {
    }

    /* Step 2: Extract restore_widget (content_vbox containing pane1) from
     * pri_paned and put it back as child1 of split_hpane. */
    g_object_ref (restore_widget);
    gtk_container_remove (GTK_CONTAINER (pri_paned), restore_widget);

    /* Step 3: Remove pri_paned from split_hpane and destroy it.
     * Hold a ref so the widget isn't freed by the remove before destroy. */
    g_object_ref (pri_paned);
    if (gtk_widget_get_parent (GTK_WIDGET (pri_paned)) == GTK_WIDGET (split_hpane)) {
        gtk_container_remove (GTK_CONTAINER (split_hpane), GTK_WIDGET (pri_paned));
    }
    gtk_widget_destroy (GTK_WIDGET (pri_paned));
    g_object_unref (pri_paned);

    /* Step 4: Restore pane1 (or its vbox) as child1 of split_hpane. */
    gtk_paned_pack1 (GTK_PANED (split_hpane), restore_widget, TRUE, FALSE);
    g_object_unref (restore_widget);

    /* Step 5: Unlock sidebar1 from pane1 now that we are leaving per-pane mode. */
    nemo_window_refresh_sidebar1_pane_lock (window, FALSE);

    return;

restore_sidebar:
    /* Fallback: pane1 was not inside pri_paned (already moved). Still need to
     * rescue sidebar1 from pri_paned back to content_paned if it's there. */
    if (sidebar1 != NULL && gtk_widget_get_parent (sidebar1) == GTK_WIDGET (pri_paned)) {
        g_object_ref (sidebar1);
        gtk_container_remove (GTK_CONTAINER (pri_paned), sidebar1);
        gtk_paned_pack1 (GTK_PANED (window->details->content_paned),
                         sidebar1, FALSE, FALSE);
        g_object_unref (sidebar1);
        gtk_paned_set_position (GTK_PANED (window->details->content_paned),
                                g_settings_get_int (nemo_window_state,
                                                    NEMO_WINDOW_STATE_SIDEBAR_WIDTH));
    }
    g_object_ref (pri_paned);
    if (gtk_widget_get_parent (GTK_WIDGET (pri_paned)) == GTK_WIDGET (split_hpane)) {
        gtk_container_remove (GTK_CONTAINER (split_hpane), GTK_WIDGET (pri_paned));
    }
    gtk_widget_destroy (GTK_WIDGET (pri_paned));
    g_object_unref (pri_paned);
    nemo_window_refresh_sidebar1_pane_lock (window, FALSE);
}

/* Recreate the split_view_hpane with the new orientation */
static void
nemo_window_update_split_view_orientation (NemoWindow *window)
{
    gboolean vertical;
    GtkOrientation desired;
    GtkOrientation current;
    GtkWidget *old_paned;
    GtkWidget *parent;
    GtkWidget *child1;
    GtkWidget *child2;
    GtkWidget *new_paned;

    vertical = g_settings_get_boolean (nemo_preferences,
                                       NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT);
    desired = vertical ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL;

    old_paned = window->details->split_view_hpane;
    if (old_paned == NULL) {
        return;
    }

    current = gtk_orientable_get_orientation (GTK_ORIENTABLE (old_paned));
    if (current == desired) {
        return;  /* Nothing to do */
    }

    /* Get parent container (the vbox) */
    parent = gtk_widget_get_parent (old_paned);
    if (parent == NULL) {
        return;
    }

    /* Collect children from the old paned */
    child1 = gtk_paned_get_child1 (GTK_PANED (old_paned));
    child2 = gtk_paned_get_child2 (GTK_PANED (old_paned));

    if (child1) g_object_ref (child1);
    if (child2) g_object_ref (child2);
    if (child1) gtk_container_remove (GTK_CONTAINER (old_paned), child1);
    if (child2) gtk_container_remove (GTK_CONTAINER (old_paned), child2);

    gtk_container_remove (GTK_CONTAINER (parent), old_paned);

    /* Create new paned with desired orientation */
    new_paned = gtk_paned_new (desired);
    gtk_box_pack_start (GTK_BOX (parent), new_paned, TRUE, TRUE, 0);
    gtk_widget_show (new_paned);
    window->details->split_view_hpane = new_paned;

    /* Re-attach children */
    if (child1) {
        gtk_paned_pack1 (GTK_PANED (new_paned), child1, TRUE, FALSE);
        g_object_unref (child1);
    }
    if (child2) {
        gtk_paned_pack2 (GTK_PANED (new_paned), child2, TRUE, FALSE);
        g_object_unref (child2);
    }

    /* Center the divider */
    if (child2) {
        g_signal_connect_after (new_paned,
                                "notify::position",
                                G_CALLBACK (center_pane_divider),
                                NULL);
    }
}

/* Refresh sidebar background colours to match current per-pane state.
 *
 * Called after any operation that creates, destroys, or changes sidebars:
 *   - split_view_on / split_view_off
 *   - show_sidebar / hide_sidebar
 *   - side_pane_id_changed (Places <-> Tree switch)
 *   - dual_pane_prefs_changed (any of the three dual-pane prefs)
 *
 * Rules:
 *   Per-pane mode active (both sidebars visible):
 *     - sidebar for the active pane   → GTK_STYLE_CLASS_SIDEBAR removed (normal bg)
 *     - sidebar for the inactive pane → GTK_STYLE_CLASS_SIDEBAR kept + nemo-inactive-pane
 *   Single sidebar / not in per-pane mode:
 *     - sidebar1 → GTK_STYLE_CLASS_SIDEBAR restored (theme default, no custom class)
 */
static void
nemo_window_refresh_sidebar_colours (NemoWindow *window)
{
    gboolean per_pane_mode;
    NemoWindowPane *active_pane;
    NemoWindowPane *pane1;
    GList *last;
    NemoWindowPane *pane2;

    if (window->details->sidebar == NULL) {
        return;
    }

    per_pane_mode = (window->details->primary_pane_content_paned != NULL) &&
                    (window->details->sidebar2 != NULL);

    if (per_pane_mode) {
        active_pane = nemo_window_get_active_pane (window);
        pane1 = window->details->panes ? window->details->panes->data : NULL;
        last  = g_list_last (window->details->panes);
        pane2 = (last && last->data != pane1) ? last->data : NULL;

        gboolean pane1_active = (active_pane == pane1);

        nemo_window_pane_set_active (pane1,  pane1_active);
        if (pane2) {
            nemo_window_pane_set_active (pane2, !pane1_active);
        }
    } else {
        /* Not in per-pane mode: restore sidebar1 to normal themed state.
         * Remove any custom classes we may have set, restore GTK_STYLE_CLASS_SIDEBAR. */
        GtkStyleContext *style = gtk_widget_get_style_context (window->details->sidebar);
        gtk_style_context_remove_class (style, "nemo-inactive-pane");
        if (!gtk_style_context_has_class (style, GTK_STYLE_CLASS_SIDEBAR)) {
            gtk_style_context_add_class (style, GTK_STYLE_CLASS_SIDEBAR);
        }
        gtk_widget_reset_style (window->details->sidebar);
        gtk_widget_queue_draw  (window->details->sidebar);
    }
}

static void
dual_pane_prefs_changed (gpointer callback_data)
{
    NemoWindow *window = NEMO_WINDOW (callback_data);
    gboolean split_showing;
    gboolean want_per_pane;
    gboolean want_sidebar2;
    GList *last;
    NemoWindowPane *pane1;
    NemoWindowPane *pane2;
    gboolean vertical;
    gboolean separate_nav;
    gboolean separate_sidebar;

    split_showing = nemo_window_split_view_showing (window);
    vertical = g_settings_get_boolean (nemo_preferences,
                                       NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT);
    separate_nav = g_settings_get_boolean (nemo_preferences,
                                           NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR);
    separate_sidebar = g_settings_get_boolean (nemo_preferences,
                                               NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR);

    /* Handle orientation change - must happen before any wrapper setup */
    nemo_window_update_split_view_orientation (window);

    /* The pane1 sidebar wrapper (pri_paned) is ONLY needed when separate_sidebar
     * is ON.  Per spec 3.5.3, toolbar embedding (separate_nav) is independent of
     * sidebar layout — when nav bar is ON but sidebar is OFF the sidebar stays
     * full-height in content_paned and pri_paned must NOT be created.
     *
     * want_per_pane  = controls the pane1 sidebar wrapper (pri_paned)
     * want_sidebar2  = controls the pane2 sidebar wrapper (sec_paned + sidebar2) */
    want_per_pane = split_showing && vertical && separate_sidebar &&
                    window->details->show_sidebar;

    want_sidebar2 = want_per_pane;

    /* --- Sidebar wrapper teardown/setup (order is critical, per spec section 5):
     *
     * TEARDOWN order: pane1_wrapper first, then sidebar2.
     *   tear_down_pane1_wrapper() rescues sidebar1 back into content_paned.
     *   tear_down_sidebar2() then finds pane2 cleanly inside hpane.child2.
     *   Reversing this order leaves sidebar1 stranded inside a half-destroyed
     *   pri_paned when sidebar2 tries to walk the widget tree from pane2.
     *
     * SETUP order: sidebar2 first, then pane1_wrapper (matches build order).
     */

    /* --- pane1 sidebar wrapper (teardown only — setup happens after sidebar2) --- */
    if (!want_per_pane && window->details->primary_pane_content_paned != NULL) {
        nemo_window_tear_down_pane1_wrapper (window);
    }

    /* --- pane2 sidebar wrapper --- */
    if (want_sidebar2 && window->details->secondary_pane_content_paned == NULL) {
        nemo_window_set_up_sidebar2 (window);
    } else if (!want_sidebar2 && window->details->secondary_pane_content_paned != NULL) {
        nemo_window_tear_down_sidebar2 (window);
    }

    /* --- pane1 sidebar wrapper (setup only — teardown happened above) --- */
    if (want_per_pane && window->details->primary_pane_content_paned == NULL) {
        nemo_window_set_up_pane1_wrapper (window);
    }

    /* --- per-pane toolbars --- */
    if (split_showing) {
        pane1 = window->details->panes->data;
        last = g_list_last (window->details->panes);
        pane2 = (last && last->data != pane1) ? last->data : NULL;

        if (vertical && separate_nav) {
            /* Embed toolbar into each pane */
            nemo_window_pane_embed_toolbar (pane1);
            if (pane2) {
                nemo_window_pane_embed_toolbar (pane2);
            }
        } else {
            /* Detach pane toolbars back to toolbar_holder.
             * detach_toolbar() hides the toolbar after moving it (so only the
             * active pane's bar is visible in the shared holder).  After
             * detaching we must explicitly re-show the active pane's toolbar
             * because no set_active_slot() call will be triggered by the pref
             * change alone — without this the toolbar holder stays empty. */
            nemo_window_pane_detach_toolbar (pane1);
            if (pane2) {
                nemo_window_pane_detach_toolbar (pane2);
            }

            /* Re-show the active pane's toolbar in the holder, respecting the
             * toolbar visibility setting. */
            {
                NemoWindowPane *active_pane = nemo_window_get_active_pane (window);
                if (active_pane != NULL) {
                    GtkWidget *tb = GTK_WIDGET (active_pane->tool_bar);
                    gboolean show_toolbar = g_settings_get_boolean (nemo_window_state,
                                                                    NEMO_WINDOW_STATE_START_WITH_TOOLBAR);
                    if (show_toolbar &&
                        gtk_widget_get_parent (tb) == window->details->toolbar_holder) {
                        gtk_widget_show (tb);
                    }
                }
            }
        }
    }

    /* Apply sidebar background colours after any structural change. */
    nemo_window_refresh_sidebar_colours (window);
}

void
nemo_window_split_view_on (NemoWindow *window)
{
	NemoWindowSlot *slot, *old_active_slot;
	GFile *location;
    NemoWindowPane *pane2;
    gboolean vertical;
    gboolean separate_nav;

	old_active_slot = nemo_window_get_active_slot (window);
	slot = create_extra_pane (window);

    location = window->details->secondary_pane_last_location;

	if (location == NULL && old_active_slot != NULL) {
		location = nemo_window_slot_get_location (old_active_slot);
		if (location != NULL) {
			if (g_file_has_uri_scheme (location, "x-nemo-search")) {
				g_object_unref (location);
				location = NULL;
			}
		}
	}
	if (location == NULL) {
		location = g_file_new_for_path (g_get_home_dir ());
	}

	nemo_window_slot_open_location (slot, location, 0);
	g_object_unref (location);

	window_set_search_action_text (window, FALSE);

    /* Set up per-pane features if enabled */
    pane2 = slot->pane;

    vertical = g_settings_get_boolean (nemo_preferences,
                                       NEMO_PREFERENCES_DUAL_PANE_VERTICAL_SPLIT);
    separate_nav = g_settings_get_boolean (nemo_preferences,
                                           NEMO_PREFERENCES_DUAL_PANE_SEPARATE_NAV_BAR);
    gboolean separate_sidebar = g_settings_get_boolean (nemo_preferences,
                                                        NEMO_PREFERENCES_DUAL_PANE_SEPARATE_SIDEBAR);
    /* Per spec 3.5.3: toolbar embedding is independent of sidebar layout.
     * pane1 wrapper (pri_paned) is ONLY created when separate_sidebar is ON. */
    gboolean per_pane = vertical && separate_sidebar &&
                        window->details->show_sidebar;

    if (vertical && separate_nav) {
        NemoWindowPane *pane1 = window->details->panes->data;
        nemo_window_pane_embed_toolbar (pane1);
        nemo_window_pane_embed_toolbar (pane2);
    }

    if (per_pane) {
        nemo_window_set_up_pane1_wrapper (window);
        nemo_window_set_up_sidebar2 (window);
    }

    /* Apply sidebar background colours now that all sidebars are in place. */
    nemo_window_refresh_sidebar_colours (window);
}

void
nemo_window_split_view_off (NemoWindow *window)
{
	NemoWindowPane *pane, *active_pane;
	GList *l, *next;

	active_pane = nemo_window_get_active_pane (window);

    /* Tear down all per-pane wrappers before closing panes.
     * Order is critical (spec section 5): pane1_wrapper first, then sidebar2.
     * tear_down_pane1_wrapper rescues sidebar1 back to content_paned first;
     * tear_down_sidebar2 then finds pane2 cleanly in hpane.child2. */
    nemo_window_tear_down_pane1_wrapper (window);
    nemo_window_tear_down_sidebar2 (window);

	/* delete all panes except the first (main) pane */
	for (l = window->details->panes; l != NULL; l = next) {
		next = l->next;
		pane = l->data;
		if (pane != active_pane) {
            g_clear_object (&window->details->secondary_pane_last_location);
            window->details->secondary_pane_last_location = nemo_window_slot_get_location (pane->active_slot);
            /* Detach embedded toolbar back to toolbar_holder before destroying pane */
            nemo_window_pane_detach_toolbar (pane);
			nemo_window_close_pane (window, pane);
		}
	}

    /* Also detach pane1 toolbar (pane1 might have had its toolbar embedded) */
    if (window->details->panes != NULL) {
        NemoWindowPane *pane1 = window->details->panes->data;
        nemo_window_pane_detach_toolbar (pane1);
    }

    /* Reset split view pane's position so the position can be
     * caught again later */
    g_object_set (G_OBJECT (window->details->split_view_hpane),
                  "position", 0,
                  "position-set", FALSE,
                  NULL);

	nemo_window_set_active_pane (window, active_pane);
	nemo_navigation_state_set_master (window->details->nav_state,
					      active_pane->action_group);

	nemo_window_update_show_hide_ui_elements (window);
    nemo_window_refresh_sidebar_colours (window);
}

gboolean
nemo_window_split_view_showing (NemoWindow *window)
{
	return g_list_length (NEMO_WINDOW (window)->details->panes) > 1;
}

void
nemo_window_clear_secondary_pane_location (NemoWindow *window)
{
    g_return_if_fail (NEMO_IS_WINDOW (window));
    g_clear_object (&window->details->secondary_pane_last_location);
}

void
nemo_window_set_sidebar_id (NemoWindow *window,
                            const gchar *id)
{
    if (g_strcmp0 (id, window->details->sidebar_id) != 0) {

        g_settings_set_string (nemo_window_state,
                               NEMO_WINDOW_STATE_SIDE_PANE_VIEW,
                               id);

        g_free (window->details->sidebar_id);

        window->details->sidebar_id = g_strdup (id);

        g_object_notify_by_pspec (G_OBJECT (window), properties[PROP_SIDEBAR_VIEW_TYPE]);
    }
}

const gchar *
nemo_window_get_sidebar_id (NemoWindow *window)
{
    return window->details->sidebar_id;
}

void
nemo_window_set_show_sidebar (NemoWindow *window,
                              gboolean show)
{
    if (!NEMO_IS_DESKTOP_WINDOW (window)) {
        window->details->show_sidebar = show;

        g_settings_set_boolean (nemo_window_state, NEMO_WINDOW_STATE_START_WITH_SIDEBAR, show);

        g_object_notify_by_pspec (G_OBJECT (window), properties[PROP_SHOW_SIDEBAR]);
    }
}

gboolean
nemo_window_get_show_sidebar (NemoWindow *window)
{
    return window->details->show_sidebar;
}

const gchar *
nemo_window_get_ignore_meta_view_id (NemoWindow *window)
{
    return window->details->ignore_meta_view_id;
}

void
nemo_window_set_ignore_meta_view_id (NemoWindow *window, const gchar *id)
{
    if (id != NULL) {
        gchar *old_id = window->details->ignore_meta_view_id;
        if (g_strcmp0 (old_id, id) != 0) {
            nemo_window_set_ignore_meta_zoom_level (window, -1);
        }
        window->details->ignore_meta_view_id = g_strdup (id);
        g_free (old_id);
    }
}

gint
nemo_window_get_ignore_meta_zoom_level (NemoWindow *window)
{
    return window->details->ignore_meta_zoom_level;
}

void
nemo_window_set_ignore_meta_zoom_level (NemoWindow *window, gint level)
{
    window->details->ignore_meta_zoom_level = level;
}

GList *
nemo_window_get_ignore_meta_visible_columns (NemoWindow *window)
{
    return g_list_copy_deep (window->details->ignore_meta_visible_columns, (GCopyFunc) g_strdup, NULL);
}

void
nemo_window_set_ignore_meta_visible_columns (NemoWindow *window, GList *list)
{
    GList *old = window->details->ignore_meta_visible_columns;
    window->details->ignore_meta_visible_columns = list != NULL ? g_list_copy_deep (list, (GCopyFunc) g_strdup, NULL) :
                                                                  NULL;
    if (old != NULL)
        g_list_free_full (old, g_free);
}

GList *
nemo_window_get_ignore_meta_column_order (NemoWindow *window)
{
    return g_list_copy_deep (window->details->ignore_meta_column_order, (GCopyFunc) g_strdup, NULL);
}

void
nemo_window_set_ignore_meta_column_order (NemoWindow *window, GList *list)
{
    GList *old = window->details->ignore_meta_column_order;
    window->details->ignore_meta_column_order = list != NULL ? g_list_copy_deep (list, (GCopyFunc) g_strdup, NULL) :
                                                               NULL;
    if (old != NULL)
        g_list_free_full (old, g_free);
}

const gchar *
nemo_window_get_ignore_meta_sort_column (NemoWindow *window)
{
    return window->details->ignore_meta_sort_column;
}

void
nemo_window_set_ignore_meta_sort_column (NemoWindow *window, const gchar *column)
{
    if (column != NULL) {
        gchar *old_column = window->details->ignore_meta_sort_column;
        window->details->ignore_meta_sort_column = g_strdup (column);
        g_free (old_column);
    }
}

gint
nemo_window_get_ignore_meta_sort_direction (NemoWindow *window)
{
    return window->details->ignore_meta_sort_direction;
}

void
nemo_window_set_ignore_meta_sort_direction (NemoWindow *window, gint direction)
{
    window->details->ignore_meta_sort_direction = direction;
}

NemoWindowOpenFlags
nemo_event_get_window_open_flags (void)
{
	NemoWindowOpenFlags flags = 0;
	GdkEvent *event;

	event = gtk_get_current_event ();

	if (event == NULL) {
		return flags;
	}

	if ((event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) &&
	    (event->button.button == 2)) {
		flags |= NEMO_WINDOW_OPEN_FLAG_NEW_TAB;
	}

	gdk_event_free (event);

	return flags;
}

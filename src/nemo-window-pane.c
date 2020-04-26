/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-window-pane.c: Nemo window pane
 *
 * Copyright (C) 2008 Free Software Foundation, Inc.
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Holger Berndt <berndth@gmx.de>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "nemo-window-pane.h"

#include "nemo-actions.h"
#include "nemo-application.h"
#include "nemo-location-bar.h"
#include "nemo-notebook.h"
#include "nemo-pathbar.h"
#include "nemo-toolbar.h"
#include "nemo-window-manage-views.h"
#include "nemo-window-private.h"
#include "nemo-window-menus.h"
#include "nemo-icon-view.h"
#include "nemo-list-view.h"

#include <glib/gi18n.h>

#include <libnemo-private/nemo-clipboard.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-entry.h>

#define DEBUG_FLAG NEMO_DEBUG_WINDOW
#include <libnemo-private/nemo-debug.h>

// For: NEMO_IS_DESKTOP_WINDOW
#include "nemo-desktop-window.h"

enum {
	PROP_WINDOW = 1,
	NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NemoWindowPane, nemo_window_pane,
	       GTK_TYPE_BOX)

static gboolean
widget_is_in_temporary_bars (GtkWidget *widget,
			     NemoWindowPane *pane)
{
	gboolean res = FALSE;

	if (gtk_widget_get_ancestor (widget, NEMO_TYPE_LOCATION_BAR) != NULL &&
	     pane->temporary_navigation_bar)
		res = TRUE;

	return res;
}

static void
unset_focus_widget (NemoWindowPane *pane)
{
	if (pane->last_focus_widget != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (pane->last_focus_widget),
					      (gpointer *) &pane->last_focus_widget);
		pane->last_focus_widget = NULL;
	}
}

static void
remember_focus_widget (NemoWindowPane *pane)
{
	GtkWidget *focus_widget;

	focus_widget = gtk_window_get_focus (GTK_WINDOW (pane->window));
	if (focus_widget != NULL &&
	    !widget_is_in_temporary_bars (focus_widget, pane)) {
		unset_focus_widget (pane);

		pane->last_focus_widget = focus_widget;
		g_object_add_weak_pointer (G_OBJECT (focus_widget),
					   (gpointer *) &(pane->last_focus_widget));
	}
}

static void
restore_focus_widget (NemoWindowPane *pane)
{
	if (pane->last_focus_widget != NULL) {
		if (NEMO_IS_VIEW (pane->last_focus_widget)) {
			nemo_view_grab_focus (NEMO_VIEW (pane->last_focus_widget));
		} else {
			gtk_widget_grab_focus (pane->last_focus_widget);
		}

		unset_focus_widget (pane);
	}
}

static NemoWindowSlot *
get_next_or_previous_slot (NemoWindowPane *pane)
{
	GtkNotebook *gnotebook;
	gint page_num;
	GtkWidget *page;

	g_return_val_if_fail (pane != NULL, NULL);

	gnotebook = GTK_NOTEBOOK (pane->notebook);
	page = NULL;

	/* If we don't have an active slot, return the first page in the
	 * notebook.
	 */
	if (!pane->active_slot) {
		page = gtk_notebook_get_nth_page (gnotebook, 0);
	} else {
		/* Otherwise, return the next or previous slot that appears in
		 * the notebook. We don't use pane->slots to select the next
		 * slot as the list does not represent the order in which slots
		 * are actually displayed in the pane.
		 */
		page_num = gtk_notebook_page_num (
			gnotebook, GTK_WIDGET (pane->active_slot));
		if (page_num != -1) {
			page = gtk_notebook_get_nth_page (
				gnotebook, page_num+1);
			/* Get the previous page if there is no next one. */
			if (!page) {
				page = gtk_notebook_get_nth_page (
					gnotebook, page_num-1);
			}
		}
	}

	if (page)
		return NEMO_WINDOW_SLOT (page);
	return NULL;
}

static int
bookmark_list_get_uri_index (GList *list, GFile *location)
{
	NemoBookmark *bookmark;
	GList *l;
	GFile *tmp;
	int i;

	g_return_val_if_fail (location != NULL, -1);

	for (i = 0, l = list; l != NULL; i++, l = l->next) {
		bookmark = NEMO_BOOKMARK (l->data);

		tmp = nemo_bookmark_get_location (bookmark);
		if (g_file_equal (location, tmp)) {
			g_object_unref (tmp);
			return i;
		}
		g_object_unref (tmp);
	}

	return -1;
}

static void
nemo_window_pane_hide_temporary_bars (NemoWindowPane *pane)
{
	NemoWindowSlot *slot;
	NemoDirectory *directory;

	slot = pane->active_slot;

	if (pane->temporary_navigation_bar) {
		directory = nemo_directory_get (slot->location);

		pane->temporary_navigation_bar = FALSE;

		/* if we're in a search directory, hide the main bar, and show the search
		 * bar again; otherwise, just hide the whole toolbar.
		 */
		if (NEMO_IS_SEARCH_DIRECTORY (directory)) {
			nemo_toolbar_set_show_main_bar (NEMO_TOOLBAR (pane->tool_bar), FALSE);
		} else {
			gtk_widget_hide (pane->tool_bar);
		}

		nemo_directory_unref (directory);
	}
}

static void
location_entry_changed_cb (NemoToolbar *toolbar, gboolean val, gpointer data)
{
    NemoWindowPane *pane = NEMO_WINDOW_PANE (data);
    nemo_window_pane_ensure_location_bar (pane);
}

static void
navigation_bar_cancel_callback (GtkWidget *widget,
				NemoWindowPane *pane)
{
	GtkAction *location;

	location = gtk_action_group_get_action (pane->action_group,
					      NEMO_ACTION_TOGGLE_LOCATION);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (location), FALSE);

	nemo_window_pane_hide_temporary_bars (pane);
	restore_focus_widget (pane);
}

static void
navigation_bar_location_changed_callback (GtkWidget *widget,
                                          GFile *location,
                                          NemoWindowPane *pane)
{
    nemo_window_pane_hide_temporary_bars (pane);

    restore_focus_widget (pane);

    nemo_window_slot_open_location (pane->active_slot, location, 0);
}

static gboolean
toolbar_focus_in_callback (GtkWidget *widget,
			   GdkEventFocus *event,
			   gpointer user_data)
{
	NemoWindowPane *pane = user_data;
	nemo_window_set_active_pane (pane->window, pane);

	return FALSE;
}

static void
path_bar_location_changed_callback (GtkWidget *widget,
				    GFile *location,
				    NemoWindowPane *pane)
{
	NemoWindowSlot *slot;
	int i;

	slot = pane->active_slot;
	nemo_window_set_active_pane (pane->window, pane);

	/* check whether we already visited the target location */
	i = bookmark_list_get_uri_index (slot->back_list, location);
	if (i >= 0) {
		nemo_window_back_or_forward (pane->window, TRUE, i, 0);
	} else {
		nemo_window_slot_open_location (pane->active_slot, location, 0);
	}
}

static gboolean
path_bar_button_pressed_callback (GtkWidget *widget,
				  GdkEventButton *event,
				  NemoWindowPane *pane)
{
	NemoWindowSlot *slot;
	NemoView *view;
	GFile *location;
	char *uri;

	g_object_set_data (G_OBJECT (widget), "handle-button-release",
			   GINT_TO_POINTER (TRUE));

	if (event->button == 3) {
		slot = nemo_window_get_active_slot (pane->window);
		view = slot->content_view;
		if (view != NULL) {
			location = nemo_path_bar_get_path_for_button (
				NEMO_PATH_BAR (pane->path_bar), widget);
			if (location != NULL) {
				uri = g_file_get_uri (location);
				nemo_view_pop_up_location_context_menu (
					view, event, uri);
				g_object_unref (location);
				g_free (uri);
				return TRUE;
			}
		}
	}

    if (event->button == 2)
        return TRUE;

    return FALSE;
}

static gboolean
path_bar_button_released_callback (GtkWidget *widget,
				   GdkEventButton *event,
				   NemoWindowPane *pane)
{
	NemoWindowSlot *slot;
	NemoWindowOpenFlags flags;
	GFile *location;
	int mask;
	gboolean handle_button_release;

	mask = event->state & gtk_accelerator_get_default_mod_mask ();
	flags = 0;

	handle_button_release = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget),
						  "handle-button-release"));

	if (event->type == GDK_BUTTON_RELEASE && handle_button_release) {
		location = nemo_path_bar_get_path_for_button (NEMO_PATH_BAR (pane->path_bar), widget);

		if (event->button == 2 && mask == 0) {
			flags = NEMO_WINDOW_OPEN_FLAG_NEW_TAB;
		} else if (event->button == 1 && mask == GDK_CONTROL_MASK) {
			flags = NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW;
		}

		if (flags != 0) {
			slot = nemo_window_get_active_slot (pane->window);
			nemo_window_slot_open_location (slot, location, flags);
			g_object_unref (location);
			return TRUE;
		}

		g_object_unref (location);
	}

	return FALSE;
}

static void
path_bar_button_drag_begin_callback (GtkWidget *widget,
				     GdkEventButton *event,
				     gpointer user_data)
{
	g_object_set_data (G_OBJECT (widget), "handle-button-release",
			   GINT_TO_POINTER (FALSE));
}

static void
notebook_popup_menu_new_tab_cb (GtkMenuItem *menuitem,
				gpointer user_data)
{
	NemoWindowPane *pane;

	pane = user_data;
	nemo_window_new_tab (pane->window);
}

static void
path_bar_path_set_callback (GtkWidget *widget,
			    GFile *location,
			    NemoWindowPane *pane)
{
	GList *children, *l;
	GtkWidget *child;

	children = gtk_container_get_children (GTK_CONTAINER (widget));

	for (l = children; l != NULL; l = l->next) {
		child = GTK_WIDGET (l->data);

		if (!GTK_IS_TOGGLE_BUTTON (child)) {
			continue;
		}

		if (!g_signal_handler_find (child,
					    G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
					    0, 0, NULL,
					    path_bar_button_pressed_callback,
					    pane)) {
			g_signal_connect (child, "button-press-event",
					  G_CALLBACK (path_bar_button_pressed_callback),
					  pane);
			g_signal_connect (child, "button-release-event",
					  G_CALLBACK (path_bar_button_released_callback),
					  pane);
			g_signal_connect (child, "drag-begin",
					  G_CALLBACK (path_bar_button_drag_begin_callback),
					  pane);
		}
	}

	g_list_free (children);
}

static void
reorder_tab (NemoWindowPane *pane, int offset)
{
	int num_target_tab;

	g_return_if_fail (pane != NULL);

	num_target_tab = GPOINTER_TO_INT (
		g_object_get_data (G_OBJECT (pane), "num_target_tab"));
	nemo_notebook_reorder_child_relative (
		NEMO_NOTEBOOK (pane->notebook), num_target_tab, offset);
}

static void
notebook_popup_menu_move_left_cb (GtkMenuItem *menuitem,
				  gpointer user_data)
{
	reorder_tab (NEMO_WINDOW_PANE (user_data), -1);
}

static void
notebook_popup_menu_move_right_cb (GtkMenuItem *menuitem,
				   gpointer user_data)
{
	reorder_tab (NEMO_WINDOW_PANE (user_data), 1);
}

/* emitted when the user clicks the "close" button of tabs */
static void
notebook_tab_close_requested (NemoNotebook *notebook,
			      NemoWindowSlot *slot,
			      NemoWindowPane *pane)
{
	nemo_window_pane_close_slot (pane, slot);
}

static void
notebook_popup_menu_close_cb (GtkMenuItem *menuitem,
			      gpointer user_data)
{
	NemoWindowPane *pane;
	NemoNotebook *notebook;
	int num_target_tab;
	GtkWidget *page;

	pane = NEMO_WINDOW_PANE (user_data);
	g_return_if_fail (pane != NULL);
	notebook = NEMO_NOTEBOOK (pane->notebook);
	num_target_tab = GPOINTER_TO_INT (
		g_object_get_data (G_OBJECT (pane), "num_target_tab"));
	page = gtk_notebook_get_nth_page (
		GTK_NOTEBOOK (notebook), num_target_tab);
	notebook_tab_close_requested (
		notebook, NEMO_WINDOW_SLOT (page), pane);
}

static void
notebook_popup_menu_show (NemoWindowPane *pane,
			  GdkEventButton *event,
			  int 	 	  num_target_tab)
{
	GtkWidget *popup;
	GtkWidget *item;
	GtkWidget *image;
	int button, event_time;
	gboolean can_move_left, can_move_right;
	NemoNotebook *notebook;

	notebook = NEMO_NOTEBOOK (pane->notebook);

	can_move_left = nemo_notebook_can_reorder_child_relative (
		notebook, num_target_tab, -1);
	can_move_right = nemo_notebook_can_reorder_child_relative (
		notebook, num_target_tab, 1);

	popup = gtk_menu_new();

	item = gtk_menu_item_new_with_mnemonic (_("_New Tab"));
	g_signal_connect (item, "activate",
			  G_CALLBACK (notebook_popup_menu_new_tab_cb),
			  pane);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup),
			       item);

	gtk_menu_shell_append (GTK_MENU_SHELL (popup),
			       gtk_separator_menu_item_new ());

	/* Store the target tab index in the pane object so we don't have to
	 * wrap user data in a custom struct which may leak if none of the 
	 * callbacks which would free it again is invoked. 
	 */ 
	g_object_set_data (G_OBJECT (pane), "num_target_tab", 
			   GINT_TO_POINTER (num_target_tab));

	item = gtk_menu_item_new_with_mnemonic (_("Move Tab _Left"));
	g_signal_connect (item, "activate",
			  G_CALLBACK (notebook_popup_menu_move_left_cb),
			  pane);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup),
			       item);
	gtk_widget_set_sensitive (item, can_move_left);

	item = gtk_menu_item_new_with_mnemonic (_("Move Tab _Right"));
	g_signal_connect (item, "activate",
			  G_CALLBACK (notebook_popup_menu_move_right_cb),
			  pane);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup),
			       item);
	gtk_widget_set_sensitive (item, can_move_right);

	gtk_menu_shell_append (GTK_MENU_SHELL (popup),
			       gtk_separator_menu_item_new ());

	item = gtk_image_menu_item_new_with_mnemonic (_("_Close Tab"));
	image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
	g_signal_connect (item, "activate",
			  G_CALLBACK (notebook_popup_menu_close_cb), pane);
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
				   pane->notebook,
				   NULL);

	gtk_menu_popup (GTK_MENU (popup), NULL, NULL, NULL, NULL,
			button, event_time);
}

static gboolean
notebook_button_press_cb (GtkWidget *widget,
                          GdkEventButton *event,
                          gpointer user_data)
{
	NemoWindowPane *pane;
	NemoNotebook *notebook;
	int tab_clicked;

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	/* Not a button event we actually care about, so just bail */
	if (event->button != 1 && event->button != 2 && event->button != 3)
		return FALSE;

	pane = NEMO_WINDOW_PANE (user_data);
	notebook = NEMO_NOTEBOOK (pane->notebook);
	tab_clicked = nemo_notebook_find_tab_num_at_pos (
		notebook, event->x_root, event->y_root);

	/* Do not change the current page on middle-click events. Close the
	 * clicked tab instead.
	 */
	if (event->button == 2) {
		if (tab_clicked != -1) {
			GtkWidget *page = gtk_notebook_get_nth_page (
				GTK_NOTEBOOK (notebook), tab_clicked);
			notebook_tab_close_requested (
				notebook, NEMO_WINDOW_SLOT (page), pane);
		}
	} else {
		if (event->button == 3) {
			notebook_popup_menu_show (pane, event, tab_clicked);
		} else {
			gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook),
						       tab_clicked);
		}
	}

	return FALSE;
}

static gboolean
notebook_popup_menu_cb (GtkWidget *widget,
			gpointer user_data)
{
	NemoWindowPane *pane;
	int page_num;

	pane = NEMO_WINDOW_PANE (user_data);
	page_num = gtk_notebook_get_current_page (
		GTK_NOTEBOOK (pane->notebook));
	if (page_num == -1)
	       return FALSE;
	notebook_popup_menu_show (pane, NULL, page_num);
	return TRUE;
}

static gboolean
notebook_switch_page_cb (GtkNotebook *notebook,
			 GtkWidget *page,
			 unsigned int page_num,
			 NemoWindowPane *pane)
{
	NemoWindowSlot *slot;
	GtkWidget *widget;

	widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (pane->notebook), page_num);
	g_assert (widget != NULL);

	/* find slot corresponding to the target page */
	slot = NEMO_WINDOW_SLOT (widget);
	g_assert (slot != NULL);

	nemo_window_set_active_slot (nemo_window_slot_get_window (slot),
					 slot);

	return FALSE;
}

static void
notebook_page_removed_cb (GtkNotebook *notebook,
			  GtkWidget *page,
			  guint page_num,
			  gpointer user_data)
{
	NemoWindowPane *pane = user_data;
	NemoWindowSlot *slot = NEMO_WINDOW_SLOT (page), *next_slot;
	gboolean dnd_slot;

	dnd_slot = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (slot), "dnd-window-slot"));
	if (!dnd_slot) {
		return;
	}

	if (pane->active_slot == slot) {
		next_slot = get_next_or_previous_slot (pane);
		nemo_window_set_active_slot (pane->window, next_slot);
	}

	nemo_window_manage_views_close_slot (slot);
	pane->slots = g_list_remove (pane->slots, slot);
}

static void
notebook_page_added_cb (GtkNotebook *notebook,
			GtkWidget *page,
			guint page_num,
			gpointer user_data)
{
	NemoWindowPane *pane;
	NemoWindowSlot *slot;
	NemoWindowSlot *dummy_slot;
	gboolean dnd_slot;

	pane = NEMO_WINDOW_PANE (user_data);
	slot = NEMO_WINDOW_SLOT (page);

	//Slot has been dropped onto another pane (new window or tab bar of other window)
	//So reassociate the pane if needed.
	if (slot->pane != pane) {
		slot->pane->slots = g_list_remove (slot->pane->slots, slot);
		slot->pane = pane;
		pane->slots = g_list_append (pane->slots, slot);
		g_signal_emit_by_name (slot, "changed-pane");
		nemo_window_set_active_slot (nemo_window_slot_get_window (slot), slot);
	}

	dnd_slot = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (slot), "dnd-window-slot"));

	if (!dnd_slot) {
		//Slot does not come from dnd window creation.
		return;
	}

	g_object_set_data (G_OBJECT (page), "dnd-window-slot",
		   GINT_TO_POINTER (FALSE));

	dummy_slot = g_list_nth_data (pane->slots, 0);
	if (dummy_slot != NULL) {
		nemo_window_pane_remove_slot_unsafe (
			dummy_slot->pane, dummy_slot);
	}

	gtk_widget_show (GTK_WIDGET (pane));
	gtk_widget_show (GTK_WIDGET (pane->window));
}

static GtkNotebook *
notebook_create_window_cb (GtkNotebook *notebook,
			   GtkWidget *page,
			   gint x,
			   gint y,
			   gpointer user_data)
{
	NemoApplication *app;
	NemoWindow *new_window;
	NemoWindowPane *new_pane;
	NemoWindowSlot *slot;

	if (!NEMO_IS_WINDOW_SLOT (page)) {
		return NULL;
	}

	app = NEMO_APPLICATION (g_application_get_default ());
	new_window = nemo_application_create_window
		(app, gtk_widget_get_screen (GTK_WIDGET (notebook)));

	slot = NEMO_WINDOW_SLOT (page);
	g_object_set_data (G_OBJECT (slot), "dnd-window-slot",
			   GINT_TO_POINTER (TRUE));

	gtk_window_set_position (GTK_WINDOW (new_window), GTK_WIN_POS_MOUSE);

	new_pane = nemo_window_get_active_pane (new_window);
	return GTK_NOTEBOOK (new_pane->notebook);
}

static void
action_show_hide_search_callback (GtkAction *action,
				  gpointer user_data)
{
	NemoWindowPane *pane = user_data;
	NemoWindow *window = pane->window;
	NemoWindowSlot *slot;

	slot = pane->active_slot;

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
	    remember_focus_widget (pane);
	    nemo_window_slot_set_query_editor_visible (slot, TRUE);
	} else {
		/* Do nothing if the query editor is not visible to begin with,
		   i.e. if toggle action was due to switching from a search tab */
		if (nemo_query_editor_get_active (NEMO_QUERY_EDITOR (slot->query_editor))) {
			GFile *location = NULL;

			restore_focus_widget (pane);

			if (slot->query_editor != NULL) {
                /* If closing the search bar, restore the original location */
                location = g_file_new_for_uri (nemo_query_editor_get_base_uri (slot->query_editor));

				/* Last try: use the home directory as the return location */
				if (location == NULL) {
					location = g_file_new_for_path (g_get_home_dir ());
				}	

				nemo_window_go_to (window, location);
				g_object_unref (location);
			}

			nemo_window_slot_set_query_editor_visible (slot, FALSE);
		}
	}
}

static void
setup_search_action (NemoWindowPane *pane)
{
	GtkActionGroup *group = pane->action_group;
	GtkAction *action;

	action = gtk_action_group_get_action (group, NEMO_ACTION_SEARCH);
	g_signal_connect (action, "activate",
			  G_CALLBACK (action_show_hide_search_callback), pane);
}

static void
toolbar_action_group_activated_callback (GtkActionGroup *action_group,
					 GtkAction *action,
					 gpointer user_data)
{
	NemoWindowPane *pane = user_data;
	nemo_window_set_active_pane (pane->window, pane);
}

static void
nemo_window_pane_set_property (GObject *object,
				   guint arg_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	NemoWindowPane *self = NEMO_WINDOW_PANE (object);

	switch (arg_id) {
	case PROP_WINDOW:
		self->window = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
		break;
	}
}

static void
nemo_window_pane_get_property (GObject *object,
				   guint arg_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	NemoWindowPane *self = NEMO_WINDOW_PANE (object);

	switch (arg_id) {
	case PROP_WINDOW:
		g_value_set_object (value, self->window);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
		break;
	}
}

static void
nemo_window_pane_dispose (GObject *object)
{
	NemoWindowPane *pane = NEMO_WINDOW_PANE (object);

	unset_focus_widget (pane);

	pane->window = NULL;
	g_clear_object (&pane->action_group);

	g_assert (pane->slots == NULL);

	G_OBJECT_CLASS (nemo_window_pane_parent_class)->dispose (object);
}

static void
nemo_window_pane_constructed (GObject *obj)
{
	NemoWindowPane *pane = NEMO_WINDOW_PANE (obj);
	GtkSizeGroup *header_size_group;
	NemoWindow *window;
	GtkActionGroup *action_group;

	G_OBJECT_CLASS (nemo_window_pane_parent_class)->constructed (obj);

	window = pane->window;

	header_size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
	gtk_size_group_set_ignore_hidden (header_size_group, FALSE);

	/* build the toolbar */
	action_group = nemo_window_create_toolbar_action_group (window);
	pane->toolbar_action_group = action_group;
	pane->tool_bar = GTK_WIDGET (nemo_toolbar_new (action_group));

    g_signal_connect_object (pane->tool_bar, "notify::show-location-entry",
                             G_CALLBACK (location_entry_changed_cb),
                             pane, 0);

	pane->action_group = action_group;

    if (!NEMO_IS_DESKTOP_WINDOW (window))
        setup_search_action (pane);

	g_signal_connect (pane->action_group, "pre-activate",
			  G_CALLBACK (toolbar_action_group_activated_callback), pane);

	/* Pack to windows hbox (under the menu */
	gtk_box_pack_start (GTK_BOX (window->details->toolbar_holder),
			    pane->tool_bar,
			    TRUE, TRUE, 0);

	/* start as non-active */
	nemo_window_pane_set_active (pane, FALSE);

	g_settings_bind_with_mapping (nemo_window_state,
				      NEMO_WINDOW_STATE_START_WITH_TOOLBAR,
				      pane->tool_bar,
				      "visible",
				      G_SETTINGS_BIND_GET,
				      nemo_window_disable_chrome_mapping, NULL,
				      window, NULL);

	/* connect to the pathbar signals */
	pane->path_bar = nemo_toolbar_get_path_bar (NEMO_TOOLBAR (pane->tool_bar));
	gtk_size_group_add_widget (header_size_group, pane->path_bar);

	g_signal_connect_object (pane->path_bar, "path-clicked",
				 G_CALLBACK (path_bar_location_changed_callback), pane, 0);
	g_signal_connect_object (pane->path_bar, "path-set",
				 G_CALLBACK (path_bar_path_set_callback), pane, 0);

	/* connect to the location bar signals */
	pane->location_bar = nemo_toolbar_get_location_bar (NEMO_TOOLBAR (pane->tool_bar));
	gtk_size_group_add_widget (header_size_group, pane->location_bar);

	nemo_clipboard_set_up_editable
		(GTK_EDITABLE (nemo_location_bar_get_entry (NEMO_LOCATION_BAR (pane->location_bar))),
		 nemo_window_get_ui_manager (NEMO_WINDOW (window)),
		 TRUE);

	g_signal_connect_object (pane->location_bar, "location-changed",
				 G_CALLBACK (navigation_bar_location_changed_callback), pane, 0);
	g_signal_connect_object (pane->location_bar, "cancel",
				 G_CALLBACK (navigation_bar_cancel_callback), pane, 0);
	g_signal_connect_object (nemo_location_bar_get_entry (NEMO_LOCATION_BAR (pane->location_bar)), "focus-in-event",
				 G_CALLBACK (toolbar_focus_in_callback), pane, 0);

	/* initialize the notebook */
	pane->notebook = g_object_new (NEMO_TYPE_NOTEBOOK, NULL);
	gtk_box_pack_start (GTK_BOX (pane), pane->notebook,
			    TRUE, TRUE, 0);
	g_signal_connect (pane->notebook,
			  "tab-close-request",
			  G_CALLBACK (notebook_tab_close_requested),
			  pane);
	g_signal_connect_after (pane->notebook,
				"button_press_event",
				G_CALLBACK (notebook_button_press_cb),
				pane);
	g_signal_connect (pane->notebook, "popup-menu",
			  G_CALLBACK (notebook_popup_menu_cb),
			  pane);
	g_signal_connect (pane->notebook,
			  "switch-page",
			  G_CALLBACK (notebook_switch_page_cb),
			  pane);
	g_signal_connect (pane->notebook, "create-window",
			  G_CALLBACK (notebook_create_window_cb),
			  pane);
	g_signal_connect (pane->notebook, "page-added",
			  G_CALLBACK (notebook_page_added_cb),
			  pane);
	g_signal_connect (pane->notebook, "page-removed",
			  G_CALLBACK (notebook_page_removed_cb),
			  pane);

	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (pane->notebook), 
			  g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_TAB_AREA ));
	gtk_notebook_set_show_border (GTK_NOTEBOOK (pane->notebook), FALSE);
	gtk_notebook_set_group_name (GTK_NOTEBOOK (pane->notebook), "nemo-slots");
	gtk_widget_show (pane->notebook);
	gtk_container_set_border_width (GTK_CONTAINER (pane->notebook), 0);

	/* Ensure that the view has some minimal size and that other parts
	 * of the UI (like location bar and tabs) don't request more and
	 * thus affect the default position of the split view paned.
	 */
	gtk_widget_set_size_request (GTK_WIDGET (pane), 60, 60);

	/*
	 * If we're on the desktop we need to make sure the toolbar can never show
	 */
	if (NEMO_IS_DESKTOP_WINDOW(window)) {
		gtk_widget_hide (GTK_WIDGET (window->details->toolbar_holder));
	}

	/* we can unref the size group now */
	g_object_unref (header_size_group);
}

static void
nemo_window_pane_class_init (NemoWindowPaneClass *klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	oclass->constructed = nemo_window_pane_constructed;
	oclass->dispose = nemo_window_pane_dispose;
	oclass->set_property = nemo_window_pane_set_property;
	oclass->get_property = nemo_window_pane_get_property;

	properties[PROP_WINDOW] =
		g_param_spec_object ("window",
				     "The NemoWindow",
				     "The parent NemoWindow",
				     NEMO_TYPE_WINDOW,
				     G_PARAM_READWRITE |
				     G_PARAM_CONSTRUCT_ONLY |
				     G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

static void
nemo_window_pane_init (NemoWindowPane *pane)
{
	pane->slots = NULL;
	pane->active_slot = NULL;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (pane), GTK_ORIENTATION_VERTICAL);

	GtkStyleContext *context;

	context = gtk_widget_get_style_context (GTK_WIDGET (pane));
	gtk_style_context_add_class (context, "nemo-window-pane");
}

NemoWindowPane *
nemo_window_pane_new (NemoWindow *window)
{
	return g_object_new (NEMO_TYPE_WINDOW_PANE,
			     "window", window,
			     NULL);
}

static void
nemo_window_pane_set_active_style (NemoWindowPane *pane,
				       gboolean is_active)
{
	GtkStyleContext *style;
	gboolean has_inactive;

	style = gtk_widget_get_style_context (GTK_WIDGET (pane));
	has_inactive = gtk_style_context_has_class (style, "nemo-inactive-pane");

	if (has_inactive == !is_active) {
		return;
	}

	if (is_active) {
		gtk_style_context_remove_class (style, "nemo-inactive-pane");
	} else {
		gtk_style_context_add_class (style, "nemo-inactive-pane");
	}

	gtk_widget_reset_style (GTK_WIDGET (pane));
}

void
nemo_window_pane_set_active (NemoWindowPane *pane,
				 gboolean is_active)
{
	NemoNavigationState *nav_state;

	if (is_active) {
		nav_state = nemo_window_get_navigation_state (pane->window);
		nemo_navigation_state_set_master (nav_state, pane->action_group);
	}
	/* pane inactive style */
	nemo_window_pane_set_active_style (pane, is_active);
}

GtkActionGroup *
nemo_window_pane_get_toolbar_action_group (NemoWindowPane *pane)
{
	g_return_val_if_fail (NEMO_IS_WINDOW_PANE (pane), NULL);

	return pane->toolbar_action_group;
}

void
nemo_window_pane_sync_location_widgets (NemoWindowPane *pane)
{
	NemoWindowSlot *slot, *active_slot;
	NemoNavigationState *nav_state;
	slot = pane->active_slot;

	nemo_window_pane_hide_temporary_bars (pane);

	/* Change the location bar and path bar to match the current location. */
	if (slot->location != NULL) {
		char *uri;

		/* this may be NULL if we just created the slot */
		uri = nemo_window_slot_get_location_uri (slot);
		nemo_location_bar_set_location (NEMO_LOCATION_BAR (pane->location_bar), uri);
		g_free (uri);
		nemo_path_bar_set_path (NEMO_PATH_BAR (pane->path_bar), slot->location);
        restore_focus_widget (pane);
	}

	/* Update window global UI if this is the active pane */
	if (pane == nemo_window_get_active_pane (pane->window)) {
		nemo_window_sync_up_button (pane->window);

		/* Check if the back and forward buttons need enabling or disabling. */
		active_slot = nemo_window_get_active_slot (pane->window);
		nav_state = nemo_window_get_navigation_state (pane->window);

		nemo_navigation_state_set_boolean (nav_state,
						       NEMO_ACTION_BACK,
						       active_slot->back_list != NULL);
		nemo_navigation_state_set_boolean (nav_state,
						       NEMO_ACTION_FORWARD,
						       active_slot->forward_list != NULL);

	}
}

static void
toggle_toolbar_search_button (NemoWindowPane *pane,
                                  gboolean        active)
{
	GtkActionGroup *group;
	GtkAction *action;

	group = pane->action_group;
	action = gtk_action_group_get_action (group, NEMO_ACTION_SEARCH);

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);
}

void
nemo_window_pane_sync_search_widgets (NemoWindowPane *pane)
{
	NemoWindowSlot *slot;
	NemoDirectory *directory;
	NemoSearchDirectory *search_directory;

	slot = pane->active_slot;
	search_directory = NULL;

	directory = nemo_directory_get (slot->location);
	if (NEMO_IS_SEARCH_DIRECTORY (directory)) {
		search_directory = NEMO_SEARCH_DIRECTORY (directory);
	}

	if (search_directory != NULL) {
		toggle_toolbar_search_button (pane, TRUE);
	} else  {
		/* If we're not in a search directory, make sure the query editor visibility matches the
		   search button due to a quirk when switching tabs. TODO: Another approach would be to
		   leave the editor visible and toggle the search button true. Which is better? */
		if (nemo_query_editor_get_active (NEMO_QUERY_EDITOR (slot->query_editor))) {
			nemo_window_slot_set_query_editor_visible (slot, FALSE);
		}
	    	toggle_toolbar_search_button (pane, FALSE);
	}

	nemo_directory_unref (directory);
}

void
nemo_window_pane_close_slot (NemoWindowPane *pane,
				 NemoWindowSlot *slot)
{
	NemoWindow *window;

	DEBUG ("Requesting to remove slot %p from pane %p", slot, pane);

	window = pane->window;
	if (!window)
		return;

	if (pane->active_slot == slot) {
		NemoWindowSlot *next_slot;
		next_slot = get_next_or_previous_slot (NEMO_WINDOW_PANE (pane));
		nemo_window_set_active_slot (window, next_slot);
	}

	nemo_window_pane_remove_slot_unsafe (pane, slot);

	/* If that was the last slot in the pane, close the pane or even the
	 * whole window.
	 */
	if (pane->slots == NULL) {
		if (nemo_window_split_view_showing (window)) {
			NemoWindowPane *new_pane;

			DEBUG ("Last slot removed from the pane %p, closing it", pane);
			nemo_window_close_pane (window, pane);

			new_pane = g_list_nth_data (window->details->panes, 0);

			if (new_pane->active_slot == NULL) {
				new_pane->active_slot = get_next_or_previous_slot (new_pane);
			}

			DEBUG ("Calling set_active_pane, new slot %p", new_pane->active_slot);
			nemo_window_set_active_pane (window, new_pane);
			nemo_window_update_show_hide_menu_items (window);
		} else {
			DEBUG ("Last slot removed from the last pane, close the window");
			nemo_window_close (window);
		}
	}
}

void
nemo_window_pane_grab_focus (NemoWindowPane *pane)
{
	if (NEMO_IS_WINDOW_PANE (pane) && pane->active_slot) {
		nemo_view_grab_focus (pane->active_slot->content_view);
	}
}

void
nemo_window_pane_ensure_location_bar (NemoWindowPane *pane)
{
    gboolean show_location, use_temp_toolbars;

    use_temp_toolbars = !g_settings_get_boolean (nemo_window_state,
                     NEMO_WINDOW_STATE_START_WITH_TOOLBAR);
    show_location = nemo_toolbar_get_show_location_entry (NEMO_TOOLBAR (pane->tool_bar));

    if (use_temp_toolbars) {
        if (!pane->temporary_navigation_bar) {
            gtk_widget_show (pane->tool_bar);
            pane->temporary_navigation_bar = TRUE;
        }
    }
    if (show_location) {
        remember_focus_widget (pane);
        nemo_location_bar_activate (NEMO_LOCATION_BAR (pane->location_bar));
    } else {
        restore_focus_widget (pane);
    }
}

void
nemo_window_pane_remove_slot_unsafe (NemoWindowPane *pane,
				     NemoWindowSlot *slot)
{
	int page_num;
	GtkNotebook *notebook;

	g_assert (NEMO_IS_WINDOW_SLOT (slot));
	g_assert (NEMO_IS_WINDOW_PANE (slot->pane));

	DEBUG ("Removing slot %p", slot);

	/* save pane because slot is not valid anymore after this call */
	pane = slot->pane;
	notebook = GTK_NOTEBOOK (pane->notebook);

	nemo_window_manage_views_close_slot (slot);

	page_num = gtk_notebook_page_num (notebook, GTK_WIDGET (slot));
	g_assert (page_num >= 0);

	g_signal_handlers_block_by_func (notebook,
					 G_CALLBACK (notebook_switch_page_cb),
					 pane);
	/* this will call gtk_widget_destroy on the slot */
	gtk_notebook_remove_page (notebook, page_num);
	g_signal_handlers_unblock_by_func (notebook,
					   G_CALLBACK (notebook_switch_page_cb),
					   pane);

	gtk_notebook_set_show_tabs (notebook,
				    gtk_notebook_get_n_pages (notebook) > 1 || 
					g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_TAB_AREA ));
	pane->slots = g_list_remove (pane->slots, slot);
}

NemoWindowSlot *
nemo_window_pane_open_slot (NemoWindowPane *pane,
				NemoWindowOpenSlotFlags flags)
{
	NemoWindowSlot *slot;

	g_assert (NEMO_IS_WINDOW_PANE (pane));
	g_assert (NEMO_IS_WINDOW (pane->window));

	slot = nemo_window_slot_new (pane);

	g_signal_handlers_block_by_func (pane->notebook,
					 G_CALLBACK (notebook_switch_page_cb),
					 pane);
	nemo_notebook_add_tab (NEMO_NOTEBOOK (pane->notebook),
				   slot,
				   (flags & NEMO_WINDOW_OPEN_SLOT_APPEND) != 0 ?
				   -1 :
				   gtk_notebook_get_current_page (GTK_NOTEBOOK (pane->notebook)) + 1,
				   FALSE);
	g_signal_handlers_unblock_by_func (pane->notebook,
					   G_CALLBACK (notebook_switch_page_cb),
					   pane);

	pane->slots = g_list_append (pane->slots, slot);

	return slot;
}

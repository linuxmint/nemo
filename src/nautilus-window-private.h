/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           Darin Adler <darin@bentspoon.com>
 *
 */

#ifndef NAUTILUS_WINDOW_PRIVATE_H
#define NAUTILUS_WINDOW_PRIVATE_H

#include "nautilus-window.h"
#include "nautilus-window-slot.h"
#include "nautilus-window-pane.h"
#include "nautilus-navigation-state.h"
#include "nautilus-bookmark-list.h"

#include <libnautilus-private/nautilus-directory.h>

/* FIXME bugzilla.gnome.org 42575: Migrate more fields into here. */
struct NautilusWindowDetails
{
        GtkWidget *statusbar;
        GtkWidget *menubar;
        
        GtkUIManager *ui_manager;
        GtkActionGroup *main_action_group; /* owned by ui_manager */
        guint help_message_cid;

        /* Menus. */
        guint extensions_menu_merge_id;
        GtkActionGroup *extensions_menu_action_group;

        GtkActionGroup *bookmarks_action_group;
        guint bookmarks_merge_id;
        NautilusBookmarkList *bookmark_list;

	NautilusWindowShowHiddenFilesMode show_hidden_files_mode;

	/* View As menu */
	GList *short_list_viewers;
	char *extra_viewer;

	/* View As choices */
	GtkActionGroup *view_as_action_group; /* owned by ui_manager */
	GtkRadioAction *view_as_radio_action;
	GtkRadioAction *extra_viewer_radio_action;
	guint short_list_merge_id;
	guint extra_viewer_merge_id;

	/* Ensures that we do not react on signals of a
	 * view that is re-used as new view when its loading
	 * is cancelled
	 */
	gboolean temporarily_ignore_view_signals;

        /* available panes, and active pane.
         * Both of them may never be NULL.
         */
        GList *panes;
        NautilusWindowPane *active_pane;

        GtkWidget *content_paned;
        NautilusNavigationState *nav_state;
        
        /* Side Pane */
        int side_pane_width;
        GtkWidget *sidebar;
        gchar *sidebar_id;
        
        /* Toolbar */
        GtkWidget *toolbar;

        guint extensions_toolbar_merge_id;
        GtkActionGroup *extensions_toolbar_action_group;

        /* focus widget before the location bar has been shown temporarily */
        GtkWidget *last_focus_widget;
        	
        /* split view */
        GtkWidget *split_view_hpane;

        gboolean disable_chrome;

        guint sidebar_width_handler_id;
};

/* window geometry */
/* Min values are very small, and a Nautilus window at this tiny size is *almost*
 * completely unusable. However, if all the extra bits (sidebar, location bar, etc)
 * are turned off, you can see an icon or two at this size. See bug 5946.
 */

#define NAUTILUS_WINDOW_MIN_WIDTH		200
#define NAUTILUS_WINDOW_MIN_HEIGHT		200
#define NAUTILUS_WINDOW_DEFAULT_WIDTH		800
#define NAUTILUS_WINDOW_DEFAULT_HEIGHT		550

typedef void (*NautilusBookmarkFailedCallback) (NautilusWindow *window,
                                                NautilusBookmark *bookmark);

void               nautilus_window_load_view_as_menus                    (NautilusWindow    *window);
void               nautilus_window_load_extension_menus                  (NautilusWindow    *window);
NautilusWindowPane *nautilus_window_get_next_pane                        (NautilusWindow *window);
void               nautilus_menus_append_bookmark_to_menu                (NautilusWindow    *window, 
                                                                          NautilusBookmark  *bookmark, 
                                                                          const char        *parent_path,
                                                                          const char        *parent_id,
                                                                          guint              index_in_parent,
                                                                          GtkActionGroup    *action_group,
                                                                          guint              merge_id,
                                                                          GCallback          refresh_callback,
                                                                          NautilusBookmarkFailedCallback failed_callback);

NautilusWindowSlot *nautilus_window_get_slot_for_view                    (NautilusWindow *window,
									  NautilusView   *view);

void                 nautilus_window_set_active_slot                     (NautilusWindow    *window,
									  NautilusWindowSlot *slot);
void                 nautilus_window_set_active_pane                     (NautilusWindow *window,
                                                                          NautilusWindowPane *new_pane);
NautilusWindowPane * nautilus_window_get_active_pane                     (NautilusWindow *window);


/* sync window GUI with current slot. Used when changing slots,
 * and when updating the slot state.
 */
void nautilus_window_sync_status           (NautilusWindow *window);
void nautilus_window_sync_allow_stop       (NautilusWindow *window,
					    NautilusWindowSlot *slot);
void nautilus_window_sync_title            (NautilusWindow *window,
					    NautilusWindowSlot *slot);
void nautilus_window_sync_zoom_widgets     (NautilusWindow *window);
void nautilus_window_sync_up_button        (NautilusWindow *window);

/* window menus */
GtkActionGroup *nautilus_window_create_toolbar_action_group (NautilusWindow *window);
void               nautilus_window_initialize_actions                    (NautilusWindow    *window);
void               nautilus_window_initialize_menus                      (NautilusWindow    *window);
void               nautilus_window_finalize_menus                        (NautilusWindow    *window);

void               nautilus_window_update_show_hide_menu_items           (NautilusWindow     *window);

/* window toolbar */
void               nautilus_window_close_pane                            (NautilusWindow    *window,
                                                                          NautilusWindowPane *pane);

#endif /* NAUTILUS_WINDOW_PRIVATE_H */

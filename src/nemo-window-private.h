/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nemo
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Nemo is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nemo is distributed in the hope that it will be useful,
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

#ifndef NEMO_WINDOW_PRIVATE_H
#define NEMO_WINDOW_PRIVATE_H

#include "nemo-window.h"
#include "nemo-window-slot.h"
#include "nemo-window-pane.h"
#include "nemo-navigation-state.h"
#include "nemo-bookmark-list.h"

#include <libnemo-private/nemo-directory.h>

/* FIXME bugzilla.gnome.org 42575: Migrate more fields into here. */
struct NemoWindowDetails
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
        NemoBookmarkList *bookmark_list;

	NemoWindowShowHiddenFilesMode show_hidden_files_mode;

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
        NemoWindowPane *active_pane;

        GtkWidget *content_paned;
        NemoNavigationState *nav_state;
        
        /* Side Pane */
        int side_pane_width;
        GtkWidget *sidebar;
        gchar *sidebar_id;
        
        /* Toolbar */
        GtkWidget *toolbar;

        /* Toolbar holder */
        GtkWidget *toolbar_holder;

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
/* Min values are very small, and a Nemo window at this tiny size is *almost*
 * completely unusable. However, if all the extra bits (sidebar, location bar, etc)
 * are turned off, you can see an icon or two at this size. See bug 5946.
 */

#define NEMO_WINDOW_MIN_WIDTH		200
#define NEMO_WINDOW_MIN_HEIGHT		200
#define NEMO_WINDOW_DEFAULT_WIDTH		800
#define NEMO_WINDOW_DEFAULT_HEIGHT		550

typedef void (*NemoBookmarkFailedCallback) (NemoWindow *window,
                                                NemoBookmark *bookmark);

void               nemo_window_load_view_as_menus                    (NemoWindow    *window);
void               nemo_window_load_extension_menus                  (NemoWindow    *window);
NemoWindowPane *nemo_window_get_next_pane                        (NemoWindow *window);
void               nemo_menus_append_bookmark_to_menu                (NemoWindow    *window, 
                                                                          NemoBookmark  *bookmark, 
                                                                          const char        *parent_path,
                                                                          const char        *parent_id,
                                                                          guint              index_in_parent,
                                                                          GtkActionGroup    *action_group,
                                                                          guint              merge_id,
                                                                          GCallback          refresh_callback,
                                                                          NemoBookmarkFailedCallback failed_callback);

NemoWindowSlot *nemo_window_get_slot_for_view                    (NemoWindow *window,
									  NemoView   *view);

void                 nemo_window_set_active_slot                     (NemoWindow    *window,
									  NemoWindowSlot *slot);
void                 nemo_window_set_active_pane                     (NemoWindow *window,
                                                                          NemoWindowPane *new_pane);
NemoWindowPane * nemo_window_get_active_pane                     (NemoWindow *window);


/* sync window GUI with current slot. Used when changing slots,
 * and when updating the slot state.
 */
void nemo_window_sync_status           (NemoWindow *window);
void nemo_window_sync_allow_stop       (NemoWindow *window,
					    NemoWindowSlot *slot);
void nemo_window_sync_title            (NemoWindow *window,
					    NemoWindowSlot *slot);
void nemo_window_sync_zoom_widgets     (NemoWindow *window);
void nemo_window_sync_up_button        (NemoWindow *window);

/* window menus */
GtkActionGroup *nemo_window_create_toolbar_action_group (NemoWindow *window);
void               nemo_window_initialize_actions                    (NemoWindow    *window);
void               nemo_window_initialize_menus                      (NemoWindow    *window);
void               nemo_window_finalize_menus                        (NemoWindow    *window);

void               nemo_window_update_show_hide_menu_items           (NemoWindow     *window);

/* window toolbar */
void               nemo_window_close_pane                            (NemoWindow    *window,
                                                                          NemoWindowPane *pane);

#endif /* NEMO_WINDOW_PRIVATE_H */

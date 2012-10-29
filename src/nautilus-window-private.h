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
#include "nautilus-bookmark-list.h"

#include <libnautilus-private/nautilus-directory.h>

/* FIXME bugzilla.gnome.org 42575: Migrate more fields into here. */
struct NautilusWindowDetails
{
        GtkUIManager *ui_manager;
        GtkActionGroup *main_action_group; /* owned by ui_manager */

        /* Menus. */
        guint extensions_menu_merge_id;
        GtkActionGroup *extensions_menu_action_group;

        GtkWidget *notebook;

        /* available slots, and active slot.
         * Both of them may never be NULL.
         */
        GList *slots;
        NautilusWindowSlot *active_slot;

        GtkWidget *content_paned;
        
        /* Side Pane */
        int side_pane_width;
        GtkWidget *sidebar;

        /* Main view */
        GtkWidget *main_view;

        /* Toolbar */
        GtkWidget *toolbar;
        gboolean temporary_navigation_bar;

        /* focus widget before the location bar has been shown temporarily */
        GtkWidget *last_focus_widget;

        gboolean disable_chrome;

        guint sidebar_width_handler_id;
        guint app_menu_visibility_id;
        guint bookmarks_id;
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

void               nautilus_window_load_extension_menus                  (NautilusWindow    *window);

void                 nautilus_window_set_active_slot                     (NautilusWindow    *window,
									  NautilusWindowSlot *slot);

void                 nautilus_window_prompt_for_location                 (NautilusWindow *window,
                                                                          GFile          *location);

/* sync window GUI with current slot. Used when changing slots,
 * and when updating the slot state.
 */
void nautilus_window_sync_allow_stop       (NautilusWindow *window,
					    NautilusWindowSlot *slot);
void nautilus_window_sync_title            (NautilusWindow *window,
					    NautilusWindowSlot *slot);
void nautilus_window_sync_zoom_widgets     (NautilusWindow *window);
void nautilus_window_sync_up_button        (NautilusWindow *window);

/* window menus */
void               nautilus_window_initialize_actions                    (NautilusWindow    *window);
void               nautilus_window_initialize_menus                      (NautilusWindow    *window);
void               nautilus_window_finalize_menus                        (NautilusWindow    *window);

void               nautilus_window_update_show_hide_menu_items           (NautilusWindow     *window);

#endif /* NAUTILUS_WINDOW_PRIVATE_H */

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
/* nautilus-window.h: Interface of the main window object */

#ifndef NAUTILUS_WINDOW_H
#define NAUTILUS_WINDOW_H

#include <gtk/gtk.h>
#include <eel/eel-glib-extensions.h>
#include <libnautilus-private/nautilus-bookmark.h>
#include <libnautilus-private/nautilus-search-directory.h>

typedef struct NautilusWindow NautilusWindow;
typedef struct NautilusWindowClass NautilusWindowClass;
typedef struct NautilusWindowDetails NautilusWindowDetails;

typedef enum {
        NAUTILUS_WINDOW_OPEN_FLAG_CLOSE_BEHIND = 1 << 0,
        NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW = 1 << 1,
        NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB = 1 << 2,
        NAUTILUS_WINDOW_OPEN_FLAG_USE_DEFAULT_LOCATION = 1 << 3
} NautilusWindowOpenFlags;

typedef enum {
	NAUTILUS_WINDOW_OPEN_SLOT_NONE = 0,
	NAUTILUS_WINDOW_OPEN_SLOT_APPEND = 1
}  NautilusWindowOpenSlotFlags;

typedef gboolean (* NautilusWindowGoToCallback) (NautilusWindow *window,
                                                 GFile *location,
                                                 GError *error,
                                                 gpointer user_data);

#include "nautilus-view.h"
#include "nautilus-window-slot.h"

#define NAUTILUS_TYPE_WINDOW nautilus_window_get_type()
#define NAUTILUS_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_WINDOW, NautilusWindow))
#define NAUTILUS_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_WINDOW, NautilusWindowClass))
#define NAUTILUS_IS_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_WINDOW))
#define NAUTILUS_IS_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_WINDOW))
#define NAUTILUS_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_WINDOW, NautilusWindowClass))

#define NAUTILUS_WINDOW_SIDEBAR_PLACES "places"
#define NAUTILUS_WINDOW_SIDEBAR_TREE "tree"

struct NautilusWindowClass {
        GtkApplicationWindowClass parent_spot;

	/* Function pointers for overriding, without corresponding signals */
        void   (* sync_title) (NautilusWindow *window,
			       NautilusWindowSlot *slot);
        void   (* close) (NautilusWindow *window);
};

struct NautilusWindow {
        GtkApplicationWindow parent_object;
        
        NautilusWindowDetails *details;
};

GType            nautilus_window_get_type             (void);
NautilusWindow * nautilus_window_new                  (GdkScreen         *screen);
void             nautilus_window_close                (NautilusWindow    *window);

void             nautilus_window_connect_content_view (NautilusWindow    *window,
						       NautilusView      *view);
void             nautilus_window_disconnect_content_view (NautilusWindow    *window,
							  NautilusView      *view);

void             nautilus_window_go_to                (NautilusWindow    *window,
                                                       GFile             *location);
void             nautilus_window_go_to_full           (NautilusWindow    *window,
                                                       GFile             *location,
                                                       NautilusWindowGoToCallback callback,
                                                       gpointer           user_data);
void             nautilus_window_new_tab              (NautilusWindow    *window);

GtkUIManager *   nautilus_window_get_ui_manager       (NautilusWindow    *window);
GtkActionGroup * nautilus_window_get_main_action_group (NautilusWindow   *window);

void                 nautilus_window_view_visible          (NautilusWindow *window,
                                                            NautilusView *view);
NautilusWindowSlot * nautilus_window_get_active_slot       (NautilusWindow *window);
GList *              nautilus_window_get_slots             (NautilusWindow *window);
NautilusWindowSlot * nautilus_window_open_slot             (NautilusWindow *window,
                                                            NautilusWindowOpenSlotFlags flags);
void                 nautilus_window_slot_close            (NautilusWindow *window,
                                                            NautilusWindowSlot *slot);

GtkWidget *          nautilus_window_ensure_location_entry (NautilusWindow *window);
void                 nautilus_window_sync_location_widgets (NautilusWindow *window);
void                 nautilus_window_grab_focus            (NautilusWindow *window);

void     nautilus_window_hide_sidebar         (NautilusWindow *window);
void     nautilus_window_show_sidebar         (NautilusWindow *window);
void     nautilus_window_back_or_forward      (NautilusWindow *window,
                                               gboolean        back,
                                               guint           distance,
                                               NautilusWindowOpenFlags flags);


gboolean nautilus_window_disable_chrome_mapping (GValue *value,
                                                 GVariant *variant,
                                                 gpointer user_data);

NautilusWindowOpenFlags nautilus_event_get_window_open_flags   (void);
void     nautilus_window_show_about_dialog    (NautilusWindow *window);

#endif

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-window-slot.h: Nautilus window slot
 
   Copyright (C) 2008 Free Software Foundation, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Christian Neumair <cneumair@gnome.org>
*/

#ifndef NAUTILUS_WINDOW_SLOT_H
#define NAUTILUS_WINDOW_SLOT_H

#include "nautilus-query-editor.h"

typedef struct NautilusWindowSlot NautilusWindowSlot;
typedef struct NautilusWindowSlotClass NautilusWindowSlotClass;
typedef struct NautilusWindowSlotDetails NautilusWindowSlotDetails;

#include "nautilus-view.h"
#include "nautilus-window.h"

#define NAUTILUS_TYPE_WINDOW_SLOT	 (nautilus_window_slot_get_type())
#define NAUTILUS_WINDOW_SLOT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_WINDOW_SLOT, NautilusWindowSlotClass))
#define NAUTILUS_WINDOW_SLOT(obj)	 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_WINDOW_SLOT, NautilusWindowSlot))
#define NAUTILUS_IS_WINDOW_SLOT(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_WINDOW_SLOT))
#define NAUTILUS_IS_WINDOW_SLOT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_WINDOW_SLOT))
#define NAUTILUS_WINDOW_SLOT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_WINDOW_SLOT, NautilusWindowSlotClass))

typedef enum {
	NAUTILUS_LOCATION_CHANGE_STANDARD,
	NAUTILUS_LOCATION_CHANGE_BACK,
	NAUTILUS_LOCATION_CHANGE_FORWARD,
	NAUTILUS_LOCATION_CHANGE_RELOAD
} NautilusLocationChangeType;

struct NautilusWindowSlotClass {
	GtkBoxClass parent_class;

	/* wrapped NautilusWindowInfo signals, for overloading */
	void (* active)   (NautilusWindowSlot *slot);
	void (* inactive) (NautilusWindowSlot *slot);
};

/* Each NautilusWindowSlot corresponds to a location in the window
 * for displaying a NautilusView, i.e. a tab.
 */
struct NautilusWindowSlot {
	GtkBox parent;

	NautilusWindowSlotDetails *details;

	/* slot contains
 	 *  1) an event box containing extra_location_widgets
 	 *  2) the view box for the content view
 	 */
	GtkWidget *extra_location_widgets;

	NautilusView *content_view;
	NautilusView *new_content_view;

	/* Information about bookmarks */
	NautilusBookmark *current_location_bookmark;
	NautilusBookmark *last_location_bookmark;

	/* Current location. */
	GFile *location;
	char *title;

	NautilusFile *viewed_file;
	gboolean viewed_file_seen;
	gboolean viewed_file_in_trash;

	gboolean allow_stop;

	NautilusQueryEditor *query_editor;
	gulong qe_changed_id;
	gulong qe_cancel_id;
	gulong qe_activated_id;
	gboolean search_visible;

	/* New location. */
	NautilusLocationChangeType location_change_type;
	guint location_change_distance;
	GFile *pending_location;
	char *pending_scroll_to;
	GList *pending_selection;
	gboolean pending_use_default_location;
	NautilusFile *determine_view_file;
	GCancellable *mount_cancellable;
	GError *mount_error;
	gboolean tried_mount;
	NautilusWindowGoToCallback open_callback;
	gpointer open_callback_user_data;
	gboolean load_with_search;

	gboolean needs_reload;

	GCancellable *find_mount_cancellable;

	gboolean visible;

	/* Back/Forward chain, and history list. 
	 * The data in these lists are NautilusBookmark pointers. 
	 */
	GList *back_list, *forward_list;
};

GType   nautilus_window_slot_get_type (void);

NautilusWindowSlot * nautilus_window_slot_new              (NautilusWindow *window);

NautilusWindow * nautilus_window_slot_get_window           (NautilusWindowSlot *slot);
void             nautilus_window_slot_set_window           (NautilusWindowSlot *slot,
							    NautilusWindow     *window);

void    nautilus_window_slot_update_title		   (NautilusWindowSlot *slot);
void    nautilus_window_slot_set_search_visible            (NautilusWindowSlot *slot,
							    gboolean            visible);

gboolean nautilus_window_slot_handle_event       	   (NautilusWindowSlot *slot,
							    GdkEventKey        *event);

GFile * nautilus_window_slot_get_location		   (NautilusWindowSlot *slot);
char *  nautilus_window_slot_get_location_uri		   (NautilusWindowSlot *slot);

void    nautilus_window_slot_queue_reload		   (NautilusWindowSlot *slot);
void    nautilus_window_slot_force_reload		   (NautilusWindowSlot *slot);

/* convenience wrapper without selection and callback/user_data */
#define nautilus_window_slot_open_location(slot, location, flags)\
	nautilus_window_slot_open_location_full(slot, location, flags, NULL, NULL, NULL)

void nautilus_window_slot_open_location_full (NautilusWindowSlot *slot,
					      GFile *location,
					      NautilusWindowOpenFlags flags,
					      GList *new_selection, /* NautilusFile list */
					      NautilusWindowGoToCallback callback,
					      gpointer user_data);

void			nautilus_window_slot_stop_loading	      (NautilusWindowSlot	*slot);

void			nautilus_window_slot_set_content_view	      (NautilusWindowSlot	*slot,
								       const char		*id);
const char	       *nautilus_window_slot_get_content_view_id      (NautilusWindowSlot	*slot);
gboolean		nautilus_window_slot_content_view_matches_iid (NautilusWindowSlot	*slot,
								       const char		*iid);

void    nautilus_window_slot_go_home			   (NautilusWindowSlot *slot,
							    NautilusWindowOpenFlags flags);
void    nautilus_window_slot_go_up                         (NautilusWindowSlot *slot,
							    NautilusWindowOpenFlags flags);

void    nautilus_window_slot_set_content_view_widget	   (NautilusWindowSlot *slot,
							    NautilusView       *content_view);
void    nautilus_window_slot_set_viewed_file		   (NautilusWindowSlot *slot,
							    NautilusFile      *file);
void    nautilus_window_slot_set_allow_stop		   (NautilusWindowSlot *slot,
							    gboolean	    allow_stop);
void    nautilus_window_slot_set_status			   (NautilusWindowSlot *slot,
							    const char         *primary_status,
							    const char         *detail_status);

void    nautilus_window_slot_add_extra_location_widget     (NautilusWindowSlot *slot,
							    GtkWidget       *widget);
void    nautilus_window_slot_remove_extra_location_widgets (NautilusWindowSlot *slot);

NautilusView * nautilus_window_slot_get_current_view     (NautilusWindowSlot *slot);
char           * nautilus_window_slot_get_current_uri      (NautilusWindowSlot *slot);

void nautilus_window_slot_clear_forward_list (NautilusWindowSlot *slot);
void nautilus_window_slot_clear_back_list    (NautilusWindowSlot *slot);

#endif /* NAUTILUS_WINDOW_SLOT_H */

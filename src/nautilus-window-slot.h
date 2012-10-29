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
};

GType   nautilus_window_slot_get_type (void);

NautilusWindowSlot * nautilus_window_slot_new              (NautilusWindow     *window);

NautilusWindow * nautilus_window_slot_get_window           (NautilusWindowSlot *slot);
void             nautilus_window_slot_set_window           (NautilusWindowSlot *slot,
							    NautilusWindow     *window);

/* convenience wrapper without selection and callback/user_data */
#define nautilus_window_slot_open_location(slot, location, flags)\
	nautilus_window_slot_open_location_full(slot, location, flags, NULL, NULL, NULL)

void nautilus_window_slot_open_location_full              (NautilusWindowSlot *slot,
							   GFile	      *location,
							   NautilusWindowOpenFlags flags,
							   GList	      *new_selection,
							   NautilusWindowGoToCallback callback,
							   gpointer	       user_data);

GFile * nautilus_window_slot_get_location		   (NautilusWindowSlot *slot);
char *  nautilus_window_slot_get_location_uri		   (NautilusWindowSlot *slot);

NautilusFile *    nautilus_window_slot_get_file            (NautilusWindowSlot *slot);
NautilusBookmark *nautilus_window_slot_get_bookmark        (NautilusWindowSlot *slot);
NautilusView *    nautilus_window_slot_get_view            (NautilusWindowSlot *slot);

NautilusView * nautilus_window_slot_get_current_view       (NautilusWindowSlot *slot);
char *         nautilus_window_slot_get_current_uri        (NautilusWindowSlot *slot);

GList * nautilus_window_slot_get_back_history              (NautilusWindowSlot *slot);
GList * nautilus_window_slot_get_forward_history           (NautilusWindowSlot *slot);

GFile * nautilus_window_slot_get_query_editor_location     (NautilusWindowSlot *slot);
void    nautilus_window_slot_set_search_visible            (NautilusWindowSlot *slot,
							    gboolean            visible);

gboolean nautilus_window_slot_get_allow_stop               (NautilusWindowSlot *slot);
void     nautilus_window_slot_set_allow_stop		   (NautilusWindowSlot *slot,
							    gboolean	        allow_stop);
void     nautilus_window_slot_stop_loading                 (NautilusWindowSlot *slot);

const gchar *nautilus_window_slot_get_title                (NautilusWindowSlot *slot);
void         nautilus_window_slot_update_title		   (NautilusWindowSlot *slot);

gboolean nautilus_window_slot_handle_event       	   (NautilusWindowSlot *slot,
							    GdkEventKey        *event);

void    nautilus_window_slot_queue_reload		   (NautilusWindowSlot *slot);

void	 nautilus_window_slot_set_content_view	           (NautilusWindowSlot *slot,
							    const char		*id);

void    nautilus_window_slot_go_home			   (NautilusWindowSlot *slot,
							    NautilusWindowOpenFlags flags);
void    nautilus_window_slot_go_up                         (NautilusWindowSlot *slot,
							    NautilusWindowOpenFlags flags);

void    nautilus_window_slot_set_status			   (NautilusWindowSlot *slot,
							    const char         *primary_status,
							    const char         *detail_status);

#endif /* NAUTILUS_WINDOW_SLOT_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-window-slot.h: Nemo window slot
 
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
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
  
   Author: Christian Neumair <cneumair@gnome.org>
*/

#ifndef NEMO_WINDOW_SLOT_H
#define NEMO_WINDOW_SLOT_H

#include "nemo-view.h"
#include "nemo-window-types.h"
#include "nemo-query-editor.h"

#define NEMO_TYPE_WINDOW_SLOT	 (nemo_window_slot_get_type())
#define NEMO_WINDOW_SLOT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NEMO_TYPE_WINDOW_SLOT, NemoWindowSlotClass))
#define NEMO_WINDOW_SLOT(obj)	 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_WINDOW_SLOT, NemoWindowSlot))
#define NEMO_IS_WINDOW_SLOT(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_WINDOW_SLOT))
#define NEMO_IS_WINDOW_SLOT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NEMO_TYPE_WINDOW_SLOT))
#define NEMO_WINDOW_SLOT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NEMO_TYPE_WINDOW_SLOT, NemoWindowSlotClass))

typedef enum {
	NEMO_LOCATION_CHANGE_STANDARD,
	NEMO_LOCATION_CHANGE_BACK,
	NEMO_LOCATION_CHANGE_FORWARD,
	NEMO_LOCATION_CHANGE_RELOAD
} NemoLocationChangeType;

struct NemoWindowSlotClass {
	GtkBoxClass parent_class;

	/* wrapped NemoWindowInfo signals, for overloading */
	void (* active)		(NemoWindowSlot *slot);
	void (* inactive)	(NemoWindowSlot *slot);
	void (* changed_pane)	(NemoWindowSlot *slot);
};

/* Each NemoWindowSlot corresponds to a location in the window
 * for displaying a NemoView, i.e. a tab.
 */
struct NemoWindowSlot {
	GtkBox parent;

	NemoWindowPane *pane;

	/* slot contains
 	 *  1) an event box containing extra_location_widgets
 	 *  2) the view box for the content view
 	 */
	GtkWidget *extra_location_widgets;

	GtkWidget *view_overlay;
	GtkWidget *floating_bar;

	guint set_status_timeout_id;
	guint loading_timeout_id;

	NemoView *content_view;
	NemoView *new_content_view;

	/* Information about bookmarks */
	NemoBookmark *current_location_bookmark;
	NemoBookmark *last_location_bookmark;

	/* Current location. */
	GFile *location;
	char *title;
	char *status_text;

	NemoFile *viewed_file;
	gboolean viewed_file_seen;
	gboolean viewed_file_in_trash;

	gboolean allow_stop;

	NemoQueryEditor *query_editor;
	gulong qe_changed_id;
	gulong qe_cancel_id;

	/* New location. */
	NemoLocationChangeType location_change_type;
	guint location_change_distance;
	GFile *pending_location;
	char *pending_scroll_to;
	GList *pending_selection;
	NemoFile *determine_view_file;
	GCancellable *mount_cancellable;
	GError *mount_error;
	gboolean tried_mount;
	NemoWindowGoToCallback open_callback;
	gpointer open_callback_user_data;

	GCancellable *find_mount_cancellable;

	gboolean visible;

	/* Back/Forward chain, and history list. 
	 * The data in these lists are NemoBookmark pointers. 
	 */
	GList *back_list, *forward_list;
};

GType   nemo_window_slot_get_type (void);

NemoWindowSlot * nemo_window_slot_new (NemoWindowPane *pane);

void    nemo_window_slot_update_title		   (NemoWindowSlot *slot);
void    nemo_window_slot_update_icon		   (NemoWindowSlot *slot);
void    nemo_window_slot_set_query_editor_visible	   (NemoWindowSlot *slot,
							    gboolean            visible);
gboolean nemo_window_slot_handle_event       	   (NemoWindowSlot *slot,
							    GdkEventKey        *event);

GFile * nemo_window_slot_get_location		   (NemoWindowSlot *slot);
char *  nemo_window_slot_get_location_uri		   (NemoWindowSlot *slot);

void    nemo_window_slot_reload			   (NemoWindowSlot *slot);

/* convenience wrapper without selection and callback/user_data */
#define nemo_window_slot_open_location(slot, location, flags)\
	nemo_window_slot_open_location_full(slot, location, flags, NULL, NULL, NULL)

void nemo_window_slot_open_location_full (NemoWindowSlot *slot,
					      GFile *location,
					      NemoWindowOpenFlags flags,
					      GList *new_selection, /* NemoFile list */
					      NemoWindowGoToCallback callback,
					      gpointer user_data);

void			nemo_window_slot_stop_loading	      (NemoWindowSlot	*slot);

void			nemo_window_slot_set_content_view	      (NemoWindowSlot	*slot,
								       const char		*id);
const char	       *nemo_window_slot_get_content_view_id      (NemoWindowSlot	*slot);
gboolean		nemo_window_slot_content_view_matches_iid (NemoWindowSlot	*slot,
								       const char		*iid);

void    nemo_window_slot_go_home			   (NemoWindowSlot *slot,
							    NemoWindowOpenFlags flags);
void    nemo_window_slot_go_up                         (NemoWindowSlot *slot,
							    NemoWindowOpenFlags flags);

void    nemo_window_slot_set_content_view_widget	   (NemoWindowSlot *slot,
							    NemoView       *content_view);
void    nemo_window_slot_set_viewed_file		   (NemoWindowSlot *slot,
							    NemoFile      *file);
void    nemo_window_slot_set_allow_stop		   (NemoWindowSlot *slot,
							    gboolean	    allow_stop);
void    nemo_window_slot_set_status			   (NemoWindowSlot *slot,
							    const char	 *status,
							    const char   *short_status);

void    nemo_window_slot_add_extra_location_widget     (NemoWindowSlot *slot,
							    GtkWidget       *widget);
void    nemo_window_slot_remove_extra_location_widgets (NemoWindowSlot *slot);

NemoView * nemo_window_slot_get_current_view     (NemoWindowSlot *slot);
char           * nemo_window_slot_get_current_uri      (NemoWindowSlot *slot);
NemoWindow * nemo_window_slot_get_window           (NemoWindowSlot *slot);
void           nemo_window_slot_make_hosting_pane_active (NemoWindowSlot *slot);

gboolean nemo_window_slot_should_close_with_mount (NemoWindowSlot *slot,
						       GMount *mount);

void nemo_window_slot_clear_forward_list (NemoWindowSlot *slot);
void nemo_window_slot_clear_back_list    (NemoWindowSlot *slot);

#endif /* NEMO_WINDOW_SLOT_H */

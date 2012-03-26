/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-dnd.h - Common Drag & drop handling code shared by the icon container
   and the list view.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Pavel Cisler <pavel@eazel.com>,
	    Ettore Perazzoli <ettore@gnu.org>
*/

#ifndef NAUTILUS_DND_H
#define NAUTILUS_DND_H

#include <gtk/gtk.h>

/* Drag & Drop target names. */
#define NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE	"x-special/gnome-icon-list"
#define NAUTILUS_ICON_DND_URI_LIST_TYPE		"text/uri-list"
#define NAUTILUS_ICON_DND_NETSCAPE_URL_TYPE	"_NETSCAPE_URL"
#define NAUTILUS_ICON_DND_BGIMAGE_TYPE		"property/bgimage"
#define NAUTILUS_ICON_DND_ROOTWINDOW_DROP_TYPE	"application/x-rootwindow-drop"
#define NAUTILUS_ICON_DND_XDNDDIRECTSAVE_TYPE	"XdndDirectSave0" /* XDS Protocol Type */
#define NAUTILUS_ICON_DND_RAW_TYPE	"application/octet-stream"

/* Item of the drag selection list */
typedef struct {
	char *uri;
	gboolean got_icon_position;
	int icon_x, icon_y;
	int icon_width, icon_height;
} NautilusDragSelectionItem;

/* Standard Drag & Drop types. */
typedef enum {
	NAUTILUS_ICON_DND_GNOME_ICON_LIST,
	NAUTILUS_ICON_DND_URI_LIST,
	NAUTILUS_ICON_DND_NETSCAPE_URL,
	NAUTILUS_ICON_DND_TEXT,
	NAUTILUS_ICON_DND_XDNDDIRECTSAVE,
	NAUTILUS_ICON_DND_RAW,
	NAUTILUS_ICON_DND_ROOTWINDOW_DROP
} NautilusIconDndTargetType;

typedef enum {
	NAUTILUS_DND_ACTION_FIRST = GDK_ACTION_ASK << 1,
	NAUTILUS_DND_ACTION_SET_AS_BACKGROUND = NAUTILUS_DND_ACTION_FIRST << 0,
	NAUTILUS_DND_ACTION_SET_AS_FOLDER_BACKGROUND = NAUTILUS_DND_ACTION_FIRST << 1,
	NAUTILUS_DND_ACTION_SET_AS_GLOBAL_BACKGROUND = NAUTILUS_DND_ACTION_FIRST << 2
} NautilusDndAction;

/* drag&drop-related information. */
typedef struct {
	GtkTargetList *target_list;

	/* Stuff saved at "receive data" time needed later in the drag. */
	gboolean got_drop_data_type;
	NautilusIconDndTargetType data_type;
	GtkSelectionData *selection_data;
	char *direct_save_uri;

	/* Start of the drag, in window coordinates. */
	int start_x, start_y;

	/* List of NautilusDragSelectionItems, representing items being dragged, or NULL
	 * if data about them has not been received from the source yet.
	 */
	GList *selection_list;

	/* has the drop occured ? */
	gboolean drop_occured;

	/* whether or not need to clean up the previous dnd data */
	gboolean need_to_destroy;

	/* autoscrolling during dragging */
	int auto_scroll_timeout_id;
	gboolean waiting_to_autoscroll;
	gint64 start_auto_scroll_in;

} NautilusDragInfo;

typedef void		(* NautilusDragEachSelectedItemDataGet)	(const char *url, 
								 int x, int y, int w, int h, 
								 gpointer data);
typedef void		(* NautilusDragEachSelectedItemIterator)	(NautilusDragEachSelectedItemDataGet iteratee, 
								 gpointer iterator_context, 
								 gpointer data);

void			    nautilus_drag_init				(NautilusDragInfo		      *drag_info,
									 const GtkTargetEntry		      *drag_types,
									 int				       drag_type_count,
									 gboolean			       add_text_targets);
void			    nautilus_drag_finalize			(NautilusDragInfo		      *drag_info);
NautilusDragSelectionItem  *nautilus_drag_selection_item_new		(void);
void			    nautilus_drag_destroy_selection_list	(GList				      *selection_list);
GList			   *nautilus_drag_build_selection_list		(GtkSelectionData		      *data);

char **			    nautilus_drag_uri_array_from_selection_list (const GList			      *selection_list);
GList *			    nautilus_drag_uri_list_from_selection_list	(const GList			      *selection_list);

char **			    nautilus_drag_uri_array_from_list		(const GList			      *uri_list);
GList *			    nautilus_drag_uri_list_from_array		(const char			     **uris);

gboolean		    nautilus_drag_items_local			(const char			      *target_uri,
									 const GList			      *selection_list);
gboolean		    nautilus_drag_uris_local			(const char			      *target_uri,
									 const GList			      *source_uri_list);
gboolean		    nautilus_drag_items_in_trash		(const GList			      *selection_list);
gboolean		    nautilus_drag_items_on_desktop		(const GList			      *selection_list);
void			    nautilus_drag_default_drop_action_for_icons (GdkDragContext			      *context,
									 const char			      *target_uri,
									 const GList			      *items,
									 int				      *action);
GdkDragAction		    nautilus_drag_default_drop_action_for_netscape_url (GdkDragContext			     *context);
GdkDragAction		    nautilus_drag_default_drop_action_for_uri_list     (GdkDragContext			     *context,
										const char			     *target_uri_string);
gboolean		    nautilus_drag_drag_data_get			(GtkWidget			      *widget,
									 GdkDragContext			      *context,
									 GtkSelectionData		      *selection_data,
									 guint				       info,
									 guint32			       time,
									 gpointer			       container_context,
									 NautilusDragEachSelectedItemIterator  each_selected_item_iterator);
int			    nautilus_drag_modifier_based_action		(int				       default_action,
									 int				       non_default_action);

GdkDragAction		    nautilus_drag_drop_action_ask		(GtkWidget			      *widget,
									 GdkDragAction			       possible_actions);

gboolean		    nautilus_drag_autoscroll_in_scroll_region	(GtkWidget			      *widget);
void			    nautilus_drag_autoscroll_calculate_delta	(GtkWidget			      *widget,
									 float				      *x_scroll_delta,
									 float				      *y_scroll_delta);
void			    nautilus_drag_autoscroll_start		(NautilusDragInfo		      *drag_info,
									 GtkWidget			      *widget,
									 GSourceFunc			       callback,
									 gpointer			       user_data);
void			    nautilus_drag_autoscroll_stop		(NautilusDragInfo		      *drag_info);

gboolean		    nautilus_drag_selection_includes_special_link (GList			      *selection_list);

#endif

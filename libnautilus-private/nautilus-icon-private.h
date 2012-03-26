/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-icon-container-private.h

   Copyright (C) 1999, 2000 Free Software Foundation
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

   Author: Ettore Perazzoli <ettore@gnu.org>
*/

#ifndef NAUTILUS_ICON_CONTAINER_PRIVATE_H
#define NAUTILUS_ICON_CONTAINER_PRIVATE_H

#include <eel/eel-glib-extensions.h>
#include <libnautilus-private/nautilus-icon-canvas-item.h>
#include <libnautilus-private/nautilus-icon-container.h>
#include <libnautilus-private/nautilus-icon-dnd.h>

/* An Icon. */

typedef struct {
	/* Object represented by this icon. */
	NautilusIconData *data;

	/* Canvas item for the icon. */
	NautilusIconCanvasItem *item;

	/* X/Y coordinates. */
	double x, y;

	/*
	 * In RTL mode x is RTL x position, we use saved_ltr_x for
	 * keeping track of x value before it gets converted into
	 * RTL value, this is used for saving the icon position 
	 * to the nautilus metafile. 
	 */
	 double saved_ltr_x;
	
	/* Scale factor (stretches icon). */
	double scale;

	/* Whether this item is selected. */
	eel_boolean_bit is_selected : 1;

	/* Whether this item was selected before rubberbanding. */
	eel_boolean_bit was_selected_before_rubberband : 1;

	/* Whether this item is visible in the view. */
	eel_boolean_bit is_visible : 1;

	/* Whether a monitor was set on this icon. */
	eel_boolean_bit is_monitored : 1;

	eel_boolean_bit has_lazy_position : 1;
} NautilusIcon;


/* Private NautilusIconContainer members. */

typedef struct {
	gboolean active;

	double start_x, start_y;

	EelCanvasItem *selection_rectangle;

	guint timer_id;

	guint prev_x, prev_y;
	EelDRect prev_rect;
	int last_adj_x;
	int last_adj_y;
} NautilusIconRubberbandInfo;

typedef enum {
	DRAG_STATE_INITIAL,
	DRAG_STATE_MOVE_OR_COPY,
	DRAG_STATE_STRETCH
} DragState;

typedef struct {
	/* Pointer position in canvas coordinates. */
	int pointer_x, pointer_y;

	/* Icon top, left, and size in canvas coordinates. */
	int icon_x, icon_y;
	guint icon_size;
} StretchState;

typedef enum {
	AXIS_NONE,
	AXIS_HORIZONTAL,
	AXIS_VERTICAL
} Axis;

enum {
	LABEL_COLOR,
	LABEL_COLOR_HIGHLIGHT,
	LABEL_COLOR_ACTIVE,
	LABEL_COLOR_PRELIGHT,
	LABEL_INFO_COLOR,
	LABEL_INFO_COLOR_HIGHLIGHT,
	LABEL_INFO_COLOR_ACTIVE,
	LAST_LABEL_COLOR
};

struct NautilusIconContainerDetails {
	/* List of icons. */
	GList *icons;
	GList *new_icons;
	GHashTable *icon_set;

	/* Current icon for keyboard navigation. */
	NautilusIcon *keyboard_focus;
	NautilusIcon *keyboard_rubberband_start;

	/* Current icon with stretch handles, so we have only one. */
	NautilusIcon *stretch_icon;
	double stretch_initial_x, stretch_initial_y;
	guint stretch_initial_size;
	
	/* Last highlighted drop target. */
	NautilusIcon *drop_target;

	/* Rubberbanding status. */
	NautilusIconRubberbandInfo rubberband_info;

	/* Timeout used to make a selected icon fully visible after a short
	 * period of time. (The timeout is needed to make sure
	 * double-clicking still works.)
	 */
	guint keyboard_icon_reveal_timer_id;
	NautilusIcon *keyboard_icon_to_reveal;

	/* Used to coalesce selection changed signals in some cases */
	guint selection_changed_id;
	
	/* If a request is made to reveal an unpositioned icon we remember
	 * it and reveal it once it gets positioned (in relayout).
	 */
	NautilusIcon *pending_icon_to_reveal;

	/* If a request is made to rename an unpositioned icon we remember
	 * it and start renaming it once it gets positioned (in relayout).
	 */
	NautilusIcon *pending_icon_to_rename;

	/* Remembered information about the start of the current event. */
	guint32 button_down_time;
	
	/* Drag state. Valid only if drag_button is non-zero. */
	guint drag_button;
	NautilusIcon *drag_icon;
	int drag_x, drag_y;
	DragState drag_state;
	gboolean drag_started;
	StretchState stretch_start;
	gboolean drag_allow_moves;

	gboolean icon_selected_on_button_down;
	NautilusIcon *double_click_icon[2]; /* Both clicks in a double click need to be on the same icon */
	guint double_click_button[2];

	NautilusIcon *range_selection_base_icon;
	
	/* Renaming Details */
	gboolean renaming;
	GtkWidget *rename_widget;	/* Editable text item */
	char *original_text;			/* Copy of editable text for later compare */

	/* Idle ID. */
	guint idle_id;

	/* Idle handler for stretch code */
	guint stretch_idle_id;

	/* Align idle id */
	guint align_idle_id;

	/* DnD info. */
	NautilusIconDndInfo *dnd_info;

	/* zoom level */
	int zoom_level;

	/* specific fonts used to draw labels */
	char *font;
	
	/* font sizes used to draw labels */
	int font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST + 1];

	/* State used so arrow keys don't wander if icons aren't lined up.
	 */
	int arrow_key_start_x;
	int arrow_key_start_y;
	GtkDirectionType arrow_key_direction;

	/* Mode settings. */
	gboolean single_click_mode;
	gboolean auto_layout;
	gboolean tighter_layout;

	/* Whether for the vertical layout, all columns are supposed to
	 * have the same width. */
	gboolean all_columns_same_width;
	
	/* Layout mode */
	NautilusIconLayoutMode layout_mode;

	/* Label position */
	NautilusIconLabelPosition label_position;

	/* Forced icon size, iff greater than 0 */
	int forced_icon_size;

	/* Should the container keep icons aligned to a grid */
	gboolean keep_aligned;

        /* Set to TRUE after first allocation has been done */
	gboolean has_been_allocated;

	int size_allocation_count;
	guint size_allocation_count_id;
	
	/* Is the container fixed or resizable */
	gboolean is_fixed_size;
	
	/* Is the container for a desktop window */
	gboolean is_desktop;

	/* Ignore the visible area the next time the scroll region is recomputed */
	gboolean reset_scroll_region_trigger;
	
	/* The position we are scaling to on stretch */
	double world_x;
	double world_y;

	/* margins to follow, used for the desktop panel avoidance */
	int left_margin;
	int right_margin;
	int top_margin;
	int bottom_margin;

	/* Whether we should use drop shadows for the icon labels or not */
	gboolean use_drop_shadows;
	gboolean drop_shadows_requested;

	/* a11y items used by canvas items */
	guint a11y_item_action_idle_handler;
	GQueue* a11y_item_action_queue;

	eel_boolean_bit is_loading : 1;
	eel_boolean_bit needs_resort : 1;

	eel_boolean_bit store_layout_timestamps : 1;
	eel_boolean_bit store_layout_timestamps_when_finishing_new_icons : 1;
	time_t layout_timestamp;

	/* interactive search */
	gboolean imcontext_changed;
	int selected_iter;
	GtkWidget *search_window;
	GtkWidget *search_entry;
	guint search_entry_changed_id;
	guint typeselect_flush_timeout;
};

/* Private functions shared by mutiple files. */
NautilusIcon *nautilus_icon_container_get_icon_by_uri             (NautilusIconContainer *container,
								   const char            *uri);
void          nautilus_icon_container_move_icon                   (NautilusIconContainer *container,
								   NautilusIcon          *icon,
								   int                    x,
								   int                    y,
								   double                 scale,
								   gboolean               raise,
								   gboolean               snap,
								   gboolean		  update_position);
void          nautilus_icon_container_select_list_unselect_others (NautilusIconContainer *container,
								   GList                 *icons);
char *        nautilus_icon_container_get_icon_uri                (NautilusIconContainer *container,
								   NautilusIcon          *icon);
char *        nautilus_icon_container_get_icon_drop_target_uri    (NautilusIconContainer *container,
								   NautilusIcon          *icon);
void          nautilus_icon_container_update_icon                 (NautilusIconContainer *container,
								   NautilusIcon          *icon);
gboolean      nautilus_icon_container_has_stored_icon_positions   (NautilusIconContainer *container);
gboolean      nautilus_icon_container_scroll                      (NautilusIconContainer *container,
								   int                    delta_x,
								   int                    delta_y);
void          nautilus_icon_container_update_scroll_region        (NautilusIconContainer *container);

#endif /* NAUTILUS_ICON_CONTAINER_PRIVATE_H */

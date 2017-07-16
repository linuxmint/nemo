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
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Author: Ettore Perazzoli <ettore@gnu.org>
*/

#ifndef NEMO_ICON_CONTAINER_PRIVATE_H
#define NEMO_ICON_CONTAINER_PRIVATE_H

#include <eel/eel-glib-extensions.h>
#include <libnemo-private/nemo-icon-canvas-item.h>
#include <libnemo-private/nemo-icon-container.h>
#include <libnemo-private/nemo-icon-dnd.h>
#include <libnemo-private/nemo-icon.h>

/* Private NemoIconContainer members. */

typedef struct {
	double start_x, start_y;

	EelCanvasItem *selection_rectangle;
	EelDRect prev_rect;
	guint timer_id;

	guint prev_x, prev_y;

	int last_adj_x;
	int last_adj_y;
	gboolean active;
} NemoIconRubberbandInfo;

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

typedef struct {
    gint icon_pad_left;
    gint icon_pad_right;
    gint icon_pad_top;
    gint icon_pad_bottom;
    gint container_pad_left;
    gint container_pad_right;
    gint container_pad_top;
    gint container_pad_bottom;
    gint standard_icon_grid_width;
    gint text_beside_icon_grid_width;
    gint desktop_pad_horizontal;
    gint desktop_pad_vertical;
    gint snap_size_x;
    gint snap_size_y;
    gint max_text_width_standard;
    gint max_text_width_tighter;
    gint max_text_width_beside;
    gint max_text_width_beside_top_to_bottom;
    gint icon_vertical_adjust;
} NemoViewLayoutConstants;

typedef struct {
    NemoIconContainer *container;
    GtkBorder *borders;
    int **icon_grid;
    int *grid_memory;
    int num_rows;
    int num_columns;
    int icon_size;
    int real_snap_x;
    int real_snap_y;
    gboolean horizontal;
} NemoCenteredPlacementGrid;

typedef struct {
    NemoIconContainer *container;
    int **icon_grid;
    int *grid_memory;
    int num_rows;
    int num_columns;
    gboolean tight;
} NemoPlacementGrid;

struct NemoIconContainerDetails {
	/* List of icons. */
	GList *icons;
	GList *new_icons;
	GHashTable *icon_set;

	/* Current icon for keyboard navigation. */
	NemoIcon *keyboard_focus;
	NemoIcon *keyboard_rubberband_start;

	/* Rubberbanding status. */
        NemoIconRubberbandInfo rubberband_info;

        NemoViewLayoutConstants *view_constants;
        /* Last highlighted drop target. */
	NemoIcon *drop_target;

	/* Current icon with stretch handles, so we have only one. */
	NemoIcon *stretch_icon;
	double stretch_initial_x, stretch_initial_y;
		/* The position we are scaling to on stretch */
	double world_x;
	double world_y;
	
	guint stretch_initial_size;

	/* Timeout used to make a selected icon fully visible after a short
	 * period of time. (The timeout is needed to make sure
	 * double-clicking still works.)
	 */
	guint keyboard_icon_reveal_timer_id;
	NemoIcon *keyboard_icon_to_reveal;
	
	/* If a request is made to reveal an unpositioned icon we remember
	 * it and reveal it once it gets positioned (in relayout).
	 */
	NemoIcon *pending_icon_to_reveal;

	/* If a request is made to rename an unpositioned icon we remember
	 * it and start renaming it once it gets positioned (in relayout).
	 */
	NemoIcon *pending_icon_to_rename;

	/* Remembered information about the start of the current event. */
	guint32 button_down_time;
	
	/* Drag state. Valid only if drag_button is non-zero. */
	guint drag_button;
	NemoIcon *drag_icon;
	int drag_x, drag_y;
	DragState drag_state;
	gboolean drag_started;
	StretchState stretch_start;
	gboolean drag_allow_moves;

	gboolean icon_selected_on_button_down;
	guint double_click_button[2];
	gboolean skip_rename_on_release;
	NemoIcon *double_click_icon[2]; /* Both clicks in a double click need to be on the same icon */

	NemoIcon *range_selection_base_icon;
	/* DnD info. */
	NemoIconDndInfo *dnd_info;
	
	/* Renaming Details */

	GtkWidget *rename_widget;	/* Editable text item */
	char *original_text;			/* Copy of editable text for later compare */

	char *font; 	/* specific fonts used to draw labels */
        gboolean renaming;

	/* Idle ID. */
	guint idle_id;

	/* Idle handler for stretch code */
	guint stretch_idle_id;

	/* Align idle id */
	guint align_idle_id;

	/* Used to coalesce selection changed signals in some cases */
	guint selection_changed_id;

	/* zoom level */
	int zoom_level;
	
	/* font sizes used to draw labels */
	int font_size_table[NEMO_ZOOM_LEVEL_LARGEST + 1];

	/* State used so arrow keys don't wander if icons aren't lined up.
	 */
	int arrow_key_start_x;
	int arrow_key_start_y;
	GtkDirectionType arrow_key_direction;

	/* Mode settings. */
	gboolean single_click_mode;
	gboolean auto_layout;
        gboolean stored_auto_layout;
	gboolean tighter_layout;
        gboolean click_to_rename;

	/* Whether for the vertical layout, all columns are supposed to
	 * have the same width. */
	gboolean all_columns_same_width;
	
	/* Layout mode */
	NemoIconLayoutMode layout_mode;

	/* Label position */
	NemoIconLabelPosition label_position;

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

        /* Used by desktop grid container only */
        gboolean horizontal;

        gboolean show_desktop_tooltips;
        gboolean show_icon_view_tooltips;

	/* Ignore the visible area the next time the scroll region is recomputed */
	gboolean reset_scroll_region_trigger;
	
        gint tooltip_flags; /* Really a NemoFileTooltipFlags */

	/* margins to follow, used for the desktop panel avoidance */
	int left_margin;
	int right_margin;
	int top_margin;
	int bottom_margin;

	/* Whether we should use drop shadows for the icon labels or not */
	gboolean use_drop_shadows;
	gboolean drop_shadows_requested;
	/* interactive search */
	gboolean imcontext_changed;
	/* a11y items used by canvas items */
	guint a11y_item_action_idle_handler;

        time_t layout_timestamp;

	GtkWidget *search_window;
	GtkWidget *search_entry;
	NemoCenteredPlacementGrid *dnd_grid;
	GQueue* a11y_item_action_queue;
        int selected_iter;
	guint search_entry_changed_id;
	guint typeselect_flush_timeout;

        gint current_dnd_x;
        gint current_dnd_y;
        gboolean insert_dnd_mode;

	eel_boolean_bit is_loading : 1;
	eel_boolean_bit needs_resort : 1;

	eel_boolean_bit store_layout_timestamps : 1;
	eel_boolean_bit store_layout_timestamps_when_finishing_new_icons : 1;
};

typedef struct {
    double width;
    double height;
    double x_offset;
    double y_offset;
} NemoCanvasRects;

#define GET_VIEW_CONSTANT(c,name) (NEMO_ICON_CONTAINER (c)->details->view_constants->name)

#define SNAP_HORIZONTAL(func,x) ((func ((double)((x) - GET_VIEW_CONSTANT (container, desktop_pad_horizontal)) / GET_VIEW_CONSTANT (container, snap_size_x)) * GET_VIEW_CONSTANT (container, snap_size_x)) + GET_VIEW_CONSTANT (container, desktop_pad_horizontal))
#define SNAP_VERTICAL(func, y) ((func ((double)((y) - GET_VIEW_CONSTANT (container, desktop_pad_vertical)) / GET_VIEW_CONSTANT (container, snap_size_y)) * GET_VIEW_CONSTANT (container, snap_size_y)) + GET_VIEW_CONSTANT (container, desktop_pad_vertical))

#define SNAP_NEAREST_HORIZONTAL(x) SNAP_HORIZONTAL (floor, x + .5)
#define SNAP_NEAREST_VERTICAL(y) SNAP_VERTICAL (floor, y + .5)

#define SNAP_CEIL_HORIZONTAL(x) SNAP_HORIZONTAL (ceil, x)
#define SNAP_CEIL_VERTICAL(y) SNAP_VERTICAL (ceil, y)

/* Private functions shared by mutiple files. */
NemoIcon *nemo_icon_container_get_icon_by_uri             (NemoIconContainer *container,
								   const char            *uri);
void          nemo_icon_container_select_list_unselect_others (NemoIconContainer *container,
								   GList                 *icons);
char *        nemo_icon_container_get_icon_uri                (NemoIconContainer *container,
								   NemoIcon          *icon);
char *        nemo_icon_container_get_icon_drop_target_uri    (NemoIconContainer *container,
								   NemoIcon          *icon);
void          nemo_icon_container_update_icon                 (NemoIconContainer *container,
								   NemoIcon          *icon);
gboolean      nemo_icon_container_scroll                      (NemoIconContainer *container,
								   int                    delta_x,
								   int                    delta_y);
void          nemo_icon_container_update_scroll_region        (NemoIconContainer *container);
gint              nemo_icon_container_get_canvas_height (NemoIconContainer *container,
                                                         GtkAllocation      allocation);
gint              nemo_icon_container_get_canvas_width (NemoIconContainer *container,
                                                        GtkAllocation      allocation);
double        nemo_icon_container_get_mirror_x_position (NemoIconContainer *container, NemoIcon *icon, double x);
void          nemo_icon_container_set_rtl_positions (NemoIconContainer *container);
void          nemo_icon_container_end_renaming_mode (NemoIconContainer *container, gboolean commit);
NemoIcon     *nemo_icon_container_get_icon_being_renamed (NemoIconContainer *container);

void              nemo_icon_container_icon_set_position (NemoIconContainer *container,
                                                         NemoIcon          *icon,
                                                         gdouble            x,
                                                         gdouble            y);
void              nemo_icon_container_icon_get_bounding_box (NemoIconContainer *container, NemoIcon *icon,
                                                             int *x1_return, int *y1_return,
                                                             int *x2_return, int *y2_return,
                                                             NemoIconCanvasItemBoundsUsage usage);

void              nemo_icon_container_move_icon                     (NemoIconContainer *container,
                                                                     NemoIcon *icon,
                                                                     int x, int y,
                                                                     double scale,
                                                                     gboolean raise,
                                                                     gboolean snap,
                                                                     gboolean update_position);

void          nemo_icon_container_icon_raise                  (NemoIconContainer *container,
                                                               NemoIcon *icon);
void          nemo_icon_container_finish_adding_icon                (NemoIconContainer *container,
                                                                     NemoIcon           *icon);
gboolean      nemo_icon_container_icon_is_positioned (const NemoIcon *icon);
void          nemo_icon_container_sort_icons (NemoIconContainer *container,
                                              GList            **icons);
void          nemo_icon_container_resort (NemoIconContainer *container);
void          nemo_icon_container_get_all_icon_bounds (NemoIconContainer *container,
                                                       double *x1, double *y1,
                                                       double *x2, double *y2,
                                                       NemoIconCanvasItemBoundsUsage usage);
void          nemo_icon_container_store_layout_timestamps_now (NemoIconContainer *container);
void          nemo_icon_container_redo_layout (NemoIconContainer *container);
void          nemo_icon_container_start_monitor_top_left (NemoIconContainer *container,
                                                          NemoIconData *data,
                                                          gconstpointer client,
                                                          gboolean large_text);
void          nemo_icon_container_stop_monitor_top_left (NemoIconContainer *container,
                                                         NemoIconData *data,
                                                         gconstpointer client);
NemoIconInfo *nemo_icon_container_get_icon_images (NemoIconContainer *container,
                                                   NemoIconData      *data,
                                                   int                    size,
                                                   char                 **embedded_text,
                                                   gboolean               for_drag_accept,
                                                   gboolean               need_large_embeddded_text,
                                                   gboolean              *embedded_text_needs_loading,
                                                   gboolean              *has_open_window);
void          nemo_icon_container_get_icon_text (NemoIconContainer *container,
                                                 NemoIconData      *data,
                                                 char                 **editable_text,
                                                 char                 **additional_text,
                                                 gboolean               include_invisible);

/* nemo-centered-placement-grid api
 *
 * used by nemo-icon-view-grid-container.c, nemo-icon-dnd.h
 */

NemoCenteredPlacementGrid *nemo_centered_placement_grid_new               (NemoIconContainer *container, gboolean horizontal);
void               nemo_centered_placement_grid_free              (NemoCenteredPlacementGrid *grid);

void               nemo_centered_placement_grid_nominal_to_icon_position (NemoCenteredPlacementGrid *grid,
                                                                          NemoIcon                  *icon,
                                                                          gint                       x_nominal,
                                                                          gint                       y_nominal,
                                                                          gint                      *x_adjusted,
                                                                          gint                      *y_adjusted);
void               nemo_centered_placement_grid_icon_position_to_nominal (NemoCenteredPlacementGrid *grid,
                                                                          NemoIcon                  *icon,
                                                                          gint                       x_adjusted,
                                                                          gint                       y_adjusted,
                                                                          gint                      *x_nominal,
                                                                          gint                      *y_nominal);
void               nemo_centered_placement_grid_mark_icon         (NemoCenteredPlacementGrid *grid, NemoIcon *icon);
void               nemo_centered_placement_grid_unmark_icon       (NemoCenteredPlacementGrid *grid, NemoIcon *icon);
void               nemo_centered_placement_grid_mark_position (NemoCenteredPlacementGrid *grid,
                                                               gint                       x,
                                                               gint                       y);
void               nemo_centered_placement_grid_unmark_position (NemoCenteredPlacementGrid *grid,
                                                                 gint                       x,
                                                                 gint                       y);
void               nemo_centered_placement_grid_pre_populate        (NemoCenteredPlacementGrid *grid,
                                                                     GList                     *icons,
                                                                     gboolean                   ignore_lazy);
void               nemo_centered_placement_grid_get_next_position_rect (NemoCenteredPlacementGrid *grid,
                                                                        GdkRectangle              *in_rect,
                                                                        GdkRectangle              *out_rect,
                                                                        gboolean                  *is_free);
void               nemo_centered_placement_grid_get_current_position_rect (NemoCenteredPlacementGrid *grid,
                                                                           gint                       x,
                                                                           gint                       y,
                                                                           GdkRectangle              *rect,
                                                                           gboolean                  *is_free);
NemoIcon *         nemo_centered_placement_grid_get_icon_at_position (NemoCenteredPlacementGrid *grid,
                                                                      gint                       x,
                                                                      gint                       y);
GList *            nemo_centered_placement_grid_clear_grid_for_selection (NemoCenteredPlacementGrid *grid,
                                                                          gint                       start_x,
                                                                          gint                       start_y,
                                                                          GList                     *drag_sel_list);
/* nemo-placement-grid api
 *
 * used by nemo-icon-view-container.c
 */

NemoPlacementGrid *nemo_placement_grid_new               (NemoIconContainer *container, gboolean tight);
void               nemo_placement_grid_free              (NemoPlacementGrid *grid);
gboolean           nemo_placement_grid_position_is_free  (NemoPlacementGrid *grid, EelIRect pos);
void               nemo_placement_grid_mark              (NemoPlacementGrid *grid, EelIRect pos);
void               nemo_placement_grid_canvas_position_to_grid_position (NemoPlacementGrid *grid, EelIRect canvas_position, EelIRect *grid_position);
void               nemo_placement_grid_mark_icon         (NemoPlacementGrid *grid, NemoIcon *icon);

#endif /* NEMO_ICON_CONTAINER_PRIVATE_H */

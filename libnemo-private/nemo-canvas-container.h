/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* gnome-canvas-container.h - Canvas container widget.

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

   Authors: Ettore Perazzoli <ettore@gnu.org>, Darin Adler <darin@bentspoon.com>
*/

#ifndef NEMO_CANVAS_CONTAINER_H
#define NEMO_CANVAS_CONTAINER_H

#include <eel/eel-canvas.h>
#include <libnemo-private/nemo-icon-info.h>

#define NEMO_TYPE_CANVAS_CONTAINER nemo_canvas_container_get_type()
#define NEMO_CANVAS_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_CANVAS_CONTAINER, NemoCanvasContainer))
#define NEMO_CANVAS_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_CANVAS_CONTAINER, NemoCanvasContainerClass))
#define NEMO_IS_CANVAS_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_CANVAS_CONTAINER))
#define NEMO_IS_CANVAS_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_CANVAS_CONTAINER))
#define NEMO_CANVAS_CONTAINER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_CANVAS_CONTAINER, NemoCanvasContainerClass))


#define NEMO_CANVAS_ICON_DATA(pointer) \
	((NemoCanvasIconData *) (pointer))

typedef struct NemoCanvasIconData NemoCanvasIconData;

typedef void (* NemoCanvasCallback) (NemoCanvasIconData *icon_data,
					 gpointer callback_data);

typedef struct {
	int x;
	int y;
	double scale;
} NemoCanvasPosition;

typedef enum {
	NEMO_CANVAS_LAYOUT_L_R_T_B,
	NEMO_CANVAS_LAYOUT_R_L_T_B,
	NEMO_CANVAS_LAYOUT_T_B_L_R,
	NEMO_CANVAS_LAYOUT_T_B_R_L
} NemoCanvasLayoutMode;

typedef enum {
	NEMO_CANVAS_LABEL_POSITION_UNDER,
	NEMO_CANVAS_LABEL_POSITION_BESIDE
} NemoCanvasLabelPosition;

#define	NEMO_CANVAS_CONTAINER_TYPESELECT_FLUSH_DELAY 1000000

typedef struct NemoCanvasContainerDetails NemoCanvasContainerDetails;

typedef struct {
	EelCanvas canvas;
	NemoCanvasContainerDetails *details;
} NemoCanvasContainer;

typedef struct {
	EelCanvasClass parent_slot;

	/* Operations on the container. */
	int          (* button_press) 	          (NemoCanvasContainer *container,
						   GdkEventButton *event);
	void         (* context_click_background) (NemoCanvasContainer *container,
						   GdkEventButton *event);
	void         (* middle_click) 		  (NemoCanvasContainer *container,
						   GdkEventButton *event);

	/* Operations on icons. */
	void         (* activate)	  	  (NemoCanvasContainer *container,
						   NemoCanvasIconData *data);
	void         (* activate_alternate)       (NemoCanvasContainer *container,
						   NemoCanvasIconData *data);
	void         (* activate_previewer)       (NemoCanvasContainer *container,
						   GList *files,
						   GArray *locations);
	void         (* context_click_selection)  (NemoCanvasContainer *container,
						   GdkEventButton *event);
	void	     (* move_copy_items)	  (NemoCanvasContainer *container,
						   const GList *item_uris,
						   GdkPoint *relative_item_points,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_netscape_url)	  (NemoCanvasContainer *container,
						   const char *url,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_uri_list)    	  (NemoCanvasContainer *container,
						   const char *uri_list,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_text)		  (NemoCanvasContainer *container,
						   const char *text,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_raw)		  (NemoCanvasContainer *container,
						   char *raw_data,
						   int length,
						   const char *target_uri,
						   const char *direct_save_uri,
						   GdkDragAction action,
						   int x,
						   int y);

	/* Queries on the container for subclass/client.
	 * These must be implemented. The default "do nothing" is not good enough.
	 */
	char *	     (* get_container_uri)	  (NemoCanvasContainer *container);

	/* Queries on icons for subclass/client.
	 * These must be implemented. The default "do nothing" is not
	 * good enough, these are _not_ signals.
	 */
	NemoIconInfo *(* get_icon_images)     (NemoCanvasContainer *container,
						     NemoCanvasIconData *data,
						     int canvas_size,
						     char **embedded_text,
						     gboolean for_drag_accept,
						     gboolean need_large_embeddded_text,
						     gboolean *embedded_text_needs_loading,
						     gboolean *has_window_open);
	void         (* get_icon_text)            (NemoCanvasContainer *container,
						     NemoCanvasIconData *data,
						     char **editable_text,
						     char **additional_text,
						     gboolean include_invisible);
	char *       (* get_icon_description)     (NemoCanvasContainer *container,
						     NemoCanvasIconData *data);
	int          (* compare_icons)            (NemoCanvasContainer *container,
						     NemoCanvasIconData *canvas_a,
						     NemoCanvasIconData *canvas_b);
	int          (* compare_icons_by_name)    (NemoCanvasContainer *container,
						     NemoCanvasIconData *canvas_a,
						     NemoCanvasIconData *canvas_b);
	void         (* freeze_updates)           (NemoCanvasContainer *container);
	void         (* unfreeze_updates)         (NemoCanvasContainer *container);
	void         (* start_monitor_top_left)   (NemoCanvasContainer *container,
						   NemoCanvasIconData *data,
						   gconstpointer client,
						   gboolean large_text);
	void         (* stop_monitor_top_left)    (NemoCanvasContainer *container,
						   NemoCanvasIconData *data,
						   gconstpointer client);
	void         (* prioritize_thumbnailing)  (NemoCanvasContainer *container,
						   NemoCanvasIconData *data);

	/* Queries on icons for subclass/client.
	 * These must be implemented => These are signals !
	 * The default "do nothing" is not good enough.
	 */
	gboolean     (* can_accept_item)	  (NemoCanvasContainer *container,
						   NemoCanvasIconData *target, 
						   const char *item_uri);
	gboolean     (* get_stored_icon_position) (NemoCanvasContainer *container,
						     NemoCanvasIconData *data,
						     NemoCanvasPosition *position);
	char *       (* get_icon_uri)             (NemoCanvasContainer *container,
						     NemoCanvasIconData *data);
	char *       (* get_icon_drop_target_uri) (NemoCanvasContainer *container,
						     NemoCanvasIconData *data);

	/* If canvas data is NULL, the layout timestamp of the container should be retrieved.
	 * That is the time when the container displayed a fully loaded directory with
	 * all canvas positions assigned.
	 *
	 * If canvas data is not NULL, the position timestamp of the canvas should be retrieved.
	 * That is the time when the file (i.e. canvas data payload) was last displayed in a
	 * fully loaded directory with all canvas positions assigned.
	 */
	gboolean     (* get_stored_layout_timestamp) (NemoCanvasContainer *container,
						      NemoCanvasIconData *data,
						      time_t *time);
	/* If canvas data is NULL, the layout timestamp of the container should be stored.
	 * If canvas data is not NULL, the position timestamp of the container should be stored.
	 */
	gboolean     (* store_layout_timestamp) (NemoCanvasContainer *container,
						 NemoCanvasIconData *data,
						 const time_t *time);

	/* Notifications for the whole container. */
	void	     (* band_select_started)	  (NemoCanvasContainer *container);
	void	     (* band_select_ended)	  (NemoCanvasContainer *container);
	void         (* selection_changed) 	  (NemoCanvasContainer *container);
	void         (* layout_changed)           (NemoCanvasContainer *container);

	/* Notifications for icons. */
	void         (* icon_position_changed)    (NemoCanvasContainer *container,
						     NemoCanvasIconData *data,
						     const NemoCanvasPosition *position);
	void         (* icon_rename_started)      (NemoCanvasContainer *container,
						     GtkWidget *renaming_widget);
	void         (* icon_rename_ended)        (NemoCanvasContainer *container,
						     NemoCanvasIconData *data,
						     const char *text);
	void	     (* icon_stretch_started)     (NemoCanvasContainer *container,
						     NemoCanvasIconData *data);
	void	     (* icon_stretch_ended)       (NemoCanvasContainer *container,
						     NemoCanvasIconData *data);
	int	     (* preview)		  (NemoCanvasContainer *container,
						   NemoCanvasIconData *data,
						   gboolean start_flag);
        void         (* icon_added)               (NemoCanvasContainer *container,
						     NemoCanvasIconData *data);
        void         (* icon_removed)             (NemoCanvasContainer *container,
						     NemoCanvasIconData *data);
        void         (* cleared)                  (NemoCanvasContainer *container);
	gboolean     (* start_interactive_search) (NemoCanvasContainer *container);
} NemoCanvasContainerClass;

/* GtkObject */
GType             nemo_canvas_container_get_type                      (void);
GtkWidget *       nemo_canvas_container_new                           (void);


/* adding, removing, and managing icons */
void              nemo_canvas_container_clear                         (NemoCanvasContainer  *view);
gboolean          nemo_canvas_container_add                           (NemoCanvasContainer  *view,
									   NemoCanvasIconData       *data);
void              nemo_canvas_container_layout_now                    (NemoCanvasContainer *container);
gboolean          nemo_canvas_container_remove                        (NemoCanvasContainer  *view,
									   NemoCanvasIconData       *data);
void              nemo_canvas_container_for_each                      (NemoCanvasContainer  *view,
									   NemoCanvasCallback    callback,
									   gpointer                callback_data);
void              nemo_canvas_container_request_update                (NemoCanvasContainer  *view,
									   NemoCanvasIconData       *data);
void              nemo_canvas_container_request_update_all            (NemoCanvasContainer  *container);
void              nemo_canvas_container_reveal                        (NemoCanvasContainer  *container,
									   NemoCanvasIconData       *data);
gboolean          nemo_canvas_container_is_empty                      (NemoCanvasContainer  *container);
NemoCanvasIconData *nemo_canvas_container_get_first_visible_icon        (NemoCanvasContainer  *container);
void              nemo_canvas_container_scroll_to_canvas                (NemoCanvasContainer  *container,
									     NemoCanvasIconData       *data);

void              nemo_canvas_container_begin_loading                 (NemoCanvasContainer  *container);
void              nemo_canvas_container_end_loading                   (NemoCanvasContainer  *container,
									   gboolean                all_icons_added);

/* control the layout */
gboolean          nemo_canvas_container_is_auto_layout                (NemoCanvasContainer  *container);
void              nemo_canvas_container_set_auto_layout               (NemoCanvasContainer  *container,
									   gboolean                auto_layout);

gboolean          nemo_canvas_container_is_tighter_layout             (NemoCanvasContainer  *container);
void              nemo_canvas_container_set_tighter_layout            (NemoCanvasContainer  *container,
                                     gboolean                tighter_layout);




gboolean          nemo_canvas_container_is_keep_aligned               (NemoCanvasContainer  *container);
void              nemo_canvas_container_set_keep_aligned              (NemoCanvasContainer  *container,
									   gboolean                keep_aligned);
void              nemo_canvas_container_set_layout_mode               (NemoCanvasContainer  *container,
									   NemoCanvasLayoutMode  mode);
void              nemo_canvas_container_set_label_position            (NemoCanvasContainer  *container,
									   NemoCanvasLabelPosition pos);
void              nemo_canvas_container_sort                          (NemoCanvasContainer  *container);
void              nemo_canvas_container_freeze_icon_positions         (NemoCanvasContainer  *container);

int               nemo_canvas_container_get_max_layout_lines           (NemoCanvasContainer  *container);
int               nemo_canvas_container_get_max_layout_lines_for_pango (NemoCanvasContainer  *container);

void              nemo_canvas_container_set_highlighted_for_clipboard (NemoCanvasContainer  *container,
									   GList                  *clipboard_canvas_data);

/* operations on all icons */
void              nemo_canvas_container_unselect_all                  (NemoCanvasContainer  *view);
void              nemo_canvas_container_select_all                    (NemoCanvasContainer  *view);


void              nemo_canvas_container_select_first                  (NemoCanvasContainer  *view);


/* operations on the selection */
GList     *       nemo_canvas_container_get_selection                 (NemoCanvasContainer  *view);
void			  nemo_canvas_container_invert_selection				(NemoCanvasContainer  *view);
void              nemo_canvas_container_set_selection                 (NemoCanvasContainer  *view,
									   GList                  *selection);
GArray    *       nemo_canvas_container_get_selected_icon_locations   (NemoCanvasContainer  *view);
gboolean          nemo_canvas_container_has_stretch_handles           (NemoCanvasContainer  *container);
gboolean          nemo_canvas_container_is_stretched                  (NemoCanvasContainer  *container);
void              nemo_canvas_container_show_stretch_handles          (NemoCanvasContainer  *container);
void              nemo_canvas_container_unstretch                     (NemoCanvasContainer  *container);
void              nemo_canvas_container_start_renaming_selected_item  (NemoCanvasContainer  *container,
									   gboolean                select_all);

/* options */
NemoZoomLevel nemo_canvas_container_get_zoom_level                (NemoCanvasContainer  *view);
void              nemo_canvas_container_set_zoom_level                (NemoCanvasContainer  *view,
									   int                     new_zoom_level);
void              nemo_canvas_container_set_single_click_mode         (NemoCanvasContainer  *container,
									   gboolean                single_click_mode);
void              nemo_canvas_container_enable_linger_selection       (NemoCanvasContainer  *view,
									   gboolean                enable);
gboolean          nemo_canvas_container_get_is_fixed_size             (NemoCanvasContainer  *container);
void              nemo_canvas_container_set_is_fixed_size             (NemoCanvasContainer  *container,
									   gboolean                is_fixed_size);
gboolean          nemo_canvas_container_get_is_desktop                (NemoCanvasContainer  *container);
void              nemo_canvas_container_set_is_desktop                (NemoCanvasContainer  *container,
									   gboolean                is_desktop);
void              nemo_canvas_container_reset_scroll_region           (NemoCanvasContainer  *container);
void              nemo_canvas_container_set_font                      (NemoCanvasContainer  *container,
									   const char             *font); 
void              nemo_canvas_container_set_margins                   (NemoCanvasContainer  *container,
									   int                     left_margin,
									   int                     right_margin,
									   int                     top_margin,
									   int                     bottom_margin);
void              nemo_canvas_container_set_use_drop_shadows          (NemoCanvasContainer  *container,
									   gboolean                use_drop_shadows);
char*             nemo_canvas_container_get_icon_description          (NemoCanvasContainer  *container,
									     NemoCanvasIconData       *data);
gboolean          nemo_canvas_container_get_allow_moves               (NemoCanvasContainer  *container);
void              nemo_canvas_container_set_allow_moves               (NemoCanvasContainer  *container,
									   gboolean                allow_moves);
void         nemo_canvas_container_set_forced_icon_size      (NemoCanvasContainer  *container,
                                    int                     forced_icon_size);
void         nemo_canvas_container_set_all_columns_same_width    (NemoCanvasContainer  *container,
                                    gboolean                all_columns_same_width);

gboolean	  nemo_canvas_container_is_layout_rtl			(NemoCanvasContainer  *container);
gboolean	  nemo_canvas_container_is_layout_vertical		(NemoCanvasContainer  *container);

gboolean          nemo_canvas_container_get_store_layout_timestamps   (NemoCanvasContainer  *container);
void              nemo_canvas_container_set_store_layout_timestamps   (NemoCanvasContainer  *container,
									   gboolean                store_layout);

void              nemo_canvas_container_widget_to_file_operation_position (NemoCanvasContainer *container,
									     GdkPoint              *position);

void         nemo_canvas_container_setup_tooltip_preference_callback (NemoCanvasContainer *container);

#define CANVAS_WIDTH(container,allocation) ((allocation.width	  \
				- container->details->left_margin \
				- container->details->right_margin) \
				/  EEL_CANVAS (container)->pixels_per_unit)

#define CANVAS_HEIGHT(container,allocation) ((allocation.height \
			 - container->details->top_margin \
			 - container->details->bottom_margin) \
			 / EEL_CANVAS (container)->pixels_per_unit)

#endif /* NEMO_ICON_CONTAINER_H */

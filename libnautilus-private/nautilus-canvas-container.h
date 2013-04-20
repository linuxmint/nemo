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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ettore Perazzoli <ettore@gnu.org>, Darin Adler <darin@bentspoon.com>
*/

#ifndef NAUTILUS_CANVAS_CONTAINER_H
#define NAUTILUS_CANVAS_CONTAINER_H

#include <eel/eel-canvas.h>
#include <libnautilus-private/nautilus-icon-info.h>

#define NAUTILUS_TYPE_CANVAS_CONTAINER nautilus_canvas_container_get_type()
#define NAUTILUS_CANVAS_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_CANVAS_CONTAINER, NautilusCanvasContainer))
#define NAUTILUS_CANVAS_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CANVAS_CONTAINER, NautilusCanvasContainerClass))
#define NAUTILUS_IS_CANVAS_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_CANVAS_CONTAINER))
#define NAUTILUS_IS_CANVAS_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_CANVAS_CONTAINER))
#define NAUTILUS_CANVAS_CONTAINER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_CANVAS_CONTAINER, NautilusCanvasContainerClass))


#define NAUTILUS_CANVAS_ICON_DATA(pointer) \
	((NautilusCanvasIconData *) (pointer))

typedef struct NautilusCanvasIconData NautilusCanvasIconData;

typedef void (* NautilusCanvasCallback) (NautilusCanvasIconData *icon_data,
					 gpointer callback_data);

typedef struct {
	int x;
	int y;
	double scale;
} NautilusCanvasPosition;

typedef enum {
	NAUTILUS_CANVAS_LABEL_POSITION_UNDER,
} NautilusCanvasLabelPosition;

#define	NAUTILUS_CANVAS_CONTAINER_TYPESELECT_FLUSH_DELAY 1000000

typedef struct NautilusCanvasContainerDetails NautilusCanvasContainerDetails;

typedef struct {
	EelCanvas canvas;
	NautilusCanvasContainerDetails *details;
} NautilusCanvasContainer;

typedef struct {
	EelCanvasClass parent_slot;

	/* Operations on the container. */
	int          (* button_press) 	          (NautilusCanvasContainer *container,
						   GdkEventButton *event);
	void         (* context_click_background) (NautilusCanvasContainer *container,
						   GdkEventButton *event);
	void         (* middle_click) 		  (NautilusCanvasContainer *container,
						   GdkEventButton *event);

	/* Operations on icons. */
	void         (* activate)	  	  (NautilusCanvasContainer *container,
						   NautilusCanvasIconData *data);
	void         (* activate_alternate)       (NautilusCanvasContainer *container,
						   NautilusCanvasIconData *data);
	void         (* activate_previewer)       (NautilusCanvasContainer *container,
						   GList *files,
						   GArray *locations);
	void         (* context_click_selection)  (NautilusCanvasContainer *container,
						   GdkEventButton *event);
	void	     (* move_copy_items)	  (NautilusCanvasContainer *container,
						   const GList *item_uris,
						   GdkPoint *relative_item_points,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_netscape_url)	  (NautilusCanvasContainer *container,
						   const char *url,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_uri_list)    	  (NautilusCanvasContainer *container,
						   const char *uri_list,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_text)		  (NautilusCanvasContainer *container,
						   const char *text,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_raw)		  (NautilusCanvasContainer *container,
						   char *raw_data,
						   int length,
						   const char *target_uri,
						   const char *direct_save_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_hover)		  (NautilusCanvasContainer *container,
						   const char *target_uri);

	/* Queries on the container for subclass/client.
	 * These must be implemented. The default "do nothing" is not good enough.
	 */
	char *	     (* get_container_uri)	  (NautilusCanvasContainer *container);

	/* Queries on icons for subclass/client.
	 * These must be implemented. The default "do nothing" is not
	 * good enough, these are _not_ signals.
	 */
	NautilusIconInfo *(* get_icon_images)     (NautilusCanvasContainer *container,
						     NautilusCanvasIconData *data,
						     int canvas_size,
						     char **embedded_text,
						     gboolean for_drag_accept,
						     gboolean need_large_embeddded_text,
						     gboolean *embedded_text_needs_loading,
						     gboolean *has_window_open);
	void         (* get_icon_text)            (NautilusCanvasContainer *container,
						     NautilusCanvasIconData *data,
						     char **editable_text,
						     char **additional_text,
						     gboolean include_invisible);
	char *       (* get_icon_description)     (NautilusCanvasContainer *container,
						     NautilusCanvasIconData *data);
	int          (* compare_icons)            (NautilusCanvasContainer *container,
						     NautilusCanvasIconData *canvas_a,
						     NautilusCanvasIconData *canvas_b);
	int          (* compare_icons_by_name)    (NautilusCanvasContainer *container,
						     NautilusCanvasIconData *canvas_a,
						     NautilusCanvasIconData *canvas_b);
	void         (* freeze_updates)           (NautilusCanvasContainer *container);
	void         (* unfreeze_updates)         (NautilusCanvasContainer *container);
	void         (* start_monitor_top_left)   (NautilusCanvasContainer *container,
						   NautilusCanvasIconData *data,
						   gconstpointer client,
						   gboolean large_text);
	void         (* stop_monitor_top_left)    (NautilusCanvasContainer *container,
						   NautilusCanvasIconData *data,
						   gconstpointer client);
	void         (* prioritize_thumbnailing)  (NautilusCanvasContainer *container,
						   NautilusCanvasIconData *data);

	/* Queries on icons for subclass/client.
	 * These must be implemented => These are signals !
	 * The default "do nothing" is not good enough.
	 */
	gboolean     (* can_accept_item)	  (NautilusCanvasContainer *container,
						   NautilusCanvasIconData *target, 
						   const char *item_uri);
	gboolean     (* get_stored_icon_position) (NautilusCanvasContainer *container,
						     NautilusCanvasIconData *data,
						     NautilusCanvasPosition *position);
	char *       (* get_icon_uri)             (NautilusCanvasContainer *container,
						     NautilusCanvasIconData *data);
	char *       (* get_icon_activation_uri)  (NautilusCanvasContainer *container,
						     NautilusCanvasIconData *data);
	char *       (* get_icon_drop_target_uri) (NautilusCanvasContainer *container,
						     NautilusCanvasIconData *data);

	/* If canvas data is NULL, the layout timestamp of the container should be retrieved.
	 * That is the time when the container displayed a fully loaded directory with
	 * all canvas positions assigned.
	 *
	 * If canvas data is not NULL, the position timestamp of the canvas should be retrieved.
	 * That is the time when the file (i.e. canvas data payload) was last displayed in a
	 * fully loaded directory with all canvas positions assigned.
	 */
	gboolean     (* get_stored_layout_timestamp) (NautilusCanvasContainer *container,
						      NautilusCanvasIconData *data,
						      time_t *time);
	/* If canvas data is NULL, the layout timestamp of the container should be stored.
	 * If canvas data is not NULL, the position timestamp of the container should be stored.
	 */
	gboolean     (* store_layout_timestamp) (NautilusCanvasContainer *container,
						 NautilusCanvasIconData *data,
						 const time_t *time);

	/* Notifications for the whole container. */
	void	     (* band_select_started)	  (NautilusCanvasContainer *container);
	void	     (* band_select_ended)	  (NautilusCanvasContainer *container);
	void         (* selection_changed) 	  (NautilusCanvasContainer *container);
	void         (* layout_changed)           (NautilusCanvasContainer *container);

	/* Notifications for icons. */
	void         (* icon_position_changed)    (NautilusCanvasContainer *container,
						     NautilusCanvasIconData *data,
						     const NautilusCanvasPosition *position);
	void         (* icon_rename_started)      (NautilusCanvasContainer *container,
						     GtkWidget *renaming_widget);
	void         (* icon_rename_ended)        (NautilusCanvasContainer *container,
						     NautilusCanvasIconData *data,
						     const char *text);
	void	     (* icon_stretch_started)     (NautilusCanvasContainer *container,
						     NautilusCanvasIconData *data);
	void	     (* icon_stretch_ended)       (NautilusCanvasContainer *container,
						     NautilusCanvasIconData *data);
	int	     (* preview)		  (NautilusCanvasContainer *container,
						   NautilusCanvasIconData *data,
						   gboolean start_flag);
        void         (* icon_added)               (NautilusCanvasContainer *container,
						     NautilusCanvasIconData *data);
        void         (* icon_removed)             (NautilusCanvasContainer *container,
						     NautilusCanvasIconData *data);
        void         (* cleared)                  (NautilusCanvasContainer *container);
	gboolean     (* start_interactive_search) (NautilusCanvasContainer *container);
} NautilusCanvasContainerClass;

/* GtkObject */
GType             nautilus_canvas_container_get_type                      (void);
GtkWidget *       nautilus_canvas_container_new                           (void);


/* adding, removing, and managing icons */
void              nautilus_canvas_container_clear                         (NautilusCanvasContainer  *view);
gboolean          nautilus_canvas_container_add                           (NautilusCanvasContainer  *view,
									   NautilusCanvasIconData       *data);
void              nautilus_canvas_container_layout_now                    (NautilusCanvasContainer *container);
gboolean          nautilus_canvas_container_remove                        (NautilusCanvasContainer  *view,
									   NautilusCanvasIconData       *data);
void              nautilus_canvas_container_for_each                      (NautilusCanvasContainer  *view,
									   NautilusCanvasCallback    callback,
									   gpointer                callback_data);
void              nautilus_canvas_container_request_update                (NautilusCanvasContainer  *view,
									   NautilusCanvasIconData       *data);
void              nautilus_canvas_container_request_update_all            (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_reveal                        (NautilusCanvasContainer  *container,
									   NautilusCanvasIconData       *data);
gboolean          nautilus_canvas_container_is_empty                      (NautilusCanvasContainer  *container);
NautilusCanvasIconData *nautilus_canvas_container_get_first_visible_icon        (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_scroll_to_canvas                (NautilusCanvasContainer  *container,
									     NautilusCanvasIconData       *data);

void              nautilus_canvas_container_begin_loading                 (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_end_loading                   (NautilusCanvasContainer  *container,
									   gboolean                all_icons_added);

/* control the layout */
gboolean          nautilus_canvas_container_is_auto_layout                (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_set_auto_layout               (NautilusCanvasContainer  *container,
									   gboolean                auto_layout);

gboolean          nautilus_canvas_container_is_keep_aligned               (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_set_keep_aligned              (NautilusCanvasContainer  *container,
									   gboolean                keep_aligned);
void              nautilus_canvas_container_set_label_position            (NautilusCanvasContainer  *container,
									   NautilusCanvasLabelPosition pos);
void              nautilus_canvas_container_sort                          (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_freeze_icon_positions         (NautilusCanvasContainer  *container);

int               nautilus_canvas_container_get_max_layout_lines           (NautilusCanvasContainer  *container);
int               nautilus_canvas_container_get_max_layout_lines_for_pango (NautilusCanvasContainer  *container);

void              nautilus_canvas_container_set_highlighted_for_clipboard (NautilusCanvasContainer  *container,
									   GList                  *clipboard_canvas_data);

/* operations on all icons */
void              nautilus_canvas_container_unselect_all                  (NautilusCanvasContainer  *view);
void              nautilus_canvas_container_select_all                    (NautilusCanvasContainer  *view);


void              nautilus_canvas_container_select_first                  (NautilusCanvasContainer  *view);


/* operations on the selection */
GList     *       nautilus_canvas_container_get_selection                 (NautilusCanvasContainer  *view);
void			  nautilus_canvas_container_invert_selection				(NautilusCanvasContainer  *view);
void              nautilus_canvas_container_set_selection                 (NautilusCanvasContainer  *view,
									   GList                  *selection);
GArray    *       nautilus_canvas_container_get_selected_icon_locations   (NautilusCanvasContainer  *view);
gboolean          nautilus_canvas_container_has_stretch_handles           (NautilusCanvasContainer  *container);
gboolean          nautilus_canvas_container_is_stretched                  (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_show_stretch_handles          (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_unstretch                     (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_start_renaming_selected_item  (NautilusCanvasContainer  *container,
									   gboolean                select_all);

/* options */
NautilusZoomLevel nautilus_canvas_container_get_zoom_level                (NautilusCanvasContainer  *view);
void              nautilus_canvas_container_set_zoom_level                (NautilusCanvasContainer  *view,
									   int                     new_zoom_level);
void              nautilus_canvas_container_set_single_click_mode         (NautilusCanvasContainer  *container,
									   gboolean                single_click_mode);
void              nautilus_canvas_container_enable_linger_selection       (NautilusCanvasContainer  *view,
									   gboolean                enable);
gboolean          nautilus_canvas_container_get_is_fixed_size             (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_set_is_fixed_size             (NautilusCanvasContainer  *container,
									   gboolean                is_fixed_size);
gboolean          nautilus_canvas_container_get_is_desktop                (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_set_is_desktop                (NautilusCanvasContainer  *container,
									   gboolean                is_desktop);
void              nautilus_canvas_container_reset_scroll_region           (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_set_font                      (NautilusCanvasContainer  *container,
									   const char             *font); 
void              nautilus_canvas_container_set_margins                   (NautilusCanvasContainer  *container,
									   int                     left_margin,
									   int                     right_margin,
									   int                     top_margin,
									   int                     bottom_margin);
void              nautilus_canvas_container_set_use_drop_shadows          (NautilusCanvasContainer  *container,
									   gboolean                use_drop_shadows);
char*             nautilus_canvas_container_get_icon_description          (NautilusCanvasContainer  *container,
									     NautilusCanvasIconData       *data);
gboolean          nautilus_canvas_container_get_allow_moves               (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_set_allow_moves               (NautilusCanvasContainer  *container,
									   gboolean                allow_moves);

gboolean	  nautilus_canvas_container_is_layout_rtl			(NautilusCanvasContainer  *container);
gboolean	  nautilus_canvas_container_is_layout_vertical		(NautilusCanvasContainer  *container);

gboolean          nautilus_canvas_container_get_store_layout_timestamps   (NautilusCanvasContainer  *container);
void              nautilus_canvas_container_set_store_layout_timestamps   (NautilusCanvasContainer  *container,
									   gboolean                store_layout);

void              nautilus_canvas_container_widget_to_file_operation_position (NautilusCanvasContainer *container,
									       GdkPoint              *position);

#define CANVAS_WIDTH(container,allocation) ((allocation.width		\
					     - container->details->left_margin \
					     - container->details->right_margin) \
					    /  EEL_CANVAS (container)->pixels_per_unit)

#define CANVAS_HEIGHT(container,allocation) ((allocation.height		\
					      - container->details->top_margin \
					      - container->details->bottom_margin) \
					     / EEL_CANVAS (container)->pixels_per_unit)

#endif /* NAUTILUS_CANVAS_CONTAINER_H */

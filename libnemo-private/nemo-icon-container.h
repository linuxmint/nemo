/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* gnome-icon-container.h - Icon container widget.

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

#ifndef NEMO_ICON_CONTAINER_H
#define NEMO_ICON_CONTAINER_H

#include <eel/eel-canvas.h>
#include <libnemo-private/nemo-icon-info.h>

#define NEMO_TYPE_ICON_CONTAINER nemo_icon_container_get_type()
#define NEMO_ICON_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_ICON_CONTAINER, NemoIconContainer))
#define NEMO_ICON_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_ICON_CONTAINER, NemoIconContainerClass))
#define NEMO_IS_ICON_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_ICON_CONTAINER))
#define NEMO_IS_ICON_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_ICON_CONTAINER))
#define NEMO_ICON_CONTAINER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_ICON_CONTAINER, NemoIconContainerClass))


#define NEMO_ICON_CONTAINER_ICON_DATA(pointer) \
	((NemoIconData *) (pointer))

typedef struct NemoIconData NemoIconData;

typedef void (* NemoIconCallback) (NemoIconData *icon_data,
				       gpointer callback_data);

typedef struct {
	int x;
	int y;
	double scale;
} NemoIconPosition;

typedef enum {
	NEMO_ICON_LAYOUT_L_R_T_B,
	NEMO_ICON_LAYOUT_R_L_T_B,
	NEMO_ICON_LAYOUT_T_B_L_R,
	NEMO_ICON_LAYOUT_T_B_R_L
} NemoIconLayoutMode;

typedef enum {
	NEMO_ICON_LABEL_POSITION_UNDER,
	NEMO_ICON_LABEL_POSITION_BESIDE
} NemoIconLabelPosition;

#define	NEMO_ICON_CONTAINER_TYPESELECT_FLUSH_DELAY 1000000

typedef struct NemoIconContainerDetails NemoIconContainerDetails;

typedef struct {
	EelCanvas canvas;
	NemoIconContainerDetails *details;
} NemoIconContainer;

typedef struct {
	EelCanvasClass parent_slot;

	/* Operations on the container. */
	int          (* button_press) 	          (NemoIconContainer *container,
						   GdkEventButton *event);
	void         (* context_click_background) (NemoIconContainer *container,
						   GdkEventButton *event);
	void         (* middle_click) 		  (NemoIconContainer *container,
						   GdkEventButton *event);

	/* Operations on icons. */
	void         (* activate)	  	  (NemoIconContainer *container,
						   NemoIconData *data);
	void         (* activate_alternate)       (NemoIconContainer *container,
						   NemoIconData *data);
	void         (* activate_previewer)       (NemoIconContainer *container,
						   GList *files,
						   GArray *locations);
	void         (* context_click_selection)  (NemoIconContainer *container,
						   GdkEventButton *event);
	void	     (* move_copy_items)	  (NemoIconContainer *container,
						   const GList *item_uris,
						   GdkPoint *relative_item_points,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_netscape_url)	  (NemoIconContainer *container,
						   const char *url,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_uri_list)    	  (NemoIconContainer *container,
						   const char *uri_list,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_text)		  (NemoIconContainer *container,
						   const char *text,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_raw)		  (NemoIconContainer *container,
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
	char *	     (* get_container_uri)	  (NemoIconContainer *container);

	/* Queries on icons for subclass/client.
	 * These must be implemented. The default "do nothing" is not
	 * good enough, these are _not_ signals.
	 */
	NemoIconInfo *(* get_icon_images)     (NemoIconContainer *container,
						   NemoIconData *data,
						   int icon_size,
						   char **embedded_text,
						   gboolean for_drag_accept,
						   gboolean need_large_embeddded_text,
						   gboolean *embedded_text_needs_loading,
						   gboolean *has_window_open);
	void         (* get_icon_text)            (NemoIconContainer *container,
						   NemoIconData *data,
						   char **editable_text,
						   char **additional_text,
						   gboolean include_invisible);
	char *       (* get_icon_description)     (NemoIconContainer *container,
						   NemoIconData *data);
	int          (* compare_icons)            (NemoIconContainer *container,
						   NemoIconData *icon_a,
						   NemoIconData *icon_b);
	int          (* compare_icons_by_name)    (NemoIconContainer *container,
						   NemoIconData *icon_a,
						   NemoIconData *icon_b);
	void         (* freeze_updates)           (NemoIconContainer *container);
	void         (* unfreeze_updates)         (NemoIconContainer *container);
	void         (* start_monitor_top_left)   (NemoIconContainer *container,
						   NemoIconData *data,
						   gconstpointer client,
						   gboolean large_text);
	void         (* stop_monitor_top_left)    (NemoIconContainer *container,
						   NemoIconData *data,
						   gconstpointer client);
	void         (* prioritize_thumbnailing)  (NemoIconContainer *container,
						   NemoIconData *data);

	/* Queries on icons for subclass/client.
	 * These must be implemented => These are signals !
	 * The default "do nothing" is not good enough.
	 */
	gboolean     (* can_accept_item)	  (NemoIconContainer *container,
						   NemoIconData *target, 
						   const char *item_uri);
	gboolean     (* get_stored_icon_position) (NemoIconContainer *container,
						   NemoIconData *data,
						   NemoIconPosition *position);
	char *       (* get_icon_uri)             (NemoIconContainer *container,
						   NemoIconData *data);
	char *       (* get_icon_drop_target_uri) (NemoIconContainer *container,
						   NemoIconData *data);

	/* If icon data is NULL, the layout timestamp of the container should be retrieved.
	 * That is the time when the container displayed a fully loaded directory with
	 * all icon positions assigned.
	 *
	 * If icon data is not NULL, the position timestamp of the icon should be retrieved.
	 * That is the time when the file (i.e. icon data payload) was last displayed in a
	 * fully loaded directory with all icon positions assigned.
	 */
	gboolean     (* get_stored_layout_timestamp) (NemoIconContainer *container,
						      NemoIconData *data,
						      time_t *time);
	/* If icon data is NULL, the layout timestamp of the container should be stored.
	 * If icon data is not NULL, the position timestamp of the container should be stored.
	 */
	gboolean     (* store_layout_timestamp) (NemoIconContainer *container,
						 NemoIconData *data,
						 const time_t *time);

	/* Notifications for the whole container. */
	void	     (* band_select_started)	  (NemoIconContainer *container);
	void	     (* band_select_ended)	  (NemoIconContainer *container);
	void         (* selection_changed) 	  (NemoIconContainer *container);
	void         (* layout_changed)           (NemoIconContainer *container);

	/* Notifications for icons. */
	void         (* icon_position_changed)    (NemoIconContainer *container,
						   NemoIconData *data,
						   const NemoIconPosition *position);
	void         (* icon_rename_started)      (NemoIconContainer *container,
						   GtkWidget *renaming_widget);
	void         (* icon_rename_ended)        (NemoIconContainer *container,
						   NemoIconData *data,
						   const char *text);
	void	     (* icon_stretch_started)     (NemoIconContainer *container,
						   NemoIconData *data);
	void	     (* icon_stretch_ended)       (NemoIconContainer *container,
						   NemoIconData *data);
	int	     (* preview)		  (NemoIconContainer *container,
						   NemoIconData *data,
						   gboolean start_flag);
        void         (* icon_added)               (NemoIconContainer *container,
                                                   NemoIconData *data);
        void         (* icon_removed)             (NemoIconContainer *container,
                                                   NemoIconData *data);
        void         (* cleared)                  (NemoIconContainer *container);
	gboolean     (* start_interactive_search) (NemoIconContainer *container);
} NemoIconContainerClass;

/* GtkObject */
GType             nemo_icon_container_get_type                      (void);
GtkWidget *       nemo_icon_container_new                           (void);


/* adding, removing, and managing icons */
void              nemo_icon_container_clear                         (NemoIconContainer  *view);
gboolean          nemo_icon_container_add                           (NemoIconContainer  *view,
									 NemoIconData       *data);
void              nemo_icon_container_layout_now                    (NemoIconContainer *container);
gboolean          nemo_icon_container_remove                        (NemoIconContainer  *view,
									 NemoIconData       *data);
void              nemo_icon_container_for_each                      (NemoIconContainer  *view,
									 NemoIconCallback    callback,
									 gpointer                callback_data);
void              nemo_icon_container_request_update                (NemoIconContainer  *view,
									 NemoIconData       *data);
void              nemo_icon_container_request_update_all            (NemoIconContainer  *container);
void              nemo_icon_container_reveal                        (NemoIconContainer  *container,
									 NemoIconData       *data);
gboolean          nemo_icon_container_is_empty                      (NemoIconContainer  *container);
NemoIconData *nemo_icon_container_get_first_visible_icon        (NemoIconContainer  *container);
void              nemo_icon_container_scroll_to_icon                (NemoIconContainer  *container,
									 NemoIconData       *data);

void              nemo_icon_container_begin_loading                 (NemoIconContainer  *container);
void              nemo_icon_container_end_loading                   (NemoIconContainer  *container,
									 gboolean                all_icons_added);

/* control the layout */
gboolean          nemo_icon_container_is_auto_layout                (NemoIconContainer  *container);
void              nemo_icon_container_set_auto_layout               (NemoIconContainer  *container,
									 gboolean                auto_layout);

gboolean          nemo_icon_container_is_tighter_layout             (NemoIconContainer  *container);
void              nemo_icon_container_set_tighter_layout            (NemoIconContainer  *container,
                                     gboolean                tighter_layout);
 



gboolean          nemo_icon_container_is_keep_aligned               (NemoIconContainer  *container);
void              nemo_icon_container_set_keep_aligned              (NemoIconContainer  *container,
									 gboolean                keep_aligned);
void              nemo_icon_container_set_layout_mode               (NemoIconContainer  *container,
									 NemoIconLayoutMode  mode);
void              nemo_icon_container_set_label_position            (NemoIconContainer  *container,
									 NemoIconLabelPosition pos);
void              nemo_icon_container_sort                          (NemoIconContainer  *container);
void              nemo_icon_container_freeze_icon_positions         (NemoIconContainer  *container);

int               nemo_icon_container_get_max_layout_lines           (NemoIconContainer  *container);
int               nemo_icon_container_get_max_layout_lines_for_pango (NemoIconContainer  *container);

void              nemo_icon_container_set_highlighted_for_clipboard (NemoIconContainer  *container,
									 GList                  *clipboard_icon_data);

/* operations on all icons */
void              nemo_icon_container_unselect_all                  (NemoIconContainer  *view);
void              nemo_icon_container_select_all                    (NemoIconContainer  *view);


/* operations on the selection */
GList     *       nemo_icon_container_get_selection                 (NemoIconContainer  *view);
void			  nemo_icon_container_invert_selection				(NemoIconContainer  *view);
void              nemo_icon_container_set_selection                 (NemoIconContainer  *view,
									 GList                  *selection);
GArray    *       nemo_icon_container_get_selected_icon_locations   (NemoIconContainer  *view);
gboolean          nemo_icon_container_has_stretch_handles           (NemoIconContainer  *container);
gboolean          nemo_icon_container_is_stretched                  (NemoIconContainer  *container);
void              nemo_icon_container_show_stretch_handles          (NemoIconContainer  *container);
void              nemo_icon_container_unstretch                     (NemoIconContainer  *container);
void              nemo_icon_container_start_renaming_selected_item  (NemoIconContainer  *container,
									 gboolean                select_all);

/* options */
NemoZoomLevel nemo_icon_container_get_zoom_level                (NemoIconContainer  *view);
void              nemo_icon_container_set_zoom_level                (NemoIconContainer  *view,
									 int                     new_zoom_level);
void              nemo_icon_container_set_single_click_mode         (NemoIconContainer  *container,
									 gboolean                single_click_mode);
void              nemo_icon_container_enable_linger_selection       (NemoIconContainer  *view,
									 gboolean                enable);
gboolean          nemo_icon_container_get_is_fixed_size             (NemoIconContainer  *container);
void              nemo_icon_container_set_is_fixed_size             (NemoIconContainer  *container,
									 gboolean                is_fixed_size);
gboolean          nemo_icon_container_get_is_desktop                (NemoIconContainer  *container);
void              nemo_icon_container_set_is_desktop                (NemoIconContainer  *container,
									 gboolean                is_desktop);
gboolean          nemo_icon_container_get_show_desktop_tooltips     (NemoIconContainer *container);
void              nemo_icon_container_set_show_desktop_tooltips     (NemoIconContainer *container,
                                                                              gboolean  show_tooltips);
void              nemo_icon_container_reset_scroll_region           (NemoIconContainer  *container);
void              nemo_icon_container_set_font                      (NemoIconContainer  *container,
									 const char             *font); 
void              nemo_icon_container_set_margins                   (NemoIconContainer  *container,
									 int                     left_margin,
									 int                     right_margin,
									 int                     top_margin,
									 int                     bottom_margin);
void              nemo_icon_container_set_use_drop_shadows          (NemoIconContainer  *container,
									 gboolean                use_drop_shadows);
char*             nemo_icon_container_get_icon_description          (NemoIconContainer  *container,
                                                                         NemoIconData       *data);
gboolean          nemo_icon_container_get_allow_moves               (NemoIconContainer  *container);
void              nemo_icon_container_set_allow_moves               (NemoIconContainer  *container,
									 gboolean                allow_moves);
void		  nemo_icon_container_set_forced_icon_size		(NemoIconContainer  *container,
									 int                     forced_icon_size);
void		  nemo_icon_container_set_all_columns_same_width	(NemoIconContainer  *container,
									 gboolean                all_columns_same_width);

gboolean	  nemo_icon_container_is_layout_rtl			(NemoIconContainer  *container);
gboolean	  nemo_icon_container_is_layout_vertical		(NemoIconContainer  *container);

gboolean          nemo_icon_container_get_store_layout_timestamps   (NemoIconContainer  *container);
void              nemo_icon_container_set_store_layout_timestamps   (NemoIconContainer  *container,
									 gboolean                store_layout);

void              nemo_icon_container_widget_to_file_operation_position (NemoIconContainer *container,
									     GdkPoint              *position);

void         nemo_icon_container_setup_tooltip_preference_callback (NemoIconContainer *container);

#define CANVAS_WIDTH(container,allocation) ((allocation.width	  \
				- container->details->left_margin \
				- container->details->right_margin) \
				/  EEL_CANVAS (container)->pixels_per_unit)

#define CANVAS_HEIGHT(container,allocation) ((allocation.height \
			 - container->details->top_margin \
			 - container->details->bottom_margin) \
			 / EEL_CANVAS (container)->pixels_per_unit)

#endif /* NEMO_ICON_CONTAINER_H */

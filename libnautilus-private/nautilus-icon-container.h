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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ettore Perazzoli <ettore@gnu.org>, Darin Adler <darin@bentspoon.com>
*/

#ifndef NAUTILUS_ICON_CONTAINER_H
#define NAUTILUS_ICON_CONTAINER_H

#include <eel/eel-canvas.h>
#include <libnautilus-private/nautilus-icon-info.h>

#define NAUTILUS_TYPE_ICON_CONTAINER nautilus_icon_container_get_type()
#define NAUTILUS_ICON_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_ICON_CONTAINER, NautilusIconContainer))
#define NAUTILUS_ICON_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ICON_CONTAINER, NautilusIconContainerClass))
#define NAUTILUS_IS_ICON_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_ICON_CONTAINER))
#define NAUTILUS_IS_ICON_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ICON_CONTAINER))
#define NAUTILUS_ICON_CONTAINER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_ICON_CONTAINER, NautilusIconContainerClass))


#define NAUTILUS_ICON_CONTAINER_ICON_DATA(pointer) \
	((NautilusIconData *) (pointer))

typedef struct NautilusIconData NautilusIconData;

typedef void (* NautilusIconCallback) (NautilusIconData *icon_data,
				       gpointer callback_data);

typedef struct {
	int x;
	int y;
	double scale;
} NautilusIconPosition;

typedef enum {
	NAUTILUS_ICON_LAYOUT_L_R_T_B,
	NAUTILUS_ICON_LAYOUT_R_L_T_B,
	NAUTILUS_ICON_LAYOUT_T_B_L_R,
	NAUTILUS_ICON_LAYOUT_T_B_R_L
} NautilusIconLayoutMode;

typedef enum {
	NAUTILUS_ICON_LABEL_POSITION_UNDER,
	NAUTILUS_ICON_LABEL_POSITION_BESIDE
} NautilusIconLabelPosition;

#define	NAUTILUS_ICON_CONTAINER_TYPESELECT_FLUSH_DELAY 1000000

typedef struct NautilusIconContainerDetails NautilusIconContainerDetails;

typedef struct {
	EelCanvas canvas;
	NautilusIconContainerDetails *details;
} NautilusIconContainer;

typedef struct {
	EelCanvasClass parent_slot;

	/* Operations on the container. */
	int          (* button_press) 	          (NautilusIconContainer *container,
						   GdkEventButton *event);
	void         (* context_click_background) (NautilusIconContainer *container,
						   GdkEventButton *event);
	void         (* middle_click) 		  (NautilusIconContainer *container,
						   GdkEventButton *event);

	/* Operations on icons. */
	void         (* activate)	  	  (NautilusIconContainer *container,
						   NautilusIconData *data);
	void         (* activate_alternate)       (NautilusIconContainer *container,
						   NautilusIconData *data);
	void         (* activate_previewer)       (NautilusIconContainer *container,
						   GList *files,
						   GArray *locations);
	void         (* context_click_selection)  (NautilusIconContainer *container,
						   GdkEventButton *event);
	void	     (* move_copy_items)	  (NautilusIconContainer *container,
						   const GList *item_uris,
						   GdkPoint *relative_item_points,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_netscape_url)	  (NautilusIconContainer *container,
						   const char *url,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_uri_list)    	  (NautilusIconContainer *container,
						   const char *uri_list,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_text)		  (NautilusIconContainer *container,
						   const char *text,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_raw)		  (NautilusIconContainer *container,
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
	char *	     (* get_container_uri)	  (NautilusIconContainer *container);

	/* Queries on icons for subclass/client.
	 * These must be implemented. The default "do nothing" is not
	 * good enough, these are _not_ signals.
	 */
	NautilusIconInfo *(* get_icon_images)     (NautilusIconContainer *container,
						   NautilusIconData *data,
						   int icon_size,
						   char **embedded_text,
						   gboolean for_drag_accept,
						   gboolean need_large_embeddded_text,
						   gboolean *embedded_text_needs_loading,
						   gboolean *has_window_open);
	void         (* get_icon_text)            (NautilusIconContainer *container,
						   NautilusIconData *data,
						   char **editable_text,
						   char **additional_text,
						   gboolean include_invisible);
	char *       (* get_icon_description)     (NautilusIconContainer *container,
						   NautilusIconData *data);
	int          (* compare_icons)            (NautilusIconContainer *container,
						   NautilusIconData *icon_a,
						   NautilusIconData *icon_b);
	int          (* compare_icons_by_name)    (NautilusIconContainer *container,
						   NautilusIconData *icon_a,
						   NautilusIconData *icon_b);
	void         (* freeze_updates)           (NautilusIconContainer *container);
	void         (* unfreeze_updates)         (NautilusIconContainer *container);
	void         (* start_monitor_top_left)   (NautilusIconContainer *container,
						   NautilusIconData *data,
						   gconstpointer client,
						   gboolean large_text);
	void         (* stop_monitor_top_left)    (NautilusIconContainer *container,
						   NautilusIconData *data,
						   gconstpointer client);
	void         (* prioritize_thumbnailing)  (NautilusIconContainer *container,
						   NautilusIconData *data);

	/* Queries on icons for subclass/client.
	 * These must be implemented => These are signals !
	 * The default "do nothing" is not good enough.
	 */
	gboolean     (* can_accept_item)	  (NautilusIconContainer *container,
						   NautilusIconData *target, 
						   const char *item_uri);
	gboolean     (* get_stored_icon_position) (NautilusIconContainer *container,
						   NautilusIconData *data,
						   NautilusIconPosition *position);
	char *       (* get_icon_uri)             (NautilusIconContainer *container,
						   NautilusIconData *data);
	char *       (* get_icon_drop_target_uri) (NautilusIconContainer *container,
						   NautilusIconData *data);

	/* If icon data is NULL, the layout timestamp of the container should be retrieved.
	 * That is the time when the container displayed a fully loaded directory with
	 * all icon positions assigned.
	 *
	 * If icon data is not NULL, the position timestamp of the icon should be retrieved.
	 * That is the time when the file (i.e. icon data payload) was last displayed in a
	 * fully loaded directory with all icon positions assigned.
	 */
	gboolean     (* get_stored_layout_timestamp) (NautilusIconContainer *container,
						      NautilusIconData *data,
						      time_t *time);
	/* If icon data is NULL, the layout timestamp of the container should be stored.
	 * If icon data is not NULL, the position timestamp of the container should be stored.
	 */
	gboolean     (* store_layout_timestamp) (NautilusIconContainer *container,
						 NautilusIconData *data,
						 const time_t *time);

	/* Notifications for the whole container. */
	void	     (* band_select_started)	  (NautilusIconContainer *container);
	void	     (* band_select_ended)	  (NautilusIconContainer *container);
	void         (* selection_changed) 	  (NautilusIconContainer *container);
	void         (* layout_changed)           (NautilusIconContainer *container);

	/* Notifications for icons. */
	void         (* icon_position_changed)    (NautilusIconContainer *container,
						   NautilusIconData *data,
						   const NautilusIconPosition *position);
	void         (* icon_rename_started)      (NautilusIconContainer *container,
						   GtkWidget *renaming_widget);
	void         (* icon_rename_ended)        (NautilusIconContainer *container,
						   NautilusIconData *data,
						   const char *text);
	void	     (* icon_stretch_started)     (NautilusIconContainer *container,
						   NautilusIconData *data);
	void	     (* icon_stretch_ended)       (NautilusIconContainer *container,
						   NautilusIconData *data);
	int	     (* preview)		  (NautilusIconContainer *container,
						   NautilusIconData *data,
						   gboolean start_flag);
        void         (* icon_added)               (NautilusIconContainer *container,
                                                   NautilusIconData *data);
        void         (* icon_removed)             (NautilusIconContainer *container,
                                                   NautilusIconData *data);
        void         (* cleared)                  (NautilusIconContainer *container);
	gboolean     (* start_interactive_search) (NautilusIconContainer *container);
} NautilusIconContainerClass;

/* GtkObject */
GType             nautilus_icon_container_get_type                      (void);
GtkWidget *       nautilus_icon_container_new                           (void);


/* adding, removing, and managing icons */
void              nautilus_icon_container_clear                         (NautilusIconContainer  *view);
gboolean          nautilus_icon_container_add                           (NautilusIconContainer  *view,
									 NautilusIconData       *data);
void              nautilus_icon_container_layout_now                    (NautilusIconContainer *container);
gboolean          nautilus_icon_container_remove                        (NautilusIconContainer  *view,
									 NautilusIconData       *data);
void              nautilus_icon_container_for_each                      (NautilusIconContainer  *view,
									 NautilusIconCallback    callback,
									 gpointer                callback_data);
void              nautilus_icon_container_request_update                (NautilusIconContainer  *view,
									 NautilusIconData       *data);
void              nautilus_icon_container_request_update_all            (NautilusIconContainer  *container);
void              nautilus_icon_container_reveal                        (NautilusIconContainer  *container,
									 NautilusIconData       *data);
gboolean          nautilus_icon_container_is_empty                      (NautilusIconContainer  *container);
NautilusIconData *nautilus_icon_container_get_first_visible_icon        (NautilusIconContainer  *container);
void              nautilus_icon_container_scroll_to_icon                (NautilusIconContainer  *container,
									 NautilusIconData       *data);

void              nautilus_icon_container_begin_loading                 (NautilusIconContainer  *container);
void              nautilus_icon_container_end_loading                   (NautilusIconContainer  *container,
									 gboolean                all_icons_added);

/* control the layout */
gboolean          nautilus_icon_container_is_auto_layout                (NautilusIconContainer  *container);
void              nautilus_icon_container_set_auto_layout               (NautilusIconContainer  *container,
									 gboolean                auto_layout);

gboolean          nautilus_icon_container_is_keep_aligned               (NautilusIconContainer  *container);
void              nautilus_icon_container_set_keep_aligned              (NautilusIconContainer  *container,
									 gboolean                keep_aligned);
void              nautilus_icon_container_set_layout_mode               (NautilusIconContainer  *container,
									 NautilusIconLayoutMode  mode);
void              nautilus_icon_container_set_label_position            (NautilusIconContainer  *container,
									 NautilusIconLabelPosition pos);
void              nautilus_icon_container_sort                          (NautilusIconContainer  *container);
void              nautilus_icon_container_freeze_icon_positions         (NautilusIconContainer  *container);

int               nautilus_icon_container_get_max_layout_lines           (NautilusIconContainer  *container);
int               nautilus_icon_container_get_max_layout_lines_for_pango (NautilusIconContainer  *container);

void              nautilus_icon_container_set_highlighted_for_clipboard (NautilusIconContainer  *container,
									 GList                  *clipboard_icon_data);

/* operations on all icons */
void              nautilus_icon_container_unselect_all                  (NautilusIconContainer  *view);
void              nautilus_icon_container_select_all                    (NautilusIconContainer  *view);


/* operations on the selection */
GList     *       nautilus_icon_container_get_selection                 (NautilusIconContainer  *view);
void			  nautilus_icon_container_invert_selection				(NautilusIconContainer  *view);
void              nautilus_icon_container_set_selection                 (NautilusIconContainer  *view,
									 GList                  *selection);
GArray    *       nautilus_icon_container_get_selected_icon_locations   (NautilusIconContainer  *view);
gboolean          nautilus_icon_container_has_stretch_handles           (NautilusIconContainer  *container);
gboolean          nautilus_icon_container_is_stretched                  (NautilusIconContainer  *container);
void              nautilus_icon_container_show_stretch_handles          (NautilusIconContainer  *container);
void              nautilus_icon_container_unstretch                     (NautilusIconContainer  *container);
void              nautilus_icon_container_start_renaming_selected_item  (NautilusIconContainer  *container,
									 gboolean                select_all);

/* options */
NautilusZoomLevel nautilus_icon_container_get_zoom_level                (NautilusIconContainer  *view);
void              nautilus_icon_container_set_zoom_level                (NautilusIconContainer  *view,
									 int                     new_zoom_level);
void              nautilus_icon_container_set_single_click_mode         (NautilusIconContainer  *container,
									 gboolean                single_click_mode);
void              nautilus_icon_container_enable_linger_selection       (NautilusIconContainer  *view,
									 gboolean                enable);
gboolean          nautilus_icon_container_get_is_fixed_size             (NautilusIconContainer  *container);
void              nautilus_icon_container_set_is_fixed_size             (NautilusIconContainer  *container,
									 gboolean                is_fixed_size);
gboolean          nautilus_icon_container_get_is_desktop                (NautilusIconContainer  *container);
void              nautilus_icon_container_set_is_desktop                (NautilusIconContainer  *container,
									 gboolean                is_desktop);
void              nautilus_icon_container_reset_scroll_region           (NautilusIconContainer  *container);
void              nautilus_icon_container_set_font                      (NautilusIconContainer  *container,
									 const char             *font); 
void              nautilus_icon_container_set_font_size_table           (NautilusIconContainer  *container,
									 const int               font_size_table[NAUTILUS_ZOOM_LEVEL_LARGEST + 1]);
void              nautilus_icon_container_set_margins                   (NautilusIconContainer  *container,
									 int                     left_margin,
									 int                     right_margin,
									 int                     top_margin,
									 int                     bottom_margin);
void              nautilus_icon_container_set_use_drop_shadows          (NautilusIconContainer  *container,
									 gboolean                use_drop_shadows);
char*             nautilus_icon_container_get_icon_description          (NautilusIconContainer  *container,
                                                                         NautilusIconData       *data);
gboolean          nautilus_icon_container_get_allow_moves               (NautilusIconContainer  *container);
void              nautilus_icon_container_set_allow_moves               (NautilusIconContainer  *container,
									 gboolean                allow_moves);
void		  nautilus_icon_container_set_forced_icon_size		(NautilusIconContainer  *container,
									 int                     forced_icon_size);
void		  nautilus_icon_container_set_all_columns_same_width	(NautilusIconContainer  *container,
									 gboolean                all_columns_same_width);

gboolean	  nautilus_icon_container_is_layout_rtl			(NautilusIconContainer  *container);
gboolean	  nautilus_icon_container_is_layout_vertical		(NautilusIconContainer  *container);

gboolean          nautilus_icon_container_get_store_layout_timestamps   (NautilusIconContainer  *container);
void              nautilus_icon_container_set_store_layout_timestamps   (NautilusIconContainer  *container,
									 gboolean                store_layout);

void              nautilus_icon_container_widget_to_file_operation_position (NautilusIconContainer *container,
									     GdkPoint              *position);

#define CANVAS_WIDTH(container,allocation) ((allocation.width	  \
				- container->details->left_margin \
				- container->details->right_margin) \
				/  EEL_CANVAS (container)->pixels_per_unit)

#define CANVAS_HEIGHT(container,allocation) ((allocation.height \
			 - container->details->top_margin \
			 - container->details->bottom_margin) \
			 / EEL_CANVAS (container)->pixels_per_unit)

#endif /* NAUTILUS_ICON_CONTAINER_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nemo - Icon canvas item class for icon container.
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef NEMO_ICON_CANVAS_ITEM_H
#define NEMO_ICON_CANVAS_ITEM_H

#include <eel/eel-canvas.h>
#include <eel/eel-art-extensions.h>

G_BEGIN_DECLS

#define NEMO_TYPE_ICON_CANVAS_ITEM nemo_icon_canvas_item_get_type()
#define NEMO_ICON_CANVAS_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_ICON_CANVAS_ITEM, NemoIconCanvasItem))
#define NEMO_ICON_CANVAS_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_ICON_CANVAS_ITEM, NemoIconCanvasItemClass))
#define NEMO_IS_ICON_CANVAS_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_ICON_CANVAS_ITEM))
#define NEMO_IS_ICON_CANVAS_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_ICON_CANVAS_ITEM))
#define NEMO_ICON_CANVAS_ITEM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_ICON_CANVAS_ITEM, NemoIconCanvasItemClass))

typedef struct NemoIconCanvasItem NemoIconCanvasItem;
typedef struct NemoIconCanvasItemClass NemoIconCanvasItemClass;
typedef struct NemoIconCanvasItemDetails NemoIconCanvasItemDetails;

struct NemoIconCanvasItem {
	EelCanvasItem item;
	NemoIconCanvasItemDetails *details;
	gpointer user_data;
};

struct NemoIconCanvasItemClass {
	EelCanvasItemClass parent_class;
};

/* not namespaced due to their length */
typedef enum {
	BOUNDS_USAGE_FOR_LAYOUT,
	BOUNDS_USAGE_FOR_ENTIRE_ITEM,
	BOUNDS_USAGE_FOR_DISPLAY
} NemoIconCanvasItemBoundsUsage;

/* GObject */
GType       nemo_icon_canvas_item_get_type                 (void);

/* attributes */
void        nemo_icon_canvas_item_set_image                (NemoIconCanvasItem       *item,
								GdkPixbuf                    *image);
cairo_surface_t* nemo_icon_canvas_item_get_drag_surface    (NemoIconCanvasItem       *item);
void        nemo_icon_canvas_item_set_emblems              (NemoIconCanvasItem       *item,
								GList                        *emblem_pixbufs);
void        nemo_icon_canvas_item_set_show_stretch_handles (NemoIconCanvasItem       *item,
								gboolean                      show_stretch_handles);
void        nemo_icon_canvas_item_set_attach_points        (NemoIconCanvasItem       *item,
								GdkPoint                     *attach_points,
								int                           n_attach_points);
void        nemo_icon_canvas_item_set_embedded_text_rect   (NemoIconCanvasItem       *item,
								const GdkRectangle           *text_rect);
void        nemo_icon_canvas_item_set_embedded_text        (NemoIconCanvasItem       *item,
								const char                   *text);
double      nemo_icon_canvas_item_get_max_text_width       (NemoIconCanvasItem       *item);
const char *nemo_icon_canvas_item_get_editable_text        (NemoIconCanvasItem       *icon_item);
void        nemo_icon_canvas_item_set_renaming             (NemoIconCanvasItem       *icon_item,
								gboolean                      state);

/* geometry and hit testing */
gboolean    nemo_icon_canvas_item_hit_test_rectangle       (NemoIconCanvasItem       *item,
								EelIRect                      canvas_rect);
gboolean    nemo_icon_canvas_item_hit_test_stretch_handles (NemoIconCanvasItem       *item,
								gdouble                       world_x,
								gdouble                       world_y,
								GtkCornerType                *corner);
void        nemo_icon_canvas_item_invalidate_label         (NemoIconCanvasItem       *item);
void        nemo_icon_canvas_item_invalidate_label_size    (NemoIconCanvasItem       *item);
EelDRect    nemo_icon_canvas_item_get_icon_rectangle       (const NemoIconCanvasItem *item);
EelDRect    nemo_icon_canvas_item_get_text_rectangle       (NemoIconCanvasItem       *item,
								gboolean                      for_layout);
void        nemo_icon_canvas_item_get_bounds_for_layout    (NemoIconCanvasItem       *item,
								double *x1, double *y1, double *x2, double *y2);
void        nemo_icon_canvas_item_get_bounds_for_entire_item (NemoIconCanvasItem       *item,
								  double *x1, double *y1, double *x2, double *y2);
void        nemo_icon_canvas_item_update_bounds            (NemoIconCanvasItem       *item,
								double i2w_dx, double i2w_dy);
void        nemo_icon_canvas_item_set_is_visible           (NemoIconCanvasItem       *item,
								gboolean                      visible);
/* whether the entire label text must be visible at all times */
void        nemo_icon_canvas_item_set_entire_text          (NemoIconCanvasItem       *icon_item,
								gboolean                      entire_text);

G_END_DECLS

#endif /* NEMO_ICON_CANVAS_ITEM_H */

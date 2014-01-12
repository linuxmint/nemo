/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nemo - Canvas item class for canvas container.
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
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef NEMO_CANVAS_ITEM_H
#define NEMO_CANVAS_ITEM_H

#include <eel/eel-canvas.h>
#include <eel/eel-art-extensions.h>

G_BEGIN_DECLS

#define NEMO_TYPE_CANVAS_ITEM nemo_canvas_item_get_type()
#define NEMO_CANVAS_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_CANVAS_ITEM, NemoCanvasItem))
#define NEMO_CANVAS_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_CANVAS_ITEM, NemoCanvasItemClass))
#define NEMO_IS_CANVAS_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_CANVAS_ITEM))
#define NEMO_IS_CANVAS_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_CANVAS_ITEM))
#define NEMO_CANVAS_ITEM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_CANVAS_ITEM, NemoCanvasItemClass))

typedef struct NemoCanvasItem NemoCanvasItem;
typedef struct NemoCanvasItemClass NemoCanvasItemClass;
typedef struct NemoCanvasItemDetails NemoCanvasItemDetails;

struct NemoCanvasItem {
	EelCanvasItem item;
	NemoCanvasItemDetails *details;
	gpointer user_data;
    gchar *tooltip;
};

struct NemoCanvasItemClass {
	EelCanvasItemClass parent_class;
};

/* not namespaced due to their length */
typedef enum {
	BOUNDS_USAGE_FOR_LAYOUT,
	BOUNDS_USAGE_FOR_ENTIRE_ITEM,
	BOUNDS_USAGE_FOR_DISPLAY
} NemoCanvasItemBoundsUsage;

/* GObject */
GType       nemo_canvas_item_get_type                 (void);

/* attributes */
void        nemo_canvas_item_set_image                (NemoCanvasItem       *item,
							   GdkPixbuf                *image);
cairo_surface_t* nemo_canvas_item_get_drag_surface    (NemoCanvasItem       *item);
void        nemo_canvas_item_set_emblems              (NemoCanvasItem       *item,
							   GList                    *emblem_pixbufs);
void        nemo_canvas_item_set_show_stretch_handles (NemoCanvasItem       *item,
							   gboolean                  show_stretch_handles);
void        nemo_canvas_item_set_attach_points        (NemoCanvasItem       *item,
							   GdkPoint                 *attach_points,
							   int                       n_attach_points);
void        nemo_canvas_item_set_embedded_text_rect   (NemoCanvasItem       *item,
							   const GdkRectangle       *text_rect);
void        nemo_canvas_item_set_embedded_text        (NemoCanvasItem       *item,
							   const char               *text);
double      nemo_canvas_item_get_max_text_width       (NemoCanvasItem       *item);
const char *nemo_canvas_item_get_editable_text        (NemoCanvasItem       *canvas_item);
void        nemo_canvas_item_set_renaming             (NemoCanvasItem       *canvas_item,
							   gboolean                  state);

/* geometry and hit testing */
gboolean    nemo_canvas_item_hit_test_rectangle       (NemoCanvasItem       *item,
							   EelIRect                  canvas_rect);
gboolean    nemo_canvas_item_hit_test_stretch_handles (NemoCanvasItem       *item,
							   gdouble                   world_x,
							   gdouble                   world_y,
							   GtkCornerType            *corner);
void        nemo_canvas_item_invalidate_label         (NemoCanvasItem       *item);
void        nemo_canvas_item_invalidate_label_size    (NemoCanvasItem       *item);
EelDRect    nemo_canvas_item_get_icon_rectangle     (const NemoCanvasItem *item);
EelDRect    nemo_canvas_item_get_text_rectangle       (NemoCanvasItem       *item,
							   gboolean                  for_layout);
void        nemo_canvas_item_get_bounds_for_layout    (NemoCanvasItem       *item,
							   double *x1, double *y1, double *x2, double *y2);
void        nemo_canvas_item_get_bounds_for_entire_item (NemoCanvasItem       *item,
							     double *x1, double *y1, double *x2, double *y2);
void        nemo_canvas_item_update_bounds            (NemoCanvasItem       *item,
							   double i2w_dx, double i2w_dy);
void        nemo_canvas_item_set_is_visible           (NemoCanvasItem       *item,
							   gboolean                  visible);
/* whether the entire label text must be visible at all times */
void        nemo_canvas_item_set_entire_text          (NemoCanvasItem       *canvas_item,
								gboolean                      entire_text);
void        nemo_canvas_item_set_tooltip_text         (NemoCanvasItem *item, const gchar *text);

G_END_DECLS

#endif /* NEMO_CANVAS_ITEM_H */

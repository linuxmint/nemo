/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nemo - Canvas item for floating selection.
 *
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * Copyright (C) 2011 Red Hat Inc.
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
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */

#include <config.h>

#include "nemo-selection-canvas-item.h"

#include <math.h>

enum {
	PROP_X1 = 1,
	PROP_Y1,
	PROP_X2,
	PROP_Y2,
	PROP_FILL_COLOR_RGBA,
	PROP_OUTLINE_COLOR_RGBA,
	PROP_OUTLINE_STIPPLING,
	PROP_WIDTH_PIXELS,
	NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL };

typedef struct {
  /*< public >*/
  int x0, y0, x1, y1;
}  Rect;

struct _NemoSelectionCanvasItemDetails {
	Rect last_update_rect;
	Rect last_outline_update_rect;
	int last_outline_update_width;

	double x1, y1, x2, y2;		/* Corners of item */
	double width;			/* Outline width */

        GdkRGBA fill_color;
        GdkRGBA outline_color;

	gboolean outline_stippling;

	/* Configuration flags */

	unsigned int fill_set : 1;	/* Is fill color set? */
	unsigned int outline_set : 1;	/* Is outline color set? */

	double fade_out_fill_alpha;
	double fade_out_outline_alpha;

	double fade_out_fill_delta;
	double fade_out_outline_delta;	

	guint fade_out_handler_id;
};

G_DEFINE_TYPE (NemoSelectionCanvasItem, nemo_selection_canvas_item, EEL_TYPE_CANVAS_ITEM);

#define DASH_ON 0.8
#define DASH_OFF 1.7
static void
nemo_selection_canvas_item_draw (EelCanvasItem *item,
				     cairo_t *cr,
				     cairo_region_t *region)
{
	NemoSelectionCanvasItem *self;
	double x1, y1, x2, y2;
	int cx1, cy1, cx2, cy2;
	double i2w_dx, i2w_dy;

	self = NEMO_SELECTION_CANVAS_ITEM (item);

	/* Get canvas pixel coordinates */
	i2w_dx = 0.0;
	i2w_dy = 0.0;
	eel_canvas_item_i2w (item, &i2w_dx, &i2w_dy);
	
	x1 = self->priv->x1 + i2w_dx;
	y1 = self->priv->y1 + i2w_dy;
	x2 = self->priv->x2 + i2w_dx;
	y2 = self->priv->y2 + i2w_dy;

	eel_canvas_w2c (item->canvas, x1, y1, &cx1, &cy1);
	eel_canvas_w2c (item->canvas, x2, y2, &cx2, &cy2);
	
	if (cx2 <= cx1 || cy2 <= cy1 ) {
		return;
	}

	cairo_save (cr);

	if (self->priv->fill_set) {
		GdkRGBA actual_fill;

		actual_fill = self->priv->fill_color;

		if (self->priv->fade_out_handler_id != 0) {
			actual_fill.alpha = self->priv->fade_out_fill_alpha;
		}

		gdk_cairo_set_source_rgba (cr, &actual_fill);
		cairo_rectangle (cr,
				 cx1, cy1,
				 cx2 - cx1 + 1,
				 cy2 - cy1 + 1);
		cairo_fill (cr);
	}

	if (self->priv->outline_set) {
		GdkRGBA actual_outline;

		actual_outline = self->priv->outline_color;

		if (self->priv->fade_out_handler_id != 0) {
			actual_outline.alpha = self->priv->fade_out_outline_alpha;
		}

		gdk_cairo_set_source_rgba (cr, &actual_outline);
		cairo_set_line_width (cr, (int) self->priv->width);

		if (self->priv->outline_stippling) {
			double dash[2] = { DASH_ON, DASH_OFF };

			cairo_set_dash (cr, dash, G_N_ELEMENTS (dash), 0);
		}

		cairo_rectangle (cr,
				 cx1 + 0.5, cy1 + 0.5,
				 cx2 - cx1,
				 cy2 - cy1);
		cairo_stroke (cr);
	}

	cairo_restore (cr);
}

static double
nemo_selection_canvas_item_point (EelCanvasItem *item,
				      double x,
				      double y,
				      int cx,
				      int cy,
				      EelCanvasItem **actual_item)
{
	NemoSelectionCanvasItem *self;
	double x1, y1, x2, y2;
	double hwidth;
	double dx, dy;
	double tmp;

	self = NEMO_SELECTION_CANVAS_ITEM (item);
	*actual_item = item;

	/* Find the bounds for the rectangle plus its outline width */

	x1 = self->priv->x1;
	y1 = self->priv->y1;
	x2 = self->priv->x2;
	y2 = self->priv->y2;

	if (self->priv->outline_set) {
		hwidth = (self->priv->width / item->canvas->pixels_per_unit) / 2.0;

		x1 -= hwidth;
		y1 -= hwidth;
		x2 += hwidth;
		y2 += hwidth;
	} else
		hwidth = 0.0;

	/* Is point inside rectangle (which can be hollow if it has no fill set)? */

	if ((x >= x1) && (y >= y1) && (x <= x2) && (y <= y2)) {
		if (self->priv->fill_set || !self->priv->outline_set)
			return 0.0;

		dx = x - x1;
		tmp = x2 - x;
		if (tmp < dx)
			dx = tmp;

		dy = y - y1;
		tmp = y2 - y;
		if (tmp < dy)
			dy = tmp;

		if (dy < dx)
			dx = dy;

		dx -= 2.0 * hwidth;

		if (dx < 0.0)
			return 0.0;
		else
			return dx;
	}

	/* Point is outside rectangle */

	if (x < x1)
		dx = x1 - x;
	else if (x > x2)
		dx = x - x2;
	else
		dx = 0.0;

	if (y < y1)
		dy = y1 - y;
	else if (y > y2)
		dy = y - y2;
	else
		dy = 0.0;

	return sqrt (dx * dx + dy * dy);
}

static void
request_redraw_borders (EelCanvas *canvas,
			Rect      *update_rect,
			int        width)
{
	eel_canvas_request_redraw (canvas,
				   update_rect->x0, update_rect->y0,
				   update_rect->x1, update_rect->y0 + width);
	eel_canvas_request_redraw (canvas,
				   update_rect->x0, update_rect->y1-width,
				   update_rect->x1, update_rect->y1);
	eel_canvas_request_redraw (canvas,
				   update_rect->x0,       update_rect->y0,
				   update_rect->x0+width, update_rect->y1);
	eel_canvas_request_redraw (canvas,
				   update_rect->x1-width, update_rect->y0,
				   update_rect->x1,       update_rect->y1);
}

static Rect make_rect (int x0, int y0, int x1, int y1);

static int
rect_empty (const Rect *src) {
  return (src->x1 <= src->x0 || src->y1 <= src->y0);
}

static gboolean
rects_intersect (Rect r1, Rect r2)
{
	if (r1.x0 >= r2.x1) {
		return FALSE;
	}
	if (r2.x0 >= r1.x1) {
		return FALSE;
	}
	if (r1.y0 >= r2.y1) {
		return FALSE;
	}
	if (r2.y0 >= r1.y1) {
		return FALSE;
	}
	return TRUE;
}

static void
diff_rects_guts (Rect ra, Rect rb, int *count, Rect result[4])
{
	if (ra.x0 < rb.x0) {
		result[(*count)++] = make_rect (ra.x0, ra.y0, rb.x0, ra.y1);
	}
	if (ra.y0 < rb.y0) {
		result[(*count)++] = make_rect (ra.x0, ra.y0, ra.x1, rb.y0);
	}
	if (ra.x1 < rb.x1) {
		result[(*count)++] = make_rect (ra.x1, rb.y0, rb.x1, rb.y1);
	}
	if (ra.y1 < rb.y1) {
		result[(*count)++] = make_rect (rb.x0, ra.y1, rb.x1, rb.y1);
	}
}

static void
diff_rects (Rect r1, Rect r2, int *count, Rect result[4])
{
	g_assert (count != NULL);
	g_assert (result != NULL);

	*count = 0;

	if (rects_intersect (r1, r2)) {
		diff_rects_guts (r1, r2, count, result);
		diff_rects_guts (r2, r1, count, result);
	} else {
		if (!rect_empty (&r1)) {
			result[(*count)++] = r1;
		}
		if (!rect_empty (&r2)) {
			result[(*count)++] = r2;
		}
	}
}

static Rect
make_rect (int x0, int y0, int x1, int y1)
{
	Rect r;

	r.x0 = x0;
	r.y0 = y0;
	r.x1 = x1;
	r.y1 = y1;
	return r;
}

static void
nemo_selection_canvas_item_update (EelCanvasItem *item,
				       double i2w_dx,
				       double i2w_dy,
				       gint flags)
{
	NemoSelectionCanvasItem *self;
	NemoSelectionCanvasItemDetails *priv;
	double x1, y1, x2, y2;
	int cx1, cy1, cx2, cy2;
	int repaint_rects_count, i;
	int width_pixels;
	int width_lt, width_rb;
	Rect update_rect, repaint_rects[4];

	if (EEL_CANVAS_ITEM_CLASS (nemo_selection_canvas_item_parent_class)->update)
		(* EEL_CANVAS_ITEM_CLASS (nemo_selection_canvas_item_parent_class)->update) (item, i2w_dx, i2w_dy, flags);

	self = NEMO_SELECTION_CANVAS_ITEM (item);
	priv = self->priv;

	x1 = priv->x1 + i2w_dx;
	y1 = priv->y1 + i2w_dy;
	x2 = priv->x2 + i2w_dx;
	y2 = priv->y2 + i2w_dy;

	eel_canvas_w2c (item->canvas, x1, y1, &cx1, &cy1);
	eel_canvas_w2c (item->canvas, x2, y2, &cx2, &cy2);

	update_rect = make_rect (cx1, cy1, cx2+1, cy2+1);
	diff_rects (update_rect, priv->last_update_rect,
		    &repaint_rects_count, repaint_rects);
	for (i = 0; i < repaint_rects_count; i++) {
		eel_canvas_request_redraw (item->canvas,
					   repaint_rects[i].x0, repaint_rects[i].y0,
					   repaint_rects[i].x1, repaint_rects[i].y1);
	}

	priv->last_update_rect = update_rect;

	if (priv->outline_set) {
		/* Outline and bounding box */
		width_pixels = (int) priv->width;
		width_lt = width_pixels / 2;
		width_rb = (width_pixels + 1) / 2;
		
		cx1 -= width_lt;
		cy1 -= width_lt;
		cx2 += width_rb;
		cy2 += width_rb;

		update_rect = make_rect (cx1, cy1, cx2, cy2);
		request_redraw_borders (item->canvas, &update_rect,
					(width_lt + width_rb));
		request_redraw_borders (item->canvas, &priv->last_outline_update_rect,
					priv->last_outline_update_width);
		priv->last_outline_update_rect = update_rect;
		priv->last_outline_update_width = width_lt + width_rb;
		
		item->x1 = cx1;
		item->y1 = cy1;
		item->x2 = cx2+1;
		item->y2 = cy2+1;
	} else {
		item->x1 = cx1;
		item->y1 = cy1;
		item->x2 = cx2+1;
		item->y2 = cy2+1;
	}
}

static void
nemo_selection_canvas_item_translate (EelCanvasItem *item,
					  double dx,
					  double dy)
{
	NemoSelectionCanvasItem *self;

	self = NEMO_SELECTION_CANVAS_ITEM (item);

	self->priv->x1 += dx;
	self->priv->y1 += dy;
	self->priv->x2 += dx;
	self->priv->y2 += dy;
}

static void
nemo_selection_canvas_item_bounds (EelCanvasItem *item,
				       double *x1,
				       double *y1,
				       double *x2,
				       double *y2)
{
	NemoSelectionCanvasItem *self;
	double hwidth;

	self = NEMO_SELECTION_CANVAS_ITEM (item);

	hwidth = (self->priv->width / item->canvas->pixels_per_unit) / 2.0;

	*x1 = self->priv->x1 - hwidth;
	*y1 = self->priv->y1 - hwidth;
	*x2 = self->priv->x2 + hwidth;
	*y2 = self->priv->y2 + hwidth;
}

#define FADE_OUT_STEPS 5
#define FADE_OUT_SPEED 30

static gboolean
fade_and_request_redraw (gpointer user_data)
{
	NemoSelectionCanvasItem *self = user_data;

	if (self->priv->fade_out_fill_alpha <= 0 ||
	    self->priv->fade_out_outline_alpha <= 0) {
		self->priv->fade_out_handler_id = 0;
		eel_canvas_item_destroy (EEL_CANVAS_ITEM (self));

		return FALSE;
	}

	self->priv->fade_out_fill_alpha -= self->priv->fade_out_fill_delta;
	self->priv->fade_out_outline_alpha -= self->priv->fade_out_outline_delta;

	eel_canvas_item_request_redraw (EEL_CANVAS_ITEM (self));

	return TRUE;
}

void
nemo_selection_canvas_item_fade_out (NemoSelectionCanvasItem *self,
					 guint transition_time)
{
	self->priv->fade_out_fill_alpha = self->priv->fill_color.alpha;
	self->priv->fade_out_outline_alpha = self->priv->outline_color.alpha;

	self->priv->fade_out_fill_delta = self->priv->fade_out_fill_alpha / FADE_OUT_STEPS;
	self->priv->fade_out_outline_delta = self->priv->fade_out_outline_alpha / FADE_OUT_STEPS;

	self->priv->fade_out_handler_id =
		g_timeout_add ((guint) (transition_time / FADE_OUT_STEPS),
			       fade_and_request_redraw, self);
}

static void
nemo_selection_canvas_item_dispose (GObject *obj)
{
	NemoSelectionCanvasItem *self = NEMO_SELECTION_CANVAS_ITEM (obj);

	if (self->priv->fade_out_handler_id != 0) {
		g_source_remove (self->priv->fade_out_handler_id);
		self->priv->fade_out_handler_id = 0;
	}

	G_OBJECT_CLASS (nemo_selection_canvas_item_parent_class)->dispose (obj);
}

static void
do_set_fill (NemoSelectionCanvasItem *self,
	     gboolean fill_set)
{
	if (self->priv->fill_set != fill_set) {
		self->priv->fill_set = fill_set;
		eel_canvas_item_request_update (EEL_CANVAS_ITEM (self));
	}
}

static void
do_set_outline (NemoSelectionCanvasItem *self,
		gboolean outline_set)
{
	if (self->priv->outline_set != outline_set) {
		self->priv->outline_set = outline_set;
		eel_canvas_item_request_update (EEL_CANVAS_ITEM (self));
	}
}

static void
nemo_selection_canvas_item_set_property (GObject *object,
					     guint param_id,
					     const GValue *value,
					     GParamSpec *pspec)
{
	EelCanvasItem *item;
	NemoSelectionCanvasItem *self;

	self = NEMO_SELECTION_CANVAS_ITEM (object);
	item = EEL_CANVAS_ITEM (object);

	switch (param_id) {
	case PROP_X1:
		self->priv->x1 = g_value_get_double (value);

		eel_canvas_item_request_update (item);
		break;

	case PROP_Y1:
		self->priv->y1 = g_value_get_double (value);

		eel_canvas_item_request_update (item);
		break;

	case PROP_X2:
		self->priv->x2 = g_value_get_double (value);

		eel_canvas_item_request_update (item);
		break;

	case PROP_Y2:
		self->priv->y2 = g_value_get_double (value);

		eel_canvas_item_request_update (item);
		break;

	case PROP_FILL_COLOR_RGBA: {
		GdkRGBA *color;

		color = g_value_get_boxed (value);

		do_set_fill (self, color != NULL);

		if (color != NULL) {
			self->priv->fill_color = *color;
		}

		eel_canvas_item_request_redraw (item);		
		break;
	}

	case PROP_OUTLINE_COLOR_RGBA: {
		GdkRGBA *color;

		color = g_value_get_boxed (value);

		do_set_outline (self, color != NULL);

		if (color != NULL) {
			self->priv->outline_color = *color;
		}

		eel_canvas_item_request_redraw (item);		
		break;
	}

	case PROP_OUTLINE_STIPPLING:
		self->priv->outline_stippling = g_value_get_boolean (value);

		eel_canvas_item_request_redraw (item);
		break;

	case PROP_WIDTH_PIXELS:
		self->priv->width = g_value_get_uint (value);

		eel_canvas_item_request_update (item);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
nemo_selection_canvas_item_get_property (GObject *object,
					     guint param_id,
					     GValue *value,
					     GParamSpec *pspec)
{
	NemoSelectionCanvasItem *self;

	self = NEMO_SELECTION_CANVAS_ITEM (object);

	switch (param_id) {
	case PROP_X1:
		g_value_set_double (value,  self->priv->x1);
		break;

	case PROP_Y1:
		g_value_set_double (value,  self->priv->y1);
		break;

	case PROP_X2:
		g_value_set_double (value,  self->priv->x2);
		break;

	case PROP_Y2:
		g_value_set_double (value,  self->priv->y2);
		break;

	case PROP_FILL_COLOR_RGBA:
		g_value_set_boxed (value,  &self->priv->fill_color);
		break;

	case PROP_OUTLINE_COLOR_RGBA:
		g_value_set_boxed (value,  &self->priv->outline_color);
		break;

	case PROP_OUTLINE_STIPPLING:
		g_value_set_boolean (value, self->priv->outline_stippling);
		break;
	case PROP_WIDTH_PIXELS:
		g_value_set_uint (value, self->priv->width);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
nemo_selection_canvas_item_class_init (NemoSelectionCanvasItemClass *klass)
{
	EelCanvasItemClass *item_class;
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	item_class = EEL_CANVAS_ITEM_CLASS (klass);

	gobject_class->set_property = nemo_selection_canvas_item_set_property;
	gobject_class->get_property = nemo_selection_canvas_item_get_property;
	gobject_class->dispose = nemo_selection_canvas_item_dispose;

	item_class->draw = nemo_selection_canvas_item_draw;
	item_class->point = nemo_selection_canvas_item_point;
	item_class->update = nemo_selection_canvas_item_update;
	item_class->bounds = nemo_selection_canvas_item_bounds;
	item_class->translate = nemo_selection_canvas_item_translate;

	properties[PROP_X1] = 
                 g_param_spec_double ("x1", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      G_PARAM_READWRITE);
	properties[PROP_Y1] =
                 g_param_spec_double ("y1", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      G_PARAM_READWRITE);
	properties[PROP_X2] =
		g_param_spec_double ("x2", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      G_PARAM_READWRITE);
	properties[PROP_Y2] =
		g_param_spec_double ("y2", NULL, NULL,
				      -G_MAXDOUBLE, G_MAXDOUBLE, 0,
				      G_PARAM_READWRITE);
	properties[PROP_FILL_COLOR_RGBA] = 
                 g_param_spec_boxed ("fill-color-rgba", NULL, NULL,
				     GDK_TYPE_RGBA,
				     G_PARAM_READWRITE);
	properties[PROP_OUTLINE_COLOR_RGBA] =
		g_param_spec_boxed ("outline-color-rgba", NULL, NULL,
				     GDK_TYPE_RGBA,
				     G_PARAM_READWRITE);
	properties[PROP_OUTLINE_STIPPLING] =
		 g_param_spec_boolean ("outline-stippling", NULL, NULL,
				       FALSE, G_PARAM_READWRITE);
	properties[PROP_WIDTH_PIXELS] =
                 g_param_spec_uint ("width-pixels", NULL, NULL,
				    0, G_MAXUINT, 0,
				    G_PARAM_READWRITE);

	g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);
	g_type_class_add_private (klass, sizeof (NemoSelectionCanvasItemDetails));
}

static void
nemo_selection_canvas_item_init (NemoSelectionCanvasItem *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NEMO_TYPE_SELECTION_CANVAS_ITEM,
						  NemoSelectionCanvasItemDetails);
}




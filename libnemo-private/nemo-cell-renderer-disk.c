/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

*/

#include "nemo-cell-renderer-disk.h"
#include <math.h>

G_DEFINE_TYPE (NemoCellRendererDisk, nemo_cell_renderer_disk,
	       GTK_TYPE_CELL_RENDERER_TEXT);


static void     nemo_cell_renderer_disk_init       (NemoCellRendererDisk      *cell);

static void     nemo_cell_renderer_disk_class_init (NemoCellRendererDiskClass *klass);

static void     nemo_cell_renderer_disk_get_property  (GObject                    *object,
                                                       guint                       param_id,
                                                       GValue                     *value,
                                                       GParamSpec                 *pspec);

static void     nemo_cell_renderer_disk_set_property  (GObject                    *object,
                                                       guint                       param_id,
                                                       const GValue               *value,
                                                       GParamSpec                 *pspec);

static void     nemo_cell_renderer_disk_finalize (GObject *gobject);

static void     nemo_cell_renderer_disk_render (GtkCellRenderer       *cell,
                                                cairo_t               *cr,
                                                GtkWidget             *widget,
                                                const GdkRectangle    *background_area,
                                                const GdkRectangle    *cell_area,
                                                GtkCellRendererState   flags);

enum
{
  PROP_DISK_FULL_PERCENTAGE = 1,
  PROP_SHOW_DISK_FULL_PERCENTAGE = 2,
};

static   gpointer parent_class;

static void
nemo_cell_renderer_disk_init (NemoCellRendererDisk *cell)
{
	g_object_set (cell,
		      "disk-full-percent", 0,
		      "show-disk-full-percent", FALSE,
		      NULL);
}

static void
nemo_cell_renderer_disk_class_init (NemoCellRendererDiskClass *klass)
{
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);
    GObjectClass         *object_class = G_OBJECT_CLASS(klass);
    parent_class           = g_type_class_peek_parent (klass);
    object_class->finalize = nemo_cell_renderer_disk_finalize;

    object_class->get_property = nemo_cell_renderer_disk_get_property;
    object_class->set_property = nemo_cell_renderer_disk_set_property;
    cell_class->render = nemo_cell_renderer_disk_render;

    g_object_class_install_property (object_class,
                                     PROP_DISK_FULL_PERCENTAGE,
                                     g_param_spec_int ("disk-full-percent",
                                                       "Percentage",
                                                       "The fractional bar to display",
                                                       0, 100, 0,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (object_class,
                                     PROP_SHOW_DISK_FULL_PERCENTAGE,
                                     g_param_spec_boolean ("show-disk-full-percent",
                                                         "Show Percentage Graph",
                                                         "Whether to show the bar",
                                                         FALSE,
                                                         G_PARAM_READWRITE));
}

GtkCellRenderer *
nemo_cell_renderer_disk_new (void)
{
    return g_object_new (NEMO_TYPE_CELL_RENDERER_DISK, NULL);
}

static void
nemo_cell_renderer_disk_finalize (GObject *object)
{
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nemo_cell_renderer_disk_get_property (GObject    *object,
                                      guint       param_id,
                                      GValue     *value,
                                      GParamSpec *psec)
{
  NemoCellRendererDisk  *celldisk = NEMO_CELL_RENDERER_DISK (object);

  switch (param_id)
  {
    case PROP_DISK_FULL_PERCENTAGE:
        g_value_set_int(value, celldisk->disk_full_percent);
        break;
    case PROP_SHOW_DISK_FULL_PERCENTAGE:
        g_value_set_boolean(value, celldisk->show_disk_full_percent);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, psec);
        break;
  }
}

static void
nemo_cell_renderer_disk_set_property (GObject      *object,
                                      guint         param_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  NemoCellRendererDisk *celldisk = NEMO_CELL_RENDERER_DISK (object);

  switch (param_id)
  {
    case PROP_DISK_FULL_PERCENTAGE:
        celldisk->disk_full_percent = g_value_get_int (value);
        break;
    case PROP_SHOW_DISK_FULL_PERCENTAGE:
        celldisk->show_disk_full_percent = g_value_get_boolean (value);
        break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
      break;
  }
}

static void
convert_color (GdkColor *style_color, GdkRGBA *color)
{
    color->red = style_color->red / 65535.0;
    color->green = style_color->green / 65535.0;
    color->blue = style_color->blue / 65535.0;
    color->alpha = 1;
}

#define _270_DEG 270.0 * (M_PI/180.0)
#define _180_DEG 180.0 * (M_PI/180.0)
#define  _90_DEG  90.0 * (M_PI/180.0)
#define   _0_DEG 0.0

static void
cairo_rectangle_with_radius_corners (cairo_t *cr,
                                     gint x,
                                     gint y,
                                     gint w,
                                     gint h,
                                     gint rad)
{
    cairo_move_to (cr, x+rad, y);
    cairo_line_to (cr, x+w-rad, y);
    cairo_arc (cr, x+w-rad, y+rad, rad, _270_DEG, _0_DEG);
    cairo_line_to (cr, x+w, y+h-rad);
    cairo_arc (cr, x+w-rad, y+h-rad, rad, _0_DEG, _90_DEG);
    cairo_line_to (cr, x+rad, y+h);
    cairo_arc (cr, x+rad, y+h-rad, rad, _90_DEG, _180_DEG);
    cairo_line_to (cr, x, y-rad);
    cairo_arc (cr, x+rad, y+rad, rad, _180_DEG, _270_DEG);
}

static void
nemo_cell_renderer_disk_render (GtkCellRenderer       *cell,
                                cairo_t               *cr,
                                GtkWidget             *widget,
                                const GdkRectangle    *background_area,
                                const GdkRectangle    *cell_area,
                                GtkCellRendererState   flags)
{
    NemoCellRendererDisk *cellprogress = NEMO_CELL_RENDERER_DISK (cell);
    gint                        x, y, w;
    gint                        xpad, ypad;
    gint                        full;
    gboolean                    show = cellprogress->show_disk_full_percent;
    GtkStyleContext *context;

    if (show) {
        context = gtk_widget_get_style_context (widget);
        GdkColor *gdk_bg_color, *gdk_fg_color;
        GdkRGBA *bg_color, *fg_color;
        gint bar_width, bar_radius, bottom_padding, max_length;

        gtk_style_context_get_style (context,
                                     "disk-full-bg-color",       &gdk_bg_color,
                                     "disk-full-fg-color",       &gdk_fg_color,
                                     "disk-full-bar-width",      &bar_width,
                                     "disk-full-bar-radius",     &bar_radius,
                                     "disk-full-bottom-padding", &bottom_padding,
                                     "disk-full-max-length",     &max_length,
                                     NULL);

        convert_color (gdk_bg_color, bg_color);
        convert_color (gdk_fg_color, fg_color);

        gdk_color_free (gdk_bg_color);
        gdk_color_free (gdk_fg_color);

        gtk_cell_renderer_get_padding (cell, &xpad, &ypad);
        x = cell_area->x + xpad;
        y = cell_area->y + cell_area->height - bar_width - bottom_padding;
        w = cell_area->width - xpad * 2;
        w = w < max_length ? w : max_length;
        full = (int) (((float) cellprogress->disk_full_percent / 100.0) * (float) w);

        gtk_style_context_save (context);

        cairo_save (cr);

        cairo_set_source_rgba (cr,
                               bg_color->red,
                               bg_color->green,
                               bg_color->blue,
                               bg_color->alpha);

        cairo_rectangle_with_radius_corners (cr, x, y, w, bar_width, bar_radius);
        cairo_fill (cr);

        cairo_restore (cr);
        cairo_save (cr);

        cairo_set_source_rgba (cr,
                               fg_color->red,
                               fg_color->green,
                               fg_color->blue,
                               fg_color->alpha);

        cairo_rectangle_with_radius_corners (cr, x, y, full, bar_width, bar_radius);
        cairo_fill (cr);

        cairo_restore (cr);

        gdk_rgba_free (bg_color);
        gdk_rgba_free (fg_color);

        gtk_style_context_restore (context);
    }

    GTK_CELL_RENDERER_CLASS (parent_class)->render (cell,
                                                    cr,
                                                    widget,
                                                    background_area,
                                                    cell_area,
                                                    flags);
}

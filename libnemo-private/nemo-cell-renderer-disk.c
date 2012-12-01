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

static void     nemo_cell_renderer_disk_render (GtkCellRenderer *cell,
                                                cairo_t         *cr,
                                                GtkWidget       *widget,
                                                GdkRectangle    *background_area,
                                                GdkRectangle    *cell_area,
                                                guint            flags);

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
    cell_class->render   = nemo_cell_renderer_disk_render;

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

#define BAR_HEIGHT 2

static void
nemo_cell_renderer_disk_render (GtkCellRenderer *cell,
                                cairo_t         *cr,
                                GtkWidget       *widget,
                                GdkRectangle    *background_area,
                                GdkRectangle    *cell_area,
                                guint            flags)
{
    NemoCellRendererDisk *cellprogress = NEMO_CELL_RENDERER_DISK (cell);
    GtkCellRendererText *celltext = GTK_CELL_RENDERER_TEXT (cell);
    GtkCellRendererTextPrivate *priv = celltext->priv;
    GtkStateType                state;
    gint                        x, y, w;
    gint                        xpad, ypad;
    gint                        full;
    gboolean                    show = cellprogress->show_disk_full_percent;
    GtkStyleContext *context;

    if (show) {
        context = gtk_widget_get_style_context (widget);
        gtk_cell_renderer_get_padding (cell, &xpad, &ypad);
        x = cell_area->x + xpad;
        y = cell_area->y + cell_area->height - BAR_HEIGHT;
        w = cell_area->width - xpad * 2;
        w = w < 100 ? w : 100;
        full = (int) (((float) cellprogress->disk_full_percent / 100.0) * (float) w);

        gtk_style_context_save (context);
        gtk_style_context_add_class (context, GTK_STYLE_CLASS_TROUGH);
        gtk_render_frame (context, cr, x, y, w, BAR_HEIGHT);
        gtk_style_context_restore (context);

        gtk_style_context_save (context);
        gtk_style_context_add_class (context, GTK_STYLE_CLASS_PROGRESSBAR);
        gtk_render_activity (context, cr, x, y, full, BAR_HEIGHT);
    }

    GTK_CELL_RENDERER_CLASS (parent_class)->render (cell,
                                                    cr,
                                                    widget,
                                                    background_area,
                                                    cell_area,
                                                    flags);
}

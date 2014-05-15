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

#include "nemo-pathbar-button.h"
#include <math.h>

G_DEFINE_TYPE (NemoPathbarButton, nemo_pathbar_button,
	       GTK_TYPE_TOGGLE_BUTTON);


static void     nemo_pathbar_button_init       (NemoPathbarButton      *button);

static void     nemo_pathbar_button_class_init (NemoPathbarButtonClass *klass);

static void     nemo_pathbar_button_finalize (GObject *gobject);

static gboolean     nemo_pathbar_button_draw (GtkWidget               *widget,
                                            cairo_t               *cr);

static   gpointer parent_class, draw_chain_class;

static void
nemo_pathbar_button_init (NemoPathbarButton *button)
{
    button->is_left_end = FALSE;
    button->highlight = FALSE;

    GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (button));
    gtk_style_context_remove_class (context, GTK_STYLE_CLASS_BUTTON);
    gtk_style_context_add_class (context, "nemo-pathbar-button");
}

static void
nemo_pathbar_button_class_init (NemoPathbarButtonClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass         *object_class = G_OBJECT_CLASS(klass);
    parent_class           = g_type_class_peek_parent (klass);
    draw_chain_class       = g_type_class_peek_parent (g_type_class_peek_parent (parent_class));
    object_class->finalize = nemo_pathbar_button_finalize;

    widget_class->draw = nemo_pathbar_button_draw;

    gtk_widget_class_install_style_property (widget_class,
                       g_param_spec_int ("border-radius",
                                 "The border radius of the breadcrumbs border",
                                 "The border radius of the breadcrumbs border",
                                 G_MININT,
                                 G_MAXINT,
                                 0,
                                 G_PARAM_READABLE));

}

GtkWidget *
nemo_pathbar_button_new (void)
{
	return g_object_new (NEMO_TYPE_PATHBAR_BUTTON, NULL);
}

static void
nemo_pathbar_button_finalize (GObject *object)
{
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

#define A_A CAIRO_ANTIALIAS_SUBPIXEL
#define _270_DEG 270.0 * (M_PI/180.0)
#define _180_DEG 180.0 * (M_PI/180.0)
#define  _90_DEG  90.0 * (M_PI/180.0)

#define H_O 3  /*  Highlight offset */
#define H_T_COMP H_O + 0 /* Highlight tangential compensation */

static void
do_draw_middle_element (GtkStyleContext  *context,
                                 cairo_t *cr,
                                   gint   x,
                                   gint   y,
                                   gint   w,
                                   gint   h,
                               gboolean   highlight)
{
    GtkStateFlags state = gtk_style_context_get_state (context);
    GdkRGBA border_color;
    gtk_style_context_get_border_color (context, state, &border_color);

    gint offset = rintf ((float) h / PATHBAR_BUTTON_OFFSET_FACTOR);

    cairo_save (cr);
    cairo_set_antialias (cr, A_A);

    cairo_set_source_rgba (cr, border_color.red,
                               border_color.green,
                               border_color.blue,
                               border_color.alpha);

    cairo_set_line_width (cr, 3.0);

    cairo_move_to (cr, x, y);
    cairo_line_to (cr, x+w-offset, y);
    cairo_line_to (cr, x+w-1, y+(h/2));
    cairo_line_to (cr, x+w-offset, y+h);
    cairo_line_to (cr, x, y+h);
    cairo_line_to (cr, x+offset-1, y+(h/2));
    cairo_line_to (cr, x, y);

    cairo_stroke_preserve (cr);
    cairo_clip (cr);

    gtk_render_background (context, cr, x, y, w, h);
    cairo_restore (cr);

    if (!highlight)
        return;

    cairo_save (cr);
    cairo_set_antialias (cr, A_A);

    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_set_line_width (cr, .5);

    cairo_move_to (cr, x+H_O+H_T_COMP, y+H_O);
    cairo_line_to (cr, x+w-offset-H_O+H_T_COMP, y+H_O);
    cairo_line_to (cr, x+w-1-H_O, y+(h/2));
    cairo_line_to (cr, x+w-offset-H_O+H_T_COMP, y+h-H_O);
    cairo_line_to (cr, x+H_O+H_T_COMP, y+h-H_O);
    cairo_line_to (cr, x+offset-1+H_O, y+(h/2));
    cairo_line_to (cr, x+H_O+H_T_COMP, y+H_O);

    cairo_stroke (cr);

    cairo_restore (cr);
}

static void
do_draw_end_element (GtkStyleContext *context,
                             cairo_t *cr,
                             gint     x,
                             gint     y,
                             gint     w,
                             gint     h,
                         gboolean     highlight)
{

    GtkStateFlags state = gtk_style_context_get_state (context);
    GdkRGBA border_color;
    gint rad;
    gtk_style_context_get_border_color (context, state, &border_color);
    gtk_style_context_get_style (context, "border-radius", &rad, NULL);

    gint offset = rintf ((float) h / PATHBAR_BUTTON_OFFSET_FACTOR);
    cairo_save (cr);
    cairo_set_antialias (cr, A_A);

    cairo_set_source_rgba (cr, border_color.red,
                               border_color.green,
                               border_color.blue,
                               border_color.alpha);

    cairo_set_line_width (cr, 3.0);

    cairo_move_to (cr, x+rad, y);
    cairo_line_to (cr, x+w-offset, y);
    cairo_line_to (cr, x+w-1, y+(h/2));
    cairo_line_to (cr, x+w-offset, y+h);
    cairo_line_to (cr, x+rad, y+h);
    cairo_arc (cr, x+rad, y+h-rad, rad, _90_DEG, _180_DEG);
    cairo_line_to (cr, x, y+rad);
    cairo_arc (cr, x+rad, y+rad, rad, _180_DEG, _270_DEG);

    cairo_stroke_preserve (cr);
    cairo_clip (cr);

    gtk_render_background (context, cr, x, y, w, h);
    cairo_restore (cr);

    if (!highlight)
        return;

    cairo_save (cr);
    cairo_set_antialias (cr, A_A);

    cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_set_line_width (cr, .5);

    cairo_move_to (cr, x+(rad-H_O)+H_O, y+H_O);
    cairo_line_to (cr, x+w-offset-H_O+H_T_COMP, y+H_O);
    cairo_line_to (cr, x+w-1-H_O, y+(h/2));
    cairo_line_to (cr, x+w-offset-H_O+H_T_COMP, y+h-H_O);
    cairo_line_to (cr, x+(rad-H_O)+H_O, y+h-H_O);
    cairo_arc (cr, x+(rad-H_O)+H_O, y+h-(rad-H_O)-H_O, (rad-H_O), _90_DEG, _180_DEG);
    cairo_line_to (cr, x+H_O, y+(rad-H_O)+H_O);
    cairo_arc (cr, x+(rad-H_O)+H_O, y+(rad-H_O)+H_O, (rad-H_O), _180_DEG, _270_DEG);

    cairo_stroke (cr);

    cairo_restore (cr);
}

static gboolean
nemo_pathbar_button_draw (GtkWidget                   *widget,
                            cairo_t                   *cr)
{
    NemoPathbarButton *button = NEMO_PATHBAR_BUTTON (widget);
    gint x, y, width, height;

    GtkAllocation allocation;
    GtkStyleContext *context;

    context = gtk_widget_get_style_context (widget);
    gtk_widget_get_allocation (widget, &allocation);

    x = 2;
    y = 1;
    width = allocation.width-3;
    height = allocation.height-2;

    if (button->is_left_end)
        do_draw_end_element (context, cr, x, y, width, height, button->highlight);
    else
        do_draw_middle_element (context, cr, x, y, width, height, button->highlight);

    gtk_style_context_add_class (context, "breadcrumbs-no-displacement");

    return GTK_WIDGET_CLASS (draw_chain_class)->draw (widget, cr);
}

void
nemo_pathbar_button_set_is_left_end (GtkWidget *button, gboolean left_end)
{
    NEMO_PATHBAR_BUTTON (button)->is_left_end = left_end;

    gtk_widget_queue_draw (GTK_WIDGET (button));
}

void
nemo_pathbar_button_set_highlight (GtkWidget *button, gboolean highlight)
{
    NEMO_PATHBAR_BUTTON (button)->highlight = highlight;

    gtk_widget_queue_draw (GTK_WIDGET (button));
}

void
nemo_pathbar_button_get_preferred_size (GtkWidget *button, GtkRequisition *requisition, gint height)
{
    GtkRequisition req;
    gtk_widget_get_preferred_size (button, NULL, &req);
    gint offset = rintf ((float) req.height / PATHBAR_BUTTON_OFFSET_FACTOR) + 4;
    if (!NEMO_PATHBAR_BUTTON (button)->is_left_end) {
        req.width -= offset;
    }
    req.height = height;
    *requisition = req;
}


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
#define RAD 5.0
#define _270_DEG 270.0 * (M_PI/180.0)
#define _180_DEG 180.0 * (M_PI/180.0)
#define  _90_DEG  90.0 * (M_PI/180.0)
#define H_RAD 1 /* Highlight radius */
#define H_O 2  /*  Highlight offset */
#define H_T_COMP H_O - 1 /* Highlight tangential compensation */

static void
do_draw_middle_element (GtkStyleContext  *context,
                                 cairo_t *cr,
                                   gint   x,
                                   gint   y,
                                   gint   w,
                                   gint   h,
                               gboolean   highlight)
{
    cairo_save (cr);
    cairo_set_antialias (cr, A_A);

    cairo_set_source_rgb (cr, .5, .5, .5);
    cairo_set_line_width (cr, 2.0);

    cairo_move_to (cr, x, y);
    cairo_line_to (cr, x+w-(h/3), y);
    cairo_line_to (cr, x+w-1, y+(h/2));
    cairo_line_to (cr, x+w-(h/3), y+h);
    cairo_line_to (cr, x, y+h);
    cairo_line_to (cr, x+(h/3)-1, y+(h/2));
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
    cairo_line_to (cr, x+w-(h/3)-H_O+H_T_COMP, y+H_O);
    cairo_line_to (cr, x+w-1-H_O, y+(h/2));
    cairo_line_to (cr, x+w-(h/3)-H_O+H_T_COMP, y+h-H_O);
    cairo_line_to (cr, x+H_O+H_T_COMP, y+h-H_O);
    cairo_line_to (cr, x+(h/3)-1+H_O, y+(h/2));
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
    cairo_save (cr);
    cairo_set_antialias (cr, A_A);

    cairo_set_source_rgb (cr, .5, .5, .5);
    cairo_set_line_width (cr, 2.0);

    cairo_move_to (cr, x+RAD, y);
    cairo_line_to (cr, x+w-(h/3), y);
    cairo_line_to (cr, x+w-1, y+(h/2));
    cairo_line_to (cr, x+w-(h/3), y+h);
    cairo_line_to (cr, x+RAD, y+h);
    cairo_arc (cr, x+RAD, y+h-RAD, RAD, _90_DEG, _180_DEG);
    cairo_line_to (cr, x, y+RAD);
    cairo_arc (cr, x+RAD, y+RAD, RAD, _180_DEG, _270_DEG);

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

    cairo_move_to (cr, x+H_RAD+H_O, y+H_O);
    cairo_line_to (cr, x+w-(h/3)-H_O+H_T_COMP, y+H_O);
    cairo_line_to (cr, x+w-1-H_O, y+(h/2));
    cairo_line_to (cr, x+w-(h/3)-H_O+H_T_COMP, y+h-H_O);
    cairo_line_to (cr, x+H_RAD+H_O, y+h-H_O);
    cairo_arc (cr, x+H_RAD+H_O, y+h-H_RAD-H_O, H_RAD, _90_DEG, _180_DEG);
    cairo_line_to (cr, x+H_O, y+H_RAD+H_O);
    cairo_arc (cr, x+H_RAD+H_O, y+H_RAD+H_O, H_RAD, _180_DEG, _270_DEG);

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
    GtkStateFlags state;

    context = gtk_widget_get_style_context (widget);
    state = gtk_style_context_get_state (context);

    gtk_style_context_save (context);

    switch (state) {
        case GTK_STATE_FLAG_NORMAL:
            gtk_style_context_add_class (context, "nemo-pathbar-button");
            break;
        case GTK_STATE_FLAG_ACTIVE | GTK_STATE_FLAG_PRELIGHT:
        case GTK_STATE_FLAG_PRELIGHT:
        case GTK_STATE_FLAG_FOCUSED:
        case GTK_STATE_FLAG_ACTIVE | GTK_STATE_FLAG_FOCUSED:
            gtk_style_context_add_class (context, "nemo-pathbar-button-hover");
            break;
        case GTK_STATE_FLAG_ACTIVE:
        case GTK_STATE_FLAG_ACTIVE | GTK_STATE_FLAG_BACKDROP:
            gtk_style_context_add_class (context, "nemo-pathbar-button-active");
            break;
        default:
            gtk_style_context_add_class (context, "nemo-pathbar-button");
            break;
    }

    gtk_widget_get_allocation (widget, &allocation);

    x = 0;
    y = 0;
    width = allocation.width;
    height = allocation.height;

    if (button->is_left_end)
        do_draw_end_element (context, cr, x, y, width, height, button->highlight);
    else
        do_draw_middle_element (context, cr, x, y, width, height, button->highlight);

    gtk_style_context_restore (context);

    GTK_WIDGET_CLASS (draw_chain_class)->draw (widget, cr);

    return FALSE;

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
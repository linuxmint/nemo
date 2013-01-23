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

#include "nemo-places-tree-view.h"

G_DEFINE_TYPE (NemoPlacesTreeView, nemo_places_tree_view,
               GTK_TYPE_TREE_VIEW);


static void     nemo_places_tree_view_init       (NemoPlacesTreeView      *tree_view);

static void     nemo_places_tree_view_class_init (NemoPlacesTreeViewClass *klass);

static void     nemo_places_tree_view_finalize (GObject *gobject);

static gpointer parent_class;

static void
nemo_places_tree_view_init (NemoPlacesTreeView *tree_view)
{

}

static void
nemo_places_tree_view_class_init (NemoPlacesTreeViewClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    GObjectClass         *object_class = G_OBJECT_CLASS(klass);
    parent_class           = g_type_class_peek_parent (klass);
    object_class->finalize = nemo_places_tree_view_finalize;

    gtk_widget_class_install_style_property (widget_class,
                         g_param_spec_boxed ("disk-full-bg-color",
                                             "Unselected disk indicator background color",
                                             "Unselected disk indicator background color",
                                             GDK_TYPE_COLOR,
                                             G_PARAM_READABLE));

    gtk_widget_class_install_style_property (widget_class,
                         g_param_spec_boxed ("disk-full-fg-color",
                                             "Unselected disk indicator foreground color",
                                             "Unselected disk indicator foreground color",
                                             GDK_TYPE_COLOR,
                                             G_PARAM_READABLE));

    gtk_widget_class_install_style_property (widget_class,
                           g_param_spec_int ("disk-full-bar-width",
                                             "Disk indicator bar width",
                                             "Disk indicator bar width",
                                             0, G_MAXINT, 2,
                                             G_PARAM_READABLE));

    gtk_widget_class_install_style_property (widget_class,
                           g_param_spec_int ("disk-full-bar-radius",
                                             "Disk indicator bar radius (usually half the width)",
                                             "Disk indicator bar radius (usually half the width)",
                                             0, G_MAXINT, 1,
                                             G_PARAM_READABLE));

    gtk_widget_class_install_style_property (widget_class,
                           g_param_spec_int ("disk-full-bottom-padding",
                                             "Extra padding under the disk indicator",
                                             "Extra padding under the disk indicator",
                                             0, G_MAXINT, 1,
                                             G_PARAM_READABLE));

    gtk_widget_class_install_style_property (widget_class,
                           g_param_spec_int ("disk-full-max-length",
                                             "Maximum length of the disk indicator",
                                             "Maximum length of the disk indicator",
                                             0, G_MAXINT, 70,
                                             G_PARAM_READABLE));

}

GtkWidget *
nemo_places_tree_view_new (void)
{
    return g_object_new (NEMO_TYPE_PLACES_TREE_VIEW, NULL);
}

static void
nemo_places_tree_view_finalize (GObject *object)
{
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

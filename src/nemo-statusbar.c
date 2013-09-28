/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* nemo-statusbar.c
 * 
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

#include "nemo-statusbar.h"

#include "nemo-actions.h"
#include "nemo-icon-info.h"

#include <config.h>
#include <glib/gi18n.h>

enum {
        LAST_SIGNAL
};

enum {
    PROP_WINDOW = 1,
    NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NemoStatusBar, nemo_status_bar, GTK_TYPE_BOX);

static void
nemo_status_bar_init (NemoStatusBar *bar)
{
    bar->window = NULL;
}

static void
nemo_status_bar_set_property (GObject        *object,
                              guint           arg_id,
                              const GValue   *value,
                              GParamSpec     *pspec)
{
    NemoStatusBar *self = NEMO_STATUS_BAR (object);

    switch (arg_id) {
        case PROP_WINDOW:
            self->window = g_value_get_object (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
            break;
    }
}

static void
nemo_status_bar_get_property (GObject      *object,
                              guint         arg_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
    NemoStatusBar *self = NEMO_STATUS_BAR (object);

    switch (arg_id) {
        case PROP_WINDOW:
            g_value_set_object (value, self->window);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
            break;
    }
}

static void
nemo_status_bar_dispose (GObject *object)
{
    NemoStatusBar *bar = NEMO_STATUS_BAR (object);

    bar->window = NULL;

    G_OBJECT_CLASS (nemo_status_bar_parent_class)->dispose (object);
}

static void
action_places_toggle_callback (GtkButton *button, NemoStatusBar *bar)
{
    nemo_window_set_sidebar_id (NEMO_WINDOW (bar->window), NEMO_WINDOW_SIDEBAR_PLACES);

    nemo_status_bar_sync_button_states (bar);
}

static void
action_treeview_toggle_callback (GtkButton *button, NemoStatusBar *bar)
{
    nemo_window_set_sidebar_id (NEMO_WINDOW (bar->window), NEMO_WINDOW_SIDEBAR_TREE);

    nemo_status_bar_sync_button_states (bar);
}

static void
action_show_sidebar_callback (GtkButton *button, NemoStatusBar *bar)
{
    nemo_window_show_sidebar (bar->window);
}

static void
action_hide_sidebar_callback (GtkButton *button, NemoStatusBar *bar)
{
    nemo_window_hide_sidebar (bar->window);
}

static void
sidebar_state_changed_cb (gpointer pointer, gboolean state, gpointer user_data)
{
    nemo_status_bar_sync_button_states (NEMO_STATUS_BAR (user_data));
}

static void
sidebar_type_changed_cb (gpointer pointer, const gchar *sidebar_id, gpointer user_data)
{
    nemo_status_bar_sync_button_states (NEMO_STATUS_BAR (user_data));
}

static void
on_slider_changed_cb (GtkWidget *zoom_slider, gpointer user_data)
{
    NemoStatusBar *bar = NEMO_STATUS_BAR (user_data);
    gdouble val = gtk_range_get_value (GTK_RANGE (zoom_slider));

    NemoWindowSlot *slot = nemo_window_get_active_slot (bar->window);

    if (!NEMO_IS_WINDOW_SLOT (slot))
        return;

    NemoView *view = slot->content_view;

    if (!NEMO_IS_VIEW (view))
        return;

    nemo_view_zoom_to_level (view, (int) val);
}

static gboolean
on_slider_scroll_event (GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
    gboolean in;
    gint val;
    gdouble delta_y;
    gdk_event_get_scroll_deltas ((GdkEvent *) event, NULL, &delta_y);
    in = delta_y < 0;
    val = (int) gtk_range_get_value (GTK_RANGE (widget));

    if (in) {
        if (val < NEMO_ZOOM_LEVEL_LARGEST)
            val++;
    } else {
        if (val > NEMO_ZOOM_LEVEL_SMALLEST)
            val--;
    }

    gtk_range_set_value (GTK_RANGE (widget), (gdouble) val);

    return TRUE;
}

#define SLIDER_WIDTH 100

static void
nemo_status_bar_constructed (GObject *object)
{
    NemoStatusBar *bar = NEMO_STATUS_BAR (object);
    G_OBJECT_CLASS (nemo_status_bar_parent_class)->constructed (object);

    GtkWidget *statusbar = gtk_statusbar_new ();
    GtkStyleContext *context;

    bar->real_statusbar = statusbar;

    GtkIconSize size = gtk_icon_size_from_name (NEMO_STATUSBAR_ICON_SIZE_NAME);

    context = gtk_widget_get_style_context (GTK_WIDGET (bar));
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_TOOLBAR);
    gtk_container_set_border_width (GTK_CONTAINER (bar), 1);

    GtkWidget *button, *icon;

    button = gtk_toggle_button_new ();
    icon = gtk_image_new_from_icon_name ("sidebar-places-symbolic", size);
    gtk_button_set_image (GTK_BUTTON (button), icon);
    gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Show Places"));
    bar->places_button = button;
    gtk_box_pack_start (GTK_BOX (bar), button, FALSE, FALSE, 2);
    g_signal_connect (GTK_BUTTON (button), "clicked",
                      G_CALLBACK (action_places_toggle_callback), bar);

    button = gtk_toggle_button_new ();
    icon = gtk_image_new_from_icon_name ("sidebar-tree-symbolic", size);
    gtk_button_set_image (GTK_BUTTON (button), icon);
    gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Show Treeview"));
    bar->tree_button = button;
    gtk_box_pack_start (GTK_BOX (bar), button, FALSE, FALSE, 2);
    g_signal_connect (GTK_BUTTON (button), "clicked",
                      G_CALLBACK (action_treeview_toggle_callback), bar);

    GtkWidget *sep = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start (GTK_BOX (bar), sep, FALSE, FALSE, 6);
    gtk_widget_show (sep);
    bar->separator = sep;

    button = gtk_button_new ();
    icon = gtk_image_new_from_icon_name ("sidebar-hide-symbolic", size);
    gtk_button_set_image (GTK_BUTTON (button), icon);
    gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Hide the Sidebar (F9)"));
    bar->hide_button = button;
    gtk_box_pack_start (GTK_BOX (bar), button, FALSE, FALSE, 2);
    g_signal_connect (GTK_BUTTON (button), "clicked",
                      G_CALLBACK (action_hide_sidebar_callback), bar);

    button = gtk_button_new ();
    icon = gtk_image_new_from_icon_name ("sidebar-show-symbolic", size);
    gtk_button_set_image (GTK_BUTTON (button), icon);
    gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Show the Sidebar (F9)"));
    bar->show_button = button;
    gtk_box_pack_start (GTK_BOX (bar), button, FALSE, FALSE, 2);
    g_signal_connect (GTK_BUTTON (button), "clicked",
                      G_CALLBACK (action_show_sidebar_callback), bar);

    gtk_box_pack_start (GTK_BOX (bar), statusbar, TRUE, TRUE, 10);

    GtkWidget *zoom_slider = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                       (gdouble) NEMO_ZOOM_LEVEL_SMALLEST,
                                                       (gdouble) NEMO_ZOOM_LEVEL_LARGEST,
                                                       1.0);
    gtk_widget_set_tooltip_text (GTK_WIDGET (zoom_slider), _("Adjust zoom level"));
    bar->zoom_slider = zoom_slider;

    gtk_box_pack_start (GTK_BOX (bar), zoom_slider, FALSE, FALSE, 2);

    gtk_widget_set_size_request (GTK_WIDGET (zoom_slider), SLIDER_WIDTH, 0);
    gtk_scale_set_draw_value (GTK_SCALE (zoom_slider), FALSE);
    gtk_range_set_increments (GTK_RANGE (zoom_slider), 1.0, 1.0);
    gtk_range_set_round_digits (GTK_RANGE (zoom_slider), 0);

    g_signal_connect (GTK_WIDGET (zoom_slider), "scroll-event",
                      G_CALLBACK (on_slider_scroll_event), bar);

    gtk_widget_show_all (GTK_WIDGET (bar));

    g_signal_connect_object (NEMO_WINDOW (bar->window), "notify::show-sidebar",
                             G_CALLBACK (sidebar_state_changed_cb), bar, G_CONNECT_AFTER);

    g_signal_connect_object (NEMO_WINDOW (bar->window), "notify::sidebar-view-id",
                           G_CALLBACK (sidebar_type_changed_cb), bar, G_CONNECT_AFTER);

    g_signal_connect (GTK_RANGE (zoom_slider), "value-changed",
                      G_CALLBACK (on_slider_changed_cb), bar);

    GtkWidget *cont = gtk_statusbar_get_message_area (GTK_STATUSBAR (statusbar));

    GList *children = gtk_container_get_children (GTK_CONTAINER (cont));

    gtk_box_set_child_packing (GTK_BOX (cont),
                               GTK_WIDGET (children->data),
                               TRUE, FALSE, 10, GTK_PACK_START);

    nemo_status_bar_sync_button_states (bar);
}


static void
nemo_status_bar_class_init (NemoStatusBarClass *status_bar_class)
{
    GObjectClass *oclass;
    GtkWidgetClass *wclass;
    GtkBoxClass *container_class;

    oclass = G_OBJECT_CLASS (status_bar_class);
    wclass = GTK_WIDGET_CLASS (status_bar_class);
    container_class = (GtkBoxClass *) status_bar_class;

    oclass->set_property = nemo_status_bar_set_property;
    oclass->get_property = nemo_status_bar_get_property;

    oclass->dispose = nemo_status_bar_dispose;
    oclass->constructed = nemo_status_bar_constructed;

    properties[PROP_WINDOW] = g_param_spec_object ("window",
                                                   "The NemoWindow",
                                                   "The parent NemoWindow",
                                                   NEMO_TYPE_WINDOW,
                                                   G_PARAM_READWRITE |
                                                   G_PARAM_CONSTRUCT_ONLY |
                                                   G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

GtkWidget *
nemo_status_bar_new (NemoWindow *window)
{
    return g_object_new (NEMO_TYPE_STATUS_BAR,
                         "orientation", GTK_ORIENTATION_HORIZONTAL,
                         "spacing", 0,
                         "window", window,
                         NULL);
}

GtkWidget *
nemo_status_bar_get_real_statusbar (NemoStatusBar *bar)
{
    return bar->real_statusbar;
}

void
nemo_status_bar_sync_button_states (NemoStatusBar *bar)
{
    const gchar *sidebar_id = nemo_window_get_sidebar_id (NEMO_WINDOW (bar->window));

    gboolean sidebar_visible = nemo_window_get_show_sidebar (NEMO_WINDOW (bar->window));

    if (sidebar_visible) {
        gtk_widget_show (bar->tree_button);
        gtk_widget_show (bar->places_button);
        gtk_widget_show (bar->separator);
        gtk_widget_show (bar->hide_button);
        gtk_widget_hide (bar->show_button);
    } else {
        gtk_widget_hide (bar->tree_button);
        gtk_widget_hide (bar->places_button);
        gtk_widget_hide (bar->hide_button);
        gtk_widget_hide (bar->separator);
        gtk_widget_show (bar->show_button);
    }

    g_signal_handlers_block_by_func (GTK_BUTTON (bar->tree_button), action_treeview_toggle_callback, bar);
    if (g_strcmp0 (sidebar_id, NEMO_WINDOW_SIDEBAR_TREE) == 0) {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bar->tree_button), TRUE);
    } else {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bar->tree_button), FALSE);
    }
    g_signal_handlers_unblock_by_func (GTK_BUTTON (bar->tree_button), action_treeview_toggle_callback, bar);


    g_signal_handlers_block_by_func (GTK_BUTTON (bar->places_button), action_places_toggle_callback, bar);

    if (g_strcmp0 (sidebar_id, NEMO_WINDOW_SIDEBAR_PLACES) == 0) {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bar->places_button), TRUE);
    } else {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bar->places_button), FALSE);
    }
    g_signal_handlers_unblock_by_func (GTK_BUTTON (bar->places_button), action_places_toggle_callback, bar);
}

void
nemo_status_bar_sync_zoom_widgets (NemoStatusBar *bar)
{

    NemoWindowSlot *slot = nemo_window_get_active_slot (bar->window);

    if (!NEMO_IS_WINDOW_SLOT (slot))
        return;

    NemoView *view = slot->content_view;

    if (!NEMO_IS_VIEW (view))
        return;

    NemoZoomLevel zoom_level = nemo_view_get_zoom_level (NEMO_VIEW (view));

    gtk_range_set_value (GTK_RANGE (bar->zoom_slider), (double) zoom_level);
}

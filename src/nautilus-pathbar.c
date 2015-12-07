/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* nautilus-pathbar.c
 * Copyright (C) 2004  Red Hat, Inc.,  Jonathan Blandford <jrb@gnome.org>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */


#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "nautilus-pathbar.h"

#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-icon-names.h>
#include <libnautilus-private/nautilus-trash-monitor.h>

#include "nautilus-window-slot-dnd.h"

enum {
        PATH_CLICKED,
        PATH_EVENT,
        LAST_SIGNAL
};

typedef enum {
        NORMAL_BUTTON,
        ROOT_BUTTON,
        HOME_BUTTON,
	MOUNT_BUTTON
} ButtonType;

#define BUTTON_DATA(x) ((ButtonData *)(x))

#define SCROLL_TIMEOUT           150
#define INITIAL_SCROLL_TIMEOUT   300

static guint path_bar_signals [LAST_SIGNAL] = { 0 };

#define NAUTILUS_PATH_BAR_ICON_SIZE 16
#define NAUTILUS_PATH_BAR_BUTTON_MAX_WIDTH 250

typedef struct {
        GtkWidget *button;
        ButtonType type;
        char *dir_name;
        GFile *path;
	NautilusFile *file;
	unsigned int file_changed_signal_id;

        GtkWidget *image;
        GtkWidget *label;
	GtkWidget *bold_label;

        guint ignore_changes : 1;
        guint is_root : 1;
} ButtonData;

struct _NautilusPathBarDetails {
	GdkWindow *event_window;

	GFile *current_path;
	gpointer current_button_data;

	GList *button_list;
	GList *first_scrolled_button;
	GtkWidget *up_slider_button;
	GtkWidget *down_slider_button;
	guint settings_signal_id;
	gint16 slider_width;
	guint timer;
	guint slider_visible : 1;
	guint need_timer : 1;
	guint ignore_click : 1;

	unsigned int drag_slider_timeout;
	gboolean drag_slider_timeout_for_up_button;
};


G_DEFINE_TYPE (NautilusPathBar, nautilus_path_bar,
	       GTK_TYPE_CONTAINER);

static void     nautilus_path_bar_scroll_up                (NautilusPathBar *path_bar);
static void     nautilus_path_bar_scroll_down              (NautilusPathBar *path_bar);
static void     nautilus_path_bar_stop_scrolling           (NautilusPathBar *path_bar);
static gboolean nautilus_path_bar_slider_button_press      (GtkWidget       *widget,
							    GdkEventButton  *event,
							    NautilusPathBar *path_bar);
static gboolean nautilus_path_bar_slider_button_release    (GtkWidget       *widget,
							    GdkEventButton  *event,
							    NautilusPathBar *path_bar);
static void     nautilus_path_bar_check_icon_theme         (NautilusPathBar *path_bar);
static void     nautilus_path_bar_update_button_appearance (ButtonData      *button_data);
static void     nautilus_path_bar_update_button_state      (ButtonData      *button_data,
							    gboolean         current_dir);
static void     nautilus_path_bar_update_path              (NautilusPathBar *path_bar,
							    GFile           *file_path);

static GtkWidget *
get_slider_button (NautilusPathBar  *path_bar,
		   GtkArrowType arrow_type)
{
        GtkWidget *button;

        gtk_widget_push_composite_child ();

        button = gtk_button_new ();
	gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
	gtk_widget_add_events (button, GDK_SCROLL_MASK);
        gtk_container_add (GTK_CONTAINER (button), gtk_arrow_new (arrow_type, GTK_SHADOW_OUT));
        gtk_container_add (GTK_CONTAINER (path_bar), button);
        gtk_widget_show_all (button);

        gtk_widget_pop_composite_child ();

        return button;
}

static gboolean
slider_timeout (gpointer user_data)
{
	NautilusPathBar *path_bar;

	path_bar = NAUTILUS_PATH_BAR (user_data);

	path_bar->priv->drag_slider_timeout = 0;

	if (gtk_widget_get_visible (GTK_WIDGET (path_bar))) {
		if (path_bar->priv->drag_slider_timeout_for_up_button) {
			nautilus_path_bar_scroll_up (path_bar);
		} else {
			nautilus_path_bar_scroll_down (path_bar);
		}
	}

	return FALSE;
}

static void
nautilus_path_bar_slider_drag_motion (GtkWidget      *widget,
				      GdkDragContext *context,
				      int             x,
				      int             y,
				      unsigned int    time,
				      gpointer        user_data)
{
	NautilusPathBar *path_bar;
	GtkSettings *settings;
	unsigned int timeout;

	path_bar = NAUTILUS_PATH_BAR (user_data);

	if (path_bar->priv->drag_slider_timeout == 0) {
		settings = gtk_widget_get_settings (widget);

		g_object_get (settings, "gtk-timeout-expand", &timeout, NULL);
		path_bar->priv->drag_slider_timeout =
			g_timeout_add (timeout,
				       slider_timeout,
				       path_bar);

		path_bar->priv->drag_slider_timeout_for_up_button =
			widget == path_bar->priv->up_slider_button;
	}
}

static void
nautilus_path_bar_slider_drag_leave (GtkWidget      *widget,
				     GdkDragContext *context,
				     unsigned int    time,
				     gpointer        user_data)
{
	NautilusPathBar *path_bar;

	path_bar = NAUTILUS_PATH_BAR (user_data);

	if (path_bar->priv->drag_slider_timeout != 0) {
		g_source_remove (path_bar->priv->drag_slider_timeout);
		path_bar->priv->drag_slider_timeout = 0;
	}
}

static void
nautilus_path_bar_init (NautilusPathBar *path_bar)
{
	path_bar->priv = G_TYPE_INSTANCE_GET_PRIVATE (path_bar, NAUTILUS_TYPE_PATH_BAR, NautilusPathBarDetails);

	gtk_widget_set_has_window (GTK_WIDGET (path_bar), FALSE);
        gtk_widget_set_redraw_on_allocate (GTK_WIDGET (path_bar), FALSE);

        path_bar->priv->up_slider_button = get_slider_button (path_bar, GTK_ARROW_LEFT);
        path_bar->priv->down_slider_button = get_slider_button (path_bar, GTK_ARROW_RIGHT);

        g_signal_connect_swapped (path_bar->priv->up_slider_button, "clicked", G_CALLBACK (nautilus_path_bar_scroll_up), path_bar);
        g_signal_connect_swapped (path_bar->priv->down_slider_button, "clicked", G_CALLBACK (nautilus_path_bar_scroll_down), path_bar);

        g_signal_connect (path_bar->priv->up_slider_button, "button-press-event", G_CALLBACK (nautilus_path_bar_slider_button_press), path_bar);
        g_signal_connect (path_bar->priv->up_slider_button, "button-release-event", G_CALLBACK (nautilus_path_bar_slider_button_release), path_bar);
        g_signal_connect (path_bar->priv->down_slider_button, "button-press-event", G_CALLBACK (nautilus_path_bar_slider_button_press), path_bar);
        g_signal_connect (path_bar->priv->down_slider_button, "button-release-event", G_CALLBACK (nautilus_path_bar_slider_button_release), path_bar);

	gtk_drag_dest_set (GTK_WIDGET (path_bar->priv->up_slider_button),
			   0, NULL, 0, 0);
	gtk_drag_dest_set_track_motion (GTK_WIDGET (path_bar->priv->up_slider_button), TRUE);
	g_signal_connect (path_bar->priv->up_slider_button,
			  "drag-motion",
			  G_CALLBACK (nautilus_path_bar_slider_drag_motion),
			  path_bar);
	g_signal_connect (path_bar->priv->up_slider_button,
			  "drag-leave",
			  G_CALLBACK (nautilus_path_bar_slider_drag_leave),
			  path_bar);

	gtk_drag_dest_set (GTK_WIDGET (path_bar->priv->down_slider_button),
			   0, NULL, 0, 0);
	gtk_drag_dest_set_track_motion (GTK_WIDGET (path_bar->priv->down_slider_button), TRUE);
	g_signal_connect (path_bar->priv->down_slider_button,
			  "drag-motion",
			  G_CALLBACK (nautilus_path_bar_slider_drag_motion),
			  path_bar);
	g_signal_connect (path_bar->priv->down_slider_button,
			  "drag-leave",
			  G_CALLBACK (nautilus_path_bar_slider_drag_leave),
			  path_bar);

	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (path_bar)),
                                     GTK_STYLE_CLASS_LINKED);
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (path_bar)),
                                     "path-bar");
}

static void
nautilus_path_bar_finalize (GObject *object)
{
        NautilusPathBar *path_bar;

        path_bar = NAUTILUS_PATH_BAR (object);

	nautilus_path_bar_stop_scrolling (path_bar);

	if (path_bar->priv->drag_slider_timeout != 0) {
		g_source_remove (path_bar->priv->drag_slider_timeout);
		path_bar->priv->drag_slider_timeout = 0;
	}

        g_list_free (path_bar->priv->button_list);

        G_OBJECT_CLASS (nautilus_path_bar_parent_class)->finalize (object);
}

/* Removes the settings signal handler.  It's safe to call multiple times */
static void
remove_settings_signal (NautilusPathBar *path_bar,
			GdkScreen  *screen)
{
	if (path_bar->priv->settings_signal_id) {
 	 	GtkSettings *settings;
	
 	     	settings = gtk_settings_get_for_screen (screen);
 	     	g_signal_handler_disconnect (settings,
	   				     path_bar->priv->settings_signal_id);
	      	path_bar->priv->settings_signal_id = 0;
        }
}

static void
nautilus_path_bar_dispose (GObject *object)
{
        remove_settings_signal (NAUTILUS_PATH_BAR (object), gtk_widget_get_screen (GTK_WIDGET (object)));

        G_OBJECT_CLASS (nautilus_path_bar_parent_class)->dispose (object);
}

static const char *
get_dir_name (ButtonData *button_data)
{
	if (button_data->type == HOME_BUTTON) {
		return _("Home");
	} else {
		return button_data->dir_name;
	}
}

/* We always want to request the same size for the label, whether
 * or not the contents are bold
 */
static void
set_label_size_request (ButtonData *button_data)
{
        gint width, height;
	GtkRequisition nat_req, bold_req;

	if (button_data->label == NULL) {
		return;
	}

	gtk_widget_get_preferred_size (button_data->label, NULL, &nat_req);
	gtk_widget_get_preferred_size (button_data->bold_label, &bold_req, NULL);

	width = MAX (nat_req.width, bold_req.width);
	width = MIN (width, NAUTILUS_PATH_BAR_BUTTON_MAX_WIDTH);
	height = MAX (nat_req.height, bold_req.height);

	gtk_widget_set_size_request (button_data->label, width, height);
}

/* Size requisition:
 * 
 * Ideally, our size is determined by another widget, and we are just filling
 * available space.
 */
static void
nautilus_path_bar_get_preferred_width (GtkWidget *widget,
				       gint      *minimum,
				       gint      *natural)
{
	ButtonData *button_data;
	NautilusPathBar *path_bar;
	GList *list;
	gint child_height;
	gint height;
	gint child_min, child_nat;

	path_bar = NAUTILUS_PATH_BAR (widget);

	*minimum = *natural = 0;
	height = 0;

	for (list = path_bar->priv->button_list; list; list = list->next) {
		button_data = BUTTON_DATA (list->data);
		set_label_size_request (button_data);

		gtk_widget_get_preferred_width (button_data->button, &child_min, &child_nat);
		gtk_widget_get_preferred_height (button_data->button, &child_height, NULL);
		height = MAX (height, child_height);

		if (button_data->type == NORMAL_BUTTON) {
			/* Use 2*Height as button width because of ellipsized label.  */
			child_min = MAX (child_min, child_height * 2);
			child_nat = MAX (child_min, child_height * 2);
		}

		*minimum = MAX (*minimum, child_min);
		*natural = *natural + child_nat;
	}

	/* Add space for slider, if we have more than one path */
	/* Theoretically, the slider could be bigger than the other button.  But we're
	 * not going to worry about that now.
	 */
	path_bar->priv->slider_width = MIN (height * 2 / 3 + 5, height);

	if (path_bar->priv->button_list && path_bar->priv->button_list->next != NULL) {
		*minimum += (path_bar->priv->slider_width) * 2;
		*natural += (path_bar->priv->slider_width) * 2;
	}
}

static void
nautilus_path_bar_get_preferred_height (GtkWidget *widget,
					gint      *minimum,
					gint      *natural)
{
	ButtonData *button_data;
	NautilusPathBar *path_bar;
	GList *list;
	gint child_min, child_nat;

	path_bar = NAUTILUS_PATH_BAR (widget);

	*minimum = *natural = 0;

	for (list = path_bar->priv->button_list; list; list = list->next) {
		button_data = BUTTON_DATA (list->data);
		set_label_size_request (button_data);

		gtk_widget_get_preferred_height (button_data->button, &child_min, &child_nat);

		*minimum = MAX (*minimum, child_min);
		*natural = MAX (*natural, child_nat);
	}
}

static void
nautilus_path_bar_update_slider_buttons (NautilusPathBar *path_bar)
{
	if (path_bar->priv->button_list) {
                	
      		GtkWidget *button;

	        button = BUTTON_DATA (path_bar->priv->button_list->data)->button;
   		if (gtk_widget_get_child_visible (button)) {
			gtk_widget_set_sensitive (path_bar->priv->down_slider_button, FALSE);
		} else {
			gtk_widget_set_sensitive (path_bar->priv->down_slider_button, TRUE);
		}
       		button = BUTTON_DATA (g_list_last (path_bar->priv->button_list)->data)->button;
                if (gtk_widget_get_child_visible (button)) {
			gtk_widget_set_sensitive (path_bar->priv->up_slider_button, FALSE);
                } else {
			gtk_widget_set_sensitive (path_bar->priv->up_slider_button, TRUE);
		}
	}
}

static void
nautilus_path_bar_unmap (GtkWidget *widget)
{
	nautilus_path_bar_stop_scrolling (NAUTILUS_PATH_BAR (widget));
	gdk_window_hide (NAUTILUS_PATH_BAR (widget)->priv->event_window);

	GTK_WIDGET_CLASS (nautilus_path_bar_parent_class)->unmap (widget);
}

static void
nautilus_path_bar_map (GtkWidget *widget)
{
	gdk_window_show (NAUTILUS_PATH_BAR (widget)->priv->event_window);

	GTK_WIDGET_CLASS (nautilus_path_bar_parent_class)->map (widget);
}


static void
child_ordering_changed (NautilusPathBar *path_bar)
{
	GList *l;

	if (path_bar->priv->up_slider_button) {
		gtk_style_context_invalidate (gtk_widget_get_style_context (path_bar->priv->up_slider_button));
	}
	if (path_bar->priv->down_slider_button) {
		gtk_style_context_invalidate (gtk_widget_get_style_context (path_bar->priv->down_slider_button));
	}

	for (l = path_bar->priv->button_list; l; l = l->next) {
		ButtonData *data = l->data;
		gtk_style_context_invalidate (gtk_widget_get_style_context (data->button));		
	}
}

/* This is a tad complicated */
static void
nautilus_path_bar_size_allocate (GtkWidget     *widget,
			    	 GtkAllocation *allocation)
{
        GtkWidget *child;
        NautilusPathBar *path_bar;
        GtkTextDirection direction;
        GtkAllocation child_allocation;
        GList *list, *first_button;
        gint width;
        gint largest_width;
        gboolean need_sliders;
        gint up_slider_offset;
        gint down_slider_offset;
	GtkRequisition child_requisition;
	gboolean needs_reorder = FALSE;

	need_sliders = FALSE;
	up_slider_offset = 0;
	down_slider_offset = 0;
	path_bar = NAUTILUS_PATH_BAR (widget);

	gtk_widget_set_allocation (widget, allocation);

	if (gtk_widget_get_realized (widget)) {
		gdk_window_move_resize (path_bar->priv->event_window,
					allocation->x, allocation->y,
					allocation->width, allocation->height);
	}

        /* No path is set so we don't have to allocate anything. */
        if (path_bar->priv->button_list == NULL) {
                return;
	}
        direction = gtk_widget_get_direction (widget);

  	/* First, we check to see if we need the scrollbars. */
	width = 0;

	gtk_widget_get_preferred_size (BUTTON_DATA (path_bar->priv->button_list->data)->button,
				       &child_requisition, NULL);
	width += child_requisition.width;

        for (list = path_bar->priv->button_list->next; list; list = list->next) {
        	child = BUTTON_DATA (list->data)->button;
		gtk_widget_get_preferred_size (child, &child_requisition, NULL);
                width += child_requisition.width;
        }

        if (width <= allocation->width) {
		first_button = g_list_last (path_bar->priv->button_list);
        } else {
                gboolean reached_end;
                gint slider_space;
		reached_end = FALSE;
		slider_space = 2 * (path_bar->priv->slider_width);

                if (path_bar->priv->first_scrolled_button) {
			first_button = path_bar->priv->first_scrolled_button;
		} else {
			first_button = path_bar->priv->button_list;
                }        

		need_sliders = TRUE;
      		/* To see how much space we have, and how many buttons we can display.
       		* We start at the first button, count forward until hit the new
       		* button, then count backwards.
       		*/
      		/* Count down the path chain towards the end. */
		gtk_widget_get_preferred_size (BUTTON_DATA (first_button->data)->button,
					       &child_requisition, NULL);
                width = child_requisition.width;
                list = first_button->prev;
                while (list && !reached_end) {
	  		child = BUTTON_DATA (list->data)->button;
			gtk_widget_get_preferred_size (child, &child_requisition, NULL);

	  		if (width + child_requisition.width + slider_space > allocation->width) {
	    			reached_end = TRUE;
	  		} else {
				width += child_requisition.width;
			}

	  		list = list->prev;
		}

                /* Finally, we walk up, seeing how many of the previous buttons we can add*/

                while (first_button->next && ! reached_end) {
	  		child = BUTTON_DATA (first_button->next->data)->button;
			gtk_widget_get_preferred_size (child, &child_requisition, NULL);

	  		if (width + child_requisition.width + slider_space > allocation->width) {
	      			reached_end = TRUE;
	    		} else {
	      			width += child_requisition.width;
	      			first_button = first_button->next;
	    		}
		}
        }

        /* Now, we allocate space to the buttons */
        child_allocation.y = allocation->y;
        child_allocation.height = allocation->height;

        if (direction == GTK_TEXT_DIR_RTL) {
                child_allocation.x = allocation->x + allocation->width;
                if (need_sliders) {
	  		child_allocation.x -= path_bar->priv->slider_width;
	  		up_slider_offset = allocation->width - path_bar->priv->slider_width;
		}
        } else {
                child_allocation.x = allocation->x;
                if (need_sliders) {
	  		up_slider_offset = 0;
	  		child_allocation.x += path_bar->priv->slider_width;
		}
        }

        /* Determine the largest possible allocation size */
        largest_width = allocation->width;
        if (need_sliders) {
		largest_width -= (path_bar->priv->slider_width) * 2;
        }

        for (list = first_button; list; list = list->prev) {
                child = BUTTON_DATA (list->data)->button;
		gtk_widget_get_preferred_size (child, &child_requisition, NULL);

                child_allocation.width = MIN (child_requisition.width, largest_width);
                if (direction == GTK_TEXT_DIR_RTL) {
			child_allocation.x -= child_allocation.width;
		}
                /* Check to see if we've don't have any more space to allocate buttons */
                if (need_sliders && direction == GTK_TEXT_DIR_RTL) {
	  		if (child_allocation.x - path_bar->priv->slider_width < allocation->x) {
			    break;
			}
		} else {
			if (need_sliders && direction == GTK_TEXT_DIR_LTR) {
	  			if (child_allocation.x + child_allocation.width + path_bar->priv->slider_width > allocation->x + allocation->width) {
	    				break;	
				}	
			}
		}

		needs_reorder |= gtk_widget_get_child_visible (child) == FALSE;
                gtk_widget_set_child_visible (child, TRUE);
                gtk_widget_size_allocate (child, &child_allocation);

                if (direction == GTK_TEXT_DIR_RTL) {
	  		down_slider_offset = child_allocation.x - allocation->x - path_bar->priv->slider_width;
		} else {
	  		down_slider_offset += child_allocation.width;
	  		child_allocation.x += child_allocation.width;
		}
        }
        /* Now we go hide all the widgets that don't fit */
        while (list) {
                child = BUTTON_DATA (list->data)->button;
		needs_reorder |= gtk_widget_get_child_visible (child) == TRUE;
        	gtk_widget_set_child_visible (child, FALSE);
                list = list->prev;
        }
        for (list = first_button->next; list; list = list->next) {
                child = BUTTON_DATA (list->data)->button;
		needs_reorder |= gtk_widget_get_child_visible (child) == TRUE;
 	        gtk_widget_set_child_visible (child, FALSE);
        }

        if (need_sliders) {
                child_allocation.width = path_bar->priv->slider_width;
                child_allocation.x = up_slider_offset + allocation->x;
                gtk_widget_size_allocate (path_bar->priv->up_slider_button, &child_allocation);

		needs_reorder |= gtk_widget_get_child_visible (path_bar->priv->up_slider_button) == FALSE;
                gtk_widget_set_child_visible (path_bar->priv->up_slider_button, TRUE);
                gtk_widget_show_all (path_bar->priv->up_slider_button);

		if (direction == GTK_TEXT_DIR_LTR) {
			down_slider_offset += path_bar->priv->slider_width;
		}
        } else {
		needs_reorder |= gtk_widget_get_child_visible (path_bar->priv->up_slider_button) == TRUE;
        	gtk_widget_set_child_visible (path_bar->priv->up_slider_button, FALSE);
        }
	
	if (need_sliders) {
    	        child_allocation.width = path_bar->priv->slider_width;
        	child_allocation.x = down_slider_offset + allocation->x;
        	gtk_widget_size_allocate (path_bar->priv->down_slider_button, &child_allocation);

		needs_reorder |= gtk_widget_get_child_visible (path_bar->priv->down_slider_button) == FALSE;
      		gtk_widget_set_child_visible (path_bar->priv->down_slider_button, TRUE);
      		gtk_widget_show_all (path_bar->priv->down_slider_button);
      		nautilus_path_bar_update_slider_buttons (path_bar);
    	} else {
		needs_reorder |= gtk_widget_get_child_visible (path_bar->priv->down_slider_button) == TRUE;
    		gtk_widget_set_child_visible (path_bar->priv->down_slider_button, FALSE);
	}

	if (needs_reorder) {
		child_ordering_changed (path_bar);
	}
}

static void
nautilus_path_bar_style_updated (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (nautilus_path_bar_parent_class)->style_updated (widget);

        nautilus_path_bar_check_icon_theme (NAUTILUS_PATH_BAR (widget));
}

static void
nautilus_path_bar_screen_changed (GtkWidget *widget,
			          GdkScreen *previous_screen)
{
        if (GTK_WIDGET_CLASS (nautilus_path_bar_parent_class)->screen_changed) {
                GTK_WIDGET_CLASS (nautilus_path_bar_parent_class)->screen_changed (widget, previous_screen);
	}
        /* We might nave a new settings, so we remove the old one */
        if (previous_screen) {
                remove_settings_signal (NAUTILUS_PATH_BAR (widget), previous_screen);
	}
        nautilus_path_bar_check_icon_theme (NAUTILUS_PATH_BAR (widget));
}

static gboolean
nautilus_path_bar_scroll (GtkWidget      *widget,
			  GdkEventScroll *event)
{
	NautilusPathBar *path_bar;

	path_bar = NAUTILUS_PATH_BAR (widget);

	switch (event->direction) {
		case GDK_SCROLL_RIGHT:
		case GDK_SCROLL_DOWN:
			nautilus_path_bar_scroll_down (path_bar);
			return TRUE;

		case GDK_SCROLL_LEFT:
		case GDK_SCROLL_UP:
			nautilus_path_bar_scroll_up (path_bar);
			return TRUE;
		case GDK_SCROLL_SMOOTH:
			break;
	}

	return FALSE;
}

static void
nautilus_path_bar_realize (GtkWidget *widget)
{
	NautilusPathBar *path_bar;
	GtkAllocation allocation;
	GdkWindow *window;
	GdkWindowAttr attributes;
	gint attributes_mask;

	gtk_widget_set_realized (widget, TRUE);

	path_bar = NAUTILUS_PATH_BAR (widget);
	window = gtk_widget_get_parent_window (widget);
	gtk_widget_set_window (widget, window);
	g_object_ref (window);

	gtk_widget_get_allocation (widget, &allocation);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.wclass = GDK_INPUT_ONLY;
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= 
		GDK_SCROLL_MASK |
		GDK_BUTTON_PRESS_MASK |
		GDK_BUTTON_RELEASE_MASK |
		GDK_POINTER_MOTION_MASK;
	attributes_mask = GDK_WA_X | GDK_WA_Y;

	path_bar->priv->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
						 &attributes, attributes_mask);
	gdk_window_set_user_data (path_bar->priv->event_window, widget);
}

static void
nautilus_path_bar_unrealize (GtkWidget *widget)
{
	NautilusPathBar *path_bar;

	path_bar = NAUTILUS_PATH_BAR (widget);

	gdk_window_set_user_data (path_bar->priv->event_window, NULL);
	gdk_window_destroy (path_bar->priv->event_window);
	path_bar->priv->event_window = NULL;

	GTK_WIDGET_CLASS (nautilus_path_bar_parent_class)->unrealize (widget);
}

static void
nautilus_path_bar_add (GtkContainer *container,
		       GtkWidget    *widget)
{
        gtk_widget_set_parent (widget, GTK_WIDGET (container));
}

static void
nautilus_path_bar_remove_1 (GtkContainer *container,
		       	    GtkWidget    *widget)
{
        gboolean was_visible = gtk_widget_get_visible (widget);
        gtk_widget_unparent (widget);
        if (was_visible) {
                gtk_widget_queue_resize (GTK_WIDGET (container));
	}
}

static void
nautilus_path_bar_remove (GtkContainer *container,
		          GtkWidget    *widget)
{
        NautilusPathBar *path_bar;
        GList *children;

        path_bar = NAUTILUS_PATH_BAR (container);

        if (widget == path_bar->priv->up_slider_button) {
                nautilus_path_bar_remove_1 (container, widget);
                path_bar->priv->up_slider_button = NULL;
                return;
        }

        if (widget == path_bar->priv->down_slider_button) {
                nautilus_path_bar_remove_1 (container, widget);
                path_bar->priv->down_slider_button = NULL;
                return;
        }

        children = path_bar->priv->button_list;
        while (children) {              
                if (widget == BUTTON_DATA (children->data)->button) {
			nautilus_path_bar_remove_1 (container, widget);
	  		path_bar->priv->button_list = g_list_remove_link (path_bar->priv->button_list, children);
	  		g_list_free_1 (children);
	  		return;
		}
                children = children->next;
        }
}

static void
nautilus_path_bar_forall (GtkContainer *container,
		     	  gboolean      include_internals,
		     	  GtkCallback   callback,
		     	  gpointer      callback_data)
{
        NautilusPathBar *path_bar;
        GList *children;

        g_return_if_fail (callback != NULL);
        path_bar = NAUTILUS_PATH_BAR (container);

        children = path_bar->priv->button_list;
        while (children) {
               GtkWidget *child;
               child = BUTTON_DATA (children->data)->button;
                children = children->next;
                (* callback) (child, callback_data);
        }

        if (path_bar->priv->up_slider_button) {
                (* callback) (path_bar->priv->up_slider_button, callback_data);
	}

        if (path_bar->priv->down_slider_button) {
                (* callback) (path_bar->priv->down_slider_button, callback_data);
	}
}

static void
nautilus_path_bar_grab_notify (GtkWidget *widget,
			       gboolean   was_grabbed)
{
        if (!was_grabbed) {
                nautilus_path_bar_stop_scrolling (NAUTILUS_PATH_BAR (widget));
	}
}

static void
nautilus_path_bar_state_changed (GtkWidget    *widget,
			         GtkStateType  previous_state)
{
        if (!gtk_widget_get_sensitive (widget)) {
                nautilus_path_bar_stop_scrolling (NAUTILUS_PATH_BAR (widget));
	}
}

static GtkWidgetPath *
nautilus_path_bar_get_path_for_child (GtkContainer *container,
				      GtkWidget    *child)
{
	NautilusPathBar *path_bar = NAUTILUS_PATH_BAR (container);
	GtkWidgetPath *path;

	path = gtk_widget_path_copy (gtk_widget_get_path (GTK_WIDGET (path_bar)));

	if (gtk_widget_get_visible (child) &&
	    gtk_widget_get_child_visible (child)) {
		GtkWidgetPath *sibling_path;
		GList *visible_children;
		GList *l;
		int pos;

		/* 1. Build the list of visible children, in visually left-to-right order
		 * (i.e. independently of the widget's direction).  Note that our
		 * button_list is stored in innermost-to-outermost path order!
		 */

		visible_children = NULL;

		if (gtk_widget_get_visible (path_bar->priv->down_slider_button) &&
		    gtk_widget_get_child_visible (path_bar->priv->down_slider_button)) {
			visible_children = g_list_prepend (visible_children, path_bar->priv->down_slider_button);
		}

		for (l = path_bar->priv->button_list; l; l = l->next) {
			ButtonData *data = l->data;
				
			if (gtk_widget_get_visible (data->button) &&
			    gtk_widget_get_child_visible (data->button))
				visible_children = g_list_prepend (visible_children, data->button);
		}

		if (gtk_widget_get_visible (path_bar->priv->up_slider_button) &&
		    gtk_widget_get_child_visible (path_bar->priv->up_slider_button)) {
			visible_children = g_list_prepend (visible_children, path_bar->priv->up_slider_button);
		}

		if (gtk_widget_get_direction (GTK_WIDGET (path_bar)) == GTK_TEXT_DIR_RTL) {
			visible_children = g_list_reverse (visible_children);
		}

		/* 2. Find the index of the child within that list */

		pos = 0;

		for (l = visible_children; l; l = l->next) {
			GtkWidget *button = l->data;

			if (button == child) {
				break;
			}

			pos++;
		}

		/* 3. Build the path */

		sibling_path = gtk_widget_path_new ();

		for (l = visible_children; l; l = l->next) {
			gtk_widget_path_append_for_widget (sibling_path, l->data);
		}

		gtk_widget_path_append_with_siblings (path, sibling_path, pos);

		g_list_free (visible_children);
		gtk_widget_path_unref (sibling_path);
	} else {
		gtk_widget_path_append_for_widget (path, child);
	}

	return path;
}

static void
nautilus_path_bar_class_init (NautilusPathBarClass *path_bar_class)
{
        GObjectClass *gobject_class;
        GtkWidgetClass *widget_class;
        GtkContainerClass *container_class;

        gobject_class = (GObjectClass *) path_bar_class;
        widget_class = (GtkWidgetClass *) path_bar_class;
        container_class = (GtkContainerClass *) path_bar_class;

        gobject_class->finalize = nautilus_path_bar_finalize;
        gobject_class->dispose = nautilus_path_bar_dispose;

	widget_class->get_preferred_height = nautilus_path_bar_get_preferred_height;
	widget_class->get_preferred_width = nautilus_path_bar_get_preferred_width;
	widget_class->realize = nautilus_path_bar_realize;
	widget_class->unrealize = nautilus_path_bar_unrealize;
	widget_class->unmap = nautilus_path_bar_unmap;
	widget_class->map = nautilus_path_bar_map;
        widget_class->size_allocate = nautilus_path_bar_size_allocate;
        widget_class->style_updated = nautilus_path_bar_style_updated;
        widget_class->screen_changed = nautilus_path_bar_screen_changed;
        widget_class->grab_notify = nautilus_path_bar_grab_notify;
        widget_class->state_changed = nautilus_path_bar_state_changed;
	widget_class->scroll_event = nautilus_path_bar_scroll;

        container_class->add = nautilus_path_bar_add;
        container_class->forall = nautilus_path_bar_forall;
        container_class->remove = nautilus_path_bar_remove;
	container_class->get_path_for_child = nautilus_path_bar_get_path_for_child;

        path_bar_signals [PATH_CLICKED] =
                g_signal_new ("path-clicked",
		  G_OBJECT_CLASS_TYPE (path_bar_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (NautilusPathBarClass, path_clicked),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1,
		  G_TYPE_FILE);
        path_bar_signals [PATH_EVENT] =
                g_signal_new ("path-event",
		  G_OBJECT_CLASS_TYPE (path_bar_class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (NautilusPathBarClass, path_event),
		  NULL, NULL, NULL,
		  G_TYPE_BOOLEAN, 2,
		  G_TYPE_FILE,
		  GDK_TYPE_EVENT);

	 gtk_container_class_handle_border_width (container_class);
	 g_type_class_add_private (path_bar_class, sizeof (NautilusPathBarDetails));
}

static void
nautilus_path_bar_scroll_down (NautilusPathBar *path_bar)
{
        GList *list;
        GList *down_button;
        GList *up_button;
        gint space_available;
        gint space_needed;
        GtkTextDirection direction;
	GtkAllocation allocation, button_allocation, slider_allocation;

	down_button = NULL;
	up_button = NULL;

        if (path_bar->priv->ignore_click) {
                path_bar->priv->ignore_click = FALSE;
                return;   
        }

        gtk_widget_queue_resize (GTK_WIDGET (path_bar));

        direction = gtk_widget_get_direction (GTK_WIDGET (path_bar));
  
        /* We find the button at the 'down' end that we have to make */
        /* visible */
        for (list = path_bar->priv->button_list; list; list = list->next) {
        	if (list->next && gtk_widget_get_child_visible (BUTTON_DATA (list->next->data)->button)) {
			down_button = list;
	  		break;
		}
        }

	if (down_button == NULL) {
		return;
	}
  
        /* Find the last visible button on the 'up' end */
        for (list = g_list_last (path_bar->priv->button_list); list; list = list->prev) {
                if (gtk_widget_get_child_visible (BUTTON_DATA (list->data)->button)) {
	  		up_button = list;
	  		break;
		}
        }

	gtk_widget_get_allocation (BUTTON_DATA (down_button->data)->button, &button_allocation);
	gtk_widget_get_allocation (GTK_WIDGET (path_bar), &allocation);
	gtk_widget_get_allocation (path_bar->priv->down_slider_button, &slider_allocation);

        space_needed = button_allocation.width;
        if (direction == GTK_TEXT_DIR_RTL) {
                space_available = slider_allocation.x - allocation.x;
	} else {
                space_available = (allocation.x + allocation.width) -
                        (slider_allocation.x + slider_allocation.width);
	}

  	/* We have space_available extra space that's not being used.  We
   	* need space_needed space to make the button fit.  So we walk down
   	* from the end, removing buttons until we get all the space we
   	* need. */
	gtk_widget_get_allocation (BUTTON_DATA (up_button->data)->button, &button_allocation);
        while ((space_available < space_needed) &&
	       (up_button != NULL)) {
                space_available += button_allocation.width;
                up_button = up_button->prev;
                path_bar->priv->first_scrolled_button = up_button;
        }
}

static void
nautilus_path_bar_scroll_up (NautilusPathBar *path_bar)
{
        GList *list;

        if (path_bar->priv->ignore_click) {
                path_bar->priv->ignore_click = FALSE;
                return;   
        }

        gtk_widget_queue_resize (GTK_WIDGET (path_bar));

        for (list = g_list_last (path_bar->priv->button_list); list; list = list->prev) {
                if (list->prev && gtk_widget_get_child_visible (BUTTON_DATA (list->prev->data)->button)) {
			path_bar->priv->first_scrolled_button = list;
	  		return;
		}
        }
}

static gboolean
nautilus_path_bar_scroll_timeout (NautilusPathBar *path_bar)
{
        gboolean retval = FALSE;

        if (path_bar->priv->timer) {
                if (gtk_widget_has_focus (path_bar->priv->up_slider_button)) {
			nautilus_path_bar_scroll_up (path_bar);
		} else {
			if (gtk_widget_has_focus (path_bar->priv->down_slider_button)) {
				nautilus_path_bar_scroll_down (path_bar);
			}
         	}
         	if (path_bar->priv->need_timer) {
			path_bar->priv->need_timer = FALSE;

	  		path_bar->priv->timer = 
				g_timeout_add (SCROLL_TIMEOUT,
					       (GSourceFunc) nautilus_path_bar_scroll_timeout,
					       path_bar);
	  
		} else {
			retval = TRUE;
		}
        }

        return retval;
}

static void 
nautilus_path_bar_stop_scrolling (NautilusPathBar *path_bar)
{
        if (path_bar->priv->timer) {
                g_source_remove (path_bar->priv->timer);
                path_bar->priv->timer = 0;
                path_bar->priv->need_timer = FALSE;
        }
}

static gboolean
nautilus_path_bar_slider_button_press (GtkWidget       *widget, 
	   			       GdkEventButton  *event,
				       NautilusPathBar *path_bar)
{
        if (!gtk_widget_has_focus (widget)) {
                gtk_widget_grab_focus (widget);
	}

        if (event->type != GDK_BUTTON_PRESS || event->button != 1) {
                return FALSE;
	}

        path_bar->priv->ignore_click = FALSE;

        if (widget == path_bar->priv->up_slider_button) {
                nautilus_path_bar_scroll_up (path_bar);
	} else {
		if (widget == path_bar->priv->down_slider_button) {
                       nautilus_path_bar_scroll_down (path_bar);
		}
	}

        if (!path_bar->priv->timer) {
                path_bar->priv->need_timer = TRUE;
                path_bar->priv->timer = 
			g_timeout_add (INITIAL_SCROLL_TIMEOUT,
				       (GSourceFunc) nautilus_path_bar_scroll_timeout,
				       path_bar);
        }

        return FALSE;
}

static gboolean
nautilus_path_bar_slider_button_release (GtkWidget      *widget, 
  				         GdkEventButton *event,
				         NautilusPathBar     *path_bar)
{
        if (event->type != GDK_BUTTON_RELEASE) {
                return FALSE;
	}

        path_bar->priv->ignore_click = TRUE;
        nautilus_path_bar_stop_scrolling (path_bar);

        return FALSE;
}


/* Changes the icons wherever it is needed */
static void
reload_icons (NautilusPathBar *path_bar)
{
        GList *list;

        for (list = path_bar->priv->button_list; list; list = list->next) {
                ButtonData *button_data;

                button_data = BUTTON_DATA (list->data);
		if (button_data->type != NORMAL_BUTTON || button_data->is_root) {
                	nautilus_path_bar_update_button_appearance (button_data);
		}

        }
}

/* Callback used when a GtkSettings value changes */
static void
settings_notify_cb (GObject    *object,
		    GParamSpec *pspec,
		    NautilusPathBar *path_bar)
{
        const char *name;

        name = g_param_spec_get_name (pspec);

      	if (! strcmp (name, "gtk-icon-theme-name") || ! strcmp (name, "gtk-icon-sizes")) {
	      reload_icons (path_bar);
	}
}

static void
nautilus_path_bar_check_icon_theme (NautilusPathBar *path_bar)
{
        GtkSettings *settings;

        if (path_bar->priv->settings_signal_id) {
                return;
	}

        settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (path_bar)));
        path_bar->priv->settings_signal_id = g_signal_connect (settings, "notify", G_CALLBACK (settings_notify_cb), path_bar);

        reload_icons (path_bar);
}

/* Public functions and their helpers */
static void
nautilus_path_bar_clear_buttons (NautilusPathBar *path_bar)
{
        while (path_bar->priv->button_list != NULL) {
                gtk_container_remove (GTK_CONTAINER (path_bar), BUTTON_DATA (path_bar->priv->button_list->data)->button);
        }
        path_bar->priv->first_scrolled_button = NULL;
}

static void
button_clicked_cb (GtkWidget *button,
		   gpointer   data)
{
        ButtonData *button_data;
        NautilusPathBar *path_bar;
        GList *button_list;

        button_data = BUTTON_DATA (data);
        if (button_data->ignore_changes) {
                return;
	}

        path_bar = NAUTILUS_PATH_BAR (gtk_widget_get_parent (button));

        button_list = g_list_find (path_bar->priv->button_list, button_data);
        g_assert (button_list != NULL);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

        g_signal_emit (path_bar, path_bar_signals [PATH_CLICKED], 0, button_data->path);
}

static gboolean
button_event_cb (GtkWidget *button,
		 GdkEventButton *event,
		 gpointer   data)
{
        ButtonData *button_data;
        NautilusPathBar *path_bar;
        GList *button_list;
	gboolean retval;

        button_data = BUTTON_DATA (data);
        path_bar = NAUTILUS_PATH_BAR (gtk_widget_get_parent (button));

	if (event->type == GDK_BUTTON_PRESS) {
		g_object_set_data (G_OBJECT (button), "handle-button-release",
				   GINT_TO_POINTER (TRUE));
	}

	if (event->type == GDK_BUTTON_RELEASE &&
	    !GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button),
						  "handle-button-release"))) {
		return FALSE;
	}

        button_list = g_list_find (path_bar->priv->button_list, button_data);
        g_assert (button_list != NULL);

        g_signal_emit (path_bar, path_bar_signals [PATH_EVENT], 0, button_data->path, event, &retval);

	return retval;
}

static void
button_drag_begin_cb (GtkWidget *widget,
		      GdkDragContext *drag_context,
		      gpointer user_data)
{
	g_object_set_data (G_OBJECT (widget), "handle-button-release",
			   GINT_TO_POINTER (FALSE));
}

static GIcon *
get_gicon_for_mount (ButtonData *button_data)
{
	GIcon *icon;
	GMount *mount;

	icon = NULL;
	mount = nautilus_get_mounted_mount_for_root (button_data->path);

	if (mount != NULL) {
		icon = g_mount_get_symbolic_icon (mount);
		g_object_unref (mount);
	}

	return icon;
}

static GIcon *
get_gicon (ButtonData *button_data)
{
	switch (button_data->type)
        {
		case ROOT_BUTTON:
			return g_themed_icon_new (NAUTILUS_ICON_FILESYSTEM);
		case HOME_BUTTON:
			return g_themed_icon_new (NAUTILUS_ICON_HOME);
		case MOUNT_BUTTON:
			return get_gicon_for_mount (button_data);
		default:
			return NULL;
        }
  
       	return NULL;
}

static void
button_data_free (ButtonData *button_data)
{
        g_object_unref (button_data->path);
        g_free (button_data->dir_name);
	if (button_data->file != NULL) {
		g_signal_handler_disconnect (button_data->file,
					     button_data->file_changed_signal_id);
		nautilus_file_monitor_remove (button_data->file, button_data);
		nautilus_file_unref (button_data->file);
	}

        g_free (button_data);
}

static void
nautilus_path_bar_update_button_appearance (ButtonData *button_data)
{
        const gchar *dir_name = get_dir_name (button_data);
	GIcon *icon;

        if (button_data->label != NULL) {
		char *markup;

		markup = g_markup_printf_escaped ("<b>%s</b>", dir_name);

                if (gtk_label_get_use_markup (GTK_LABEL (button_data->label))) {
	  		gtk_label_set_markup (GTK_LABEL (button_data->label), markup);
		} else {
			gtk_label_set_text (GTK_LABEL (button_data->label), dir_name);
		}

		gtk_label_set_markup (GTK_LABEL (button_data->bold_label), markup);
		g_free (markup);
        }

	icon = get_gicon (button_data);
	if (icon != NULL) {
		gtk_image_set_from_gicon (GTK_IMAGE (button_data->image), icon, GTK_ICON_SIZE_MENU);
		gtk_widget_show (GTK_WIDGET (button_data->image));
		g_object_unref (icon);
	} else {
		gtk_widget_hide (GTK_WIDGET (button_data->image));
	}
}

static void
nautilus_path_bar_update_button_state (ButtonData *button_data,
				       gboolean    current_dir)
{
	if (button_data->label != NULL) {
		gtk_label_set_label (GTK_LABEL (button_data->label), NULL);
		gtk_label_set_label (GTK_LABEL (button_data->bold_label), NULL);
		gtk_label_set_use_markup (GTK_LABEL (button_data->label), current_dir);
	}

	nautilus_path_bar_update_button_appearance (button_data);

        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_data->button)) != current_dir) {
                button_data->ignore_changes = TRUE;
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_data->button), current_dir);
                button_data->ignore_changes = FALSE;
        }
}

static void
setup_button_type (ButtonData       *button_data,
		   NautilusPathBar  *path_bar,
		   GFile *location)
{
	GMount *mount;

	if (nautilus_is_root_directory (location)) {
		button_data->type = ROOT_BUTTON;
	} else if (nautilus_is_home_directory (location)) {
		button_data->type = HOME_BUTTON;
		button_data->is_root = TRUE;
	} else if ((mount = nautilus_get_mounted_mount_for_root (location)) != NULL) {
		button_data->dir_name = g_mount_get_name (mount);
		button_data->type = MOUNT_BUTTON;
		button_data->is_root = TRUE;

		g_object_unref (mount);
	} else {
		button_data->type = NORMAL_BUTTON;
	}
}

static void
button_drag_data_get_cb (GtkWidget          *widget,
			 GdkDragContext     *context,
			 GtkSelectionData   *selection_data,
			 guint               info,
			 guint               time_,
			 gpointer            user_data)
{
        ButtonData *button_data;
        char *uri_list[2];
	char *tmp;

	button_data = user_data;

	uri_list[0] = g_file_get_uri (button_data->path);
	uri_list[1] = NULL;

	if (info == NAUTILUS_ICON_DND_GNOME_ICON_LIST) {
		tmp = g_strdup_printf ("%s\r\n", uri_list[0]);
		gtk_selection_data_set (selection_data, gtk_selection_data_get_target (selection_data),
					8, (const guchar *) tmp, strlen (tmp));
		g_free (tmp);
	} else if (info == NAUTILUS_ICON_DND_URI_LIST) {
		gtk_selection_data_set_uris (selection_data, uri_list);
	}

	g_free (uri_list[0]);
}

static void
setup_button_drag_source (ButtonData *button_data)
{
	GtkTargetList *target_list;
	const GtkTargetEntry targets[] = {
		{ NAUTILUS_ICON_DND_GNOME_ICON_LIST_TYPE, 0, NAUTILUS_ICON_DND_GNOME_ICON_LIST }
	};

        gtk_drag_source_set (button_data->button,
		       	     GDK_BUTTON1_MASK |
			     GDK_BUTTON2_MASK,
		       	     NULL, 0,
			     GDK_ACTION_MOVE |
			     GDK_ACTION_COPY |
			     GDK_ACTION_LINK |
			     GDK_ACTION_ASK);

	target_list = gtk_target_list_new (targets, G_N_ELEMENTS (targets));
	gtk_target_list_add_uri_targets (target_list, NAUTILUS_ICON_DND_URI_LIST);
	gtk_drag_source_set_target_list (button_data->button, target_list);
	gtk_target_list_unref (target_list);

        g_signal_connect (button_data->button, "drag-data-get",
			  G_CALLBACK (button_drag_data_get_cb),
			  button_data);
}

static void
button_data_file_changed (NautilusFile *file,
			  ButtonData *button_data)
{
	GFile *location, *current_location, *parent, *button_parent;
	ButtonData *current_button_data;
	char *display_name;
	NautilusPathBar *path_bar;
	gboolean renamed, child;

	path_bar = (NautilusPathBar *) gtk_widget_get_ancestor (button_data->button,
								NAUTILUS_TYPE_PATH_BAR);
	if (path_bar == NULL) {
		return;
	}

	g_assert (path_bar->priv->current_path != NULL);
	g_assert (path_bar->priv->current_button_data != NULL);

	current_button_data = path_bar->priv->current_button_data;

	location = nautilus_file_get_location (file);
	if (!g_file_equal (button_data->path, location)) {
		parent = g_file_get_parent (location);
		button_parent = g_file_get_parent (button_data->path);

		renamed = (parent != NULL && button_parent != NULL) &&
			   g_file_equal (parent, button_parent);

		if (parent != NULL) {
			g_object_unref (parent);
		}
		if (button_parent != NULL) {
			g_object_unref (button_parent);
		}

		if (renamed) {
			button_data->path = g_object_ref (location);
		} else {
			/* the file has been moved.
			 * If it was below the currently displayed location, remove it.
			 * If it was not below the currently displayed location, update the path bar
			 */
			child = g_file_has_prefix (button_data->path,
						   path_bar->priv->current_path);

			if (child) {
				/* moved file inside current path hierarchy */
				g_object_unref (location);
				location = g_file_get_parent (button_data->path);
				current_location = g_object_ref (path_bar->priv->current_path);
			} else {
				/* moved current path, or file outside current path hierarchy.
				 * Update path bar to new locations.
				 */
				current_location = nautilus_file_get_location (current_button_data->file);
			}

        		nautilus_path_bar_update_path (path_bar, location);
        		nautilus_path_bar_set_path (path_bar, current_location);
			g_object_unref (location);
			g_object_unref (current_location);
			return;
		}
	} else if (nautilus_file_is_gone (file)) {
		gint idx, position;

		/* if the current or a parent location are gone, clear all the buttons,
		 * the view will set the new path.
		 */
		current_location = nautilus_file_get_location (current_button_data->file);

		if (g_file_has_prefix (current_location, location) ||
		    g_file_equal (current_location, location)) {
			nautilus_path_bar_clear_buttons (path_bar);
		} else if (g_file_has_prefix (location, current_location)) {
			/* remove this and the following buttons */
			position = g_list_position (path_bar->priv->button_list,
						    g_list_find (path_bar->priv->button_list, button_data));

			if (position != -1) {
				for (idx = 0; idx <= position; idx++) {
					gtk_container_remove (GTK_CONTAINER (path_bar),
							      BUTTON_DATA (path_bar->priv->button_list->data)->button);
				}
			}
		}

		g_object_unref (current_location);
		g_object_unref (location);
		return;
	}
	g_object_unref (location);

	/* MOUNTs use the GMount as the name, so don't update for those */
	if (button_data->type != MOUNT_BUTTON) {
		display_name = nautilus_file_get_display_name (file);
		if (g_strcmp0 (display_name, button_data->dir_name) != 0) {
			g_free (button_data->dir_name);
			button_data->dir_name = g_strdup (display_name);
		}

		g_free (display_name);
	}
	nautilus_path_bar_update_button_appearance (button_data);
}

static ButtonData *
make_button_data (NautilusPathBar  *path_bar,
		  NautilusFile     *file,
		  gboolean          current_dir)
{
	GFile *path;
        GtkWidget *child;
        ButtonData *button_data;

	path = nautilus_file_get_location (file);
	child = NULL;

        /* Is it a special button? */
        button_data = g_new0 (ButtonData, 1);

        setup_button_type (button_data, path_bar, path);
        button_data->button = gtk_toggle_button_new ();
	gtk_style_context_add_class (gtk_widget_get_style_context (button_data->button),
				     "text-button");
	gtk_button_set_focus_on_click (GTK_BUTTON (button_data->button), FALSE);
	gtk_widget_add_events (button_data->button, GDK_SCROLL_MASK);
	/* TODO update button type when xdg directories change */

	button_data->image = gtk_image_new ();

        switch (button_data->type) {
                case ROOT_BUTTON:
                        child = button_data->image;
                        button_data->label = NULL;
                        break;
                case HOME_BUTTON:
		case MOUNT_BUTTON:
		case NORMAL_BUTTON:
    		default:
			button_data->label = gtk_label_new (NULL);
                        child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
			gtk_box_pack_start (GTK_BOX (child), button_data->image, FALSE, FALSE, 0);
			gtk_box_pack_start (GTK_BOX (child), button_data->label, FALSE, FALSE, 0);
			break;
        }

	if (button_data->label != NULL) {
		gtk_label_set_ellipsize (GTK_LABEL (button_data->label), PANGO_ELLIPSIZE_MIDDLE);
		gtk_label_set_single_line_mode (GTK_LABEL (button_data->label), TRUE);

		button_data->bold_label = gtk_label_new (NULL);
		gtk_widget_set_no_show_all (button_data->bold_label, TRUE);
		gtk_label_set_single_line_mode (GTK_LABEL (button_data->bold_label), TRUE);
		gtk_box_pack_start (GTK_BOX (child), button_data->bold_label, FALSE, FALSE, 0);
	}

	if (button_data->path == NULL) {
        	button_data->path = g_object_ref (path);
	}
	if (button_data->dir_name == NULL) {
		button_data->dir_name = nautilus_file_get_display_name (file);
	}
	if (button_data->file == NULL) {
		button_data->file = nautilus_file_ref (file);
		nautilus_file_monitor_add (button_data->file, button_data,
					   NAUTILUS_FILE_ATTRIBUTES_FOR_ICON);
		button_data->file_changed_signal_id =
			g_signal_connect (button_data->file, "changed",
					  G_CALLBACK (button_data_file_changed),
					  button_data);
	}
		  
        gtk_container_add (GTK_CONTAINER (button_data->button), child);
        gtk_widget_show_all (button_data->button);

        nautilus_path_bar_update_button_state (button_data, current_dir);

        g_signal_connect (button_data->button, "clicked", G_CALLBACK (button_clicked_cb), button_data);
	g_signal_connect (button_data->button, "button-press-event", G_CALLBACK (button_event_cb), button_data);
	g_signal_connect (button_data->button, "button-release-event", G_CALLBACK (button_event_cb), button_data);
	g_signal_connect (button_data->button, "drag-begin", G_CALLBACK (button_drag_begin_cb), button_data);
        g_object_weak_ref (G_OBJECT (button_data->button), (GWeakNotify) button_data_free, button_data);

	setup_button_drag_source (button_data);

	nautilus_drag_slot_proxy_init (button_data->button, button_data->file, NULL);

	g_object_unref (path);

        return button_data;
}

static gboolean
nautilus_path_bar_check_parent_path (NautilusPathBar *path_bar,
				     GFile *location,
				     ButtonData **current_button_data)
{
        GList *list;
	ButtonData *button_data, *current_data;
	gboolean is_active;

	current_data = NULL;

        for (list = path_bar->priv->button_list; list; list = list->next) {
                button_data = list->data;
                if (g_file_equal (location, button_data->path)) {
			current_data = button_data;
			is_active = TRUE;

			if (!gtk_widget_get_child_visible (current_data->button)) {
				path_bar->priv->first_scrolled_button = list;
				gtk_widget_queue_resize (GTK_WIDGET (path_bar));
			}
		} else {
			is_active = FALSE;
		}

		nautilus_path_bar_update_button_state (button_data, is_active);
        }

	if (current_button_data != NULL) {
		*current_button_data = current_data;
	}

	return (current_data != NULL);
}

static void
nautilus_path_bar_update_path (NautilusPathBar *path_bar,
			       GFile *file_path)
{
	NautilusFile *file;
        gboolean first_directory;
        GList *new_buttons, *l;
	ButtonData *button_data;

        g_return_if_fail (NAUTILUS_IS_PATH_BAR (path_bar));
        g_return_if_fail (file_path != NULL);

	first_directory = TRUE;
	new_buttons = NULL;

	file = nautilus_file_get (file_path);

        gtk_widget_push_composite_child ();

        while (file != NULL) {
		NautilusFile *parent_file;

		parent_file = nautilus_file_get_parent (file);
		button_data = make_button_data (path_bar, file, first_directory);
		nautilus_file_unref (file);

		if (first_directory) {
			first_directory = FALSE;
		}

                new_buttons = g_list_prepend (new_buttons, button_data);

		if (parent_file != NULL &&
		    button_data->is_root) {
			nautilus_file_unref (parent_file);
			break;
		}
		
		file = parent_file;
        }

        nautilus_path_bar_clear_buttons (path_bar);
       	path_bar->priv->button_list = g_list_reverse (new_buttons);

       	for (l = path_bar->priv->button_list; l; l = l->next) {
		GtkWidget *button;
		button = BUTTON_DATA (l->data)->button;
		gtk_container_add (GTK_CONTAINER (path_bar), button);
	}	

        gtk_widget_pop_composite_child ();

	child_ordering_changed (path_bar);
}

void
nautilus_path_bar_set_path (NautilusPathBar *path_bar, 
			    GFile *file_path)
{
	ButtonData *button_data;

        g_return_if_fail (NAUTILUS_IS_PATH_BAR (path_bar));
        g_return_if_fail (file_path != NULL);
	
        /* Check whether the new path is already present in the pathbar as buttons.
         * This could be a parent directory or a previous selected subdirectory. */
        if (!nautilus_path_bar_check_parent_path (path_bar, file_path, &button_data)) {
		nautilus_path_bar_update_path (path_bar, file_path);
		button_data = g_list_nth_data (path_bar->priv->button_list, 0);
	}

	if (path_bar->priv->current_path != NULL) {
		g_object_unref (path_bar->priv->current_path);
	}

	path_bar->priv->current_path = g_object_ref (file_path);
	path_bar->priv->current_button_data = button_data;
}

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 2004, 2011 Red Hat, Inc.
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Based on ephy-navigation-action.h from Epiphany
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *           Marco Pesenti Gritti
 *           Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "nautilus-navigation-action.h"

#include "nautilus-window.h"

#include <gtk/gtk.h>
#include <eel/eel-gtk-extensions.h>

G_DEFINE_TYPE (NautilusNavigationAction, nautilus_navigation_action, GTK_TYPE_ACTION);
#define NAUTILUS_NAVIGATION_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), NAUTILUS_TYPE_NAVIGATION_ACTION, NautilusNavigationActionPrivate))

struct NautilusNavigationActionPrivate
{
	NautilusWindow *window;
	NautilusNavigationDirection direction;
	char *arrow_tooltip;

        guint popup_timeout_id;
};

enum
{
	PROP_0,
	PROP_ARROW_TOOLTIP,
	PROP_DIRECTION,
	PROP_WINDOW
};

static gboolean
should_open_in_new_tab (void)
{
	/* FIXME this is duplicated */
	GdkEvent *event;

	event = gtk_get_current_event ();
	if (event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) {
		return event->button.button == 2;
	}

	gdk_event_free (event);

	return FALSE;
}

static void
activate_back_or_forward_menu_item (GtkMenuItem *menu_item, 
				    NautilusWindow *window,
				    gboolean back)
{
	int index;
	
	g_assert (GTK_IS_MENU_ITEM (menu_item));

	index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "user_data"));

	nautilus_window_back_or_forward (window, back, index, should_open_in_new_tab ());
}

static void
activate_back_menu_item_callback (GtkMenuItem *menu_item,
                                  NautilusWindow *window)
{
	activate_back_or_forward_menu_item (menu_item, window, TRUE);
}

static void
activate_forward_menu_item_callback (GtkMenuItem *menu_item, NautilusWindow *window)
{
	activate_back_or_forward_menu_item (menu_item, window, FALSE);
}

static void
fill_menu (NautilusWindow *window,
	   GtkWidget *menu,
	   gboolean back)
{
	NautilusWindowSlot *slot;
	GtkWidget *menu_item;
	int index;
	GList *list;

	slot = nautilus_window_get_active_slot (window);
	
	list = back ? slot->back_list : slot->forward_list;
	index = 0;
	while (list != NULL) {
		menu_item = nautilus_bookmark_menu_item_new (NAUTILUS_BOOKMARK (list->data));
		g_object_set_data (G_OBJECT (menu_item), "user_data", GINT_TO_POINTER (index));
		gtk_widget_show (GTK_WIDGET (menu_item));
  		g_signal_connect_object (menu_item, "activate",
					 back
					 ? G_CALLBACK (activate_back_menu_item_callback)
					 : G_CALLBACK (activate_forward_menu_item_callback),
					 window, 0);
		
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
		list = g_list_next (list);
		++index;
	}
}

static void
show_menu (NautilusNavigationAction *self,
           guint button,
           guint32 event_time)
{
	NautilusWindow *window;
	GtkWidget *menu;

	window = self->priv->window;
	
	menu = gtk_menu_new ();

	switch (self->priv->direction) {
	case NAUTILUS_NAVIGATION_DIRECTION_FORWARD:
		fill_menu (window, menu, FALSE);
		break;
	case NAUTILUS_NAVIGATION_DIRECTION_BACK:
		fill_menu (window, menu, TRUE);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                        button, event_time);
}

#define MENU_POPUP_TIMEOUT 1200

static gboolean
popup_menu_timeout_cb (gpointer data)
{
        NautilusNavigationAction *self = data;

        show_menu (self, 1, gtk_get_current_event_time ());

        return FALSE;
}

static void
unschedule_menu_popup_timeout (NautilusNavigationAction *self)
{
        if (self->priv->popup_timeout_id != 0) {
                g_source_remove (self->priv->popup_timeout_id);
                self->priv->popup_timeout_id = 0;
        }
}

static void
schedule_menu_popup_timeout (NautilusNavigationAction *self)
{
        /* unschedule any previous timeouts */
        unschedule_menu_popup_timeout (self);

        self->priv->popup_timeout_id =
                g_timeout_add (MENU_POPUP_TIMEOUT,
                               popup_menu_timeout_cb,
                               self);
}

static gboolean
tool_button_press_cb (GtkButton *button,
                      GdkEventButton *event,
                      gpointer user_data)
{
        NautilusNavigationAction *self = user_data;

        if (event->button == 3) {
                /* right click */
                show_menu (self, event->button, event->time);
                return TRUE;
        }

        if (event->button == 1) {
                schedule_menu_popup_timeout (self);
        }

	return FALSE;
}

static gboolean
tool_button_release_cb (GtkButton *button,
                        GdkEventButton *event,
                        gpointer user_data)
{
        NautilusNavigationAction *self = user_data;

        unschedule_menu_popup_timeout (self);
        
        return FALSE;
}

static GtkWidget *
get_actual_button (GtkToolButton *tool_button)
{
	GList *children;
	GtkWidget *button;

	g_return_val_if_fail (GTK_IS_TOOL_BUTTON (tool_button), NULL);

	children = gtk_container_get_children (GTK_CONTAINER (tool_button));
	button = GTK_WIDGET (children->data);

	g_list_free (children);

	return button;
}

static void
connect_proxy (GtkAction *action,
               GtkWidget *proxy)
{
        GtkToolButton *tool;
        GtkWidget *button;

	if (GTK_IS_TOOL_BUTTON (proxy)) {
                tool = GTK_TOOL_BUTTON (proxy);
                button = get_actual_button (tool);

                g_signal_connect (button, "button-press-event",
                                  G_CALLBACK (tool_button_press_cb), action);
                g_signal_connect (button, "button-release-event",
                                  G_CALLBACK (tool_button_release_cb), action);
        }

	(* GTK_ACTION_CLASS (nautilus_navigation_action_parent_class)->connect_proxy) (action, proxy);
}

static void
disconnect_proxy (GtkAction *action,
                  GtkWidget *proxy)
{
        GtkToolButton *tool;
        GtkWidget *button;

	if (GTK_IS_TOOL_BUTTON (proxy)) {
                tool = GTK_TOOL_BUTTON (proxy);
                button = get_actual_button (tool);

                /* remove any possible timeout going on */
                unschedule_menu_popup_timeout (NAUTILUS_NAVIGATION_ACTION (action));

		g_signal_handlers_disconnect_by_func (button,
                                                      G_CALLBACK (tool_button_press_cb), action);
		g_signal_handlers_disconnect_by_func (button,
                                                      G_CALLBACK (tool_button_release_cb), action);
	}

	(* GTK_ACTION_CLASS (nautilus_navigation_action_parent_class)->disconnect_proxy) (action, proxy);
}

static void
nautilus_navigation_action_finalize (GObject *object)
{
	NautilusNavigationAction *action = NAUTILUS_NAVIGATION_ACTION (object);

        /* remove any possible timeout going on */
        unschedule_menu_popup_timeout (action);

	g_free (action->priv->arrow_tooltip);

	(* G_OBJECT_CLASS (nautilus_navigation_action_parent_class)->finalize) (object);
}

static void
nautilus_navigation_action_set_property (GObject *object,
					 guint prop_id,
					 const GValue *value,
					 GParamSpec *pspec)
{
	NautilusNavigationAction *nav;

	nav = NAUTILUS_NAVIGATION_ACTION (object);

	switch (prop_id)
	{
		case PROP_ARROW_TOOLTIP:
			g_free (nav->priv->arrow_tooltip);
			nav->priv->arrow_tooltip = g_value_dup_string (value);
			break;
		case PROP_DIRECTION:
			nav->priv->direction = g_value_get_int (value);
			break;
		case PROP_WINDOW:
			nav->priv->window = g_value_get_object (value);
			break;
	}
}

static void
nautilus_navigation_action_get_property (GObject *object,
					 guint prop_id,
					 GValue *value,
					 GParamSpec *pspec)
{
	NautilusNavigationAction *nav;

	nav = NAUTILUS_NAVIGATION_ACTION (object);

	switch (prop_id)
	{
		case PROP_ARROW_TOOLTIP:
			g_value_set_string (value, nav->priv->arrow_tooltip);
			break;
		case PROP_DIRECTION:
			g_value_set_int (value, nav->priv->direction);
			break;
		case PROP_WINDOW:
			g_value_set_object (value, nav->priv->window);
			break;
	}
}

static void
nautilus_navigation_action_class_init (NautilusNavigationActionClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GtkActionClass *action_class = GTK_ACTION_CLASS (class);

	object_class->finalize = nautilus_navigation_action_finalize;
	object_class->set_property = nautilus_navigation_action_set_property;
	object_class->get_property = nautilus_navigation_action_get_property;

	action_class->toolbar_item_type = GTK_TYPE_TOOL_BUTTON;
	action_class->connect_proxy = connect_proxy;
	action_class->disconnect_proxy = disconnect_proxy;

	g_object_class_install_property (object_class,
                                         PROP_ARROW_TOOLTIP,
                                         g_param_spec_string ("arrow-tooltip",
                                                              "Arrow Tooltip",
                                                              "Arrow Tooltip",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
                                         PROP_DIRECTION,
                                         g_param_spec_int ("direction",
                                                           "Direction",
                                                           "Direction",
                                                           0,
							   G_MAXINT,
							   0,
                                                           G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
                                         PROP_WINDOW,
                                         g_param_spec_object ("window",
                                                              "Window",
                                                              "The navigation window",
                                                              NAUTILUS_TYPE_WINDOW,
                                                              G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof(NautilusNavigationActionPrivate));
}

static void
nautilus_navigation_action_init (NautilusNavigationAction *action)
{
        action->priv = NAUTILUS_NAVIGATION_ACTION_GET_PRIVATE (action);
}

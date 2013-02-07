/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2011, Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "nautilus-toolbar.h"

#include "nautilus-location-entry.h"
#include "nautilus-pathbar.h"
#include "nautilus-actions.h"

#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-ui-utilities.h>

#include <glib/gi18n.h>
#include <math.h>

typedef enum {
	NAUTILUS_NAVIGATION_DIRECTION_NONE,
	NAUTILUS_NAVIGATION_DIRECTION_BACK,
	NAUTILUS_NAVIGATION_DIRECTION_FORWARD
} NautilusNavigationDirection;

struct _NautilusToolbarPriv {
	GtkWidget *toolbar;
	NautilusWindow *window;

	GtkWidget *path_bar;
	GtkWidget *location_entry;

	GtkToolItem *back_forward;

	gboolean show_location_entry;

	guint popup_timeout_id;
};

enum {
	PROP_WINDOW = 1,
	PROP_SHOW_LOCATION_ENTRY,
	NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NautilusToolbar, nautilus_toolbar, GTK_TYPE_BOX);

static void unschedule_menu_popup_timeout (NautilusToolbar *self);

static void
toolbar_update_appearance (NautilusToolbar *self)
{
	gboolean show_location_entry;

	show_location_entry = self->priv->show_location_entry ||
		g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY);

	gtk_widget_set_visible (self->priv->location_entry,
				show_location_entry);
	gtk_widget_set_visible (self->priv->path_bar,
				!show_location_entry);
}

static gint
get_icon_margin (NautilusToolbar *self)
{
	GtkIconSize toolbar_size;
	gint toolbar_size_px, menu_size_px;

	toolbar_size = gtk_toolbar_get_icon_size (GTK_TOOLBAR (self->priv->toolbar));

	gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &menu_size_px, NULL);
	gtk_icon_size_lookup (toolbar_size, &toolbar_size_px, NULL);

	return (gint) floor ((toolbar_size_px - menu_size_px) / 2.0);
}

static GtkWidget *
toolbar_create_toolbutton (NautilusToolbar *self,
			   gboolean create_menu,
			   gboolean create_toggle,
			   const gchar *name,
			   const gchar *tooltip)
{
	GtkWidget *button, *image;
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = nautilus_window_get_main_action_group (self->priv->window);

	if (create_menu) {
		button = gtk_menu_button_new ();
	} else if (create_toggle) {
		button = gtk_toggle_button_new ();
	} else {
		button = gtk_button_new ();
	}

	image = gtk_image_new ();
	g_object_set (image, "margin", get_icon_margin (self), NULL);

	gtk_button_set_image (GTK_BUTTON (button), image);

	if (create_menu) {
		gtk_image_set_from_icon_name (GTK_IMAGE (image), name,
					      GTK_ICON_SIZE_MENU);
		gtk_widget_set_tooltip_text (button, tooltip);
	} else {
		action = gtk_action_group_get_action (action_group, name);
		gtk_activatable_set_related_action (GTK_ACTIVATABLE (button), action);
		gtk_button_set_label (GTK_BUTTON (button), NULL);
		gtk_widget_set_tooltip_text (button, gtk_action_get_tooltip (action));
	}

	return button;
}

static void
activate_back_or_forward_menu_item (GtkMenuItem *menu_item, 
				    NautilusWindow *window,
				    gboolean back)
{
	int index;
	
	g_assert (GTK_IS_MENU_ITEM (menu_item));

	index = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menu_item), "user_data"));

	nautilus_window_back_or_forward (window, back, index, nautilus_event_get_window_open_flags ());
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
	list = back ? nautilus_window_slot_get_back_history (slot) :
		nautilus_window_slot_get_forward_history (slot);

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

/* adapted from gtk/gtkmenubutton.c */
static void
menu_position_func (GtkMenu       *menu,
		    gint          *x,
		    gint          *y,
		    gboolean      *push_in,
		    GtkWidget     *widget)
{
	GtkWidget *toplevel;
	GtkRequisition menu_req;
	GdkRectangle monitor;
	gint monitor_num;
	GdkScreen *screen;
	GdkWindow *window;
	GtkAllocation allocation;

	/* Set the dropdown menu hint on the toplevel, so the WM can omit the top side
	 * of the shadows.
	 */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (menu));
	gtk_window_set_type_hint (GTK_WINDOW (toplevel), GDK_WINDOW_TYPE_HINT_DROPDOWN_MENU);

	window = gtk_widget_get_window (widget);
	screen = gtk_widget_get_screen (GTK_WIDGET (menu));
	monitor_num = gdk_screen_get_monitor_at_window (screen, window);
	if (monitor_num < 0) {
		monitor_num = 0;
	}

	gdk_screen_get_monitor_workarea (screen, monitor_num, &monitor);
	gtk_widget_get_preferred_size (GTK_WIDGET (menu), &menu_req, NULL);
	gtk_widget_get_allocation (widget, &allocation);
	gdk_window_get_origin (window, x, y);

	*x += allocation.x;
	*y += allocation.y;

	if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) {
		*x -= MAX (menu_req.width - allocation.width, 0);
	} else {
		*x += MAX (allocation.width - menu_req.width, 0);
	}

	if ((*y + allocation.height + menu_req.height) <= monitor.y + monitor.height) {
		*y += allocation.height;
	} else if ((*y - menu_req.height) >= monitor.y) {
		*y -= menu_req.height;
	} else if (monitor.y + monitor.height - (*y + allocation.height) > *y) {
		*y += allocation.height;
	} else {
		*y -= menu_req.height;
	}

	*push_in = FALSE;
}

static void
show_menu (NautilusToolbar *self,
	   GtkWidget *widget,
           guint button,
           guint32 event_time)
{
	NautilusWindow *window;
	GtkWidget *menu;
	NautilusNavigationDirection direction;

	window = self->priv->window;
	menu = gtk_menu_new ();

	direction = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget),
							 "nav-direction"));

	switch (direction) {
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

        gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
			(GtkMenuPositionFunc) menu_position_func, widget,
                        button, event_time);
}

#define MENU_POPUP_TIMEOUT 1200

typedef struct {
	NautilusToolbar *self;
	GtkWidget *widget;
} ScheduleMenuData;

static void
schedule_menu_data_free (ScheduleMenuData *data)
{
	g_slice_free (ScheduleMenuData, data);
}

static gboolean
popup_menu_timeout_cb (gpointer user_data)
{
	ScheduleMenuData *data = user_data;

        show_menu (data->self, data->widget,
		   1, gtk_get_current_event_time ());

        return FALSE;
}

static void
unschedule_menu_popup_timeout (NautilusToolbar *self)
{
        if (self->priv->popup_timeout_id != 0) {
                g_source_remove (self->priv->popup_timeout_id);
                self->priv->popup_timeout_id = 0;
        }
}

static void
schedule_menu_popup_timeout (NautilusToolbar *self,
			     GtkWidget *widget)
{
	ScheduleMenuData *data;

        /* unschedule any previous timeouts */
        unschedule_menu_popup_timeout (self);

	data = g_slice_new0 (ScheduleMenuData);
	data->self = self;
	data->widget = widget;

        self->priv->popup_timeout_id =
                g_timeout_add_full (G_PRIORITY_DEFAULT, MENU_POPUP_TIMEOUT,
				    popup_menu_timeout_cb, data,
				    (GDestroyNotify) schedule_menu_data_free);
}

static gboolean
tool_button_press_cb (GtkButton *button,
                      GdkEventButton *event,
                      gpointer user_data)
{
        NautilusToolbar *self = user_data;

        if (event->button == 3) {
                /* right click */
                show_menu (self, GTK_WIDGET (button), event->button, event->time);
                return TRUE;
        }

        if (event->button == 1) {
                schedule_menu_popup_timeout (self, GTK_WIDGET (button));
        }

	return FALSE;
}

static gboolean
tool_button_release_cb (GtkButton *button,
                        GdkEventButton *event,
                        gpointer user_data)
{
        NautilusToolbar *self = user_data;

        unschedule_menu_popup_timeout (self);

        return FALSE;
}

static void
navigation_button_setup_menu (NautilusToolbar *self,
			      GtkWidget *button,
			      NautilusNavigationDirection direction)
{
	g_object_set_data (G_OBJECT (button), "nav-direction", GUINT_TO_POINTER (direction));

	g_signal_connect (button, "button-press-event",
			  G_CALLBACK (tool_button_press_cb), self);
	g_signal_connect (button, "button-release-event",
			  G_CALLBACK (tool_button_release_cb), self);
}

static gboolean
gear_menu_key_press (GtkWidget *widget,
                     GdkEventKey *event,
                     gpointer user_data)
{
        GdkModifierType mask = gtk_accelerator_get_default_mod_mask ();

        if ((event->state & mask) == 0 && (event->keyval == GDK_KEY_F10)) {
            gtk_menu_shell_deactivate (GTK_MENU_SHELL (widget));
            return TRUE;
        }

        return FALSE;
}

static void
nautilus_toolbar_constructed (GObject *obj)
{
	NautilusToolbar *self = NAUTILUS_TOOLBAR (obj);
	GtkWidget *hbox, *toolbar;
	GtkStyleContext *context;
	GtkWidget *tool_button;
	GtkWidget *menu;
	GtkWidget *box;
	GtkToolItem *back_forward;
	GtkToolItem *tool_item;
	GtkUIManager *ui_manager;

	G_OBJECT_CLASS (nautilus_toolbar_parent_class)->constructed (obj);

	ui_manager = nautilus_window_get_ui_manager (self->priv->window);

	gtk_style_context_set_junction_sides (gtk_widget_get_style_context (GTK_WIDGET (self)),
					      GTK_JUNCTION_BOTTOM);

	toolbar = gtk_toolbar_new ();
	self->priv->toolbar = toolbar;

	gtk_box_pack_start (GTK_BOX (self), self->priv->toolbar, TRUE, TRUE, 0);
	gtk_widget_show_all (self->priv->toolbar);

	context = gtk_widget_get_style_context (toolbar);
	/* Set the MENUBAR style class so it's possible to drag the app
	 * using the toolbar. */
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_MENUBAR);

	/* Back and Forward */
	back_forward = gtk_tool_item_new ();
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	/* Back */
	tool_button = toolbar_create_toolbutton (self, FALSE, FALSE, NAUTILUS_ACTION_BACK, NULL);
	navigation_button_setup_menu (self, tool_button, NAUTILUS_NAVIGATION_DIRECTION_BACK);
	gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (tool_button));

	/* Forward */
	tool_button = toolbar_create_toolbutton (self, FALSE, FALSE, NAUTILUS_ACTION_FORWARD, NULL);
	navigation_button_setup_menu (self, tool_button, NAUTILUS_NAVIGATION_DIRECTION_FORWARD);
	gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (tool_button));

	gtk_style_context_add_class (gtk_widget_get_style_context (box),
				     GTK_STYLE_CLASS_RAISED);
	gtk_style_context_add_class (gtk_widget_get_style_context (box),
				     GTK_STYLE_CLASS_LINKED);

	gtk_container_add (GTK_CONTAINER (back_forward), box);
	gtk_container_add (GTK_CONTAINER (self->priv->toolbar), GTK_WIDGET (back_forward));

	gtk_widget_show_all (GTK_WIDGET (back_forward));
	gtk_widget_set_margin_right (GTK_WIDGET (back_forward), 12);

	/* regular path bar */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox);

	self->priv->path_bar = g_object_new (NAUTILUS_TYPE_PATH_BAR, NULL);
	gtk_box_pack_start (GTK_BOX (hbox), self->priv->path_bar, TRUE, TRUE, 0);

	/* entry-like location bar */
	self->priv->location_entry = nautilus_location_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), self->priv->location_entry, TRUE, TRUE, 0);

	tool_item = gtk_tool_item_new ();
	gtk_tool_item_set_expand (tool_item, TRUE);
	gtk_container_add (GTK_CONTAINER (tool_item), hbox);
	gtk_container_add (GTK_CONTAINER (self->priv->toolbar), GTK_WIDGET (tool_item));
	gtk_widget_show (GTK_WIDGET (tool_item));

	/* search */
	tool_item = gtk_tool_item_new ();
	tool_button = toolbar_create_toolbutton (self, FALSE, TRUE, NAUTILUS_ACTION_SEARCH, NULL);
	gtk_container_add (GTK_CONTAINER (tool_item), GTK_WIDGET (tool_button));
	gtk_container_add (GTK_CONTAINER (self->priv->toolbar), GTK_WIDGET (tool_item));
	gtk_widget_show_all (GTK_WIDGET (tool_item));
	gtk_widget_set_margin_left (GTK_WIDGET (tool_item), 12);

	/* View buttons */
	tool_item = gtk_tool_item_new ();
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

	tool_button = toolbar_create_toolbutton (self, FALSE, TRUE, NAUTILUS_ACTION_VIEW_LIST, NULL);
	gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (tool_button));
	tool_button = toolbar_create_toolbutton (self, FALSE, TRUE, NAUTILUS_ACTION_VIEW_GRID, NULL);
	gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (tool_button));
	tool_button = toolbar_create_toolbutton (self, TRUE, FALSE, "go-down-symbolic", _("View options"));
	gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (tool_button));
	menu = gtk_ui_manager_get_widget (ui_manager, "/ViewMenu");
	gtk_menu_button_set_popup (GTK_MENU_BUTTON (tool_button), menu);

	gtk_style_context_add_class (gtk_widget_get_style_context (box),
				     GTK_STYLE_CLASS_RAISED);
	gtk_style_context_add_class (gtk_widget_get_style_context (box),
				     GTK_STYLE_CLASS_LINKED);

	gtk_container_add (GTK_CONTAINER (tool_item), box);
	gtk_container_add (GTK_CONTAINER (self->priv->toolbar), GTK_WIDGET (tool_item));
	gtk_widget_show_all (GTK_WIDGET (tool_item));
	gtk_widget_set_margin_left (GTK_WIDGET (tool_item), 12);

	/* Action Menu */
	tool_item = gtk_tool_item_new ();
	tool_button = toolbar_create_toolbutton (self, TRUE, FALSE, "emblem-system-symbolic", _("Location options"));
	menu = gtk_ui_manager_get_widget (ui_manager, "/ActionMenu");
	gtk_widget_set_halign (menu, GTK_ALIGN_END);
	gtk_menu_button_set_popup (GTK_MENU_BUTTON (tool_button), menu);
	gtk_actionable_set_action_name (GTK_ACTIONABLE (tool_button), "win.gear-menu");
        g_signal_connect (menu, "key-press-event", G_CALLBACK (gear_menu_key_press), self);

	gtk_container_add (GTK_CONTAINER (tool_item), tool_button);
	gtk_container_add (GTK_CONTAINER (toolbar), GTK_WIDGET (tool_item));
	gtk_widget_show_all (GTK_WIDGET (tool_item));
	gtk_widget_set_margin_left (GTK_WIDGET (tool_item), 12);

	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY,
				  G_CALLBACK (toolbar_update_appearance), self);

	toolbar_update_appearance (self);
}

static void
nautilus_toolbar_init (NautilusToolbar *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NAUTILUS_TYPE_TOOLBAR,
						  NautilusToolbarPriv);
}

static void
nautilus_toolbar_get_property (GObject *object,
			       guint property_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	NautilusToolbar *self = NAUTILUS_TOOLBAR (object);

	switch (property_id) {
	case PROP_SHOW_LOCATION_ENTRY:
		g_value_set_boolean (value, self->priv->show_location_entry);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nautilus_toolbar_set_property (GObject *object,
			       guint property_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	NautilusToolbar *self = NAUTILUS_TOOLBAR (object);

	switch (property_id) {
	case PROP_WINDOW:
		self->priv->window = g_value_get_object (value);
		break;
	case PROP_SHOW_LOCATION_ENTRY:
		nautilus_toolbar_set_show_location_entry (self, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nautilus_toolbar_dispose (GObject *obj)
{
	NautilusToolbar *self = NAUTILUS_TOOLBAR (obj);

	g_signal_handlers_disconnect_by_func (nautilus_preferences,
					      toolbar_update_appearance, self);
	unschedule_menu_popup_timeout (self);

	G_OBJECT_CLASS (nautilus_toolbar_parent_class)->dispose (obj);
}

static void
nautilus_toolbar_class_init (NautilusToolbarClass *klass)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (klass);
	oclass->get_property = nautilus_toolbar_get_property;
	oclass->set_property = nautilus_toolbar_set_property;
	oclass->constructed = nautilus_toolbar_constructed;
	oclass->dispose = nautilus_toolbar_dispose;

	properties[PROP_WINDOW] =
		g_param_spec_object ("window",
				     "The NautilusWindow",
				     "The NautilusWindow this toolbar is part of",
				     NAUTILUS_TYPE_WINDOW,
				     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
				     G_PARAM_STATIC_STRINGS);
	properties[PROP_SHOW_LOCATION_ENTRY] =
		g_param_spec_boolean ("show-location-entry",
				      "Whether to show the location entry",
				      "Whether to show the location entry instead of the pathbar",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	
	g_type_class_add_private (klass, sizeof (NautilusToolbarClass));
	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

GtkWidget *
nautilus_toolbar_new (NautilusWindow *window)
{
	return g_object_new (NAUTILUS_TYPE_TOOLBAR,
			     "window", window,
			     "orientation", GTK_ORIENTATION_VERTICAL,
			     NULL);
}

GtkWidget *
nautilus_toolbar_get_path_bar (NautilusToolbar *self)
{
	return self->priv->path_bar;
}

GtkWidget *
nautilus_toolbar_get_location_entry (NautilusToolbar *self)
{
	return self->priv->location_entry;
}

void
nautilus_toolbar_set_show_location_entry (NautilusToolbar *self,
					  gboolean show_location_entry)
{
	if (show_location_entry != self->priv->show_location_entry) {
		self->priv->show_location_entry = show_location_entry;
		toolbar_update_appearance (self);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_LOCATION_ENTRY]);
	}
}

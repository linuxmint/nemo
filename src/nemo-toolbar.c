/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 2011, Red Hat, Inc.
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "nemo-toolbar.h"

#include "nemo-location-bar.h"
#include "nemo-pathbar.h"
#include "nemo-window-private.h"
#include "nemo-actions.h"
#include "nemo-file-utilities.h"
#include <glib/gi18n.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-ui-utilities.h>

struct _NemoToolbarPriv {
	GtkWidget *toolbar;

	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;

    GtkWidget *previous_button;
    GtkWidget *next_button;
    GtkWidget *up_button;
    GtkWidget *refresh_button;
    GtkWidget *home_button;
    GtkWidget *computer_button;
    GtkWidget *toggle_location_button;
    GtkWidget *open_terminal_button;
    GtkWidget *new_folder_button;
    GtkWidget *search_button;
    GtkWidget *icon_view_button;
    GtkWidget *list_view_button;
    GtkWidget *compact_view_button;
    GtkWidget *show_thumbnails_button;

    GtkToolItem *navigation_box;
    GtkToolItem *refresh_box;
    GtkToolItem *location_box;
    GtkToolItem *tools_box;
    GtkToolItem *view_box;

	GtkWidget *path_bar;
	GtkWidget *location_bar;
    GtkWidget *root_bar;
    GtkWidget *stack;

	gboolean show_main_bar;
	gboolean show_location_entry;
    gboolean show_root_bar;
};

enum {
	PROP_ACTION_GROUP = 1,
	PROP_SHOW_LOCATION_ENTRY,
	PROP_SHOW_MAIN_BAR,
	NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NemoToolbar, nemo_toolbar, GTK_TYPE_BOX);

static void
nemo_toolbar_update_root_state (NemoToolbar *self)
{
    if ((nemo_user_is_root () && !nemo_treating_root_as_normal()) &&
         g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_ROOT_WARNING)) {
        if (self->priv->show_root_bar != TRUE) {
            self->priv->show_root_bar = TRUE;
        }
    } else {
        self->priv->show_root_bar = FALSE;
    }
}

static void
toolbar_update_appearance (NemoToolbar *self)
{
	GtkWidget *widgetitem;
	gboolean icon_toolbar;
	gboolean show_location_entry;

    nemo_toolbar_update_root_state (self);

	show_location_entry = self->priv->show_location_entry;

	gtk_widget_set_visible (GTK_WIDGET(self->priv->toolbar),
				self->priv->show_main_bar);

    if (show_location_entry) {
        gtk_stack_set_visible_child_name (GTK_STACK (self->priv->stack), "location_bar");
    } else {
        gtk_stack_set_visible_child_name (GTK_STACK (self->priv->stack), "path_bar");
    }

    gtk_widget_set_visible (self->priv->root_bar,
                self->priv->show_root_bar);
        
        /* Please refer to the element name, not the action name after the forward slash, otherwise the prefs will not work*/

    widgetitem = self->priv->previous_button;
    icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_PREVIOUS_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->next_button;
    icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_NEXT_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->up_button;
    icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_UP_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->refresh_button;
    icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_RELOAD_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->home_button;
    icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_HOME_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->computer_button;
    icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_COMPUTER_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->search_button;
    icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_SEARCH_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->new_folder_button;
    icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_NEW_FOLDER_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}
        
    widgetitem = self->priv->open_terminal_button;
    icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_OPEN_IN_TERMINAL_TOOLBAR);
    if (icon_toolbar == FALSE ) {gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->toggle_location_button;
    icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_EDIT_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->icon_view_button;
    icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_ICON_VIEW_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->list_view_button;
    icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_LIST_VIEW_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->compact_view_button;
    icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_COMPACT_VIEW_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}
    
    widgetitem = self->priv->show_thumbnails_button;
    icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_SHOW_THUMBNAILS_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    if (gtk_widget_get_visible(self->priv->previous_button) == FALSE &&
        gtk_widget_get_visible(self->priv->next_button) == FALSE &&
        gtk_widget_get_visible(self->priv->up_button) == FALSE)
    {
        gtk_widget_hide(GTK_WIDGET (self->priv->navigation_box));
    } else {
        gtk_widget_show(GTK_WIDGET (self->priv->navigation_box));
    }

    if (gtk_widget_get_visible(self->priv->home_button) == FALSE && 
        gtk_widget_get_visible(self->priv->computer_button) == FALSE)
    {
        gtk_widget_hide(GTK_WIDGET (self->priv->location_box));
    } else {
        gtk_widget_show(GTK_WIDGET (self->priv->location_box));
    }

    if (gtk_widget_get_visible(self->priv->refresh_button) == FALSE) 
    {
        gtk_widget_hide(GTK_WIDGET (self->priv->refresh_box));
    } else {
        gtk_widget_show(GTK_WIDGET (self->priv->refresh_box));
    }

    if (gtk_widget_get_visible(self->priv->search_button) == FALSE &&
        gtk_widget_get_visible(self->priv->new_folder_button) == FALSE && 
        gtk_widget_get_visible(self->priv->open_terminal_button) == FALSE &&
        gtk_widget_get_visible(self->priv->toggle_location_button) == FALSE &&
        gtk_widget_get_visible(self->priv->show_thumbnails_button) == FALSE)
    {
        gtk_widget_hide(GTK_WIDGET (self->priv->tools_box));
    } else {
        gtk_widget_show(GTK_WIDGET (self->priv->tools_box));
    }

    if (gtk_widget_get_visible(self->priv->icon_view_button) == FALSE &&
        gtk_widget_get_visible(self->priv->list_view_button) == FALSE &&
        gtk_widget_get_visible(self->priv->compact_view_button) == FALSE)
    {
        gtk_widget_hide(GTK_WIDGET (self->priv->view_box));
    } else {
        gtk_widget_show(GTK_WIDGET (self->priv->view_box));
    }
}

static void
setup_root_info_bar (NemoToolbar *self) {

    GtkWidget *root_bar = gtk_info_bar_new ();
    gtk_info_bar_set_message_type (GTK_INFO_BAR (root_bar), GTK_MESSAGE_ERROR);
    GtkWidget *content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (root_bar));

    GtkWidget *label = gtk_label_new (_("Elevated Privileges"));
    gtk_widget_show (label);
    gtk_container_add (GTK_CONTAINER (content_area), label);

    self->priv->root_bar = root_bar;
    gtk_box_pack_start (GTK_BOX (self), self->priv->root_bar, TRUE, TRUE, 0);
}

static GtkWidget *
toolbar_create_toolbutton (NemoToolbar *self,
                gboolean create_toggle,
                const gchar *name)
{
    GtkWidget *button;
    GtkWidget *image;
    GtkAction *action;

    if (create_toggle)
    {
        button = gtk_toggle_button_new ();
    } else {
        button = gtk_button_new ();
    }

    image = gtk_image_new ();

    gtk_button_set_image (GTK_BUTTON (button), image);
    action = gtk_action_group_get_action (self->priv->action_group, name);
    gtk_activatable_set_related_action (GTK_ACTIVATABLE (button), action);
    gtk_button_set_label (GTK_BUTTON (button), NULL);
    gtk_widget_set_tooltip_text (button, gtk_action_get_tooltip (action));

    return button;
}

static void
nemo_toolbar_constructed (GObject *obj)
{
	NemoToolbar *self = NEMO_TOOLBAR (obj);
	GtkWidget *toolbar;
    GtkWidget *hbox;
    GtkToolItem *tool_box;
    GtkWidget *box;
	GtkStyleContext *context;

	G_OBJECT_CLASS (nemo_toolbar_parent_class)->constructed (obj);

	gtk_style_context_set_junction_sides (gtk_widget_get_style_context (GTK_WIDGET (self)),
					      GTK_JUNCTION_BOTTOM);

    self->priv->show_location_entry = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_LOCATION_ENTRY);

	/* add the UI */
	self->priv->ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (self->priv->ui_manager, self->priv->action_group, 0);

	toolbar = gtk_toolbar_new ();
	self->priv->toolbar = toolbar;
    gtk_box_pack_start (GTK_BOX (self), self->priv->toolbar, TRUE, TRUE, 0);
	
	context = gtk_widget_get_style_context (GTK_WIDGET(toolbar));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_PRIMARY_TOOLBAR);
	
    /* Back/Forward/Up */
    self->priv->navigation_box = gtk_tool_item_new ();
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    self->priv->previous_button = toolbar_create_toolbutton (self, FALSE, NEMO_ACTION_BACK);
    gtk_container_add (GTK_CONTAINER (box), self->priv->previous_button);

    self->priv->next_button = toolbar_create_toolbutton (self, FALSE, NEMO_ACTION_FORWARD);
    gtk_container_add (GTK_CONTAINER (box), self->priv->next_button);

    self->priv->up_button = toolbar_create_toolbutton (self, FALSE, NEMO_ACTION_UP);
    gtk_container_add (GTK_CONTAINER (box), self->priv->up_button);

    gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_RAISED);
    gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_LINKED);

    gtk_container_add (GTK_CONTAINER (self->priv->navigation_box), GTK_WIDGET (box));
    gtk_container_add (GTK_CONTAINER (self->priv->toolbar), GTK_WIDGET (self->priv->navigation_box));

    gtk_widget_show_all (GTK_WIDGET (self->priv->navigation_box));
    gtk_widget_set_margin_right (GTK_WIDGET (self->priv->navigation_box), 6);

    /* Refresh */
    self->priv->refresh_box = gtk_tool_item_new ();
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    self->priv->refresh_button = toolbar_create_toolbutton (self, FALSE, NEMO_ACTION_RELOAD);
    gtk_container_add (GTK_CONTAINER (box), self->priv->refresh_button);
    gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_RAISED);

    gtk_container_add (GTK_CONTAINER (self->priv->refresh_box), GTK_WIDGET (box));
    gtk_container_add (GTK_CONTAINER (self->priv->toolbar), GTK_WIDGET (self->priv->refresh_box));

    gtk_widget_show_all (GTK_WIDGET (self->priv->refresh_box));
    gtk_widget_set_margin_right (GTK_WIDGET (self->priv->refresh_box), 6);

    /* Home/Computer */
    self->priv->location_box = gtk_tool_item_new ();
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    self->priv->home_button = toolbar_create_toolbutton (self, FALSE, NEMO_ACTION_HOME);
    gtk_container_add (GTK_CONTAINER (box), self->priv->home_button);

    self->priv->computer_button = toolbar_create_toolbutton (self, FALSE, NEMO_ACTION_COMPUTER);
    gtk_container_add (GTK_CONTAINER (box), self->priv->computer_button);

    gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_RAISED);
    gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_LINKED);

    gtk_container_add (GTK_CONTAINER (self->priv->location_box), GTK_WIDGET (box));
    gtk_container_add (GTK_CONTAINER (self->priv->toolbar), GTK_WIDGET (self->priv->location_box));

    gtk_widget_show_all (GTK_WIDGET (self->priv->location_box));
    gtk_widget_set_margin_right (GTK_WIDGET (self->priv->location_box), 6);

    /* Container to hold the location and pathbars */
    self->priv->stack = gtk_stack_new();
    gtk_stack_set_transition_type (GTK_STACK (self->priv->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration (GTK_STACK (self->priv->stack), 150);

    /* Regular Path Bar */
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (self->priv->stack), TRUE, TRUE, 0);

    self->priv->path_bar = g_object_new (NEMO_TYPE_PATH_BAR, NULL);
    gtk_stack_add_named(GTK_STACK (self->priv->stack), GTK_WIDGET (self->priv->path_bar), "path_bar");
    
    /* Entry-Like Location Bar */
    self->priv->location_bar = nemo_location_bar_new ();
    gtk_stack_add_named(GTK_STACK (self->priv->stack), GTK_WIDGET (self->priv->location_bar), "location_bar");
    gtk_widget_show_all (hbox);

    tool_box = gtk_tool_item_new ();
    gtk_tool_item_set_expand (tool_box, TRUE);
    gtk_container_add (GTK_CONTAINER (tool_box), hbox);
    gtk_container_add (GTK_CONTAINER (self->priv->toolbar), GTK_WIDGET (tool_box));
    gtk_widget_show (GTK_WIDGET (tool_box));

    /* Search/Open in Terminal/New Folder/Toggle Location */
    self->priv->tools_box = gtk_tool_item_new ();
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    self->priv->toggle_location_button = toolbar_create_toolbutton (self, FALSE, NEMO_ACTION_TOGGLE_LOCATION);
    gtk_container_add (GTK_CONTAINER (box), self->priv->toggle_location_button);

    self->priv->open_terminal_button = toolbar_create_toolbutton (self, FALSE, NEMO_ACTION_OPEN_IN_TERMINAL);
    gtk_container_add (GTK_CONTAINER (box), self->priv->open_terminal_button);

    self->priv->new_folder_button = toolbar_create_toolbutton (self, FALSE, NEMO_ACTION_NEW_FOLDER);
    gtk_container_add (GTK_CONTAINER (box), self->priv->new_folder_button);

    self->priv->search_button = toolbar_create_toolbutton (self, TRUE, NEMO_ACTION_SEARCH);
    gtk_container_add (GTK_CONTAINER (box), self->priv->search_button);
    
    self->priv->show_thumbnails_button = toolbar_create_toolbutton (self, TRUE, NEMO_ACTION_SHOW_THUMBNAILS);
    gtk_container_add (GTK_CONTAINER (box), self->priv->show_thumbnails_button);

    gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_RAISED);
    gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_LINKED);

    gtk_container_add (GTK_CONTAINER (self->priv->tools_box), GTK_WIDGET (box));
    gtk_container_add (GTK_CONTAINER (self->priv->toolbar), GTK_WIDGET (self->priv->tools_box));

    gtk_widget_show_all (GTK_WIDGET (self->priv->tools_box));
    gtk_widget_set_margin_left (GTK_WIDGET (self->priv->tools_box), 6);

    setup_root_info_bar (self);

    /* View Select */
    self->priv->view_box = gtk_tool_item_new ();
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    self->priv->icon_view_button = toolbar_create_toolbutton (self, TRUE, NEMO_ACTION_ICON_VIEW);
    gtk_container_add (GTK_CONTAINER (box), self->priv->icon_view_button);

    self->priv->list_view_button = toolbar_create_toolbutton (self, TRUE, NEMO_ACTION_LIST_VIEW);
    gtk_container_add (GTK_CONTAINER (box), self->priv->list_view_button);

    self->priv->compact_view_button = toolbar_create_toolbutton (self, TRUE, NEMO_ACTION_COMPACT_VIEW);
    gtk_container_add (GTK_CONTAINER (box), self->priv->compact_view_button);

    gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_RAISED);
    gtk_style_context_add_class (gtk_widget_get_style_context (box), GTK_STYLE_CLASS_LINKED);

    gtk_container_add (GTK_CONTAINER (self->priv->view_box), GTK_WIDGET (box));
    gtk_container_add (GTK_CONTAINER (self->priv->toolbar), GTK_WIDGET (self->priv->view_box));

    gtk_widget_show_all (GTK_WIDGET (self->priv->view_box));
    gtk_widget_set_margin_left (GTK_WIDGET (self->priv->view_box), 6);

    /* nemo patch */
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_PREVIOUS_ICON_TOOLBAR,
                  G_CALLBACK (toolbar_update_appearance), self);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_NEXT_ICON_TOOLBAR,
                  G_CALLBACK (toolbar_update_appearance), self);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_UP_ICON_TOOLBAR,
                  G_CALLBACK (toolbar_update_appearance), self);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_EDIT_ICON_TOOLBAR,
                  G_CALLBACK (toolbar_update_appearance), self);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_RELOAD_ICON_TOOLBAR,
                  G_CALLBACK (toolbar_update_appearance), self);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_HOME_ICON_TOOLBAR,
                  G_CALLBACK (toolbar_update_appearance), self);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_COMPUTER_ICON_TOOLBAR,
                  G_CALLBACK (toolbar_update_appearance), self);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_SEARCH_ICON_TOOLBAR,
                  G_CALLBACK (toolbar_update_appearance), self);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_NEW_FOLDER_ICON_TOOLBAR,
                  G_CALLBACK (toolbar_update_appearance), self);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_OPEN_IN_TERMINAL_TOOLBAR,
                  G_CALLBACK (toolbar_update_appearance), self);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_ICON_VIEW_ICON_TOOLBAR,
                  G_CALLBACK (toolbar_update_appearance), self);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_LIST_VIEW_ICON_TOOLBAR,
                  G_CALLBACK (toolbar_update_appearance), self);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_COMPACT_VIEW_ICON_TOOLBAR,
                  G_CALLBACK (toolbar_update_appearance), self);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_SHOW_THUMBNAILS_TOOLBAR,
                  G_CALLBACK (toolbar_update_appearance), self);
    g_signal_connect_swapped (nemo_preferences,
                  "changed::" NEMO_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
                  G_CALLBACK (toolbar_update_appearance), self);

	toolbar_update_appearance (self);
}

static void
nemo_toolbar_init (NemoToolbar *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NEMO_TYPE_TOOLBAR,
						  NemoToolbarPriv);
	self->priv->show_main_bar = TRUE;	
}

static void
nemo_toolbar_get_property (GObject *object,
			       guint property_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	NemoToolbar *self = NEMO_TOOLBAR (object);

	switch (property_id) {
	case PROP_SHOW_LOCATION_ENTRY:
		g_value_set_boolean (value, self->priv->show_location_entry);
		break;
	case PROP_SHOW_MAIN_BAR:
		g_value_set_boolean (value, self->priv->show_main_bar);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nemo_toolbar_set_property (GObject *object,
			       guint property_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	NemoToolbar *self = NEMO_TOOLBAR (object);

	switch (property_id) {
	case PROP_ACTION_GROUP:
		self->priv->action_group = g_value_dup_object (value);
		break;
	case PROP_SHOW_LOCATION_ENTRY:
		nemo_toolbar_set_show_location_entry (self, g_value_get_boolean (value));
		break;
	case PROP_SHOW_MAIN_BAR:
		nemo_toolbar_set_show_main_bar (self, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nemo_toolbar_dispose (GObject *obj)
{
	NemoToolbar *self = NEMO_TOOLBAR (obj);

	g_clear_object (&self->priv->action_group);

	g_signal_handlers_disconnect_by_func (nemo_preferences,
					      toolbar_update_appearance, self);

	G_OBJECT_CLASS (nemo_toolbar_parent_class)->dispose (obj);
}

static void
nemo_toolbar_class_init (NemoToolbarClass *klass)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (klass);
	oclass->get_property = nemo_toolbar_get_property;
	oclass->set_property = nemo_toolbar_set_property;
	oclass->constructed = nemo_toolbar_constructed;
	oclass->dispose = nemo_toolbar_dispose;

	properties[PROP_ACTION_GROUP] =
		g_param_spec_object ("action-group",
				     "The action group",
				     "The action group to get actions from",
				     GTK_TYPE_ACTION_GROUP,
				     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
				     G_PARAM_STATIC_STRINGS);
	properties[PROP_SHOW_LOCATION_ENTRY] =
		g_param_spec_boolean ("show-location-entry",
				      "Whether to show the location entry",
				      "Whether to show the location entry instead of the pathbar",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	properties[PROP_SHOW_MAIN_BAR] =
		g_param_spec_boolean ("show-main-bar",
				      "Whether to show the main bar",
				      "Whether to show the main toolbar",
				      TRUE,
				      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	
	g_type_class_add_private (klass, sizeof (NemoToolbarClass));
	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

GtkWidget *
nemo_toolbar_new (GtkActionGroup *action_group)
{
	return g_object_new (NEMO_TYPE_TOOLBAR,
			     "action-group", action_group,
			     "orientation", GTK_ORIENTATION_VERTICAL,
			     NULL);
}

GtkWidget *
nemo_toolbar_get_path_bar (NemoToolbar *self)
{
	return self->priv->path_bar;
}

GtkWidget *
nemo_toolbar_get_location_bar (NemoToolbar *self)
{
	return self->priv->location_bar;
}

gboolean
nemo_toolbar_get_show_location_entry (NemoToolbar *self)
{
	return self->priv->show_location_entry;
}

void
nemo_toolbar_set_show_main_bar (NemoToolbar *self,
				    gboolean show_main_bar)
{
	if (show_main_bar != self->priv->show_main_bar) {
		self->priv->show_main_bar = show_main_bar;
		toolbar_update_appearance (self);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_MAIN_BAR]);
	}
}

void
nemo_toolbar_set_show_location_entry (NemoToolbar *self,
					  gboolean show_location_entry)
{
	if (show_location_entry != self->priv->show_location_entry) {
		self->priv->show_location_entry = show_location_entry;
		toolbar_update_appearance (self);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_LOCATION_ENTRY]);
	}
}

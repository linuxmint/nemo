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

#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-ui-utilities.h>

struct _NemoToolbarPriv {
	GtkToolbar *toolbar;
	GtkToolbar *secondary_toolbar;

	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;

	GtkWidget *path_bar;
	GtkWidget *location_bar;
	GtkWidget *search_bar;

	gboolean show_main_bar;
	gboolean show_location_entry;
	gboolean show_search_bar;
};

enum {
	PROP_ACTION_GROUP = 1,
	PROP_SHOW_LOCATION_ENTRY,
	PROP_SHOW_SEARCH_BAR,
	PROP_SHOW_MAIN_BAR,
	NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NemoToolbar, nemo_toolbar, GTK_TYPE_BOX);

static void
toolbar_update_appearance (NemoToolbar *self)
{
	GtkAction *action;
	GtkWidget *widgetitem;
	gboolean icon_toolbar;

	gboolean show_location_entry;

	show_location_entry = self->priv->show_location_entry ||
		g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_LOCATION_ENTRY);

	gtk_widget_set_visible (GTK_WIDGET(self->priv->toolbar),
				self->priv->show_main_bar);

	gtk_widget_set_visible (self->priv->location_bar,
				show_location_entry);
	gtk_widget_set_visible (self->priv->path_bar,
				!show_location_entry);

	gtk_widget_set_visible (self->priv->search_bar,
				self->priv->show_search_bar);


	widgetitem = gtk_ui_manager_get_widget (self->priv->ui_manager, "/Toolbar/Up");
	icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_UP_ICON_TOOLBAR);
	if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
	else {gtk_widget_show (GTK_WIDGET(widgetitem));}

	widgetitem = gtk_ui_manager_get_widget (self->priv->ui_manager, "/Toolbar/Reload");
	icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_RELOAD_ICON_TOOLBAR);
	if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
	else {gtk_widget_show (GTK_WIDGET(widgetitem));}

	widgetitem = gtk_ui_manager_get_widget (self->priv->ui_manager, "/SecondaryToolbar/Edit Location");
	icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_EDIT_ICON_TOOLBAR);
	if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
	else {gtk_widget_show (GTK_WIDGET(widgetitem));}

	widgetitem = gtk_ui_manager_get_widget (self->priv->ui_manager, "/Toolbar/Home");
	icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_HOME_ICON_TOOLBAR);
	if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
	else {gtk_widget_show (GTK_WIDGET(widgetitem));}

	widgetitem = gtk_ui_manager_get_widget (self->priv->ui_manager, "/Toolbar/Computer");
	icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_COMPUTER_ICON_TOOLBAR);
	if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
	else {gtk_widget_show (GTK_WIDGET(widgetitem));}

	widgetitem = gtk_ui_manager_get_widget (self->priv->ui_manager, "/SecondaryToolbar/Search");
	icon_toolbar = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_SEARCH_ICON_TOOLBAR);
	if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
	else {gtk_widget_show (GTK_WIDGET(widgetitem));}
}

static void
nemo_toolbar_constructed (GObject *obj)
{
	NemoToolbar *self = NEMO_TOOLBAR (obj);
	GtkToolItem *item;
	GtkBox *hbox;
	GtkToolbar *toolbar, *secondary_toolbar;
	GtkWidget *search;
	GtkStyleContext *context;

	GtkWidget *sep_space;

	G_OBJECT_CLASS (nemo_toolbar_parent_class)->constructed (obj);

	gtk_style_context_set_junction_sides (gtk_widget_get_style_context (GTK_WIDGET (self)),
					      GTK_JUNCTION_BOTTOM);

	/* add the UI */
	self->priv->ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_add_ui_from_resource (self->priv->ui_manager, "/org/nemo/nemo-toolbar-ui.xml", NULL);
	gtk_ui_manager_insert_action_group (self->priv->ui_manager, self->priv->action_group, 0);

	toolbar = GTK_TOOLBAR (gtk_ui_manager_get_widget (self->priv->ui_manager, "/Toolbar"));
	self->priv->toolbar = toolbar;
	
	secondary_toolbar = GTK_TOOLBAR (gtk_ui_manager_get_widget (self->priv->ui_manager, "/SecondaryToolbar"));
	self->priv->secondary_toolbar = secondary_toolbar;
		
	gtk_toolbar_set_icon_size (GTK_TOOLBAR (toolbar), GTK_ICON_SIZE_BUTTON);
	gtk_toolbar_set_icon_size (GTK_TOOLBAR (secondary_toolbar), GTK_ICON_SIZE_MENU);

	context = gtk_widget_get_style_context (GTK_WIDGET(toolbar));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_PRIMARY_TOOLBAR);
	
	context = gtk_widget_get_style_context (GTK_WIDGET(secondary_toolbar));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_PRIMARY_TOOLBAR);
	
	//search = gtk_ui_manager_get_widget (self->priv->ui_manager, "/Toolbar/Search");
	//gtk_style_context_add_class (gtk_widget_get_style_context (search), GTK_STYLE_CLASS_RAISED);
	//gtk_widget_set_name (search, "nemo-search-button");
    
    hbox = GTK_BOX(gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));

	gtk_box_pack_start (hbox, GTK_WIDGET(self->priv->toolbar), TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET(self->priv->toolbar));		
	
	gtk_toolbar_set_show_arrow (self->priv->secondary_toolbar, FALSE);
	gtk_box_pack_start (hbox, GTK_WIDGET(self->priv->secondary_toolbar), FALSE, TRUE, 0);	
	gtk_widget_show_all (GTK_WIDGET(self->priv->secondary_toolbar));	

	gtk_box_pack_start (GTK_BOX (self), GTK_WIDGET(hbox), TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET(hbox));

	hbox = GTK_BOX(gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
	gtk_widget_show (GTK_WIDGET(hbox));

	/* regular path bar */
	self->priv->path_bar = g_object_new (NEMO_TYPE_PATH_BAR, NULL);
    
    /* entry-like location bar */
	self->priv->location_bar = nemo_location_bar_new ();
	gtk_box_pack_start (GTK_BOX (hbox), self->priv->location_bar, TRUE, TRUE, 0);    
	gtk_box_pack_start (GTK_BOX (hbox), self->priv->path_bar, TRUE, TRUE, 0);
	
	item = gtk_tool_item_new ();
	gtk_tool_item_set_expand (item, TRUE);
	gtk_container_add (GTK_CONTAINER (item), GTK_WIDGET(hbox));
	/* append to the end of the toolbar so navigation buttons are at the beginning */
	gtk_toolbar_insert (GTK_TOOLBAR (self->priv->toolbar), item, 8);
	gtk_widget_show (GTK_WIDGET (item));

	/* search bar */
	self->priv->search_bar = nemo_search_bar_new ();
	gtk_box_pack_start (GTK_BOX (self), self->priv->search_bar, TRUE, TRUE, 0);

	g_signal_connect_swapped (nemo_preferences,
				  "changed::" NEMO_PREFERENCES_SHOW_LOCATION_ENTRY,
				  G_CALLBACK (toolbar_update_appearance), self);

	/* nemo patch */
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
				  "changed::" NEMO_PREFERENCES_SHOW_LABEL_SEARCH_ICON_TOOLBAR,
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
	case PROP_SHOW_SEARCH_BAR:
		g_value_set_boolean (value, self->priv->show_search_bar);
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
	case PROP_SHOW_SEARCH_BAR:
		nemo_toolbar_set_show_search_bar (self, g_value_get_boolean (value));
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

	g_clear_object (&self->priv->ui_manager);
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
	properties[PROP_SHOW_SEARCH_BAR] =
		g_param_spec_boolean ("show-search-bar",
				      "Whether to show the search bar",
				      "Whether to show the search bar beside the toolbar",
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

NemoToolbar *
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

GtkWidget *
nemo_toolbar_get_search_bar (NemoToolbar *self)
{
	return self->priv->search_bar;
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

void
nemo_toolbar_set_show_search_bar (NemoToolbar *self,
				      gboolean show_search_bar)
{
	if (show_search_bar != self->priv->show_search_bar) {
		self->priv->show_search_bar = show_search_bar;
		toolbar_update_appearance (self);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_SEARCH_BAR]);
	}
}

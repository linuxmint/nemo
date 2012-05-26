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

#include "nautilus-location-bar.h"
#include "nautilus-pathbar.h"
#include "nautilus-actions.h"
#include "nautilus-window-private.h"

#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-ui-utilities.h>

struct _NautilusToolbarPriv {
	GtkWidget *toolbar;

	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;

	GtkWidget *path_bar;
	GtkWidget *location_bar;
	GtkWidget *search_bar;

	GtkToolItem *back_forward;

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

G_DEFINE_TYPE (NautilusToolbar, nautilus_toolbar, GTK_TYPE_BOX);

static void
toolbar_update_appearance (NautilusToolbar *self)
{
	gboolean show_location_entry;

	show_location_entry = self->priv->show_location_entry ||
		g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY);

	gtk_widget_set_visible (self->priv->toolbar,
				self->priv->show_main_bar);

	gtk_widget_set_visible (self->priv->location_bar,
				show_location_entry);
	gtk_widget_set_visible (self->priv->path_bar,
				!show_location_entry);

	gtk_widget_set_visible (self->priv->search_bar,
				self->priv->show_search_bar);
}

static void
nautilus_toolbar_constructed (GObject *obj)
{
	NautilusToolbar *self = NAUTILUS_TOOLBAR (obj);
	GtkToolItem *item;
	GtkWidget *hbox, *toolbar;
	GtkStyleContext *context;
	GtkWidget *tool_button, *box;
	GtkToolItem *back_forward;
	GtkToolItem *search;
	GtkActionGroup *action_group;
	GtkAction *action;

	G_OBJECT_CLASS (nautilus_toolbar_parent_class)->constructed (obj);

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

	action_group = self->priv->action_group;

	/* Back */
	tool_button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (tool_button), gtk_image_new ());
	action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_BACK);
	gtk_activatable_set_related_action (GTK_ACTIVATABLE (tool_button),
					    action);
	gtk_button_set_label (GTK_BUTTON (tool_button), NULL);
	gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (tool_button));

	/* Forward */
	tool_button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (tool_button), gtk_image_new ());
	action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_FORWARD);
	gtk_activatable_set_related_action (GTK_ACTIVATABLE (tool_button),
					    action);
	gtk_button_set_label (GTK_BUTTON (tool_button), NULL);
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
	self->priv->location_bar = nautilus_location_bar_new ();
	gtk_box_pack_start (GTK_BOX (hbox), self->priv->location_bar, TRUE, TRUE, 0);

	item = gtk_tool_item_new ();
	gtk_tool_item_set_expand (item, TRUE);
	gtk_container_add (GTK_CONTAINER (item), hbox);
	gtk_container_add (GTK_CONTAINER (self->priv->toolbar), GTK_WIDGET (item));
	gtk_widget_show (GTK_WIDGET (item));

	/* search */
	search = gtk_tool_item_new ();
	tool_button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (tool_button), gtk_image_new ());
	action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_SEARCH);
	gtk_activatable_set_related_action (GTK_ACTIVATABLE (tool_button),
					    action);
	gtk_button_set_label (GTK_BUTTON (tool_button), NULL);
	gtk_widget_set_name (tool_button, "nautilus-search-button");
	gtk_container_add (GTK_CONTAINER (search), GTK_WIDGET (tool_button));
	gtk_container_add (GTK_CONTAINER (self->priv->toolbar), GTK_WIDGET (search));
	gtk_widget_show_all (GTK_WIDGET (search));
	gtk_widget_set_margin_left (GTK_WIDGET (search), 12);

	/* search bar */
	self->priv->search_bar = nautilus_search_bar_new ();
	gtk_box_pack_start (GTK_BOX (self), self->priv->search_bar, TRUE, TRUE, 0);

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
	self->priv->show_main_bar = TRUE;	
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
nautilus_toolbar_set_property (GObject *object,
			       guint property_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	NautilusToolbar *self = NAUTILUS_TOOLBAR (object);

	switch (property_id) {
	case PROP_ACTION_GROUP:
		self->priv->action_group = g_value_dup_object (value);
		break;
	case PROP_SHOW_LOCATION_ENTRY:
		nautilus_toolbar_set_show_location_entry (self, g_value_get_boolean (value));
		break;
	case PROP_SHOW_SEARCH_BAR:
		nautilus_toolbar_set_show_search_bar (self, g_value_get_boolean (value));
		break;
	case PROP_SHOW_MAIN_BAR:
		nautilus_toolbar_set_show_main_bar (self, g_value_get_boolean (value));
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

	g_clear_object (&self->priv->ui_manager);
	g_clear_object (&self->priv->action_group);

	g_signal_handlers_disconnect_by_func (nautilus_preferences,
					      toolbar_update_appearance, self);

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
	
	g_type_class_add_private (klass, sizeof (NautilusToolbarClass));
	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

GtkWidget *
nautilus_toolbar_new (GtkActionGroup *action_group)
{
	return g_object_new (NAUTILUS_TYPE_TOOLBAR,
			     "action-group", action_group,
			     "orientation", GTK_ORIENTATION_VERTICAL,
			     NULL);
}

GtkWidget *
nautilus_toolbar_get_path_bar (NautilusToolbar *self)
{
	return self->priv->path_bar;
}

GtkWidget *
nautilus_toolbar_get_location_bar (NautilusToolbar *self)
{
	return self->priv->location_bar;
}

GtkWidget *
nautilus_toolbar_get_search_bar (NautilusToolbar *self)
{
	return self->priv->search_bar;
}

void
nautilus_toolbar_set_show_main_bar (NautilusToolbar *self,
				    gboolean show_main_bar)
{
	if (show_main_bar != self->priv->show_main_bar) {
		self->priv->show_main_bar = show_main_bar;
		toolbar_update_appearance (self);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_MAIN_BAR]);
	}
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

void
nautilus_toolbar_set_show_search_bar (NautilusToolbar *self,
				      gboolean show_search_bar)
{
	if (show_search_bar != self->priv->show_search_bar) {
		self->priv->show_search_bar = show_search_bar;
		toolbar_update_appearance (self);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_SEARCH_BAR]);
	}
}

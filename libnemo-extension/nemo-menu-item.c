/*
 *  nemo-menu-item.c - Menu items exported by NemoMenuProvider
 *                         objects.
 *
 *  Copyright (C) 2003 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 * 
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

#include <config.h>
#include "nemo-menu.h"
#include "nemo-extension-i18n.h"

enum {
	ACTIVATE,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_NAME,
	PROP_LABEL,
	PROP_TIP,
	PROP_ICON,
	PROP_SENSITIVE,
	PROP_PRIORITY,
	PROP_MENU,
	LAST_PROP
};

struct _NemoMenuItemDetails {
	char *name;
	char *label;
	char *tip;
	char *icon;
	NemoMenu *menu;
	gboolean sensitive;
	gboolean priority;
};

static guint signals[LAST_SIGNAL];

static GObjectClass *parent_class = NULL;

/**
 * nemo_menu_item_new:
 * @name: the identifier for the menu item
 * @label: the user-visible label of the menu item
 * @tip: the tooltip of the menu item
 * @icon: the name of the icon to display in the menu item
 *
 * Creates a new menu item that can be added to the toolbar or to a contextual menu.
 *
 * Returns: a newly create #NemoMenuItem
 */
NemoMenuItem *
nemo_menu_item_new (const char *name,
			const char *label,
			const char *tip,
			const char *icon)
{
	NemoMenuItem *item;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (label != NULL, NULL);
	g_return_val_if_fail (tip != NULL, NULL);

	item = g_object_new (NEMO_TYPE_MENU_ITEM, 
			     "name", name,
			     "label", label,
			     "tip", tip,
			     "icon", icon,
			     NULL);

	return item;
}

/**
 * nemo_menu_item_activate:
 * @item: pointer to a #NemoMenuItem
 *
 * emits the activate signal.
 */
void
nemo_menu_item_activate (NemoMenuItem *item)
{
	g_signal_emit (item, signals[ACTIVATE], 0);
}

/**
 * nemo_menu_item_set_submenu:
 * @item: pointer to a #NemoMenuItem
 * @menu: pointer to a #NemoMenu to attach to the button
 *
 * Attachs a menu to the given #NemoMenuItem.
 */
void
nemo_menu_item_set_submenu (NemoMenuItem *item, NemoMenu *menu)
{
	g_object_set (item, "menu", menu, NULL);
}

static void
nemo_menu_item_get_property (GObject *object,
				 guint param_id,
				 GValue *value,
				 GParamSpec *pspec)
{
	NemoMenuItem *item;
	
	item = NEMO_MENU_ITEM (object);
	
	switch (param_id) {
	case PROP_NAME :
		g_value_set_string (value, item->details->name);
		break;
	case PROP_LABEL :
		g_value_set_string (value, item->details->label);
		break;
	case PROP_TIP :
		g_value_set_string (value, item->details->tip);
		break;
	case PROP_ICON :
		g_value_set_string (value, item->details->icon);
		break;
	case PROP_SENSITIVE :
		g_value_set_boolean (value, item->details->sensitive);
		break;
	case PROP_PRIORITY :
		g_value_set_boolean (value, item->details->priority);
		break;
	case PROP_MENU :
		g_value_set_object (value, item->details->menu);
		break;
	default :
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
nemo_menu_item_set_property (GObject *object,
				 guint param_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
	NemoMenuItem *item;
	
	item = NEMO_MENU_ITEM (object);

	switch (param_id) {
	case PROP_NAME :
		g_free (item->details->name);
		item->details->name = g_strdup (g_value_get_string (value));
		g_object_notify (object, "name");
		break;
	case PROP_LABEL :
		g_free (item->details->label);
		item->details->label = g_strdup (g_value_get_string (value));
		g_object_notify (object, "label");
		break;
	case PROP_TIP :
		g_free (item->details->tip);
		item->details->tip = g_strdup (g_value_get_string (value));
		g_object_notify (object, "tip");
		break;
	case PROP_ICON :
		g_free (item->details->icon);
		item->details->icon = g_strdup (g_value_get_string (value));
		g_object_notify (object, "icon");
		break;
	case PROP_SENSITIVE :
		item->details->sensitive = g_value_get_boolean (value);
		g_object_notify (object, "sensitive");
		break;
	case PROP_PRIORITY :
		item->details->priority = g_value_get_boolean (value);
		g_object_notify (object, "priority");
		break;
	case PROP_MENU :
		if (item->details->menu) {
			g_object_unref (item->details->menu);
		}
		item->details->menu = g_object_ref (g_value_get_object (value));
		g_object_notify (object, "menu");
		break;
	default :
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
nemo_menu_item_finalize (GObject *object)
{
	NemoMenuItem *item;

	item = NEMO_MENU_ITEM (object);

	g_free (item->details->name);
	g_free (item->details->label);
	g_free (item->details->tip);
	g_free (item->details->icon);
	if (item->details->menu) {
		g_object_unref (item->details->menu);
	}

	g_free (item->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nemo_menu_item_instance_init (NemoMenuItem *item)
{
	item->details = g_new0 (NemoMenuItemDetails, 1);
	item->details->sensitive = TRUE;
	item->details->menu = NULL;
}

static void
nemo_menu_item_class_init (NemoMenuItemClass *class)
{
	parent_class = g_type_class_peek_parent (class);
	
	G_OBJECT_CLASS (class)->finalize = nemo_menu_item_finalize;
	G_OBJECT_CLASS (class)->get_property = nemo_menu_item_get_property;
	G_OBJECT_CLASS (class)->set_property = nemo_menu_item_set_property;

        signals[ACTIVATE] =
                g_signal_new ("activate",
                              G_TYPE_FROM_CLASS (class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (NemoMenuItemClass, 
					       activate),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0); 

	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Name",
							      "Name of the item",
							      NULL,
							      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_LABEL,
					 g_param_spec_string ("label",
							      "Label",
							      "Label to display to the user",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_TIP,
					 g_param_spec_string ("tip",
							      "Tip",
							      "Tooltip for the menu item",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_ICON,
					 g_param_spec_string ("icon",
							      "Icon",
							      "Name of the icon to display in the menu item",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_SENSITIVE,
					 g_param_spec_boolean ("sensitive",
							       "Sensitive",
							       "Whether the menu item is sensitive",
							       TRUE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_PRIORITY,
					 g_param_spec_boolean ("priority",
							       "Priority",
							       "Show priority text in toolbars",
							       TRUE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_MENU,
					 g_param_spec_object ("menu",
							       "Menu",
							       "The menu belonging to this item. May be null.",
							       NEMO_TYPE_MENU,
							       G_PARAM_READWRITE));
}

GType 
nemo_menu_item_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		const GTypeInfo info = {
			sizeof (NemoMenuItemClass),
			NULL,
			NULL,
			(GClassInitFunc)nemo_menu_item_class_init,
			NULL,
			NULL,
			sizeof (NemoMenuItem),
			0,
			(GInstanceInitFunc)nemo_menu_item_instance_init
		};
		
		type = g_type_register_static 
			(G_TYPE_OBJECT, 
			 "NemoMenuItem",
			 &info, 0);
	}

	return type;
}


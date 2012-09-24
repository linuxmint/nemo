/*
 *  nemo-menu.h - Menus exported by NemoMenuProvider objects.
 *
 *  Copyright (C) 2005 Raffaele Sandrini
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
 *  Author:  Raffaele Sandrini <rasa@gmx.ch>
 *
 */

#include <config.h>
#include "nemo-menu.h"
#include "nemo-extension-i18n.h"

#include <glib.h>

#define NEMO_MENU_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NEMO_TYPE_MENU, NemoMenuPrivate))
G_DEFINE_TYPE (NemoMenu, nemo_menu, G_TYPE_OBJECT);

struct _NemoMenuPrivate {
	GList *item_list;
};

void
nemo_menu_append_item (NemoMenu *menu, NemoMenuItem *item)
{
	g_return_if_fail (menu != NULL);
	g_return_if_fail (item != NULL);
	
	menu->priv->item_list = g_list_append (menu->priv->item_list, g_object_ref (item));
}

/**
 * nemo_menu_get_items:
 * @menu: a #NemoMenu
 *
 * Returns: (element-type NemoMenuItem) (transfer full): the provided #NemoMenuItem list
 */
GList *
nemo_menu_get_items (NemoMenu *menu)
{
	GList *item_list;

	g_return_val_if_fail (menu != NULL, NULL);
	
	item_list = g_list_copy (menu->priv->item_list);
	g_list_foreach (item_list, (GFunc)g_object_ref, NULL);
	
	return item_list;
}

/**
 * nemo_menu_item_list_free:
 * @item_list: (element-type NemoMenuItem): a list of #NemoMenuItem
 *
 */
void
nemo_menu_item_list_free (GList *item_list)
{
	g_return_if_fail (item_list != NULL);
	
	g_list_foreach (item_list, (GFunc)g_object_unref, NULL);
	g_list_free (item_list);
}

/* Type initialization */

static void
nemo_menu_finalize (GObject *object)
{
	NemoMenu *menu = NEMO_MENU (object);

	if (menu->priv->item_list) {
		g_list_free (menu->priv->item_list);
	}

	G_OBJECT_CLASS (nemo_menu_parent_class)->finalize (object);
}

static void
nemo_menu_init (NemoMenu *menu)
{
	menu->priv = NEMO_MENU_GET_PRIVATE (menu);

	menu->priv->item_list = NULL;
}

static void
nemo_menu_class_init (NemoMenuClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (NemoMenuPrivate));
	
	object_class->finalize = nemo_menu_finalize;
}

/* public constructors */

NemoMenu *
nemo_menu_new (void)
{
	NemoMenu *obj;
	
	obj = NEMO_MENU (g_object_new (NEMO_TYPE_MENU, NULL));
	
	return obj;
}

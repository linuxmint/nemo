/*
 *  nemo-menu.h - Menus exported by NemoMenuProvider objects.
 *
 *  Copyright (C) 2005 Raffaele Sandrini
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
 *           Raffaele Sandrini <rasa@gmx.ch>
 *
 */

#ifndef NEMO_MENU_H
#define NEMO_MENU_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include "nemo-extension-types.h"

G_BEGIN_DECLS

/* NemoMenu defines */
#define NEMO_TYPE_MENU         nemo_menu_get_type ()
G_DECLARE_FINAL_TYPE (NemoMenu, nemo_menu, NEMO, MENU, GObject)
/* NemoMenuItem defines */
#define NEMO_TYPE_MENU_ITEM    nemo_menu_item_get_type()
G_DECLARE_FINAL_TYPE (NemoMenuItem, nemo_menu_item, NEMO, MENU_ITEM, GObject)

/* NemoMenu methods */
NemoMenu *	nemo_menu_new	(void);

void	nemo_menu_append_item	(NemoMenu      *menu,
					 NemoMenuItem  *item);
GList*	nemo_menu_get_items		(NemoMenu *menu);
void	nemo_menu_item_list_free	(GList *item_list);

/* NemoMenuItem methods */
NemoMenuItem *nemo_menu_item_new           (const char       *name,
						    const char       *label,
						    const char       *tip,
						    const char       *icon);

NemoMenuItem *nemo_menu_item_new_widget (const char *name,
                                         GtkWidget  *widget_a,
                                         GtkWidget  *widget_b);

NemoMenuItem *nemo_menu_item_new_separator (const char *name);

void nemo_menu_item_set_widget_a (NemoMenuItem *item, GtkWidget *widget);
void nemo_menu_item_set_widget_b (NemoMenuItem *item, GtkWidget *widget);

void              nemo_menu_item_activate      (NemoMenuItem *item);
void              nemo_menu_item_set_submenu   (NemoMenuItem *item,
						    NemoMenu     *menu);
/* NemoMenuItem has the following properties:
 *   name (string)        - the identifier for the menu item
 *   label (string)       - the user-visible label of the menu item
 *   tip (string)         - the tooltip of the menu item 
 *   icon (string)        - the name of the icon to display in the menu item
 *   sensitive (boolean)  - whether the menu item is sensitive or not
 *   priority (boolean)   - used for toolbar items, whether to show priority
 *                          text.
 *   menu (NemoMenu)  - The menu belonging to this item. May be null.
 *   widget (GtkWidget) - The optional widget to use in place of a normal menu entr
 */

G_END_DECLS

#endif /* NEMO_MENU_H */

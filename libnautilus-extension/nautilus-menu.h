/*
 *  nautilus-menu.h - Menus exported by NautilusMenuProvider objects.
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
 *  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 * 
 *  Author:  Dave Camp <dave@ximian.com>
 *           Raffaele Sandrini <rasa@gmx.ch>
 *
 */

#ifndef NAUTILUS_MENU_H
#define NAUTILUS_MENU_H

#include <glib-object.h>
#include "nautilus-extension-types.h"


G_BEGIN_DECLS

/* NautilusMenu defines */
#define NAUTILUS_TYPE_MENU         (nautilus_menu_get_type ())
#define NAUTILUS_MENU(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_MENU, NautilusMenu))
#define NAUTILUS_MENU_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_MENU, NautilusMenuClass))
#define NAUTILUS_IS_MENU(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_MENU))
#define NAUTILUS_IS_MENU_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_MENU))
#define NAUTILUS_MENU_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_MENU, NautilusMenuClass))
/* NautilusMenuItem defines */
#define NAUTILUS_TYPE_MENU_ITEM            (nautilus_menu_item_get_type())
#define NAUTILUS_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_MENU_ITEM, NautilusMenuItem))
#define NAUTILUS_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_MENU_ITEM, NautilusMenuItemClass))
#define NAUTILUS_MENU_IS_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_MENU_ITEM))
#define NAUTILUS_MENU_IS_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_MENU_ITEM))
#define NAUTILUS_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NAUTILUS_TYPE_MENU_ITEM, NautilusMenuItemClass))


/* NautilusMenu types */
typedef struct _NautilusMenu		NautilusMenu;
typedef struct _NautilusMenuPrivate	NautilusMenuPrivate;
typedef struct _NautilusMenuClass	NautilusMenuClass;
/* NautilusMenuItem types */
typedef struct _NautilusMenuItem        NautilusMenuItem;
typedef struct _NautilusMenuItemDetails NautilusMenuItemDetails;
typedef struct _NautilusMenuItemClass   NautilusMenuItemClass;


/* NautilusMenu structs */
struct _NautilusMenu {
	GObject parent;
	NautilusMenuPrivate *priv;
};

struct _NautilusMenuClass {
	GObjectClass parent_class;
};

/* NautilusMenuItem structs */
struct _NautilusMenuItem {
	GObject parent;

	NautilusMenuItemDetails *details;
};

struct _NautilusMenuItemClass {
	GObjectClass parent;

	void (*activate) (NautilusMenuItem *item);
};


/* NautilusMenu methods */
GType		nautilus_menu_get_type	(void);
NautilusMenu *	nautilus_menu_new	(void);

void	nautilus_menu_append_item	(NautilusMenu      *menu,
					 NautilusMenuItem  *item);
GList*	nautilus_menu_get_items		(NautilusMenu *menu);
void	nautilus_menu_item_list_free	(GList *item_list);

/* NautilusMenuItem methods */
GType             nautilus_menu_item_get_type      (void);
NautilusMenuItem *nautilus_menu_item_new           (const char       *name,
						    const char       *label,
						    const char       *tip,
						    const char       *icon);

void              nautilus_menu_item_activate      (NautilusMenuItem *item);
void              nautilus_menu_item_set_submenu   (NautilusMenuItem *item,
						    NautilusMenu     *menu);
/* NautilusMenuItem has the following properties:
 *   name (string)        - the identifier for the menu item
 *   label (string)       - the user-visible label of the menu item
 *   tip (string)         - the tooltip of the menu item 
 *   icon (string)        - the name of the icon to display in the menu item
 *   sensitive (boolean)  - whether the menu item is sensitive or not
 *   priority (boolean)   - used for toolbar items, whether to show priority
 *                          text.
 *   menu (NautilusMenu)  - The menu belonging to this item. May be null.
 */

G_END_DECLS

#endif /* NAUTILUS_MENU_H */

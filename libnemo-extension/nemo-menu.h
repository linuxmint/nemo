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
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 *  Author:  Dave Camp <dave@ximian.com>
 *           Raffaele Sandrini <rasa@gmx.ch>
 *
 */

#ifndef NEMO_MENU_H
#define NEMO_MENU_H

#include <glib-object.h>
#include "nemo-extension-types.h"


G_BEGIN_DECLS

/* NemoMenu defines */
#define NEMO_TYPE_MENU         (nemo_menu_get_type ())
#define NEMO_MENU(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_MENU, NemoMenu))
#define NEMO_MENU_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NEMO_TYPE_MENU, NemoMenuClass))
#define NEMO_IS_MENU(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NEMO_TYPE_MENU))
#define NEMO_IS_MENU_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NEMO_TYPE_MENU))
#define NEMO_MENU_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NEMO_TYPE_MENU, NemoMenuClass))
/* NemoMenuItem defines */
#define NEMO_TYPE_MENU_ITEM            (nemo_menu_item_get_type())
#define NEMO_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_MENU_ITEM, NemoMenuItem))
#define NEMO_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_MENU_ITEM, NemoMenuItemClass))
#define NEMO_MENU_IS_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_MENU_ITEM))
#define NEMO_MENU_IS_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), NEMO_TYPE_MENU_ITEM))
#define NEMO_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NEMO_TYPE_MENU_ITEM, NemoMenuItemClass))


/* NemoMenu types */
typedef struct _NemoMenu		NemoMenu;
typedef struct _NemoMenuPrivate	NemoMenuPrivate;
typedef struct _NemoMenuClass	NemoMenuClass;
/* NemoMenuItem types */
typedef struct _NemoMenuItem        NemoMenuItem;
typedef struct _NemoMenuItemDetails NemoMenuItemDetails;
typedef struct _NemoMenuItemClass   NemoMenuItemClass;


/* NemoMenu structs */
struct _NemoMenu {
	GObject parent;
	NemoMenuPrivate *priv;
};

struct _NemoMenuClass {
	GObjectClass parent_class;
};

/* NemoMenuItem structs */
struct _NemoMenuItem {
	GObject parent;

	NemoMenuItemDetails *details;
};

struct _NemoMenuItemClass {
	GObjectClass parent;

	void (*activate) (NemoMenuItem *item);
};


/* NemoMenu methods */
GType		nemo_menu_get_type	(void);
NemoMenu *	nemo_menu_new	(void);

void	nemo_menu_append_item	(NemoMenu      *menu,
					 NemoMenuItem  *item);
GList*	nemo_menu_get_items		(NemoMenu *menu);
void	nemo_menu_item_list_free	(GList *item_list);

/* NemoMenuItem methods */
GType             nemo_menu_item_get_type      (void);
NemoMenuItem *nemo_menu_item_new           (const char       *name,
						    const char       *label,
						    const char       *tip,
						    const char       *icon);

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
 */

G_END_DECLS

#endif /* NEMO_MENU_H */

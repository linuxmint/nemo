/*unity-quicklist-handler.c: handle Unity quicklists
 *
 * Copyright (C) 2012 Canonical
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Didier Roche <didrocks@ubuntu.com>
 *
 */

#include <config.h>

#include "unity-quicklist-handler.h"

struct _UnityQuicklistHandlerPriv {
    GList *launcher_entries;
};

G_DEFINE_TYPE (UnityQuicklistHandler, unity_quicklist_handler, G_TYPE_OBJECT);

static UnityQuicklistHandler *unity_quicklist_handler_singleton = NULL;

GList *
unity_quicklist_get_launcher_entries (UnityQuicklistHandler *self)
{
	return self->priv->launcher_entries;
}

gboolean
unity_quicklist_handler_menuitem_is_progress_item (DbusmenuMenuitem *ql)
{
	g_return_val_if_fail(ql, FALSE);
	const gchar *label = dbusmenu_menuitem_property_get (ql, DBUSMENU_MENUITEM_PROP_LABEL);

	return ((g_strcmp0 (label, (const gchar*)UNITY_QUICKLIST_SHOW_COPY_DIALOG) == 0) ||
	        (g_strcmp0 (label, (const gchar*)UNITY_QUICKLIST_CANCEL_COPY) == 0));
}

gboolean
unity_quicklist_handler_menuitem_is_bookmark_item (DbusmenuMenuitem *ql)
{
	g_return_val_if_fail(ql, FALSE);
	return (!unity_quicklist_handler_menuitem_is_progress_item(ql));
}

void
unity_quicklist_handler_append_menuitem (UnityLauncherEntry *entry, DbusmenuMenuitem *elem)
{
	g_return_if_fail (entry);

	GList *children, *l;
	int position = 0;
	DbusmenuMenuitem *ql = unity_launcher_entry_get_quicklist (entry);

	gboolean is_bookmark = unity_quicklist_handler_menuitem_is_bookmark_item (elem);
	gboolean is_progress = unity_quicklist_handler_menuitem_is_progress_item (elem);

	if (!ql) {
		ql = dbusmenu_menuitem_new ();
		unity_launcher_entry_set_quicklist (entry, ql);
	}

	children = dbusmenu_menuitem_get_children (ql);
	for (l = children; l; l = l->next) {
		DbusmenuMenuitem *child = l->data;
		/* set quicklist groups together, and bookmarks group after progress group.
		   bookmarks elements are ordered alphabetically */
		if ((is_bookmark && unity_quicklist_handler_menuitem_is_bookmark_item (child) &&
                (g_strcmp0 (dbusmenu_menuitem_property_get (child, DBUSMENU_MENUITEM_PROP_LABEL), dbusmenu_menuitem_property_get (elem, DBUSMENU_MENUITEM_PROP_LABEL)) < 0)) ||
			(is_progress && unity_quicklist_handler_menuitem_is_progress_item (child)) ||
			(is_progress && unity_quicklist_handler_menuitem_is_bookmark_item (child)))
			position++;
		else
			break;
	}

	dbusmenu_menuitem_child_add_position (ql, elem, position);
}

static void
unity_quicklist_handler_dispose (GObject *obj)
{
	UnityQuicklistHandler *self = UNITY_QUICKLIST_HANDLER (obj);

	if (self->priv->launcher_entries) {
		g_list_free_full (self->priv->launcher_entries, g_object_unref);
		self->priv->launcher_entries = NULL;
	}

	G_OBJECT_CLASS (unity_quicklist_handler_parent_class)->dispose (obj);
}

static void
unity_quicklist_handler_launcher_entry_add (UnityQuicklistHandler *self,
                                            const gchar *entry_id)
{
	GList **entries;
	UnityLauncherEntry *entry;

	entries = &(self->priv->launcher_entries);
	entry = unity_launcher_entry_get_for_desktop_id (entry_id);

	if (entry) {
		*entries = g_list_prepend (*entries, entry);
	}
}

static void
unity_quicklist_handler_init (UnityQuicklistHandler *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, UNITY_TYPE_QUICKLIST_HANDLER,
	                                          UnityQuicklistHandlerPriv);

	unity_quicklist_handler_launcher_entry_add (self, "nemo.desktop");
	unity_quicklist_handler_launcher_entry_add (self, "nemo-home.desktop");
	g_return_if_fail (g_list_length (self->priv->launcher_entries) != 0);
}

static void
unity_quicklist_handler_class_init (UnityQuicklistHandlerClass *klass)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (klass);
	oclass->dispose = unity_quicklist_handler_dispose;

	g_type_class_add_private (klass, sizeof (UnityQuicklistHandlerPriv));
}

UnityQuicklistHandler *
unity_quicklist_handler_get_singleton (void)
{
	if (!unity_quicklist_handler_singleton)
		unity_quicklist_handler_singleton = unity_quicklist_handler_new ();
	return unity_quicklist_handler_singleton;
}

UnityQuicklistHandler *
unity_quicklist_handler_new (void)
{
	return g_object_new (UNITY_TYPE_QUICKLIST_HANDLER, NULL);
}


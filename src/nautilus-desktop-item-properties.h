/*
 *  fm-ditem-page.h - A property page for desktop items
 * 
 *  Copyright (C) 2004 James Willcox
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Authors: James Willcox <james@gnome.org>
 * 
 */

#ifndef NAUTILUS_DESKTOP_ITEM_PROPERTIES_H
#define NAUTILUS_DESKTOP_ITEM_PROPERTIES_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* This is a mis-nomer. Launcher editables initially were displayed on separate
 * a property notebook page, which implemented the NautilusPropertyPageProvider
 * interface.
 *
 * Nowadays, they are displayed on the "Basic" page, so just the setup
 * routines are left.
 */

GtkWidget *nautilus_desktop_item_properties_make_box (GtkSizeGroup *label_size_group,
                                                      GList *files);
gboolean   nautilus_desktop_item_properties_should_show (GList *files);

G_END_DECLS

#endif /* NAUTILUS_DESKTOP_ITEM_PROPERTIES_H */

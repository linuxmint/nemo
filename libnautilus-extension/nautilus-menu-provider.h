/*
 *  nautilus-menu-provider.h - Interface for Nautilus extensions that 
 *                             provide context menu items.
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
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

/* This interface is implemented by Nautilus extensions that want to
 * add context menu entries to files.  Extensions are called when
 * Nautilus constructs the context menu for a file.  They are passed a
 * list of NautilusFileInfo objects which holds the current selection */

#ifndef NAUTILUS_MENU_PROVIDER_H
#define NAUTILUS_MENU_PROVIDER_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include "nautilus-extension-types.h"
#include "nautilus-file-info.h"
#include "nautilus-menu.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_MENU_PROVIDER           (nautilus_menu_provider_get_type ())
#define NAUTILUS_MENU_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_MENU_PROVIDER, NautilusMenuProvider))
#define NAUTILUS_IS_MENU_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_MENU_PROVIDER))
#define NAUTILUS_MENU_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NAUTILUS_TYPE_MENU_PROVIDER, NautilusMenuProviderIface))

typedef struct _NautilusMenuProvider       NautilusMenuProvider;
typedef struct _NautilusMenuProviderIface  NautilusMenuProviderIface;

struct _NautilusMenuProviderIface {
	GTypeInterface g_iface;

	GList *(*get_file_items)       (NautilusMenuProvider *provider,
					GtkWidget            *window,
					GList                *files);
	GList *(*get_background_items) (NautilusMenuProvider *provider,
					GtkWidget            *window,
					NautilusFileInfo     *current_folder);
};

/* Interface Functions */
GType                   nautilus_menu_provider_get_type             (void);
GList                  *nautilus_menu_provider_get_file_items       (NautilusMenuProvider *provider,
								     GtkWidget            *window,
								     GList                *files);
GList                  *nautilus_menu_provider_get_background_items (NautilusMenuProvider *provider,
								     GtkWidget            *window,
								     NautilusFileInfo     *current_folder);

/* This function emit a signal to inform nautilus that its item list has changed. */
void                    nautilus_menu_provider_emit_items_updated_signal (NautilusMenuProvider *provider);

G_END_DECLS

#endif

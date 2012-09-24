/*
 *  nemo-menu-provider.h - Interface for Nemo extensions that 
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
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 * 
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

/* This interface is implemented by Nemo extensions that want to
 * add context menu entries to files.  Extensions are called when
 * Nemo constructs the context menu for a file.  They are passed a
 * list of NemoFileInfo objects which holds the current selection */

#ifndef NEMO_MENU_PROVIDER_H
#define NEMO_MENU_PROVIDER_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include "nemo-extension-types.h"
#include "nemo-file-info.h"
#include "nemo-menu.h"

G_BEGIN_DECLS

#define NEMO_TYPE_MENU_PROVIDER           (nemo_menu_provider_get_type ())
#define NEMO_MENU_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_MENU_PROVIDER, NemoMenuProvider))
#define NEMO_IS_MENU_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_MENU_PROVIDER))
#define NEMO_MENU_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NEMO_TYPE_MENU_PROVIDER, NemoMenuProviderIface))

typedef struct _NemoMenuProvider       NemoMenuProvider;
typedef struct _NemoMenuProviderIface  NemoMenuProviderIface;

struct _NemoMenuProviderIface {
	GTypeInterface g_iface;

	GList *(*get_file_items)       (NemoMenuProvider *provider,
					GtkWidget            *window,
					GList                *files);
	GList *(*get_background_items) (NemoMenuProvider *provider,
					GtkWidget            *window,
					NemoFileInfo     *current_folder);
};

/* Interface Functions */
GType                   nemo_menu_provider_get_type             (void);
GList                  *nemo_menu_provider_get_file_items       (NemoMenuProvider *provider,
								     GtkWidget            *window,
								     GList                *files);
GList                  *nemo_menu_provider_get_background_items (NemoMenuProvider *provider,
								     GtkWidget            *window,
								     NemoFileInfo     *current_folder);

/* This function emit a signal to inform nemo that its item list has changed. */
void                    nemo_menu_provider_emit_items_updated_signal (NemoMenuProvider *provider);

G_END_DECLS

#endif

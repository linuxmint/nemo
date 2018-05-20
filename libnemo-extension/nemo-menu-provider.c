/*
 *  nemo-property-page-provider.c - Interface for Nemo extensions 
 *                                      that provide context menu items
 *                                      for files.
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
#include "nemo-menu-provider.h"

#include <glib-object.h>

G_DEFINE_INTERFACE (NemoMenuProvider, nemo_menu_provider, G_TYPE_OBJECT)

/**
 * SECTION:nemo-menu-provider
 * @Title: NemoMenuProvider
 * @Short_description: Allows additional menu items to be added to context menus.
 *
 * This interface allows custom menu entries to be inserted in the selection and
 * background context menus.  Submenus and separators can also be generated.
 *
 **/

static void
nemo_menu_provider_default_init (NemoMenuProviderInterface *klass)
{
    static gboolean initialized = FALSE;

    if (!initialized)
    {
        /* This signal should be emited each time the extension modify the list of menu items */
        g_signal_new ("items_updated",
            NEMO_TYPE_MENU_PROVIDER,
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
        initialized = TRUE;
    }
}

/**
 * nemo_menu_provider_get_file_items:
 * @provider: a #NemoMenuProvider
 * @window: the parent #GtkWidget window
 * @files: (element-type NemoFileInfo): a list of #NemoFileInfo
 *
 * Returns: (element-type NemoMenuItem) (transfer full): the provided list of #NemoMenuItem
 */
GList *
nemo_menu_provider_get_file_items (NemoMenuProvider *provider,
				       GtkWidget *window,
				       GList *files)
{
	g_return_val_if_fail (NEMO_IS_MENU_PROVIDER (provider), NULL);

	if (NEMO_MENU_PROVIDER_GET_IFACE (provider)->get_file_items) {
		return NEMO_MENU_PROVIDER_GET_IFACE (provider)->get_file_items 
			(provider, window, files);
	} else {
		return NULL;
	}
}

/**
 * nemo_menu_provider_get_background_items:
 * @provider: a #NemoMenuProvider
 * @window: the parent #GtkWidget window
 * @current_folder: the folder for which background items are requested
 *
 * Returns: (element-type NemoMenuItem) (transfer full): the provided list of #NemoMenuItem
 */
GList *
nemo_menu_provider_get_background_items (NemoMenuProvider *provider,
					     GtkWidget *window,
					     NemoFileInfo *current_folder)
{
	g_return_val_if_fail (NEMO_IS_MENU_PROVIDER (provider), NULL);
	g_return_val_if_fail (NEMO_IS_FILE_INFO (current_folder), NULL);

	if (NEMO_MENU_PROVIDER_GET_IFACE (provider)->get_background_items) {
		return NEMO_MENU_PROVIDER_GET_IFACE (provider)->get_background_items 
			(provider, window, current_folder);
	} else {
		return NULL;
	}
}

/* This function emit a signal to inform nemo that its item list has changed */
void
nemo_menu_provider_emit_items_updated_signal (NemoMenuProvider* provider)
{
	g_return_if_fail (NEMO_IS_MENU_PROVIDER (provider));

	g_signal_emit_by_name (provider, "items_updated");
}


/*
 *  nemo-selection-provider.c - Interface for Nemo extensions that
 *                              want to be notified when the file
 *                              selection changes.
 *
 *  Copyright (C) 2026
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
 */

#include <config.h>
#include "nemo-selection-provider.h"
#include <glib-object.h>

/**
 * SECTION:nemo-selection-provider
 * @Title: NemoSelectionProvider
 * @Short_description: Notifies extensions when the file selection changes.
 *
 * Extensions implementing this interface will have their selection_changed
 * method called whenever the user changes the file selection in Nemo.
 **/

G_DEFINE_INTERFACE (NemoSelectionProvider, nemo_selection_provider, G_TYPE_OBJECT)

static void
nemo_selection_provider_default_init (NemoSelectionProviderInterface *klass)
{
    /* No signals needed — Nemo calls selection_changed() directly */
}

/**
 * nemo_selection_provider_selection_changed:
 * @provider: a #NemoSelectionProvider
 * @window: the parent #GtkWidget window
 * @files: (element-type NemoFileInfo): a list of currently selected #NemoFileInfo
 *
 * Called by Nemo when the file selection changes. Dispatches to the
 * extension's selection_changed() implementation.
 */
void
nemo_selection_provider_selection_changed (NemoSelectionProvider *provider,
                                           GtkWidget             *window,
                                           GList                 *files)
{
    g_return_if_fail (NEMO_IS_SELECTION_PROVIDER (provider));

    if (NEMO_SELECTION_PROVIDER_GET_IFACE (provider)->selection_changed) {
        NEMO_SELECTION_PROVIDER_GET_IFACE (provider)->selection_changed
            (provider, window, files);
    }
}

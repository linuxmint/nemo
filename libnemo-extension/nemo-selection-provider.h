/*
 *  nemo-selection-provider.h - Interface for Nemo extensions that
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

/* This interface is implemented by Nemo extensions that want to
 * be notified when the user's file selection changes.  Extensions
 * receive a list of NemoFileInfo objects representing the current
 * selection, and the GtkWidget of the parent window. */

#ifndef NEMO_SELECTION_PROVIDER_H
#define NEMO_SELECTION_PROVIDER_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include "nemo-extension-types.h"
#include "nemo-file-info.h"

G_BEGIN_DECLS

#define NEMO_TYPE_SELECTION_PROVIDER (nemo_selection_provider_get_type ())

G_DECLARE_INTERFACE (NemoSelectionProvider, nemo_selection_provider,
                     NEMO, SELECTION_PROVIDER,
                     GObject)

typedef NemoSelectionProviderInterface NemoSelectionProviderIface;

struct _NemoSelectionProviderInterface {
    GTypeInterface g_iface;

    void (*selection_changed) (NemoSelectionProvider *provider,
                               GtkWidget             *window,
                               GList                 *files);
};

/* Called by Nemo when the selection changes. files is a list of NemoFileInfo. */
void nemo_selection_provider_selection_changed (NemoSelectionProvider *provider,
                                               GtkWidget             *window,
                                               GList                 *files);

G_END_DECLS

#endif /* NEMO_SELECTION_PROVIDER_H */

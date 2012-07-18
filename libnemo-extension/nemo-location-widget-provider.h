/*
 *  nemo-info-provider.h - Interface for Nemo extensions that 
 *                             provide info about files.
 *
 *  Copyright (C) 2003 Novell, Inc.
 *  Copyright (C) 2005 Red Hat, Inc.
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
 *           Alexander Larsson <alexl@redhat.com>
 *
 */

/* This interface is implemented by Nemo extensions that want to 
 * provide extra location widgets for a particular location.
 * Extensions are called when Nemo displays a location.
 */

#ifndef NEMO_LOCATION_WIDGET_PROVIDER_H
#define NEMO_LOCATION_WIDGET_PROVIDER_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include "nemo-extension-types.h"

G_BEGIN_DECLS

#define NEMO_TYPE_LOCATION_WIDGET_PROVIDER           (nemo_location_widget_provider_get_type ())
#define NEMO_LOCATION_WIDGET_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_LOCATION_WIDGET_PROVIDER, NemoLocationWidgetProvider))
#define NEMO_IS_LOCATION_WIDGET_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_LOCATION_WIDGET_PROVIDER))
#define NEMO_LOCATION_WIDGET_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NEMO_TYPE_LOCATION_WIDGET_PROVIDER, NemoLocationWidgetProviderIface))

typedef struct _NemoLocationWidgetProvider       NemoLocationWidgetProvider;
typedef struct _NemoLocationWidgetProviderIface  NemoLocationWidgetProviderIface;

struct _NemoLocationWidgetProviderIface {
	GTypeInterface g_iface;

	GtkWidget * (*get_widget) (NemoLocationWidgetProvider *provider,
				   const char                     *uri,
				   GtkWidget                      *window);
};

/* Interface Functions */
GType       nemo_location_widget_provider_get_type      (void);
GtkWidget * nemo_location_widget_provider_get_widget    (NemoLocationWidgetProvider     *provider,
							     const char                         *uri,
							     GtkWidget                          *window);
G_END_DECLS

#endif

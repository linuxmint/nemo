/*
 *  nautilus-location-widget-provider.c - Interface for Nautilus
                 extensions that provide extra widgets for a location
 *
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
 *  Author:  Alexander Larsson <alexl@redhat.com>
 *
 */

#include <config.h>
#include "nautilus-location-widget-provider.h"

#include <glib-object.h>

static void
nautilus_location_widget_provider_base_init (gpointer g_class)
{
}

GType                   
nautilus_location_widget_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		const GTypeInfo info = {
			sizeof (NautilusLocationWidgetProviderIface),
			nautilus_location_widget_provider_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, 
					       "NautilusLocationWidgetProvider",
					       &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

/**
 * nautilus_location_widget_provider_get_widget:
 * @provider: a #NautilusLocationWidgetProvider
 * @uri: the URI of the location
 * @window: parent #GtkWindow
 *
 * Returns: (transfer none): the location widget for @provider at @uri
 */
GtkWidget *
nautilus_location_widget_provider_get_widget (NautilusLocationWidgetProvider     *provider,
					      const char                         *uri,
					      GtkWidget                          *window)
{
	g_return_val_if_fail (NAUTILUS_IS_LOCATION_WIDGET_PROVIDER (provider), NULL);

	return NAUTILUS_LOCATION_WIDGET_PROVIDER_GET_IFACE (provider)->get_widget 
		(provider, uri, window);

}				       

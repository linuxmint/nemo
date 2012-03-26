/*
 *  nautilus-info-provider.c - Interface for Nautilus extensions that 
 *                             provide info about files.
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

#include <config.h>
#include "nautilus-info-provider.h"

#include <glib-object.h>

static void
nautilus_info_provider_base_init (gpointer g_class)
{
}

GType                   
nautilus_info_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		const GTypeInfo info = {
			sizeof (NautilusInfoProviderIface),
			nautilus_info_provider_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, 
					       "NautilusInfoProvider",
					       &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

NautilusOperationResult 
nautilus_info_provider_update_file_info (NautilusInfoProvider *provider,
					 NautilusFileInfo *file,
					 GClosure *update_complete,
					 NautilusOperationHandle **handle)
{
	g_return_val_if_fail (NAUTILUS_IS_INFO_PROVIDER (provider),
			      NAUTILUS_OPERATION_FAILED);
	g_return_val_if_fail (NAUTILUS_INFO_PROVIDER_GET_IFACE (provider)->update_file_info != NULL,
			      NAUTILUS_OPERATION_FAILED);
	g_return_val_if_fail (update_complete != NULL, 
			      NAUTILUS_OPERATION_FAILED);
	g_return_val_if_fail (handle != NULL, NAUTILUS_OPERATION_FAILED);

	return NAUTILUS_INFO_PROVIDER_GET_IFACE (provider)->update_file_info 
		(provider, file, update_complete, handle);
}

void
nautilus_info_provider_cancel_update (NautilusInfoProvider *provider,
				      NautilusOperationHandle *handle)
{
	g_return_if_fail (NAUTILUS_IS_INFO_PROVIDER (provider));
	g_return_if_fail (NAUTILUS_INFO_PROVIDER_GET_IFACE (provider)->cancel_update != NULL);
	g_return_if_fail (NAUTILUS_INFO_PROVIDER_GET_IFACE (provider)->cancel_update != NULL);
	g_return_if_fail (handle != NULL);

	NAUTILUS_INFO_PROVIDER_GET_IFACE (provider)->cancel_update (provider,
								    handle);
}

void
nautilus_info_provider_update_complete_invoke (GClosure *update_complete,
					       NautilusInfoProvider *provider,
					       NautilusOperationHandle *handle,
					       NautilusOperationResult result)
{
	GValue args[3] = { { 0, } };
	GValue return_val = { 0, };
	
	g_return_if_fail (update_complete != NULL);
	g_return_if_fail (NAUTILUS_IS_INFO_PROVIDER (provider));

	g_value_init (&args[0], NAUTILUS_TYPE_INFO_PROVIDER);
	g_value_init (&args[1], G_TYPE_POINTER);
	g_value_init (&args[2], NAUTILUS_TYPE_OPERATION_RESULT);

	g_value_set_object (&args[0], provider);
	g_value_set_pointer (&args[1], handle);
	g_value_set_enum (&args[2], result);

	g_closure_invoke (update_complete, &return_val, 3, args, NULL);

	g_value_unset (&args[0]);
	g_value_unset (&args[1]);
	g_value_unset (&args[2]);
}

					       

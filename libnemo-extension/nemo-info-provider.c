/*
 *  nemo-info-provider.c - Interface for Nemo extensions that 
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
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 * 
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

#include <config.h>
#include "nemo-info-provider.h"

#include <glib-object.h>

G_DEFINE_INTERFACE (NemoInfoProvider, nemo_info_provider, G_TYPE_OBJECT)

/**
 * SECTION:nemo-info-provider
 * @Title: NemoInfoProvider
 * @Short_description: An interface to allow collection of additional file info.
 *
 * This interface can be used to collect additional file info, generally used
 * together with a #NemoColumnProvider.  It can be used as a synchronous or async
 * interface.
 *
 * Additional, it can act in the background and notify Nemo when information has
 * changed from some external source.
 **/

static void
nemo_info_provider_default_init (NemoInfoProviderInterface *klass)
{
}

NemoOperationResult 
nemo_info_provider_update_file_info (NemoInfoProvider     *provider,
                                     NemoFileInfo         *file,
                                     GClosure             *update_complete,
                                     NemoOperationHandle **handle)
{
	g_return_val_if_fail (NEMO_IS_INFO_PROVIDER (provider),
			      NEMO_OPERATION_FAILED);
	g_return_val_if_fail (NEMO_INFO_PROVIDER_GET_IFACE (provider)->update_file_info != NULL,
			      NEMO_OPERATION_FAILED);
	g_return_val_if_fail (update_complete != NULL, 
			      NEMO_OPERATION_FAILED);
	g_return_val_if_fail (handle != NULL, NEMO_OPERATION_FAILED);

	return NEMO_INFO_PROVIDER_GET_IFACE (provider)->update_file_info 
		(provider, file, update_complete, handle);
}

void
nemo_info_provider_cancel_update (NemoInfoProvider    *provider,
                                  NemoOperationHandle *handle)
{
	g_return_if_fail (NEMO_IS_INFO_PROVIDER (provider));
	g_return_if_fail (NEMO_INFO_PROVIDER_GET_IFACE (provider)->cancel_update != NULL);
	g_return_if_fail (handle != NULL);

	NEMO_INFO_PROVIDER_GET_IFACE (provider)->cancel_update (provider,
								    handle);
}

void
nemo_info_provider_update_complete_invoke (GClosure            *update_complete,
                                           NemoInfoProvider    *provider,
                                           NemoOperationHandle *handle,
                                           NemoOperationResult  result)
{
	GValue args[3] = { { 0, } };
	GValue return_val = { 0, };
	
	g_return_if_fail (update_complete != NULL);
	g_return_if_fail (NEMO_IS_INFO_PROVIDER (provider));

	g_value_init (&args[0], NEMO_TYPE_INFO_PROVIDER);
	g_value_init (&args[1], G_TYPE_POINTER);
	g_value_init (&args[2], G_TYPE_INT);

	g_value_set_object (&args[0], provider);
	g_value_set_pointer (&args[1], handle);
	g_value_set_int (&args[2], result);

	g_closure_invoke (update_complete, &return_val, 3, args, NULL);

	g_value_unset (&args[0]);
	g_value_unset (&args[1]);
	g_value_unset (&args[2]);
}


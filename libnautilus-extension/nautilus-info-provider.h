/*
 *  nautilus-info-provider.h - Interface for Nautilus extensions that 
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

/* This interface is implemented by Nautilus extensions that want to 
 * provide information about files.  Extensions are called when Nautilus 
 * needs information about a file.  They are passed a NautilusFileInfo 
 * object which should be filled with relevant information */

#ifndef NAUTILUS_INFO_PROVIDER_H
#define NAUTILUS_INFO_PROVIDER_H

#include <glib-object.h>
#include "nautilus-extension-types.h"
#include "nautilus-file-info.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_INFO_PROVIDER           (nautilus_info_provider_get_type ())
#define NAUTILUS_INFO_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_INFO_PROVIDER, NautilusInfoProvider))
#define NAUTILUS_IS_INFO_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_INFO_PROVIDER))
#define NAUTILUS_INFO_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NAUTILUS_TYPE_INFO_PROVIDER, NautilusInfoProviderIface))

typedef struct _NautilusInfoProvider       NautilusInfoProvider;
typedef struct _NautilusInfoProviderIface  NautilusInfoProviderIface;

typedef void (*NautilusInfoProviderUpdateComplete) (NautilusInfoProvider    *provider,
						    NautilusOperationHandle *handle,
						    NautilusOperationResult  result,
						    gpointer                 user_data);

struct _NautilusInfoProviderIface {
	GTypeInterface g_iface;

	NautilusOperationResult (*update_file_info) (NautilusInfoProvider     *provider,
						     NautilusFileInfo         *file,
						     GClosure                 *update_complete,
						     NautilusOperationHandle **handle);
	void                    (*cancel_update)    (NautilusInfoProvider     *provider,
						     NautilusOperationHandle  *handle);
};

/* Interface Functions */
GType                   nautilus_info_provider_get_type               (void);
NautilusOperationResult nautilus_info_provider_update_file_info       (NautilusInfoProvider     *provider,
								       NautilusFileInfo         *file,
								       GClosure                 *update_complete,
								       NautilusOperationHandle **handle);
void                    nautilus_info_provider_cancel_update          (NautilusInfoProvider     *provider,
								       NautilusOperationHandle  *handle);



/* Helper functions for implementations */
void                    nautilus_info_provider_update_complete_invoke (GClosure                 *update_complete,
								       NautilusInfoProvider     *provider,
								       NautilusOperationHandle  *handle,
								       NautilusOperationResult   result);

G_END_DECLS

#endif

/*
 *  nemo-info-provider.h - Interface for Nemo extensions that 
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

/* This interface is implemented by Nemo extensions that want to 
 * provide information about files.  Extensions are called when Nemo 
 * needs information about a file.  They are passed a NemoFileInfo 
 * object which should be filled with relevant information */

#ifndef NEMO_INFO_PROVIDER_H
#define NEMO_INFO_PROVIDER_H

#include <glib-object.h>
#include "nemo-extension-types.h"
#include "nemo-file-info.h"

G_BEGIN_DECLS

#define NEMO_TYPE_INFO_PROVIDER           (nemo_info_provider_get_type ())
#define NEMO_INFO_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_INFO_PROVIDER, NemoInfoProvider))
#define NEMO_IS_INFO_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_INFO_PROVIDER))
#define NEMO_INFO_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NEMO_TYPE_INFO_PROVIDER, NemoInfoProviderIface))

typedef struct _NemoInfoProvider       NemoInfoProvider;
typedef struct _NemoInfoProviderIface  NemoInfoProviderIface;

typedef void (*NemoInfoProviderUpdateComplete) (NemoInfoProvider    *provider,
						    NemoOperationHandle *handle,
						    NemoOperationResult  result,
						    gpointer                 user_data);

struct _NemoInfoProviderIface {
	GTypeInterface g_iface;

	NemoOperationResult (*update_file_info) (NemoInfoProvider     *provider,
						     NemoFileInfo         *file,
						     GClosure                 *update_complete,
						     NemoOperationHandle **handle);
	void                    (*cancel_update)    (NemoInfoProvider     *provider,
						     NemoOperationHandle  *handle);
};

/* Interface Functions */
GType                   nemo_info_provider_get_type               (void);
NemoOperationResult nemo_info_provider_update_file_info       (NemoInfoProvider     *provider,
								       NemoFileInfo         *file,
								       GClosure                 *update_complete,
								       NemoOperationHandle **handle);
void                    nemo_info_provider_cancel_update          (NemoInfoProvider     *provider,
								       NemoOperationHandle  *handle);



/* Helper functions for implementations */
void                    nemo_info_provider_update_complete_invoke (GClosure                 *update_complete,
								       NemoInfoProvider     *provider,
								       NemoOperationHandle  *handle,
								       NemoOperationResult   result);

G_END_DECLS

#endif

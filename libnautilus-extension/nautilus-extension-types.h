/*
 *  nautilus-info-provider.h - Type definitions for Nautilus extensions
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
 *  Author: Dave Camp <dave@ximian.com>
 * 
 */

/* This interface is implemented by Nautilus extensions that want to 
 * provide information about files.  Extensions are called when Nautilus 
 * needs information about a file.  They are passed a NautilusFileInfo 
 * object which should be filled with relevant information */

#ifndef NAUTILUS_EXTENSION_TYPES_H
#define NAUTILUS_EXTENSION_TYPES_H

#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_OPERATION_RESULT (nautilus_operation_result_get_type ())

/* Handle for asynchronous interfaces.  These are opaque handles that must
 * be unique within an extension object.  These are returned by operations
 * that return NAUTILUS_OPERATION_IN_PROGRESS */
typedef struct _NautilusOperationHandle NautilusOperationHandle;

typedef enum {
	/* Returned if the call succeeded, and the extension is done 
	 * with the request */
	NAUTILUS_OPERATION_COMPLETE,

	/* Returned if the call failed */
	NAUTILUS_OPERATION_FAILED,

	/* Returned if the extension has begun an async operation. 
	 * If this is returned, the extension must set the handle 
	 * parameter and call the callback closure when the 
	 * operation is complete. */
	NAUTILUS_OPERATION_IN_PROGRESS
} NautilusOperationResult;

GType nautilus_operation_result_get_type (void);

void nautilus_module_initialize (GTypeModule  *module);
void nautilus_module_shutdown   (void);
void nautilus_module_list_types (const GType **types,
				 int          *num_types);

G_END_DECLS

#endif

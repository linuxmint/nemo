/*
 *  nautilus-extension-types.c - Type definitions for Nautilus extensions
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

#include <config.h>
#include "nautilus-extension-types.h"


GType
nautilus_operation_result_get_type (void)
{
	static GType type = 0;
	if (type == 0) {
		static const GEnumValue values[] = {
			{ 
				NAUTILUS_OPERATION_COMPLETE, 
				"NAUTILUS_OPERATION_COMPLETE",
				"complete",
			}, 
			{
				NAUTILUS_OPERATION_FAILED,
				"NAUTILUS_OPERATION_FAILED",
				"failed",
			},
			{
				NAUTILUS_OPERATION_IN_PROGRESS,
				"NAUTILUS_OPERATION_IN_PROGRESS",
				"in_progress",
			},
			{ 0, NULL, NULL }
		};
		
		type = g_enum_register_static ("NautilusOperationResult", 
					       values);
	}

	return type;
}

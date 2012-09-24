/*
 *  nemo-extension-types.c - Type definitions for Nemo extensions
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
 *  Author: Dave Camp <dave@ximian.com>
 * 
 */

#include <config.h>
#include "nemo-extension-types.h"


GType
nemo_operation_result_get_type (void)
{
	static GType type = 0;
	if (type == 0) {
		static const GEnumValue values[] = {
			{ 
				NEMO_OPERATION_COMPLETE, 
				"NEMO_OPERATION_COMPLETE",
				"complete",
			}, 
			{
				NEMO_OPERATION_FAILED,
				"NEMO_OPERATION_FAILED",
				"failed",
			},
			{
				NEMO_OPERATION_IN_PROGRESS,
				"NEMO_OPERATION_IN_PROGRESS",
				"in_progress",
			},
			{ 0, NULL, NULL }
		};
		
		type = g_enum_register_static ("NemoOperationResult", 
					       values);
	}

	return type;
}

/*
 *  nemo-module.h - Interface to nemo extensions
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

#ifndef NEMO_MODULE_H
#define NEMO_MODULE_H

#include <glib-object.h>

G_BEGIN_DECLS

void   nemo_module_setup                   (void);
GList *nemo_module_get_extensions_for_type (GType  type);
void   nemo_module_extension_list_free     (GList *list);


/* Add a type to the module interface - allows nemo to add its own modules
 * without putting them in separate shared libraries */
void   nemo_module_add_type                (GType  type);

G_END_DECLS

#endif

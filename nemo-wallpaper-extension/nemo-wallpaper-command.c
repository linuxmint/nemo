/*
 *	Nemo Wallpaper extension
 *
 *	Copyright (C) 2005 Adam Israel
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Author: Adam Israel <adam@battleaxe.net> 
 */

#include <config.h>
#include <libnemo-extension/nemo-extension-types.h>
#include <libnemo-extension/nemo-column-provider.h>
#include <glib/gi18n-lib.h>
 #include "nemo-nwe.h"

void
nemo_module_initialize (GTypeModule*module)
{
	nemo_nwe_register_type (module);

}


void
nemo_module_shutdown (void)
{
}


void 
nemo_module_list_types (const GType **types,
			    int          *num_types)
{
	static GType type_list[1];
	
	type_list[0] = NEMO_TYPE_NWE;
	*types = type_list;
	*num_types = 1;
}

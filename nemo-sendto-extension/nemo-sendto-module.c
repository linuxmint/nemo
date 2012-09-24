/*
 *  Nemo SendTo
 *
 *  Copyright (C) 2005 Roberto Majadas
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 *  Author: Roberto Majadas <roberto.majadas@openshine.com>
 *
 */

#include <config.h>
#include <libnemo-extension/nemo-extension-types.h>
#include <libnemo-extension/nemo-column-provider.h>
#include <glib/gi18n-lib.h>
#include "nemo-nste.h"


void
nemo_module_initialize (GTypeModule*module)
{
	nemo_nste_register_type (module);

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
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

	type_list[0] = NEMO_TYPE_NSTE;
	*types = type_list;

	*num_types = 1;
}


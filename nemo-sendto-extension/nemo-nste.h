/*
 *  Nemo SendTo extension
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
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Roberto Majadas <roberto.majadas@openshine.com>
 *
 */

#ifndef NEMO_NSTE_H
#define NEMO_NSTE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define NEMO_TYPE_NSTE  (nemo_nste_get_type ())
#define NEMO_NSTE(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_NSTE, NemoNste))
#define NEMO_IS_NSTE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), NEMO_TYPE_NSTE))

typedef struct _NemoNste      NemoNste;
typedef struct _NemoNsteClass NemoNsteClass;

struct _NemoNste {
	GObject  __parent;
	gboolean nst_present;
};

struct _NemoNsteClass {
	GObjectClass __parent;
};

GType nemo_nste_get_type      (void);
void  nemo_nste_register_type (GTypeModule *module);

G_END_DECLS

#endif /* NEMO_NSTE_H */

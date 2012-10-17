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
 
#ifndef NEMO_NWE_H
#define NEMO_NWE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define NEMO_TYPE_NWE  (nemo_nwe_get_type ())
#define NEMO_NWE(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_NWE, NemoNwe))
#define NEMO_IS_NWE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), NEMO_TYPE_NWE))

typedef struct _NemoNwe      NemoNwe;
typedef struct _NemoNweClass NemoNweClass;

struct _NemoNwe {
	GObject __parent;
};

struct _NemoNweClass {
	GObjectClass __parent;
};

GType nemo_nwe_get_type      (void);
void  nemo_nwe_register_type (GTypeModule *module);


G_END_DECLS

#endif

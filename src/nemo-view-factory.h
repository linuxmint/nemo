/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-view-factory.h: register and create NemoViews
 
   Copyright (C) 2004 Red Hat Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef NEMO_VIEW_FACTORY_H
#define NEMO_VIEW_FACTORY_H

#include <string.h>

#include <gio/gio.h>

#include "nemo-view.h"
#include "nemo-window-slot.h"

G_BEGIN_DECLS

typedef struct _NemoViewInfo NemoViewInfo;

struct _NemoViewInfo {
	char *id;
	char *view_combo_label;               /* Foo View (used in preferences dialog and navigation combo) */
	char *view_menu_label_with_mnemonic;  /* View -> _Foo (this is the "_Foo" part) */
	char *error_label;                 /* The foo view encountered an error. */
	char *startup_error_label;         /* The foo view encountered an error while starting up. */
	char *display_location_label;      /* Display this location with the foo view. */
	NemoView * (*create) (NemoWindowSlot *slot);
	/* BONOBOTODO: More args here */
	gboolean (*supports_uri) (const char *uri,
				  GFileType file_type,
				  const char *mime_type);
};


void                    nemo_view_factory_register          (NemoViewInfo   *view_info);
const NemoViewInfo *nemo_view_factory_lookup            (const char         *id);
NemoView *          nemo_view_factory_create            (const char         *id,
								 NemoWindowSlot *slot);
gboolean                nemo_view_factory_view_supports_uri (const char         *id,
								 GFile              *location,
								 GFileType          file_type,
								 const char         *mime_type);
GList *                 nemo_view_factory_get_views_for_uri (const char         *uri,
								 GFileType          file_type,
								 const char         *mime_type);




G_END_DECLS

#endif /* NEMO_VIEW_FACTORY_H */

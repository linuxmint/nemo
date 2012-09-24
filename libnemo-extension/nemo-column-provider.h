/*
 *  nemo-column-provider.h - Interface for Nemo extensions that 
 *                               provide column descriptions.
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
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

/* This interface is implemented by Nemo extensions that want to
 * add columns to the list view and details to the icon view.
 * Extensions are asked for a list of columns to display.  Each
 * returned column refers to a string attribute which can be filled in
 * by NemoInfoProvider */

#ifndef NEMO_COLUMN_PROVIDER_H
#define NEMO_COLUMN_PROVIDER_H

#include <glib-object.h>
#include "nemo-extension-types.h"
#include "nemo-column.h"

G_BEGIN_DECLS

#define NEMO_TYPE_COLUMN_PROVIDER           (nemo_column_provider_get_type ())
#define NEMO_COLUMN_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_COLUMN_PROVIDER, NemoColumnProvider))
#define NEMO_IS_COLUMN_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_COLUMN_PROVIDER))
#define NEMO_COLUMN_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NEMO_TYPE_COLUMN_PROVIDER, NemoColumnProviderIface))

typedef struct _NemoColumnProvider       NemoColumnProvider;
typedef struct _NemoColumnProviderIface  NemoColumnProviderIface;

struct _NemoColumnProviderIface {
	GTypeInterface g_iface;

	GList *(*get_columns) (NemoColumnProvider *provider);
};

/* Interface Functions */
GType                   nemo_column_provider_get_type       (void);
GList                  *nemo_column_provider_get_columns    (NemoColumnProvider *provider);

G_END_DECLS

#endif

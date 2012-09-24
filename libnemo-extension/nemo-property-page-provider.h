/*
 *  nemo-property-page-provider.h - Interface for Nemo extensions
 *                                      that provide property pages.
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
 * add property page to property dialogs.  Extensions are called when 
 * Nemo needs property pages for a selection.  They are passed a 
 * list of NemoFileInfo objects for which information should
 * be displayed  */

#ifndef NEMO_PROPERTY_PAGE_PROVIDER_H
#define NEMO_PROPERTY_PAGE_PROVIDER_H

#include <glib-object.h>
#include "nemo-extension-types.h"
#include "nemo-file-info.h"
#include "nemo-property-page.h"

G_BEGIN_DECLS

#define NEMO_TYPE_PROPERTY_PAGE_PROVIDER           (nemo_property_page_provider_get_type ())
#define NEMO_PROPERTY_PAGE_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_PROPERTY_PAGE_PROVIDER, NemoPropertyPageProvider))
#define NEMO_IS_PROPERTY_PAGE_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_PROPERTY_PAGE_PROVIDER))
#define NEMO_PROPERTY_PAGE_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NEMO_TYPE_PROPERTY_PAGE_PROVIDER, NemoPropertyPageProviderIface))

typedef struct _NemoPropertyPageProvider       NemoPropertyPageProvider;
typedef struct _NemoPropertyPageProviderIface  NemoPropertyPageProviderIface;

struct _NemoPropertyPageProviderIface {
	GTypeInterface g_iface;

	GList *(*get_pages) (NemoPropertyPageProvider     *provider,
			     GList                    *files);
};

/* Interface Functions */
GType                   nemo_property_page_provider_get_type  (void);
GList                  *nemo_property_page_provider_get_pages (NemoPropertyPageProvider *provider,
								   GList                        *files);

G_END_DECLS

#endif

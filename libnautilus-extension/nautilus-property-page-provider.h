/*
 *  nautilus-property-page-provider.h - Interface for Nautilus extensions
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
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

/* This interface is implemented by Nautilus extensions that want to 
 * add property page to property dialogs.  Extensions are called when 
 * Nautilus needs property pages for a selection.  They are passed a 
 * list of NautilusFileInfo objects for which information should
 * be displayed  */

#ifndef NAUTILUS_PROPERTY_PAGE_PROVIDER_H
#define NAUTILUS_PROPERTY_PAGE_PROVIDER_H

#include <glib-object.h>
#include "nautilus-extension-types.h"
#include "nautilus-file-info.h"
#include "nautilus-property-page.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PROPERTY_PAGE_PROVIDER           (nautilus_property_page_provider_get_type ())
#define NAUTILUS_PROPERTY_PAGE_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_PROPERTY_PAGE_PROVIDER, NautilusPropertyPageProvider))
#define NAUTILUS_IS_PROPERTY_PAGE_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_PROPERTY_PAGE_PROVIDER))
#define NAUTILUS_PROPERTY_PAGE_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NAUTILUS_TYPE_PROPERTY_PAGE_PROVIDER, NautilusPropertyPageProviderIface))

typedef struct _NautilusPropertyPageProvider       NautilusPropertyPageProvider;
typedef struct _NautilusPropertyPageProviderIface  NautilusPropertyPageProviderIface;

struct _NautilusPropertyPageProviderIface {
	GTypeInterface g_iface;

	GList *(*get_pages) (NautilusPropertyPageProvider     *provider,
			     GList                    *files);
};

/* Interface Functions */
GType                   nautilus_property_page_provider_get_type  (void);
GList                  *nautilus_property_page_provider_get_pages (NautilusPropertyPageProvider *provider,
								   GList                        *files);

G_END_DECLS

#endif

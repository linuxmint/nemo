/*
 *  Copyright (C) 2012 Red Hat, Inc.
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
 */

#ifndef NAUTILUS_SEARCH_PROVIDER_H
#define NAUTILUS_SEARCH_PROVIDER_H

#include <glib-object.h>
#include <libnautilus-private/nautilus-query.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SEARCH_PROVIDER           (nautilus_search_provider_get_type ())
#define NAUTILUS_SEARCH_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_SEARCH_PROVIDER, NautilusSearchProvider))
#define NAUTILUS_IS_SEARCH_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_SEARCH_PROVIDER))
#define NAUTILUS_SEARCH_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NAUTILUS_TYPE_SEARCH_PROVIDER, NautilusSearchProviderIface))

typedef struct _NautilusSearchProvider       NautilusSearchProvider;
typedef struct _NautilusSearchProviderIface  NautilusSearchProviderIface;

struct _NautilusSearchProviderIface {
        GTypeInterface g_iface;

        /* VTable */
        void (*set_query) (NautilusSearchProvider *provider, NautilusQuery *query);
        void (*start) (NautilusSearchProvider *provider);
        void (*stop) (NautilusSearchProvider *provider);

        /* Signals */
        void (*hits_added) (NautilusSearchProvider *provider, GList *hits);
        void (*hits_subtracted) (NautilusSearchProvider *provider, GList *hits);
        void (*finished) (NautilusSearchProvider *provider);
        void (*error) (NautilusSearchProvider *provider, const char *error_message);
};

GType          nautilus_search_provider_get_type        (void) G_GNUC_CONST;

/* Interface Functions */
void           nautilus_search_provider_set_query       (NautilusSearchProvider *provider,
                                                         NautilusQuery *query);
void           nautilus_search_provider_start           (NautilusSearchProvider *provider);
void           nautilus_search_provider_stop            (NautilusSearchProvider *provider);

void           nautilus_search_provider_hits_added      (NautilusSearchProvider *provider,
                                                         GList *hits);
void           nautilus_search_provider_hits_subtracted (NautilusSearchProvider *provider,
                                                         GList *hits);
void           nautilus_search_provider_finished        (NautilusSearchProvider *provider);
void           nautilus_search_provider_error           (NautilusSearchProvider *provider,
                                                         const char *error_message);

G_END_DECLS

#endif

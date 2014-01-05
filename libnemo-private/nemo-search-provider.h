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

#ifndef NEMO_SEARCH_PROVIDER_H
#define NEMO_SEARCH_PROVIDER_H

#include <glib-object.h>
#include <libnemo-private/nemo-query.h>

G_BEGIN_DECLS

#define NEMO_TYPE_SEARCH_PROVIDER           (nemo_search_provider_get_type ())
#define NEMO_SEARCH_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_SEARCH_PROVIDER, NemoSearchProvider))
#define NEMO_IS_SEARCH_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_SEARCH_PROVIDER))
#define NEMO_SEARCH_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NEMO_TYPE_SEARCH_PROVIDER, NemoSearchProviderIface))

typedef struct _NemoSearchProvider       NemoSearchProvider;
typedef struct _NemoSearchProviderIface  NemoSearchProviderIface;

struct _NemoSearchProviderIface {
        GTypeInterface g_iface;

        /* VTable */
        void (*set_query) (NemoSearchProvider *provider, NemoQuery *query);
        void (*start) (NemoSearchProvider *provider);
        void (*stop) (NemoSearchProvider *provider);

        /* Signals */
        void (*hits_added) (NemoSearchProvider *provider, GList *hits);
        void (*hits_subtracted) (NemoSearchProvider *provider, GList *hits);
        void (*finished) (NemoSearchProvider *provider);
        void (*error) (NemoSearchProvider *provider, const char *error_message);
};

GType          nemo_search_provider_get_type        (void) G_GNUC_CONST;

/* Interface Functions */
void           nemo_search_provider_set_query       (NemoSearchProvider *provider,
                                                         NemoQuery *query);
void           nemo_search_provider_start           (NemoSearchProvider *provider);
void           nemo_search_provider_stop            (NemoSearchProvider *provider);

void           nemo_search_provider_hits_added      (NemoSearchProvider *provider,
                                                         GList *hits);
void           nemo_search_provider_hits_subtracted (NemoSearchProvider *provider,
                                                         GList *hits);
void           nemo_search_provider_finished        (NemoSearchProvider *provider);
void           nemo_search_provider_error           (NemoSearchProvider *provider,
                                                         const char *error_message);

G_END_DECLS

#endif

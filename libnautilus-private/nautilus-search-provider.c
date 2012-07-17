/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
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
 *
 */

#include <config.h>
#include "nautilus-search-provider.h"

#include <glib-object.h>

enum {
       HITS_ADDED,
       HITS_SUBTRACTED,
       FINISHED,
       ERROR,
       LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static void nautilus_search_provider_base_init (gpointer g_iface);

GType
nautilus_search_provider_get_type (void)
{
	static GType search_provider_type = 0;

	if (!search_provider_type) {
		const GTypeInfo search_provider_info = {
			sizeof (NautilusSearchProviderIface), /* class_size */
			nautilus_search_provider_base_init,   /* base_init */
			NULL,           /* base_finalize */
			NULL,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			0,
			0,              /* n_preallocs */
			NULL
		};

		search_provider_type = g_type_register_static (G_TYPE_INTERFACE,
							       "NautilusSearchProvider",
							       &search_provider_info,
							       0);

		g_type_interface_add_prerequisite (search_provider_type, G_TYPE_OBJECT);
	}

	return search_provider_type;
}

static void
nautilus_search_provider_base_init (gpointer g_iface)
{
	static gboolean initialized = FALSE;

	if (initialized) {
		return;
	}

	signals[HITS_ADDED] = g_signal_new ("hits-added",
					    NAUTILUS_TYPE_SEARCH_PROVIDER,
					    G_SIGNAL_RUN_LAST,
					    G_STRUCT_OFFSET (NautilusSearchProviderIface, hits_added),
					    NULL, NULL,
					    g_cclosure_marshal_VOID__POINTER,
					    G_TYPE_NONE, 1,
					    G_TYPE_POINTER);
	signals[HITS_SUBTRACTED] = g_signal_new ("hits-subtracted",
						 NAUTILUS_TYPE_SEARCH_PROVIDER,
						 G_SIGNAL_RUN_LAST,
						 G_STRUCT_OFFSET (NautilusSearchProviderIface, hits_subtracted),
						 NULL, NULL,
						 g_cclosure_marshal_VOID__POINTER,
						 G_TYPE_NONE, 1,
						 G_TYPE_POINTER);

	signals[FINISHED] = g_signal_new ("finished",
					  NAUTILUS_TYPE_SEARCH_PROVIDER,
					  G_SIGNAL_RUN_LAST,
					  G_STRUCT_OFFSET (NautilusSearchProviderIface, finished),
					  NULL, NULL,
					  g_cclosure_marshal_VOID__VOID,
					  G_TYPE_NONE, 0);

	signals[ERROR] = g_signal_new ("error",
				       NAUTILUS_TYPE_SEARCH_PROVIDER,
				       G_SIGNAL_RUN_LAST,
				       G_STRUCT_OFFSET (NautilusSearchProviderIface, error),
				       NULL, NULL,
				       g_cclosure_marshal_VOID__STRING,
				       G_TYPE_NONE, 1,
				       G_TYPE_STRING);

	initialized = TRUE;
}

void
nautilus_search_provider_set_query (NautilusSearchProvider *provider, NautilusQuery *query)
{
	g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));
	g_return_if_fail (NAUTILUS_SEARCH_PROVIDER_GET_IFACE (provider)->set_query != NULL);

	NAUTILUS_SEARCH_PROVIDER_GET_IFACE (provider)->set_query (provider, query);
}

void
nautilus_search_provider_start (NautilusSearchProvider *provider)
{
	g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));
	g_return_if_fail (NAUTILUS_SEARCH_PROVIDER_GET_IFACE (provider)->start != NULL);

	NAUTILUS_SEARCH_PROVIDER_GET_IFACE (provider)->start (provider);
}

void
nautilus_search_provider_stop (NautilusSearchProvider *provider)
{
	g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));
	g_return_if_fail (NAUTILUS_SEARCH_PROVIDER_GET_IFACE (provider)->stop != NULL);

	NAUTILUS_SEARCH_PROVIDER_GET_IFACE (provider)->stop (provider);
}

void
nautilus_search_provider_hits_added (NautilusSearchProvider *provider, GList *hits)
{
	g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));

	g_signal_emit (provider, signals[HITS_ADDED], 0, hits);
}

void
nautilus_search_provider_hits_subtracted (NautilusSearchProvider *provider, GList *hits)
{
	g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));

	g_signal_emit (provider, signals[HITS_SUBTRACTED], 0, hits);
}

void
nautilus_search_provider_finished (NautilusSearchProvider *provider)
{
	g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));

	g_signal_emit (provider, signals[FINISHED], 0);
}

void
nautilus_search_provider_error (NautilusSearchProvider *provider, const char *error_message)
{
	g_return_if_fail (NAUTILUS_IS_SEARCH_PROVIDER (provider));

	g_signal_emit (provider, signals[ERROR], 0, error_message);
}

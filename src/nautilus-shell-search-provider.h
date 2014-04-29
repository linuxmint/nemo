/*
 * nautilus-shell-search-provider.h - Implementation of a GNOME Shell
 *   search provider
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
 *
 */

#ifndef __NAUTILUS_SHELL_SEARCH_PROVIDER_H__
#define __NAUTILUS_SHELL_SEARCH_PROVIDER_H__

#define NAUTILUS_TYPE_SHELL_SEARCH_PROVIDER nautilus_shell_search_provider_get_type()
#define NAUTILUS_SHELL_SEARCH_PROVIDER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_SHELL_SEARCH_PROVIDER, NautilusShellSearchProvider))

typedef struct _NautilusShellSearchProvider NautilusShellSearchProvider;
typedef GObjectClass NautilusShellSearchProviderClass;

GType nautilus_shell_search_provider_get_type (void);
NautilusShellSearchProvider * nautilus_shell_search_provider_new (void);

gboolean nautilus_shell_search_provider_register   (NautilusShellSearchProvider *self,
                                                    GDBusConnection             *connection,
                                                    GError                     **error);
void     nautilus_shell_search_provider_unregister (NautilusShellSearchProvider *self);

#endif /* __NAUTILUS_SHELL_SEARCH_PROVIDER_H__ */

/*
 * nemo-shell-search-provider.h - Implementation of a GNOME Shell
 *   search provider
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
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

#ifndef __NEMO_SHELL_SEARCH_PROVIDER_H__
#define __NEMO_SHELL_SEARCH_PROVIDER_H__

#define NEMO_TYPE_SHELL_SEARCH_PROVIDER nemo_shell_search_provider_get_type()
#define NEMO_SHELL_SEARCH_PROVIDER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_SHELL_SEARCH_PROVIDER, NemoShellSearchProvider))

typedef struct _NemoShellSearchProvider NemoShellSearchProvider;
typedef GObjectClass NemoShellSearchProviderClass;

GType nemo_shell_search_provider_get_type (void);
NemoShellSearchProvider * nemo_shell_search_provider_new (void);

gboolean nemo_shell_search_provider_register   (NemoShellSearchProvider *self,
                                                    GDBusConnection             *connection,
                                                    GError                     **error);
void     nemo_shell_search_provider_unregister (NemoShellSearchProvider *self);

#endif /* __NEMO_SHELL_SEARCH_PROVIDER_H__ */

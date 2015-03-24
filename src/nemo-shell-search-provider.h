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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#endif /* __NEMO_SHELL_SEARCH_PROVIDER_H__ */


/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-search-directory.h: Subclass of NautilusDirectory to implement
   a virtual directory consisting of the search directory and the search
   icons
 
   Copyright (C) 2005 Novell, Inc
  
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
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef NAUTILUS_SEARCH_DIRECTORY_H
#define NAUTILUS_SEARCH_DIRECTORY_H

#include <libnautilus-private/nautilus-directory.h>
#include <libnautilus-private/nautilus-query.h>

#define NAUTILUS_TYPE_SEARCH_DIRECTORY nautilus_search_directory_get_type()
#define NAUTILUS_SEARCH_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_SEARCH_DIRECTORY, NautilusSearchDirectory))
#define NAUTILUS_SEARCH_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SEARCH_DIRECTORY, NautilusSearchDirectoryClass))
#define NAUTILUS_IS_SEARCH_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_SEARCH_DIRECTORY))
#define NAUTILUS_IS_SEARCH_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SEARCH_DIRECTORY))
#define NAUTILUS_SEARCH_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_SEARCH_DIRECTORY, NautilusSearchDirectoryClass))

typedef struct NautilusSearchDirectoryDetails NautilusSearchDirectoryDetails;

typedef struct {
	NautilusDirectory parent_slot;
	NautilusSearchDirectoryDetails *details;
} NautilusSearchDirectory;

typedef struct {
	NautilusDirectoryClass parent_slot;
} NautilusSearchDirectoryClass;

GType   nautilus_search_directory_get_type             (void);

char   *nautilus_search_directory_generate_new_uri     (void);

NautilusSearchDirectory *nautilus_search_directory_new_from_saved_search (const char *uri);

gboolean       nautilus_search_directory_is_saved_search (NautilusSearchDirectory *search);
gboolean       nautilus_search_directory_is_modified     (NautilusSearchDirectory *search);
void           nautilus_search_directory_save_search     (NautilusSearchDirectory *search);
void           nautilus_search_directory_save_to_file    (NautilusSearchDirectory *search,
							  const char              *save_file_uri);

NautilusQuery *nautilus_search_directory_get_query       (NautilusSearchDirectory *search);
void           nautilus_search_directory_set_query       (NautilusSearchDirectory *search,
							  NautilusQuery           *query);

#endif /* NAUTILUS_SEARCH_DIRECTORY_H */

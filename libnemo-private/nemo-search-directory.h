/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-search-directory.h: Subclass of NemoDirectory to implement
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
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
*/

#ifndef NEMO_SEARCH_DIRECTORY_H
#define NEMO_SEARCH_DIRECTORY_H

#include <libnemo-private/nemo-directory.h>
#include <libnemo-private/nemo-query.h>

#define NEMO_TYPE_SEARCH_DIRECTORY nemo_search_directory_get_type()
#define NEMO_SEARCH_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_SEARCH_DIRECTORY, NemoSearchDirectory))
#define NEMO_SEARCH_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_SEARCH_DIRECTORY, NemoSearchDirectoryClass))
#define NEMO_IS_SEARCH_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_SEARCH_DIRECTORY))
#define NEMO_IS_SEARCH_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_SEARCH_DIRECTORY))
#define NEMO_SEARCH_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_SEARCH_DIRECTORY, NemoSearchDirectoryClass))

typedef struct NemoSearchDirectoryDetails NemoSearchDirectoryDetails;

typedef struct {
	NemoDirectory parent_slot;
	NemoSearchDirectoryDetails *details;
} NemoSearchDirectory;

typedef struct {
	NemoDirectoryClass parent_slot;
} NemoSearchDirectoryClass;

GType   nemo_search_directory_get_type             (void);

char   *nemo_search_directory_generate_new_uri     (void);

NemoSearchDirectory *nemo_search_directory_new_from_saved_search (const char *uri);

gboolean       nemo_search_directory_is_saved_search (NemoSearchDirectory *search);
gboolean       nemo_search_directory_is_modified     (NemoSearchDirectory *search);
void           nemo_search_directory_save_search     (NemoSearchDirectory *search);
void           nemo_search_directory_save_to_file    (NemoSearchDirectory *search,
							  const char              *save_file_uri);

NemoQuery *nemo_search_directory_get_query       (NemoSearchDirectory *search);
void           nemo_search_directory_set_query       (NemoSearchDirectory *search,
							  NemoQuery           *query);

#endif /* NEMO_SEARCH_DIRECTORY_H */

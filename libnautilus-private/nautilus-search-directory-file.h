/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-search-directory-file.h: Subclass of NautilusFile to implement the
   the case of the search directory
 
   Copyright (C) 2003 Red Hat, Inc.
  
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
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef NAUTILUS_SEARCH_DIRECTORY_FILE_H
#define NAUTILUS_SEARCH_DIRECTORY_FILE_H

#include <libnautilus-private/nautilus-file.h>

#define NAUTILUS_TYPE_SEARCH_DIRECTORY_FILE nautilus_search_directory_file_get_type()
#define NAUTILUS_SEARCH_DIRECTORY_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_SEARCH_DIRECTORY_FILE, NautilusSearchDirectoryFile))
#define NAUTILUS_SEARCH_DIRECTORY_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SEARCH_DIRECTORY_FILE, NautilusSearchDirectoryFileClass))
#define NAUTILUS_IS_SEARCH_DIRECTORY_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_SEARCH_DIRECTORY_FILE))
#define NAUTILUS_IS_SEARCH_DIRECTORY_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SEARCH_DIRECTORY_FILE))
#define NAUTILUS_SEARCH_DIRECTORY_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_SEARCH_DIRECTORY_FILE, NautilusSearchDirectoryFileClass))

typedef struct NautilusSearchDirectoryFileDetails NautilusSearchDirectoryFileDetails;

typedef struct {
	NautilusFile parent_slot;
	NautilusSearchDirectoryFileDetails *details;
} NautilusSearchDirectoryFile;

typedef struct {
	NautilusFileClass parent_slot;
} NautilusSearchDirectoryFileClass;

GType   nautilus_search_directory_file_get_type (void);
void    nautilus_search_directory_file_update_display_name (NautilusSearchDirectoryFile *search_file);

#endif /* NAUTILUS_SEARCH_DIRECTORY_FILE_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-saved-search-file.h: Subclass of NautilusVFSFile to implement the
   the case of a Saved Search file.
 
   Copyright (C) 2005 Red Hat, Inc
  
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
  
   Author: Alexander Larsson
*/

#ifndef NAUTILUS_SAVED_SEARCH_FILE_H
#define NAUTILUS_SAVED_SEARCH_FILE_H

#include <libnautilus-private/nautilus-vfs-file.h>

#define NAUTILUS_TYPE_SAVED_SEARCH_FILE nautilus_saved_search_file_get_type()
#define NAUTILUS_SAVED_SEARCH_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_SAVED_SEARCH_FILE, NautilusSavedSearchFile))
#define NAUTILUS_SAVED_SEARCH_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SAVED_SEARCH_FILE, NautilusSavedSearchFileClass))
#define NAUTILUS_IS_SAVED_SEARCH_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_SAVED_SEARCH_FILE))
#define NAUTILUS_IS_SAVED_SEARCH_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SAVED_SEARCH_FILE))
#define NAUTILUS_SAVED_SEARCH_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_SAVED_SEARCH_FILE, NautilusSavedSearchFileClass))


typedef struct NautilusSavedSearchFileDetails NautilusSavedSearchFileDetails;

typedef struct {
	NautilusFile parent_slot;
} NautilusSavedSearchFile;

typedef struct {
	NautilusFileClass parent_slot;
} NautilusSavedSearchFileClass;

GType   nautilus_saved_search_file_get_type (void);

#endif /* NAUTILUS_SAVED_SEARCH_FILE_H */

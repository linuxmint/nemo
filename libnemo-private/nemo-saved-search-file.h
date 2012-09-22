/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-saved-search-file.h: Subclass of NemoVFSFile to implement the
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
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
  
   Author: Alexander Larsson
*/

#ifndef NEMO_SAVED_SEARCH_FILE_H
#define NEMO_SAVED_SEARCH_FILE_H

#include <libnemo-private/nemo-vfs-file.h>

#define NEMO_TYPE_SAVED_SEARCH_FILE nemo_saved_search_file_get_type()
#define NEMO_SAVED_SEARCH_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_SAVED_SEARCH_FILE, NemoSavedSearchFile))
#define NEMO_SAVED_SEARCH_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_SAVED_SEARCH_FILE, NemoSavedSearchFileClass))
#define NEMO_IS_SAVED_SEARCH_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_SAVED_SEARCH_FILE))
#define NEMO_IS_SAVED_SEARCH_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_SAVED_SEARCH_FILE))
#define NEMO_SAVED_SEARCH_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_SAVED_SEARCH_FILE, NemoSavedSearchFileClass))


typedef struct NemoSavedSearchFileDetails NemoSavedSearchFileDetails;

typedef struct {
	NemoFile parent_slot;
} NemoSavedSearchFile;

typedef struct {
	NemoFileClass parent_slot;
} NemoSavedSearchFileClass;

GType   nemo_saved_search_file_get_type (void);

#endif /* NEMO_SAVED_SEARCH_FILE_H */

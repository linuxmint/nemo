/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-merged-directory.h: Subclass of NautilusDirectory to implement
   a virtual directory consisting of the merged contents of some real
   directories.
 
   Copyright (C) 1999, 2000 Eazel, Inc.
  
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
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#ifndef NAUTILUS_MERGED_DIRECTORY_H
#define NAUTILUS_MERGED_DIRECTORY_H

#include <libnautilus-private/nautilus-directory.h>

#define NAUTILUS_TYPE_MERGED_DIRECTORY nautilus_merged_directory_get_type()
#define NAUTILUS_MERGED_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_MERGED_DIRECTORY, NautilusMergedDirectory))
#define NAUTILUS_MERGED_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_MERGED_DIRECTORY, NautilusMergedDirectoryClass))
#define NAUTILUS_IS_MERGED_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_MERGED_DIRECTORY))
#define NAUTILUS_IS_MERGED_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_MERGED_DIRECTORY))
#define NAUTILUS_MERGED_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_MERGED_DIRECTORY, NautilusMergedDirectoryClass))

typedef struct NautilusMergedDirectoryDetails NautilusMergedDirectoryDetails;

typedef struct {
	NautilusDirectory parent_slot;
	NautilusMergedDirectoryDetails *details;
} NautilusMergedDirectory;

typedef struct {
	NautilusDirectoryClass parent_slot;

	void (* add_real_directory)    (NautilusMergedDirectory *merged_directory,
					NautilusDirectory       *real_directory);
	void (* remove_real_directory) (NautilusMergedDirectory *merged_directory,
					NautilusDirectory       *real_directory);
} NautilusMergedDirectoryClass;

GType   nautilus_merged_directory_get_type              (void);
void    nautilus_merged_directory_add_real_directory    (NautilusMergedDirectory *merged_directory,
							 NautilusDirectory       *real_directory);
void    nautilus_merged_directory_remove_real_directory (NautilusMergedDirectory *merged_directory,
							 NautilusDirectory       *real_directory);
GList * nautilus_merged_directory_get_real_directories  (NautilusMergedDirectory *merged_directory);

#endif /* NAUTILUS_MERGED_DIRECTORY_H */

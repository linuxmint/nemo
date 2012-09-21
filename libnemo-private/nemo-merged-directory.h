/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-merged-directory.h: Subclass of NemoDirectory to implement
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
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#ifndef NEMO_MERGED_DIRECTORY_H
#define NEMO_MERGED_DIRECTORY_H

#include <libnemo-private/nemo-directory.h>

#define NEMO_TYPE_MERGED_DIRECTORY nemo_merged_directory_get_type()
#define NEMO_MERGED_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_MERGED_DIRECTORY, NemoMergedDirectory))
#define NEMO_MERGED_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_MERGED_DIRECTORY, NemoMergedDirectoryClass))
#define NEMO_IS_MERGED_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_MERGED_DIRECTORY))
#define NEMO_IS_MERGED_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_MERGED_DIRECTORY))
#define NEMO_MERGED_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_MERGED_DIRECTORY, NemoMergedDirectoryClass))

typedef struct NemoMergedDirectoryDetails NemoMergedDirectoryDetails;

typedef struct {
	NemoDirectory parent_slot;
	NemoMergedDirectoryDetails *details;
} NemoMergedDirectory;

typedef struct {
	NemoDirectoryClass parent_slot;

	void (* add_real_directory)    (NemoMergedDirectory *merged_directory,
					NemoDirectory       *real_directory);
	void (* remove_real_directory) (NemoMergedDirectory *merged_directory,
					NemoDirectory       *real_directory);
} NemoMergedDirectoryClass;

GType   nemo_merged_directory_get_type              (void);
void    nemo_merged_directory_add_real_directory    (NemoMergedDirectory *merged_directory,
							 NemoDirectory       *real_directory);
void    nemo_merged_directory_remove_real_directory (NemoMergedDirectory *merged_directory,
							 NemoDirectory       *real_directory);
GList * nemo_merged_directory_get_real_directories  (NemoMergedDirectory *merged_directory);

#endif /* NEMO_MERGED_DIRECTORY_H */

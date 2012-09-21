/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-vfs-directory.h: Subclass of NemoDirectory to implement the
   the case of a VFS directory.
 
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

#ifndef NEMO_VFS_DIRECTORY_H
#define NEMO_VFS_DIRECTORY_H

#include <libnemo-private/nemo-directory.h>

#define NEMO_TYPE_VFS_DIRECTORY nemo_vfs_directory_get_type()
#define NEMO_VFS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_VFS_DIRECTORY, NemoVFSDirectory))
#define NEMO_VFS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_VFS_DIRECTORY, NemoVFSDirectoryClass))
#define NEMO_IS_VFS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_VFS_DIRECTORY))
#define NEMO_IS_VFS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_VFS_DIRECTORY))
#define NEMO_VFS_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_VFS_DIRECTORY, NemoVFSDirectoryClass))

typedef struct NemoVFSDirectoryDetails NemoVFSDirectoryDetails;

typedef struct {
	NemoDirectory parent_slot;
} NemoVFSDirectory;

typedef struct {
	NemoDirectoryClass parent_slot;
} NemoVFSDirectoryClass;

GType   nemo_vfs_directory_get_type (void);

#endif /* NEMO_VFS_DIRECTORY_H */

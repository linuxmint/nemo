/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-vfs-file.h: Subclass of NemoFile to implement the
   the case of a VFS file.
 
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

#ifndef NEMO_VFS_FILE_H
#define NEMO_VFS_FILE_H

#include <libnemo-private/nemo-file.h>

#define NEMO_TYPE_VFS_FILE nemo_vfs_file_get_type()
#define NEMO_VFS_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_VFS_FILE, NemoVFSFile))
#define NEMO_VFS_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_VFS_FILE, NemoVFSFileClass))
#define NEMO_IS_VFS_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_VFS_FILE))
#define NEMO_IS_VFS_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_VFS_FILE))
#define NEMO_VFS_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_VFS_FILE, NemoVFSFileClass))

typedef struct NemoVFSFileDetails NemoVFSFileDetails;

typedef struct {
	NemoFile parent_slot;
} NemoVFSFile;

typedef struct {
	NemoFileClass parent_slot;
} NemoVFSFileClass;

GType   nemo_vfs_file_get_type (void);

#endif /* NEMO_VFS_FILE_H */

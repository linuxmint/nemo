/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-desktop-directory-file.h: Subclass of NemoFile to implement the
   the case of the desktop directory
 
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

#ifndef NEMO_DESKTOP_DIRECTORY_FILE_H
#define NEMO_DESKTOP_DIRECTORY_FILE_H

#include <libnemo-private/nemo-file.h>

#define NEMO_TYPE_DESKTOP_DIRECTORY_FILE nemo_desktop_directory_file_get_type()
#define NEMO_DESKTOP_DIRECTORY_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_DESKTOP_DIRECTORY_FILE, NemoDesktopDirectoryFile))
#define NEMO_DESKTOP_DIRECTORY_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_DESKTOP_DIRECTORY_FILE, NemoDesktopDirectoryFileClass))
#define NEMO_IS_DESKTOP_DIRECTORY_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_DESKTOP_DIRECTORY_FILE))
#define NEMO_IS_DESKTOP_DIRECTORY_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_DESKTOP_DIRECTORY_FILE))
#define NEMO_DESKTOP_DIRECTORY_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_DESKTOP_DIRECTORY_FILE, NemoDesktopDirectoryFileClass))

typedef struct NemoDesktopDirectoryFileDetails NemoDesktopDirectoryFileDetails;

typedef struct {
	NemoFile parent_slot;
	NemoDesktopDirectoryFileDetails *details;
} NemoDesktopDirectoryFile;

typedef struct {
	NemoFileClass parent_slot;
} NemoDesktopDirectoryFileClass;

GType    nemo_desktop_directory_file_get_type    (void);

#endif /* NEMO_DESKTOP_DIRECTORY_FILE_H */

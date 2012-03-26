/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-desktop-directory-file.h: Subclass of NautilusFile to implement the
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

#ifndef NAUTILUS_DESKTOP_DIRECTORY_FILE_H
#define NAUTILUS_DESKTOP_DIRECTORY_FILE_H

#include <libnautilus-private/nautilus-file.h>

#define NAUTILUS_TYPE_DESKTOP_DIRECTORY_FILE nautilus_desktop_directory_file_get_type()
#define NAUTILUS_DESKTOP_DIRECTORY_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_DESKTOP_DIRECTORY_FILE, NautilusDesktopDirectoryFile))
#define NAUTILUS_DESKTOP_DIRECTORY_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DESKTOP_DIRECTORY_FILE, NautilusDesktopDirectoryFileClass))
#define NAUTILUS_IS_DESKTOP_DIRECTORY_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_DESKTOP_DIRECTORY_FILE))
#define NAUTILUS_IS_DESKTOP_DIRECTORY_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DESKTOP_DIRECTORY_FILE))
#define NAUTILUS_DESKTOP_DIRECTORY_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_DESKTOP_DIRECTORY_FILE, NautilusDesktopDirectoryFileClass))

typedef struct NautilusDesktopDirectoryFileDetails NautilusDesktopDirectoryFileDetails;

typedef struct {
	NautilusFile parent_slot;
	NautilusDesktopDirectoryFileDetails *details;
} NautilusDesktopDirectoryFile;

typedef struct {
	NautilusFileClass parent_slot;
} NautilusDesktopDirectoryFileClass;

GType    nautilus_desktop_directory_file_get_type    (void);

#endif /* NAUTILUS_DESKTOP_DIRECTORY_FILE_H */

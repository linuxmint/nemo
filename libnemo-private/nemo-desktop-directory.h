/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-desktop-directory.h: Subclass of NemoDirectory to implement
   a virtual directory consisting of the desktop directory and the desktop
   icons
 
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

#ifndef NEMO_DESKTOP_DIRECTORY_H
#define NEMO_DESKTOP_DIRECTORY_H

#include <libnemo-private/nemo-directory.h>

#define NEMO_TYPE_DESKTOP_DIRECTORY nemo_desktop_directory_get_type()
#define NEMO_DESKTOP_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_DESKTOP_DIRECTORY, NemoDesktopDirectory))
#define NEMO_DESKTOP_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_DESKTOP_DIRECTORY, NemoDesktopDirectoryClass))
#define NEMO_IS_DESKTOP_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_DESKTOP_DIRECTORY))
#define NEMO_IS_DESKTOP_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_DESKTOP_DIRECTORY))
#define NEMO_DESKTOP_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_DESKTOP_DIRECTORY, NemoDesktopDirectoryClass))

typedef struct NemoDesktopDirectoryDetails NemoDesktopDirectoryDetails;

typedef struct {
	NemoDirectory parent_slot;
	NemoDesktopDirectoryDetails *details;
} NemoDesktopDirectory;

typedef struct {
	NemoDirectoryClass parent_slot;

} NemoDesktopDirectoryClass;

GType   nemo_desktop_directory_get_type             (void);
NemoDirectory * nemo_desktop_directory_get_real_directory   (NemoDesktopDirectory *desktop_directory);

#endif /* NEMO_DESKTOP_DIRECTORY_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-desktop-file.h: Subclass of NemoFile to implement the
   the case of a desktop icon file
 
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
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef NEMO_DESKTOP_ICON_FILE_H
#define NEMO_DESKTOP_ICON_FILE_H

#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-desktop-link.h>

#define NEMO_TYPE_DESKTOP_ICON_FILE nemo_desktop_icon_file_get_type()
#define NEMO_DESKTOP_ICON_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_DESKTOP_ICON_FILE, NemoDesktopIconFile))
#define NEMO_DESKTOP_ICON_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_DESKTOP_ICON_FILE, NemoDesktopIconFileClass))
#define NEMO_IS_DESKTOP_ICON_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_DESKTOP_ICON_FILE))
#define NEMO_IS_DESKTOP_ICON_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_DESKTOP_ICON_FILE))
#define NEMO_DESKTOP_ICON_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_DESKTOP_ICON_FILE, NemoDesktopIconFileClass))

typedef struct NemoDesktopIconFileDetails NemoDesktopIconFileDetails;

typedef struct {
	NemoFile parent_slot;
	NemoDesktopIconFileDetails *details;
} NemoDesktopIconFile;

typedef struct {
	NemoFileClass parent_slot;
} NemoDesktopIconFileClass;

GType   nemo_desktop_icon_file_get_type (void);

NemoDesktopIconFile *nemo_desktop_icon_file_new      (NemoDesktopLink     *link);
void                     nemo_desktop_icon_file_update   (NemoDesktopIconFile *icon_file);
void                     nemo_desktop_icon_file_remove   (NemoDesktopIconFile *icon_file);
NemoDesktopLink     *nemo_desktop_icon_file_get_link (NemoDesktopIconFile *icon_file);

#endif /* NEMO_DESKTOP_ICON_FILE_H */

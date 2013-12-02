/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-thumbnails.h: Thumbnail code for icon factory.
 
   Copyright (C) 2000 Eazel, Inc.
  
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
  
   Author: Andy Hertzfeld <andy@eazel.com>
*/

#ifndef NEMO_THUMBNAILS_H
#define NEMO_THUMBNAILS_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libnemo-private/nemo-file.h>

/* Returns NULL if there's no thumbnail yet. */
void       nemo_create_thumbnail                (NemoFile *file);
gboolean   nemo_can_thumbnail                   (NemoFile *file);
gboolean   nemo_can_thumbnail_internally        (NemoFile *file);
gboolean   nemo_thumbnail_is_mimetype_limited_by_size
						    (const char *mime_type);
void       nemo_thumbnail_frame_image           (GdkPixbuf **pixbuf);

/* Queue handling: */
void       nemo_thumbnail_remove_from_queue     (const char   *file_uri);
void       nemo_thumbnail_prioritize            (const char   *file_uri);


#endif /* NEMO_THUMBNAILS_H */

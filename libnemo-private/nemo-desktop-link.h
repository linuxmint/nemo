/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-desktop-link.h: Class that handles the links on the desktop
    
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

#ifndef NEMO_DESKTOP_LINK_H
#define NEMO_DESKTOP_LINK_H

#include <libnemo-private/nemo-file.h>
#include <gio/gio.h>

#define NEMO_TYPE_DESKTOP_LINK nemo_desktop_link_get_type()
#define NEMO_DESKTOP_LINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_DESKTOP_LINK, NemoDesktopLink))
#define NEMO_DESKTOP_LINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_DESKTOP_LINK, NemoDesktopLinkClass))
#define NEMO_IS_DESKTOP_LINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_DESKTOP_LINK))
#define NEMO_IS_DESKTOP_LINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_DESKTOP_LINK))
#define NEMO_DESKTOP_LINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_DESKTOP_LINK, NemoDesktopLinkClass))

typedef struct NemoDesktopLinkDetails NemoDesktopLinkDetails;

typedef struct {
	GObject parent_slot;
	NemoDesktopLinkDetails *details;
} NemoDesktopLink;

typedef struct {
	GObjectClass parent_slot;
} NemoDesktopLinkClass;

typedef enum {
	NEMO_DESKTOP_LINK_HOME,
	NEMO_DESKTOP_LINK_COMPUTER,
	NEMO_DESKTOP_LINK_TRASH,
	NEMO_DESKTOP_LINK_MOUNT,
	NEMO_DESKTOP_LINK_NETWORK
} NemoDesktopLinkType;

GType   nemo_desktop_link_get_type (void);

NemoDesktopLink *   nemo_desktop_link_new                     (NemoDesktopLinkType  type);
NemoDesktopLink *   nemo_desktop_link_new_from_mount          (GMount                 *mount);
NemoDesktopLinkType nemo_desktop_link_get_link_type           (NemoDesktopLink     *link);
char *                  nemo_desktop_link_get_file_name           (NemoDesktopLink     *link);
char *                  nemo_desktop_link_get_display_name        (NemoDesktopLink     *link);
GIcon *                 nemo_desktop_link_get_icon                (NemoDesktopLink     *link);
GFile *                 nemo_desktop_link_get_activation_location (NemoDesktopLink     *link);
char *                  nemo_desktop_link_get_activation_uri      (NemoDesktopLink     *link);
gboolean                nemo_desktop_link_get_date                (NemoDesktopLink     *link,
								       NemoDateType         date_type,
								       time_t                  *date);
GMount *                nemo_desktop_link_get_mount               (NemoDesktopLink     *link);
gboolean                nemo_desktop_link_can_rename              (NemoDesktopLink     *link);
gboolean                nemo_desktop_link_rename                  (NemoDesktopLink     *link,
								       const char              *name);


#endif /* NEMO_DESKTOP_LINK_H */

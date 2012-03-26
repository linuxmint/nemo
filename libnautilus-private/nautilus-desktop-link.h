/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-desktop-link.h: Class that handles the links on the desktop
    
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

#ifndef NAUTILUS_DESKTOP_LINK_H
#define NAUTILUS_DESKTOP_LINK_H

#include <libnautilus-private/nautilus-file.h>
#include <gio/gio.h>

#define NAUTILUS_TYPE_DESKTOP_LINK nautilus_desktop_link_get_type()
#define NAUTILUS_DESKTOP_LINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_DESKTOP_LINK, NautilusDesktopLink))
#define NAUTILUS_DESKTOP_LINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DESKTOP_LINK, NautilusDesktopLinkClass))
#define NAUTILUS_IS_DESKTOP_LINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_DESKTOP_LINK))
#define NAUTILUS_IS_DESKTOP_LINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DESKTOP_LINK))
#define NAUTILUS_DESKTOP_LINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_DESKTOP_LINK, NautilusDesktopLinkClass))

typedef struct NautilusDesktopLinkDetails NautilusDesktopLinkDetails;

typedef struct {
	GObject parent_slot;
	NautilusDesktopLinkDetails *details;
} NautilusDesktopLink;

typedef struct {
	GObjectClass parent_slot;
} NautilusDesktopLinkClass;

typedef enum {
	NAUTILUS_DESKTOP_LINK_HOME,
	NAUTILUS_DESKTOP_LINK_COMPUTER,
	NAUTILUS_DESKTOP_LINK_TRASH,
	NAUTILUS_DESKTOP_LINK_MOUNT,
	NAUTILUS_DESKTOP_LINK_NETWORK
} NautilusDesktopLinkType;

GType   nautilus_desktop_link_get_type (void);

NautilusDesktopLink *   nautilus_desktop_link_new                     (NautilusDesktopLinkType  type);
NautilusDesktopLink *   nautilus_desktop_link_new_from_mount          (GMount                 *mount);
NautilusDesktopLinkType nautilus_desktop_link_get_link_type           (NautilusDesktopLink     *link);
char *                  nautilus_desktop_link_get_file_name           (NautilusDesktopLink     *link);
char *                  nautilus_desktop_link_get_display_name        (NautilusDesktopLink     *link);
GIcon *                 nautilus_desktop_link_get_icon                (NautilusDesktopLink     *link);
GFile *                 nautilus_desktop_link_get_activation_location (NautilusDesktopLink     *link);
char *                  nautilus_desktop_link_get_activation_uri      (NautilusDesktopLink     *link);
gboolean                nautilus_desktop_link_get_date                (NautilusDesktopLink     *link,
								       NautilusDateType         date_type,
								       time_t                  *date);
GMount *                nautilus_desktop_link_get_mount               (NautilusDesktopLink     *link);
gboolean                nautilus_desktop_link_can_rename              (NautilusDesktopLink     *link);
gboolean                nautilus_desktop_link_rename                  (NautilusDesktopLink     *link,
								       const char              *name);


#endif /* NAUTILUS_DESKTOP_LINK_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nautilus-desktop-background.c: Helper object to handle desktop background
 *                                changes.
 *
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2010 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Darin Adler <darin@bentspoon.com>
 *          Cosimo Cecchi <cosimoc@gnome.org>
 */

#ifndef __NAUTILIUS_DESKTOP_BACKGROUND_H__
#define __NAUTILIUS_DESKTOP_BACKGROUND_H__

#include <gtk/gtk.h>

#include "nautilus-icon-container.h"

typedef struct NautilusDesktopBackground NautilusDesktopBackground;
typedef struct NautilusDesktopBackgroundClass NautilusDesktopBackgroundClass;

#define NAUTILUS_TYPE_DESKTOP_BACKGROUND nautilus_desktop_background_get_type()
#define NAUTILUS_DESKTOP_BACKGROUND(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_DESKTOP_BACKGROUND, NautilusDesktopBackground))
#define NAUTILUS_DESKTOP_BACKGROUND_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DESKTOP_BACKGROUND, NautilusDesktopBackgroundClass))
#define NAUTILUS_IS_DESKTOP_BACKGROUND(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_DESKTOP_BACKGROUND))
#define NAUTILUS_IS_DESKTOP_BACKGROUND_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DESKTOP_BACKGROUND))
#define NAUTILUS_DESKTOP_BACKGROUND_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_DESKTOP_BACKGROUND, NautilusDesktopBackgroundClass))

GType nautilus_desktop_background_get_type (void);
NautilusDesktopBackground * nautilus_desktop_background_new (NautilusIconContainer *container);

void nautilus_desktop_background_receive_dropped_background_image (NautilusDesktopBackground *self,
								   const gchar *image_uri);

typedef struct NautilusDesktopBackgroundDetails NautilusDesktopBackgroundDetails;

struct NautilusDesktopBackground {
	GObject parent;
	NautilusDesktopBackgroundDetails *details;
};

struct NautilusDesktopBackgroundClass {
	GObjectClass parent_class;
};

#endif /* __NAUTILIUS_DESKTOP_BACKGROUND_H__ */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-desktop-background.c: Helper object to handle desktop background
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
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Darin Adler <darin@bentspoon.com>
 *          Cosimo Cecchi <cosimoc@gnome.org>
 */

#ifndef __NAUTILIUS_DESKTOP_BACKGROUND_H__
#define __NAUTILIUS_DESKTOP_BACKGROUND_H__

#include <gtk/gtk.h>

#include "nemo-icon-container.h"

typedef struct NemoDesktopBackground NemoDesktopBackground;
typedef struct NemoDesktopBackgroundClass NemoDesktopBackgroundClass;

#define NEMO_TYPE_DESKTOP_BACKGROUND nemo_desktop_background_get_type()
#define NEMO_DESKTOP_BACKGROUND(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_DESKTOP_BACKGROUND, NemoDesktopBackground))
#define NEMO_DESKTOP_BACKGROUND_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_DESKTOP_BACKGROUND, NemoDesktopBackgroundClass))
#define NEMO_IS_DESKTOP_BACKGROUND(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_DESKTOP_BACKGROUND))
#define NEMO_IS_DESKTOP_BACKGROUND_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_DESKTOP_BACKGROUND))
#define NEMO_DESKTOP_BACKGROUND_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_DESKTOP_BACKGROUND, NemoDesktopBackgroundClass))

GType nemo_desktop_background_get_type (void);
NemoDesktopBackground * nemo_desktop_background_new (NemoIconContainer *container);

typedef struct NemoDesktopBackgroundDetails NemoDesktopBackgroundDetails;

struct NemoDesktopBackground {
	GObject parent;
	NemoDesktopBackgroundDetails *details;
};

struct NemoDesktopBackgroundClass {
	GObjectClass parent_class;
};

#endif /* __NAUTILIUS_DESKTOP_BACKGROUND_H__ */

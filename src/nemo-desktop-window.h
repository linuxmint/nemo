/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 * Authors: Darin Adler <darin@bentspoon.com>
 */

/* nemo-desktop-window.h
 */

#ifndef NEMO_DESKTOP_WINDOW_H
#define NEMO_DESKTOP_WINDOW_H

#include "nemo-window.h"

#define NEMO_TYPE_DESKTOP_WINDOW nemo_desktop_window_get_type()
#define NEMO_DESKTOP_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_DESKTOP_WINDOW, NemoDesktopWindow))
#define NEMO_DESKTOP_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_DESKTOP_WINDOW, NemoDesktopWindowClass))
#define NEMO_IS_DESKTOP_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_DESKTOP_WINDOW))
#define NEMO_IS_DESKTOP_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_DESKTOP_WINDOW))
#define NEMO_DESKTOP_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_DESKTOP_WINDOW, NemoDesktopWindowClass))

typedef struct NemoDesktopWindowDetails NemoDesktopWindowDetails;

typedef struct {
	NemoWindow parent_spot;
	NemoDesktopWindowDetails *details;
} NemoDesktopWindow;

typedef struct {
	NemoWindowClass parent_spot;
} NemoDesktopWindowClass;

GType                  nemo_desktop_window_get_type            (void);
NemoDesktopWindow *nemo_desktop_window_new                 (GdkScreen *screen);
gboolean               nemo_desktop_window_loaded              (NemoDesktopWindow *window);

#endif /* NEMO_DESKTOP_WINDOW_H */

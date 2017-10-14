/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
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
 */

/* nemo-blank-desktop-window.h
 */

#ifndef NEMO_BLANK_DESKTOP_WINDOW_H
#define NEMO_BLANK_DESKTOP_WINDOW_H

#include <gtk/gtk.h>

#include <libnemo-private/nemo-action-manager.h>

#define NEMO_TYPE_BLANK_DESKTOP_WINDOW nemo_blank_desktop_window_get_type()
#define NEMO_BLANK_DESKTOP_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_BLANK_DESKTOP_WINDOW, NemoBlankDesktopWindow))
#define NEMO_BLANK_DESKTOP_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_BLANK_DESKTOP_WINDOW, NemoBlankDesktopWindowClass))
#define NEMO_IS_BLANK_DESKTOP_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_BLANK_DESKTOP_WINDOW))
#define NEMO_IS_BLANK_DESKTOP_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_BLANK_DESKTOP_WINDOW))
#define NEMO_BLANK_DESKTOP_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_BLANK_DESKTOP_WINDOW, NemoBlankDesktopWindowClass))

typedef struct NemoBlankDesktopWindowDetails NemoBlankDesktopWindowDetails;

typedef struct {
	GtkWindow parent_spot;
	NemoBlankDesktopWindowDetails *details;
} NemoBlankDesktopWindow;

typedef struct {
	GtkWindowClass parent_spot;

    void   (* plugin_manager)  (NemoBlankDesktopWindow *window);
} NemoBlankDesktopWindowClass;

GType                   nemo_blank_desktop_window_get_type            (void);
NemoBlankDesktopWindow *nemo_blank_desktop_window_new                 (gint monitor);
NemoActionManager      *nemo_desktop_manager_get_action_manager       (void);
void                    nemo_blank_desktop_window_update_geometry     (NemoBlankDesktopWindow *window);

#endif /* NEMO_BLANK_DESKTOP_WINDOW_H */

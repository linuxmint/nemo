/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-properties-window.h - interface for window that lets user modify 
                            icon properties

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Darin Adler <darin@bentspoon.com>
*/

#ifndef NEMO_PROPERTIES_WINDOW_H
#define NEMO_PROPERTIES_WINDOW_H

#include <gtk/gtk.h>
#include <libnemo-private/nemo-file.h>

typedef struct NemoPropertiesWindow NemoPropertiesWindow;

#define NEMO_TYPE_PROPERTIES_WINDOW nemo_properties_window_get_type()
#define NEMO_PROPERTIES_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_PROPERTIES_WINDOW, NemoPropertiesWindow))
#define NEMO_PROPERTIES_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_PROPERTIES_WINDOW, NemoPropertiesWindowClass))
#define NEMO_IS_PROPERTIES_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_PROPERTIES_WINDOW))
#define NEMO_IS_PROPERTIES_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_PROPERTIES_WINDOW))
#define NEMO_PROPERTIES_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_PROPERTIES_WINDOW, NemoPropertiesWindowClass))

typedef struct NemoPropertiesWindowDetails NemoPropertiesWindowDetails;

struct NemoPropertiesWindow {
	GtkDialog window;
	NemoPropertiesWindowDetails *details;	
};

struct NemoPropertiesWindowClass {
	GtkDialogClass parent_class;
	
	/* Keybinding signals */
	void (* close)    (NemoPropertiesWindow *window);
};

typedef struct NemoPropertiesWindowClass NemoPropertiesWindowClass;

GType   nemo_properties_window_get_type   (void);

void 	nemo_properties_window_present    (GList       *files,
					       GtkWidget   *parent_widget,
					       const gchar *startup_id);

#endif /* NEMO_PROPERTIES_WINDOW_H */

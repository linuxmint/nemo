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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Darin Adler <darin@bentspoon.com>
*/

#ifndef NAUTILUS_PROPERTIES_WINDOW_H
#define NAUTILUS_PROPERTIES_WINDOW_H

#include <gtk/gtk.h>
#include <libnautilus-private/nautilus-file.h>

typedef struct NautilusPropertiesWindow NautilusPropertiesWindow;

#define NAUTILUS_TYPE_PROPERTIES_WINDOW nautilus_properties_window_get_type()
#define NAUTILUS_PROPERTIES_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_PROPERTIES_WINDOW, NautilusPropertiesWindow))
#define NAUTILUS_PROPERTIES_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PROPERTIES_WINDOW, NautilusPropertiesWindowClass))
#define NAUTILUS_IS_PROPERTIES_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_PROPERTIES_WINDOW))
#define NAUTILUS_IS_PROPERTIES_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PROPERTIES_WINDOW))
#define NAUTILUS_PROPERTIES_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_PROPERTIES_WINDOW, NautilusPropertiesWindowClass))

typedef struct NautilusPropertiesWindowDetails NautilusPropertiesWindowDetails;

struct NautilusPropertiesWindow {
	GtkDialog window;
	NautilusPropertiesWindowDetails *details;	
};

struct NautilusPropertiesWindowClass {
	GtkDialogClass parent_class;
	
	/* Keybinding signals */
	void (* close)    (NautilusPropertiesWindow *window);
};

typedef struct NautilusPropertiesWindowClass NautilusPropertiesWindowClass;

GType   nautilus_properties_window_get_type   (void);

void 	nautilus_properties_window_present    (GList       *files,
					       GtkWidget   *parent_widget,
					       const gchar *startup_id);

#endif /* NAUTILUS_PROPERTIES_WINDOW_H */

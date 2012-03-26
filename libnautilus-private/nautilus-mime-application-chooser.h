/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
   nautilus-mime-application-chooser.c: Manages applications for mime types
 
   Copyright (C) 2004 Novell, Inc.
 
   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but APPLICATIONOUT ANY WARRANTY; applicationout even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along application the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Dave Camp <dave@novell.com>
*/

#ifndef NAUTILUS_MIME_APPLICATION_CHOOSER_H
#define NAUTILUS_MIME_APPLICATION_CHOOSER_H

#include <gtk/gtk.h>

#define NAUTILUS_TYPE_MIME_APPLICATION_CHOOSER         (nautilus_mime_application_chooser_get_type ())
#define NAUTILUS_MIME_APPLICATION_CHOOSER(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_MIME_APPLICATION_CHOOSER, NautilusMimeApplicationChooser))
#define NAUTILUS_MIME_APPLICATION_CHOOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_MIME_APPLICATION_CHOOSER, NautilusMimeApplicationChooserClass))
#define NAUTILUS_IS_MIME_APPLICATION_CHOOSER(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_MIME_APPLICATION_CHOOSER)

typedef struct _NautilusMimeApplicationChooser        NautilusMimeApplicationChooser;
typedef struct _NautilusMimeApplicationChooserClass   NautilusMimeApplicationChooserClass;
typedef struct _NautilusMimeApplicationChooserDetails NautilusMimeApplicationChooserDetails;

struct _NautilusMimeApplicationChooser {
	GtkBox parent;
	NautilusMimeApplicationChooserDetails *details;
};

struct _NautilusMimeApplicationChooserClass {
	GtkBoxClass parent_class;
};

GType      nautilus_mime_application_chooser_get_type (void);
GtkWidget * nautilus_mime_application_chooser_new (const char *uri,
						   GList *files,
						   const char *mime_type);

#endif /* NAUTILUS_MIME_APPLICATION_CHOOSER_H */

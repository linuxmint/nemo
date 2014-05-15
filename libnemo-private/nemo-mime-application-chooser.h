/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
   nemo-mime-application-chooser.c: Manages applications for mime types
 
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
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Dave Camp <dave@novell.com>
*/

#ifndef NEMO_MIME_APPLICATION_CHOOSER_H
#define NEMO_MIME_APPLICATION_CHOOSER_H

#include <gtk/gtk.h>

#define NEMO_TYPE_MIME_APPLICATION_CHOOSER         (nemo_mime_application_chooser_get_type ())
#define NEMO_MIME_APPLICATION_CHOOSER(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_MIME_APPLICATION_CHOOSER, NemoMimeApplicationChooser))
#define NEMO_MIME_APPLICATION_CHOOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_MIME_APPLICATION_CHOOSER, NemoMimeApplicationChooserClass))
#define NEMO_IS_MIME_APPLICATION_CHOOSER(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_MIME_APPLICATION_CHOOSER)

typedef struct _NemoMimeApplicationChooser        NemoMimeApplicationChooser;
typedef struct _NemoMimeApplicationChooserClass   NemoMimeApplicationChooserClass;
typedef struct _NemoMimeApplicationChooserDetails NemoMimeApplicationChooserDetails;

struct _NemoMimeApplicationChooser {
	GtkBox parent;
	NemoMimeApplicationChooserDetails *details;
};

struct _NemoMimeApplicationChooserClass {
	GtkBoxClass parent_class;
};

GType      nemo_mime_application_chooser_get_type (void);
GtkWidget * nemo_mime_application_chooser_new (GList *files,
						   const char *mime_type);
GAppInfo  *nemo_mime_application_chooser_get_info (NemoMimeApplicationChooser *chooser);
const GList *nemo_mime_application_chooser_get_files (NemoMimeApplicationChooser *chooser);

#endif /* NEMO_MIME_APPLICATION_CHOOSER_H */

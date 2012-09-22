/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-column-choose.h - A column chooser widget

   Copyright (C) 2004 Novell, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the column COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Dave Camp <dave@ximian.com>
*/

#ifndef NEMO_COLUMN_CHOOSER_H
#define NEMO_COLUMN_CHOOSER_H

#include <gtk/gtk.h>
#include <libnemo-private/nemo-file.h>

#define NEMO_TYPE_COLUMN_CHOOSER nemo_column_chooser_get_type()
#define NEMO_COLUMN_CHOOSER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_COLUMN_CHOOSER, NemoColumnChooser))
#define NEMO_COLUMN_CHOOSER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_COLUMN_CHOOSER, NemoColumnChooserClass))
#define NEMO_IS_COLUMN_CHOOSER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_COLUMN_CHOOSER))
#define NEMO_IS_COLUMN_CHOOSER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_COLUMN_CHOOSER))
#define NEMO_COLUMN_CHOOSER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_COLUMN_CHOOSER, NemoColumnChooserClass))

typedef struct _NemoColumnChooserDetails NemoColumnChooserDetails;

typedef struct {
	GtkBox parent;
	
	NemoColumnChooserDetails *details;
} NemoColumnChooser;

typedef struct {
        GtkBoxClass parent_slot;

	void (*changed) (NemoColumnChooser *chooser);
	void (*use_default) (NemoColumnChooser *chooser);
} NemoColumnChooserClass;

GType      nemo_column_chooser_get_type            (void);
GtkWidget *nemo_column_chooser_new                 (NemoFile *file);
void       nemo_column_chooser_set_settings    (NemoColumnChooser   *chooser,
						    char                   **visible_columns, 
						    char                   **column_order);
void       nemo_column_chooser_get_settings    (NemoColumnChooser *chooser,
						    char                  ***visible_columns, 
						    char                  ***column_order);

#endif /* NEMO_COLUMN_CHOOSER_H */

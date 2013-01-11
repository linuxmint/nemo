/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 
   Copyright (C) 2007 Martin Wehner
  
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
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Author: Martin Wehner <martin.wehner@gmail.com>
*/

#ifndef NEMO_PATHBAR_BUTTON_H
#define NEMO_PATHBAR_BUTTON_H

#include <gtk/gtk.h>

#define NEMO_TYPE_PATHBAR_BUTTON nemo_pathbar_button_get_type()
#define NEMO_PATHBAR_BUTTON(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_PATHBAR_BUTTON, NemoPathbarButton))
#define NEMO_PATHBAR_BUTTON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_PATHBAR_BUTTON, NemoPathbarButtonClass))
#define NEMO_IS_PATHBAR_BUTTON(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_PATHBAR_BUTTON))
#define NEMO_IS_PATHBAR_BUTTON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_PATHBAR_BUTTON))
#define NEMO_PATHBAR_BUTTON_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_PATHBAR_BUTTON, NemoPathbarButtonClass))


#define PATHBAR_BUTTON_OFFSET_FACTOR 2.125

typedef struct _NemoPathbarButton NemoPathbarButton;
typedef struct _NemoPathbarButtonClass NemoPathbarButtonClass;

struct _NemoPathbarButton {
	GtkToggleButton parent;
    gboolean is_left_end;
    gboolean highlight;
};

struct _NemoPathbarButtonClass {
	GtkToggleButtonClass parent_class;
};

GType        nemo_pathbar_button_get_type (void);
GtkWidget   *nemo_pathbar_button_new      (void);

void nemo_pathbar_button_set_is_left_end (GtkWidget *button, gboolean left_end);

void nemo_pathbar_button_set_highlight (GtkWidget *button, gboolean highlight);

void nemo_pathbar_button_get_preferred_size (GtkWidget *button, GtkRequisition *requisition, gint height);

#endif /* NEMO_PATHBAR_BUTTON_H */

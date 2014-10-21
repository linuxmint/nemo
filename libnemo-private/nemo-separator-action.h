/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 
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

*/

#ifndef NEMO_SEPARATOR_ACTION_H
#define NEMO_SEPARATOR_ACTION_H

#include <gtk/gtk.h>

#define NEMO_TYPE_SEPARATOR_ACTION nemo_separator_action_get_type()
#define NEMO_SEPARATOR_ACTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_SEPARATOR_ACTION, NemoSeparatorAction))
#define NEMO_SEPARATOR_ACTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_SEPARATOR_ACTION, NemoSeparatorActionClass))
#define NEMO_IS_SEPARATOR_ACTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_SEPARATOR_ACTION))
#define NEMO_IS_SEPARATOR_ACTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_SEPARATOR_ACTION))
#define NEMO_SEPARATOR_ACTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_SEPARATOR_ACTION, NemoSeparatorActionClass))

typedef struct _NemoSeparatorAction NemoSeparatorAction;
typedef struct _NemoSeparatorActionClass NemoSeparatorActionClass;

struct _NemoSeparatorAction {
    GtkAction parent;
};

struct _NemoSeparatorActionClass {
	GtkActionClass parent_class;
};

GType         nemo_separator_action_get_type             (void);
GtkAction    *nemo_separator_action_new                  (const gchar *name);

#endif /* NEMO_SEPARATOR_ACTION_H */

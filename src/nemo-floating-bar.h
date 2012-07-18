/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nemo - Floating status bar.
 *
 * Copyright (C) 2011 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NEMO_FLOATING_BAR_H__
#define __NEMO_FLOATING_BAR_H__

#include <gtk/gtk.h>

#define NEMO_FLOATING_BAR_ACTION_ID_STOP 1

#define NEMO_TYPE_FLOATING_BAR nemo_floating_bar_get_type()
#define NEMO_FLOATING_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_FLOATING_BAR, NemoFloatingBar))
#define NEMO_FLOATING_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_FLOATING_BAR, NemoFloatingBarClass))
#define NEMO_IS_FLOATING_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_FLOATING_BAR))
#define NEMO_IS_FLOATING_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_FLOATING_BAR))
#define NEMO_FLOATING_BAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_FLOATING_BAR, NemoFloatingBarClass))

typedef struct _NemoFloatingBar NemoFloatingBar;
typedef struct _NemoFloatingBarClass NemoFloatingBarClass;
typedef struct _NemoFloatingBarDetails NemoFloatingBarDetails;

struct _NemoFloatingBar {
	GtkBox parent;
	NemoFloatingBarDetails *priv;
};

struct _NemoFloatingBarClass {
	GtkBoxClass parent_class;
};

/* GObject */
GType       nemo_floating_bar_get_type  (void);

GtkWidget * nemo_floating_bar_new              (const gchar *label,
						    gboolean show_spinner);

void        nemo_floating_bar_set_label        (NemoFloatingBar *self,
						    const gchar *label);
void        nemo_floating_bar_set_show_spinner (NemoFloatingBar *self,
						    gboolean show_spinner);

void        nemo_floating_bar_add_action       (NemoFloatingBar *self,
						    const gchar *stock_id,
						    gint action_id);
void        nemo_floating_bar_cleanup_actions  (NemoFloatingBar *self);

#endif /* __NEMO_FLOATING_BAR_H__ */


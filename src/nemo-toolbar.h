/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 2011, Red Hat, Inc.
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NEMO_TOOLBAR_H__
#define __NEMO_TOOLBAR_H__

#include <gtk/gtk.h>

#define NEMO_TYPE_TOOLBAR nemo_toolbar_get_type()
#define NEMO_TOOLBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_TOOLBAR, NemoToolbar))
#define NEMO_TOOLBAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_TOOLBAR, NemoToolbarClass))
#define NEMO_IS_TOOLBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_TOOLBAR))
#define NEMO_IS_TOOLBAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_TOOLBAR))
#define NEMO_TOOLBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_TOOLBAR, NemoToolbarClass))

typedef struct _NemoToolbar NemoToolbar;
typedef struct _NemoToolbarPriv NemoToolbarPriv;
typedef struct _NemoToolbarClass NemoToolbarClass;

typedef enum {
	NEMO_TOOLBAR_MODE_PATH_BAR,
	NEMO_TOOLBAR_MODE_LOCATION_BAR,
} NemoToolbarMode;

struct _NemoToolbar {
	GtkBox parent;

	/* private */
	NemoToolbarPriv *priv;
};

struct _NemoToolbarClass {
	GtkBoxClass parent_class;
};

GType nemo_toolbar_get_type (void);

GtkWidget *nemo_toolbar_new (GtkActionGroup *action_group);

gboolean  nemo_toolbar_get_show_location_entry (NemoToolbar *self);
GtkWidget *nemo_toolbar_get_path_bar (NemoToolbar *self);
GtkWidget *nemo_toolbar_get_location_bar (NemoToolbar *self);
GtkWidget *nemo_toolbar_get_search_bar (NemoToolbar *self);

void nemo_toolbar_set_show_main_bar (NemoToolbar *self,
					 gboolean show_main_bar);
void nemo_toolbar_set_show_location_entry (NemoToolbar *self,
					       gboolean show_location_entry);
void nemo_toolbar_set_show_search_bar (NemoToolbar *self,
					   gboolean show_search_bar);

#endif /* __NEMO_TOOLBAR_H__ */

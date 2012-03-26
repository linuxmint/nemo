/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2011, Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
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

#ifndef __NAUTILUS_TOOLBAR_H__
#define __NAUTILUS_TOOLBAR_H__

#include <gtk/gtk.h>

#define NAUTILUS_TYPE_TOOLBAR nautilus_toolbar_get_type()
#define NAUTILUS_TOOLBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_TOOLBAR, NautilusToolbar))
#define NAUTILUS_TOOLBAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_TOOLBAR, NautilusToolbarClass))
#define NAUTILUS_IS_TOOLBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_TOOLBAR))
#define NAUTILUS_IS_TOOLBAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_TOOLBAR))
#define NAUTILUS_TOOLBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_TOOLBAR, NautilusToolbarClass))

typedef struct _NautilusToolbar NautilusToolbar;
typedef struct _NautilusToolbarPriv NautilusToolbarPriv;
typedef struct _NautilusToolbarClass NautilusToolbarClass;

typedef enum {
	NAUTILUS_TOOLBAR_MODE_PATH_BAR,
	NAUTILUS_TOOLBAR_MODE_LOCATION_BAR,
} NautilusToolbarMode;

struct _NautilusToolbar {
	GtkBox parent;

	/* private */
	NautilusToolbarPriv *priv;
};

struct _NautilusToolbarClass {
	GtkBoxClass parent_class;
};

GType nautilus_toolbar_get_type (void);

GtkWidget *nautilus_toolbar_new (GtkActionGroup *action_group);

GtkWidget *nautilus_toolbar_get_path_bar (NautilusToolbar *self);
GtkWidget *nautilus_toolbar_get_location_bar (NautilusToolbar *self);
GtkWidget *nautilus_toolbar_get_search_bar (NautilusToolbar *self);

void nautilus_toolbar_set_show_main_bar (NautilusToolbar *self,
					 gboolean show_main_bar);
void nautilus_toolbar_set_show_location_entry (NautilusToolbar *self,
					       gboolean show_location_entry);
void nautilus_toolbar_set_show_search_bar (NautilusToolbar *self,
					   gboolean show_search_bar);

#endif /* __NAUTILUS_TOOLBAR_H__ */

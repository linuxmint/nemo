/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nautilus-application: main Nautilus application class.
 *
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2010 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __NAUTILUS_APPLICATION_H__
#define __NAUTILUS_APPLICATION_H__

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <libnautilus-private/nautilus-undo-manager.h>

#include "nautilus-window.h"

#define NAUTILUS_DESKTOP_ICON_VIEW_IID	"OAFIID:Nautilus_File_Manager_Desktop_Icon_View"

#define NAUTILUS_TYPE_APPLICATION nautilus_application_get_type()
#define NAUTILUS_APPLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_APPLICATION, NautilusApplication))
#define NAUTILUS_APPLICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_APPLICATION, NautilusApplicationClass))
#define NAUTILUS_IS_APPLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_APPLICATION))
#define NAUTILUS_IS_APPLICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_APPLICATION))
#define NAUTILUS_APPLICATION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_APPLICATION, NautilusApplicationClass))

#ifndef NAUTILUS_SPATIAL_WINDOW_DEFINED
#define NAUTILUS_SPATIAL_WINDOW_DEFINED
typedef struct _NautilusSpatialWindow NautilusSpatialWindow;
#endif

typedef struct _NautilusApplicationPriv NautilusApplicationPriv;

typedef struct {
	GtkApplication parent;

	NautilusUndoManager *undo_manager;

	NautilusApplicationPriv *priv;
} NautilusApplication;

typedef struct {
	GtkApplicationClass parent_class;
} NautilusApplicationClass;

GType nautilus_application_get_type (void);

NautilusApplication *nautilus_application_get_singleton (void);

void nautilus_application_quit (NautilusApplication *self);

NautilusWindow *     nautilus_application_create_window (NautilusApplication *application,
							 GdkScreen           *screen);

void nautilus_application_open_location (NautilusApplication *application,
					 GFile *location,
					 GFile *selection,
					 const char *startup_id);

void nautilus_application_close_all_windows (NautilusApplication *self);

#endif /* __NAUTILUS_APPLICATION_H__ */

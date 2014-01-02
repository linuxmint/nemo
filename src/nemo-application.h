/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nemo-application: main Nemo application class.
 *
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2010 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef __NEMO_APPLICATION_H__
#define __NEMO_APPLICATION_H__

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "nemo-bookmark-list.h"
#include "nemo-window.h"

#define NEMO_DESKTOP_ICON_VIEW_IID	"OAFIID:Nemo_File_Manager_Desktop_Icon_View"

#define NEMO_TYPE_APPLICATION nemo_application_get_type()
#define NEMO_APPLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_APPLICATION, NemoApplication))
#define NEMO_APPLICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_APPLICATION, NemoApplicationClass))
#define NEMO_IS_APPLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_APPLICATION))
#define NEMO_IS_APPLICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_APPLICATION))
#define NEMO_APPLICATION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_APPLICATION, NemoApplicationClass))

#ifndef NEMO_SPATIAL_WINDOW_DEFINED
#define NEMO_SPATIAL_WINDOW_DEFINED
typedef struct _NemoSpatialWindow NemoSpatialWindow;
#endif

typedef struct _NemoApplicationPriv NemoApplicationPriv;

typedef struct {
	GtkApplication parent;

	NemoApplicationPriv *priv;
} NemoApplication;

typedef struct {
	GtkApplicationClass parent_class;
} NemoApplicationClass;

GType nemo_application_get_type (void);

void nemo_application_quit (NemoApplication *self);

NemoWindow *     nemo_application_create_window (NemoApplication *application,
							 GdkScreen           *screen);

void nemo_application_open_location (NemoApplication *application,
					 GFile *location,
					 GFile *selection,
					 const char *startup_id);

void nemo_application_close_all_windows (NemoApplication *self);

#if GLIB_CHECK_VERSION (2,34,0)
void nemo_application_notify_unmount_show (NemoApplication *application,
					       const gchar *message);

void nemo_application_notify_unmount_done (NemoApplication *application,
					       const gchar *message);
#endif // GLIB_CHECK_VERSION (2,34,0)

NemoBookmarkList *
     nemo_application_get_bookmarks  (NemoApplication *application);
void nemo_application_edit_bookmarks (NemoApplication *application,
					  NemoWindow      *window);

#endif /* __NEMO_APPLICATION_H__ */

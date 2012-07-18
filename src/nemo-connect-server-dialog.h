/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Nemo
 *
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2010 Cosimo Cecchi <cosimoc@gnome.org>
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
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef NEMO_CONNECT_SERVER_DIALOG_H
#define NEMO_CONNECT_SERVER_DIALOG_H

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "nemo-application.h"
#include "nemo-window.h"

#define NEMO_TYPE_CONNECT_SERVER_DIALOG\
	(nemo_connect_server_dialog_get_type ())
#define NEMO_CONNECT_SERVER_DIALOG(obj)\
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_CONNECT_SERVER_DIALOG,\
				     NemoConnectServerDialog))
#define NEMO_CONNECT_SERVER_DIALOG_CLASS(klass)\
	(G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_CONNECT_SERVER_DIALOG,\
				  NemoConnectServerDialogClass))
#define NEMO_IS_CONNECT_SERVER_DIALOG(obj)\
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_CONNECT_SERVER_DIALOG)

typedef struct _NemoConnectServerDialog NemoConnectServerDialog;
typedef struct _NemoConnectServerDialogClass NemoConnectServerDialogClass;
typedef struct _NemoConnectServerDialogDetails NemoConnectServerDialogDetails;

struct _NemoConnectServerDialog {
	GtkDialog parent;
	NemoConnectServerDialogDetails *details;
};

struct _NemoConnectServerDialogClass {
	GtkDialogClass parent_class;
};

GType nemo_connect_server_dialog_get_type (void);

GtkWidget* nemo_connect_server_dialog_new (NemoWindow *window);

void nemo_connect_server_dialog_display_location_async (NemoConnectServerDialog *self,
							    GFile *location,
							    GAsyncReadyCallback callback,
							    gpointer user_data);
gboolean nemo_connect_server_dialog_display_location_finish (NemoConnectServerDialog *self,
								 GAsyncResult *result,
								 GError **error);

void nemo_connect_server_dialog_fill_details_async (NemoConnectServerDialog *self,
							GMountOperation *operation,
							const gchar *default_user,
							const gchar *default_domain,
							GAskPasswordFlags flags,
							GAsyncReadyCallback callback,
							gpointer user_data);
gboolean nemo_connect_server_dialog_fill_details_finish (NemoConnectServerDialog *self,
							     GAsyncResult *result);

#endif /* NEMO_CONNECT_SERVER_DIALOG_H */

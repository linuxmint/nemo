/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Nautilus
 *
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2010 Cosimo Cecchi <cosimoc@gnome.org>
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
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

#ifndef NAUTILUS_CONNECT_SERVER_DIALOG_H
#define NAUTILUS_CONNECT_SERVER_DIALOG_H

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "nautilus-application.h"
#include "nautilus-window.h"

#define NAUTILUS_TYPE_CONNECT_SERVER_DIALOG\
	(nautilus_connect_server_dialog_get_type ())
#define NAUTILUS_CONNECT_SERVER_DIALOG(obj)\
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_CONNECT_SERVER_DIALOG,\
				     NautilusConnectServerDialog))
#define NAUTILUS_CONNECT_SERVER_DIALOG_CLASS(klass)\
	(G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CONNECT_SERVER_DIALOG,\
				  NautilusConnectServerDialogClass))
#define NAUTILUS_IS_CONNECT_SERVER_DIALOG(obj)\
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_CONNECT_SERVER_DIALOG)

typedef struct _NautilusConnectServerDialog NautilusConnectServerDialog;
typedef struct _NautilusConnectServerDialogClass NautilusConnectServerDialogClass;
typedef struct _NautilusConnectServerDialogDetails NautilusConnectServerDialogDetails;

struct _NautilusConnectServerDialog {
	GtkDialog parent;
	NautilusConnectServerDialogDetails *details;
};

struct _NautilusConnectServerDialogClass {
	GtkDialogClass parent_class;
};

GType nautilus_connect_server_dialog_get_type (void);

GtkWidget * nautilus_connect_server_dialog_new             (NautilusWindow *window);
GFile *     nautilus_connect_server_dialog_get_location    (NautilusConnectServerDialog *dialog);
void        nautilus_connect_server_dialog_set_show_browse (NautilusConnectServerDialog *dialog,
							    gboolean                     show);

#endif /* NAUTILUS_CONNECT_SERVER_DIALOG_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Nautilus
 *
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
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 */

#ifndef __NAUTILUS_CONNECT_SERVER_OPERATION_H__
#define __NAUTILUS_CONNECT_SERVER_OPERATION_H__

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "nautilus-connect-server-dialog.h"

#define NAUTILUS_TYPE_CONNECT_SERVER_OPERATION\
	(nautilus_connect_server_operation_get_type ())
#define NAUTILUS_CONNECT_SERVER_OPERATION(obj)\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
			       NAUTILUS_TYPE_CONNECT_SERVER_OPERATION,\
			       NautilusConnectServerOperation))
#define NAUTILUS_CONNECT_SERVER_OPERATION_CLASS(klass)\
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CONNECT_SERVER_OPERATION,\
			    NautilusConnectServerOperationClass))
#define NAUTILUS_IS_CONNECT_SERVER_OPERATION(obj)\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_CONNECT_SERVER_OPERATION)

typedef struct _NautilusConnectServerOperationDetails
  NautilusConnectServerOperationDetails;

typedef struct {
	GtkMountOperation parent;
	NautilusConnectServerOperationDetails *details;
} NautilusConnectServerOperation;

typedef struct {
	GtkMountOperationClass parent_class;
} NautilusConnectServerOperationClass;

GType nautilus_connect_server_operation_get_type (void);

GMountOperation *
nautilus_connect_server_operation_new (NautilusConnectServerDialog *dialog);


#endif /* __NAUTILUS_CONNECT_SERVER_OPERATION_H__ */

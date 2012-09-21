/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Nemo
 *
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
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 */

#ifndef __NEMO_CONNECT_SERVER_OPERATION_H__
#define __NEMO_CONNECT_SERVER_OPERATION_H__

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "nemo-connect-server-dialog.h"

#define NEMO_TYPE_CONNECT_SERVER_OPERATION\
	(nemo_connect_server_operation_get_type ())
#define NEMO_CONNECT_SERVER_OPERATION(obj)\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
			       NEMO_TYPE_CONNECT_SERVER_OPERATION,\
			       NemoConnectServerOperation))
#define NEMO_CONNECT_SERVER_OPERATION_CLASS(klass)\
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_CONNECT_SERVER_OPERATION,\
			    NemoConnectServerOperationClass))
#define NEMO_IS_CONNECT_SERVER_OPERATION(obj)\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_CONNECT_SERVER_OPERATION)

typedef struct _NemoConnectServerOperationDetails
  NemoConnectServerOperationDetails;

typedef struct {
	GtkMountOperation parent;
	NemoConnectServerOperationDetails *details;
} NemoConnectServerOperation;

typedef struct {
	GtkMountOperationClass parent_class;
} NemoConnectServerOperationClass;

GType nemo_connect_server_operation_get_type (void);

GMountOperation *
nemo_connect_server_operation_new (NemoConnectServerDialog *dialog);


#endif /* __NEMO_CONNECT_SERVER_OPERATION_H__ */

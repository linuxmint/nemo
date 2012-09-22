/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nemo-progress-ui-handler.h: file operation progress user interface.
 *
 * Copyright (C) 2007, 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NEMO_PROGRESS_UI_HANDLER_H__
#define __NEMO_PROGRESS_UI_HANDLER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define NEMO_TYPE_PROGRESS_UI_HANDLER nemo_progress_ui_handler_get_type()
#define NEMO_PROGRESS_UI_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_PROGRESS_UI_HANDLER, NemoProgressUIHandler))
#define NEMO_PROGRESS_UI_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_PROGRESS_UI_HANDLER, NemoProgressUIHandlerClass))
#define NEMO_IS_PROGRESS_UI_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_PROGRESS_UI_HANDLER))
#define NEMO_IS_PROGRESS_UI_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_PROGRESS_UI_HANDLER))
#define NEMO_PROGRESS_UI_HANDLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_PROGRESS_UI_HANDLER, NemoProgressUIHandlerClass))

typedef struct _NemoProgressUIHandlerPriv NemoProgressUIHandlerPriv;

typedef struct {
  GObject parent;

  /* private */
  NemoProgressUIHandlerPriv *priv;
} NemoProgressUIHandler;

typedef struct {
  GObjectClass parent_class;
} NemoProgressUIHandlerClass;

GType nemo_progress_ui_handler_get_type (void);

NemoProgressUIHandler * nemo_progress_ui_handler_new (void);

G_END_DECLS

#endif /* __NEMO_PROGRESS_UI_HANDLER_H__ */

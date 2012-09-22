/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nemo-progress-info-widget.h: file operation progress user interface.
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

#ifndef __NEMO_PROGRESS_INFO_WIDGET_H__
#define __NEMO_PROGRESS_INFO_WIDGET_H__

#include <gtk/gtk.h>

#include <libnemo-private/nemo-progress-info.h>

#define NEMO_TYPE_PROGRESS_INFO_WIDGET nemo_progress_info_widget_get_type()
#define NEMO_PROGRESS_INFO_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_PROGRESS_INFO_WIDGET, NemoProgressInfoWidget))
#define NEMO_PROGRESS_INFO_WIDGET_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_PROGRESS_INFO_WIDGET, NemoProgressInfoWidgetClass))
#define NEMO_IS_PROGRESS_INFO_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_PROGRESS_INFO_WIDGET))
#define NEMO_IS_PROGRESS_INFO_WIDGET_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_PROGRESS_INFO_WIDGET))
#define NEMO_PROGRESS_INFO_WIDGET_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_PROGRESS_INFO_WIDGET, NemoProgressInfoWidgetClass))

typedef struct _NemoProgressInfoWidgetPriv NemoProgressInfoWidgetPriv;

typedef struct {
	GtkBox parent;

	/* private */
	NemoProgressInfoWidgetPriv *priv;
} NemoProgressInfoWidget;

typedef struct {
	GtkBoxClass parent_class;
} NemoProgressInfoWidgetClass;

GType nemo_progress_info_widget_get_type (void);

GtkWidget * nemo_progress_info_widget_new (NemoProgressInfo *info);

#endif /* __NEMO_PROGRESS_INFO_WIDGET_H__ */

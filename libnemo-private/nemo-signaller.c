/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Nemo is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 *
 * Author: John Sullivan <sullivan@eazel.com>
 */

/* nemo-signaller.h: Class to manage nemo-wide signals that don't
 * correspond to any particular object.
 */

#include <config.h>
#include "nemo-signaller.h"

#include <eel/eel-debug.h>

typedef GObject NemoSignaller;
typedef GObjectClass NemoSignallerClass;

enum {
	HISTORY_LIST_CHANGED,
	POPUP_MENU_CHANGED,
	USER_DIRS_CHANGED,
	MIME_DATA_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static GType nemo_signaller_get_type (void);

G_DEFINE_TYPE (NemoSignaller, nemo_signaller, G_TYPE_OBJECT);

GObject *
nemo_signaller_get_current (void)
{
	static GObject *global_signaller = NULL;

	if (global_signaller == NULL) {
		global_signaller = g_object_new (nemo_signaller_get_type (), NULL);
	}

	return global_signaller;
}

static void
nemo_signaller_init (NemoSignaller *signaller)
{
}

static void
nemo_signaller_class_init (NemoSignallerClass *class)
{
	signals[HISTORY_LIST_CHANGED] =
		g_signal_new ("history_list_changed",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[POPUP_MENU_CHANGED] =
		g_signal_new ("popup_menu_changed",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[USER_DIRS_CHANGED] =
		g_signal_new ("user_dirs_changed",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[MIME_DATA_CHANGED] =
		g_signal_new ("mime_data_changed",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
}

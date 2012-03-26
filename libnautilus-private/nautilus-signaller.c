/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: John Sullivan <sullivan@eazel.com>
 */

/* nautilus-signaller.h: Class to manage nautilus-wide signals that don't
 * correspond to any particular object.
 */

#include <config.h>
#include "nautilus-signaller.h"

#include <eel/eel-debug.h>

typedef GObject NautilusSignaller;
typedef GObjectClass NautilusSignallerClass;

enum {
	HISTORY_LIST_CHANGED,
	POPUP_MENU_CHANGED,
	USER_DIRS_CHANGED,
	MIME_DATA_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static GType nautilus_signaller_get_type (void);

G_DEFINE_TYPE (NautilusSignaller, nautilus_signaller, G_TYPE_OBJECT);

GObject *
nautilus_signaller_get_current (void)
{
	static GObject *global_signaller = NULL;

	if (global_signaller == NULL) {
		global_signaller = g_object_new (nautilus_signaller_get_type (), NULL);
	}

	return global_signaller;
}

static void
nautilus_signaller_init (NautilusSignaller *signaller)
{
}

static void
nautilus_signaller_class_init (NautilusSignallerClass *class)
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

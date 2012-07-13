/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
   nautilus-trash-monitor.c: Nautilus trash state watcher.
 
   Copyright (C) 2000, 2001 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Pavel Cisler <pavel@eazel.com>
*/

#include <config.h>
#include "nautilus-trash-monitor.h"

#include "nautilus-directory-notify.h"
#include "nautilus-directory.h"
#include "nautilus-file-attributes.h"
#include "nautilus-icon-names.h"
#include <eel/eel-debug.h>
#include <gio/gio.h>
#include <string.h>

struct NautilusTrashMonitorDetails {
	gboolean empty;
	GIcon *icon;
	GFileMonitor *file_monitor;
};

enum {
	TRASH_STATE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static NautilusTrashMonitor *nautilus_trash_monitor = NULL;

G_DEFINE_TYPE(NautilusTrashMonitor, nautilus_trash_monitor, G_TYPE_OBJECT)

static void
nautilus_trash_monitor_finalize (GObject *object)
{
	NautilusTrashMonitor *trash_monitor;

	trash_monitor = NAUTILUS_TRASH_MONITOR (object);

	if (trash_monitor->details->icon) {
		g_object_unref (trash_monitor->details->icon);
	}
	if (trash_monitor->details->file_monitor) {
		g_object_unref (trash_monitor->details->file_monitor);
	}

	G_OBJECT_CLASS (nautilus_trash_monitor_parent_class)->finalize (object);
}

static void
nautilus_trash_monitor_class_init (NautilusTrashMonitorClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = nautilus_trash_monitor_finalize;

	signals[TRASH_STATE_CHANGED] = g_signal_new
		("trash_state_changed",
		 G_TYPE_FROM_CLASS (object_class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusTrashMonitorClass, trash_state_changed),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__BOOLEAN,
		 G_TYPE_NONE, 1,
		 G_TYPE_BOOLEAN);

	g_type_class_add_private (object_class, sizeof(NautilusTrashMonitorDetails));
}

static void
update_icon (NautilusTrashMonitor *trash_monitor)
{
	g_clear_object (&trash_monitor->details->icon);

	if (trash_monitor->details->empty) {
		trash_monitor->details->icon = g_themed_icon_new (NAUTILUS_ICON_TRASH);
	} else {
		trash_monitor->details->icon = g_themed_icon_new (NAUTILUS_ICON_TRASH_FULL);
	}
}

static void
update_empty_info (NautilusTrashMonitor *trash_monitor,
		   gboolean is_empty)
{
	if (trash_monitor->details->empty == is_empty) {
		return;
	}
	
	trash_monitor->details->empty = is_empty;
	update_icon (trash_monitor);

	/* trash got empty or full, notify everyone who cares */
	g_signal_emit (trash_monitor,
		       signals[TRASH_STATE_CHANGED], 0,
		       trash_monitor->details->empty);
}

static void
enumerate_next_files_cb (GObject *source,
			 GAsyncResult *res,
			 gpointer user_data)
{
	NautilusTrashMonitor *trash_monitor = user_data;
	GList *infos;

	infos = g_file_enumerator_next_files_finish (G_FILE_ENUMERATOR (source), res, NULL);
	if (!infos) {
		update_empty_info (trash_monitor, TRUE);
	} else {
		update_empty_info (trash_monitor, FALSE);
		g_list_free_full (infos, g_object_unref);
	}

	g_object_unref (trash_monitor);
}

static void
enumerate_children_cb (GObject *source,
		       GAsyncResult *res,
		       gpointer user_data)
{
	GFileEnumerator *enumerator;
	NautilusTrashMonitor *trash_monitor = user_data;

	enumerator = g_file_enumerate_children_finish (G_FILE (source), res, NULL);
	if (!enumerator) {
		update_empty_info (trash_monitor, TRUE);
		g_object_unref (trash_monitor);
		return;
	}

	g_file_enumerator_next_files_async (enumerator, 1,
					    G_PRIORITY_DEFAULT, NULL,
					    enumerate_next_files_cb, trash_monitor);
	g_object_unref (enumerator);
}

static void
schedule_update_info (NautilusTrashMonitor *trash_monitor)
{
	GFile *location;

	location = g_file_new_for_uri ("trash:///");
	g_file_enumerate_children_async (location,
					 G_FILE_ATTRIBUTE_STANDARD_TYPE,
					 G_FILE_QUERY_INFO_NONE,
					 G_PRIORITY_DEFAULT, NULL,
					 enumerate_children_cb, g_object_ref (trash_monitor));
	
	g_object_unref (location);
}

static void
file_changed (GFileMonitor* monitor,
	      GFile *child,
	      GFile *other_file,
	      GFileMonitorEvent event_type,
	      gpointer user_data)
{
	NautilusTrashMonitor *trash_monitor;

	trash_monitor = NAUTILUS_TRASH_MONITOR (user_data);

	schedule_update_info (trash_monitor);
}

static void
nautilus_trash_monitor_init (NautilusTrashMonitor *trash_monitor)
{
	GFile *location;

	trash_monitor->details = G_TYPE_INSTANCE_GET_PRIVATE (trash_monitor,
							      NAUTILUS_TYPE_TRASH_MONITOR,
							      NautilusTrashMonitorDetails);

	trash_monitor->details->empty = TRUE;
	update_icon (trash_monitor);

	location = g_file_new_for_uri ("trash:///");

	trash_monitor->details->file_monitor = g_file_monitor_file (location, 0, NULL, NULL);

	g_signal_connect (trash_monitor->details->file_monitor, "changed",
			  (GCallback)file_changed, trash_monitor);

	g_object_unref (location);

	schedule_update_info (trash_monitor);
}

static void
unref_trash_monitor (void)
{
	g_object_unref (nautilus_trash_monitor);
}

NautilusTrashMonitor *
nautilus_trash_monitor_get (void)
{
	if (nautilus_trash_monitor == NULL) {
		/* not running yet, start it up */

		nautilus_trash_monitor = NAUTILUS_TRASH_MONITOR
			(g_object_new (NAUTILUS_TYPE_TRASH_MONITOR, NULL));
		eel_debug_call_at_shutdown (unref_trash_monitor);
	}

	return nautilus_trash_monitor;
}

gboolean
nautilus_trash_monitor_is_empty (void)
{
	NautilusTrashMonitor *monitor;

	monitor = nautilus_trash_monitor_get ();
	return monitor->details->empty;
}

GIcon *
nautilus_trash_monitor_get_icon (void)
{
	NautilusTrashMonitor *monitor;

	monitor = nautilus_trash_monitor_get ();
	if (monitor->details->icon) {
		return g_object_ref (monitor->details->icon);
	}
	return NULL;
}

void
nautilus_trash_monitor_add_new_trash_directories (void)
{
	/* We trashed something... */
}

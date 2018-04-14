/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
   nemo-trash-monitor.c: Nemo trash state watcher.
 
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
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
  
   Author: Pavel Cisler <pavel@eazel.com>
*/

#include <config.h>
#include "nemo-trash-monitor.h"

#include "nemo-directory-notify.h"
#include "nemo-directory.h"
#include "nemo-file-attributes.h"
#include "nemo-icon-names.h"
#include <eel/eel-debug.h>
#include <gio/gio.h>
#include <string.h>

struct NemoTrashMonitorDetails {
	gboolean empty;
	GIcon *icon;
    GIcon *symbolic_icon;
	GFileMonitor *file_monitor;
};

enum {
	TRASH_STATE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };
static NemoTrashMonitor *nemo_trash_monitor = NULL;

G_DEFINE_TYPE(NemoTrashMonitor, nemo_trash_monitor, G_TYPE_OBJECT)

static void
nemo_trash_monitor_finalize (GObject *object)
{
	NemoTrashMonitor *trash_monitor;

	trash_monitor = NEMO_TRASH_MONITOR (object);

	if (trash_monitor->details->icon) {
		g_object_unref (trash_monitor->details->icon);
	}
    if (trash_monitor->details->symbolic_icon) {
        g_object_unref (trash_monitor->details->icon);
    }
	if (trash_monitor->details->file_monitor) {
		g_object_unref (trash_monitor->details->file_monitor);
	}

	G_OBJECT_CLASS (nemo_trash_monitor_parent_class)->finalize (object);
}

static void
nemo_trash_monitor_class_init (NemoTrashMonitorClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = nemo_trash_monitor_finalize;

	signals[TRASH_STATE_CHANGED] = g_signal_new
		("trash_state_changed",
		 G_TYPE_FROM_CLASS (object_class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NemoTrashMonitorClass, trash_state_changed),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__BOOLEAN,
		 G_TYPE_NONE, 1,
		 G_TYPE_BOOLEAN);

	g_type_class_add_private (object_class, sizeof(NemoTrashMonitorDetails));
}

static void
update_icon (NemoTrashMonitor *trash_monitor)
{
	g_clear_object (&trash_monitor->details->icon);

	if (trash_monitor->details->empty) {
		trash_monitor->details->icon = g_themed_icon_new (NEMO_ICON_TRASH);
        trash_monitor->details->symbolic_icon = g_themed_icon_new (NEMO_ICON_SYMBOLIC_TRASH);
	} else {
		trash_monitor->details->icon = g_themed_icon_new (NEMO_ICON_TRASH_FULL);
        trash_monitor->details->symbolic_icon = g_themed_icon_new (NEMO_ICON_SYMBOLIC_TRASH_FULL);
	}
}

static void
update_empty_info (NemoTrashMonitor *trash_monitor,
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

/* Use G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT since we only want to know whether the
 * trash is empty or not, not access its children. This is available for the
 * trash backend since it uses a cache. In this way we prevent flooding the
 * trash backend with enumeration requests when trashing > 1000 files
 */
static void
trash_query_info_cb (GObject *source,
                     GAsyncResult *res,
                     gpointer user_data)
{
        NemoTrashMonitor *trash_monitor = user_data;
        GFileInfo *info;
        guint32 item_count;
        gboolean is_empty = TRUE;

        info = g_file_query_info_finish (G_FILE (source), res, NULL);

        if (info != NULL) {
                item_count = g_file_info_get_attribute_uint32 (info,
                                                               G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT);
                is_empty = item_count == 0;

                g_object_unref (info);

        }

        update_empty_info (trash_monitor, is_empty);

        g_object_unref (trash_monitor);
}

static void
schedule_update_info (NemoTrashMonitor *trash_monitor)
{
	GFile *location;

	location = g_file_new_for_uri ("trash:///");
        g_file_query_info_async (location,
                                 G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT,
                                 G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_DEFAULT, NULL,
                                 trash_query_info_cb, g_object_ref (trash_monitor));
	
	g_object_unref (location);
}

static void
file_changed (GFileMonitor* monitor,
	      GFile *child,
	      GFile *other_file,
	      GFileMonitorEvent event_type,
	      gpointer user_data)
{
	NemoTrashMonitor *trash_monitor;

	trash_monitor = NEMO_TRASH_MONITOR (user_data);

	schedule_update_info (trash_monitor);
}

static void
nemo_trash_monitor_init (NemoTrashMonitor *trash_monitor)
{
	GFile *location;

	trash_monitor->details = G_TYPE_INSTANCE_GET_PRIVATE (trash_monitor,
							      NEMO_TYPE_TRASH_MONITOR,
							      NemoTrashMonitorDetails);

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
	g_object_unref (nemo_trash_monitor);
}

NemoTrashMonitor *
nemo_trash_monitor_get (void)
{
	if (nemo_trash_monitor == NULL) {
		/* not running yet, start it up */

		nemo_trash_monitor = NEMO_TRASH_MONITOR
			(g_object_new (NEMO_TYPE_TRASH_MONITOR, NULL));
		eel_debug_call_at_shutdown (unref_trash_monitor);
	}

	return nemo_trash_monitor;
}

gboolean
nemo_trash_monitor_is_empty (void)
{
	NemoTrashMonitor *monitor;

	monitor = nemo_trash_monitor_get ();
	return monitor->details->empty;
}

GIcon *
nemo_trash_monitor_get_icon (void)
{
	NemoTrashMonitor *monitor;

	monitor = nemo_trash_monitor_get ();
	if (monitor->details->icon) {
		return g_object_ref (monitor->details->icon);
	}
	return NULL;
}

GIcon *
nemo_trash_monitor_get_symbolic_icon (void)
{
    NemoTrashMonitor *monitor;

    monitor = nemo_trash_monitor_get ();
    if (monitor->details->symbolic_icon) {
        return g_object_ref (monitor->details->symbolic_icon);
    }
    return NULL;
}

void
nemo_trash_monitor_add_new_trash_directories (void)
{
	/* We trashed something... */
}

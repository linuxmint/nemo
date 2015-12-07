/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-monitor.c: file and directory change monitoring for nemo
 
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
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
  
   Authors: Seth Nickell <seth@eazel.com>
            Darin Adler <darin@bentspoon.com>
	    Alex Graveley <alex@ximian.com>
*/

#include <config.h>
#include "nemo-monitor.h"
#include "nemo-file-changes-queue.h"
#include "nemo-file-utilities.h"

#include <gio/gio.h>

struct NemoMonitor {
	GFileMonitor *monitor;
	GVolumeMonitor *volume_monitor;
	GFile *location;
};

gboolean
nemo_monitor_active (void)
{
	static gboolean tried_monitor = FALSE;
	static gboolean monitor_success;
	GFileMonitor *dir_monitor;
	GFile *file;

	if (tried_monitor == FALSE) {	
		file = g_file_new_for_path (g_get_home_dir ());
		dir_monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, NULL);
		g_object_unref (file);
		
		monitor_success = (dir_monitor != NULL);
		if (dir_monitor) {
			g_object_unref (dir_monitor);
		}

		tried_monitor = TRUE;
	}

	return monitor_success;
}

static gboolean call_consume_changes_idle_id = 0;

static gboolean
call_consume_changes_idle_cb (gpointer not_used)
{
	nemo_file_changes_consume_changes (TRUE);
	call_consume_changes_idle_id = 0;
	return FALSE;
}

static void
schedule_call_consume_changes (void)
{
	if (call_consume_changes_idle_id == 0) {
		call_consume_changes_idle_id =
			g_idle_add (call_consume_changes_idle_cb, NULL);
	}
}

static void
mount_removed (GVolumeMonitor *volume_monitor,
	       GMount *mount,
	       gpointer user_data)
{
	NemoMonitor *monitor = user_data;
	GFile *mount_location;

	mount_location = g_mount_get_root (mount);

	if (g_file_has_prefix (monitor->location, mount_location)) {
		nemo_file_changes_queue_file_removed (monitor->location);
		schedule_call_consume_changes ();
	}

	g_object_unref (mount_location);
}

static void
dir_changed (GFileMonitor* monitor,
	     GFile *child,
	     GFile *other_file,
	     GFileMonitorEvent event_type,
	     gpointer user_data)
{
	char *uri, *to_uri;
	
	uri = g_file_get_uri (child);
	to_uri = NULL;
	if (other_file) {
		to_uri = g_file_get_uri (other_file);
	}

	switch (event_type) {
	default:
	case G_FILE_MONITOR_EVENT_CHANGED:
		/* ignore */
		break;
	case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
	case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
		nemo_file_changes_queue_file_changed (child);
		break;
	case G_FILE_MONITOR_EVENT_UNMOUNTED:
	case G_FILE_MONITOR_EVENT_DELETED:
		nemo_file_changes_queue_file_removed (child);
		break;
	case G_FILE_MONITOR_EVENT_CREATED:
		nemo_file_changes_queue_file_added (child);
		break;
	}

	g_free (uri);
	g_free (to_uri);

	schedule_call_consume_changes ();
}
 
NemoMonitor *
nemo_monitor_directory (GFile *location)
{
	GFileMonitor *dir_monitor;
	NemoMonitor *ret;

	ret = g_slice_new0 (NemoMonitor);
	dir_monitor = g_file_monitor_directory (location, G_FILE_MONITOR_WATCH_MOUNTS, NULL, NULL);

	if (dir_monitor != NULL) {
		ret->monitor = dir_monitor;
	} else if (!g_file_is_native (location)) {
		ret->location = g_object_ref (location);
		ret->volume_monitor = g_volume_monitor_get ();
	}

	if (ret->monitor != NULL) {
		g_signal_connect (ret->monitor, "changed",
				  G_CALLBACK (dir_changed), ret);
	}

	if (ret->volume_monitor != NULL) {
		g_signal_connect (ret->volume_monitor, "mount-removed",
				  G_CALLBACK (mount_removed), ret);
	}

	/* We return a monitor even on failure, so we can avoid later trying again */
	return ret;
}

void 
nemo_monitor_cancel (NemoMonitor *monitor)
{
	if (monitor->monitor != NULL) {
		g_signal_handlers_disconnect_by_func (monitor->monitor, dir_changed, monitor);
		g_file_monitor_cancel (monitor->monitor);
		g_object_unref (monitor->monitor);
	}

	if (monitor->volume_monitor != NULL) {
		g_signal_handlers_disconnect_by_func (monitor->volume_monitor, mount_removed, monitor);
		g_object_unref (monitor->volume_monitor);
	}

	g_clear_object (&monitor->location);
	g_slice_free (NemoMonitor, monitor);
}

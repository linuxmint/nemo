/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-desktop-link-monitor.h: singleton that manages the desktop links
    
   Copyright (C) 2003 Red Hat, Inc.
  
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
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef NEMO_DESKTOP_LINK_MONITOR_H
#define NEMO_DESKTOP_LINK_MONITOR_H

#include <gtk/gtk.h>
#include <libnemo-private/nemo-desktop-link.h>

#define NEMO_TYPE_DESKTOP_LINK_MONITOR nemo_desktop_link_monitor_get_type()
#define NEMO_DESKTOP_LINK_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_DESKTOP_LINK_MONITOR, NemoDesktopLinkMonitor))
#define NEMO_DESKTOP_LINK_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_DESKTOP_LINK_MONITOR, NemoDesktopLinkMonitorClass))
#define NEMO_IS_DESKTOP_LINK_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_DESKTOP_LINK_MONITOR))
#define NEMO_IS_DESKTOP_LINK_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_DESKTOP_LINK_MONITOR))
#define NEMO_DESKTOP_LINK_MONITOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_DESKTOP_LINK_MONITOR, NemoDesktopLinkMonitorClass))

typedef struct NemoDesktopLinkMonitorDetails NemoDesktopLinkMonitorDetails;

typedef struct {
	GObject parent_slot;
	NemoDesktopLinkMonitorDetails *details;
} NemoDesktopLinkMonitor;

typedef struct {
	GObjectClass parent_slot;
} NemoDesktopLinkMonitorClass;

GType   nemo_desktop_link_monitor_get_type (void);

NemoDesktopLinkMonitor *   nemo_desktop_link_monitor_get (void);
void nemo_desktop_link_monitor_delete_link (NemoDesktopLinkMonitor *monitor,
						NemoDesktopLink *link,
						GtkWidget *parent_view);

/* Used by nemo-desktop-link.c */
char * nemo_desktop_link_monitor_make_filename_unique (NemoDesktopLinkMonitor *monitor,
							   const char *filename);

#endif /* NEMO_DESKTOP_LINK_MONITOR_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-desktop-link-monitor.h: singleton that manages the desktop links
    
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
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef NAUTILUS_DESKTOP_LINK_MONITOR_H
#define NAUTILUS_DESKTOP_LINK_MONITOR_H

#include <gtk/gtk.h>
#include <libnautilus-private/nautilus-desktop-link.h>

#define NAUTILUS_TYPE_DESKTOP_LINK_MONITOR nautilus_desktop_link_monitor_get_type()
#define NAUTILUS_DESKTOP_LINK_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_DESKTOP_LINK_MONITOR, NautilusDesktopLinkMonitor))
#define NAUTILUS_DESKTOP_LINK_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DESKTOP_LINK_MONITOR, NautilusDesktopLinkMonitorClass))
#define NAUTILUS_IS_DESKTOP_LINK_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_DESKTOP_LINK_MONITOR))
#define NAUTILUS_IS_DESKTOP_LINK_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DESKTOP_LINK_MONITOR))
#define NAUTILUS_DESKTOP_LINK_MONITOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_DESKTOP_LINK_MONITOR, NautilusDesktopLinkMonitorClass))

typedef struct NautilusDesktopLinkMonitorDetails NautilusDesktopLinkMonitorDetails;

typedef struct {
	GObject parent_slot;
	NautilusDesktopLinkMonitorDetails *details;
} NautilusDesktopLinkMonitor;

typedef struct {
	GObjectClass parent_slot;
} NautilusDesktopLinkMonitorClass;

GType   nautilus_desktop_link_monitor_get_type (void);

NautilusDesktopLinkMonitor *   nautilus_desktop_link_monitor_get (void);
void nautilus_desktop_link_monitor_delete_link (NautilusDesktopLinkMonitor *monitor,
						NautilusDesktopLink *link,
						GtkWidget *parent_view);

/* Used by nautilus-desktop-link.c */
char * nautilus_desktop_link_monitor_make_filename_unique (NautilusDesktopLinkMonitor *monitor,
							   const char *filename);

#endif /* NAUTILUS_DESKTOP_LINK_MONITOR_H */

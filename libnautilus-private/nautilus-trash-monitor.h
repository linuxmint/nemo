/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
   nautilus-trash-monitor.h: Nautilus trash state watcher.
 
   Copyright (C) 2000 Eazel, Inc.
  
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

#ifndef NAUTILUS_TRASH_MONITOR_H
#define NAUTILUS_TRASH_MONITOR_H

#include <gtk/gtk.h>
#include <gio/gio.h>

typedef struct NautilusTrashMonitor NautilusTrashMonitor;
typedef struct NautilusTrashMonitorClass NautilusTrashMonitorClass;
typedef struct NautilusTrashMonitorDetails NautilusTrashMonitorDetails;

#define NAUTILUS_TYPE_TRASH_MONITOR nautilus_trash_monitor_get_type()
#define NAUTILUS_TRASH_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_TRASH_MONITOR, NautilusTrashMonitor))
#define NAUTILUS_TRASH_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_TRASH_MONITOR, NautilusTrashMonitorClass))
#define NAUTILUS_IS_TRASH_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_TRASH_MONITOR))
#define NAUTILUS_IS_TRASH_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_TRASH_MONITOR))
#define NAUTILUS_TRASH_MONITOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_TRASH_MONITOR, NautilusTrashMonitorClass))

struct NautilusTrashMonitor {
	GObject object;
	NautilusTrashMonitorDetails *details;
};

struct NautilusTrashMonitorClass {
	GObjectClass parent_class;

	void (* trash_state_changed)		(NautilusTrashMonitor 	*trash_monitor,
				      		 gboolean 		 new_state);
};

GType			nautilus_trash_monitor_get_type				(void);

NautilusTrashMonitor   *nautilus_trash_monitor_get 				(void);
gboolean		nautilus_trash_monitor_is_empty 			(void);
GIcon                  *nautilus_trash_monitor_get_icon                         (void);

void		        nautilus_trash_monitor_add_new_trash_directories        (void);

#endif

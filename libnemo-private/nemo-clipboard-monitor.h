/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-clipboard-monitor.h: lets you notice clipboard changes.
    
   Copyright (C) 2004 Red Hat, Inc.
  
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

#ifndef NEMO_CLIPBOARD_MONITOR_H
#define NEMO_CLIPBOARD_MONITOR_H

#include <gtk/gtk.h>

#define NEMO_TYPE_CLIPBOARD_MONITOR nemo_clipboard_monitor_get_type()
#define NEMO_CLIPBOARD_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_CLIPBOARD_MONITOR, NemoClipboardMonitor))
#define NEMO_CLIPBOARD_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_CLIPBOARD_MONITOR, NemoClipboardMonitorClass))
#define NEMO_IS_CLIPBOARD_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_CLIPBOARD_MONITOR))
#define NEMO_IS_CLIPBOARD_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_CLIPBOARD_MONITOR))
#define NEMO_CLIPBOARD_MONITOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_CLIPBOARD_MONITOR, NemoClipboardMonitorClass))

typedef struct NemoClipboardMonitorDetails NemoClipboardMonitorDetails;
typedef struct NemoClipboardInfo NemoClipboardInfo;

typedef struct {
	GObject parent_slot;

	NemoClipboardMonitorDetails *details;
} NemoClipboardMonitor;

typedef struct {
	GObjectClass parent_slot;
  
	void (* clipboard_changed) (NemoClipboardMonitor *monitor);
	void (* clipboard_info) (NemoClipboardMonitor *monitor,
	                         NemoClipboardInfo *info);
} NemoClipboardMonitorClass;

struct NemoClipboardInfo {
	GList *files;
	gboolean cut;
};

GType   nemo_clipboard_monitor_get_type (void);

NemoClipboardMonitor *   nemo_clipboard_monitor_get (void);
void nemo_clipboard_monitor_set_clipboard_info (NemoClipboardMonitor *monitor,
                                                    NemoClipboardInfo *info);
NemoClipboardInfo * nemo_clipboard_monitor_get_clipboard_info (NemoClipboardMonitor *monitor);
void nemo_clipboard_monitor_emit_changed (void);

void nemo_clear_clipboard_callback (GtkClipboard *clipboard,
                                        gpointer      user_data);
void nemo_get_clipboard_callback   (GtkClipboard     *clipboard,
                                        GtkSelectionData *selection_data,
                                        guint             info,
                                        gpointer          user_data);



#endif /* NEMO_CLIPBOARD_MONITOR_H */


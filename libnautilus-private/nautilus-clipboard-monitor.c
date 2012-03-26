/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-clipboard-monitor.c: catch clipboard changes.
    
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
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "nautilus-clipboard-monitor.h"
#include "nautilus-file.h"

#include <eel/eel-debug.h>
#include <gtk/gtk.h>

/* X11 has a weakness when it comes to clipboard handling,
 * there is no way to get told when the owner of the clipboard
 * changes. This is often needed, for instance to set the
 * sensitivity of the paste menu item. We work around this
 * internally in an app by telling the clipboard monitor when
 * we changed the clipboard. Unfortunately this doesn't give
 * us perfect results, we still don't catch changes made by
 * other clients
 *
 * This is fixed with the XFIXES extensions, which recent versions
 * of Gtk+ supports as the owner_change signal on GtkClipboard. We
 * use this now, but keep the old code since not all X servers support
 * XFIXES.
 */

enum {
	CLIPBOARD_CHANGED,
	CLIPBOARD_INFO,
	LAST_SIGNAL
};

struct NautilusClipboardMonitorDetails {
	NautilusClipboardInfo *info;
};

static guint signals[LAST_SIGNAL];
static GdkAtom copied_files_atom;

G_DEFINE_TYPE (NautilusClipboardMonitor, nautilus_clipboard_monitor, G_TYPE_OBJECT);

static NautilusClipboardMonitor *clipboard_monitor = NULL;

static void
destroy_clipboard_monitor (void)
{
	if (clipboard_monitor != NULL) {
		g_object_unref (clipboard_monitor);
	}
}

NautilusClipboardMonitor *
nautilus_clipboard_monitor_get (void)
{
	GtkClipboard *clipboard;
	
	if (clipboard_monitor == NULL) {
		clipboard_monitor = NAUTILUS_CLIPBOARD_MONITOR (g_object_new (NAUTILUS_TYPE_CLIPBOARD_MONITOR, NULL));
		eel_debug_call_at_shutdown (destroy_clipboard_monitor);
		
		clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
		g_signal_connect (clipboard, "owner_change",
				  G_CALLBACK (nautilus_clipboard_monitor_emit_changed), NULL);
	}
	return clipboard_monitor;
}

void
nautilus_clipboard_monitor_emit_changed (void)
{
	NautilusClipboardMonitor *monitor;
  
	monitor = nautilus_clipboard_monitor_get ();
	
	g_signal_emit (monitor, signals[CLIPBOARD_CHANGED], 0);
}

static NautilusClipboardInfo *
nautilus_clipboard_info_new (GList *files,
                             gboolean cut)
{
	NautilusClipboardInfo *info;

	info = g_slice_new0 (NautilusClipboardInfo);
	info->files = nautilus_file_list_copy (files);
	info->cut = cut;

	return info;
}

static NautilusClipboardInfo *
nautilus_clipboard_info_copy (NautilusClipboardInfo *info)
{
	NautilusClipboardInfo *new_info;

	new_info = NULL;

	if (info != NULL) {
		new_info = nautilus_clipboard_info_new (info->files,
			                                info->cut);
	}

	return new_info;
}

static void
nautilus_clipboard_info_free (NautilusClipboardInfo *info)
{
	nautilus_file_list_free (info->files);

	g_slice_free (NautilusClipboardInfo, info);
}

static void
nautilus_clipboard_monitor_init (NautilusClipboardMonitor *monitor)
{
	monitor->details = 
		G_TYPE_INSTANCE_GET_PRIVATE (monitor, NAUTILUS_TYPE_CLIPBOARD_MONITOR,
		                             NautilusClipboardMonitorDetails);
}	

static void
clipboard_monitor_finalize (GObject *object)
{
	NautilusClipboardMonitor *monitor;

	monitor = NAUTILUS_CLIPBOARD_MONITOR (object);

	if (monitor->details->info != NULL) {
		nautilus_clipboard_info_free (monitor->details->info);
		monitor->details->info = NULL;
	}

	G_OBJECT_CLASS (nautilus_clipboard_monitor_parent_class)->finalize (object);
}

static void
nautilus_clipboard_monitor_class_init (NautilusClipboardMonitorClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = clipboard_monitor_finalize;

	copied_files_atom = gdk_atom_intern ("x-special/gnome-copied-files", FALSE);

	signals[CLIPBOARD_CHANGED] =
		g_signal_new ("clipboard_changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusClipboardMonitorClass, clipboard_changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	signals[CLIPBOARD_INFO] =
		g_signal_new ("clipboard_info",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusClipboardMonitorClass, clipboard_info),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__POINTER,
		              G_TYPE_NONE,
		              1, G_TYPE_POINTER);

	g_type_class_add_private (klass, sizeof (NautilusClipboardMonitorDetails));
}

void
nautilus_clipboard_monitor_set_clipboard_info (NautilusClipboardMonitor *monitor,
                                               NautilusClipboardInfo *info)
{
	if (monitor->details->info != NULL) {
		nautilus_clipboard_info_free (monitor->details->info);
		monitor->details->info = NULL;
	}

	monitor->details->info = nautilus_clipboard_info_copy (info);

	g_signal_emit (monitor, signals[CLIPBOARD_INFO], 0, monitor->details->info);
	
	nautilus_clipboard_monitor_emit_changed ();
}

NautilusClipboardInfo *
nautilus_clipboard_monitor_get_clipboard_info (NautilusClipboardMonitor *monitor)
{
	return monitor->details->info;
}

void
nautilus_clear_clipboard_callback (GtkClipboard *clipboard,
                                   gpointer      user_data)
{
	nautilus_clipboard_monitor_set_clipboard_info 
		(nautilus_clipboard_monitor_get (), NULL);
}

static char *
convert_file_list_to_string (NautilusClipboardInfo *info,
			     gboolean format_for_text,
                             gsize *len)
{
	GString *uris;
	char *uri, *tmp;
	GFile *f;
        guint i;
	GList *l;

	if (format_for_text) {
		uris = g_string_new (NULL);
	} else {
		uris = g_string_new (info->cut ? "cut" : "copy");
	}

        for (i = 0, l = info->files; l != NULL; l = l->next, i++) {
		uri = nautilus_file_get_uri (l->data);

		if (format_for_text) {
			f = g_file_new_for_uri (uri);
			tmp = g_file_get_parse_name (f);
			g_object_unref (f);
			
			if (tmp != NULL) {
				g_string_append (uris, tmp);
				g_free (tmp);
			} else {
				g_string_append (uris, uri);
			}

			/* skip newline for last element */
			if (i + 1 < g_list_length (info->files)) {
				g_string_append_c (uris, '\n');
			}
		} else {
			g_string_append_c (uris, '\n');
			g_string_append (uris, uri);
		}

		g_free (uri);
	}

        *len = uris->len;
	return g_string_free (uris, FALSE);
}

void
nautilus_get_clipboard_callback (GtkClipboard     *clipboard,
                                 GtkSelectionData *selection_data,
                                 guint             info,
                                 gpointer          user_data)
{
	char **uris;
	GList *l;
	int i;
	NautilusClipboardInfo *clipboard_info;
	GdkAtom target;

	clipboard_info =
		nautilus_clipboard_monitor_get_clipboard_info (nautilus_clipboard_monitor_get ());

	target = gtk_selection_data_get_target (selection_data);

        if (gtk_targets_include_uri (&target, 1)) {
		uris = g_malloc ((g_list_length (clipboard_info->files) + 1) * sizeof (char *));
		i = 0;

		for (l = clipboard_info->files; l != NULL; l = l->next) {
			uris[i] = nautilus_file_get_uri (l->data);
			i++;
		}

		uris[i] = NULL;

		gtk_selection_data_set_uris (selection_data, uris);

		g_strfreev (uris);
        } else if (gtk_targets_include_text (&target, 1)) {
                char *str;
                gsize len;

                str = convert_file_list_to_string (clipboard_info, TRUE, &len);
                gtk_selection_data_set_text (selection_data, str, len);
                g_free (str);
        } else if (target == copied_files_atom) {
                char *str;
                gsize len;

                str = convert_file_list_to_string (clipboard_info, FALSE, &len);
                gtk_selection_data_set (selection_data, copied_files_atom, 8, str, len);
                g_free (str);
        }
}

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Signal handlers to enable undo in Gtk Widgets.
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: Gene Z. Ragan <gzr@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#include <config.h>
#include <gtk/gtk.h>

#include <glib/gi18n.h>
#include <libnemo-private/nemo-undo.h>

#include <string.h>

#include "nemo-undo-signal-handlers.h"


typedef struct {
	char *undo_text;
	gint position;
	guint selection_start;
	guint selection_end;
} EditableUndoData;

typedef struct {
	gboolean undo_registered;
} EditableUndoObjectData;


static void restore_editable_from_undo_snapshot_callback (GObject 	*target, 
							  gpointer 	callback_data);
static void editable_register_edit_undo 		 (GtkEditable 	*editable);
static void free_editable_object_data 			 (gpointer 	data);

/* nemo_undo_set_up_nemo_entry_for_undo
 * 
 * Functions and callback methods to handle undo 
 * in a NemoEntry
 */

static void 
nemo_entry_user_changed_callback (NemoEntry *entry)
{		
	/* Register undo transaction */	
	editable_register_edit_undo (GTK_EDITABLE (entry));
}

void
nemo_undo_set_up_nemo_entry_for_undo (NemoEntry *entry)
{
	EditableUndoObjectData *data;
	
	if (!NEMO_IS_ENTRY (entry) ) {
		return;
	}

	data = g_new(EditableUndoObjectData, 1);
	data->undo_registered = FALSE;
	g_object_set_data_full (G_OBJECT (entry), "undo_registered", 
				data, free_editable_object_data);

	/* Connect to entry signals */
	g_signal_connect (entry, "user_changed",
			  G_CALLBACK (nemo_entry_user_changed_callback),
			  NULL);
}

void
nemo_undo_tear_down_nemo_entry_for_undo (NemoEntry *entry)
{
	if (!NEMO_IS_ENTRY (entry) ) {
		return;
	}

	/* Disconnect from entry signals */
	g_signal_handlers_disconnect_by_func
		(entry, G_CALLBACK (nemo_entry_user_changed_callback), NULL);

}

/* nemo_undo_set_up_nemo_entry_for_undo
 * 
 * Functions and callback methods to handle undo 
 * in a NemoEntry
 */

static void 
free_editable_undo_data (gpointer data)
{
	EditableUndoData *undo_data;

	undo_data = (EditableUndoData *) data;
	
	g_free (undo_data->undo_text);
	g_free (undo_data);
}

static void 
free_editable_object_data (gpointer data)
{
	g_free (data);
}


static void 
editable_insert_text_callback (GtkEditable *editable)
{
	/* Register undo transaction */	
	editable_register_edit_undo (editable);
}

static void 
editable_delete_text_callback (GtkEditable *editable)
{
	/* Register undo transaction */	
	editable_register_edit_undo (editable);
}

static void
editable_register_edit_undo (GtkEditable *editable)
{	
	EditableUndoData *undo_data;
	EditableUndoObjectData *undo_info;
	gpointer data;

	if (!GTK_IS_EDITABLE (editable) ) {
		return;
	}

	/* Check our undo registered flag */
	data = g_object_get_data (G_OBJECT (editable), "undo_registered");
	if (data == NULL) {
		g_warning ("Undo data is NULL");
		return;
	}

	undo_info = (EditableUndoObjectData *)data;		
	if (undo_info->undo_registered) {
		return;
	}
	
	undo_data = g_new0 (EditableUndoData, 1);
	undo_data->undo_text = gtk_editable_get_chars (editable, 0, -1);
	undo_data->position = gtk_editable_get_position (editable);
	gtk_editable_get_selection_bounds (editable,
					   &undo_data->selection_start,
					   &undo_data->selection_end);

	nemo_undo_register
		(G_OBJECT (editable),
		 restore_editable_from_undo_snapshot_callback,
		 undo_data,
		 (GDestroyNotify) free_editable_undo_data,
		 _("Edit"),
		 _("Undo Edit"),
		 _("Undo the edit"),
		 _("Redo Edit"),
		 _("Redo the edit"));

	undo_info->undo_registered = TRUE;
}

void
nemo_undo_set_up_editable_for_undo (GtkEditable *editable)
{
	EditableUndoObjectData *data;
	
	if (!GTK_IS_EDITABLE (editable) ) {
		return;
	}

	/* Connect to editable signals */
	g_signal_connect (editable, "insert_text",
			  G_CALLBACK (editable_insert_text_callback), NULL);
	g_signal_connect (editable, "delete_text",
			  G_CALLBACK (editable_delete_text_callback), NULL);


	data = g_new (EditableUndoObjectData, 1);
	data->undo_registered = FALSE;
	g_object_set_data_full (G_OBJECT (editable), "undo_registered", 
				data, free_editable_object_data);
}

void
nemo_undo_tear_down_editable_for_undo (GtkEditable *editable)
{
	if (!GTK_IS_EDITABLE (editable) ) {
		return;
	}

	/* Disconnect from entry signals */
	g_signal_handlers_disconnect_by_func
		(editable, G_CALLBACK (editable_insert_text_callback), NULL);
	g_signal_handlers_disconnect_by_func
		(editable, G_CALLBACK (editable_delete_text_callback), NULL);
}

/* restore_editable_from_undo_snapshot_callback
 * 
 * Restore edited text.
 */
static void
restore_editable_from_undo_snapshot_callback (GObject *target, gpointer callback_data)
{
	GtkEditable *editable;
	GtkWindow *window;
	EditableUndoData *undo_data;
	EditableUndoObjectData *data;
	gint position;
	
	editable = GTK_EDITABLE (target);
	undo_data = (EditableUndoData *) callback_data;

	/* Check our undo registered flag */
	data = g_object_get_data (target, "undo_registered");
	if (data == NULL) {
		g_warning ("Undo regisetred flag not found");
		return;
	}
	
	/* Reset the registered flag so we get a new item for future editing. */
	data->undo_registered = FALSE;

	/* Register a new undo transaction for redo. */
	editable_register_edit_undo (editable);
	
	/* Restore the text. */
	position = 0;
	gtk_editable_delete_text (editable, 0, -1);
	gtk_editable_insert_text (editable, undo_data->undo_text,
				  strlen (undo_data->undo_text), &position);

	/* Set focus to widget */
	window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (target)));
	gtk_window_set_focus (window, GTK_WIDGET (editable));

	/* We have to do this call, because the previous call selects all text */
	gtk_editable_select_region (editable, 0, 0);

	/* Restore selection */
	gtk_editable_select_region (editable, undo_data->selection_start, 
			   	    undo_data->selection_end);
	
	/* Set the i-beam to the saved position */
	gtk_editable_set_position (editable, undo_data->position);

	/* Reset the registered flag so we get a new item for future editing. */
	data->undo_registered = FALSE;
}

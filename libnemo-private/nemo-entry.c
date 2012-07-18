/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NemoEntry: one-line text editing widget. This consists of bug fixes
 * and other improvements to GtkEntry, and all the changes could be rolled
 * into GtkEntry some day.
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: John Sullivan <sullivan@eazel.com>
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
#include "nemo-entry.h"

#include <string.h>
#include "nemo-global-preferences.h"
#include "nemo-undo-signal-handlers.h"
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

struct NemoEntryDetails {
	gboolean user_edit;
	gboolean special_tab_handling;

	guint select_idle_id;
};

enum {
	USER_CHANGED,
	SELECTION_CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

static void nemo_entry_editable_init (GtkEditableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (NemoEntry, nemo_entry, GTK_TYPE_ENTRY,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
						nemo_entry_editable_init));

static GtkEditableInterface *parent_editable_interface = NULL;

static void
nemo_entry_init (NemoEntry *entry)
{
	entry->details = g_new0 (NemoEntryDetails, 1);
	
	entry->details->user_edit = TRUE;

	nemo_undo_set_up_nemo_entry_for_undo (entry);
}

GtkWidget *
nemo_entry_new (void)
{
	return gtk_widget_new (NEMO_TYPE_ENTRY, NULL);
}

GtkWidget *
nemo_entry_new_with_max_length (guint16 max)
{
	GtkWidget *widget;

	widget = gtk_widget_new (NEMO_TYPE_ENTRY, NULL);
	gtk_entry_set_max_length (GTK_ENTRY (widget), max);

	return widget;
}

static void
nemo_entry_finalize (GObject *object)
{
	NemoEntry *entry;

	entry = NEMO_ENTRY (object);

	if (entry->details->select_idle_id != 0) {
		g_source_remove (entry->details->select_idle_id);
	}
	
	g_free (entry->details);

	G_OBJECT_CLASS (nemo_entry_parent_class)->finalize (object);
}

static gboolean
nemo_entry_key_press (GtkWidget *widget, GdkEventKey *event)
{
	NemoEntry *entry;
	GtkEditable *editable;
	int position;
	gboolean old_has, new_has;
	gboolean result;
	
	entry = NEMO_ENTRY (widget);
	editable = GTK_EDITABLE (widget);
	
	if (!gtk_editable_get_editable (editable)) {
		return FALSE;
	}

	switch (event->keyval) {
	case GDK_KEY_Tab:
		/* The location bar entry wants TAB to work kind of
		 * like it does in the shell for command completion,
		 * so if we get a tab and there's a selection, we
		 * should position the insertion point at the end of
		 * the selection.
		 */
		if (entry->details->special_tab_handling && gtk_editable_get_selection_bounds (editable, NULL, NULL)) {
			position = strlen (gtk_entry_get_text (GTK_ENTRY (editable)));
			gtk_editable_select_region (editable, position, position);
			return TRUE;
		}
		break;

	default:
		break;
	}
	
	old_has = gtk_editable_get_selection_bounds (editable, NULL, NULL);

	result = GTK_WIDGET_CLASS (nemo_entry_parent_class)->key_press_event (widget, event);

	/* Pressing a key usually changes the selection if there is a selection.
	 * If there is not selection, we can save work by not emitting a signal.
	 */
	if (result) {
		new_has = gtk_editable_get_selection_bounds (editable, NULL, NULL);
		if (old_has || new_has) {
			g_signal_emit (widget, signals[SELECTION_CHANGED], 0);
		}
	}

	return result;
	
}

static gboolean
nemo_entry_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
	int result;
	gboolean old_had, new_had;
	int old_start, old_end, new_start, new_end;
	GtkEditable *editable;

	editable = GTK_EDITABLE (widget);

	old_had = gtk_editable_get_selection_bounds (editable, &old_start, &old_end);

	result = GTK_WIDGET_CLASS (nemo_entry_parent_class)->motion_notify_event (widget, event);

	/* Send a signal if dragging the mouse caused the selection to change. */
	if (result) {
		new_had = gtk_editable_get_selection_bounds (editable, &new_start, &new_end);
		if (old_had != new_had || (old_had && (old_start != new_start || old_end != new_end))) {
			g_signal_emit (widget, signals[SELECTION_CHANGED], 0);
		}
	}
	
	return result;
}

/**
 * nemo_entry_select_all
 *
 * Select all text, leaving the text cursor position at the end.
 * 
 * @entry: A NemoEntry
 **/
void
nemo_entry_select_all (NemoEntry *entry)
{
	g_return_if_fail (NEMO_IS_ENTRY (entry));

	gtk_editable_set_position (GTK_EDITABLE (entry), -1);
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
}

static gboolean
select_all_at_idle (gpointer callback_data)
{
	NemoEntry *entry;

	entry = NEMO_ENTRY (callback_data);

	nemo_entry_select_all (entry);

	entry->details->select_idle_id = 0;
	
	return FALSE;
}

/**
 * nemo_entry_select_all_at_idle
 *
 * Select all text at the next idle, not immediately.
 * This is useful when reacting to a key press, because
 * changing the selection and the text cursor position doesn't
 * work in a key_press signal handler.
 * 
 * @entry: A NemoEntry
 **/
void
nemo_entry_select_all_at_idle (NemoEntry *entry)
{
	g_return_if_fail (NEMO_IS_ENTRY (entry));

	/* If the text cursor position changes in this routine
	 * then gtk_entry_key_press will unselect (and we want
	 * to move the text cursor position to the end).
	 */

	if (entry->details->select_idle_id == 0) {
		entry->details->select_idle_id = g_idle_add (select_all_at_idle, entry);
	}
}

/**
 * nemo_entry_set_text
 *
 * This function wraps gtk_entry_set_text.  It sets undo_registered
 * to TRUE and preserves the old value for a later restore.  This is
 * done so the programmatic changes to the entry do not register
 * with the undo manager.
 *  
 * @entry: A NemoEntry
 * @test: The text to set
 **/

void
nemo_entry_set_text (NemoEntry *entry, const gchar *text)
{
	g_return_if_fail (NEMO_IS_ENTRY (entry));

	entry->details->user_edit = FALSE;
	gtk_entry_set_text (GTK_ENTRY (entry), text);
	entry->details->user_edit = TRUE;
	
	g_signal_emit (entry, signals[SELECTION_CHANGED], 0);
}

static void
nemo_entry_set_selection_bounds (GtkEditable *editable,
				     int start_pos,
				     int end_pos)
{
	parent_editable_interface->set_selection_bounds (editable, start_pos, end_pos);

	g_signal_emit (editable, signals[SELECTION_CHANGED], 0);
}

static gboolean
nemo_entry_button_press (GtkWidget *widget,
			     GdkEventButton *event)
{
	gboolean result;

	result = GTK_WIDGET_CLASS (nemo_entry_parent_class)->button_press_event (widget, event);

	if (result) {
		g_signal_emit (widget, signals[SELECTION_CHANGED], 0);
	}

	return result;
}

static gboolean
nemo_entry_button_release (GtkWidget *widget,
			       GdkEventButton *event)
{
	gboolean result;

	result = GTK_WIDGET_CLASS (nemo_entry_parent_class)->button_release_event (widget, event);

	if (result) {
		g_signal_emit (widget, signals[SELECTION_CHANGED], 0);
	}

	return result;
}

static void
nemo_entry_insert_text (GtkEditable *editable, const gchar *text,
			    int length, int *position)
{
	NemoEntry *entry;

	entry = NEMO_ENTRY(editable);

	/* Fire off user changed signals */
	if (entry->details->user_edit) {
		g_signal_emit (editable, signals[USER_CHANGED], 0);
	}

	parent_editable_interface->insert_text (editable, text, length, position);

	g_signal_emit (editable, signals[SELECTION_CHANGED], 0);
}
			 		     
static void 
nemo_entry_delete_text (GtkEditable *editable, int start_pos, int end_pos)
{
	NemoEntry *entry;
	
	entry = NEMO_ENTRY (editable);

	/* Fire off user changed signals */
	if (entry->details->user_edit) {
		g_signal_emit (editable, signals[USER_CHANGED], 0);
	}

	parent_editable_interface->delete_text (editable, start_pos, end_pos);

	g_signal_emit (editable, signals[SELECTION_CHANGED], 0);
}

/* Overridden to work around GTK bug. The selection_clear_event is queued
 * when the selection changes. Changing the selection to NULL and then
 * back to the original selection owner still sends the event, so the
 * selection owner then gets the selection ripped away from it. We ran into
 * this with type-completion behavior in NemoLocationBar (see bug 5313).
 * There's a FIXME comment that seems to be about this same issue in
 * gtk+/gtkselection.c, gtk_selection_clear.
 */
static gboolean
nemo_entry_selection_clear (GtkWidget *widget,
			        GdkEventSelection *event)
{
	g_assert (NEMO_IS_ENTRY (widget));
	
	if (gdk_selection_owner_get (event->selection) == gtk_widget_get_window (widget)) {
		return FALSE;
	}
	
	return GTK_WIDGET_CLASS (nemo_entry_parent_class)->selection_clear_event (widget, event);
}

static void
nemo_entry_editable_init (GtkEditableInterface *iface)
{
	parent_editable_interface = g_type_interface_peek_parent (iface);

	iface->insert_text = nemo_entry_insert_text;
	iface->delete_text = nemo_entry_delete_text;
	iface->set_selection_bounds = nemo_entry_set_selection_bounds;

	/* Otherwise we might need some memcpy loving */
	g_assert (iface->do_insert_text != NULL);
	g_assert (iface->get_position != NULL);
	g_assert (iface->get_chars != NULL);
}

static void
nemo_entry_class_init (NemoEntryClass *class)
{
	GtkWidgetClass *widget_class;
	GObjectClass *gobject_class;

	widget_class = GTK_WIDGET_CLASS (class);
	gobject_class = G_OBJECT_CLASS (class);
		
	widget_class->button_press_event = nemo_entry_button_press;
	widget_class->button_release_event = nemo_entry_button_release;
	widget_class->key_press_event = nemo_entry_key_press;
	widget_class->motion_notify_event = nemo_entry_motion_notify;
	widget_class->selection_clear_event = nemo_entry_selection_clear;
	
	gobject_class->finalize = nemo_entry_finalize;

	/* Set up signals */
	signals[USER_CHANGED] = g_signal_new
		("user_changed",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NemoEntryClass, user_changed),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
	signals[SELECTION_CHANGED] = g_signal_new
		("selection_changed",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NemoEntryClass, selection_changed),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
}

void
nemo_entry_set_special_tab_handling (NemoEntry *entry,
					 gboolean special_tab_handling)
{
	g_return_if_fail (NEMO_IS_ENTRY (entry));

	entry->details->special_tab_handling = special_tab_handling;
}



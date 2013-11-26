/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-clipboard.c
 *
 * Nemo Clipboard support.  For now, routines to support component cut
 * and paste.
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000, 2001  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Rebecca Schulman <rebecka@eazel.com>,
 *          Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "nemo-clipboard.h"
#include "nemo-file-utilities.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

typedef struct _TargetCallbackData TargetCallbackData;

typedef void (* SelectAllCallback)    (gpointer target);
typedef void (* ConnectCallbacksFunc) (GObject            *object,
				       TargetCallbackData *target_data);

static void selection_changed_callback            (GtkWidget *widget,
						   gpointer callback_data);
static void owner_change_callback (GtkClipboard        *clipboard,
				   GdkEventOwnerChange *event,
				   gpointer callback_data);
struct _TargetCallbackData {
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	gboolean shares_selection_changes;

	SelectAllCallback select_all_callback;

	ConnectCallbacksFunc connect_callbacks;
	ConnectCallbacksFunc disconnect_callbacks;
};

static void
cut_callback (gpointer target)
{
	g_assert (target != NULL);

	g_signal_emit_by_name (target, "cut-clipboard");
}

static void
copy_callback (gpointer target)
{
	g_assert (target != NULL);

	g_signal_emit_by_name (target, "copy-clipboard");
}

static void
paste_callback (gpointer target)
{
	g_assert (target != NULL);

	g_signal_emit_by_name (target, "paste-clipboard");
}

static void
editable_select_all_callback (gpointer target)
{
	GtkEditable *editable;

	editable = GTK_EDITABLE (target);
	g_assert (editable != NULL);

	gtk_editable_set_position (editable, -1);
	gtk_editable_select_region (editable, 0, -1);
}

static void
action_cut_callback (GtkAction *action,
		     gpointer callback_data)
{
	cut_callback (callback_data);
}

static void
action_copy_callback (GtkAction *action,
		      gpointer callback_data)
{
	copy_callback (callback_data);
}

static void
action_paste_callback (GtkAction *action,
		       gpointer callback_data)
{
	paste_callback (callback_data);
}

static void
action_select_all_callback (GtkAction *action,
			    gpointer callback_data)
{
	TargetCallbackData *target_data;

	g_assert (callback_data != NULL);

	target_data = g_object_get_data (callback_data, "Nemo:clipboard_target_data");
	g_assert (target_data != NULL);

	target_data->select_all_callback (callback_data);
}

static void
received_clipboard_contents (GtkClipboard     *clipboard,
			     GtkSelectionData *selection_data,
			     gpointer          data)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = data;

	action = gtk_action_group_get_action (action_group,
					      "Paste");
	if (action != NULL) {
		gtk_action_set_sensitive (action,
					  gtk_selection_data_targets_include_text (selection_data));
	}

	g_object_unref (action_group);
}


static void
set_paste_sensitive_if_clipboard_contains_data (GtkActionGroup *action_group)
{
	GtkAction *action;
	if (gdk_display_supports_selection_notification (gdk_display_get_default ())) {
		gtk_clipboard_request_contents (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
						gdk_atom_intern ("TARGETS", FALSE),
						received_clipboard_contents,
						g_object_ref (action_group));
	} else {
		/* If selection notification isn't supported, always activate Paste */
		action = gtk_action_group_get_action (action_group,
						      "Paste");
		gtk_action_set_sensitive (action, TRUE);
	}
}

static void
set_clipboard_menu_items_sensitive (GtkActionGroup *action_group)
{
	GtkAction *action;

	action = gtk_action_group_get_action (action_group,
					      "Cut");
	gtk_action_set_sensitive (action, TRUE);
	action = gtk_action_group_get_action (action_group,
					      "Copy");
	gtk_action_set_sensitive (action, TRUE);
}

static void
set_clipboard_menu_items_insensitive (GtkActionGroup *action_group)
{
	GtkAction *action;

	action = gtk_action_group_get_action (action_group,
					      "Cut");
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (action_group,
					      "Copy");
	gtk_action_set_sensitive (action, FALSE);
}

static gboolean
clipboard_items_are_merged_in (GtkWidget *widget)
{
	return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
						   "Nemo:clipboard_menu_items_merged"));
}

static void
set_clipboard_items_are_merged_in (GObject *widget_as_object,
				   gboolean merged_in)
{
	g_object_set_data (widget_as_object,
			   "Nemo:clipboard_menu_items_merged",
			   GINT_TO_POINTER (merged_in));
}

static void
editable_connect_callbacks (GObject *object,
			    TargetCallbackData *target_data)
{
	g_signal_connect_after (object, "selection_changed",
				G_CALLBACK (selection_changed_callback), target_data);
	selection_changed_callback (GTK_WIDGET (object),
				    target_data);
}

static void
editable_disconnect_callbacks (GObject *object,
			       TargetCallbackData *target_data)
{
	g_signal_handlers_disconnect_matched (object,
					      G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
					      0, 0, NULL,
					      G_CALLBACK (selection_changed_callback),
					      target_data);
}

static void
merge_in_clipboard_menu_items (GObject *widget_as_object,
			       TargetCallbackData *target_data)
{
	gboolean add_selection_callback;

	g_assert (target_data != NULL);
	
	add_selection_callback = target_data->shares_selection_changes;

	gtk_ui_manager_insert_action_group (target_data->ui_manager,
					    target_data->action_group, 0);

	set_paste_sensitive_if_clipboard_contains_data (target_data->action_group);
	
	g_signal_connect (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), "owner_change",
			  G_CALLBACK (owner_change_callback), target_data);
	
	if (add_selection_callback) {
		target_data->connect_callbacks (widget_as_object, target_data);
	} else {
		/* If we don't use sensitivity, everything should be on */
		set_clipboard_menu_items_sensitive (target_data->action_group);
	}
	set_clipboard_items_are_merged_in (widget_as_object, TRUE);
}

static void
merge_out_clipboard_menu_items (GObject *widget_as_object,
				TargetCallbackData *target_data)

{
	gboolean selection_callback_was_added;

	g_assert (target_data != NULL);

	gtk_ui_manager_remove_action_group (target_data->ui_manager,
					    target_data->action_group);

	g_signal_handlers_disconnect_matched (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
					      G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
					      0, 0, NULL,
					      G_CALLBACK (owner_change_callback),
					      target_data);
	
	selection_callback_was_added = target_data->shares_selection_changes;

	if (selection_callback_was_added) {
		target_data->disconnect_callbacks (widget_as_object, target_data);
	}
	set_clipboard_items_are_merged_in (widget_as_object, FALSE);
}

static gboolean
focus_changed_callback (GtkWidget *widget,
			GdkEventAny *event,
			gpointer callback_data)
{
	/* Connect the component to the container if the widget has focus. */
	if (gtk_widget_has_focus (widget)) {
		if (!clipboard_items_are_merged_in (widget)) {
			merge_in_clipboard_menu_items (G_OBJECT (widget), callback_data);
		}
	} else {
		if (clipboard_items_are_merged_in (widget)) {
			merge_out_clipboard_menu_items (G_OBJECT (widget), callback_data);
		}
	}

	return FALSE;
}

static void
selection_changed_callback (GtkWidget *widget,
			    gpointer callback_data)
{
	TargetCallbackData *target_data;
	GtkEditable *editable;
	int start, end;

	target_data = (TargetCallbackData *) callback_data;
	g_assert (target_data != NULL);

	editable = GTK_EDITABLE (widget);
	g_assert (editable != NULL);

	if (gtk_editable_get_selection_bounds (editable, &start, &end) && start != end) {
		set_clipboard_menu_items_sensitive (target_data->action_group);
	} else {
		set_clipboard_menu_items_insensitive (target_data->action_group);
	}
}

static void
owner_change_callback (GtkClipboard        *clipboard,
		       GdkEventOwnerChange *event,
		       gpointer callback_data)
{
	TargetCallbackData *target_data;

	g_assert (callback_data != NULL);
	target_data = callback_data;

	set_paste_sensitive_if_clipboard_contains_data (target_data->action_group);
}

static void
target_destroy_callback (GtkWidget *object,
			 gpointer callback_data)
{
	g_assert (callback_data != NULL);

	if (clipboard_items_are_merged_in (object)) {
		merge_out_clipboard_menu_items (G_OBJECT (object), callback_data);
	}
}

static void
target_data_free (TargetCallbackData *target_data)
{
	g_object_unref (target_data->action_group);
	g_free (target_data);
}

static const GtkActionEntry clipboard_entries[] = {
  /* name, stock id */      { "Cut", GTK_STOCK_CUT,
  /* label, accelerator */    NULL, NULL,
  /* tooltip */               N_("Cut the selected text to the clipboard"),
                              G_CALLBACK (action_cut_callback) },
  /* name, stock id */      { "Copy", GTK_STOCK_COPY,
  /* label, accelerator */    NULL, NULL,
  /* tooltip */               N_("Copy the selected text to the clipboard"),
                              G_CALLBACK (action_copy_callback) },
  /* name, stock id */      { "Paste", GTK_STOCK_PASTE,
  /* label, accelerator */    NULL, NULL,
  /* tooltip */               N_("Paste the text stored on the clipboard"),
                              G_CALLBACK (action_paste_callback) },
  /* name, stock id */      { "Select All", NULL,
  /* label, accelerator */    N_("Select _All"), "<control>A",
  /* tooltip */               N_("Select all the text in a text field"),
                              G_CALLBACK (action_select_all_callback) },
};

static TargetCallbackData *
initialize_clipboard_component_with_callback_data (GtkEditable *target,
						   GtkUIManager *ui_manager,
						   gboolean shares_selection_changes,
						   SelectAllCallback select_all_callback,
						   ConnectCallbacksFunc connect_callbacks,
						   ConnectCallbacksFunc disconnect_callbacks)
{
	GtkActionGroup *action_group;
	TargetCallbackData *target_data;

	action_group = gtk_action_group_new ("ClipboardActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group, 
				      clipboard_entries, G_N_ELEMENTS (clipboard_entries),
				      target);
	
	/* Do the actual connection of the UI to the container at
	 * focus time, and disconnect at both focus and destroy
	 * time.
	 */
	target_data = g_new (TargetCallbackData, 1);
	target_data->ui_manager = ui_manager;
	target_data->action_group = action_group;
	target_data->shares_selection_changes = shares_selection_changes;
	target_data->select_all_callback = select_all_callback;
	target_data->connect_callbacks = connect_callbacks;
	target_data->disconnect_callbacks = disconnect_callbacks;

	return target_data;
}

static void
nemo_clipboard_real_set_up (gpointer target,
				GtkUIManager *ui_manager,
				gboolean shares_selection_changes,
				SelectAllCallback select_all_callback,
				ConnectCallbacksFunc connect_callbacks,
				ConnectCallbacksFunc disconnect_callbacks)
{
	TargetCallbackData *target_data;

	if (g_object_get_data (G_OBJECT (target), "Nemo:clipboard_target_data") != NULL) {
		return;
	}

	target_data = initialize_clipboard_component_with_callback_data
		(target, 
		 ui_manager,
		 shares_selection_changes,
		 select_all_callback,
		 connect_callbacks,
		 disconnect_callbacks);

	g_signal_connect (target, "focus_in_event",
			  G_CALLBACK (focus_changed_callback), target_data);
	g_signal_connect (target, "focus_out_event",
			  G_CALLBACK (focus_changed_callback), target_data);
	g_signal_connect (target, "destroy",
			  G_CALLBACK (target_destroy_callback), target_data);

	g_object_set_data_full (G_OBJECT (target), "Nemo:clipboard_target_data",
				target_data, (GDestroyNotify) target_data_free);

	/* Call the focus changed callback once to merge if the window is
	 * already in focus.
	 */
	focus_changed_callback (GTK_WIDGET (target), NULL, target_data);
}

void
nemo_clipboard_set_up_editable (GtkEditable *target,
				    GtkUIManager *ui_manager,
				    gboolean shares_selection_changes)
{
	g_return_if_fail (GTK_IS_EDITABLE (target));
	g_return_if_fail (GTK_IS_UI_MANAGER (ui_manager));

	nemo_clipboard_real_set_up (target, ui_manager,
					shares_selection_changes,
					editable_select_all_callback,
					editable_connect_callbacks,
					editable_disconnect_callbacks);
}

static GList *
convert_lines_to_str_list (char **lines, gboolean *cut)
{
	int i;
	GList *result;

	if (cut) {
		*cut = FALSE;
	}

	if (lines[0] == NULL) {
		return NULL;
	}

	if (strcmp (lines[0], "cut") == 0) {
		if (cut) {
			*cut = TRUE;
		}
	} else if (strcmp (lines[0], "copy") != 0) {
		return NULL;
	}

	result = NULL;
	for (i = 1; lines[i] != NULL; i++) {
		result = g_list_prepend (result, g_strdup (lines[i]));
	}
	return g_list_reverse (result);
}

GList*
nemo_clipboard_get_uri_list_from_selection_data (GtkSelectionData *selection_data,
						     gboolean *cut,
						     GdkAtom copied_files_atom)
{
	GList *items;
	char **lines;

	if (gtk_selection_data_get_data_type (selection_data) != copied_files_atom
	    || gtk_selection_data_get_length (selection_data) <= 0) {
		items = NULL;
	} else {
		gchar *data;
		/* Not sure why it's legal to assume there's an extra byte
		 * past the end of the selection data that it's safe to write
		 * to. But gtk_editable_selection_received does this, so I
		 * think it is OK.
		 */
		data = (gchar *) gtk_selection_data_get_data (selection_data);
		data[gtk_selection_data_get_length (selection_data)] = '\0';
		lines = g_strsplit (data, "\n", 0);
		items = convert_lines_to_str_list (lines, cut);
		g_strfreev (lines);
	}
	
	return items;
}

GtkClipboard *
nemo_clipboard_get (GtkWidget *widget)
{
	return gtk_clipboard_get_for_display (gtk_widget_get_display (GTK_WIDGET (widget)),
					      GDK_SELECTION_CLIPBOARD);
}

void
nemo_clipboard_clear_if_colliding_uris (GtkWidget *widget,
					    const GList *item_uris,
					    GdkAtom copied_files_atom)
{
	GtkSelectionData *data;
	GList *clipboard_item_uris, *l;
	gboolean collision;

	collision = FALSE;
	data = gtk_clipboard_wait_for_contents (nemo_clipboard_get (widget),
						copied_files_atom);
	if (data == NULL) {
		return;
	}

	clipboard_item_uris = nemo_clipboard_get_uri_list_from_selection_data (data, NULL,
										   copied_files_atom);

	for (l = (GList *) item_uris; l; l = l->next) {
		if (g_list_find_custom ((GList *) item_uris, l->data,
					(GCompareFunc) g_strcmp0)) {
			collision = TRUE;
			break;
		}
	}
	
	if (collision) {
		gtk_clipboard_clear (nemo_clipboard_get (widget));
	}
	
	if (clipboard_item_uris) {
		g_list_free_full (clipboard_item_uris, g_free);
	}
}

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 *         Michael Meeks <michael@nuclecu.unam.mx>
 *	   Andy Hertzfeld <andy@eazel.com>
 *
 */

/* nautilus-location-bar.c - Location bar for Nautilus
 */

#include <config.h>
#include "nautilus-location-entry.h"

#include "nautilus-application.h"
#include "nautilus-window-private.h"
#include "nautilus-window.h"
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-entry.h>
#include <libnautilus-private/nautilus-clipboard.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <stdio.h>
#include <string.h>

#define NAUTILUS_DND_URI_LIST_TYPE 	  "text/uri-list"
#define NAUTILUS_DND_TEXT_PLAIN_TYPE 	  "text/plain"

enum {
	NAUTILUS_DND_URI_LIST,
	NAUTILUS_DND_TEXT_PLAIN,
	NAUTILUS_DND_NTARGETS
};

static const GtkTargetEntry drag_types [] = {
	{ NAUTILUS_DND_URI_LIST_TYPE,   0, NAUTILUS_DND_URI_LIST },
	{ NAUTILUS_DND_TEXT_PLAIN_TYPE, 0, NAUTILUS_DND_TEXT_PLAIN },
};

static const GtkTargetEntry drop_types [] = {
	{ NAUTILUS_DND_URI_LIST_TYPE,   0, NAUTILUS_DND_URI_LIST },
	{ NAUTILUS_DND_TEXT_PLAIN_TYPE, 0, NAUTILUS_DND_TEXT_PLAIN },
};

struct NautilusLocationEntryDetails {
	char *current_directory;
	GFilenameCompleter *completer;

	guint idle_id;

	GFile *last_location;

	gboolean has_special_text;
	gboolean setting_special_text;
	gchar *special_text;
	NautilusLocationEntryAction secondary_action;
};

enum {
	CANCEL,
	LOCATION_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (NautilusLocationEntry, nautilus_location_entry, NAUTILUS_TYPE_ENTRY);

void
nautilus_location_entry_focus (NautilusLocationEntry *entry)
{
	/* Put the keyboard focus in the text field when switching to this mode,
	 * and select all text for easy overtyping
	 */
	gtk_widget_grab_focus (GTK_WIDGET (entry));
	nautilus_entry_select_all (NAUTILUS_ENTRY (entry));
}

static GFile *
nautilus_location_entry_get_location (NautilusLocationEntry *entry)
{
	char *user_location;
	GFile *location;

	user_location = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	location = g_file_parse_name (user_location);
	g_free (user_location);

	return location;
}

static void
emit_location_changed (NautilusLocationEntry *entry)
{
	GFile *location;

	location = nautilus_location_entry_get_location (entry);
	g_signal_emit (entry, signals[LOCATION_CHANGED], 0, location);
	g_object_unref (location);
}

static void
nautilus_location_entry_update_action (NautilusLocationEntry *entry)
{
	const char *current_text;
	GFile *location;

	if (entry->details->last_location == NULL){
		nautilus_location_entry_set_secondary_action (entry,
							      NAUTILUS_LOCATION_ENTRY_ACTION_GOTO);
		return;
	}

	current_text = gtk_entry_get_text (GTK_ENTRY (entry));
	location = g_file_parse_name (current_text);

	if (g_file_equal (entry->details->last_location, location)) {
		nautilus_location_entry_set_secondary_action (entry,
							      NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR);
	} else {
		nautilus_location_entry_set_secondary_action (entry,
							      NAUTILUS_LOCATION_ENTRY_ACTION_GOTO);
	}

	g_object_unref (location);
}

static int
get_editable_number_of_chars (GtkEditable *editable)
{
	char *text;
	int length;

	text = gtk_editable_get_chars (editable, 0, -1);
	length = g_utf8_strlen (text, -1);
	g_free (text);
	return length;
}

static void
set_position_and_selection_to_end (GtkEditable *editable)
{
	int end;

	end = get_editable_number_of_chars (editable);
	gtk_editable_select_region (editable, end, end);
	gtk_editable_set_position (editable, end);
}

static void
nautilus_location_entry_update_current_uri (NautilusLocationEntry *entry,
					    const char *uri)
{
	g_free (entry->details->current_directory);
	entry->details->current_directory = g_strdup (uri);

	nautilus_entry_set_text (NAUTILUS_ENTRY (entry), uri);
	set_position_and_selection_to_end (GTK_EDITABLE (entry));
}

void
nautilus_location_entry_set_location (NautilusLocationEntry *entry,
				      GFile                 *location)
{
	gchar *uri, *formatted_uri;

	g_assert (location != NULL);

	/* Note: This is called in reaction to external changes, and
	 * thus should not emit the LOCATION_CHANGED signal. */
	uri = g_file_get_uri (location);
	formatted_uri = g_file_get_parse_name (location);

	if (eel_uri_is_search (uri)) {
		nautilus_location_entry_set_special_text (entry, "");
	} else {
		nautilus_location_entry_update_current_uri (entry, formatted_uri);
	}

	/* remember the original location for later comparison */
	if (!entry->details->last_location ||
	    !g_file_equal (entry->details->last_location, location)) {
		g_clear_object (&entry->details->last_location);
		entry->details->last_location = g_object_ref (location);
	}

	nautilus_location_entry_update_action (entry);

	g_free (uri);
	g_free (formatted_uri);
}

static void
drag_data_received_callback (GtkWidget *widget,
			     GdkDragContext *context,
			     int x,
			     int y,
			     GtkSelectionData *data,
			     guint info,
			     guint32 time,
			     gpointer callback_data)
{
	char **names;
	NautilusApplication *application;
	int name_count;
	NautilusWindow *new_window;
	GtkWidget *window;
	GdkScreen      *screen;
	gboolean new_windows_for_extras;
	char *prompt;
	char *detail;
	GFile *location;
	NautilusLocationEntry *self = NAUTILUS_LOCATION_ENTRY (widget);

	g_assert (data != NULL);
	g_assert (callback_data == NULL);

	names = g_uri_list_extract_uris ((const gchar *) gtk_selection_data_get_data (data));

	if (names == NULL || *names == NULL) {
		g_warning ("No D&D URI's");
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	window = gtk_widget_get_toplevel (widget);
	new_windows_for_extras = FALSE;
	/* Ask user if they really want to open multiple windows
	 * for multiple dropped URIs. This is likely to have been
	 * a mistake.
	 */
	name_count = g_strv_length (names);
	if (name_count > 1) {
		prompt = g_strdup_printf (ngettext("Do you want to view %d location?",
						   "Do you want to view %d locations?",
						   name_count),
					  name_count);
		detail = g_strdup_printf (ngettext("This will open %d separate window.",
						   "This will open %d separate windows.",
						   name_count),
					  name_count);
		/* eel_run_simple_dialog should really take in pairs
		 * like gtk_dialog_new_with_buttons() does. */
		new_windows_for_extras = eel_run_simple_dialog (GTK_WIDGET (window),
								TRUE,
								GTK_MESSAGE_QUESTION,
								prompt,
								detail,
								_("_Cancel"), _("_OK"),
								NULL) != 0 /* GNOME_OK */;

		g_free (prompt);
		g_free (detail);

		if (!new_windows_for_extras) {
			gtk_drag_finish (context, FALSE, FALSE, time);
			return;
		}
	}

	location = g_file_new_for_uri (names[0]);
	nautilus_location_entry_set_location (self, location);
	emit_location_changed (self);
	g_object_unref (location);

	if (new_windows_for_extras) {
		int i;

		application = NAUTILUS_APPLICATION (g_application_get_default ());
		screen = gtk_window_get_screen (GTK_WINDOW (window));

		for (i = 1; names[i] != NULL; ++i) {
			new_window = nautilus_application_create_window (application, screen);
			location = g_file_new_for_uri (names[i]);
			nautilus_window_go_to (new_window, location);
			g_object_unref (location);
		}
	}

	g_strfreev (names);

	gtk_drag_finish (context, TRUE, FALSE, time);
}

static void
drag_data_get_callback (GtkWidget *widget,
			GdkDragContext *context,
			GtkSelectionData *selection_data,
			guint info,
			guint32 time,
			gpointer callback_data)
{
	NautilusLocationEntry *self;
	GFile *location;
	gchar *uri;

	g_assert (selection_data != NULL);
	self = callback_data;

	location = nautilus_location_entry_get_location (self);
	uri = g_file_get_uri (location);

	switch (info) {
	case NAUTILUS_DND_URI_LIST:
	case NAUTILUS_DND_TEXT_PLAIN:
		gtk_selection_data_set (selection_data,
					gtk_selection_data_get_target (selection_data),
					8, (guchar *) uri,
					strlen (uri));
		break;
	default:
		g_assert_not_reached ();
	}
	g_free (uri);
	g_object_unref (location);
}

/* routine that performs the tab expansion.  Extract the directory name and
   incomplete basename, then iterate through the directory trying to complete it.  If we
   find something, add it to the entry */
  
static gboolean
try_to_expand_path (gpointer callback_data)
{
	NautilusLocationEntry *entry;
	GtkEditable *editable;
	char *suffix, *user_location, *absolute_location, *uri_scheme;
	int user_location_length, pos;

	entry = NAUTILUS_LOCATION_ENTRY (callback_data);
	editable = GTK_EDITABLE (entry);
	user_location = gtk_editable_get_chars (editable, 0, -1);
	user_location_length = g_utf8_strlen (user_location, -1);
	entry->details->idle_id = 0;

	uri_scheme = g_uri_parse_scheme (user_location);

	if (!g_path_is_absolute (user_location) && uri_scheme == NULL && user_location[0] != '~') {
		absolute_location = g_build_filename (entry->details->current_directory, user_location, NULL);
		suffix = g_filename_completer_get_completion_suffix (entry->details->completer,
							     absolute_location);
		g_free (absolute_location);
	} else {
		suffix = g_filename_completer_get_completion_suffix (entry->details->completer,
							     user_location);
	}

	g_free (user_location);
	g_free (uri_scheme);

	/* if we've got something, add it to the entry */
	if (suffix != NULL) {
		pos = user_location_length;
		gtk_editable_insert_text (editable,
					  suffix, -1,  &pos);
		pos = user_location_length;
		gtk_editable_select_region (editable, pos, -1);
		
		g_free (suffix);
	}

	return FALSE;
}

/* Until we have a more elegant solution, this is how we figure out if
 * the GtkEntry inserted characters, assuming that the return value is
 * TRUE indicating that the GtkEntry consumed the key event for some
 * reason. This is a clone of code from GtkEntry.
 */
static gboolean
entry_would_have_inserted_characters (const GdkEventKey *event)
{
	switch (event->keyval) {
	case GDK_KEY_BackSpace:
	case GDK_KEY_Clear:
	case GDK_KEY_Insert:
	case GDK_KEY_Delete:
	case GDK_KEY_Home:
	case GDK_KEY_End:
	case GDK_KEY_KP_Home:
	case GDK_KEY_KP_End:
	case GDK_KEY_Left:
	case GDK_KEY_Right:
	case GDK_KEY_KP_Left:
	case GDK_KEY_KP_Right:
	case GDK_KEY_Return:
		return FALSE;
	default:
		if (event->keyval >= 0x20 && event->keyval <= 0xFF) {
			if ((event->state & GDK_CONTROL_MASK) != 0) {
				return FALSE;
			}
			if ((event->state & GDK_MOD1_MASK) != 0) {
				return FALSE;
			}
		}
		return event->length > 0;
	}
}

static gboolean
position_and_selection_are_at_end (GtkEditable *editable)
{
	int end;
	int start_sel, end_sel;
	
	end = get_editable_number_of_chars (editable);
	if (gtk_editable_get_selection_bounds (editable, &start_sel, &end_sel)) {
		if (start_sel != end || end_sel != end) {
			return FALSE;
		}
	}
	return gtk_editable_get_position (editable) == end;
}

static void
got_completion_data_callback (GFilenameCompleter *completer,
			      NautilusLocationEntry *entry)
{
	if (entry->details->idle_id) {
		g_source_remove (entry->details->idle_id);
		entry->details->idle_id = 0;
	}
	try_to_expand_path (entry);
}

static void
editable_event_after_callback (GtkEntry *entry,
			       GdkEvent *event,
			       NautilusLocationEntry *location_entry)
{
	GtkEditable *editable;
	GdkEventKey *keyevent;

	if (event->type != GDK_KEY_PRESS) {
		return;
	}

	editable = GTK_EDITABLE (entry);
	keyevent = (GdkEventKey *)event;

	/* After typing the right arrow key we move the selection to
	 * the end, if we have a valid selection - since this is most
	 * likely an auto-completion. We ignore shift / control since
	 * they can validly be used to extend the selection.
	 */
	if ((keyevent->keyval == GDK_KEY_Right || keyevent->keyval == GDK_KEY_End) &&
	    !(keyevent->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK)) && 
	    gtk_editable_get_selection_bounds (editable, NULL, NULL)) {
		set_position_and_selection_to_end (editable);
	}

	/* Only do expanding when we are typing at the end of the
	 * text. Do the expand at idle time to avoid slowing down
	 * typing when the directory is large. Only trigger the expand
	 * when we type a key that would have inserted characters.
	 */
	if (position_and_selection_are_at_end (editable)) {
		if (entry_would_have_inserted_characters (keyevent)) {
			if (location_entry->details->idle_id == 0) {
				location_entry->details->idle_id = g_idle_add (try_to_expand_path, location_entry);
			}
		}
	} else {
		/* FIXME: Also might be good to do this when you click
		 * to change the position or selection.
		 */
		if (location_entry->details->idle_id != 0) {
			g_source_remove (location_entry->details->idle_id);
			location_entry->details->idle_id = 0;
		}
	}
}

static void
finalize (GObject *object)
{
	NautilusLocationEntry *entry;

	entry = NAUTILUS_LOCATION_ENTRY (object);

	g_object_unref (entry->details->completer);
	g_free (entry->details->special_text);

	g_clear_object (&entry->details->last_location);

	G_OBJECT_CLASS (nautilus_location_entry_parent_class)->finalize (object);
}

static void
destroy (GtkWidget *object)
{
	NautilusLocationEntry *entry;

	entry = NAUTILUS_LOCATION_ENTRY (object);
	
	/* cancel the pending idle call, if any */
	if (entry->details->idle_id != 0) {
		g_source_remove (entry->details->idle_id);
		entry->details->idle_id = 0;
	}
	
	g_free (entry->details->current_directory);
	entry->details->current_directory = NULL;
	
	GTK_WIDGET_CLASS (nautilus_location_entry_parent_class)->destroy (object);
}

static void
nautilus_location_entry_text_changed (NautilusLocationEntry *entry,
				      GParamSpec            *pspec)
{
	if (entry->details->setting_special_text) {
		return;
	}

	entry->details->has_special_text = FALSE;
}

static void
nautilus_location_entry_icon_release (GtkEntry *gentry,
				      GtkEntryIconPosition position,
				      GdkEvent *event,
				      gpointer unused)
{
	switch (NAUTILUS_LOCATION_ENTRY (gentry)->details->secondary_action) {
	case NAUTILUS_LOCATION_ENTRY_ACTION_GOTO:
		g_signal_emit_by_name (gentry, "activate", gentry);
		break;
	case NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR:
		gtk_entry_set_text (gentry, "");
		break;
	default:
		g_assert_not_reached ();
	}
}

static gboolean
nautilus_location_entry_focus_in (GtkWidget     *widget,
				  GdkEventFocus *event)
{
	NautilusLocationEntry *entry = NAUTILUS_LOCATION_ENTRY (widget);

	if (entry->details->has_special_text) {
		entry->details->setting_special_text = TRUE;
		gtk_entry_set_text (GTK_ENTRY (entry), "");
		entry->details->setting_special_text = FALSE;
	}

	return GTK_WIDGET_CLASS (nautilus_location_entry_parent_class)->focus_in_event (widget, event);
}

static void
nautilus_location_entry_activate (GtkEntry *entry)
{
	NautilusLocationEntry *loc_entry;
	const gchar *entry_text;
	gchar *full_path, *uri_scheme = NULL;

	loc_entry = NAUTILUS_LOCATION_ENTRY (entry);
	entry_text = gtk_entry_get_text (entry);

	if (entry_text != NULL && *entry_text != '\0') {
		uri_scheme = g_uri_parse_scheme (entry_text);

		if (!g_path_is_absolute (entry_text) && uri_scheme == NULL && entry_text[0] != '~') {
			/* Fix non absolute paths */
			full_path = g_build_filename (loc_entry->details->current_directory, entry_text, NULL);
			gtk_entry_set_text (entry, full_path);
			g_free (full_path);
		}

		g_free (uri_scheme);
	}

	GTK_ENTRY_CLASS (nautilus_location_entry_parent_class)->activate (entry);
}

static void
nautilus_location_entry_cancel (NautilusLocationEntry *entry)
{
	nautilus_location_entry_set_location (entry, entry->details->last_location);
}

static void
nautilus_location_entry_class_init (NautilusLocationEntryClass *class)
{
	GtkWidgetClass *widget_class;
	GObjectClass *gobject_class;
	GtkEntryClass *entry_class;
	GtkBindingSet *binding_set;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->focus_in_event = nautilus_location_entry_focus_in;
	widget_class->destroy = destroy;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	entry_class = GTK_ENTRY_CLASS (class);
	entry_class->activate = nautilus_location_entry_activate;

	class->cancel = nautilus_location_entry_cancel;

	signals[CANCEL] = g_signal_new
		("cancel",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		 G_STRUCT_OFFSET (NautilusLocationEntryClass,
				  cancel),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);

	signals[LOCATION_CHANGED] = g_signal_new
		("location-changed",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST, 0,
		 NULL, NULL,
		 g_cclosure_marshal_generic,
		 G_TYPE_NONE, 1, G_TYPE_OBJECT);

	binding_set = gtk_binding_set_by_class (class);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "cancel", 0);

	g_type_class_add_private (class, sizeof (NautilusLocationEntryDetails));
}

void
nautilus_location_entry_set_secondary_action (NautilusLocationEntry *entry,
					      NautilusLocationEntryAction secondary_action)
{
	if (entry->details->secondary_action == secondary_action) {
		return;
	}

	switch (secondary_action) {
	case NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR:
		gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry), 
						   GTK_ENTRY_ICON_SECONDARY,
						   "edit-clear-symbolic");
		break;
	case NAUTILUS_LOCATION_ENTRY_ACTION_GOTO:
		gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
						   GTK_ENTRY_ICON_SECONDARY,
						   "go-next-symbolic");
		break;
	default:
		g_assert_not_reached ();
	}
	entry->details->secondary_action = secondary_action;
}

static void
editable_activate_callback (GtkEntry *entry,
			    gpointer user_data)
{
	NautilusLocationEntry *self = user_data;
	const char *entry_text;

	entry_text = gtk_entry_get_text (entry);
	if (entry_text != NULL && *entry_text != '\0') {
		emit_location_changed (self);
	}
}

static void
editable_changed_callback (GtkEntry *entry,
			   gpointer user_data)
{
	nautilus_location_entry_update_action (NAUTILUS_LOCATION_ENTRY (entry));
}

static void
nautilus_location_entry_init (NautilusLocationEntry *entry)
{
	GtkTargetList *targetlist;

	entry->details = G_TYPE_INSTANCE_GET_PRIVATE (entry, NAUTILUS_TYPE_LOCATION_ENTRY,
						      NautilusLocationEntryDetails);

	entry->details->completer = g_filename_completer_new ();
	g_filename_completer_set_dirs_only (entry->details->completer, TRUE);

	gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry), GTK_ENTRY_ICON_PRIMARY, "folder-symbolic");
	gtk_entry_set_icon_activatable (GTK_ENTRY (entry), GTK_ENTRY_ICON_PRIMARY, FALSE);
	targetlist = gtk_target_list_new (drag_types, G_N_ELEMENTS (drag_types));
	gtk_entry_set_icon_drag_source (GTK_ENTRY (entry), GTK_ENTRY_ICON_PRIMARY, targetlist, GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	gtk_target_list_unref (targetlist);

	nautilus_location_entry_set_secondary_action (entry,
						      NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR);

	nautilus_entry_set_special_tab_handling (NAUTILUS_ENTRY (entry), TRUE);

	g_signal_connect (entry, "event-after",
		          G_CALLBACK (editable_event_after_callback), entry);

	g_signal_connect (entry, "notify::text",
			  G_CALLBACK (nautilus_location_entry_text_changed), NULL);

	g_signal_connect (entry, "icon-release",
			  G_CALLBACK (nautilus_location_entry_icon_release), NULL);

	g_signal_connect (entry->details->completer, "got-completion-data",
		          G_CALLBACK (got_completion_data_callback), entry);

	/* Drag source */
	g_signal_connect_object (entry, "drag-data-get",
				 G_CALLBACK (drag_data_get_callback), entry, 0);

	/* Drag dest. */
	gtk_drag_dest_set (GTK_WIDGET (entry),
			   GTK_DEST_DEFAULT_ALL,
			   drop_types, G_N_ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	g_signal_connect (entry, "drag-data-received",
			  G_CALLBACK (drag_data_received_callback), NULL);

	g_signal_connect_object (entry, "activate",
				 G_CALLBACK (editable_activate_callback), entry, G_CONNECT_AFTER);
	g_signal_connect_object (entry, "changed",
				 G_CALLBACK (editable_changed_callback), entry, 0);
}

GtkWidget *
nautilus_location_entry_new (void)
{
	GtkWidget *entry;

	entry = gtk_widget_new (NAUTILUS_TYPE_LOCATION_ENTRY, "max-width-chars", 350, NULL);

	return entry;
}

void
nautilus_location_entry_set_special_text (NautilusLocationEntry *entry,
					  const char            *special_text)
{
	entry->details->has_special_text = TRUE;
	
	g_free (entry->details->special_text);
	entry->details->special_text = g_strdup (special_text);

	entry->details->setting_special_text = TRUE;
	gtk_entry_set_text (GTK_ENTRY (entry), special_text);
	entry->details->setting_special_text = FALSE;
}


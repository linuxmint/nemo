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
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
#include "nautilus-location-bar.h"

#include "nautilus-application.h"
#include "nautilus-location-entry.h"
#include "nautilus-window.h"
#include <eel/eel-accessibility.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libnautilus-private/nautilus-icon-dnd.h>
#include <libnautilus-private/nautilus-clipboard.h>
#include <stdio.h>
#include <string.h>

#define NAUTILUS_DND_URI_LIST_TYPE 	  "text/uri-list"
#define NAUTILUS_DND_TEXT_PLAIN_TYPE 	  "text/plain"

static const char untranslated_location_label[] = N_("Location:");
static const char untranslated_go_to_label[] = N_("Go To:");
#define LOCATION_LABEL _(untranslated_location_label)
#define GO_TO_LABEL _(untranslated_go_to_label)

struct NautilusLocationBarDetails {
	GtkLabel *label;
	NautilusEntry *entry;
	
	char *last_location;
	
	guint idle_id;
};

enum {
	NAUTILUS_DND_MC_DESKTOP_ICON,
	NAUTILUS_DND_URI_LIST,
	NAUTILUS_DND_TEXT_PLAIN,
	NAUTILUS_DND_NTARGETS
};

enum {
	CANCEL,
	LOCATION_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static const GtkTargetEntry drag_types [] = {
	{ NAUTILUS_DND_URI_LIST_TYPE,   0, NAUTILUS_DND_URI_LIST },
	{ NAUTILUS_DND_TEXT_PLAIN_TYPE, 0, NAUTILUS_DND_TEXT_PLAIN },
};

static const GtkTargetEntry drop_types [] = {
	{ NAUTILUS_DND_URI_LIST_TYPE,   0, NAUTILUS_DND_URI_LIST },
	{ NAUTILUS_DND_TEXT_PLAIN_TYPE, 0, NAUTILUS_DND_TEXT_PLAIN },
};

G_DEFINE_TYPE (NautilusLocationBar, nautilus_location_bar,
	       GTK_TYPE_BOX);

static NautilusWindow *
nautilus_location_bar_get_window (GtkWidget *bar)
{
	return NAUTILUS_WINDOW (gtk_widget_get_ancestor (bar, NAUTILUS_TYPE_WINDOW));
}

/**
 * nautilus_location_bar_get_location
 *
 * Get the "URI" represented by the text in the location bar.
 *
 * @bar: A NautilusLocationBar.
 *
 * returns a newly allocated "string" containing the mangled
 * (by g_file_parse_name) text that the user typed in...maybe a URI 
 * but not guaranteed.
 *
 **/
static char *
nautilus_location_bar_get_location (NautilusLocationBar *bar) 
{
	char *user_location, *uri;
	GFile *location;
	
	user_location = gtk_editable_get_chars (GTK_EDITABLE (bar->details->entry), 0, -1);
	location = g_file_parse_name (user_location);
	g_free (user_location);
	uri = g_file_get_uri (location);
	g_object_unref (location);
	return uri;
}

static void
emit_location_changed (NautilusLocationBar *bar)
{
	char *location;

	location = nautilus_location_bar_get_location (bar);
	g_signal_emit (bar,
		       signals[LOCATION_CHANGED], 0,
		       location);
	g_free (location);
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
	NautilusWindow *new_window, *window;
	GdkScreen      *screen;
	gboolean new_windows_for_extras;
	char *prompt;
	char *detail;
	GFile *location;
	NautilusLocationBar *self = NAUTILUS_LOCATION_BAR (widget);

	g_assert (data != NULL);
	g_assert (callback_data == NULL);

	names = g_uri_list_extract_uris (gtk_selection_data_get_data (data));

	if (names == NULL || *names == NULL) {
		g_warning ("No D&D URI's");
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	window = nautilus_location_bar_get_window (widget);
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
		new_windows_for_extras = eel_run_simple_dialog 
			(GTK_WIDGET (window),
			 TRUE,
			 GTK_MESSAGE_QUESTION,
			 prompt,
			 detail,
			 GTK_STOCK_CANCEL, GTK_STOCK_OK,
			 NULL) != 0 /* GNOME_OK */;

		g_free (prompt);
		g_free (detail);
		
		if (!new_windows_for_extras) {
			gtk_drag_finish (context, FALSE, FALSE, time);
			return;
		}
	}

	nautilus_location_bar_set_location (self, names[0]);	
	emit_location_changed (self);

	if (new_windows_for_extras) {
		int i;

		application = nautilus_application_get_singleton ();
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
	NautilusLocationBar *self;
	char *entry_text;

	g_assert (selection_data != NULL);
	self = callback_data;

	entry_text = nautilus_location_bar_get_location (self);

	switch (info) {
	case NAUTILUS_DND_URI_LIST:
	case NAUTILUS_DND_TEXT_PLAIN:
		gtk_selection_data_set (selection_data,
					gtk_selection_data_get_target (selection_data),
					8, (guchar *) entry_text,
					strlen (entry_text));
		break;
	default:
		g_assert_not_reached ();
	}
	g_free (entry_text);
}

/* routine that determines the usize for the label widget as larger
   then the size of the largest string and then sets it to that so
   that we don't have localization problems. see
   gtk_label_finalize_lines in gtklabel.c (line 618) for the code that
   we are imitating here. */

static void
style_set_handler (GtkWidget *widget, GtkStyle *previous_style)
{
	PangoLayout *layout;
	int width, width2;
	int xpad;

	layout = gtk_label_get_layout (GTK_LABEL(widget));

	layout = pango_layout_copy (layout);

	pango_layout_set_text (layout, LOCATION_LABEL, -1);
	pango_layout_get_pixel_size (layout, &width, NULL);
	
	pango_layout_set_text (layout, GO_TO_LABEL, -1);
	pango_layout_get_pixel_size (layout, &width2, NULL);
	width = MAX (width, width2);

	gtk_misc_get_padding (GTK_MISC (widget),
			      &xpad, NULL);

	width += 2 * xpad;

	gtk_widget_set_size_request (widget, width, -1);

	g_object_unref (layout);
}

static gboolean
label_button_pressed_callback (GtkWidget             *widget,
			       GdkEventButton        *event)
{
	NautilusWindow *window;
	NautilusWindowSlot *slot;
	NautilusView *view;
	GtkWidget *label;

	if (event->button != 3) {
		return FALSE;
	}

	window = nautilus_location_bar_get_window (gtk_widget_get_parent (widget));
	slot = nautilus_window_get_active_slot (window);
	view = slot->content_view;
	label = gtk_bin_get_child (GTK_BIN (widget));
	/* only pop-up if the URI in the entry matches the displayed location */
	if (view == NULL ||
	    strcmp (gtk_label_get_text (GTK_LABEL (label)), LOCATION_LABEL)) {
		return FALSE;
	}

	nautilus_view_pop_up_location_context_menu (view, event, NULL);

	return FALSE;
}

static void
editable_activate_callback (GtkEntry *entry,
			    gpointer user_data)
{
	NautilusLocationBar *self = user_data;
	const char *entry_text;

	entry_text = gtk_entry_get_text (entry);
	if (entry_text != NULL && *entry_text != '\0') {
		emit_location_changed (self);
	}
}

/**
 * nautilus_location_bar_update_label
 *
 * if the text in the entry matches the uri, set the label to "location", otherwise use "goto"
 *
 **/
static void
nautilus_location_bar_update_label (NautilusLocationBar *bar)
{
	const char *current_text;
	GFile *location;
	GFile *last_location;
	
	if (bar->details->last_location == NULL){
		gtk_label_set_text (GTK_LABEL (bar->details->label), GO_TO_LABEL);
		nautilus_location_entry_set_secondary_action (NAUTILUS_LOCATION_ENTRY (bar->details->entry), 
							      NAUTILUS_LOCATION_ENTRY_ACTION_GOTO);
		return;
	}

	current_text = gtk_entry_get_text (GTK_ENTRY (bar->details->entry));
	location = g_file_parse_name (current_text);
	last_location = g_file_parse_name (bar->details->last_location);
	
	if (g_file_equal (last_location, location)) {
		gtk_label_set_text (GTK_LABEL (bar->details->label), LOCATION_LABEL);
		nautilus_location_entry_set_secondary_action (NAUTILUS_LOCATION_ENTRY (bar->details->entry), 
							      NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR);
	} else {		 
		gtk_label_set_text (GTK_LABEL (bar->details->label), GO_TO_LABEL);
		nautilus_location_entry_set_secondary_action (NAUTILUS_LOCATION_ENTRY (bar->details->entry), 
							      NAUTILUS_LOCATION_ENTRY_ACTION_GOTO);
	}

	g_object_unref (location);
	g_object_unref (last_location);
}

static void
editable_changed_callback (GtkEntry *entry,
			   gpointer user_data)
{
	nautilus_location_bar_update_label (NAUTILUS_LOCATION_BAR (user_data));
}

void
nautilus_location_bar_activate (NautilusLocationBar *bar)
{
	/* Put the keyboard focus in the text field when switching to this mode,
	 * and select all text for easy overtyping 
	 */
	gtk_widget_grab_focus (GTK_WIDGET (bar->details->entry));
	nautilus_entry_select_all (bar->details->entry);
}

static void
nautilus_location_bar_cancel (NautilusLocationBar *bar)
{
	char *last_location;

	last_location = bar->details->last_location;
	nautilus_location_bar_set_location (bar, last_location);
}

static void
finalize (GObject *object)
{
	NautilusLocationBar *bar;

	bar = NAUTILUS_LOCATION_BAR (object);

	/* cancel the pending idle call, if any */
	if (bar->details->idle_id != 0) {
		g_source_remove (bar->details->idle_id);
		bar->details->idle_id = 0;
	}
	
	g_free (bar->details->last_location);
	bar->details->last_location = NULL;

	G_OBJECT_CLASS (nautilus_location_bar_parent_class)->finalize (object);
}

static void
nautilus_location_bar_class_init (NautilusLocationBarClass *klass)
{
	GObjectClass *gobject_class;
	GtkBindingSet *binding_set;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = finalize;

	klass->cancel = nautilus_location_bar_cancel;

	signals[CANCEL] = g_signal_new
		("cancel",
		 G_TYPE_FROM_CLASS (klass),
		 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		 G_STRUCT_OFFSET (NautilusLocationBarClass,
				  cancel),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);

	signals[LOCATION_CHANGED] = g_signal_new
		("location-changed",
		 G_TYPE_FROM_CLASS (klass),
		 G_SIGNAL_RUN_LAST, 0,
		 NULL, NULL,
		 g_cclosure_marshal_VOID__STRING,
		 G_TYPE_NONE, 1, G_TYPE_STRING);

	binding_set = gtk_binding_set_by_class (klass);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "cancel", 0);

	g_type_class_add_private (klass, sizeof (NautilusLocationBarDetails));
}

static void
nautilus_location_bar_init (NautilusLocationBar *bar)
{
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *event_box;

	bar->details = G_TYPE_INSTANCE_GET_PRIVATE (bar, NAUTILUS_TYPE_LOCATION_BAR,
						    NautilusLocationBarDetails);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (bar),
					GTK_ORIENTATION_HORIZONTAL);

	event_box = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box), FALSE);
	
	gtk_container_set_border_width (GTK_CONTAINER (event_box), 4);
	label = gtk_label_new (LOCATION_LABEL);
	gtk_container_add   (GTK_CONTAINER (event_box), label);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
	g_signal_connect (label, "style_set", 
			  G_CALLBACK (style_set_handler), NULL);

	gtk_box_pack_start (GTK_BOX (bar), event_box, FALSE, TRUE, 4);

	entry = nautilus_location_entry_new ();
	
	g_signal_connect_object (entry, "activate",
				 G_CALLBACK (editable_activate_callback), bar, G_CONNECT_AFTER);
	g_signal_connect_object (entry, "changed",
				 G_CALLBACK (editable_changed_callback), bar, 0);

	gtk_box_pack_start (GTK_BOX (bar), entry, TRUE, TRUE, 0);

	eel_accessibility_set_up_label_widget_relation (label, entry);


	/* Label context menu */
	g_signal_connect (event_box, "button-press-event",
			  G_CALLBACK (label_button_pressed_callback), NULL);

	/* Drag source */
	gtk_drag_source_set (GTK_WIDGET (event_box), 
			     GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
			     drag_types, G_N_ELEMENTS (drag_types),
			     GDK_ACTION_COPY | GDK_ACTION_LINK);
	g_signal_connect_object (event_box, "drag_data_get",
				 G_CALLBACK (drag_data_get_callback), bar, 0);

	/* Drag dest. */
	gtk_drag_dest_set (GTK_WIDGET (bar),
			   GTK_DEST_DEFAULT_ALL,
			   drop_types, G_N_ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	g_signal_connect (bar, "drag_data_received",
			  G_CALLBACK (drag_data_received_callback), NULL);

	bar->details->label = GTK_LABEL (label);
	bar->details->entry = NAUTILUS_ENTRY (entry);	

	gtk_widget_show_all (GTK_WIDGET (bar));
}

GtkWidget *
nautilus_location_bar_new (void)
{
	GtkWidget *bar;

	bar = gtk_widget_new (NAUTILUS_TYPE_LOCATION_BAR, NULL);

	return bar;
}

void
nautilus_location_bar_set_location (NautilusLocationBar *bar,
				    const char *location)
{
	char *formatted_location;
	GFile *file;

	g_assert (location != NULL);

	/* Note: This is called in reaction to external changes, and 
	 * thus should not emit the LOCATION_CHANGED signal. */

	if (eel_uri_is_search (location)) {
		nautilus_location_entry_set_special_text (NAUTILUS_LOCATION_ENTRY (bar->details->entry),
							  "");
	} else {
		file = g_file_new_for_uri (location);
		formatted_location = g_file_get_parse_name (file);
		g_object_unref (file);
		nautilus_location_entry_update_current_location (NAUTILUS_LOCATION_ENTRY (bar->details->entry),
								 formatted_location);
		g_free (formatted_location);
	}

	/* remember the original location for later comparison */
	
	if (bar->details->last_location != location) {
		g_free (bar->details->last_location);
		bar->details->last_location = g_strdup (location);
	}

	nautilus_location_bar_update_label (bar);
}

NautilusEntry *
nautilus_location_bar_get_entry (NautilusLocationBar *location_bar)
{
	return location_bar->details->entry;
}

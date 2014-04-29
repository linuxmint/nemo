/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nautilus-application: main Nautilus application class.
 *
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2010, 2014 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "nautilus-application-actions.h"

#include "nautilus-desktop-window.h"
#include "nautilus-file-management-properties.h"

#include <glib/gi18n.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_APPLICATION
#include <libnautilus-private/nautilus-debug.h>

static void
action_new_window (GSimpleAction *action,
		   GVariant *parameter,
		   gpointer user_data)
{
	GtkApplication *application = user_data;
	NautilusWindow *window;
	GtkWindow *cur_window;

	cur_window = gtk_application_get_active_window (application);
	window = nautilus_application_create_window (NAUTILUS_APPLICATION (application),
						     cur_window ?
						     gtk_window_get_screen (cur_window) :
						     gdk_screen_get_default ());

	nautilus_window_slot_go_home (nautilus_window_get_active_slot (window), 0);
}

static void
action_connect_to_server (GSimpleAction *action,
			  GVariant *parameter,
			  gpointer user_data)
{
	GtkApplication *application = user_data;

	nautilus_application_connect_server (NAUTILUS_APPLICATION (application),
					     NAUTILUS_WINDOW (gtk_application_get_active_window (application)));
}

static void
action_enter_location (GSimpleAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	GtkApplication *application = user_data;
	NautilusWindow *window;
	GtkWindow *cur_window;

	cur_window = gtk_application_get_active_window (application);
	window = NAUTILUS_WINDOW (cur_window);
	nautilus_window_ensure_location_entry (window);
}

static void
action_bookmarks (GSimpleAction *action,
		  GVariant *parameter,
		  gpointer user_data)
{
	GtkApplication *application = user_data;

	nautilus_application_edit_bookmarks (NAUTILUS_APPLICATION (application),
					     NAUTILUS_WINDOW (gtk_application_get_active_window (application)));
}

static void
action_preferences (GSimpleAction *action,
		    GVariant *parameter,
		    gpointer user_data)
{
	GtkApplication *application = user_data;
	nautilus_file_management_properties_dialog_show (gtk_application_get_active_window (application));
}

static void
action_about (GSimpleAction *action,
	      GVariant *parameter,
	      gpointer user_data)
{
	GtkApplication *application = user_data;

	nautilus_window_show_about_dialog (NAUTILUS_WINDOW (gtk_application_get_active_window (application)));
}

static void
action_help (GSimpleAction *action,
	     GVariant *parameter,
	     gpointer user_data)
{
	GtkWindow *window;
	GtkWidget *dialog;
	GtkApplication *application = user_data;
	GError *error = NULL;

	window = gtk_application_get_active_window (application);
	gtk_show_uri (window ? 
		      gtk_window_get_screen (GTK_WINDOW (window)) :
		      gdk_screen_get_default (),
		      "help:gnome-help/files",
		      gtk_get_current_event_time (), &error);

	if (error) {
		dialog = gtk_message_dialog_new (window ? GTK_WINDOW (window) : NULL,
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("There was an error displaying help: \n%s"),
						 error->message);
		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}

static void
action_open_desktop (GSimpleAction *action,
                     GVariant *parameter,
                     gpointer user_data)
{
	nautilus_desktop_window_ensure ();
}

static void
action_close_desktop (GSimpleAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	GtkWidget *desktop_window;

	desktop_window = nautilus_desktop_window_get ();
	if (desktop_window != NULL) {
		gtk_widget_destroy (desktop_window);
	}
}

static void
action_kill (GSimpleAction *action,
	     GVariant *parameter,
	     gpointer user_data)
{
	GtkApplication *application = user_data;
	GList *windows;

	/* this will also destroy the desktop windows */
	windows = gtk_application_get_windows (application);
	g_list_foreach (windows, (GFunc) gtk_widget_destroy, NULL);

	/* we have been asked to force quit */
	g_application_quit (G_APPLICATION (application));
}

static void
action_quit (GSimpleAction *action,
	     GVariant *parameter,
	     gpointer user_data)
{
	GtkApplication *application = user_data;
	GList *l;

	/* nautilus_window_close() doesn't do anything for desktop windows */
	for (l = gtk_application_get_windows (GTK_APPLICATION (application)); l; l = l->next) {
		if (NAUTILUS_IS_WINDOW (l->data))
			nautilus_window_close (NAUTILUS_WINDOW (l->data));
	}
}

static void
action_search (GSimpleAction *action,
	       GVariant *parameter,
	       gpointer user_data)
{
	GtkApplication *application = user_data;
	const gchar *string, *uri;
	NautilusQuery *query;
	NautilusDirectory *directory;
	gchar *search_uri;
	NautilusWindow *window;
	GtkWindow *cur_window;
	GFile *location;

	g_variant_get (parameter, "(ss)", &uri, &string);

	if (strlen (string) == 0 ||
	    strlen (uri) == 0) {
		return;
	}

	query = nautilus_query_new ();
	nautilus_query_set_location (query, uri);
	nautilus_query_set_text (query, string);

	search_uri = nautilus_search_directory_generate_new_uri ();
	location = g_file_new_for_uri (search_uri);
	g_free (search_uri);

	directory = nautilus_directory_get (location);
	nautilus_search_directory_set_query (NAUTILUS_SEARCH_DIRECTORY (directory), query);

	cur_window = gtk_application_get_active_window (application);
	window = nautilus_application_create_window (NAUTILUS_APPLICATION (application),
						     cur_window ?
						     gtk_window_get_screen (cur_window) :
						     gdk_screen_get_default ());

	nautilus_window_slot_open_location (nautilus_window_get_active_slot (window), location, 0);

	nautilus_directory_unref (directory);
	g_object_unref (query);
	g_object_unref (location);
}

static GActionEntry app_entries[] = {
	{ "new-window", action_new_window, NULL, NULL, NULL },
	{ "connect-to-server", action_connect_to_server, NULL, NULL, NULL },
	{ "enter-location", action_enter_location, NULL, NULL, NULL },
	{ "bookmarks", action_bookmarks, NULL, NULL, NULL },
	{ "preferences", action_preferences, NULL, NULL, NULL },
	{ "search", action_search, "(ss)", NULL, NULL },
	{ "about", action_about, NULL, NULL, NULL },
	{ "help", action_help, NULL, NULL, NULL },
	{ "quit", action_quit, NULL, NULL, NULL },
	{ "kill", action_kill, NULL, NULL, NULL },
	{ "open-desktop", action_open_desktop, NULL, NULL, NULL },
	{ "close-desktop", action_close_desktop, NULL, NULL, NULL },
};

void
nautilus_init_application_actions (NautilusApplication *app)
{
	GtkBuilder *builder;
	GError *error = NULL;
	const gchar *debug_no_app_menu;

	g_action_map_add_action_entries (G_ACTION_MAP (app),
					 app_entries, G_N_ELEMENTS (app_entries),
					 app);
	gtk_application_add_accelerator (GTK_APPLICATION (app), "F10", "win.gear-menu", NULL);

	builder = gtk_builder_new ();
	gtk_builder_add_from_resource (builder, "/org/gnome/nautilus/nautilus-app-menu.ui", &error);

	if (error == NULL) {
		gtk_application_set_app_menu (GTK_APPLICATION (app),
					      G_MENU_MODEL (gtk_builder_get_object (builder, "app-menu")));
	} else {
		g_critical ("Unable to add the application menu: %s\n", error->message);
		g_error_free (error);
	}

	g_object_unref (builder);

	debug_no_app_menu = g_getenv ("NAUTILUS_DEBUG_NO_APP_MENU");
	if (debug_no_app_menu) {
		DEBUG ("Disabling app menu GtkSetting as requested...");
		g_object_set (gtk_settings_get_default (),
			      "gtk-shell-shows-app-menu", FALSE,
			      NULL);
	}
}

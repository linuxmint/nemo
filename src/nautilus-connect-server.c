/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * Copyright (C) 2005 Vincent Untz
 * Copyright (C) 2012 Red Hat, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "nautilus-connect-server-dialog.h"
#include <eel/eel-stock-dialogs.h>

static gboolean just_print_uri = FALSE;

static void
mount_ready_callback (GObject *source_object,
		      GAsyncResult *res,
		      gpointer user_data)
{
	GError *error;
	GFile *location;
	gboolean show = TRUE;
	GtkWidget *dialog = user_data;

	location = G_FILE (source_object);

	error = NULL;
	if (!g_file_mount_enclosing_volume_finish (location, res, &error)) {
		show = FALSE;
		if (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_ALREADY_MOUNTED) {
			show = TRUE;
		} else if (error->domain != G_IO_ERROR ||
			   (error->code != G_IO_ERROR_CANCELLED &&
			    error->code != G_IO_ERROR_FAILED_HANDLED)) {
			/* if it wasn't cancelled show a dialog */
			eel_show_error_dialog (_("Unable to access location"), error->message, GTK_WINDOW (dialog));
		}
		g_clear_error (&error);
	}

	if (show) {
		char *uri;
		uri = g_file_get_uri (location);
		if (just_print_uri) {
			g_print ("%s\n", uri);
		} else {
			GdkAppLaunchContext *launch_context;

			launch_context = gdk_display_get_app_launch_context (gtk_widget_get_display (dialog));
			gdk_app_launch_context_set_screen (launch_context,
							   gtk_widget_get_screen (dialog));
			error = NULL;
			g_app_info_launch_default_for_uri (uri,
							   G_APP_LAUNCH_CONTEXT (launch_context),
							   &error);
			if (error != NULL) {
				eel_show_error_dialog (_("Unable to display location"), error->message, GTK_WINDOW (dialog));
				g_clear_error (&error);
			}
			g_object_unref (launch_context);
		}
		g_free (uri);
	}
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
mount_location (GtkWidget *dialog,
		GFile     *location)
{
	GMountOperation *mount_op;

	mount_op = gtk_mount_operation_new (GTK_WINDOW (dialog));
	g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
	g_file_mount_enclosing_volume (location,
				       0,
				       mount_op,
				       NULL,
				       mount_ready_callback,
				       dialog);
	/* unref mount_op here - g_file_mount_enclosing_volume() does ref for itself */
	g_object_unref (mount_op);
}

static void
on_connect_server_destroy (GtkWidget *widget,
			   gpointer   user_data)
{
	/* this only happens when user clicks "cancel"
	 * on the main dialog or when we are all done.
	 */
	gtk_main_quit ();
}

static void
on_connect_server_response (GtkDialog      *dialog,
			    int             response,
			    gpointer        user_data)
{
	if (response == GTK_RESPONSE_OK) {
		GFile *location;

		location = nautilus_connect_server_dialog_get_location (NAUTILUS_CONNECT_SERVER_DIALOG (dialog));
		if (location != NULL) {
			mount_location (GTK_WIDGET (dialog), location);
			g_object_unref (location);
		} else {
			g_warning ("Unable to get remote server location");
			gtk_widget_destroy (GTK_WIDGET (dialog));
		}
	} else {
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
}

int
main (int argc, char *argv[])
{
	GtkWidget *dialog;
	GOptionContext *context;
	GError *error;
	const GOptionEntry options[] = {
		{ "print-uri", 0, 0, G_OPTION_ARG_NONE, &just_print_uri, N_("Print but do not open the URI"), NULL },
		{ NULL }
	};

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_set_prgname ("nautilus-connect-server");

	/* Translators: This is the --help description for the connect to server app,
	   the initial newlines are between the command line arg and the description */
	context = g_option_context_new (N_("\n\nAdd connect to server mount"));
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));

	error = NULL;
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_critical ("Failed to parse arguments: %s", error->message);
		g_error_free (error);
		g_option_context_free (context);
		exit (1);
	}

	g_option_context_free (context);

	dialog = nautilus_connect_server_dialog_new (NULL);

	gtk_window_set_default_icon_name ("folder-remote-symbolic");

	g_signal_connect (dialog, "response",
			  G_CALLBACK (on_connect_server_response),
			  NULL);
	g_signal_connect (dialog, "destroy",
			  G_CALLBACK (on_connect_server_destroy),
			  NULL);

	gtk_widget_show (dialog);

	gtk_main ();

	return 0;
}

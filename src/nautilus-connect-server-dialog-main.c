/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-connect-server-main.c - Start the "Connect to Server" dialog.
 * Nautilus
 *
 * Copyright (C) 2005 Vincent Untz
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
 * Authors:
 *   Vincent Untz <vincent@vuntz.net>
 *   Cosimo Cecchi <cosimoc@gnome.org>
 */

#include <config.h>

#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <stdlib.h>

#include <eel/eel-stock-dialogs.h>

#include <libnautilus-private/nautilus-icon-names.h>
#include <libnautilus-private/nautilus-global-preferences.h>

#include "nautilus-connect-server-dialog.h"

static GSimpleAsyncResult *display_location_res = NULL;
static gboolean just_print_uri = FALSE;

static void
main_dialog_destroyed (GtkWidget *widget,
		       gpointer   user_data)
{
	/* this only happens when user clicks "cancel"
	 * on the main dialog or when we are all done.
	 */
	gtk_main_quit ();
}

gboolean
nautilus_connect_server_dialog_display_location_finish (NautilusConnectServerDialog *self,
							GAsyncResult *res,
							GError **error)
{
	if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (res), error)) {
		return FALSE;
	}

	return TRUE;
}

void
nautilus_connect_server_dialog_display_location_async (NautilusConnectServerDialog *self,
						       GFile *location,
						       GAsyncReadyCallback callback,
						       gpointer user_data)
{
	GError *error;
	GdkAppLaunchContext *launch_context;
	gchar *uri;

	display_location_res = g_simple_async_result_new (G_OBJECT (self),
							  callback, user_data,
							  nautilus_connect_server_dialog_display_location_async);

	error = NULL;
	uri = g_file_get_uri (location);

	if (just_print_uri) {
		g_print ("%s\n", uri);
	}
	else {
		launch_context = gdk_display_get_app_launch_context (gtk_widget_get_display (GTK_WIDGET (self)));
		gdk_app_launch_context_set_screen (launch_context,
						   gtk_widget_get_screen (GTK_WIDGET (self)));

		g_app_info_launch_default_for_uri (uri,
						   G_APP_LAUNCH_CONTEXT (launch_context),
						   &error);

		g_object_unref (launch_context);
	}

	if (error != NULL) {
		g_simple_async_result_set_from_error (display_location_res, error);
		g_error_free (error);
	}

	g_simple_async_result_complete_in_idle (display_location_res);

	g_free (uri);
	g_object_unref (display_location_res);
	display_location_res = NULL;
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

	error = NULL;
	/* Translators: This is the --help description for the connect to server app,
	   the initial newlines are between the command line arg and the description */
	context = g_option_context_new (N_("\n\nAdd connect to server mount"));
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_critical ("Failed to parse arguments: %s", error->message);
		g_error_free (error);
		g_option_context_free (context);
		exit (1);
	}

	g_option_context_free (context);

	nautilus_global_preferences_init ();

	gtk_window_set_default_icon_name (NAUTILUS_ICON_FOLDER);

	dialog = nautilus_connect_server_dialog_new (NULL);

	g_signal_connect (dialog, "destroy",
			  G_CALLBACK (main_dialog_destroyed), NULL);

	gtk_widget_show (dialog);

	gtk_main ();

	return 0;
}

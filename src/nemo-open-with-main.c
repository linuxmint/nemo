/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-connect-server-main.c - Start the "Connect to Server" dialog.
 * Nemo
 *
 * Copyright (C) 2005 Vincent Untz
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
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

#include <libnemo-private/nemo-icon-names.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-mime-application-chooser.h>
#include <libnemo-private/nemo-program-choosing.h>

static void
main_dialog_destroyed (GtkWidget *widget,
		       gpointer   user_data)
{
	/* this only happens when user clicks "cancel"
	 * on the main dialog or when we are all done.
	 */
	gtk_main_quit ();
}

static void
app_chooser_dialog_response_cb (GtkDialog *dialog,
                gint response_id,
                gpointer user_data)
{

    NemoFile *file;
    GAppInfo *info;
    GList files;

    if (response_id != GTK_RESPONSE_OK) {
        gtk_widget_destroy (GTK_WIDGET (dialog));
        return;
    }

    NemoMimeApplicationChooser *chooser = NEMO_MIME_APPLICATION_CHOOSER (user_data);

    info = nemo_mime_application_chooser_get_info (chooser);
    file = nemo_file_get_by_uri (nemo_mime_application_chooser_get_uri (chooser));

    files.next = NULL;
    files.prev = NULL;
    files.data = file;
    nemo_launch_application (info, &files, NULL);

    gtk_widget_destroy (GTK_WIDGET (dialog));
    g_object_unref (info);
}

int
main (int argc, char *argv[])
{
	GtkWidget *dialog;
	GOptionContext *context;
	GError *error;
	const GOptionEntry options[] = {
		{ NULL }
	};

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	error = NULL;
	/* Translators: This is the --help description for the connect to server app,
	   the initial newlines are between the command line arg and the description */
	context = g_option_context_new (N_("\n\nShow an open-with dialog given a uri, "
                                       "to allow the user to change the default mimetype handler."));
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

    if (argc != 2) {
        g_critical ("uri required");
        exit(1);
    }

	nemo_global_preferences_init ();

    const gchar *uri, *mime_type;

    uri = argv[1];

    GFile *file = g_file_new_for_uri (uri);

    GFileInfo *info = g_file_query_info (file,
                                         G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                         G_FILE_QUERY_INFO_NONE,
                                         NULL, NULL);

    if (info == NULL) {
        g_critical ("Unable to query mimetype from given uri");
        g_object_unref (info);
        g_object_unref (file);
        exit(1);
    }

    mime_type = g_file_info_get_content_type (info);

    dialog = gtk_dialog_new_with_buttons (_("Open with"),
                                          NULL,
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          GTK_STOCK_CANCEL,
                                          GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OK,
                                          GTK_RESPONSE_OK,
                                          NULL);

    GtkWidget *chooser = nemo_mime_application_chooser_new (uri, NULL, mime_type);

    GtkWidget *content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    gtk_box_pack_start (GTK_BOX (content), chooser, TRUE, TRUE, 0);

    gtk_widget_show_all (dialog);

    g_signal_connect_object (dialog, "response", 
                             G_CALLBACK (app_chooser_dialog_response_cb),
                             chooser, 0);

	g_signal_connect (dialog, "destroy",
                      G_CALLBACK (main_dialog_destroyed), NULL);

	gtk_widget_show (dialog);

	gtk_main ();

	return 0;
}

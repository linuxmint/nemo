/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nemo

   Copyright (C) 2008 Red Hat, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Author: David Zeuthen <davidz@redhat.com>
*/


#include <config.h>

#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <glib/gi18n.h>

#include <libnemo-private/nemo-module.h>
#include <libnemo-private/nemo-icon-info.h>

typedef struct
{
	GtkWidget *dialog;
	GMount *mount;
} AutorunSoftwareDialogData;

static void autorun_software_dialog_mount_unmounted (GMount *mount, AutorunSoftwareDialogData *data);

static void
autorun_software_dialog_destroy (AutorunSoftwareDialogData *data)
{
	g_signal_handlers_disconnect_by_func (G_OBJECT (data->mount),
					      G_CALLBACK (autorun_software_dialog_mount_unmounted),
					      data);

	gtk_widget_destroy (GTK_WIDGET (data->dialog));
	g_object_unref (data->mount);
	g_free (data);
}

static void 
autorun_software_dialog_mount_unmounted (GMount *mount, AutorunSoftwareDialogData *data)
{
	autorun_software_dialog_destroy (data);
}

static gboolean
_check_file (GFile *mount_root, const char *file_path, gboolean must_be_executable)
{
	GFile *file;
	GFileInfo *file_info;
	gboolean ret;

	ret = FALSE;

	file = g_file_get_child (mount_root, file_path);
	file_info = g_file_query_info (file,
				       G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE,
				       G_FILE_QUERY_INFO_NONE,
				       NULL,
				       NULL);
	if (file_info != NULL) {
		if (must_be_executable) {
			if (g_file_info_get_attribute_boolean (file_info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE)) {
				ret = TRUE;
			}
		} else {
			ret = TRUE;
		}
		g_object_unref (file_info);
	}
	g_object_unref (file);

	return ret;
}

static void
autorun (GMount *mount)
{
	char *error_string;
        GFile *root;
        GFile *program_to_spawn;
        GFile *program_parameter_file;
        char *path_to_spawn;
        char *cwd_for_program;
        char *program_parameter;

        root = g_mount_get_root (mount);

        /* Careful here, according to 
         *
         *  http://standards.freedesktop.org/autostart-spec/autostart-spec-latest.html
         *
         * the ordering does matter.
         */

        program_to_spawn = NULL;
        path_to_spawn = NULL;
        program_parameter_file = NULL;
        program_parameter = NULL;

	if (_check_file (root, ".autorun", TRUE)) {
                program_to_spawn = g_file_get_child (root, ".autorun");
        } else if (_check_file (root, "autorun", TRUE)) {
                program_to_spawn = g_file_get_child (root, "autorun");
        } else if (_check_file (root, "autorun.sh", TRUE)) {
                program_to_spawn = g_file_new_for_path ("/bin/sh");
                program_parameter_file = g_file_get_child (root, "autorun.sh");
        }

        if (program_to_spawn != NULL) {
                path_to_spawn = g_file_get_path (program_to_spawn);
	}
        if (program_parameter_file != NULL) {
                program_parameter = g_file_get_path (program_parameter_file);
        }

        cwd_for_program = g_file_get_path (root);

	error_string = NULL;
        if (path_to_spawn != NULL && cwd_for_program != NULL) {
                if (chdir (cwd_for_program) == 0)  {
                        execl (path_to_spawn, path_to_spawn, program_parameter, NULL);
			error_string = g_strdup_printf (_("Error starting autorun program: %s"), strerror (errno));
			goto out;
                }
                error_string = g_strdup_printf (_("Error starting autorun program: %s"), strerror (errno));
		goto out;
        }
	error_string = g_strdup_printf (_("Cannot find the autorun program"));

out:
        if (program_to_spawn != NULL) {
                g_object_unref (program_to_spawn);
	}
        if(program_parameter_file != NULL) {
                g_object_unref (program_parameter_file);
        }
	if (root != NULL) {
		g_object_unref (root);
	}
        g_free (path_to_spawn);
        g_free (cwd_for_program);
        g_free (program_parameter);

	if (error_string != NULL) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new_with_markup (NULL, /* TODO: parent window? */
							     0,
							     GTK_MESSAGE_ERROR,
							     GTK_BUTTONS_OK,
							     _("<big><b>Error autorunning software</b></big>"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error_string);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		g_free (error_string);
	}
}

static void
present_autorun_for_software_dialog (GMount *mount)
{
	GIcon *icon;
	int icon_size;
	NemoIconInfo *icon_info;
	GdkPixbuf *pixbuf;
	GtkWidget *image;
	char *mount_name;
	GtkWidget *dialog;
	AutorunSoftwareDialogData *data;

	mount_name = g_mount_get_name (mount);

	dialog = gtk_message_dialog_new_with_markup (NULL, /* TODO: parent window? */
						     0,
						     GTK_MESSAGE_OTHER,
						     GTK_BUTTONS_CANCEL,
						     _("<big><b>This medium contains software intended to be automatically started. Would you like to run it?</b></big>"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("The software will run directly from the medium “%s”. "
						    "You should never run software that you don't trust.\n"
						    "\n"
						    "If in doubt, press Cancel."),
                                                  mount_name);

	/* TODO: in a star trek future add support for verifying
	 * software on media (e.g. if it has a certificate, check it
	 * etc.)
	 */


	icon = g_mount_get_icon (mount);
	icon_size = nemo_get_icon_size_for_stock_size (GTK_ICON_SIZE_DIALOG);
	icon_info = nemo_icon_info_lookup (icon, icon_size);
	pixbuf = nemo_icon_info_get_pixbuf_at_size (icon_info, icon_size);
	image = gtk_image_new_from_pixbuf (pixbuf);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);

	gtk_message_dialog_set_image (GTK_MESSAGE_DIALOG (dialog), image);

	gtk_window_set_title (GTK_WINDOW (dialog), mount_name);
	gtk_window_set_icon (GTK_WINDOW (dialog), pixbuf);

	data = g_new0 (AutorunSoftwareDialogData, 1);
	data->dialog = dialog;
	data->mount = g_object_ref (mount);

	g_signal_connect (G_OBJECT (mount),
			  "unmounted",
			  G_CALLBACK (autorun_software_dialog_mount_unmounted),
			  data);

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_Run"),
			       GTK_RESPONSE_OK);

        gtk_widget_show_all (dialog);

        if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
                autorun (mount);
        }

	g_object_unref (icon_info);
	g_object_unref (pixbuf);
	g_free (mount_name);
}

int
main (int argc, char *argv[])
{
        GVolumeMonitor *monitor;
        GFile *file;
        GMount *mount;
	GError *error;

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

        if (argc != 2) {
		g_print ("Usage: %s mount-uri\n", argv[0]);
                goto out;
	}

        /* instantiate monitor so we get the "unmounted" signal properly */
        monitor = g_volume_monitor_get ();
        if (monitor == NULL) {
		g_warning ("Unable to connect to the volume monitor");
                goto out;
	}

        file = g_file_new_for_commandline_arg (argv[1]);
        if (file == NULL) {
		g_object_unref (monitor);
		g_warning ("Unable to parse mount URI");
                goto out;
	}

	error = NULL;
        mount = g_file_find_enclosing_mount (file, NULL, &error);
        if (mount == NULL) {
		g_warning ("Unable to find device for URI: %s", error->message);
		g_clear_error (&error);
		g_object_unref (file);
		g_object_unref (monitor);
                goto out;
	}

        present_autorun_for_software_dialog (mount);
	g_object_unref (file);
	g_object_unref (monitor);
	g_object_unref (mount);

out:	
	return 0;
}

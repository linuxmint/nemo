/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-error-reporting.h - implementation of file manager functions that report
 	                        errors to the user.

   Copyright (C) 2000 Eazel, Inc.

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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>

#include "nemo-error-reporting.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libnemo-private/nemo-file.h>
#include <eel/eel-string.h>
#include <eel/eel-stock-dialogs.h>

#define DEBUG_FLAG NEMO_DEBUG_DIRECTORY_VIEW
#include <libnemo-private/nemo-debug.h>

#define NEW_NAME_TAG "Nemo: new name"
#define MAXIMUM_DISPLAYED_FILE_NAME_LENGTH	50

static void finish_rename (NemoFile *file, gboolean stop_timer, GError *error);

void
nemo_report_error_loading_directory (NemoFile *file,
					 GError *error,
					 GtkWindow *parent_window)
{
	char *file_name;
	char *message;

	if (error == NULL ||
	    error->message == NULL) {
		return;
	}

	if (error->domain == G_IO_ERROR &&
	    error->code == G_IO_ERROR_NOT_MOUNTED) {
		/* This case is retried automatically */
		return;
	}
	
	file_name = nemo_file_get_display_name (file);
	
	if (error->domain == G_IO_ERROR) {
		switch (error->code) {
		case G_IO_ERROR_PERMISSION_DENIED:
			message = g_strdup_printf (_("You do not have the permissions necessary to view the contents of \"%s\"."),
						   file_name);
			break;
		case G_IO_ERROR_NOT_FOUND:
			message = g_strdup_printf (_("\"%s\" could not be found. Perhaps it has recently been deleted."),
						   file_name);
			break;
		default:
			message = g_strdup_printf (_("Sorry, could not display all the contents of \"%s\": %s"), file_name,
						   error->message);
		}
	} else {
		message = g_strdup (error->message);
	}

	eel_show_error_dialog (_("The folder contents could not be displayed."), message, parent_window);

	g_free (file_name);
	g_free (message);
}

void
nemo_report_error_setting_group (NemoFile *file,
				     GError *error,
				     GtkWindow *parent_window)
{
	char *file_name;
	char *message;

	if (error == NULL) {
		return;
	}

	file_name = nemo_file_get_display_name (file);

	message = NULL;
	if (error->domain == G_IO_ERROR) {
		switch (error->code) {
		case G_IO_ERROR_PERMISSION_DENIED:
			message = g_strdup_printf (_("You do not have the permissions necessary to change the group of \"%s\"."),
						   file_name);
			break;
		default:
			break;
		}
	}
			
	if (message == NULL) {
		/* We should invent decent error messages for every case we actually experience. */
		g_warning ("Hit unhandled case %s:%d in nemo_report_error_setting_group", 
			   g_quark_to_string (error->domain), error->code);
		/* fall through */
		message = g_strdup_printf (_("Sorry, could not change the group of \"%s\": %s"), file_name,
					   error->message);
	}
	
	
	eel_show_error_dialog (_("The group could not be changed."), message, parent_window);
	
	g_free (file_name);
	g_free (message);
}

void
nemo_report_error_setting_owner (NemoFile *file,
				     GError *error,
				     GtkWindow *parent_window)
{
	char *file_name;
	char *message;

	if (error == NULL) {
		return;
	}

	file_name = nemo_file_get_display_name (file);

	message = g_strdup_printf (_("Sorry, could not change the owner of \"%s\": %s"), file_name, error->message);

	eel_show_error_dialog (_("The owner could not be changed."), message, parent_window);

	g_free (file_name);
	g_free (message);
}		

void
nemo_report_error_setting_permissions (NemoFile *file,
					   GError *error,
					   GtkWindow *parent_window)
{
	char *file_name;
	char *message;

	if (error == NULL) {
		return;
	}

	file_name = nemo_file_get_display_name (file);

	message = g_strdup_printf (_("Sorry, could not change the permissions of \"%s\": %s"), file_name, error->message);

	eel_show_error_dialog (_("The permissions could not be changed."), message, parent_window);

	g_free (file_name);
	g_free (message);
}		

typedef struct _NemoRenameData {
	char *name;
	NemoFileOperationCallback callback;
	gpointer callback_data;
} NemoRenameData;

void
nemo_report_error_renaming_file (NemoFile *file,
				     const char *new_name,
				     GError *error,
				     GtkWindow *parent_window)
{
	char *original_name, *original_name_truncated;
	char *new_name_truncated;
	char *message;

	/* Truncate names for display since very long file names with no spaces
	 * in them won't get wrapped, and can create insanely wide dialog boxes.
	 */
	original_name = nemo_file_get_display_name (file);
	original_name_truncated = eel_str_middle_truncate (original_name, MAXIMUM_DISPLAYED_FILE_NAME_LENGTH);
	g_free (original_name);
	
	new_name_truncated = eel_str_middle_truncate (new_name, MAXIMUM_DISPLAYED_FILE_NAME_LENGTH);

	message = NULL;
	if (error->domain == G_IO_ERROR) {
		switch (error->code) {
		case G_IO_ERROR_EXISTS:
			message = g_strdup_printf (_("The name \"%s\" is already used in this folder. "
						     "Please use a different name."), 
						   new_name_truncated);
			break;
		case G_IO_ERROR_NOT_FOUND:
			message = g_strdup_printf (_("There is no \"%s\" in this folder. "
						     "Perhaps it was just moved or deleted?"), 
						   original_name_truncated);
			break;
		case G_IO_ERROR_PERMISSION_DENIED:
			message = g_strdup_printf (_("You do not have the permissions necessary to rename \"%s\"."),
						   original_name_truncated);
			break;
		case G_IO_ERROR_INVALID_FILENAME:
			if (strchr (new_name, '/') != NULL) {
				message = g_strdup_printf (_("The name \"%s\" is not valid because it contains the character \"/\". "
							     "Please use a different name."),
							   new_name_truncated);
			} else {
				message = g_strdup_printf (_("The name \"%s\" is not valid. "
							     "Please use a different name."),
							   new_name_truncated);
			}
			break;
                case G_IO_ERROR_FILENAME_TOO_LONG:
                        message = g_strdup_printf (_("The name \"%s\" is too long. "
                                                     "Please use a different name."),
                                                     new_name_truncated);
                        break;
		default:
			break;
		}
	}
	
	if (message == NULL) {
		/* We should invent decent error messages for every case we actually experience. */
		g_warning ("Hit unhandled case %s:%d in nemo_report_error_renaming_file", 
			   g_quark_to_string (error->domain), error->code);
		/* fall through */
		message = g_strdup_printf (_("Sorry, could not rename \"%s\" to \"%s\": %s"), 
					   original_name_truncated, new_name_truncated,
					   error->message);
	}
	
	g_free (original_name_truncated);
	g_free (new_name_truncated);

	eel_show_error_dialog (_("The item could not be renamed."), message, parent_window);
	g_free (message);
}

static void
nemo_rename_data_free (NemoRenameData *data)
{
	g_free (data->name);
	g_free (data);
}

static void
rename_callback (NemoFile *file, GFile *result_location,
		 GError *error, gpointer callback_data)
{
	NemoRenameData *data;

	g_assert (NEMO_IS_FILE (file));
	g_assert (callback_data == NULL);
	
	data = g_object_get_data (G_OBJECT (file), NEW_NAME_TAG);
	g_assert (data != NULL);

	if (error &&
	    !(error->domain == G_IO_ERROR && error->code == G_IO_ERROR_CANCELLED)) {
		/* If rename failed, notify the user. */
		nemo_report_error_renaming_file (file, data->name, error, NULL);
	}

	finish_rename (file, TRUE, error);
}

static void
cancel_rename_callback (gpointer callback_data)
{
	GError *error;
	
	error = g_error_new (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");
	finish_rename (NEMO_FILE (callback_data), FALSE, error);
	g_error_free (error);
}

static void
finish_rename (NemoFile *file, gboolean stop_timer, GError *error)
{
	NemoRenameData *data;

	data = g_object_get_data (G_OBJECT (file), NEW_NAME_TAG);
	if (data == NULL) {
		return;
	}

	/* Cancel both the rename and the timed wait. */
	nemo_file_cancel (file, rename_callback, NULL);
	if (stop_timer) {
		eel_timed_wait_stop (cancel_rename_callback, file);
	}

	if (data->callback != NULL) {
		data->callback (file, NULL, error, data->callback_data);
	}
	
	/* Let go of file name. */
	g_object_set_data (G_OBJECT (file), NEW_NAME_TAG, NULL);
}

void
nemo_rename_file (NemoFile *file,
		      const char *new_name,
		      NemoFileOperationCallback callback,
		      gpointer callback_data)
{
	char *old_name, *wait_message;
	NemoRenameData *data;
	char *uri;
	GError *error;

	g_return_if_fail (NEMO_IS_FILE (file));
	g_return_if_fail (new_name != NULL);

	/* Stop any earlier rename that's already in progress. */
	error = g_error_new (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");
	finish_rename (file, TRUE, error);
	g_error_free (error);

	data = g_new0 (NemoRenameData, 1);
	data->name = g_strdup (new_name);
	data->callback = callback;
	data->callback_data = callback_data;
	
	/* Attach the new name to the file. */
	g_object_set_data_full (G_OBJECT (file),
				NEW_NAME_TAG,
				data, (GDestroyNotify)nemo_rename_data_free);

	/* Start the timed wait to cancel the rename. */
	old_name = nemo_file_get_display_name (file);
	wait_message = g_strdup_printf (_("Renaming \"%s\" to \"%s\"."),
					old_name,
					new_name);
	g_free (old_name);
	eel_timed_wait_start (cancel_rename_callback, file, wait_message, 
			      NULL); /* FIXME bugzilla.gnome.org 42395: Parent this? */
	g_free (wait_message);

	uri = nemo_file_get_uri (file);
	DEBUG ("Renaming file %s to %s", uri, new_name);
	g_free (uri);

	/* Start the rename. */
	nemo_file_rename (file, new_name,
			      rename_callback, NULL);
}

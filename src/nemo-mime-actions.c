/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-mime-actions.c - uri-specific versions of mime action functions

   Copyright (C) 2000, 2001 Eazel, Inc.

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

   Authors: Maciej Stachowiak <mjs@eazel.com>
*/

#include <config.h>

#include "nemo-mime-actions.h"

#include "nemo-window-slot.h"
#include "nemo-error-reporting.h"

#include <eel/eel-glib-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <string.h>
#include <gdk/gdkx.h>

#include <libnemo-private/nemo-file-attributes.h>
#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-file-operations.h>
#include <libnemo-private/nemo-metadata.h>
#include <libnemo-private/nemo-program-choosing.h>
#include <libnemo-private/nemo-desktop-icon-file.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-signaller.h>
#include <libnemo-private/nemo-mime-application-chooser.h>
#include <sys/stat.h>

enum {
    UNIX_PERM_USER_EXEC = S_IXUSR,
    UNIX_PERM_GROUP_EXEC = S_IXGRP,
    UNIX_PERM_OTHER_EXEC = S_IXOTH,
};

#define DEBUG_FLAG NEMO_DEBUG_MIME
#include <libnemo-private/nemo-debug.h>

typedef enum {
	ACTIVATION_ACTION_LAUNCH_DESKTOP_FILE,
	ACTIVATION_ACTION_ASK,
	ACTIVATION_ACTION_LAUNCH,
	ACTIVATION_ACTION_LAUNCH_IN_TERMINAL,
	ACTIVATION_ACTION_OPEN_IN_VIEW,
	ACTIVATION_ACTION_OPEN_IN_APPLICATION,
	ACTIVATION_ACTION_DO_NOTHING,
} ActivationAction;

typedef struct {
	NemoFile *file;
	char *uri;
} LaunchLocation;

typedef struct {
	GAppInfo *application;
	GList *uris;
} ApplicationLaunchParameters;

typedef struct {
	NemoWindowSlot *slot;
	gpointer window;
	GtkWindow *parent_window;
	GCancellable *cancellable;
	GList *locations;
	GList *mountables;
	GList *start_mountables;
	GList *not_mounted;
	NemoWindowOpenFlags flags;
	char *timed_wait_prompt;
	gboolean timed_wait_active;
	NemoFileListHandle *files_handle;
	gboolean tried_mounting;
	char *activation_directory;
	gboolean user_confirmation;
} ActivateParameters;

/* Number of seconds until cancel dialog shows up */
#define DELAY_UNTIL_CANCEL_MSECS 5000

#define RESPONSE_RUN 1000
#define RESPONSE_DISPLAY 1001
#define RESPONSE_RUN_IN_TERMINAL 1002
#define RESPONSE_MARK_TRUSTED 1003
#define RESPONSE_OPEN_WITH 1004

#define SILENT_WINDOW_OPEN_LIMIT 5
#define SILENT_OPEN_LIMIT 5

/* This number controls a maximum character count for a URL that is
 * displayed as part of a dialog. It's fairly arbitrary -- big enough
 * to allow most "normal" URIs to display in full, but small enough to
 * prevent the dialog from getting insanely wide.
 */
#define MAX_URI_IN_DIALOG_LENGTH 60

static void cancel_activate_callback                (gpointer            callback_data);
static void activate_activation_uris_ready_callback (GList              *files,
						     gpointer            callback_data);
static void activation_mount_mountables             (ActivateParameters *parameters);
static void activation_start_mountables             (ActivateParameters *parameters);
static void activate_callback                       (GList              *files,
						     gpointer            callback_data);
static void activation_mount_not_mounted            (ActivateParameters *parameters);


static void
launch_location_free (LaunchLocation *location)
{
	nemo_file_unref (location->file);
	g_free (location->uri);
	g_free (location);
}

static void
launch_location_list_free (GList *list)
{
	g_list_foreach (list, (GFunc)launch_location_free, NULL);
	g_list_free (list);
}

static GList *
get_file_list_for_launch_locations (GList *locations)
{
	GList *files, *l;
	LaunchLocation *location;

	files = NULL;
	for (l = locations; l != NULL; l = l->next) {
		location = l->data;

		files = g_list_prepend (files,
					nemo_file_ref (location->file));
	}
	return g_list_reverse (files);
}


static LaunchLocation *
launch_location_from_file (NemoFile *file)
{
	LaunchLocation *location;
	location = g_new (LaunchLocation, 1);
	location->file = nemo_file_ref (file);
	location->uri = nemo_file_get_uri (file);
	
	return location;
}

static void
launch_location_update_from_file (LaunchLocation *location,
				  NemoFile *file)
{
	nemo_file_unref (location->file);
	g_free (location->uri);
	location->file = nemo_file_ref (file);
	location->uri = nemo_file_get_uri (file);
}

static void
launch_location_update_from_uri (LaunchLocation *location,
				 const char *uri)
{
	nemo_file_unref (location->file);
	g_free (location->uri);
	location->file = nemo_file_get_by_uri (uri);
	location->uri = g_strdup (uri);
}

static LaunchLocation *
find_launch_location_for_file (GList *list,
			       NemoFile *file)
{
	LaunchLocation *location;
	GList *l;

	for (l = list; l != NULL; l = l->next) {
		location = l->data;

		if (location->file == file) {
			return location;
		}
	}
	return NULL;
}

static GList *
launch_locations_from_file_list (GList *list)
{
	GList *new;

	new = NULL;
	while (list) {
		new = g_list_prepend (new,
				      launch_location_from_file (list->data));
		list = list->next;
	}
	new = g_list_reverse (new);
	return new;
}

static ApplicationLaunchParameters *
application_launch_parameters_new (GAppInfo *application,
			      	   GList *uris)
{
	ApplicationLaunchParameters *result;

	result = g_new0 (ApplicationLaunchParameters, 1);
	result->application = g_object_ref (application);
	result->uris = eel_g_str_list_copy (uris);

	return result;
}

static void
application_launch_parameters_free (ApplicationLaunchParameters *parameters)
{
	g_object_unref (parameters->application);
	g_list_free_full (parameters->uris, g_free);

	g_free (parameters);
}			      

static GList*
filter_nemo_handler (GList *apps)
{
	GList *l, *next;
	GAppInfo *application;
	const char *id;

	l = apps;
	while (l != NULL) {
		application = (GAppInfo *) l->data;
		next = l->next;

		id = g_app_info_get_id (application);
		if (id != NULL &&
		    strcmp (id,
			    "nemo.desktop") == 0) {
			g_object_unref (application);
			apps = g_list_delete_link (apps, l); 
		}

		l = next;
	}

	return apps;
}

static GList*
filter_non_uri_apps (GList *apps)
{
	GList *l, *next;
	GAppInfo *app;

	for (l = apps; l != NULL; l = next) {
		app = l->data;
		next = l->next;
		
		if (!g_app_info_supports_uris (app)) {
			apps = g_list_delete_link (apps, l);
			g_object_unref (app);
		}
	}
	return apps;
}


static gboolean
nemo_mime_actions_check_if_required_attributes_ready (NemoFile *file)
{
	NemoFileAttributes attributes;
	gboolean ready;

	attributes = nemo_mime_actions_get_required_file_attributes ();
	ready = nemo_file_check_if_ready (file, attributes);

	return ready;
}

NemoFileAttributes 
nemo_mime_actions_get_required_file_attributes (void)
{
	return NEMO_FILE_ATTRIBUTE_INFO |
		NEMO_FILE_ATTRIBUTE_LINK_INFO;
}

static gboolean
file_has_local_path (NemoFile *file)
{
	GFile *location;
	char *path;
	gboolean res;

	
	/* Don't only check _is_native, because we want to support
	   using the fuse path */
	location = nemo_file_get_location (file);
	if (g_file_is_native (location)) {
		res = TRUE;
	} else {
		path = g_file_get_path (location);
		
		res = path != NULL;
		
		g_free (path);
	}
	g_object_unref (location);
	
	return res;
}

GAppInfo *
nemo_mime_get_default_application_for_file (NemoFile *file)
{
	GAppInfo *app;
	char *mime_type;
	char *uri_scheme;

	if (!nemo_mime_actions_check_if_required_attributes_ready (file)) {
		return NULL;
	}

	mime_type = nemo_file_get_mime_type (file);
	app = g_app_info_get_default_for_type (mime_type, !file_has_local_path (file));
	g_free (mime_type);

	if (app == NULL) {
		uri_scheme = nemo_file_get_uri_scheme (file);
		if (uri_scheme != NULL) {
			app = g_app_info_get_default_for_uri_scheme (uri_scheme);
			g_free (uri_scheme);
		}
	}
	
	return app;
}

static int
file_compare_by_mime_type (NemoFile *file_a,
			   NemoFile *file_b)
{
	char *mime_type_a, *mime_type_b;
	int ret;
	
	mime_type_a = nemo_file_get_mime_type (file_a);
	mime_type_b = nemo_file_get_mime_type (file_b);
	
	ret = strcmp (mime_type_a, mime_type_b);
	
	g_free (mime_type_a);
	g_free (mime_type_b);
	
	return ret;
}

static int
file_compare_by_parent_uri (NemoFile *file_a,
			    NemoFile *file_b) {
	char *parent_uri_a, *parent_uri_b;
	int ret;

	parent_uri_a = nemo_file_get_parent_uri (file_a);
	parent_uri_b = nemo_file_get_parent_uri (file_b);

	ret = strcmp (parent_uri_a, parent_uri_b);

	g_free (parent_uri_a);
	g_free (parent_uri_b);

	return ret;
}

static int
application_compare_by_name (const GAppInfo *app_a,
			     const GAppInfo *app_b)
{
	return g_utf8_collate (g_app_info_get_name ((GAppInfo *)app_a),
			       g_app_info_get_name ((GAppInfo *)app_b));
}

static int
application_compare_by_id (const GAppInfo *app_a,
			   const GAppInfo *app_b)
{
	const char *id_a, *id_b;

	id_a = g_app_info_get_id ((GAppInfo *)app_a);
	id_b = g_app_info_get_id ((GAppInfo *)app_b);

	if (id_a == NULL && id_b == NULL) {
		if (g_app_info_equal ((GAppInfo *)app_a, (GAppInfo *)app_b)) {
			return 0;
		}
		if ((gsize)app_a < (gsize) app_b) {
			return -1;
		}
		return 1;
	}

	if (id_a == NULL) {
		return -1;
	}
	
	if (id_b == NULL) {
		return 1;
	}
	
	
	return strcmp (id_a, id_b);
}

GList *
nemo_mime_get_applications_for_file (NemoFile *file)
{
	char *mime_type;
	char *uri_scheme;
	GList *result;
	GAppInfo *uri_handler;

	if (!nemo_mime_actions_check_if_required_attributes_ready (file)) {
		return NULL;
	}
	mime_type = nemo_file_get_mime_type (file);
	result = g_app_info_get_all_for_type (mime_type);

	uri_scheme = nemo_file_get_uri_scheme (file);
	if (uri_scheme != NULL) {
		uri_handler = g_app_info_get_default_for_uri_scheme (uri_scheme);
		if (uri_handler) {
			result = g_list_prepend (result, uri_handler);
		}
		g_free (uri_scheme);
	}

	if (!file_has_local_path (file)) {
		/* Filter out non-uri supporting apps */
		result = filter_non_uri_apps (result);
	}
	
	result = g_list_sort (result, (GCompareFunc) application_compare_by_name);
	g_free (mime_type);

	return filter_nemo_handler (result);
}

GAppInfo *
nemo_mime_get_default_application_for_files (GList *files)
{
	GList *l, *sorted_files;
	NemoFile *file;
	GAppInfo *app, *one_app;

	g_assert (files != NULL);

	sorted_files = g_list_sort (g_list_copy (files), (GCompareFunc) file_compare_by_mime_type);

	app = NULL;
	for (l = sorted_files; l != NULL; l = l->next) {
		file = l->data;

		if (l->prev &&
		    file_compare_by_mime_type (file, l->prev->data) == 0 &&
		    file_compare_by_parent_uri (file, l->prev->data) == 0) {
			continue;
		}

		one_app = nemo_mime_get_default_application_for_file (file);
		if (one_app == NULL || (app != NULL && !g_app_info_equal (app, one_app))) {
			if (app) {
				g_object_unref (app);
			}
			if (one_app) {
				g_object_unref (one_app);
			}
			app = NULL;
			break;
		}

		if (app == NULL) {
			app = one_app;
		} else {
			g_object_unref (one_app);
		}
	}

	g_list_free (sorted_files);

	return app;
}

/* returns an intersection of two mime application lists,
 * and returns a new list, freeing a, b and all applications
 * that are not in the intersection set.
 * The lists are assumed to be pre-sorted by their IDs */
static GList *
intersect_application_lists (GList *a,
			     GList *b)
{
	GList *l, *m;
	GList *ret;
	GAppInfo *a_app, *b_app;
	int cmp;

	ret = NULL;

	l = a;
	m = b;

	while (l != NULL && m != NULL) {
		a_app = (GAppInfo *) l->data;
		b_app = (GAppInfo *) m->data;

		cmp = application_compare_by_id (a_app, b_app);
		if (cmp > 0) {
			g_object_unref (b_app);
			m = m->next;
		} else if (cmp < 0) {
			g_object_unref (a_app);
			l = l->next;
		} else {
			g_object_unref (b_app);
			ret = g_list_prepend (ret, a_app);
			l = l->next;
			m = m->next;
		}
	}

	g_list_foreach (l, (GFunc) g_object_unref, NULL);
	g_list_foreach (m, (GFunc) g_object_unref, NULL);

	g_list_free (a);
	g_list_free (b);

	return g_list_reverse (ret);
}

GList *
nemo_mime_get_applications_for_files (GList *files)
{
	GList *l, *sorted_files;
	NemoFile *file;
	GList *one_ret, *ret;

	g_assert (files != NULL);

	sorted_files = g_list_sort (g_list_copy (files), (GCompareFunc) file_compare_by_mime_type);

	ret = NULL;
	for (l = sorted_files; l != NULL; l = l->next) {
		file = l->data;

		if (l->prev &&
		    file_compare_by_mime_type (file, l->prev->data) == 0 &&
		    file_compare_by_parent_uri (file, l->prev->data) == 0) {
			continue;
		}

		one_ret = nemo_mime_get_applications_for_file (file);
		one_ret = g_list_sort (one_ret, (GCompareFunc) application_compare_by_id);
		if (ret != NULL) {
			ret = intersect_application_lists (ret, one_ret);
		} else {
			ret = one_ret;
		}

		if (ret == NULL) {
			break;
		}
	}

	g_list_free (sorted_files);

	ret = g_list_sort (ret, (GCompareFunc) application_compare_by_name);
	
	return ret;
}

static void
trash_or_delete_files (GtkWindow *parent_window,
		       const GList *files,
		       gboolean delete_if_all_already_in_trash)
{
	GList *locations;
	const GList *node;
	
	locations = NULL;
	for (node = files; node != NULL; node = node->next) {
		locations = g_list_prepend (locations,
					    nemo_file_get_location ((NemoFile *) node->data));
	}
	
	locations = g_list_reverse (locations);

	nemo_file_operations_trash_or_delete (locations,
						  parent_window,
						  NULL, NULL);
	g_list_free_full (locations, g_object_unref);
}

static void
report_broken_symbolic_link (GtkWindow *parent_window, NemoFile *file)
{
	char *target_path;
	char *display_name;
	char *prompt;
	char *detail;
	GtkDialog *dialog;
	GList file_as_list;
	int response;
	
	g_assert (nemo_file_is_broken_symbolic_link (file));

	display_name = nemo_file_get_display_name (file);
	if (nemo_file_is_in_trash (file)) {
		prompt = g_strdup_printf (_("The Link “%s” is Broken."), display_name);
	} else {
		prompt = g_strdup_printf (_("The Link “%s” is Broken. Move it to Trash?"), display_name);
	}
	g_free (display_name);

	target_path = nemo_file_get_symbolic_link_target_path (file);
	if (target_path == NULL) {
		detail = g_strdup (_("This link cannot be used, because it has no target."));
	} else {
		detail = g_strdup_printf (_("This link cannot be used, because its target "
					    "“%s” doesn't exist."), target_path);
	}
	
	if (nemo_file_is_in_trash (file)) {
		eel_run_simple_dialog (GTK_WIDGET (parent_window), FALSE, GTK_MESSAGE_WARNING,
				       prompt, detail, GTK_STOCK_CANCEL, NULL);
		goto out;
	}

	dialog = eel_show_yes_no_dialog (prompt, detail, _("Mo_ve to Trash"), GTK_STOCK_CANCEL,
					 parent_window);

	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_CANCEL);

	/* Make this modal to avoid problems with reffing the view & file
	 * to keep them around in case the view changes, which would then
	 * cause the old view not to be destroyed, which would cause its
	 * merged Bonobo items not to be un-merged. Maybe we need to unmerge
	 * explicitly when disconnecting views instead of relying on the
	 * unmerge in Destroy. But since BonoboUIHandler is probably going
	 * to change wildly, I don't want to mess with this now.
	 */

	response = gtk_dialog_run (dialog);
	gtk_widget_destroy (GTK_WIDGET (dialog));

	if (response == GTK_RESPONSE_YES) {
		file_as_list.data = file;
		file_as_list.next = NULL;
		file_as_list.prev = NULL;
	        trash_or_delete_files (parent_window, &file_as_list, TRUE);
	}

out:
	g_free (prompt);
	g_free (target_path);
	g_free (detail);
}

static ActivationAction
get_executable_text_file_action (GtkWindow *parent_window, NemoFile *file)
{
	GtkDialog *dialog;
	char *file_name;
	char *prompt;
	char *detail;
	int preferences_value;
	int response;

	g_assert (nemo_file_contains_text (file));

	preferences_value = g_settings_get_enum	(nemo_preferences,
						 NEMO_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION);
	switch (preferences_value) {
	case NEMO_EXECUTABLE_TEXT_LAUNCH:
		return ACTIVATION_ACTION_LAUNCH;
	case NEMO_EXECUTABLE_TEXT_DISPLAY:
		return ACTIVATION_ACTION_OPEN_IN_APPLICATION;
	case NEMO_EXECUTABLE_TEXT_ASK:
		break;
	default:
		/* Complain non-fatally, since preference data can't be trusted */
		g_warning ("Unknown value %d for NEMO_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION",
			   preferences_value);
		
	}


	file_name = nemo_file_get_display_name (file);
	prompt = g_strdup_printf (_("Do you want to run “%s”, or display its contents?"), 
	                            file_name);
	detail = g_strdup_printf (_("“%s” is an executable text file."),
				    file_name);
	g_free (file_name);

	dialog = eel_create_question_dialog (prompt,
					     detail,
					     _("Run in _Terminal"), RESPONSE_RUN_IN_TERMINAL,
     					     _("_Display"), RESPONSE_DISPLAY,
					     parent_window);
	gtk_dialog_add_button (dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (dialog, _("_Run"), RESPONSE_RUN);
	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_CANCEL);
	gtk_widget_show (GTK_WIDGET (dialog));
	
	g_free (prompt);
	g_free (detail);

	response = gtk_dialog_run (dialog);
	gtk_widget_destroy (GTK_WIDGET (dialog));
	
	switch (response) {
	case RESPONSE_RUN:
		return ACTIVATION_ACTION_LAUNCH;
	case RESPONSE_RUN_IN_TERMINAL:
		return ACTIVATION_ACTION_LAUNCH_IN_TERMINAL;
	case RESPONSE_DISPLAY:
		return ACTIVATION_ACTION_OPEN_IN_APPLICATION;
	default:
		return ACTIVATION_ACTION_DO_NOTHING;
	}
}

static ActivationAction
get_default_executable_text_file_action (void)
{
	int preferences_value;

	preferences_value = g_settings_get_enum	(nemo_preferences,
						 NEMO_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION);
	switch (preferences_value) {
	case NEMO_EXECUTABLE_TEXT_LAUNCH:
		return ACTIVATION_ACTION_LAUNCH;
	case NEMO_EXECUTABLE_TEXT_DISPLAY:
		return ACTIVATION_ACTION_OPEN_IN_APPLICATION;
	case NEMO_EXECUTABLE_TEXT_ASK:
	default:
		return ACTIVATION_ACTION_ASK;
	}
}

gboolean
nemo_mime_file_opens_in_view (NemoFile *file)
{
  return (nemo_file_is_directory (file) ||
	  NEMO_IS_DESKTOP_ICON_FILE (file));
}

static ActivationAction
get_activation_action (NemoFile *file)
{
	ActivationAction action;
	char *activation_uri;

	if (nemo_file_is_nemo_link (file)) {
		return ACTIVATION_ACTION_LAUNCH_DESKTOP_FILE;
	}
	
	activation_uri = nemo_file_get_activation_uri (file);
	if (activation_uri == NULL) {
		activation_uri = nemo_file_get_uri (file);
	}

	action = ACTIVATION_ACTION_DO_NOTHING;
	if (nemo_file_is_launchable (file)) {
		char *executable_path;
		
		action = ACTIVATION_ACTION_LAUNCH;
		
		executable_path = g_filename_from_uri (activation_uri, NULL, NULL);
		if (!executable_path) {
			action = ACTIVATION_ACTION_DO_NOTHING;
		} else if (nemo_file_contains_text (file)) {
			action = get_default_executable_text_file_action ();
		}
		g_free (executable_path);
	} 

	if (action == ACTIVATION_ACTION_DO_NOTHING) {
		if (nemo_mime_file_opens_in_view (file)) {
			action = ACTIVATION_ACTION_OPEN_IN_VIEW;
		} else {
			action = ACTIVATION_ACTION_OPEN_IN_APPLICATION;
		}
	}
	g_free (activation_uri);

	return action;
}

gboolean
nemo_mime_file_opens_in_external_app (NemoFile *file)
{
  ActivationAction activation_action;
  
  activation_action = get_activation_action (file);
  
  return (activation_action == ACTIVATION_ACTION_OPEN_IN_APPLICATION);
}


static unsigned int
mime_application_hash (GAppInfo *app)
{
	const char *id;

	id = g_app_info_get_id (app);

	if (id == NULL) {
		return GPOINTER_TO_UINT(app);
	}

	return g_str_hash (id);
}

static void
list_to_parameters_foreach (GAppInfo *application,
			    GList *uris,
			    GList **ret)
{
	ApplicationLaunchParameters *parameters;

	uris = g_list_reverse (uris);

	parameters = application_launch_parameters_new
		(application, uris);
	*ret = g_list_prepend (*ret, parameters);
}


/**
 * make_activation_parameters
 *
 * Construct a list of ApplicationLaunchParameters from a list of NemoFiles,
 * where files that have the same default application are put into the same
 * launch parameter, and others are put into the unhandled_files list.
 *
 * @files: Files to use for construction.
 * @unhandled_files: Files without any default application will be put here.
 * 
 * Return value: Newly allocated list of ApplicationLaunchParameters.
 **/
static GList *
make_activation_parameters (GList *uris,
			    GList **unhandled_uris)
{
	GList *ret, *l, *app_uris;
	NemoFile *file;
	GAppInfo *app, *old_app;
	GHashTable *app_table;
	char *uri;

	ret = NULL;
	*unhandled_uris = NULL;

	app_table = g_hash_table_new_full
		((GHashFunc) mime_application_hash,
		 (GEqualFunc) g_app_info_equal,
		 (GDestroyNotify) g_object_unref,
		 (GDestroyNotify) g_list_free);

	for (l = uris; l != NULL; l = l->next) {
		uri = l->data;
		file = nemo_file_get_by_uri (uri);

		app = nemo_mime_get_default_application_for_file (file);
		if (app != NULL) {
			app_uris = NULL;

			if (g_hash_table_lookup_extended (app_table, app,
							  (gpointer *) &old_app,
							  (gpointer *) &app_uris)) {
				g_hash_table_steal (app_table, old_app);

				app_uris = g_list_prepend (app_uris, uri);

				g_object_unref (app);
				app = old_app;
			} else {
				app_uris = g_list_prepend (NULL, uri);
			}

			g_hash_table_insert (app_table, app, app_uris);
		} else {
			*unhandled_uris = g_list_prepend (*unhandled_uris, uri);
		}
		nemo_file_unref (file);
	}

	g_hash_table_foreach (app_table,
			      (GHFunc) list_to_parameters_foreach,
			      &ret);

	g_hash_table_destroy (app_table);

	*unhandled_uris = g_list_reverse (*unhandled_uris);

	return g_list_reverse (ret);
}

static gboolean
file_was_cancelled (NemoFile *file)
{
	GError *error;
	
	error = nemo_file_get_file_info_error (file);
	return
		error != NULL &&
		error->domain == G_IO_ERROR &&
		error->code == G_IO_ERROR_CANCELLED;
}

static gboolean
file_was_not_mounted (NemoFile *file)
{
	GError *error;
	
	error = nemo_file_get_file_info_error (file);
	return
		error != NULL &&
		error->domain == G_IO_ERROR &&
		error->code == G_IO_ERROR_NOT_MOUNTED;
}

static void
activation_parameters_free (ActivateParameters *parameters)
{
	if (parameters->timed_wait_active) {
		eel_timed_wait_stop (cancel_activate_callback, parameters);
	}
	
	if (parameters->slot) {
		g_object_remove_weak_pointer (G_OBJECT (parameters->slot), (gpointer *)&parameters->slot);
	}
	if (parameters->parent_window) {
		g_object_remove_weak_pointer (G_OBJECT (parameters->parent_window), (gpointer *)&parameters->parent_window);
	}
	g_object_unref (parameters->cancellable);
	launch_location_list_free (parameters->locations);
	nemo_file_list_free (parameters->mountables);
	nemo_file_list_free (parameters->start_mountables);
	nemo_file_list_free (parameters->not_mounted);
	g_free (parameters->activation_directory);
	g_free (parameters->timed_wait_prompt);
	g_assert (parameters->files_handle == NULL);
	g_free (parameters);
}

static void
cancel_activate_callback (gpointer callback_data)
{
	ActivateParameters *parameters = callback_data;

	parameters->timed_wait_active = FALSE;
	
	g_cancellable_cancel (parameters->cancellable);

	if (parameters->files_handle) {
		nemo_file_list_cancel_call_when_ready (parameters->files_handle);
		parameters->files_handle = NULL;
		activation_parameters_free (parameters);
	}
}

static void
activation_start_timed_cancel (ActivateParameters *parameters)
{
	parameters->timed_wait_active = TRUE;
	eel_timed_wait_start_with_duration
		(DELAY_UNTIL_CANCEL_MSECS,
		 cancel_activate_callback,
		 parameters,
		 parameters->timed_wait_prompt,
		 parameters->parent_window);
}

static void
pause_activation_timed_cancel (ActivateParameters *parameters)
{
	if (parameters->timed_wait_active) {
		eel_timed_wait_stop (cancel_activate_callback, parameters);
		parameters->timed_wait_active = FALSE;
	}
}

static void
unpause_activation_timed_cancel (ActivateParameters *parameters)
{
	if (!parameters->timed_wait_active) {
		activation_start_timed_cancel (parameters);
	}
}


static void
activate_mount_op_active (GtkMountOperation *operation,
			  GParamSpec *pspec,
			  ActivateParameters *parameters)
{
	gboolean is_active;

	g_object_get (operation, "is-showing", &is_active, NULL);

	if (is_active) {
		pause_activation_timed_cancel (parameters);
	} else {
		unpause_activation_timed_cancel (parameters);
	}
}

static gboolean
confirm_multiple_windows (GtkWindow *parent_window,
			  int count,
			  gboolean use_tabs)
{
	GtkDialog *dialog;
	char *prompt;
	char *detail;
	int response;

	if (count <= SILENT_WINDOW_OPEN_LIMIT) {
		return TRUE;
	}

	prompt = _("Are you sure you want to open all files?");
	if (use_tabs) {
		detail = g_strdup_printf (ngettext("This will open %d separate tab.",
						   "This will open %d separate tabs.", count), count);
	} else {
		detail = g_strdup_printf (ngettext("This will open %d separate window.",
						   "This will open %d separate windows.", count), count);
	}
	dialog = eel_show_yes_no_dialog (prompt, detail, 
					 GTK_STOCK_OK, GTK_STOCK_CANCEL,
					 parent_window);
	g_free (detail);

	response = gtk_dialog_run (dialog);
	gtk_widget_destroy (GTK_WIDGET (dialog));

	return response == GTK_RESPONSE_YES;
}

typedef struct {
    GtkWindow *parent_window;
    NemoFile *file;
} ActivateParametersSpecial;

static void
activate_parameters_special_free (ActivateParametersSpecial *parameters_special)
{
    if (parameters_special->parent_window) {
        g_object_remove_weak_pointer (G_OBJECT (parameters_special->parent_window), (gpointer *)&parameters_special->parent_window);
    }
    nemo_file_unref (parameters_special->file);
    g_free (parameters_special);
}

static void
make_exec_callback (NemoFile *file,
                       GFile *res_loc,
                      GError *error,
                     gpointer callback_data)
{
    ActivateParametersSpecial *params = (ActivateParametersSpecial *) callback_data;

    if (error == NULL) {
        gchar *path = nemo_file_get_path (params->file);
        nemo_launch_application_from_command (gtk_widget_get_screen (GTK_WIDGET (params->parent_window)),
                                              path, FALSE, NULL);
        g_free (path);
    }
    nemo_report_error_setting_permissions (file, error, NULL);

    activate_parameters_special_free (params);
}

static void
open_with_dialog_response_cb (GtkDialog *dialog,
                              gint response_id,
                              gpointer user_data)
{
    GtkWindow *parent_window;
    GAppInfo *info;

    parent_window = user_data;

    if (response_id != GTK_RESPONSE_OK) {
        gtk_widget_destroy (GTK_WIDGET (dialog));
        return;
    }

    GtkWidget *content = gtk_dialog_get_content_area (dialog);
    GList *children = gtk_container_get_children (GTK_CONTAINER (content));

    NemoMimeApplicationChooser *chooser = children->data;

    g_list_free (children);

    info = nemo_mime_application_chooser_get_info (chooser);
    const GList *files = nemo_mime_application_chooser_get_files (chooser);

    g_signal_emit_by_name (nemo_signaller_get_current (), "mime_data_changed");

    nemo_launch_application (info, files, parent_window);

    gtk_widget_destroy (GTK_WIDGET (dialog));
    g_object_unref (info);
}

static void
run_open_with_dialog (ActivateParametersSpecial *params)
{
    GtkWidget *dialog;

    char *mime_type;
    GList files;

    mime_type = nemo_file_get_mime_type (params->file);

    dialog = gtk_dialog_new_with_buttons (_("Open with"),
                                          params->parent_window,
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          GTK_STOCK_CANCEL,
                                          GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OK,
                                          GTK_RESPONSE_OK,
                                          NULL);

    files.next = NULL;
    files.prev = NULL;
    files.data = params->file;
    GtkWidget *chooser = nemo_mime_application_chooser_new (&files, mime_type);


    GtkWidget *content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    gtk_box_pack_start (GTK_BOX (content), chooser, TRUE, TRUE, 0);

    gtk_widget_show_all (dialog);

    g_signal_connect_object (dialog, "response", 
                             G_CALLBACK (open_with_dialog_response_cb),
                             params->parent_window, 0);
    activate_parameters_special_free (params);
}

static void
unhandled_uri_response_callback (GtkDialog *dialog,
                                 int response_id,
                                 ActivateParametersSpecial *parameters)
{
    switch (response_id) {
    case RESPONSE_RUN:
        ;
        guint32 existing = nemo_file_get_permissions (parameters->file);
        nemo_file_set_permissions (parameters->file,
                                   existing | (UNIX_PERM_USER_EXEC|UNIX_PERM_GROUP_EXEC|UNIX_PERM_OTHER_EXEC),
                                   (NemoFileOperationCallback) make_exec_callback,
                                   parameters);
        break;
    case RESPONSE_OPEN_WITH:
        run_open_with_dialog (parameters);
        break;
    default:
        /* Just destroy dialog */
        break;
    }
    gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
application_unhandled_uri (ActivateParameters *parameters, char *uri)
{

    NemoFile *file = nemo_file_get_existing_by_uri (uri);

    char *primary, *secondary, *display_name;
    GtkWidget *dialog;

    ActivateParametersSpecial *parameters_special;

    parameters_special = g_new0 (ActivateParametersSpecial, 1);
    if (parameters->parent_window) {
        parameters_special->parent_window = parameters->parent_window;
        g_object_add_weak_pointer (G_OBJECT (parameters_special->parent_window), (gpointer *)&parameters_special->parent_window);
    }
    parameters_special->file = nemo_file_ref (file);

    primary = _("Unknown file type");
    display_name = nemo_file_get_display_name (file);
    secondary =
        g_strdup_printf (_("The file \"%s\" has no known programs associated with it.  "
                   "If you trust the source of this file, and have sufficient permissions, you can mark it executable and launch it.  "
                   "Or, you can use the Open With dialog to pick a program to associate it with."
                   ),
                 display_name);
    
    dialog = gtk_message_dialog_new (parameters->parent_window,
                     0,
                     GTK_MESSAGE_WARNING,
                     GTK_BUTTONS_NONE,
                     NULL);
    g_object_set (dialog,
              "text", primary,
              "secondary-text", secondary,
              NULL);

    gtk_dialog_add_button (GTK_DIALOG (dialog),
                   _("Make executable and run"), RESPONSE_RUN);
    gtk_dialog_add_button (GTK_DIALOG (dialog),
                   _("Choose a program"), RESPONSE_OPEN_WITH);

    if (!nemo_file_can_set_permissions (file)) {
        GtkWidget *w = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), RESPONSE_RUN);
        gtk_widget_set_sensitive (w, FALSE);
    }

    gtk_dialog_add_button (GTK_DIALOG (dialog),
                   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

    g_signal_connect (dialog, "response",
              G_CALLBACK (unhandled_uri_response_callback),
                          parameters_special);
    gtk_widget_show (dialog);
    
    g_free (display_name);
    g_free (secondary);
    return;
}


static void
untrusted_launcher_response_callback (GtkDialog *dialog,
				      int response_id,
				      ActivateParametersSpecial *parameters)
{
	GdkScreen *screen;
	char *uri;
	GFile *file;
	
	switch (response_id) {
	case RESPONSE_RUN:
		screen = gtk_widget_get_screen (GTK_WIDGET (parameters->parent_window));
		uri = nemo_file_get_uri (parameters->file);
		DEBUG ("Launching untrusted launcher %s", uri);
		nemo_launch_desktop_file (screen, uri, NULL,
					      parameters->parent_window);
		g_free (uri);
		break;
	case RESPONSE_MARK_TRUSTED:
		file = nemo_file_get_location (parameters->file);
		nemo_file_mark_desktop_file_trusted (file,
							 parameters->parent_window,
							 TRUE, 
							 NULL, NULL);
		g_object_unref (file);
		break;
	default:
		/* Just destroy dialog */
		break;
	}
	
	gtk_widget_destroy (GTK_WIDGET (dialog));
	activate_parameters_special_free (parameters);
}

static void
activate_desktop_file (ActivateParameters *parameters,
		       NemoFile *file)
{
	ActivateParametersSpecial *parameters_special;
	char *primary, *secondary, *display_name;
	GtkWidget *dialog;
	GdkScreen *screen;
	char *uri;
	
	screen = gtk_widget_get_screen (GTK_WIDGET (parameters->parent_window));

	if (!nemo_file_is_trusted_link (file)) {
		/* copy the parts of parameters we are interested in as the orignal will be freed */
		parameters_special = g_new0 (ActivateParametersSpecial, 1);
		if (parameters->parent_window) {
			parameters_special->parent_window = parameters->parent_window;
			g_object_add_weak_pointer (G_OBJECT (parameters_special->parent_window), (gpointer *)&parameters_special->parent_window);
		}
		parameters_special->file = nemo_file_ref (file);

		primary = _("Untrusted application launcher");
		display_name = nemo_file_get_display_name (file);
		secondary =
			g_strdup_printf (_("The application launcher “%s” has not been marked as trusted (executable). "
					   "If you do not know the source of this file, launching it may be unsafe."
					   ),
					 display_name);
		
		dialog = gtk_message_dialog_new (parameters->parent_window,
						 0,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_NONE,
						 NULL);
		g_object_set (dialog,
			      "text", primary,
			      "secondary-text", secondary,
			      NULL);

		gtk_dialog_add_button (GTK_DIALOG (dialog),
				       _("_Launch Anyway"), RESPONSE_RUN);
		if (nemo_file_can_set_permissions (file)) {
			gtk_dialog_add_button (GTK_DIALOG (dialog),
					       _("Mark as _Trusted"), RESPONSE_MARK_TRUSTED);
		}

		gtk_dialog_add_button (GTK_DIALOG (dialog),
				       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

		g_signal_connect (dialog, "response",
				  G_CALLBACK (untrusted_launcher_response_callback),
				  parameters_special);
		gtk_widget_show (dialog);
		
		g_free (display_name);
		g_free (secondary);
		return;
	}
	
	uri = nemo_file_get_uri (file);
	DEBUG ("Launching trusted launcher %s", uri);
	nemo_launch_desktop_file (screen, uri, NULL,
				      parameters->parent_window);
	g_free (uri);
}

static void
activate_files (ActivateParameters *parameters)
{
	NemoWindow *window;
	NemoWindowOpenFlags flags;
	NemoFile *file;
	GList *launch_desktop_files;
	GList *launch_files;
	GList *launch_in_terminal_files;
	GList *open_in_app_uris;
	GList *open_in_app_parameters;
	GList *unhandled_open_in_app_uris;
	ApplicationLaunchParameters *one_parameters;
	GList *open_in_view_files;
	GList *l;
	int count;
	char *uri;
	char *executable_path, *quoted_path;
	char *old_working_dir;
	ActivationAction action;
	GdkScreen *screen;
	LaunchLocation *location;
	gint num_apps;
	gint num_unhandled;
	gint num_files;
	gboolean open_files;
	
	screen = gtk_widget_get_screen (GTK_WIDGET (parameters->parent_window));

	launch_desktop_files = NULL;
	launch_files = NULL;
	launch_in_terminal_files = NULL;
	open_in_app_uris = NULL;
	open_in_view_files = NULL;

	for (l = parameters->locations; l != NULL; l = l->next) {
		location = l->data;
		file = location->file;

		if (file_was_cancelled (file)) {
			continue;
		}

		action = get_activation_action (file);
		if (action == ACTIVATION_ACTION_ASK) {
			/* Special case for executable text files, since it might be
			 * dangerous & unexpected to launch these.
			 */
			pause_activation_timed_cancel (parameters);
			action = get_executable_text_file_action (parameters->parent_window, file);
			unpause_activation_timed_cancel (parameters);
		}

		switch (action) {
		case ACTIVATION_ACTION_LAUNCH_DESKTOP_FILE :
			launch_desktop_files = g_list_prepend (launch_desktop_files, file);
			break;
		case ACTIVATION_ACTION_LAUNCH :
			launch_files = g_list_prepend (launch_files, file);
			break;
		case ACTIVATION_ACTION_LAUNCH_IN_TERMINAL :
			launch_in_terminal_files = g_list_prepend (launch_in_terminal_files, file);
			break;
		case ACTIVATION_ACTION_OPEN_IN_VIEW :
			open_in_view_files = g_list_prepend (open_in_view_files, file);
			break;
		case ACTIVATION_ACTION_OPEN_IN_APPLICATION :
			open_in_app_uris = g_list_prepend (open_in_app_uris, location->uri);
			break;
		case ACTIVATION_ACTION_DO_NOTHING :
			break;
		case ACTIVATION_ACTION_ASK :
			g_assert_not_reached ();
			break;
		}
	}

	launch_desktop_files = g_list_reverse (launch_desktop_files);
	for (l = launch_desktop_files; l != NULL; l = l->next) {
		file = NEMO_FILE (l->data);

		activate_desktop_file (parameters, file);
	}

	old_working_dir = NULL;
	if (parameters->activation_directory &&
	    (launch_files != NULL || launch_in_terminal_files != NULL)) {
		old_working_dir = g_get_current_dir ();
		g_chdir (parameters->activation_directory);
		
	}

	launch_files = g_list_reverse (launch_files);
	for (l = launch_files; l != NULL; l = l->next) {
		file = NEMO_FILE (l->data);

		uri = nemo_file_get_activation_uri (file);
		executable_path = g_filename_from_uri (uri, NULL, NULL);
		quoted_path = g_shell_quote (executable_path);

		DEBUG ("Launching file path %s", quoted_path);

		nemo_launch_application_from_command (screen, quoted_path, FALSE, NULL);
		g_free (quoted_path);
		g_free (executable_path);
		g_free (uri);
			
	}

	launch_in_terminal_files = g_list_reverse (launch_in_terminal_files);
	for (l = launch_in_terminal_files; l != NULL; l = l->next) {
		file = NEMO_FILE (l->data);

		uri = nemo_file_get_activation_uri (file);
		executable_path = g_filename_from_uri (uri, NULL, NULL);
		quoted_path = g_shell_quote (executable_path);

		DEBUG ("Launching in terminal file quoted path %s", quoted_path);

		nemo_launch_application_from_command (screen, quoted_path, TRUE, NULL);

		g_free (quoted_path);
		g_free (executable_path);
		g_free (uri);
	}

	if (old_working_dir != NULL) {
		g_chdir (old_working_dir);
		g_free (old_working_dir);
	}

	open_in_view_files = g_list_reverse (open_in_view_files);
	count = g_list_length (open_in_view_files);

	flags = parameters->flags;
	if (count > 1) {
		if ((parameters->flags & NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW) == 0) {
			flags |= NEMO_WINDOW_OPEN_FLAG_NEW_TAB;
		} else {
			flags |= NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW;
		}
	}

	if (parameters->slot != NULL &&
	    (!parameters->user_confirmation ||
	     confirm_multiple_windows (parameters->parent_window, count,
				       (flags & NEMO_WINDOW_OPEN_FLAG_NEW_TAB) != 0))) {

		if ((flags & NEMO_WINDOW_OPEN_FLAG_NEW_TAB) != 0 &&
		    g_settings_get_enum (nemo_preferences, NEMO_PREFERENCES_NEW_TAB_POSITION) ==
		    NEMO_NEW_TAB_POSITION_AFTER_CURRENT_TAB) {
			/* When inserting N tabs after the current one,
			 * we first open tab N, then tab N-1, ..., then tab 0.
			 * Each of them is appended to the current tab, i.e.
			 * prepended to the list of tabs to open.
			 */
			open_in_view_files = g_list_reverse (open_in_view_files);
		}


		for (l = open_in_view_files; l != NULL; l = l->next) {
			GFile *f;
			/* The ui should ask for navigation or object windows
			 * depending on what the current one is */
			file = NEMO_FILE (l->data);

			uri = nemo_file_get_activation_uri (file);
			f = g_file_new_for_uri (uri);
			nemo_window_slot_open_location (parameters->slot, f, flags);
			g_object_unref (f);
			g_free (uri);
		}
	}

	open_in_app_parameters = NULL;
	unhandled_open_in_app_uris = NULL;

	if (open_in_app_uris != NULL) {
		open_in_app_uris = g_list_reverse (open_in_app_uris);

		open_in_app_parameters = make_activation_parameters
			(open_in_app_uris, &unhandled_open_in_app_uris);
	}

	num_apps = g_list_length (open_in_app_parameters);
	num_unhandled = g_list_length (unhandled_open_in_app_uris);
	num_files = g_list_length (open_in_app_uris);
	open_files = TRUE;

	if (open_in_app_uris != NULL &&
	    (!parameters->user_confirmation ||
	     num_files + num_unhandled > SILENT_OPEN_LIMIT) &&
	     num_apps > 1) {
		GtkDialog *dialog;
		char *prompt;
		char *detail;
		int response;

		pause_activation_timed_cancel (parameters);

		prompt = _("Are you sure you want to open all files?");
		detail = g_strdup_printf (ngettext ("This will open %d separate application.",
						    "This will open %d separate applications.", num_apps), num_apps);
		dialog = eel_show_yes_no_dialog (prompt, detail,
						 GTK_STOCK_OK, GTK_STOCK_CANCEL,
						 parameters->parent_window);
		g_free (detail);

		response = gtk_dialog_run (dialog);
		gtk_widget_destroy (GTK_WIDGET (dialog));

		unpause_activation_timed_cancel (parameters);

		if (response != GTK_RESPONSE_YES) {
			open_files = FALSE;
		}
	}

	if (open_files) {
		for (l = open_in_app_parameters; l != NULL; l = l->next) {
			one_parameters = l->data;

			nemo_launch_application_by_uri (one_parameters->application,
							    one_parameters->uris,
							    parameters->parent_window);
			application_launch_parameters_free (one_parameters);
		}

		for (l = unhandled_open_in_app_uris; l != NULL; l = l->next) {
			uri = l->data;

			/* this does not block */
			application_unhandled_uri (parameters, uri);
		}
	}

	window = NULL;
	if (parameters->slot != NULL) {
		window = nemo_window_slot_get_window (parameters->slot);
	}

	if (open_in_app_parameters != NULL ||
	    unhandled_open_in_app_uris != NULL) {
		if ((parameters->flags & NEMO_WINDOW_OPEN_FLAG_CLOSE_BEHIND) != 0 &&
		    window != NULL) {
			nemo_window_close (window);
		}
	}

	g_list_free (launch_desktop_files);
	g_list_free (launch_files);
	g_list_free (launch_in_terminal_files);
	g_list_free (open_in_view_files);
	g_list_free (open_in_app_uris);
	g_list_free (open_in_app_parameters);
	g_list_free (unhandled_open_in_app_uris);
	
	activation_parameters_free (parameters);
}

static void 
activation_mount_not_mounted_callback (GObject *source_object,
				       GAsyncResult *res,
				       gpointer user_data)
{
	ActivateParameters *parameters = user_data;
	GError *error;
	NemoFile *file;
	LaunchLocation *loc;

	file = parameters->not_mounted->data;
		
	error = NULL;
	if (!g_file_mount_enclosing_volume_finish (G_FILE (source_object), res, &error)) {
		if (error->domain != G_IO_ERROR ||
		    (error->code != G_IO_ERROR_CANCELLED &&
		     error->code != G_IO_ERROR_FAILED_HANDLED &&
		     error->code != G_IO_ERROR_ALREADY_MOUNTED)) {
			eel_show_error_dialog (_("Unable to mount location"), error->message, parameters->parent_window);
		}

		if (error->domain != G_IO_ERROR ||
		    error->code != G_IO_ERROR_ALREADY_MOUNTED) {
			loc = find_launch_location_for_file (parameters->locations,
							     file);
			if (loc) {
				parameters->locations =
					g_list_remove (parameters->locations, loc); 
				launch_location_free (loc);
			}
		}

		g_error_free (error);
	} 
	
	parameters->not_mounted = g_list_delete_link (parameters->not_mounted,
						      parameters->not_mounted);
	nemo_file_unref (file);

	activation_mount_not_mounted (parameters);
}

static void
activation_mount_not_mounted (ActivateParameters *parameters)
{
	NemoFile *file;
	GFile *location;
	LaunchLocation *loc;
	GMountOperation *mount_op;
	GList *l, *next, *files;

	if (parameters->not_mounted != NULL) {
		file = parameters->not_mounted->data;
		mount_op = gtk_mount_operation_new (parameters->parent_window);
		g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
		g_signal_connect (mount_op, "notify::is-showing",
				  G_CALLBACK (activate_mount_op_active), parameters);
		location = nemo_file_get_location (file);
		g_file_mount_enclosing_volume (location, 0, mount_op, parameters->cancellable,
					       activation_mount_not_mounted_callback, parameters);
		g_object_unref (location);
		/* unref mount_op here - g_file_mount_enclosing_volume() does ref for itself */
		g_object_unref (mount_op);
		return;
	}

	parameters->tried_mounting = TRUE;

	if (parameters->locations == NULL) {
		activation_parameters_free (parameters);
		return;
	}
	
	/*  once the mount is finished, refresh all attributes        */
	/*  - fixes new windows not appearing after successful mount  */
	for (l = parameters->locations; l != NULL; l = next) {
		loc = l->data;
		next = l->next;
		nemo_file_invalidate_all_attributes (loc->file);
	}
	
	files = get_file_list_for_launch_locations (parameters->locations);
	nemo_file_list_call_when_ready
		(files,
		 nemo_mime_actions_get_required_file_attributes () | NEMO_FILE_ATTRIBUTE_LINK_INFO,
		 &parameters->files_handle,
		 activate_callback, parameters);
	nemo_file_list_free (files);
}


static void
activate_callback (GList *files, gpointer callback_data)
{
	ActivateParameters *parameters = callback_data;
	GList *l, *next;
	NemoFile *file;
	LaunchLocation *location;

	parameters->files_handle = NULL;

	for (l = parameters->locations; l != NULL; l = next) {
		location = l->data;
		file = location->file;
		next = l->next;

		if (file_was_cancelled (file)) {
			launch_location_free (location);
			parameters->locations = g_list_delete_link (parameters->locations, l);
			continue;
		}

		if (file_was_not_mounted (file)) {
			if (parameters->tried_mounting) {
				launch_location_free (location);
				parameters->locations = g_list_delete_link (parameters->locations, l);
			} else {
				parameters->not_mounted = g_list_prepend (parameters->not_mounted,
									  nemo_file_ref (file));
			}
			continue;
		}
	}


	if (parameters->not_mounted != NULL) {
		activation_mount_not_mounted (parameters);
	} else {
		activate_files (parameters);
	}
}

static void
activate_activation_uris_ready_callback (GList *files_ignore,
					 gpointer callback_data)
{
	ActivateParameters *parameters = callback_data;
	GList *l, *next, *files;
	NemoFile *file;
	LaunchLocation *location;

	parameters->files_handle = NULL;
	
	for (l = parameters->locations; l != NULL; l = next) {
		location = l->data;
		file = location->file;
		next = l->next;

		if (file_was_cancelled (file)) {
			launch_location_free (location);
			parameters->locations = g_list_delete_link (parameters->locations, l);
			continue;
		}

		if (nemo_file_is_broken_symbolic_link (file)) {
			launch_location_free (location);
			parameters->locations = g_list_delete_link (parameters->locations, l);
			pause_activation_timed_cancel (parameters);
			report_broken_symbolic_link (parameters->parent_window, file);
			unpause_activation_timed_cancel (parameters);
			continue;
		}

		if (nemo_file_get_file_type (file) == G_FILE_TYPE_MOUNTABLE &&
		    !nemo_file_has_activation_uri (file)) {
			/* Don't launch these... There is nothing we
			   can do */
			launch_location_free (location);
			parameters->locations = g_list_delete_link (parameters->locations, l);
			continue;
		}
		
	}

	if (parameters->locations == NULL) {
		activation_parameters_free (parameters);
		return;
	}

	/* Convert the files to the actual activation uri files */
	for (l = parameters->locations; l != NULL; l = l->next) {
		char *uri;
		location = l->data;

		/* We want the file for the activation URI since we care
		 * about the attributes for that, not for the original file.
		 */
		uri = nemo_file_get_activation_uri (location->file);
		if (uri != NULL) {
			launch_location_update_from_uri (location, uri);
		}
		g_free (uri);
	}


	/* get the parameters for the actual files */	
	files = get_file_list_for_launch_locations (parameters->locations);
	nemo_file_list_call_when_ready
		(files,
		 nemo_mime_actions_get_required_file_attributes () | NEMO_FILE_ATTRIBUTE_LINK_INFO,
		 &parameters->files_handle,
		 activate_callback, parameters);
	nemo_file_list_free (files);
}

static void
activation_get_activation_uris (ActivateParameters *parameters)
{
	GList *l, *files;
	NemoFile *file;
	LaunchLocation *location;

	/* link target info might be stale, re-read it */
	for (l = parameters->locations; l != NULL; l = l->next) {
		location = l->data;
		file = location->file;

		if (file_was_cancelled (file)) {
			launch_location_free (location);
			parameters->locations = g_list_delete_link (parameters->locations, l);
			continue;
		}
		
		if (nemo_file_is_symbolic_link (file)) {
			nemo_file_invalidate_attributes 
				(file,
				 NEMO_FILE_ATTRIBUTE_INFO |
				 NEMO_FILE_ATTRIBUTE_LINK_INFO);
		}
	}

	if (parameters->locations == NULL) {
		activation_parameters_free (parameters);
		return;
	}

	files = get_file_list_for_launch_locations (parameters->locations);
	nemo_file_list_call_when_ready
		(files,
		 NEMO_FILE_ATTRIBUTE_INFO |
		 NEMO_FILE_ATTRIBUTE_LINK_INFO,
		 &parameters->files_handle,
		 activate_activation_uris_ready_callback, parameters);
	nemo_file_list_free (files);
}

static void
activation_mountable_mounted (NemoFile  *file,
			      GFile         *result_location,
			      GError        *error,
			      gpointer       callback_data)
{
	ActivateParameters *parameters = callback_data;
	NemoFile *target_file;
	LaunchLocation *location;

	/* Remove from list of files that have to be mounted */
	parameters->mountables = g_list_remove (parameters->mountables, file); 
	nemo_file_unref (file);

	
	if (error == NULL) {
		/* Replace file with the result of the mount */
		target_file = nemo_file_get (result_location);

		location = find_launch_location_for_file (parameters->locations,
							  file);
		if (location) {
			launch_location_update_from_file (location, target_file);
		}
		nemo_file_unref (target_file);
	} else {
		/* Remove failed file */
		
		if (error->domain != G_IO_ERROR ||
		    (error->code != G_IO_ERROR_FAILED_HANDLED &&
		     error->code != G_IO_ERROR_ALREADY_MOUNTED)) {
			location = find_launch_location_for_file (parameters->locations,
								  file);
			if (location) {
				parameters->locations =
					g_list_remove (parameters->locations,
						       location); 
				launch_location_free (location);
			}
		}
		
		if (error->domain != G_IO_ERROR ||
		    (error->code != G_IO_ERROR_CANCELLED &&
		     error->code != G_IO_ERROR_FAILED_HANDLED &&
		     error->code != G_IO_ERROR_ALREADY_MOUNTED)) {
			eel_show_error_dialog (_("Unable to mount location"),
					       error->message, parameters->parent_window);
		}

		if (error->code == G_IO_ERROR_CANCELLED) {
			activation_parameters_free (parameters);
			return;
		}
	}

	/* Mount more mountables */
	activation_mount_mountables (parameters);
}


static void
activation_mount_mountables (ActivateParameters *parameters)
{
	NemoFile *file;
	GMountOperation *mount_op;

	if (parameters->mountables != NULL) {
		file = parameters->mountables->data;
		mount_op = gtk_mount_operation_new (parameters->parent_window);
		g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
		g_signal_connect (mount_op, "notify::is-showing",
				  G_CALLBACK (activate_mount_op_active), parameters);
		nemo_file_mount (file,
				     mount_op,
				     parameters->cancellable,
				     activation_mountable_mounted,
				     parameters);
		g_object_unref (mount_op);
		return;
	}

	if (parameters->mountables == NULL && parameters->start_mountables == NULL)
		activation_get_activation_uris (parameters);
}


static void
activation_mountable_started (NemoFile  *file,
			      GFile         *gfile_of_file,
			      GError        *error,
			      gpointer       callback_data)
{
	ActivateParameters *parameters = callback_data;
	LaunchLocation *location;

	/* Remove from list of files that have to be mounted */
	parameters->start_mountables = g_list_remove (parameters->start_mountables, file);
	nemo_file_unref (file);

	if (error == NULL) {
		/* Remove file */
		location = find_launch_location_for_file (parameters->locations, file);
		if (location != NULL) {
			parameters->locations = g_list_remove (parameters->locations, location);
			launch_location_free (location);
		}

	} else {
		/* Remove failed file */
		if (error->domain != G_IO_ERROR ||
		    (error->code != G_IO_ERROR_FAILED_HANDLED)) {
			location = find_launch_location_for_file (parameters->locations,
								  file);
			if (location) {
				parameters->locations =
					g_list_remove (parameters->locations,
						       location);
				launch_location_free (location);
			}
		}

		if (error->domain != G_IO_ERROR ||
		    (error->code != G_IO_ERROR_CANCELLED &&
		     error->code != G_IO_ERROR_FAILED_HANDLED)) {
			eel_show_error_dialog (_("Unable to start location"),
					       error->message, NULL);
		}

		if (error->code == G_IO_ERROR_CANCELLED) {
			activation_parameters_free (parameters);
			return;
		}
	}

	/* Start more mountables */
	activation_start_mountables (parameters);
}

static void
activation_start_mountables (ActivateParameters *parameters)
{
	NemoFile *file;
	GMountOperation *start_op;

	if (parameters->start_mountables != NULL) {
		file = parameters->start_mountables->data;
		start_op = gtk_mount_operation_new (parameters->parent_window);
		g_signal_connect (start_op, "notify::is-showing",
				  G_CALLBACK (activate_mount_op_active), parameters);
		nemo_file_start (file,
				     start_op,
				     parameters->cancellable,
				     activation_mountable_started,
				     parameters);
		g_object_unref (start_op);
		return;
	}

	if (parameters->mountables == NULL && parameters->start_mountables == NULL)
		activation_get_activation_uris (parameters);
}

/**
 * nemo_mime_activate_files:
 * 
 * Activate a list of files. Each one might launch with an application or
 * with a component. This is normally called only by subclasses.
 * @view: FMDirectoryView in question.
 * @files: A GList of NemoFiles to activate.
 * 
 **/
void
nemo_mime_activate_files (GtkWindow *parent_window,
			      NemoWindowSlot *slot,
			      GList *files,
			      const char *launch_directory,
			      NemoWindowOpenFlags flags,
			      gboolean user_confirmation)
{
	ActivateParameters *parameters;
	char *file_name;
	int file_count;
	GList *l, *next;
	NemoFile *file;
	LaunchLocation *location;

	if (files == NULL) {
		return;
	}

	DEBUG_FILES (files, "Calling activate_files() with files:");

	parameters = g_new0 (ActivateParameters, 1);
	parameters->slot = slot;
	g_object_add_weak_pointer (G_OBJECT (parameters->slot), (gpointer *)&parameters->slot);
	if (parent_window) {
		parameters->parent_window = parent_window;
		g_object_add_weak_pointer (G_OBJECT (parameters->parent_window), (gpointer *)&parameters->parent_window);
	}
	parameters->cancellable = g_cancellable_new ();
	parameters->activation_directory = g_strdup (launch_directory);
	parameters->locations = launch_locations_from_file_list (files);
	parameters->flags = flags;
	parameters->user_confirmation = user_confirmation;

	file_count = g_list_length (files);
	if (file_count == 1) {
		file_name = nemo_file_get_display_name (files->data);
		parameters->timed_wait_prompt = g_strdup_printf (_("Opening “%s”."), file_name);
		g_free (file_name);
	} else {
		parameters->timed_wait_prompt = g_strdup_printf (ngettext ("Opening %d item.",
									   "Opening %d items.",
									   file_count),
								 file_count);
	}

	
	for (l = parameters->locations; l != NULL; l = next) {
		location = l->data;
		file = location->file;
		next = l->next;
		
		if (nemo_file_can_mount (file)) {
			parameters->mountables = g_list_prepend (parameters->mountables,
								 nemo_file_ref (file));
		}

		if (nemo_file_can_start (file)) {
			parameters->start_mountables = g_list_prepend (parameters->start_mountables,
								       nemo_file_ref (file));
		}
	}
	
	activation_start_timed_cancel (parameters);
	if (parameters->mountables != NULL)
		activation_mount_mountables (parameters);
	if (parameters->start_mountables != NULL)
		activation_start_mountables (parameters);
	if (parameters->mountables == NULL && parameters->start_mountables == NULL)
		activation_get_activation_uris (parameters);
}

/**
 * nemo_mime_activate_file:
 * 
 * Activate a file in this view. This might involve switching the displayed
 * location for the current window, or launching an application.
 * @view: FMDirectoryView in question.
 * @file: A NemoFile representing the file in this view to activate.
 * @use_new_window: Should this item be opened in a new window?
 * 
 **/

void
nemo_mime_activate_file (GtkWindow *parent_window,
			     NemoWindowSlot *slot,
			     NemoFile *file,
			     const char *launch_directory,
			     NemoWindowOpenFlags flags)
{
	GList *files;

	g_return_if_fail (NEMO_IS_FILE (file));

	files = g_list_prepend (NULL, file);
	nemo_mime_activate_files (parent_window, slot, files, launch_directory, flags, FALSE);
	g_list_free (files);
}

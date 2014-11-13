/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nemo-application: main Nemo application class.
 *
 * Copyright (C) 1999, 2000 Red Hat, Inc.
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2010, Cosimo Cecchi <cosimoc@gnome.org>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 * Authors: Elliot Lee <sopwith@redhat.com>,
 *          Darin Adler <darin@bentspoon.com>
 *          Cosimo Cecchi <cosimoc@gnome.org>
 *
 */

#include <config.h>

#include "nemo-application.h"

#if ENABLE_EMPTY_VIEW
#include "nemo-empty-view.h"
#endif /* ENABLE_EMPTY_VIEW */

#include "nemo-bookmarks-window.h"
#include "nemo-connect-server-dialog.h"
#include "nemo-desktop-canvas-view.h"
#include "nemo-desktop-window.h"
#include "nemo-freedesktop-dbus.h"
#include "nemo-canvas-view.h"
#include "nemo-image-properties-page.h"
#include "nemo-list-view.h"
#include "nemo-previewer.h"
#include "nemo-progress-ui-handler.h"
#include "nemo-self-check-functions.h"
#include "nemo-window.h"
#include "nemo-window-manage-views.h"
#include "nemo-window-private.h"
#include "nemo-window-slot.h"
#include "nemo-statusbar.h"

#include <libnemo-private/nemo-dbus-manager.h>
#include <libnemo-private/nemo-desktop-link-monitor.h>
#include <libnemo-private/nemo-directory-private.h>
#include <libnemo-private/nemo-file-changes-queue.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-file-operations.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-lib-self-check-functions.h>
#include <libnemo-private/nemo-module.h>
#include <libnemo-private/nemo-profile.h>
#include <libnemo-private/nemo-signaller.h>
#include <libnemo-private/nemo-ui-utilities.h>
#include <libnemo-extension/nemo-menu-provider.h>

#define DEBUG_FLAG NEMO_DEBUG_APPLICATION
#include <libnemo-private/nemo-debug.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <libnotify/notify.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#ifdef HAVE_UNITY
#include "src/unity-bookmarks-handler.h"
#endif

/* Keep window from shrinking down ridiculously small; numbers are somewhat arbitrary */
#define APPLICATION_WINDOW_MIN_WIDTH	300
#define APPLICATION_WINDOW_MIN_HEIGHT	100

#define START_STATE_CONFIG "start-state"

#define NEMO_ACCEL_MAP_SAVE_DELAY 30

/* Keeps track of all the desktop windows. */
static GList *nemo_application_desktop_windows;

/* The saving of the accelerator map was requested  */
static gboolean save_of_accel_map_requested = FALSE;

static void     desktop_changed_callback          (gpointer                  user_data);
static void     mount_added_callback              (GVolumeMonitor            *monitor,
						   GMount                    *mount,
						   NemoApplication       *application);

G_DEFINE_TYPE (NemoApplication, nemo_application, GTK_TYPE_APPLICATION);

struct _NemoApplicationPriv {
	GVolumeMonitor *volume_monitor;
	NemoProgressUIHandler *progress_handler;

	gboolean no_desktop;
	gchar *geometry;

#if GLIB_CHECK_VERSION (2,34,0)
	NotifyNotification *unmount_notify;
#endif

	NemoBookmarkList *bookmark_list;

	GtkWidget *connect_server_window;
};

NemoBookmarkList *
nemo_application_get_bookmarks (NemoApplication *application)
{
	return application->priv->bookmark_list;
}

void
nemo_application_edit_bookmarks (NemoApplication *application,
				     NemoWindow      *window)
{
	GtkWindow *bookmarks_window;

	bookmarks_window = nemo_bookmarks_window_new (window);
	gtk_window_present (bookmarks_window);
}

#if GLIB_CHECK_VERSION (2,34,0)
void
nemo_application_notify_unmount_done (NemoApplication *application,
					  const gchar *message)
{
	if (application->priv->unmount_notify) {
		notify_notification_close (application->priv->unmount_notify, NULL);
		g_clear_object (&application->priv->unmount_notify);
	}

	if (message != NULL) {
		NotifyNotification *unplug;
		gchar **strings;

		strings = g_strsplit (message, "\n", 0);
		unplug = notify_notification_new (strings[0], strings[1],
						  "media-removable");

		notify_notification_show (unplug, NULL);
		g_object_unref (unplug);
		g_strfreev (strings);
	}
}

void
nemo_application_notify_unmount_show (NemoApplication *application,
					  const gchar *message)
{
	gchar **strings;

	strings = g_strsplit (message, "\n", 0);

	if (!application->priv->unmount_notify) {
		application->priv->unmount_notify =
			notify_notification_new (strings[0], strings[1],
						 "media-removable");

		notify_notification_set_hint (application->priv->unmount_notify,
					      "transient", g_variant_new_boolean (TRUE));
		notify_notification_set_urgency (application->priv->unmount_notify,
						 NOTIFY_URGENCY_CRITICAL);
	} else {
		notify_notification_update (application->priv->unmount_notify,
					    strings[0], strings[1],
					    "media-removable");
	}

	notify_notification_show (application->priv->unmount_notify, NULL);
	g_strfreev (strings);
}
#endif // GLIB_CHECK_VERSION (2,34,0)

static gboolean
check_required_directories (NemoApplication *application)
{
	char *user_directory;
	char *desktop_directory;
	GSList *directories;
	gboolean ret;

	g_assert (NEMO_IS_APPLICATION (application));

	nemo_profile_start (NULL);

	ret = TRUE;

	user_directory = nemo_get_user_directory ();
	desktop_directory = nemo_get_desktop_directory ();

	directories = NULL;

	if (!g_file_test (user_directory, G_FILE_TEST_IS_DIR)) {
		directories = g_slist_prepend (directories, user_directory);
	}

	if (!g_file_test (desktop_directory, G_FILE_TEST_IS_DIR)) {
		directories = g_slist_prepend (directories, desktop_directory);
	}

	if (directories != NULL) {
		int failed_count;
		GString *directories_as_string;
		GSList *l;
		char *error_string;
		const char *detail_string;
		GtkDialog *dialog;

		ret = FALSE;

		failed_count = g_slist_length (directories);

		directories_as_string = g_string_new ((const char *)directories->data);
		for (l = directories->next; l != NULL; l = l->next) {
			g_string_append_printf (directories_as_string, ", %s", (const char *)l->data);
		}

		error_string = _("Oops! Something went wrong.");
		if (failed_count == 1) {
			detail_string = g_strdup_printf (_("Unable to create a required folder. "
							   "Please create the following folder, or "
							   "set permissions such that it can be created:\n%s"),
							 directories_as_string->str);
		} else {
			detail_string = g_strdup_printf (_("Unable to create required folders. "
							   "Please create the following folders, or "
							   "set permissions such that they can be created:\n%s"),
							 directories_as_string->str);
		}

		dialog = eel_show_error_dialog (error_string, detail_string, NULL);
		/* We need the main event loop so the user has a chance to see the dialog. */
		gtk_application_add_window (GTK_APPLICATION (application),
					    GTK_WINDOW (dialog));

		g_string_free (directories_as_string, TRUE);
	}

	g_slist_free (directories);
	g_free (user_directory);
	g_free (desktop_directory);
	nemo_profile_end (NULL);

	return ret;
}

static void
menu_provider_items_updated_handler (NemoMenuProvider *provider, GtkWidget* parent_window, gpointer data)
{

	g_signal_emit_by_name (nemo_signaller_get_current (),
			       "popup_menu_changed");
}

static void
menu_provider_init_callback (void)
{
        GList *providers;
        GList *l;

        providers = nemo_module_get_extensions_for_type (NEMO_TYPE_MENU_PROVIDER);

        for (l = providers; l != NULL; l = l->next) {
                NemoMenuProvider *provider = NEMO_MENU_PROVIDER (l->data);

		g_signal_connect_after (G_OBJECT (provider), "items_updated",
                           (GCallback)menu_provider_items_updated_handler,
                           NULL);
        }

        nemo_module_extension_list_free (providers);
}

static void 
selection_get_cb (GtkWidget          *widget,
		  GtkSelectionData   *selection_data,
		  guint               info,
		  guint               time)
{
	/* No extra targets atm */
}

static GtkWidget *
get_desktop_manager_selection (GdkDisplay *display, int screen)
{
	char selection_name[32];
	GdkAtom selection_atom;
	Window selection_owner;
	GtkWidget *selection_widget;

	g_snprintf (selection_name, sizeof (selection_name), "_NET_DESKTOP_MANAGER_S%d", screen);
	selection_atom = gdk_atom_intern (selection_name, FALSE);

	selection_owner = XGetSelectionOwner (GDK_DISPLAY_XDISPLAY (display),
					      gdk_x11_atom_to_xatom_for_display (display, 
										 selection_atom));
	if (selection_owner != None) {
		return NULL;
	}
	
	selection_widget = gtk_invisible_new_for_screen (gdk_display_get_screen (display, screen));
	/* We need this for gdk_x11_get_server_time() */
	gtk_widget_add_events (selection_widget, GDK_PROPERTY_CHANGE_MASK);

	if (gtk_selection_owner_set_for_display (display,
						 selection_widget,
						 selection_atom,
						 gdk_x11_get_server_time (gtk_widget_get_window (selection_widget)))) {
		
		g_signal_connect (selection_widget, "selection_get",
				  G_CALLBACK (selection_get_cb), NULL);
		return selection_widget;
	}

	gtk_widget_destroy (selection_widget);
	
	return NULL;
}

static void
desktop_unrealize_cb (GtkWidget        *widget,
		      GtkWidget        *selection_widget)
{
	gtk_widget_destroy (selection_widget);
}

static gboolean
selection_clear_event_cb (GtkWidget	        *widget,
			  GdkEventSelection     *event,
			  NemoDesktopWindow *window)
{
	gtk_widget_destroy (GTK_WIDGET (window));
	
	nemo_application_desktop_windows =
		g_list_remove (nemo_application_desktop_windows, window);

	return TRUE;
}

static void
nemo_application_create_desktop_windows (NemoApplication *application)
{
	GdkDisplay *display;
	NemoDesktopWindow *window;
	GtkWidget *selection_widget;
	int screens, i;

	display = gdk_display_get_default ();
	screens = gdk_display_get_n_screens (display);

	for (i = 0; i < screens; i++) {

		DEBUG ("Creating a desktop window for screen %d", i);
		
		selection_widget = get_desktop_manager_selection (display, i);
		if (selection_widget != NULL) {
			window = nemo_desktop_window_new (GTK_APPLICATION (application),
							      gdk_display_get_screen (display, i));

			g_signal_connect (selection_widget, "selection_clear_event",
					  G_CALLBACK (selection_clear_event_cb), window);
			
			g_signal_connect (window, "unrealize",
					  G_CALLBACK (desktop_unrealize_cb), selection_widget);
			
			/* We realize it immediately so that the NEMO_DESKTOP_WINDOW_ID
			   property is set so gnome-settings-daemon doesn't try to set the
			   background. And we do a gdk_flush() to be sure X gets it. */
			gtk_widget_realize (GTK_WIDGET (window));
			gdk_flush ();

			nemo_application_desktop_windows =
				g_list_prepend (nemo_application_desktop_windows, window);
		}
	}
}

void
nemo_application_close_all_windows (NemoApplication *self)
{
	GList *list_copy;
	GList *l;
	
	list_copy = g_list_copy (gtk_application_get_windows (GTK_APPLICATION (self)));
	for (l = list_copy; l != NULL; l = l->next) {
		NemoWindow *window;
		
		window = NEMO_WINDOW (l->data);
		nemo_window_close (window);
	}
	g_list_free (list_copy);
}

static gboolean
another_navigation_window_already_showing (NemoApplication *application,
					   NemoWindow *the_window)
{
	GList *list, *item;
	
	list = gtk_application_get_windows (GTK_APPLICATION (application));
	for (item = list; item != NULL; item = item->next) {
		if (item->data != the_window) {
			return TRUE;
		}
	}
	
	return FALSE;
}

NemoWindow *
nemo_application_create_window (NemoApplication *application,
				    GdkScreen           *screen)
{
	NemoWindow *window;
	char *geometry_string;
	gboolean maximized;

	g_return_val_if_fail (NEMO_IS_APPLICATION (application), NULL);
	nemo_profile_start (NULL);

	window = nemo_window_new (GTK_APPLICATION (application), screen);

	maximized = g_settings_get_boolean
		(nemo_window_state, NEMO_WINDOW_STATE_MAXIMIZED);
	if (maximized) {
		gtk_window_maximize (GTK_WINDOW (window));
	} else {
		gtk_window_unmaximize (GTK_WINDOW (window));
	}

	geometry_string = g_settings_get_string
		(nemo_window_state, NEMO_WINDOW_STATE_GEOMETRY);
	if (geometry_string != NULL &&
	    geometry_string[0] != 0) {
		/* Ignore saved window position if a window with the same
		 * location is already showing. That way the two windows
		 * wont appear at the exact same location on the screen.
		 */
		eel_gtk_window_set_initial_geometry_from_string 
			(GTK_WINDOW (window), 
			 geometry_string,
			 NEMO_WINDOW_MIN_WIDTH,
			 NEMO_WINDOW_MIN_HEIGHT,
			 another_navigation_window_already_showing (application, window));
	}
	g_free (geometry_string);

	DEBUG ("Creating a new navigation window");
	nemo_profile_end (NULL);

	return window;
}

static void
mount_added_callback (GVolumeMonitor *monitor,
		      GMount *mount,
		      NemoApplication *application)
{
	NemoDirectory *directory;
	GFile *root;
	gchar *uri;
		
	root = g_mount_get_root (mount);
	uri = g_file_get_uri (root);

	DEBUG ("Added mount at uri %s", uri);
	g_free (uri);
	
	directory = nemo_directory_get_existing (root);
	g_object_unref (root);
	if (directory != NULL) {
		nemo_directory_force_reload (directory);
		nemo_directory_unref (directory);
	}
}

static void
open_window (NemoApplication *application,
	     GFile *location, GdkScreen *screen, const char *geometry)
{
	NemoWindow *window;
	gchar *uri;

	uri = g_file_get_uri (location);
	DEBUG ("Opening new window at uri %s", uri);
	nemo_profile_start (NULL);
	window = nemo_application_create_window (application,
						     screen);
	nemo_window_go_to (window, location);

	if (geometry != NULL && !gtk_widget_get_visible (GTK_WIDGET (window))) {
		/* never maximize windows opened from shell if a
		 * custom geometry has been requested.
		 */
		gtk_window_unmaximize (GTK_WINDOW (window));
		eel_gtk_window_set_initial_geometry_from_string (GTK_WINDOW (window),
								 geometry,
								 APPLICATION_WINDOW_MIN_WIDTH,
								 APPLICATION_WINDOW_MIN_HEIGHT,
								 FALSE);
	}

	nemo_profile_end (NULL);

	g_free (uri);
}

static void
open_windows (NemoApplication *application,
	      GFile **files,
	      gint n_files,
	      GdkScreen *screen,
	      const char *geometry)
{
	guint i;

	if (files == NULL || files[0] == NULL) {
		/* Open a window pointing at the default location. */
		open_window (application, NULL, screen, geometry);
	} else {
		/* Open windows at each requested location. */
		for (i = 0; i < n_files; i++) {
			open_window (application, files[i], screen, geometry);
		}
	}
}

void
nemo_application_open_location (NemoApplication *application,
				    GFile *location,
				    GFile *selection,
				    const char *startup_id)
{
	NemoWindow *window;
	GList *sel_list = NULL;

	nemo_profile_start (NULL);

	window = nemo_application_create_window (application, gdk_screen_get_default ());
	gtk_window_set_startup_id (GTK_WINDOW (window), startup_id);

	if (selection != NULL) {
		sel_list = g_list_prepend (sel_list, nemo_file_get (selection));
	}

	nemo_window_slot_open_location_full (nemo_window_get_active_slot (window), location,
						 0, sel_list, NULL, NULL);

	if (sel_list != NULL) {
		nemo_file_list_free (sel_list);
	}

	nemo_profile_end (NULL);
}

static void
nemo_application_open (GApplication *app,
			   GFile **files,
			   gint n_files,
			   const gchar *hint)
{
	NemoApplication *self = NEMO_APPLICATION (app);

	DEBUG ("Open called on the GApplication instance; %d files", n_files);

	open_windows (self, files, n_files,
		      gdk_screen_get_default (),
		      self->priv->geometry);
}

static GtkWindow *
get_focus_window (GtkApplication *application)
{
	GList *windows;
	GtkWindow *window = NULL;

	/* the windows are ordered with the last focused first */
	windows = gtk_application_get_windows (application);

	if (windows != NULL) {
		window = g_list_nth_data (windows, 0);
	}

	return window;
}

static gboolean
go_to_server_cb (NemoWindow *window,
		 GFile          *location,
		 GError         *error,
		 gpointer        user_data)
{
	gboolean retval;

	if (error == NULL) {
		GBookmarkFile *bookmarks;
		GError *error = NULL;
		char *datadir;
		char *filename;
		char *uri;
		char *title;
		NemoFile *file;
		gboolean safe_to_save = TRUE;

		file = nemo_file_get_existing (location);

		bookmarks = g_bookmark_file_new ();
		datadir = nemo_get_user_directory ();
		filename = g_build_filename (datadir, "servers", NULL);
		g_mkdir_with_parents (datadir, 0700);
		g_free (datadir);
		g_bookmark_file_load_from_file (bookmarks,
						filename,
						&error);
		if (error != NULL) {
			if (! g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
				/* only warn if the file exists */
				g_warning ("Unable to open server bookmarks: %s", error->message);
				safe_to_save = FALSE;
			}
			g_error_free (error);
		}

		if (safe_to_save) {
			uri = nemo_file_get_uri (file);
			title = nemo_file_get_display_name (file);
			g_bookmark_file_set_title (bookmarks, uri, title);
			g_bookmark_file_set_visited (bookmarks, uri, -1);
			g_bookmark_file_add_application (bookmarks, uri, NULL, NULL);
			g_free (uri);
			g_free (title);

			g_bookmark_file_to_file (bookmarks, filename, NULL);
		}

		g_free (filename);
		g_bookmark_file_free (bookmarks);

		retval = TRUE;
	} else {
		retval = FALSE;
	}

	return retval;
}

static void
on_connect_server_response (GtkDialog      *dialog,
			    int             response,
			    GtkApplication *application)
{
	if (response == GTK_RESPONSE_OK) {
		GFile *location;
		NemoWindow *window = NEMO_WINDOW (get_focus_window (application));

		location = nemo_connect_server_dialog_get_location (NEMO_CONNECT_SERVER_DIALOG (dialog));
		if (location != NULL) {
			nemo_window_slot_open_location_full (nemo_window_get_active_slot (window),
								 location,
								 NEMO_WINDOW_OPEN_FLAG_USE_DEFAULT_LOCATION,
								 NULL, go_to_server_cb, application);
		}
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
nemo_application_connect_server (NemoApplication *application,
				     NemoWindow      *window)
{
	GtkWidget *dialog;

	dialog = application->priv->connect_server_window;

	if (dialog == NULL) {
		dialog = nemo_connect_server_dialog_new (window);
		g_signal_connect (dialog, "response", G_CALLBACK (on_connect_server_response), application);
		application->priv->connect_server_window = GTK_WIDGET (dialog);

		g_object_add_weak_pointer (G_OBJECT (dialog),
					   (gpointer *) &application->priv->connect_server_window);
	}

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (window));
	gtk_window_set_screen (GTK_WINDOW (dialog), gtk_window_get_screen (GTK_WINDOW (window)));
	gtk_window_present (GTK_WINDOW (dialog));
}

static void
nemo_application_init (NemoApplication *application)
{
	GSimpleAction *action;

	application->priv =
		G_TYPE_INSTANCE_GET_PRIVATE (application, NEMO_TYPE_APPLICATION,
					     NemoApplicationPriv);

	action = g_simple_action_new ("quit", NULL);

        g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (action));

	g_signal_connect_swapped (action, "activate",
				  G_CALLBACK (nemo_application_quit), application);

	g_object_unref (action);
}

static void
nemo_application_finalize (GObject *object)
{
	NemoApplication *application;

	application = NEMO_APPLICATION (object);

	g_clear_object (&application->priv->volume_monitor);
	g_clear_object (&application->priv->progress_handler);
	g_clear_object (&application->priv->bookmark_list);

	g_free (application->priv->geometry);

	nemo_dbus_manager_stop ();
	nemo_freedesktop_dbus_stop ();
	notify_uninit ();

        G_OBJECT_CLASS (nemo_application_parent_class)->finalize (object);
}

static gboolean
do_cmdline_sanity_checks (NemoApplication *self,
			  gboolean perform_self_check,
			  gboolean version,
			  gboolean kill_shell,
			  gchar **remaining)
{
	gboolean retval = FALSE;

	if (perform_self_check && (remaining != NULL || kill_shell)) {
		g_printerr ("%s\n",
			    _("--check cannot be used with other options."));
		goto out;
	}

	if (kill_shell && remaining != NULL) {
		g_printerr ("%s\n",
			    _("--quit cannot be used with URIs."));
		goto out;
	}

	if (self->priv->geometry != NULL &&
	    remaining != NULL && remaining[0] != NULL && remaining[1] != NULL) {
		g_printerr ("%s\n",
			    _("--geometry cannot be used with more than one URI."));
		goto out;
	}

	retval = TRUE;

 out:
	return retval;
}

static void
do_perform_self_checks (gint *exit_status)
{
#ifndef NEMO_OMIT_SELF_CHECK
	gtk_init (NULL, NULL);

	nemo_profile_start (NULL);
	/* Run the checks (each twice) for nemo and libnemo-private. */

	nemo_run_self_checks ();
	nemo_run_lib_self_checks ();
	eel_exit_if_self_checks_failed ();

	nemo_run_self_checks ();
	nemo_run_lib_self_checks ();
	eel_exit_if_self_checks_failed ();
	nemo_profile_end (NULL);
#endif

	*exit_status = EXIT_SUCCESS;
}

void
nemo_application_quit (NemoApplication *self)
{
	GApplication *app = G_APPLICATION (self);
	GList *windows;

	windows = gtk_application_get_windows (GTK_APPLICATION (app));
	g_list_foreach (windows, (GFunc) gtk_widget_destroy, NULL);
}

static gboolean
nemo_application_local_command_line (GApplication *application,
					 gchar ***arguments,
					 gint *exit_status)
{
	gboolean perform_self_check = FALSE;
	gboolean version = FALSE;
	gboolean browser = FALSE;
	gboolean kill_shell = FALSE;
	gboolean no_default_window = FALSE;
	gchar **remaining = NULL;
	NemoApplication *self = NEMO_APPLICATION (application);

	const GOptionEntry options[] = {
#ifndef NEMO_OMIT_SELF_CHECK
		{ "check", 'c', 0, G_OPTION_ARG_NONE, &perform_self_check, 
		  N_("Perform a quick set of self-check tests."), NULL },
#endif
		/* dummy, only for compatibility reasons */
		{ "browser", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &browser,
		  NULL, NULL },
		{ "version", '\0', 0, G_OPTION_ARG_NONE, &version,
		  N_("Show the version of the program."), NULL },
		{ "geometry", 'g', 0, G_OPTION_ARG_STRING, &self->priv->geometry,
		  N_("Create the initial window with the given geometry."), N_("GEOMETRY") },
		{ "no-default-window", 'n', 0, G_OPTION_ARG_NONE, &no_default_window,
		  N_("Only create windows for explicitly specified URIs."), NULL },
		{ "no-desktop", '\0', 0, G_OPTION_ARG_NONE, &self->priv->no_desktop,
		  N_("Do not manage the desktop (ignore the preference set in the preferences dialog)."), NULL },
		{ "quit", 'q', 0, G_OPTION_ARG_NONE, &kill_shell, 
		  N_("Quit Nemo."), NULL },
		{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &remaining, NULL,  N_("[URI...]") },

		{ NULL }
	};
	GOptionContext *context;
	GError *error = NULL;
	gint argc = 0;
	gchar **argv = NULL;

	*exit_status = EXIT_SUCCESS;

	nemo_profile_start (NULL);

	context = g_option_context_new (_("\n\nBrowse the file system with the file manager"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gtk_get_option_group (FALSE));

	argv = *arguments;
	argc = g_strv_length (argv);

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		/* Translators: this is a fatal error quit message printed on the
		 * command line */
		g_printerr ("%s: %s\n", _("Could not parse arguments"), error->message);
		g_error_free (error);

		*exit_status = EXIT_FAILURE;
		goto out;
	}

	if (version) {
		g_print ("nemo " PACKAGE_VERSION "\n");
		goto out;
	}

	if (!do_cmdline_sanity_checks (self, perform_self_check,
				       version, kill_shell, remaining)) {
		*exit_status = EXIT_FAILURE;
		goto out;
	}

	if (perform_self_check) {
		do_perform_self_checks (exit_status);
		goto out;
	}

	DEBUG ("Parsing local command line, no_default_window %d, quit %d, "
	       "self checks %d, no_desktop %d",
	       no_default_window, kill_shell, perform_self_check, self->priv->no_desktop);

	g_application_register (application, NULL, &error);

	if (error != NULL) {
		/* Translators: this is a fatal error quit message printed on the
		 * command line */
		g_printerr ("%s: %s\n", _("Could not register the application"), error->message);
		g_error_free (error);

		*exit_status = EXIT_FAILURE;
		goto out;
	}

	if (kill_shell) {
		DEBUG ("Killing application, as requested");
		g_action_group_activate_action (G_ACTION_GROUP (application),
						"quit", NULL);
		goto out;
	}

	GFile **files;
	gint idx, len;

	len = 0;
	files = NULL;

	/* Convert args to GFiles */
	if (remaining != NULL) {
		GFile *file;
		GPtrArray *file_array;

		file_array = g_ptr_array_new ();

		for (idx = 0; remaining[idx] != NULL; idx++) {
			file = g_file_new_for_commandline_arg (remaining[idx]);
			if (file != NULL) {
				g_ptr_array_add (file_array, file);
			}
		}

		len = file_array->len;
		files = (GFile **) g_ptr_array_free (file_array, FALSE);
		g_strfreev (remaining);
	}

	if (files == NULL && !no_default_window) {
		files = g_malloc0 (2 * sizeof (GFile *));
		len = 1;

		files[0] = g_file_new_for_path (g_get_home_dir ());
		files[1] = NULL;
	}

	/* Invoke "Open" to create new windows */
	if (len > 0) {
		g_application_open (application, files, len, "");
	}

	for (idx = 0; idx < len; idx++) {
		g_object_unref (files[idx]);
	}
	g_free (files);

 out:
	g_option_context_free (context);
	nemo_profile_end (NULL);

	return TRUE;	
}

static gboolean
css_provider_load_from_resource (GtkCssProvider *provider,
                     const char     *resource_path,
                     GError        **error)
{
   GBytes  *data;
   gboolean retval;

   data = g_resources_lookup_data (resource_path, 0, error);
   if (!data)
       return FALSE;

   retval = gtk_css_provider_load_from_data (provider,
                         g_bytes_get_data (data, NULL),
                         g_bytes_get_size (data),
                         error);
   g_bytes_unref (data);

   return retval;
}

static void
nemo_application_add_app_css_provider (void)
{
  GtkCssProvider *provider;
  GError *error = NULL;
  GdkScreen *screen;

  provider = gtk_css_provider_new ();

  if (!css_provider_load_from_resource (provider, "/org/nemo/nemo-style-fallback.css", &error))
    {
      g_warning ("Failed to load fallback css file: %s", error->message);
      if (error->message != NULL)
        g_error_free (error);
      goto out_a;
    }

    screen = gdk_screen_get_default ();

  gtk_style_context_add_provider_for_screen (screen,
      GTK_STYLE_PROVIDER (provider),
      GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);

out_a:
  g_object_unref (provider);
}

static void
init_icons_and_styles (void)
{
	/* initialize search path for custom icons */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   NEMO_DATADIR G_DIR_SEPARATOR_S "icons");

    gtk_icon_size_register (NEMO_STATUSBAR_ICON_SIZE_NAME,
                            NEMO_STATUSBAR_ICON_SIZE,
                            NEMO_STATUSBAR_ICON_SIZE);

    nemo_application_add_app_css_provider ();
}

static void
nemo_application_open_desktop (NemoApplication *application)
{
	/* Initialize the desktop link monitor singleton */
	nemo_desktop_link_monitor_get ();

	if (nemo_application_desktop_windows == NULL) {
		nemo_application_create_desktop_windows (application);
	}
}

static void
nemo_application_close_desktop (void)
{
	if (nemo_application_desktop_windows != NULL) {
		g_list_foreach (nemo_application_desktop_windows,
				(GFunc) gtk_widget_destroy, NULL);
		g_list_free (nemo_application_desktop_windows);
		nemo_application_desktop_windows = NULL;
	}
	nemo_desktop_link_monitor_shutdown ();
}

/* callback for showing or hiding the desktop based on the user's preference */
static void
desktop_changed_callback (gpointer user_data)
{
	NemoApplication *application;

	application = NEMO_APPLICATION (user_data);
	if (g_settings_get_boolean (gnome_background_preferences, NEMO_PREFERENCES_SHOW_DESKTOP)) {
		nemo_application_open_desktop (application);
	} else {
		nemo_application_close_desktop ();
	}
}

static void
monitors_changed_callback (GdkScreen *screen, NemoApplication *application)
{
    if (g_settings_get_boolean (nemo_desktop_preferences, NEMO_PREFERENCES_SHOW_DESKTOP)) {
        nemo_application_close_desktop ();
        nemo_application_open_desktop (application);
    } else {
        nemo_application_close_desktop ();
    }
}

static void
init_desktop (NemoApplication *self)
{
	GdkScreen *screen;
	screen = gdk_display_get_screen (gdk_display_get_default (), 0);
	/* Initialize the desktop link monitor singleton */
	nemo_desktop_link_monitor_get ();


	if (!self->priv->no_desktop &&
	    !g_settings_get_boolean (nemo_desktop_preferences,
				     NEMO_PREFERENCES_SHOW_DESKTOP)) {
		self->priv->no_desktop = TRUE;
	}

	if (!self->priv->no_desktop) {
		nemo_application_open_desktop (self);
	}

	/* Monitor the preference to show or hide the desktop */
	g_signal_connect_swapped (nemo_desktop_preferences, "changed::" NEMO_PREFERENCES_SHOW_DESKTOP,
				  G_CALLBACK (desktop_changed_callback),
				  self);

	g_signal_connect (screen, "monitors-changed",
				  G_CALLBACK (monitors_changed_callback),
				  self);
}

static gboolean 
nemo_application_save_accel_map (gpointer data)
{
	if (save_of_accel_map_requested) {
		char *accel_map_filename;
	 	accel_map_filename = nemo_get_accel_map_file ();
	 	if (accel_map_filename) {
	 		gtk_accel_map_save (accel_map_filename);
	 		g_free (accel_map_filename);
	 	}
		save_of_accel_map_requested = FALSE;
	}

	return FALSE;
}

static void 
queue_accel_map_save_callback (GtkAccelMap *object, gchar *accel_path,
		guint accel_key, GdkModifierType accel_mods,
		gpointer user_data)
{
	if (!save_of_accel_map_requested) {
		save_of_accel_map_requested = TRUE;
		g_timeout_add_seconds (NEMO_ACCEL_MAP_SAVE_DELAY, 
				nemo_application_save_accel_map, NULL);
	}
}

static void
update_accel_due_to_scripts_migration (gpointer data, const gchar *accel_path, guint accel_key,
		GdkModifierType accel_mods, gboolean changed)
{
	char *old_scripts_location_esc = data;
	char *old_scripts_pt;

	old_scripts_pt = strstr (accel_path, old_scripts_location_esc);

	if (old_scripts_pt != NULL) {
		/* There's a mention of the deprecated scripts directory in the accel. Remove it, and
		 * add a migrated one. */
		char *tmp;
		char *tmp2;
		GString *new_accel_path;

		/* base part of accel */
		tmp = g_strndup (accel_path, old_scripts_pt - accel_path);
		new_accel_path = g_string_new (tmp);
		g_free (tmp);

		/* new script directory, escaped */
		tmp = nemo_get_scripts_directory_path ();
		tmp2 = nemo_escape_action_name (tmp, "");
		g_free (tmp);
		g_string_append (new_accel_path, tmp2);
		g_free (tmp2);

		/* script path relative to scripts directory */
		g_string_append (new_accel_path, old_scripts_pt + strlen (old_scripts_location_esc));

		/* exchange entry */
		gtk_accel_map_change_entry (accel_path, 0, 0, FALSE);
		gtk_accel_map_change_entry (new_accel_path->str, accel_key, accel_mods, TRUE);

		g_string_free (new_accel_path, TRUE);
	}
}

static void
init_gtk_accels (void)
{
	char *accel_map_filename;
	gboolean performed_migration = FALSE;
	char *old_scripts_directory_path = NULL;

	accel_map_filename = nemo_get_accel_map_file ();

	/* If the accel map file doesn't exist, try to migrate from
	 * former locations. */
	if (!g_file_test (accel_map_filename, G_FILE_TEST_IS_REGULAR)) {
		char *old_accel_map_filename;
		const gchar *override;

		override = g_getenv ("GNOME22_USER_DIR");
		if (override) {
			old_accel_map_filename = g_build_filename (override,
					"accels", "nemo", NULL);
			old_scripts_directory_path = g_build_filename (override,
				       "nemo-scripts",
				       NULL);
		} else {
			old_accel_map_filename = g_build_filename (g_get_home_dir (),
					".gnome2", "accels", "nemo", NULL);
			old_scripts_directory_path = g_build_filename (g_get_home_dir (),
								       ".gnome2",
								       "nemo-scripts",
								       NULL);
		}

		if (g_file_test (old_accel_map_filename, G_FILE_TEST_IS_REGULAR)) {
			char *parent_dir;

			parent_dir = g_path_get_dirname (accel_map_filename);
			if (g_mkdir_with_parents (parent_dir, 0700) == 0) {
				GFile *accel_map_file;
				GFile *old_accel_map_file;

				accel_map_file = g_file_new_for_path (accel_map_filename);
				old_accel_map_file = g_file_new_for_path (old_accel_map_filename);

				/* If the move fails, it's safer to not read any accel map
				 * on startup instead of reading the old one and possibly
				 * being stuck with it. */
				performed_migration = g_file_move (old_accel_map_file, accel_map_file, 0, NULL, NULL, NULL, NULL);

				g_object_unref (accel_map_file);
				g_object_unref (old_accel_map_file);
			}
			g_free (parent_dir);
		}

		g_free (old_accel_map_filename);
	}

	/* load accelerator map, and register save callback */
	gtk_accel_map_load (accel_map_filename);
	g_free (accel_map_filename);

	if (performed_migration) {
		/* Migrate accels pointing to scripts */
		char *old_scripts_location_esc;

		old_scripts_location_esc = nemo_escape_action_name (old_scripts_directory_path, "");
		gtk_accel_map_foreach (old_scripts_location_esc, update_accel_due_to_scripts_migration);
		g_free (old_scripts_location_esc);
		/* save map immediately */
		save_of_accel_map_requested = TRUE;
		nemo_application_save_accel_map (NULL);
	}

	g_signal_connect (gtk_accel_map_get (), "changed",
			  G_CALLBACK (queue_accel_map_save_callback), NULL);

	g_free (old_scripts_directory_path);
}

static void
menu_state_changed_callback (NemoApplication *self)
{
    if (!g_settings_get_boolean (nemo_window_state, NEMO_WINDOW_STATE_START_WITH_MENU_BAR) &&
        !g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_DISABLE_MENU_WARNING)) {

        GtkWidget *dialog;
        GtkWidget *msg_area;
        GtkWidget *checkbox;

        dialog = gtk_message_dialog_new (NULL,
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_INFO,
                                         GTK_BUTTONS_OK,
                                         _("Nemo's main menu is now hidden"));

        gchar *secondary;
        secondary = g_strdup_printf (_("You have chosen to hide the main menu.  You can get it back temporarily by:\n\n"
                                     "- Tapping the <Alt> key\n"
                                     "- Right-clicking an empty region of the main toolbar\n"
                                     "- Right-clicking an empty region of the status bar.\n\n"
                                     "You can restore it permanently by selecting this option again from the View menu."));
        g_object_set (dialog,
                      "secondary-text", secondary,
                      NULL);
        g_free (secondary);

        msg_area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dialog));
        checkbox = gtk_check_button_new_with_label (_("Don't show this message again."));
        gtk_box_pack_start (GTK_BOX (msg_area), checkbox, TRUE, TRUE, 2);

        g_settings_bind (nemo_preferences,
                         NEMO_PREFERENCES_DISABLE_MENU_WARNING,
                         checkbox,
                         "active",
                         G_SETTINGS_BIND_DEFAULT);

        gtk_widget_show_all (dialog);

        g_signal_connect (dialog, "response",
                          G_CALLBACK (gtk_widget_destroy), NULL);
    }

}














static void
nemo_application_startup (GApplication *app)
{
	NemoApplication *self = NEMO_APPLICATION (app);

	nemo_profile_start (NULL);

	/* chain up to the GTK+ implementation early, so gtk_init()
	 * is called for us.
	 */
	G_APPLICATION_CLASS (nemo_application_parent_class)->startup (app);

	gtk_window_set_default_icon_name ("system-file-manager");

	/* initialize the previewer singleton */
	//nemo_previewer_get_singleton ();

	/* create DBus manager */
	nemo_dbus_manager_start (app);
	nemo_freedesktop_dbus_start (self);

	/* initialize preferences and create the global GSettings objects */
	nemo_global_preferences_init ();

	/* register views */
	nemo_profile_start ("Register views");
	nemo_canvas_view_register ();
	nemo_desktop_canvas_view_register ();
	nemo_list_view_register ();
	nemo_canvas_view_compact_register ();
#if ENABLE_EMPTY_VIEW
	nemo_empty_view_register ();
#endif
	nemo_profile_end ("Register views");

	/* register property pages */
	nemo_image_properties_page_register ();

	/* initialize theming */
	init_icons_and_styles ();
	init_gtk_accels ();
	
	/* initialize nemo modules */
	nemo_profile_start ("Modules");
	nemo_module_setup ();
	nemo_profile_end ("Modules");

	/* attach menu-provider module callback */
	menu_provider_init_callback ();
	
	/* Initialize the UI handler singleton for file operations */
	notify_init (GETTEXT_PACKAGE);
	self->priv->progress_handler = nemo_progress_ui_handler_new ();

	self->priv->volume_monitor = g_volume_monitor_get ();
	g_signal_connect_object (self->priv->volume_monitor, "mount_added",
				 G_CALLBACK (mount_added_callback), self, 0);

    g_signal_connect_swapped (nemo_window_state, "changed::" NEMO_WINDOW_STATE_START_WITH_MENU_BAR,
                              G_CALLBACK (menu_state_changed_callback), self);

	self->priv->bookmark_list = nemo_bookmark_list_new ();

	/* Check the user's .nemo directories and post warnings
	 * if there are problems.
	 */
	check_required_directories (self);
	init_desktop (self);

#ifdef HAVE_UNITY
	unity_bookmarks_handler_initialize ();
#endif

	nemo_profile_end (NULL);
}

static void
nemo_application_quit_mainloop (GApplication *app)
{
	DEBUG ("Quitting mainloop");

	nemo_icon_info_clear_caches ();
 	nemo_application_save_accel_map (NULL);

#if GLIB_CHECK_VERSION (2,34,0)
	nemo_application_notify_unmount_done (NEMO_APPLICATION (app), NULL);
#endif

	G_APPLICATION_CLASS (nemo_application_parent_class)->quit_mainloop (app);
}

static void
nemo_application_window_removed (GtkApplication *app,
				     GtkWindow *window)
{
	NemoPreviewer *previewer;

	/* chain to parent */
	GTK_APPLICATION_CLASS (nemo_application_parent_class)->window_removed (app, window);

	/* if this was the last window, close the previewer */
	if (g_list_length (gtk_application_get_windows (app)) == 0) {
		previewer = nemo_previewer_get_singleton ();
		nemo_previewer_call_close (previewer);
	}
}

static void
nemo_application_class_init (NemoApplicationClass *class)
{
        GObjectClass *object_class;
	GApplicationClass *application_class;
	GtkApplicationClass *gtkapp_class;

        object_class = G_OBJECT_CLASS (class);
        object_class->finalize = nemo_application_finalize;

	application_class = G_APPLICATION_CLASS (class);
	application_class->startup = nemo_application_startup;
	application_class->quit_mainloop = nemo_application_quit_mainloop;
	application_class->open = nemo_application_open;
	application_class->local_command_line = nemo_application_local_command_line;

	gtkapp_class = GTK_APPLICATION_CLASS (class);
	gtkapp_class->window_removed = nemo_application_window_removed;

	g_type_class_add_private (class, sizeof (NemoApplicationPriv));
}

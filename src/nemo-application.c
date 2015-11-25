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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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
#include "nemo-dbus-manager.h"
#include "nemo-desktop-canvas-view.h"
#include "nemo-desktop-window.h"
#include "nemo-freedesktop-dbus.h"
#include "nemo-canvas-view.h"
#include "nemo-image-properties-page.h"
#include "nemo-list-view.h"
#include "nemo-previewer.h"
#include "nemo-progress-ui-handler.h"
#include "nemo-self-check-functions.h"
#include "nemo-shell-search-provider.h"
#include "nemo-window.h"
#include "nemo-window-private.h"
#include "nemo-window-slot.h"
#include "nemo-statusbar.h"

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
#include <libnemo-private/nemo-thumbnails.h>
#include <libnemo-extension/nemo-menu-provider.h>
#include <libnemo-private/nemo-thumbnails.c>

#define DEBUG_FLAG NEMO_DEBUG_APPLICATION
#include <libnemo-private/nemo-debug.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <errno.h>
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

#define GNOME_DESKTOP_USE_UNSTABLE_API

#ifdef GNOME_BUILD
#include <libgnome-desktop/gnome-desktop-thumbnail.h>
#else
#include <libcinnamon-desktop/gnome-desktop-thumbnail.h>
#endif

#define NEMO_ACCEL_MAP_SAVE_DELAY 30

/* The saving of the accelerator map was requested  */
static gboolean save_of_accel_map_requested = FALSE;

static void     desktop_changed_callback          (gpointer                  user_data);
static void     nemo_application_open_desktop (NemoApplication *application);
static void     nemo_application_close_desktop (void);


G_DEFINE_TYPE (NemoApplication, nemo_application, GTK_TYPE_APPLICATION);

struct _NemoApplicationPriv {
	NemoProgressUIHandler *progress_handler;
	NemoDBusManager *dbus_manager;
	NemoFreedesktopDBus *fdb_manager;

	gboolean no_desktop;
	gboolean force_desktop;

    gboolean cache_problem;
    gboolean ignore_cache_problem;

#if GLIB_CHECK_VERSION (2,34,0)
	NotifyNotification *unmount_notify;
#endif

	NemoBookmarkList *bookmark_list;

	GtkWidget *connect_server_window;

	NemoShellSearchProvider *search_provider;
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
		notify_notification_set_hint (unplug,
					      "desktop-entry", g_variant_new_string ("nemo"));

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
					      "desktop-entry", g_variant_new_string ("nemo"));
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
			       "popup-menu-changed");
}

static void
menu_provider_init_callback (void)
{
        GList *providers;
        GList *l;

        providers = nemo_module_get_extensions_for_type (NEMO_TYPE_MENU_PROVIDER);

        for (l = providers; l != NULL; l = l->next) {
                NemoMenuProvider *provider = NEMO_MENU_PROVIDER (l->data);

		g_signal_connect_after (G_OBJECT (provider), "items-updated",
                           (GCallback)menu_provider_items_updated_handler,
                           NULL);
        }

        nemo_module_extension_list_free (providers);
}

void
nemo_application_close_all_windows (NemoApplication *self)
{
	GList *l;
	
	for (l = gtk_application_get_windows (GTK_APPLICATION (self)); l; l = l->next) {
		if (NEMO_IS_WINDOW (l->data))
			nemo_window_close (NEMO_WINDOW (l->data));
	}	
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

	window = nemo_window_new (screen);

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
			 TRUE);
	}
	g_free (geometry_string);

	DEBUG ("Creating a new navigation window");
	nemo_profile_end (NULL);

	return window;
}

static NemoWindowSlot *
get_window_slot_for_location (NemoApplication *application, GFile *location)
{
	NemoWindowSlot *slot;
	GList *l, *sl;

	slot = NULL;

	if (g_file_query_file_type (location, G_FILE_QUERY_INFO_NONE, NULL) != G_FILE_TYPE_DIRECTORY) {
		location = g_file_get_parent (location);
	} else {
		g_object_ref (location);
	}

	for (l = gtk_application_get_windows (GTK_APPLICATION (application)); l; l = l->next) {
		if (!NEMO_IS_WINDOW (l->data) || NEMO_IS_DESKTOP_WINDOW (l->data))
			continue;

		GList *p;
		GList *panes = nemo_window_get_panes (NEMO_WINDOW (l->data));
		for (p = panes; p != NULL; p = p->next) {
			NemoWindowPane *pane = NEMO_WINDOW_PANE (p->data);
			for (sl = pane->slots; sl; sl = sl->next) {
				NemoWindowSlot *current = NEMO_WINDOW_SLOT (sl->data);
				GFile *slot_location = nemo_window_slot_get_location (current);

				if (g_file_equal (slot_location, location)) {
					slot = current;
					break;
				}
			}
			if (slot) {
				break;
			}
		}
		if (slot) {
			break;
		}
	}

	g_object_unref (location);

	return slot;
}


static void
open_window (NemoApplication *application,
	     GFile *location, GdkScreen *screen)
{
	NemoWindow *window;

	nemo_profile_start (NULL);
	window = nemo_application_create_window (application, screen);

	if (location != NULL) {
		nemo_window_go_to (window, location);
	} else {
		nemo_window_slot_go_home (nemo_window_get_active_slot (window), 0);
	}

	nemo_profile_end (NULL);
}

static void
open_windows (NemoApplication *application,
	      gboolean force_new,
	      GFile **files,
	      gint n_files,
	      GdkScreen *screen)
{
	guint i;

	if (files == NULL || files[0] == NULL) {
		/* Open a window pointing at the default location. */
		open_window (application, NULL, screen);
	} else {
		/* Open windows at each requested location. */
		for (i = 0; i < n_files; ++i) {
			NemoWindowSlot *slot = NULL;

			if (!force_new)
				slot = get_window_slot_for_location (application, files[i]);

			if (!slot) {
				open_window (application, files[i], screen);
			} else {
				/* We open the location again to update any possible selection */
				nemo_window_slot_open_location (slot, files[i], 0);

				NemoWindow *window = nemo_window_slot_get_window (slot);
				nemo_window_set_active_slot (window, slot);
				gtk_window_present (GTK_WINDOW (window));
			}
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
	NemoWindowSlot *slot;
	GList *sel_list = NULL;

	nemo_profile_start (NULL);

	slot = get_window_slot_for_location (application, location);

	if (!slot) {
		window = nemo_application_create_window (application, gdk_screen_get_default ());
		slot = nemo_window_get_active_slot (window);
	} else {
		window = nemo_window_slot_get_window (slot);
		nemo_window_set_active_slot (window, slot);
		gtk_window_present (GTK_WINDOW (window));
	}

	if (selection != NULL) {
		sel_list = g_list_prepend (sel_list, nemo_file_get (selection));
	}

	gtk_window_set_startup_id (GTK_WINDOW (window), startup_id);
	nemo_window_slot_open_location_full (slot, location, 0, sel_list, NULL, NULL);

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

	gboolean force_new = (g_strcmp0 (hint, "new-window") == 0);

	open_windows (self, force_new, files, n_files,
		      gdk_screen_get_default ());
}

static GtkWindow *
get_focus_window (GtkApplication *application)
{
#if GTK_CHECK_VERSION(3, 6, 0)
        return gtk_application_get_active_window (application);
#else  
        GList *windows;
	GtkWindow *window = NULL;

	/* the windows are ordered with the last focused first */
	windows = gtk_application_get_windows (application);

	if (windows != NULL) {
		window = g_list_nth_data (windows, 0);
	}

	return window;
#endif
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

		nemo_file_unref (file);
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

GtkWidget *
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

	return dialog;
}

static void
nemo_application_init (NemoApplication *application)
{
	GSimpleAction *action_quit;
	GSimpleAction *action_open_desktop;
	GSimpleAction *action_close_desktop;

	application->priv =
		G_TYPE_INSTANCE_GET_PRIVATE (application, NEMO_TYPE_APPLICATION,
					     NemoApplicationPriv);

	action_quit = g_simple_action_new ("quit", NULL);
    g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (action_quit));
	g_signal_connect_swapped (action_quit, "activate",
				  G_CALLBACK (nemo_application_quit), application);
	g_object_unref (action_quit);

	action_open_desktop = g_simple_action_new ("open-desktop", NULL);
    g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (action_open_desktop));
	g_signal_connect_swapped (action_open_desktop, "activate",
				  G_CALLBACK (nemo_application_open_desktop), application);
    g_object_unref (action_open_desktop);


    action_close_desktop = g_simple_action_new ("close-desktop", NULL);
    g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (action_close_desktop));
	g_signal_connect_swapped (action_close_desktop, "activate",
				  G_CALLBACK (nemo_application_close_desktop), application);
    g_object_unref (action_close_desktop);

}

static void
nemo_application_finalize (GObject *object)
{
	NemoApplication *application;

	application = NEMO_APPLICATION (object);

	g_clear_object (&application->priv->progress_handler);
	g_clear_object (&application->priv->bookmark_list);

	g_clear_object (&application->priv->dbus_manager);
	g_clear_object (&application->priv->fdb_manager);
	g_clear_object (&application->priv->search_provider);

	notify_uninit ();

        G_OBJECT_CLASS (nemo_application_parent_class)->finalize (object);
}

static gboolean
do_cmdline_sanity_checks (NemoApplication *self,
			  gboolean perform_self_check,
			  gboolean version,
			  gboolean kill_shell,
			  gboolean select_uris,
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


	if (select_uris && remaining == NULL) {
		g_printerr ("%s\n",
			    _("--select must be used with at least an URI."));
		goto out;
	}

	if (self->priv->no_desktop && self->priv->force_desktop) {
		g_printerr ("%s\n",
			    _("--no-desktop and --force-desktop cannot be used together."));
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

	/* we have been asked to force quit */
	g_application_quit (G_APPLICATION (app));
}

static void
select_items_ready_cb (GObject *source,
		       GAsyncResult *res,
		       gpointer user_data)
{
	GDBusConnection *connection = G_DBUS_CONNECTION (source);
	NemoApplication *self = user_data;
	GError *error = NULL;

	g_dbus_connection_call_finish (connection, res, &error);

	if (error != NULL) {
		g_warning ("Unable to select specified URIs %s\n", error->message);
		g_error_free (error);

		/* open default location instead */
		g_application_open (G_APPLICATION (self), NULL, 0, "");
	}
}

static void
nemo_application_select (NemoApplication *self,
			     GFile **files,
			     gint len)
{
	GVariantBuilder builder;
	gint idx;
	gchar *uri;
	GDBusConnection *connection;
#if GLIB_CHECK_VERSION (2, 34, 0)
	connection = g_application_get_dbus_connection (g_application_get_default ());
#else
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
#endif

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
	for (idx = 0; idx < len; idx++) {
		uri = g_file_get_uri (files[idx]);
		g_variant_builder_add (&builder, "s", uri);
		g_free (uri);
	}

	g_dbus_connection_call (connection,
				NEMO_FDO_DBUS_NAME,
				NEMO_FDO_DBUS_PATH,
				NEMO_FDO_DBUS_IFACE,
				"ShowItems",
				g_variant_new ("(ass)", &builder, ""), NULL,
				G_DBUS_CALL_FLAGS_NONE, G_MAXINT, NULL,
				select_items_ready_cb, self);

	g_variant_builder_clear (&builder);
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
	gboolean open_new_window = FALSE;
	gboolean no_default_window = FALSE;
	gboolean select_uris = FALSE;
#ifndef GNOME_BUILD
    gboolean fix_cache = FALSE;
#endif
	gchar *geometry = NULL;
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
                /* ditto */
		{ "geometry", 'g', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &geometry,
		  N_("Create the initial window with the given geometry."), N_("GEOMETRY") },
		{ "version", '\0', 0, G_OPTION_ARG_NONE, &version,
		  N_("Show the version of the program."), NULL },
		{ "new-window", 'w', 0, G_OPTION_ARG_NONE, &open_new_window,
		  N_("Always open a new window for browsing specified URIs"), NULL },
		{ "no-default-window", 'n', 0, G_OPTION_ARG_NONE, &no_default_window,
		  N_("Only create windows for explicitly specified URIs."), NULL },
		{ "no-desktop", '\0', 0, G_OPTION_ARG_NONE, &self->priv->no_desktop,
		  N_("Never manage the desktop (ignore the GSettings preference)."), NULL },
		{ "force-desktop", '\0', 0, G_OPTION_ARG_NONE, &self->priv->force_desktop,
		  N_("Always manage the desktop (ignore the GSettings preference)."), NULL },
#ifndef GNOME_BUILD
        { "fix-cache", '\0', 0, G_OPTION_ARG_NONE, &fix_cache,
          N_("Repair the user thumbnail cache - this can be useful if you're having trouble with file thumbnails.  Must be run as root"), NULL },
#endif
		{ "quit", 'q', 0, G_OPTION_ARG_NONE, &kill_shell, 
		  N_("Quit Nemo."), NULL },
		{ "select", 's', 0, G_OPTION_ARG_NONE, &select_uris,
		  N_("Select specified URI in parent folder."), NULL },
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
				       version, kill_shell, select_uris, remaining)) {
		*exit_status = EXIT_FAILURE;
		goto out;
	}

	if (perform_self_check) {
		do_perform_self_checks (exit_status);
		goto out;
	}

#ifndef GNOME_BUILD
    if (fix_cache) {
        if (geteuid () != 0) {
            g_printerr ("The --fix-cache option must be run with sudo or as the root user.\n");
        } else {


            gnome_desktop_thumbnail_cache_fix_permissions ();
            g_print ("User thumbnail cache successfully repaired.\n");
        }

        goto out;
    }
#endif

	DEBUG ("Parsing local command line: open_new_window %d, no_default_window %d, "
	       "quit %d, self checks %d, no_desktop %d, show_desktop %d",
	       open_new_window, no_default_window, kill_shell, perform_self_check,
	       self->priv->no_desktop, self->priv->force_desktop);

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

	if (self->priv->force_desktop) {
		DEBUG ("Forcing desktop, as requested");
		g_action_group_activate_action (G_ACTION_GROUP (application),
						"open-desktop", NULL);
                /* fall through */
	}

	if (self->priv->no_desktop) {
		DEBUG ("Forcing desktop off, as requested");
		g_action_group_activate_action (G_ACTION_GROUP (application),
						"close-desktop", NULL);
                /* fall through */
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

	if (files == NULL && !no_default_window && !select_uris) {
		files = g_malloc0 (2 * sizeof (GFile *));
		len = 1;

		files[0] = g_file_new_for_path (g_get_home_dir ());
		files[1] = NULL;
	}

	if (len == 0) {
		goto out;
	}

	if (select_uris) {
		nemo_application_select (self, files, len);
	} else {
		/* Invoke "Open" to create new windows */
		g_application_open (application, files, len, open_new_window ? "new-window" : "");
	}

	for (idx = 0; idx < len; idx++) {
		g_object_unref (files[idx]);
	}
	g_free (files);

 out:
	g_free (geometry);
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

  provider = gtk_css_provider_new ();

  if (!css_provider_load_from_resource (provider, "/org/nemo/nemo-style-application.css", &error))
    {
      g_warning ("Failed to load application css file: %s", error->message);
      if (error->message != NULL)
        g_error_free (error);
      goto out_b;
    }

  screen = gdk_screen_get_default ();

  gtk_style_context_add_provider_for_screen (screen,
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

out_b:
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
	nemo_desktop_window_ensure ();	
}

static void
nemo_application_close_desktop (void)
{
	GtkWidget *desktop_window;

	desktop_window = nemo_desktop_window_get ();
	if (desktop_window != NULL) {
		gtk_widget_destroy (desktop_window);
	}
}

/* callback for showing or hiding the desktop based on the user's preference */
static void
desktop_changed_callback (gpointer user_data)
{
	NemoApplication *application;

	application = NEMO_APPLICATION (user_data);
	if (g_settings_get_boolean (nemo_desktop_preferences, NEMO_PREFERENCES_SHOW_DESKTOP)) {
		g_action_group_activate_action (G_ACTION_GROUP (application),
						"open-desktop", NULL);
	} else {
		g_action_group_activate_action (G_ACTION_GROUP (application),
						"close-desktop", NULL);
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
	screen = gdk_display_get_default_screen (gdk_display_get_default ());

	gboolean should_show;

	/* by default, take the GSettings value */
	should_show = g_settings_get_boolean (nemo_desktop_preferences,
					      NEMO_PREFERENCES_SHOW_DESKTOP);

	/* the command line switches take over the setting */
	if (self->priv->no_desktop) {
		should_show = FALSE;
	} else if (self->priv->force_desktop) {
		should_show = TRUE;
	}

	if (should_show) {
		g_action_group_activate_action (G_ACTION_GROUP (self),
						"open-desktop", NULL);
	} else {
		g_action_group_activate_action (G_ACTION_GROUP (self),
						"close-desktop", NULL);
	}

	/* Monitor the preference to show or hide the desktop, if no other
	 * command line option was specified.
	 */
	if (!self->priv->no_desktop && !self->priv->force_desktop) {
	g_signal_connect_swapped (nemo_desktop_preferences, "changed::" NEMO_PREFERENCES_SHOW_DESKTOP,
				  G_CALLBACK (desktop_changed_callback),
				  self);
	}

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

	/* create DBus manager */
	self->priv->fdb_manager = nemo_freedesktop_dbus_new ();

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

        g_signal_connect_swapped (nemo_window_state, "changed::" NEMO_WINDOW_STATE_START_WITH_MENU_BAR,
                              G_CALLBACK (menu_state_changed_callback), self);


	/* Bookmarks and search */
	self->priv->bookmark_list = nemo_bookmark_list_new ();
	self->priv->search_provider = nemo_shell_search_provider_new ();

	/* Check the user's .nemo directories and post warnings
	 * if there are problems.
	 */
	check_required_directories (self);

    self->priv->cache_problem = FALSE;
    self->priv->ignore_cache_problem = FALSE;

#ifndef GNOME_BUILD
    /* silently do a full check of the cache and fix if running as root.
     * If running as a normal user, do a quick check, and we'll notify the
     * user later if there's a problem via an infobar */
    if (geteuid () == 0) {
        if (!gnome_desktop_thumbnail_cache_check_permissions (NULL, FALSE))
            gnome_desktop_thumbnail_cache_fix_permissions ();
    } else {
        if (!gnome_desktop_thumbnail_cache_check_permissions (NULL, TRUE))
            self->priv->cache_problem = TRUE;
    }
#endif

    if (geteuid() != 0)
		init_desktop (self);

#ifdef HAVE_UNITY
	unity_bookmarks_handler_initialize ();
#endif

	nemo_profile_end (NULL);
}

static gboolean
nemo_application_dbus_register (GApplication	 *app,
				    GDBusConnection      *connection,
				    const gchar		 *object_path,
				    GError		**error)
{
	NemoApplication *self = NEMO_APPLICATION (app);

	self->priv->dbus_manager = nemo_dbus_manager_new ();
	if (!nemo_dbus_manager_register (self->priv->dbus_manager, connection, error)) {
		return FALSE;
	}

	return TRUE;
}

static void
nemo_application_dbus_unregister (GApplication	*app,
				      GDBusConnection   *connection,
				      const gchar	*object_path)
{
	NemoApplication *self = NEMO_APPLICATION (app);

	if (self->priv->dbus_manager) {
		nemo_dbus_manager_unregister (self->priv->dbus_manager);
	}
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
update_dbus_opened_locations (NemoApplication *app)
{
	gint i;
	GList *l, *sl;
	GList *locations = NULL;
	gsize locations_size = 0;
	gchar **locations_array;

	g_return_if_fail (NEMO_IS_APPLICATION (app));

	for (l = gtk_application_get_windows (GTK_APPLICATION (app)); l; l = l->next) {
		if (!NEMO_IS_WINDOW (l->data) || NEMO_IS_DESKTOP_WINDOW (l->data))
			continue;

		GList *p;
		GList *panes = nemo_window_get_panes (NEMO_WINDOW (l->data));
		for (p = panes; p != NULL; p = p->next) {
			NemoWindowPane *pane = NEMO_WINDOW_PANE (p->data);
			for (sl = pane->slots; sl; sl = sl->next) {
				NemoWindowSlot *slot = NEMO_WINDOW_SLOT (sl->data);
				gchar *uri = nemo_window_slot_get_location_uri (slot);

				if (uri) {
					GList *found = g_list_find_custom (locations, uri, (GCompareFunc) g_strcmp0);

					if (!found) {
						locations = g_list_prepend (locations, uri);
						++locations_size;
					} else {
						g_free (uri);
					}
				}
			}
		}
	}

	locations_array = g_new (gchar*, locations_size + 1);

	for (i = 0, l = locations; l; l = l->next, ++i) {
		/* We reuse the locations string locations saved on list */
		locations_array[i] = l->data;
	}

	locations_array[locations_size] = NULL;

	nemo_freedesktop_dbus_set_open_locations (app->priv->fdb_manager,
		                                      (const gchar**) locations_array);

	g_free (locations_array);
	g_list_free_full (locations, g_free);
}

static void
on_slot_location_changed (NemoWindowSlot *slot,
			  const char         *from,
 			  const char         *to,
			  NemoApplication *application)
{
	update_dbus_opened_locations (application);
}

static void
on_slot_added (NemoWindow      *window,
	       NemoWindowSlot  *slot,
	       NemoApplication *application)
{
	if (nemo_window_slot_get_location (slot)) {
		update_dbus_opened_locations (application);
	}

	g_signal_connect (slot, "location-changed", G_CALLBACK (on_slot_location_changed), application);
}

static void
on_slot_removed (NemoWindow      *window,
		 NemoWindowSlot  *slot,
		 NemoApplication *application)
{
	update_dbus_opened_locations (application);

	g_signal_handlers_disconnect_by_func (slot, on_slot_location_changed, application);
}

static void
nemo_application_window_added (GtkApplication *app,
				   GtkWindow *window)
{
	/* chain to parent */
	GTK_APPLICATION_CLASS (nemo_application_parent_class)->window_added (app, window);

	if (!NEMO_IS_BOOKMARKS_WINDOW (window)) {
		g_signal_connect (window, "slot-added", G_CALLBACK (on_slot_added), app);
		g_signal_connect (window, "slot-removed", G_CALLBACK (on_slot_removed), app);
	}
}

static void
nemo_application_window_removed (GtkApplication *app,
				     GtkWindow *window)
{
	GList *l;

	/* chain to parent */
	GTK_APPLICATION_CLASS (nemo_application_parent_class)->window_removed (app, window);

	/* if this was the last window, close the previewer */
	for (l = gtk_application_get_windows (GTK_APPLICATION (app)); l && !NEMO_IS_WINDOW (l->data); l = l->next);
	if (!l) {
		nemo_previewer_call_close ();
	}

	g_signal_handlers_disconnect_by_func (window, on_slot_added, app);
	g_signal_handlers_disconnect_by_func (window, on_slot_removed, app);
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
	application_class->dbus_register = nemo_application_dbus_register;
	application_class->dbus_unregister = nemo_application_dbus_unregister;

	gtkapp_class = GTK_APPLICATION_CLASS (class);
	gtkapp_class->window_added = nemo_application_window_added;
	gtkapp_class->window_removed = nemo_application_window_removed;

	g_type_class_add_private (class, sizeof (NemoApplicationPriv));
}

NemoApplication *
nemo_application_new (void)
{
	return g_object_new (NEMO_TYPE_APPLICATION,
                         "application-id", "org.Nemo",
                         "flags", G_APPLICATION_HANDLES_OPEN,
                         "inactivity-timeout", 12000,
                         "register-session", TRUE,
                         NULL);
}

void
nemo_application_check_thumbnail_cache (NemoApplication *application)
{
    application->priv->cache_problem = !nemo_thumbnail_factory_check_status ();
}

gboolean
nemo_application_get_cache_bad (NemoApplication *application)
{
    return application->priv->cache_problem;
}

void
nemo_application_clear_cache_flag (NemoApplication *application)
{
    application->priv->cache_problem = FALSE;
}

void
nemo_application_set_cache_flag (NemoApplication *application)
{
    application->priv->cache_problem = TRUE;
}

void
nemo_application_ignore_cache_problem (NemoApplication *application)
{
    application->priv->ignore_cache_problem = TRUE;
}

gboolean
nemo_application_get_cache_problem_ignored (NemoApplication *application)
{
    return application->priv->ignore_cache_problem;
}

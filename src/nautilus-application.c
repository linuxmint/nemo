/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nautilus-application: main Nautilus application class.
 *
 * Copyright (C) 1999, 2000 Red Hat, Inc.
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2010, Cosimo Cecchi <cosimoc@gnome.org>
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
 * Authors: Elliot Lee <sopwith@redhat.com>,
 *          Darin Adler <darin@bentspoon.com>
 *          Cosimo Cecchi <cosimoc@gnome.org>
 *
 */

#include <config.h>

#include "nautilus-application.h"

#include "nautilus-application-actions.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-connect-server-dialog.h"
#include "nautilus-dbus-manager.h"
#include "nautilus-desktop-window.h"
#include "nautilus-freedesktop-dbus.h"
#include "nautilus-image-properties-page.h"
#include "nautilus-previewer.h"
#include "nautilus-progress-ui-handler.h"
#include "nautilus-self-check-functions.h"
#include "nautilus-shell-search-provider.h"
#include "nautilus-window.h"
#include "nautilus-window-private.h"
#include "nautilus-window-slot.h"

#include <libnautilus-private/nautilus-directory-private.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-lib-self-check-functions.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-profile.h>
#include <libnautilus-private/nautilus-signaller.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-extension/nautilus-menu-provider.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_APPLICATION
#include <libnautilus-private/nautilus-debug.h>

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

#define NAUTILUS_ACCEL_MAP_SAVE_DELAY 30

/* The saving of the accelerator map was requested  */
static gboolean save_of_accel_map_requested = FALSE;

G_DEFINE_TYPE (NautilusApplication, nautilus_application, GTK_TYPE_APPLICATION);

struct _NautilusApplicationPriv {
	NautilusProgressUIHandler *progress_handler;
	NautilusDBusManager *dbus_manager;
	NautilusFreedesktopDBus *fdb_manager;

	gboolean desktop_override;

	NotifyNotification *unmount_notify;

	NautilusBookmarkList *bookmark_list;

	GtkWidget *connect_server_window;

	NautilusShellSearchProvider *search_provider;
};

NautilusBookmarkList *
nautilus_application_get_bookmarks (NautilusApplication *application)
{
	if (!application->priv->bookmark_list) {
		application->priv->bookmark_list = nautilus_bookmark_list_new ();
	}

	return application->priv->bookmark_list;
}

void
nautilus_application_edit_bookmarks (NautilusApplication *application,
				     NautilusWindow      *window)
{
	GtkWindow *bookmarks_window;

	bookmarks_window = nautilus_bookmarks_window_new (window);
	gtk_window_present (bookmarks_window);
}

void
nautilus_application_notify_unmount_done (NautilusApplication *application,
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
					      "desktop-entry", g_variant_new_string ("nautilus"));

		notify_notification_show (unplug, NULL);
		g_object_unref (unplug);
		g_strfreev (strings);
	}
}

void
nautilus_application_notify_unmount_show (NautilusApplication *application,
					  const gchar *message)
{
	gchar **strings;

	strings = g_strsplit (message, "\n", 0);

	if (!application->priv->unmount_notify) {
		application->priv->unmount_notify =
			notify_notification_new (strings[0], strings[1],
						 "media-removable");

		notify_notification_set_hint (application->priv->unmount_notify,
					      "desktop-entry", g_variant_new_string ("nautilus"));
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

static gboolean
check_required_directories (NautilusApplication *application)
{
	char *user_directory;
	char *desktop_directory;
	GSList *directories;
	gboolean ret;

	g_assert (NAUTILUS_IS_APPLICATION (application));

	nautilus_profile_start (NULL);

	ret = TRUE;

	user_directory = nautilus_get_user_directory ();
	desktop_directory = nautilus_get_desktop_directory ();

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
	nautilus_profile_end (NULL);

	return ret;
}

static void
menu_provider_items_updated_handler (NautilusMenuProvider *provider, GtkWidget* parent_window, gpointer data)
{

	g_signal_emit_by_name (nautilus_signaller_get_current (),
			       "popup-menu-changed");
}

static void
menu_provider_init_callback (void)
{
        GList *providers;
        GList *l;

        providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);

        for (l = providers; l != NULL; l = l->next) {
                NautilusMenuProvider *provider = NAUTILUS_MENU_PROVIDER (l->data);

		g_signal_connect_after (G_OBJECT (provider), "items-updated",
                           (GCallback)menu_provider_items_updated_handler,
                           NULL);
        }

        nautilus_module_extension_list_free (providers);
}

static void
mark_desktop_files_trusted (void)
{
	char *do_once_file;
	GFile *f, *c;
	GFileEnumerator *e;
	GFileInfo *info;
	const char *name;
	int fd;
	
	do_once_file = g_build_filename (g_get_user_data_dir (),
					 ".converted-launchers", NULL);

	if (g_file_test (do_once_file, G_FILE_TEST_EXISTS)) {
		goto out;
	}

	f = nautilus_get_desktop_location ();
	e = g_file_enumerate_children (f,
				       G_FILE_ATTRIBUTE_STANDARD_TYPE ","
				       G_FILE_ATTRIBUTE_STANDARD_NAME ","
				       G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE
				       ,
				       G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				       NULL, NULL);
	if (e == NULL) {
		goto out2;
	}
	
	while ((info = g_file_enumerator_next_file (e, NULL, NULL)) != NULL) {
		name = g_file_info_get_name (info);
		
		if (g_str_has_suffix (name, ".desktop") &&
		    !g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE)) {
			c = g_file_get_child (f, name);
			nautilus_file_mark_desktop_file_trusted (c,
								 NULL, FALSE,
								 NULL, NULL);
			g_object_unref (c);
		}
		g_object_unref (info);
	}
	
	g_object_unref (e);
 out2:
	fd = g_creat (do_once_file, 0666);
	close (fd);
	
	g_object_unref (f);
 out:	
	g_free (do_once_file);
}

static void
do_upgrades_once (NautilusApplication *self)
{
	char *metafile_dir, *updated, *nautilus_dir, *xdg_dir;
	const gchar *message;
	int fd, res;

	mark_desktop_files_trusted ();

	metafile_dir = g_build_filename (g_get_home_dir (),
					 ".nautilus/metafiles", NULL);
	if (g_file_test (metafile_dir, G_FILE_TEST_IS_DIR)) {
		updated = g_build_filename (metafile_dir, "migrated-to-gvfs", NULL);
		if (!g_file_test (updated, G_FILE_TEST_EXISTS)) {
			g_spawn_command_line_async (LIBEXECDIR"/nautilus-convert-metadata --quiet", NULL);
			fd = g_creat (updated, 0600);
			if (fd != -1) {
				close (fd);
			}
		}
		g_free (updated);
	}
	g_free (metafile_dir);

	nautilus_dir = g_build_filename (g_get_home_dir (),
					 ".nautilus", NULL);
	xdg_dir = nautilus_get_user_directory ();
	if (g_file_test (nautilus_dir, G_FILE_TEST_IS_DIR)) {
		/* test if we already attempted to migrate first */
		updated = g_build_filename (nautilus_dir, "DEPRECATED-DIRECTORY", NULL);
		message = _("Nautilus 3.0 deprecated this directory and tried migrating "
			    "this configuration to ~/.config/nautilus");
		if (!g_file_test (updated, G_FILE_TEST_EXISTS)) {
			/* rename() works fine if the destination directory is
			 * empty.
			 */
			res = g_rename (nautilus_dir, xdg_dir);

			if (res == -1) {
				fd = g_creat (updated, 0600);
				if (fd != -1) {
					res = write (fd, message, strlen (message));
					close (fd);
				}
			}
		}

		g_free (updated);
	}

	g_free (nautilus_dir);
	g_free (xdg_dir);
}

static gboolean
another_navigation_window_already_showing (NautilusApplication *application,
					   NautilusWindow *the_window)
{
	GList *l;
	
	for (l = gtk_application_get_windows (GTK_APPLICATION (application)); l; l = l->next) {
		if (NAUTILUS_IS_WINDOW (l->data) && l->data != the_window) {
			return TRUE;
		}
	}
	
	return FALSE;
}

NautilusWindow *
nautilus_application_create_window (NautilusApplication *application,
				    GdkScreen           *screen)
{
	NautilusWindow *window;
	char *geometry_string;
	gboolean maximized;

	g_return_val_if_fail (NAUTILUS_IS_APPLICATION (application), NULL);
	nautilus_profile_start (NULL);

	window = nautilus_window_new (screen);

	maximized = g_settings_get_boolean
		(nautilus_window_state, NAUTILUS_WINDOW_STATE_MAXIMIZED);
	if (maximized) {
		gtk_window_maximize (GTK_WINDOW (window));
	} else {
		gtk_window_unmaximize (GTK_WINDOW (window));
	}

	geometry_string = g_settings_get_string
		(nautilus_window_state, NAUTILUS_WINDOW_STATE_GEOMETRY);
	if (geometry_string != NULL &&
	    geometry_string[0] != 0) {
		/* Ignore saved window position if a window with the same
		 * location is already showing. That way the two windows
		 * wont appear at the exact same location on the screen.
		 */
		eel_gtk_window_set_initial_geometry_from_string 
			(GTK_WINDOW (window), 
			 geometry_string,
			 NAUTILUS_WINDOW_MIN_WIDTH,
			 NAUTILUS_WINDOW_MIN_HEIGHT,
			 another_navigation_window_already_showing (application, window));
	}
	g_free (geometry_string);

	DEBUG ("Creating a new navigation window");
	nautilus_profile_end (NULL);

	return window;
}

static NautilusWindowSlot *
get_window_slot_for_location (NautilusApplication *application, GFile *location)
{
	NautilusWindowSlot *slot;
	GList *l, *sl;

	slot = NULL;

	if (g_file_query_file_type (location, G_FILE_QUERY_INFO_NONE, NULL) != G_FILE_TYPE_DIRECTORY) {
		location = g_file_get_parent (location);
	} else {
		g_object_ref (location);
	}

	for (l = gtk_application_get_windows (GTK_APPLICATION (application)); l; l = l->next) {
		if (!NAUTILUS_IS_WINDOW (l->data) || NAUTILUS_IS_DESKTOP_WINDOW (l->data))
			continue;

		for (sl = nautilus_window_get_slots (NAUTILUS_WINDOW (l->data)); sl; sl = sl->next) {
			NautilusWindowSlot *current = NAUTILUS_WINDOW_SLOT (sl->data);
			GFile *slot_location = nautilus_window_slot_get_location (current);

			if (slot_location && g_file_equal (slot_location, location)) {
				slot = current;
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
open_window (NautilusApplication *application,
	     GFile *location)
{
	NautilusWindow *window;

	nautilus_profile_start (NULL);
	window = nautilus_application_create_window (application, gdk_screen_get_default ());

	if (location != NULL) {
		nautilus_window_go_to (window, location);
	} else {
		nautilus_window_slot_go_home (nautilus_window_get_active_slot (window), 0);
	}

	nautilus_profile_end (NULL);
}

void
nautilus_application_open_location (NautilusApplication *application,
				    GFile *location,
				    GFile *selection,
				    const char *startup_id)
{
	NautilusWindow *window;
	NautilusWindowSlot *slot;
	GList *sel_list = NULL;

	nautilus_profile_start (NULL);

	slot = get_window_slot_for_location (application, location);

	if (!slot) {
		window = nautilus_application_create_window (application, gdk_screen_get_default ());
		slot = nautilus_window_get_active_slot (window);
	} else {
		window = nautilus_window_slot_get_window (slot);
		nautilus_window_set_active_slot (window, slot);
		gtk_window_present (GTK_WINDOW (window));
	}

	if (selection != NULL) {
		sel_list = g_list_prepend (sel_list, nautilus_file_get (selection));
	}

	gtk_window_set_startup_id (GTK_WINDOW (window), startup_id);
	nautilus_window_slot_open_location_full (slot, location, 0, sel_list, NULL, NULL);

	if (sel_list != NULL) {
		nautilus_file_list_free (sel_list);
	}

	nautilus_profile_end (NULL);
}

static void
nautilus_application_open (GApplication *app,
			   GFile **files,
			   gint n_files,
			   const gchar *hint)
{
	NautilusApplication *self = NAUTILUS_APPLICATION (app);
	gboolean force_new = (g_strcmp0 (hint, "new-window") == 0);
	NautilusWindowSlot *slot = NULL;
	NautilusWindow *window;
	GFile *file;
	gint idx;

	DEBUG ("Open called on the GApplication instance; %d files", n_files);

	/* Open windows at each requested location. */
	for (idx = 0; idx < n_files; idx++) {
		file = files[idx];

		if (!force_new) {
			slot = get_window_slot_for_location (self, file);
		}

		if (!slot) {
			open_window (self, file);
		} else {
			/* We open the location again to update any possible selection */
			nautilus_window_slot_open_location (slot, file, 0);

			window = nautilus_window_slot_get_window (slot);
			nautilus_window_set_active_slot (window, slot);
			gtk_window_present (GTK_WINDOW (window));
		}
	}
}

static gboolean
go_to_server_cb (NautilusWindow *window,
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
		NautilusFile *file;
		gboolean safe_to_save = TRUE;

		file = nautilus_file_get_existing (location);

		bookmarks = g_bookmark_file_new ();
		datadir = g_build_filename (g_get_user_config_dir (), "nautilus", NULL);
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
			uri = nautilus_file_get_uri (file);
			title = nautilus_file_get_display_name (file);
			g_bookmark_file_set_title (bookmarks, uri, title);
			g_bookmark_file_set_visited (bookmarks, uri, -1);
			g_bookmark_file_add_application (bookmarks, uri, NULL, NULL);
			g_free (uri);
			g_free (title);

			g_bookmark_file_to_file (bookmarks, filename, NULL);
		}

		nautilus_file_unref (file);
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
		NautilusWindow *window = NAUTILUS_WINDOW (gtk_application_get_active_window (application));

		location = nautilus_connect_server_dialog_get_location (NAUTILUS_CONNECT_SERVER_DIALOG (dialog));
		if (location != NULL) {
			nautilus_window_slot_open_location_full (nautilus_window_get_active_slot (window),
								 location,
								 NAUTILUS_WINDOW_OPEN_FLAG_USE_DEFAULT_LOCATION,
								 NULL, go_to_server_cb, application);
		}
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

GtkWidget *
nautilus_application_connect_server (NautilusApplication *application,
				     NautilusWindow      *window)
{
	GtkWidget *dialog;

	dialog = application->priv->connect_server_window;

	if (dialog == NULL) {
		dialog = nautilus_connect_server_dialog_new (window);
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
nautilus_application_finalize (GObject *object)
{
	NautilusApplication *application;

	application = NAUTILUS_APPLICATION (object);

	g_clear_object (&application->priv->progress_handler);
	g_clear_object (&application->priv->bookmark_list);

	g_clear_object (&application->priv->dbus_manager);
	g_clear_object (&application->priv->fdb_manager);
	g_clear_object (&application->priv->search_provider);

	notify_uninit ();

        G_OBJECT_CLASS (nautilus_application_parent_class)->finalize (object);
}

static gboolean
do_cmdline_sanity_checks (NautilusApplication *self,
			  GVariantDict        *options)
{
	gboolean retval = FALSE;

	if (g_variant_dict_contains (options, "check") &&
	    (g_variant_dict_contains (options, G_OPTION_REMAINING) ||
	     g_variant_dict_contains (options, "quit"))) {
		g_printerr ("%s\n",
			    _("--check cannot be used with other options."));
		goto out;
	}

	if (g_variant_dict_contains (options, "quit") &&
	    g_variant_dict_contains (options, G_OPTION_REMAINING)) {
		g_printerr ("%s\n",
			    _("--quit cannot be used with URIs."));
		goto out;
	}


	if (g_variant_dict_contains (options, "select") &&
	    !g_variant_dict_contains (options, G_OPTION_REMAINING)) {
		g_printerr ("%s\n",
			    _("--select must be used with at least an URI."));
		goto out;
	}

	if (g_variant_dict_contains (options, "force-desktop") &&
	    g_variant_dict_contains (options, "no-desktop")) {
		g_printerr ("%s\n",
			    _("--no-desktop and --force-desktop cannot be used together."));
		goto out;
	}

	retval = TRUE;

 out:
	return retval;
}

static int
do_perform_self_checks (void)
{
#ifndef NAUTILUS_OMIT_SELF_CHECK
	gtk_init (NULL, NULL);

	nautilus_profile_start (NULL);
	/* Run the checks (each twice) for nautilus and libnautilus-private. */

	nautilus_run_self_checks ();
	nautilus_run_lib_self_checks ();
	eel_exit_if_self_checks_failed ();

	nautilus_run_self_checks ();
	nautilus_run_lib_self_checks ();
	eel_exit_if_self_checks_failed ();
	nautilus_profile_end (NULL);
#endif

	return EXIT_SUCCESS;
}

static void
select_items_ready_cb (GObject *source,
		       GAsyncResult *res,
		       gpointer user_data)
{
	GDBusConnection *connection = G_DBUS_CONNECTION (source);
	NautilusApplication *self = user_data;
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
nautilus_application_select (NautilusApplication *self,
			     GFile **files,
			     gint len)
{
	GDBusConnection *connection = g_application_get_dbus_connection (G_APPLICATION (self));
	GVariantBuilder builder;
	gint idx;
	gchar *uri;

	g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
	for (idx = 0; idx < len; idx++) {
		uri = g_file_get_uri (files[idx]);
		g_variant_builder_add (&builder, "s", uri);
		g_free (uri);
	}

	g_dbus_connection_call (connection,
				NAUTILUS_FDO_DBUS_NAME,
				NAUTILUS_FDO_DBUS_PATH,
				NAUTILUS_FDO_DBUS_IFACE,
				"ShowItems",
				g_variant_new ("(ass)", &builder, ""), NULL,
				G_DBUS_CALL_FLAGS_NONE, G_MAXINT, NULL,
				select_items_ready_cb, self);

	g_variant_builder_clear (&builder);
}

const GOptionEntry options[] = {
#ifndef NAUTILUS_OMIT_SELF_CHECK
	{ "check", 'c', 0, G_OPTION_ARG_NONE, NULL,
	  N_("Perform a quick set of self-check tests."), NULL },
#endif
	/* dummy, only for compatibility reasons */
	{ "browser", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, NULL,
	  NULL, NULL },
	/* ditto */
	{ "geometry", 'g', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, NULL,
	  N_("Create the initial window with the given geometry."), N_("GEOMETRY") },
	{ "version", '\0', 0, G_OPTION_ARG_NONE, NULL,
	  N_("Show the version of the program."), NULL },
	{ "new-window", 'w', 0, G_OPTION_ARG_NONE, NULL,
	  N_("Always open a new window for browsing specified URIs"), NULL },
	{ "no-default-window", 'n', 0, G_OPTION_ARG_NONE, NULL,
	  N_("Only create windows for explicitly specified URIs."), NULL },
	{ "no-desktop", '\0', 0, G_OPTION_ARG_NONE, NULL,
	  N_("Never manage the desktop (ignore the GSettings preference)."), NULL },
	{ "force-desktop", '\0', 0, G_OPTION_ARG_NONE, NULL,
	  N_("Always manage the desktop (ignore the GSettings preference)."), NULL },
	{ "quit", 'q', 0, G_OPTION_ARG_NONE, NULL,
	  N_("Quit Nautilus."), NULL },
	{ "select", 's', 0, G_OPTION_ARG_NONE, NULL,
	  N_("Select specified URI in parent folder."), NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, NULL, NULL,  N_("[URI...]") },

	{ NULL }
};

static gint
nautilus_application_handle_file_args (NautilusApplication *self,
				       GVariantDict        *options)
{
	GFile **files;
	GFile *file;
	gint idx, len;
	const gchar * const *remaining = NULL;
	GPtrArray *file_array;

	g_variant_dict_lookup (options, G_OPTION_REMAINING, "^a&s", &remaining);

	/* Convert args to GFiles */
	file_array = g_ptr_array_new_full (0, g_object_unref);

	if (remaining) {
		for (idx = 0; remaining[idx] != NULL; idx++) {
			file = g_file_new_for_commandline_arg (remaining[idx]);
			g_ptr_array_add (file_array, file);
		}
	} else if (g_variant_dict_contains (options, "new-window")) {
		file = g_file_new_for_path (g_get_home_dir ());
		g_ptr_array_add (file_array, file);
	} else {
		/* No options or options that glib already manages */
		return -1;
	}

	len = file_array->len;
	files = (GFile **) file_array->pdata;

	if (g_variant_dict_contains (options, "select")) {
		nautilus_application_select (self, files, len);
	} else {
		/* Invoke "Open" to create new windows */
		g_application_open (G_APPLICATION (self), files, len,
				    g_variant_dict_contains (options, "new-window") ? "new-window" : "");
	}

	g_ptr_array_unref (file_array);

	return EXIT_SUCCESS;
}

static gint
nautilus_application_handle_local_options (GApplication *application,
					   GVariantDict *options)
{
	NautilusApplication *self = NAUTILUS_APPLICATION (application);
	gint retval = -1;
	GError *error = NULL;

	nautilus_profile_start (NULL);

	if (g_variant_dict_contains (options, "version")) {
		g_print ("GNOME nautilus " PACKAGE_VERSION "\n");
		retval = EXIT_SUCCESS;
		goto out;
	}

	if (!do_cmdline_sanity_checks (self, options)) {
		retval = EXIT_FAILURE;
		goto out;
	}

	if (g_variant_dict_contains (options, "check")) {
		retval = do_perform_self_checks ();
		goto out;
	}

	g_application_register (application, NULL, &error);

	if (error != NULL) {
		/* Translators: this is a fatal error quit message printed on the
		 * command line */
		g_printerr ("%s: %s\n", _("Could not register the application"), error->message);
		g_error_free (error);

		retval = EXIT_FAILURE;
		goto out;
	}

	if (g_variant_dict_contains (options, "quit")) {
		DEBUG ("Killing application, as requested");
		g_action_group_activate_action (G_ACTION_GROUP (application),
						"kill", NULL);
		goto out;
	}

	if (g_variant_dict_contains (options, "force-desktop")) {
		DEBUG ("Forcing desktop, as requested");
		self->priv->desktop_override = TRUE;
		g_action_group_activate_action (G_ACTION_GROUP (application),
						"open-desktop", NULL);
	} else if (g_variant_dict_contains (options, "no-desktop")) {
		DEBUG ("Forcing desktop off, as requested");
		self->priv->desktop_override = TRUE;
		g_action_group_activate_action (G_ACTION_GROUP (application),
						"close-desktop", NULL);
	}  else if (g_variant_dict_contains (options, "no-default-window")) {
		/* We want to avoid trigering the activate signal; so no window is created.
		 * GApplication doesn't call activate if we return a value >= 0.
		 * Use EXIT_SUCCESS since is >= 0. */
		retval = EXIT_SUCCESS;
		goto out;
	}

	retval = nautilus_application_handle_file_args (self, options);

 out:
	nautilus_profile_end (NULL);

	return retval;
}

static void
nautilus_application_activate (GApplication *app)
{
	NautilusApplication *self = NAUTILUS_APPLICATION (app);
	GFile **files;

	DEBUG ("Calling activate");

	files = g_malloc0 (2 * sizeof (GFile *));
	files[0] = g_file_new_for_path (g_get_home_dir ());
	g_application_open (app, files, 1, "new-window");

	g_object_unref (files[0]);
	g_free (files);
}

static void
nautilus_application_init (NautilusApplication *application)
{
	application->priv =
		G_TYPE_INSTANCE_GET_PRIVATE (application, NAUTILUS_TYPE_APPLICATION,
					     NautilusApplicationPriv);

	g_application_add_main_option_entries (G_APPLICATION (application), options);
}

static void
init_icons_and_styles (void)
{
	/* initialize search path for custom icons */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   NAUTILUS_DATADIR G_DIR_SEPARATOR_S "icons");
}

static void
nautilus_application_set_desktop_visible (NautilusApplication *self,
					  gboolean             visible)
{
	const gchar *action_name;

	action_name = visible ? "open-desktop" : "close-desktop";
	g_action_group_activate_action (G_ACTION_GROUP (self),
					action_name, NULL);
}

static void
update_desktop_from_gsettings (NautilusApplication *self)
{
	/* desktop GSetting was overridden - don't do anything */
	if (self->priv->desktop_override) {
		return;
	}

	nautilus_application_set_desktop_visible (self, g_settings_get_boolean (gnome_background_preferences,
										NAUTILUS_PREFERENCES_SHOW_DESKTOP));
}

static void
init_desktop (NautilusApplication *self)
{
	update_desktop_from_gsettings (self);
	g_signal_connect_swapped (gnome_background_preferences, "changed::" NAUTILUS_PREFERENCES_SHOW_DESKTOP,
				  G_CALLBACK (update_desktop_from_gsettings),
				  self);
}

static gboolean 
nautilus_application_save_accel_map (gpointer data)
{
	if (save_of_accel_map_requested) {
		char *accel_map_filename;
	 	accel_map_filename = nautilus_get_accel_map_file ();
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
		g_timeout_add_seconds (NAUTILUS_ACCEL_MAP_SAVE_DELAY, 
				nautilus_application_save_accel_map, NULL);
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
		tmp = nautilus_get_scripts_directory_path ();
		tmp2 = nautilus_escape_action_name (tmp, "");
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

	accel_map_filename = nautilus_get_accel_map_file ();

	/* If the accel map file doesn't exist, try to migrate from
	 * former locations. */
	if (!g_file_test (accel_map_filename, G_FILE_TEST_IS_REGULAR)) {
		char *old_accel_map_filename;
		const gchar *override;

		override = g_getenv ("GNOME22_USER_DIR");
		if (override) {
			old_accel_map_filename = g_build_filename (override,
					"accels", "nautilus", NULL);
			old_scripts_directory_path = g_build_filename (override,
				       "nautilus-scripts",
				       NULL);
		} else {
			old_accel_map_filename = g_build_filename (g_get_home_dir (),
					".gnome2", "accels", "nautilus", NULL);
			old_scripts_directory_path = g_build_filename (g_get_home_dir (),
								       ".gnome2",
								       "nautilus-scripts",
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

		old_scripts_location_esc = nautilus_escape_action_name (old_scripts_directory_path, "");
		gtk_accel_map_foreach (old_scripts_location_esc, update_accel_due_to_scripts_migration);
		g_free (old_scripts_location_esc);
		/* save map immediately */
		save_of_accel_map_requested = TRUE;
		nautilus_application_save_accel_map (NULL);
	}

	g_signal_connect (gtk_accel_map_get (), "changed",
			  G_CALLBACK (queue_accel_map_save_callback), NULL);

	g_free (old_scripts_directory_path);
}

static void
theme_changed (GtkSettings *settings)
{
	static GtkCssProvider *provider = NULL;
	gchar *theme;
	GdkScreen *screen;

	g_object_get (settings, "gtk-theme-name", &theme, NULL);
	screen = gdk_screen_get_default ();

	if (g_str_equal (theme, "Adwaita"))
	{
		if (provider == NULL)
		{
			GFile *file;

			provider = gtk_css_provider_new ();
			file = g_file_new_for_uri ("resource:///org/gnome/nautilus/Adwaita.css");
			gtk_css_provider_load_from_file (provider, file, NULL);
			g_object_unref (file);
		}

		gtk_style_context_add_provider_for_screen (screen,
							   GTK_STYLE_PROVIDER (provider),
							   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
	else if (provider != NULL)
	{
		gtk_style_context_remove_provider_for_screen (screen,
							      GTK_STYLE_PROVIDER (provider));
		g_clear_object (&provider);
	}

	g_free (theme);
}

static void
setup_theme_extensions (void)
{
	GtkSettings *settings;

	/* Set up a handler to load our custom css for Adwaita.
	 * See https://bugzilla.gnome.org/show_bug.cgi?id=732959
	 * for a more automatic solution that is still under discussion.
	 */
	settings = gtk_settings_get_default ();
	g_signal_connect (settings, "notify::gtk-theme-name", G_CALLBACK (theme_changed), NULL);
	theme_changed (settings);
}

static void
nautilus_application_startup (GApplication *app)
{
	NautilusApplication *self = NAUTILUS_APPLICATION (app);

	nautilus_profile_start (NULL);

	/* chain up to the GTK+ implementation early, so gtk_init()
	 * is called for us.
	 */
	G_APPLICATION_CLASS (nautilus_application_parent_class)->startup (app);

	gtk_window_set_default_icon_name ("system-file-manager");

	setup_theme_extensions ();

	/* create DBus manager */
	self->priv->fdb_manager = nautilus_freedesktop_dbus_new ();

	/* initialize preferences and create the global GSettings objects */
	nautilus_global_preferences_init ();

	/* register property pages */
	nautilus_image_properties_page_register ();

	/* initialize theming */
	init_icons_and_styles ();
	init_gtk_accels ();
	
	/* initialize nautilus modules */
	nautilus_profile_start ("Modules");
	nautilus_module_setup ();
	nautilus_profile_end ("Modules");

	/* attach menu-provider module callback */
	menu_provider_init_callback ();
	
	/* Initialize the UI handler singleton for file operations */
	notify_init (GETTEXT_PACKAGE);
	self->priv->progress_handler = nautilus_progress_ui_handler_new ();

	/* Check the user's .nautilus directories and post warnings
	 * if there are problems.
	 */
	check_required_directories (self);

	do_upgrades_once (self);

	nautilus_init_application_actions (self);
	init_desktop (self);

	nautilus_profile_end (NULL);
}

static gboolean
nautilus_application_dbus_register (GApplication	 *app,
				    GDBusConnection      *connection,
				    const gchar		 *object_path,
				    GError		**error)
{
	NautilusApplication *self = NAUTILUS_APPLICATION (app);

	self->priv->dbus_manager = nautilus_dbus_manager_new ();
	if (!nautilus_dbus_manager_register (self->priv->dbus_manager, connection, error)) {
		return FALSE;
	}

	self->priv->search_provider = nautilus_shell_search_provider_new ();
	if (!nautilus_shell_search_provider_register (self->priv->search_provider, connection, error)) {
		return FALSE;
	}

	return TRUE;
}

static void
nautilus_application_dbus_unregister (GApplication	*app,
				      GDBusConnection   *connection,
				      const gchar	*object_path)
{
	NautilusApplication *self = NAUTILUS_APPLICATION (app);

	if (self->priv->dbus_manager) {
		nautilus_dbus_manager_unregister (self->priv->dbus_manager);
	}

	if (self->priv->search_provider) {
		nautilus_shell_search_provider_unregister (self->priv->search_provider);
	}
}

static void
nautilus_application_quit_mainloop (GApplication *app)
{
	DEBUG ("Quitting mainloop");

	nautilus_icon_info_clear_caches ();
 	nautilus_application_save_accel_map (NULL);

	nautilus_application_notify_unmount_done (NAUTILUS_APPLICATION (app), NULL);

	G_APPLICATION_CLASS (nautilus_application_parent_class)->quit_mainloop (app);
}

static void
update_dbus_opened_locations (NautilusApplication *app)
{
	gint i;
	GList *l, *sl;
	GList *locations = NULL;
	gsize locations_size = 0;
	gchar **locations_array;

	g_return_if_fail (NAUTILUS_IS_APPLICATION (app));

	for (l = gtk_application_get_windows (GTK_APPLICATION (app)); l; l = l->next) {
		if (!NAUTILUS_IS_WINDOW (l->data) || NAUTILUS_IS_DESKTOP_WINDOW (l->data))
			continue;

		for (sl = nautilus_window_get_slots (NAUTILUS_WINDOW (l->data)); sl; sl = sl->next) {
			NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (sl->data);
			gchar *uri = nautilus_window_slot_get_location_uri (slot);

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

	locations_array = g_new (gchar*, locations_size + 1);

	for (i = 0, l = locations; l; l = l->next, ++i) {
		/* We reuse the locations string locations saved on list */
		locations_array[i] = l->data;
	}

	locations_array[locations_size] = NULL;

	nautilus_freedesktop_dbus_set_open_locations (app->priv->fdb_manager,
		                                      (const gchar**) locations_array);

	g_free (locations_array);
	g_list_free_full (locations, g_free);
}

static void
on_slot_location_changed (NautilusWindowSlot *slot,
			  const char         *from,
 			  const char         *to,
			  NautilusApplication *application)
{
	update_dbus_opened_locations (application);
}

static void
on_slot_added (NautilusWindow      *window,
	       NautilusWindowSlot  *slot,
	       NautilusApplication *application)
{
	if (nautilus_window_slot_get_location (slot)) {
		update_dbus_opened_locations (application);
	}

	g_signal_connect (slot, "location-changed", G_CALLBACK (on_slot_location_changed), application);
}

static void
on_slot_removed (NautilusWindow      *window,
		 NautilusWindowSlot  *slot,
		 NautilusApplication *application)
{
	update_dbus_opened_locations (application);

	g_signal_handlers_disconnect_by_func (slot, on_slot_location_changed, application);
}

static void
nautilus_application_window_added (GtkApplication *app,
				   GtkWindow *window)
{
	/* chain to parent */
	GTK_APPLICATION_CLASS (nautilus_application_parent_class)->window_added (app, window);

	g_signal_connect (window, "slot-added", G_CALLBACK (on_slot_added), app);
	g_signal_connect (window, "slot-removed", G_CALLBACK (on_slot_removed), app);
}

static void
nautilus_application_window_removed (GtkApplication *app,
				     GtkWindow *window)
{
	GList *l;

	/* chain to parent */
	GTK_APPLICATION_CLASS (nautilus_application_parent_class)->window_removed (app, window);

	/* if this was the last window, close the previewer */
	for (l = gtk_application_get_windows (GTK_APPLICATION (app)); l && !NAUTILUS_IS_WINDOW (l->data); l = l->next);
	if (!l) {
		nautilus_previewer_call_close ();
	}

	g_signal_handlers_disconnect_by_func (window, on_slot_added, app);
	g_signal_handlers_disconnect_by_func (window, on_slot_removed, app);
}

static void
nautilus_application_class_init (NautilusApplicationClass *class)
{
        GObjectClass *object_class;
	GApplicationClass *application_class;
	GtkApplicationClass *gtkapp_class;

        object_class = G_OBJECT_CLASS (class);
        object_class->finalize = nautilus_application_finalize;

	application_class = G_APPLICATION_CLASS (class);
	application_class->startup = nautilus_application_startup;
	application_class->activate = nautilus_application_activate;
	application_class->quit_mainloop = nautilus_application_quit_mainloop;
	application_class->open = nautilus_application_open;
	application_class->dbus_register = nautilus_application_dbus_register;
	application_class->dbus_unregister = nautilus_application_dbus_unregister;
	application_class->handle_local_options = nautilus_application_handle_local_options;

	gtkapp_class = GTK_APPLICATION_CLASS (class);
	gtkapp_class->window_added = nautilus_application_window_added;
	gtkapp_class->window_removed = nautilus_application_window_removed;

	g_type_class_add_private (class, sizeof (NautilusApplicationPriv));
}

NautilusApplication *
nautilus_application_new (void)
{
	return g_object_new (NAUTILUS_TYPE_APPLICATION,
			     "application-id", "org.gnome.Nautilus",
			     "flags", G_APPLICATION_HANDLES_OPEN,
			     "inactivity-timeout", 12000,
			     "register-session", TRUE,
			     NULL);
}

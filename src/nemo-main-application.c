/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nemo-main-application: Nemo application subclass for standard windows
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
 */

#include <config.h>

#include "nemo-main-application.h"

#if ENABLE_EMPTY_VIEW
#include "nemo-empty-view.h"
#endif /* ENABLE_EMPTY_VIEW */

#include "nemo-freedesktop-dbus.h"
#include "nemo-icon-view.h"
#include "nemo-image-properties-page.h"
#include "nemo-list-view.h"
#include "nemo-previewer.h"
#include "nemo-progress-ui-handler.h"
#include "nemo-self-check-functions.h"
#include "nemo-window.h"
#include "nemo-window-bookmarks.h"
#include "nemo-window-manage-views.h"
#include "nemo-window-private.h"
#include "nemo-window-slot.h"
#include "nemo-statusbar.h"

#include <libnemo-private/nemo-dbus-manager.h>
#include <libnemo-private/nemo-directory-private.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-file-operations.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-lib-self-check-functions.h>
#include <libnemo-private/nemo-module.h>
#include <libnemo-private/nemo-signaller.h>
#include <libnemo-private/nemo-ui-utilities.h>
#include <libnemo-private/nemo-undo-manager.h>
#include <libnemo-private/nemo-thumbnails.h>
#include <libnemo-extension/nemo-menu-provider.h>

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

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <libcinnamon-desktop/gnome-desktop-thumbnail.h>

/* Keep window from shrinking down ridiculously small; numbers are somewhat arbitrary */
#define APPLICATION_WINDOW_MIN_WIDTH	300
#define APPLICATION_WINDOW_MIN_HEIGHT	100

#define START_STATE_CONFIG "start-state"

#define NEMO_ACCEL_MAP_SAVE_DELAY 30

static void     mount_removed_callback            (GVolumeMonitor            *monitor,
						   GMount                    *mount,
						   NemoMainApplication       *application);
static void     mount_added_callback              (GVolumeMonitor            *monitor,
						   GMount                    *mount,
						   NemoMainApplication       *application);

G_DEFINE_TYPE (NemoMainApplication, nemo_main_application, NEMO_TYPE_APPLICATION);

struct _NemoMainApplicationPriv {
	GVolumeMonitor *volume_monitor;

	NemoDBusManager *dbus_manager;
	NemoFreedesktopDBus *fdb_manager;

	gchar *geometry;

    NotifyNotification *unmount_notify;
};

static void
nemo_main_application_notify_unmount_done (NemoApplication *application,
                                           const gchar     *message)
{
    NemoMainApplication *app = NEMO_MAIN_APPLICATION (application);

    if (app->priv->unmount_notify) {
        notify_notification_close (app->priv->unmount_notify, NULL);
        g_clear_object (&app->priv->unmount_notify);
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

static void
nemo_main_application_notify_unmount_show (NemoApplication *application,
                                           const gchar     *message)
{
    NemoMainApplication *app = NEMO_MAIN_APPLICATION (application);

    gchar **strings;

    strings = g_strsplit (message, "\n", 0);

    if (!app->priv->unmount_notify) {
        app->priv->unmount_notify = notify_notification_new (strings[0], strings[1],
                                                             "media-removable");

        notify_notification_set_hint (app->priv->unmount_notify,
                                      "transient", g_variant_new_boolean (TRUE));
        notify_notification_set_urgency (app->priv->unmount_notify,
                                         NOTIFY_URGENCY_CRITICAL);
    } else {
        notify_notification_update (app->priv->unmount_notify,
                                    strings[0], strings[1],
                                    "media-removable");
    }

    notify_notification_show (app->priv->unmount_notify, NULL);
    g_strfreev (strings);
}

static void
nemo_main_application_close_all_windows (NemoApplication *self)
{
	GList *list_copy;
	GList *l;
	
	list_copy = g_list_copy (gtk_application_get_windows (GTK_APPLICATION (self)));
	for (l = list_copy; l != NULL; l = l->next) {
		if (NEMO_IS_WINDOW (l->data)) {
			NemoWindow *window;

			window = NEMO_WINDOW (l->data);
			nemo_window_close (window);
		}
	}
	g_list_free (list_copy);
}

static NemoWindow *
nemo_main_application_create_window (NemoApplication *application,
                                     GdkScreen       *screen)
{
	NemoWindow *window;
	char *geometry_string;
	gboolean maximized;

	g_return_val_if_fail (NEMO_IS_APPLICATION (application), NULL);

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
			 TRUE);
	}
	g_free (geometry_string);

    nemo_undo_manager_attach (application->undo_manager, G_OBJECT (window));

	DEBUG ("Creating a new navigation window");
	
	return window;
}

static void
mount_added_callback (GVolumeMonitor *monitor,
		      GMount *mount,
		      NemoMainApplication *application)
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

/* Called whenever a mount is unmounted. Check and see if there are
 * any windows open displaying contents on the mount. If there are,
 * close them.  It would also be cool to save open window and position
 * info.
 */
static void
mount_removed_callback (GVolumeMonitor *monitor,
			GMount *mount,
			NemoMainApplication *application)
{
	GList *window_list, *node, *close_list;
	NemoWindow *window;
	NemoWindowSlot *slot;
	NemoWindowSlot *force_no_close_slot;
	GFile *root, *computer;
	gchar *uri;
	guint n_slots;

	close_list = NULL;
	force_no_close_slot = NULL;
	n_slots = 0;

	/* Check and see if any of the open windows are displaying contents from the unmounted mount */
	window_list = gtk_application_get_windows (GTK_APPLICATION (application));

	root = g_mount_get_root (mount);
	uri = g_file_get_uri (root);

	DEBUG ("Removed mount at uri %s", uri);
	g_free (uri);

	/* Construct a list of windows to be closed. Do not add the non-closable windows to the list. */
	for (node = window_list; node != NULL; node = node->next) {
		window = NEMO_WINDOW (node->data);
		if (window != NULL) {
			GList *l;
			GList *lp;

			for (lp = window->details->panes; lp != NULL; lp = lp->next) {
				NemoWindowPane *pane;
				pane = (NemoWindowPane*) lp->data;
				for (l = pane->slots; l != NULL; l = l->next) {
					slot = l->data;
					n_slots++;
					if (nemo_window_slot_should_close_with_mount (slot, mount)) {
						close_list = g_list_prepend (close_list, slot);
					}
				} /* for all slots */
			} /* for all panes */
		}
	}

	/* Handle the windows in the close list. */
	for (node = close_list; node != NULL; node = node->next) {
		slot = node->data;

		if (slot != force_no_close_slot) {
            if (g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_CLOSE_DEVICE_VIEW_ON_EJECT))
                nemo_window_pane_close_slot (slot->pane, slot);
            else
                nemo_window_slot_go_home (slot, FALSE);
		} else {
			computer = g_file_new_for_path (g_get_home_dir ());
			nemo_window_slot_open_location (slot, computer, 0);
			g_object_unref(computer);
		}
	}

	g_list_free (close_list);
}

static void
open_window (NemoMainApplication *application,
	     GFile *location, GdkScreen *screen, const char *geometry)
{
	NemoWindow *window;
	gchar *uri;
	gboolean have_geometry;

	uri = g_file_get_uri (location);
	DEBUG ("Opening new window at uri %s", uri);

	window = nemo_main_application_create_window (application,
						     screen);
	nemo_window_go_to (window, location);

	have_geometry = geometry != NULL && strcmp(geometry, "") != 0;

	if (have_geometry && !gtk_widget_get_visible (GTK_WIDGET (window))) {
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

	g_free (uri);
}

static void
open_windows (NemoMainApplication *application,
	      GFile **files,
	      gint n_files,
	      GdkScreen *screen,
	      const char *geometry)
{
	gint i;

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

static void
nemo_main_application_open_location (NemoApplication     *application,
                                     GFile               *location,
                                     GFile               *selection,
                                     const char          *startup_id)
{
	NemoWindow *window;
	GList *sel_list = NULL;

	window = nemo_main_application_create_window (application, gdk_screen_get_default ());
	gtk_window_set_startup_id (GTK_WINDOW (window), startup_id);

	if (selection != NULL) {
		sel_list = g_list_prepend (sel_list, nemo_file_get (selection));
	}

	nemo_window_slot_open_location_full (nemo_window_get_active_slot (window), location,
						 0, sel_list, NULL, NULL);

	if (sel_list != NULL) {
		nemo_file_list_free (sel_list);
	}
}

static void
nemo_main_application_open (GApplication *app,
                            GFile       **files,
                            gint          n_files,
                            const gchar  *geometry)
{
	NemoMainApplication *self = NEMO_MAIN_APPLICATION (app);

	DEBUG ("Open called on the GApplication instance; %d files", n_files);

	open_windows (self, files, n_files, gdk_screen_get_default (), geometry);
}

static void
nemo_main_application_init (NemoMainApplication *application)
{
    application->priv = G_TYPE_INSTANCE_GET_PRIVATE (application,
                                                     NEMO_TYPE_MAIN_APPLICATION,
                                                     NemoMainApplicationPriv);
}

static void
nemo_main_application_finalize (GObject *object)
{
    NemoMainApplication *application;

    application = NEMO_MAIN_APPLICATION (object);

    nemo_bookmarks_exiting ();

    g_clear_object (&application->priv->volume_monitor);
    g_free (application->priv->geometry);

    g_clear_object (&application->priv->dbus_manager);
    g_clear_object (&application->priv->fdb_manager);

    G_OBJECT_CLASS (nemo_main_application_parent_class)->finalize (object);
}

static gboolean
do_cmdline_sanity_checks (NemoMainApplication *self,
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
	/* Run the checks (each twice) for nemo and libnemo-private. */

	nemo_run_self_checks ();
	nemo_run_lib_self_checks ();
	eel_exit_if_self_checks_failed ();

	nemo_run_self_checks ();
	nemo_run_lib_self_checks ();
	eel_exit_if_self_checks_failed ();
#endif

	*exit_status = EXIT_SUCCESS;
}

static gboolean
nemo_main_application_local_command_line (GApplication *application,
					 gchar ***arguments,
					 gint *exit_status)
{
	gboolean perform_self_check = FALSE;
	gboolean version = FALSE;
	gboolean browser = FALSE;
	gboolean kill_shell = FALSE;
	gboolean no_default_window = FALSE;
    gboolean no_desktop_ignored = FALSE;
	gboolean fix_cache = FALSE;
	gchar **remaining = NULL;
    GApplicationFlags init_flags;
	NemoMainApplication *self = NEMO_MAIN_APPLICATION (application);

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
        { "no-desktop", '\0', 0, G_OPTION_ARG_NONE, &no_desktop_ignored,
          N_("Ignored argument - left for compatibility only."), NULL },
		{ "fix-cache", '\0', 0, G_OPTION_ARG_NONE, &fix_cache,
		  N_("Repair the user thumbnail cache - this can be useful if you're having trouble with file thumbnails.  Must be run as root"), NULL },
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

	context = g_option_context_new (_("\n\nBrowse the file system with the file manager"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));

	argv = *arguments;
	argc = g_strv_length (argv);

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr ("Could not parse arguments: %s\n", error->message);
		g_error_free (error);

		*exit_status = EXIT_FAILURE;
		goto out;
	}

	if (version) {
		g_print ("nemo " VERSION "\n");
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

    if (fix_cache) {
        if (geteuid () != 0) {
            g_printerr ("The --fix-cache option must be run with sudo or as the root user.\n");
        } else {
            gnome_desktop_thumbnail_cache_fix_permissions ();
            g_print ("User thumbnail cache successfully repaired.\n");
        }

        goto out;
    }

	DEBUG ("Parsing local command line, no_default_window %d, quit %d, "
	       "self checks %d",
	       no_default_window, kill_shell, perform_self_check);

    /* Keep our original flags handy */
    init_flags = g_application_get_flags (application);

    /* First try to register as a service (this allows our dbus activation to succeed
     * if we're not already running */
    g_application_set_flags (application, init_flags | G_APPLICATION_IS_SERVICE);
    g_application_register (application, NULL, &error);

	if (error != NULL) {
        g_debug ("Could not register nemo as a service, trying as a remote: %s", error->message);
        g_clear_error (&error);
    } else {
        goto post_registration;
    }

    /* If service registration failed, try to connect to the existing instance */
    g_application_set_flags (application, init_flags | G_APPLICATION_IS_LAUNCHER);
    g_application_register (application, NULL, &error);

    if (error != NULL) {
        g_printerr ("Could not register nemo as a remote: %s\n", error->message);
        g_clear_error (&error);

        *exit_status = EXIT_FAILURE;
        goto out;
    }

post_registration:

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
		if (self->priv->geometry != NULL) {
			g_application_open (application, files, len, self->priv->geometry);
		} else {
			g_application_open (application, files, len, "");
		}
	}

	for (idx = 0; idx < len; idx++) {
		g_object_unref (files[idx]);
	}
	g_free (files);

 out:
	g_option_context_free (context);

	return TRUE;	
}

static void
menu_state_changed_callback (NemoMainApplication *self)
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
nemo_main_application_continue_startup (NemoApplication *app)
{
	NemoMainApplication *self = NEMO_MAIN_APPLICATION (app);

	/* create DBus manager */
	self->priv->dbus_manager = nemo_dbus_manager_new ();
	self->priv->fdb_manager = nemo_freedesktop_dbus_new ();

    /* Check the user's ~/.config/nemo directory and post warnings
     * if there are problems.
     */

    nemo_application_check_required_directory (app, nemo_get_user_directory ());

	/* register views */
	nemo_icon_view_register ();
	nemo_list_view_register ();
	nemo_icon_view_compact_register ();
#if defined(ENABLE_EMPTY_VIEW) && ENABLE_EMPTY_VIEW
	nemo_empty_view_register ();
#endif

	/* Watch for unmounts so we can close open windows */
	/* TODO-gio: This should be using the UNMOUNTED feature of GFileMonitor instead */
	self->priv->volume_monitor = g_volume_monitor_get ();
	g_signal_connect_object (self->priv->volume_monitor, "mount_removed",
				 G_CALLBACK (mount_removed_callback), self, 0);
	g_signal_connect_object (self->priv->volume_monitor, "mount_added",
				 G_CALLBACK (mount_added_callback), self, 0);

    g_signal_connect_swapped (nemo_window_state, "changed::" NEMO_WINDOW_STATE_START_WITH_MENU_BAR,
                              G_CALLBACK (menu_state_changed_callback), self);
}

static void
nemo_desktop_application_continue_quit (NemoApplication *app)
{
}

static void
nemo_main_application_quit_mainloop (GApplication *app)
{
    nemo_main_application_notify_unmount_done (NEMO_APPLICATION (app), NULL);

    G_APPLICATION_CLASS (nemo_main_application_parent_class)->quit_mainloop (app);
}

static void
nemo_main_application_class_init (NemoMainApplicationClass *class)
{
    GObjectClass *object_class;
    GApplicationClass *application_class;
    NemoApplicationClass *nemo_app_class;

    object_class = G_OBJECT_CLASS (class);
    object_class->finalize = nemo_main_application_finalize;

    application_class = G_APPLICATION_CLASS (class);
    application_class->open = nemo_main_application_open;
    application_class->local_command_line = nemo_main_application_local_command_line;
    application_class->quit_mainloop = nemo_main_application_quit_mainloop;

    nemo_app_class = NEMO_APPLICATION_CLASS (class);
    nemo_app_class->open_location = nemo_main_application_open_location;
    nemo_app_class->create_window = nemo_main_application_create_window;
    nemo_app_class->notify_unmount_show = nemo_main_application_notify_unmount_show;
    nemo_app_class->notify_unmount_done = nemo_main_application_notify_unmount_done;
    nemo_app_class->close_all_windows = nemo_main_application_close_all_windows;
    nemo_app_class->continue_startup = nemo_main_application_continue_startup;
    nemo_app_class->continue_quit = nemo_desktop_application_continue_quit;

    g_type_class_add_private (class, sizeof (NemoMainApplicationPriv));
}

NemoApplication *
nemo_main_application_get_singleton (void)
{
    return nemo_application_initialize_singleton (NEMO_TYPE_MAIN_APPLICATION,
                                                  "application-id", "org.Nemo",
                                                  "flags", G_APPLICATION_HANDLES_OPEN,
                                                  "inactivity-timeout", 30 * 1000, // seconds
                                                  "register-session", TRUE,
                                                  NULL);
}

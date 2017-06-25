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

#if (defined(ENABLE_EMPTY_VIEW) && ENABLE_EMPTY_VIEW)
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

#define NEMO_ACCEL_MAP_SAVE_DELAY 30

G_DEFINE_TYPE (NemoApplication, nemo_application, GTK_TYPE_APPLICATION);

struct _NemoApplicationPriv {
	NemoProgressUIHandler *progress_handler;

    gboolean cache_problem;
    gboolean ignore_cache_problem;
};

static NemoApplication *singleton = NULL;

/* Common startup stuff */

/* The saving of the accelerator map was requested  */
static gboolean save_of_accel_map_requested = FALSE;

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
add_app_css_provider (void)
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

    add_app_css_provider ();
}

static gboolean
save_accel_map (gpointer data)
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
                               save_accel_map, NULL);
    }
}

static void
init_gtk_accels (void)
{
    char *accel_map_filename;

    /* load accelerator map, and register save callback */
    accel_map_filename = nemo_get_accel_map_file ();
    if (accel_map_filename) {
        gtk_accel_map_load (accel_map_filename);
        g_free (accel_map_filename);
    }

    g_signal_connect (gtk_accel_map_get (), "changed",
              G_CALLBACK (queue_accel_map_save_callback), NULL);
}

static void
menu_provider_items_updated_handler (NemoMenuProvider *provider, GtkWidget* parent_window, gpointer data)
{

    g_signal_emit_by_name (nemo_signaller_get_current (),
                           "popup_menu_changed");
}

static void
init_menu_provider_callback (void)
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

/* end Common Startup Stuff */

gboolean
nemo_application_check_required_directory (NemoApplication *application,
                                           gchar           *path)
{
    gboolean ret;

    g_assert (NEMO_IS_APPLICATION (application));

    ret = TRUE;

    if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
        ret = FALSE;
    }

    if (!ret) {
        char *error_string;
        const char *detail_string;
        GtkDialog *dialog;

        error_string = g_strdup_printf (_("Nemo could not create the required folder \"%s\"."),
                                        path);
        detail_string = _("Before running Nemo, please create the following folder, or "
                          "set permissions such that Nemo can create it.");

        dialog = eel_show_error_dialog (error_string, detail_string, NULL);
        /* We need the main event loop so the user has a chance to see the dialog. */
        gtk_application_add_window (GTK_APPLICATION (application),
                                    GTK_WINDOW (dialog));

        g_free (error_string);
    }

    g_free (path);

    return ret;
}

void
nemo_application_open_location (NemoApplication *application,
                                GFile           *location,
                                GFile           *selection,
                                const char      *startup_id)
{
    NEMO_APPLICATION_CLASS (G_OBJECT_GET_CLASS (application))->open_location (application,
                                                                              location,
                                                                              selection,
                                                                              startup_id);
}

NemoWindow *
nemo_application_create_window (NemoApplication *application,
                                GdkScreen       *screen)
{
    return NEMO_APPLICATION_CLASS (G_OBJECT_GET_CLASS (application))->create_window (application,
                                                                              screen);
}

void
nemo_application_notify_unmount_done (NemoApplication *application,
                                          const gchar *message)
{
    NEMO_APPLICATION_CLASS (G_OBJECT_GET_CLASS (application))->notify_unmount_done (application,
                                                                                    message);
}

void
nemo_application_notify_unmount_show (NemoApplication *application,
                                          const gchar *message)
{
    NEMO_APPLICATION_CLASS (G_OBJECT_GET_CLASS (application))->notify_unmount_show (application,
                                                                                    message);
}

void
nemo_application_close_all_windows (NemoApplication *application)
{
    NEMO_APPLICATION_CLASS (G_OBJECT_GET_CLASS (application))->close_all_windows (application);
}

static GObject *
nemo_application_constructor (GType type,
				  guint n_construct_params,
				  GObjectConstructParam *construct_params)
{
        GObject *retval;

        retval = G_OBJECT_CLASS (nemo_application_parent_class)->constructor
                (type, n_construct_params, construct_params);

        singleton = NEMO_APPLICATION (retval);
        g_object_add_weak_pointer (retval, (gpointer) &singleton);

        return retval;
}

static void
nemo_application_init (NemoApplication *application)
{
	GSimpleAction *action;

	application->priv = G_TYPE_INSTANCE_GET_PRIVATE (application,
                                                     NEMO_TYPE_APPLICATION,
                                                     NemoApplicationPriv);

    if (g_getenv("NEMO_BENCHMARK_LOADING"))
        nemo_startup_timer = g_timer_new ();

    action = g_simple_action_new ("quit", NULL);

    g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (action));

	g_signal_connect_swapped (action, "activate",
				  G_CALLBACK (nemo_application_quit), application);

	g_object_unref (action);
}

void
nemo_application_quit (NemoApplication *self)
{
	GApplication *app = G_APPLICATION (self);

    /* Run desktop or -main specific destruction - namely tearing down the
     * desktop manager before our g_list_foreach below takes out its windows.
     */

    NEMO_APPLICATION_CLASS (G_OBJECT_GET_CLASS (self))->continue_quit (self);

	GList *windows;

	windows = gtk_application_get_windows (GTK_APPLICATION (app));
	g_list_foreach (windows, (GFunc) gtk_widget_destroy, NULL);

    nemo_global_preferences_finalize ();

    /* we have been asked to force quit */
    g_application_quit (G_APPLICATION (self));
}

static void
nemo_application_startup (GApplication *app)
{
	NemoApplication *self = NEMO_APPLICATION (app);
	/* chain up to the GTK+ implementation early, so gtk_init()
	 * is called for us.
	 */
	G_APPLICATION_CLASS (nemo_application_parent_class)->startup (app);

	/* create an undo manager */
	self->undo_manager = nemo_undo_manager_new ();

	/* initialize preferences and create the global GSettings objects */
	nemo_global_preferences_init ();

    /* Run desktop- or main- specific things */
    NEMO_APPLICATION_CLASS (G_OBJECT_GET_CLASS (self))->continue_startup (self);

	/* register property pages */
	nemo_image_properties_page_register ();

	/* initialize theming */
	init_icons_and_styles ();
	init_gtk_accels ();

	/* initialize nemo modules */
	nemo_module_setup ();

	/* attach menu-provider module callback */
	init_menu_provider_callback ();

	/* Initialize the UI handler singleton for file operations */
	notify_init (GETTEXT_PACKAGE);
	self->priv->progress_handler = nemo_progress_ui_handler_new ();

    self->priv->cache_problem = FALSE;
    self->priv->ignore_cache_problem = FALSE;

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
}

static void
nemo_application_quit_mainloop (GApplication *app)
{
	DEBUG ("Quitting mainloop");

    nemo_icon_info_clear_caches ();
    save_accel_map (NULL);

    nemo_application_notify_unmount_done (NEMO_APPLICATION (app), NULL);

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
    object_class->constructor = nemo_application_constructor;

    application_class = G_APPLICATION_CLASS (class);
    application_class->startup = nemo_application_startup;
    application_class->quit_mainloop = nemo_application_quit_mainloop;

    gtkapp_class = GTK_APPLICATION_CLASS (class);
    gtkapp_class->window_removed = nemo_application_window_removed;

    g_type_class_add_private (class, sizeof (NemoApplicationPriv));
}

NemoApplication *
nemo_application_initialize_singleton (GType object_type,
                                       const gchar *first_property_name,
                                       ...)
{
    NemoApplication *application;
    va_list var_args;

    va_start (var_args, first_property_name);
    application = NEMO_APPLICATION (g_object_new_valist (object_type, first_property_name, var_args));
    va_end (var_args);

    return application;
}

NemoApplication *
nemo_application_get_singleton (void)
{
    return singleton;
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

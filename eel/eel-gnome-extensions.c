/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gnome-extensions.c - implementation of new functions that operate on
                            gnome classes. Perhaps some of these should be
  			    rolled into gnome someday.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

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

   Authors: Darin Adler <darin@eazel.com>
*/

#include <config.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include "eel-gnome-extensions.h"

#include <gtk/gtk.h>

/* Adapted from gio - gdesktopappinfo.c - prepend_terminal_to_vector */
static char *
prepend_terminal_to_command_line (const char *command_line)
{
    GSettings *settings;
    gchar *prefix = NULL;
    gchar *terminal = NULL;
    gchar *ret = NULL;

    g_return_val_if_fail (command_line != NULL, g_strdup (command_line));

    settings = g_settings_new ("org.cinnamon.desktop.default-applications.terminal");
    terminal = g_settings_get_string (settings, "exec");

    if (terminal != NULL) {
        gchar *term_path = NULL;

        term_path = g_find_program_in_path (terminal);

        if (term_path != NULL) {
            gchar *exec_flag = NULL;

            exec_flag = g_settings_get_string (settings, "exec-arg");

            if (exec_flag == NULL) {
                prefix = g_strdup (term_path);
            } else {
                prefix = g_strdup_printf ("%s %s", term_path, exec_flag);
            }

            g_free (exec_flag);
        }

        g_free (term_path);
    }

    g_object_unref (settings);
    g_free (terminal);

    if (prefix == NULL) {
        gchar *check = NULL;

        check = g_find_program_in_path ("gnome-terminal");
        if (check != NULL) {
            /* Note that gnome-terminal takes -x and
             * as -e in gnome-terminal is broken we use that.
               20201114 - There looks to be an issue with -x now with gnome-terminal
               and -- is now the recommended option */
            prefix = g_strdup_printf ("gnome-terminal --");
        } else {
            check = g_find_program_in_path ("nxterm");

            if (check == NULL)
                check = g_find_program_in_path ("color-xterm");
            if (check == NULL)
                check = g_find_program_in_path ("rxvt");
            if (check == NULL)
                check = g_find_program_in_path ("xterm");
            if (check == NULL)
                check = g_find_program_in_path ("dtterm");
            if (check == NULL) {
                check = g_strdup ("xterm");
                g_warning ("couldn't find a terminal, falling back to xterm");
            }

            prefix = g_strdup_printf ("%s -e", check);
        }

        g_free (check);
    }

    ret = g_strdup_printf ("%s %s", prefix, command_line);
    g_free (prefix);

    return ret;
}

/* Return a command string containing the path to a terminal on this system. */

void
eel_gnome_open_terminal_on_screen (const gchar *command,
                                   GdkScreen   *screen)
{
    gchar *command_line;
    GAppInfo *app;
    GdkAppLaunchContext *ctx;
    GError *error = NULL;
    GdkDisplay *display;

    command_line = prepend_terminal_to_command_line (command);

    app = g_app_info_create_from_commandline (command_line, NULL, 0, &error);

	if (app != NULL && screen != NULL) {
		display = gdk_screen_get_display (screen);
		ctx = gdk_display_get_app_launch_context (display);
		gdk_app_launch_context_set_screen (ctx, screen);

		g_app_info_launch (app, NULL, G_APP_LAUNCH_CONTEXT (ctx), &error);

		g_object_unref (app);
		g_object_unref (ctx);
	}

	if (error != NULL) {
		g_message ("Could not start application on terminal: %s", error->message);

		g_error_free (error);
	}
}

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

/* Return a command string containing the path to a terminal on this system. */

void
eel_gnome_open_terminal_on_screen (const char *command,
				   GdkScreen  *screen)
{
	GAppInfo *app;
	GdkAppLaunchContext *ctx;
	GError *error = NULL;
	GdkDisplay *display;

	app = g_app_info_create_from_commandline (command, NULL, G_APP_INFO_CREATE_NEEDS_TERMINAL, &error);

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

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 1999, 2000 Red Hat, Inc.
 * Copyright (C) 1999, 2000 Eazel, Inc.
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
 *          Darin Adler <darin@bentspoon.com>,
 *          John Sullivan <sullivan@eazel.com>
 *
 */

/* nemo-main.c: Implementation of the routines that drive program lifecycle and main window creation/destruction. */

#include <config.h>

#include "nemo-main-application.h"

#include <libnemo-private/nemo-debug.h>
#include <libnemo-private/nemo-malloc-utils.h>
#include <eel/eel-debug.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_EXEMPI
#include <exempi/xmp.h>
#endif

int
main (int argc, char *argv[])
{
	gint retval;
	NemoApplication *application;

	nemo_malloc_setup ();

	/* This will be done by gtk+ later, but for now, force it to GNOME */
	g_desktop_app_info_set_desktop_env ("GNOME");

	if (g_getenv ("NEMO_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger ();
	}
	
	/* Initialize gettext support */
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_set_prgname ("nemo");

#ifdef HAVE_EXEMPI
	xmp_init();
#endif

	/* Run the nemo application. */
	application = nemo_main_application_get_singleton ();

    /* hold indefinitely if we're asked to persist */
    if (g_getenv ("NEMO_PERSIST") != NULL) {
        g_application_hold (G_APPLICATION (application));
    }

	retval = g_application_run (G_APPLICATION (application),
				    argc, argv);

	g_object_unref (application);

 	eel_debug_shut_down ();

	return retval;
}

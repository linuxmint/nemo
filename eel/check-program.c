/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* check-program.c: A simple driver for eel self checks.

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
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>

#include <eel/eel-debug.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-lib-self-check-functions.h>
#include <eel/eel-self-checks.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <stdlib.h>

int
main (int argc, char *argv[])
{
#if !defined (EEL_OMIT_SELF_CHECK)

	eel_make_warnings_and_criticals_stop_in_debugger ();
	

	LIBXML_TEST_VERSION
	gtk_init (&argc, &argv);

	/* Run the checks for eel twice. */

	eel_run_lib_self_checks ();
	eel_exit_if_self_checks_failed ();

	eel_run_lib_self_checks ();
	eel_exit_if_self_checks_failed ();

	eel_debug_shut_down ();

#endif /* !EEL_OMIT_SELF_CHECK */
	
	return EXIT_SUCCESS;
}

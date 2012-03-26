/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* test-mime.c - Test for the mime handler detection features of the GNOME
   Virtual File System Library

   Copyright (C) 2000 Eazel

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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Maciej Stachowiak <mjs@eazel.com>
*/

#include <config.h>

#include <gtk/gtk.h>
#include <libnautilus-private/nautilus-mime-actions.h>
#include <stdio.h>

static gboolean ready = FALSE;


static void
append_comma_and_scheme (gpointer scheme,
			 gpointer user_data)
{
	char **string;

	string = (char **) user_data;
	if (strlen (*string) > 0) {
		*string = g_strconcat (*string, ", ", scheme, NULL);
	}
	else {
		*string = g_strdup (scheme);
	}
}


static char *
format_supported_uri_schemes_for_display (GList *supported_uri_schemes)
{
	char *string;

	string = g_strdup ("");
	g_list_foreach (supported_uri_schemes,
			append_comma_and_scheme,
			&string);
	return string;
}

static void
print_application (GAppInfo *application)
{
        if (application == NULL) {
	        puts ("(none)");
	} else {
	        printf ("name: %s\ncommand: %s\nexpects_uris: %s\n", 
			g_application_get_name (application),
			g_application_get_executable (application), 
			(g_app_info_supports_uris (application) ? "TRUE" : "FALSE"));
	}
}

static void 
print_application_list (GList *applications)
{
	GList *p;

	if (applications == NULL) {
		puts ("(none)");
	} else {
		for (p = applications; p != NULL; p = p->next) {
			print_application (p->data);
			puts ("------");
		}
	}
}

static void
ready_callback (NautilusFile *file,
		gpointer callback_data)
{
	ready = TRUE;
}

int
main (int argc, char **argv)
{
        const char *uri;  
	GAppInfo *default_application;
	GList *all_applications;
	NautilusFile *file;
	NautilusFileAttributes attributes;

	gtk_init (&argc, &argv);

	if (argc != 2) {
		fprintf (stderr, "Usage: %s uri\n", *argv);
		return 1;
	}

	uri = argv[1];
	file = nautilus_file_get_by_uri (uri);

	attributes = nautilus_mime_actions_get_full_file_attributes ();
	nautilus_file_call_when_ready (file, attributes, ready_callback, NULL);

	while (!ready) {
		gtk_main_iteration ();
	}

	default_application = nautilus_mime_get_default_application_for_file (file);
	puts("Default Application");
	print_application (default_application);
	puts ("");

	all_applications = nautilus_mime_get_applications_for_file (file); 
	puts("All Applications");
	print_application_list (all_applications);
	puts ("");

	return 0;
}



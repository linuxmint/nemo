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
usage (const char *name)
{
	fprintf (stderr, "Usage: %s uri field value\n", name);
	fprintf (stderr, "Valid field values are: \n");
	fprintf (stderr, "\tdefault_action_type\n");
	fprintf (stderr, "\tdefault_application\n");
	fprintf (stderr, "\tdefault_component\n");
	fprintf (stderr, "\tshort_list_applicationss\n");
	fprintf (stderr, "\tshort_list_components\n");
	fprintf (stderr, "\tadd_to_all_applicationss\n");
	fprintf (stderr, "\tremove_from_all_applications\n");
	exit (1);
}

static GnomeVFSMimeActionType
str_to_action_type (const char *str)
{
	if (g_ascii_strcasecmp (str, "component") == 0) {
		return GNOME_VFS_MIME_ACTION_TYPE_COMPONENT;
	} else if (g_ascii_strcasecmp (str, "application") == 0) {
		return GNOME_VFS_MIME_ACTION_TYPE_APPLICATION;
	} else {
		return GNOME_VFS_MIME_ACTION_TYPE_NONE;
	}
}

static char **
strsplit_handle_null (const char *str, const char *delim, int max)
{
	return g_strsplit ((str == NULL ? "" : str), delim, max);
}


static GList *
strsplit_to_list (const char *str, const char *delim, int max)
{
	char **strv;
	GList *retval;
	int i;

	strv = strsplit_handle_null (str, delim, max);

	retval = NULL;

	for (i = 0; strv[i] != NULL; i++) {
		retval = g_list_prepend (retval, strv[i]);
	}

	retval = g_list_reverse (retval);
	/* Don't strfreev, since we didn't copy the individual strings. */
	g_free (strv);

	return retval;
}

static GList *
comma_separated_str_to_str_list (const char *str)
{
	return strsplit_to_list (str, ",", 0);
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
	const char *field;
	const char *value;
	NautilusFile *file;
	NautilusFileAttributes attributes;

	gtk_init (&argc, &argv);	

	if (argc < 3) {
		usage (argv[0]);
	}

	uri = argv[1];
	field = argv[2];
 	value = argv[3];

	file = nautilus_file_get_by_uri (uri);

	attributes = nautilus_mime_actions_get_full_file_attributes ();
	nautilus_file_call_when_ready (file, attributes, ready_callback, NULL);

	while (!ready) {
		gtk_main_iteration ();
	}

	if (strcmp (field, "default_action_type") == 0) {
		puts ("default_action_type");
		nautilus_mime_set_default_action_type_for_file (file, str_to_action_type (value));
	} else if (strcmp (field, "default_application") == 0) {
		puts ("default_application");
		nautilus_mime_set_default_application_for_file (file, value);
	} else if (strcmp (field, "default_component") == 0) {
		puts ("default_component");
		nautilus_mime_set_default_component_for_file (file, value);
	} else if (strcmp (field, "short_list_applicationss") == 0) {
		puts ("short_list_applications");
		nautilus_mime_set_short_list_applications_for_file 
			(file, comma_separated_str_to_str_list (value));
	} else if (strcmp (field, "short_list_components") == 0) {
		puts ("short_list_components");
		nautilus_mime_set_short_list_components_for_file
			(file, comma_separated_str_to_str_list (value));
	} else if (strcmp (field, "add_to_all_applicationss") == 0) {
		puts ("add_to_all_applications");
		nautilus_mime_extend_all_applications_for_file
			(file, comma_separated_str_to_str_list (value));
	} else if (strcmp (field, "remove_from_all_applications") == 0) {
		puts ("remove_from_all_applications");
		nautilus_mime_remove_from_all_applications_for_file 
			(file, comma_separated_str_to_str_list (value));

	} else {
		usage (argv[0]);
	}

	return 0;
}

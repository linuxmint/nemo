/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Nemo
 *
 * Copyright (C) 2011 Red Hat, Inc.
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
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 */

#include <config.h>

#include "nemo-desktop-metadata.h"

#include "nemo-file-utilities.h"
#include "nemo-keyfile-metadata.h"

static gchar *
get_keyfile_path (void)
{
	gchar *xdg_dir, *retval;

	xdg_dir = nemo_get_user_directory ();
	retval = g_build_filename (xdg_dir, "desktop-metadata", NULL);

	g_free (xdg_dir);

	return retval;
}

void
nemo_desktop_set_metadata_string (NemoFile *file,
                                      const gchar *name,
                                      const gchar *key,
                                      const gchar *string)
{
	gchar *keyfile_filename;

	keyfile_filename = get_keyfile_path ();

	nemo_keyfile_metadata_set_string (file, keyfile_filename,
	                                      name, key, string);

	g_free (keyfile_filename);
}

void
nemo_desktop_set_metadata_stringv (NemoFile *file,
                                       const char *name,
                                       const char *key,
                                       const char * const *stringv)
{
	gchar *keyfile_filename;

	keyfile_filename = get_keyfile_path ();

	nemo_keyfile_metadata_set_stringv (file, keyfile_filename,
	                                       name, key, stringv);

	g_free (keyfile_filename);
}

gboolean
nemo_desktop_update_metadata_from_keyfile (NemoFile *file,
					       const gchar *name)
{
	gchar *keyfile_filename;
	gboolean result;

	keyfile_filename = get_keyfile_path ();

	result = nemo_keyfile_metadata_update_from_keyfile (file,
	                                                        keyfile_filename,
	                                                        name);

	g_free (keyfile_filename);
	return result;
}

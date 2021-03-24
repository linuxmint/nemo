/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-file-utilities.c - implementation of file manipulation routines.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "nemo-file-utilities.h"

#include "nemo-global-preferences.h"
#include "nemo-lib-self-check-functions.h"
#include "nemo-metadata.h"
#include "nemo-file.h"
#include "nemo-file-operations.h"
#include "nemo-search-directory.h"
#include "nemo-signaller.h"
#include "nemo-statx.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-debug.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <unistd.h>
#include <stdlib.h>

#define NEMO_USER_DIRECTORY_NAME "nemo"

#define DESKTOP_DIRECTORY_NAME "Desktop"
#define LEGACY_DESKTOP_DIRECTORY_NAME ".gnome-desktop"

static void update_xdg_dir_cache (void);
static void schedule_user_dirs_changed (void);
static void desktop_dir_changed (void);
static GFile *nemo_find_file_insensitive_next (GFile *parent, const gchar *name);

char *
nemo_compute_title_for_location (GFile *location)
{
	NemoFile *file;
	char *title;
    char *builder;
	/* TODO-gio: This doesn't really work all that great if the
	   info about the file isn't known atm... */

	if (nemo_is_home_directory (location)) {
		return g_strdup (_("Home"));
	}

	builder = NULL;
	if (location) {
		file = nemo_file_get (location);
		builder = nemo_file_get_description (file);
		if (builder == NULL) {
			builder = nemo_file_get_display_name (file);
		}
		nemo_file_unref (file);
	}

	if (builder == NULL) {
		builder = g_file_get_basename (location);
	}

    if (g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_FULL_PATH_TITLES)) {
        gchar *uri, *path;
        file = nemo_file_get (location);

        uri = nemo_file_get_uri (file);
        path = g_filename_from_uri (uri, NULL, NULL);

        if (path != NULL) {
            title = g_strdup_printf("%s - %s", builder, path);
        } else {
            title = g_strdup_printf("%s - %s", builder, uri);
        }
        nemo_file_unref (file);
        g_free (uri);
        g_free (path);
        g_free (builder);
    } else {
        title = g_strdup(builder);
        g_free (builder);
    }
    return title;
}

// TODO: Maybe this can replace nemo_compute_title_for_location() all around?
char *
nemo_compute_search_title_for_location (GFile *location)
{
    GFile *home_file;
    gchar *location_string;

    if (nemo_is_home_directory (location)) {
        return g_strdup (_("Home"));
    }

    home_file = g_file_new_for_path (g_get_home_dir ());

    location_string = NULL;

    location_string = g_file_get_relative_path (home_file, location);

    if (location_string == NULL) {
        if (g_file_is_native (location)) {
            location_string = g_file_get_path (location);
        } else {
            location_string = g_file_get_uri (location);
        }
    }

    g_object_unref (home_file);

    return location_string;
}

/**
 * nemo_get_user_directory:
 *
 * Get the path for the directory containing nemo settings.
 *
 * Return value: the directory path.
 **/
char *
nemo_get_user_directory (void)
{
	char *user_directory = NULL;

	user_directory = g_build_filename (g_get_user_config_dir (),
					   NEMO_USER_DIRECTORY_NAME,
					   NULL);

	if (!g_file_test (user_directory, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (user_directory, DEFAULT_NEMO_DIRECTORY_MODE);
		/* FIXME bugzilla.gnome.org 41286:
		 * How should we handle the case where this mkdir fails?
		 * Note that nemo_application_startup will refuse to launch if this
		 * directory doesn't get created, so that case is OK. But the directory
		 * could be deleted after Nemo was launched, and perhaps
		 * there is some bad side-effect of not handling that case.
		 */
	}

	return user_directory;
}

/**
 * nemo_get_accel_map_file:
 *
 * Get the path for the filename containing nemo accelerator map.
 * The filename need not exist.
 *
 * Return value: the filename path, or NULL if the home directory could not be found
 **/
char *
nemo_get_accel_map_file (void)
{
	const gchar *override;

	override = g_getenv ("GNOME22_USER_DIR");

	if (override) {
		return g_build_filename (override, "accels/nemo", NULL);
	} else {
		return g_build_filename (g_get_home_dir (), ".gnome2/accels/nemo", NULL);
	}
}

/**
 * nemo_get_scripts_directory_path:
 *
 * Get the path for the directory containing nemo scripts.
 *
 * Return value: the directory path containing nemo scripts
 **/
char *
nemo_get_scripts_directory_path (void)
{
	return g_build_filename (g_get_user_data_dir (), "nemo", "scripts", NULL);
}

typedef struct {
	char *type;
	char *path;
	NemoFile *file;
} XdgDirEntry;


static XdgDirEntry *
parse_xdg_dirs (const char *config_file)
{
  GArray *array;
  char *config_file_free = NULL;
  XdgDirEntry dir;
  char *data;
  char **lines;
  char *p, *d;
  int i;
  char *type_start, *type_end;
  char *value, *unescaped;
  gboolean relative;

  array = g_array_new (TRUE, TRUE, sizeof (XdgDirEntry));

  if (config_file == NULL)
    {
      config_file_free = g_build_filename (g_get_user_config_dir (),
					   "user-dirs.dirs", NULL);
      config_file = (const char *)config_file_free;
    }

  if (g_file_get_contents (config_file, &data, NULL, NULL))
    {
      lines = g_strsplit (data, "\n", 0);
      g_free (data);
      for (i = 0; lines[i] != NULL; i++)
	{
	  p = lines[i];
	  while (g_ascii_isspace (*p))
	    p++;

	  if (*p == '#')
	    continue;

	  value = strchr (p, '=');
	  if (value == NULL)
	    continue;
	  *value++ = 0;

	  g_strchug (g_strchomp (p));
	  if (!g_str_has_prefix (p, "XDG_"))
	    continue;
	  if (!g_str_has_suffix (p, "_DIR"))
	    continue;
	  type_start = p + 4;
	  type_end = p + strlen (p) - 4;

	  while (g_ascii_isspace (*value))
	    value++;

	  if (*value != '"')
	    continue;
	  value++;

	  relative = FALSE;
	  if (g_str_has_prefix (value, "$HOME"))
	    {
	      relative = TRUE;
	      value += 5;
	      while (*value == '/')
		      value++;
	    }
	  else if (*value != '/')
	    continue;

	  d = unescaped = g_malloc (strlen (value) + 1);
	  while (*value && *value != '"')
	    {
	      if ((*value == '\\') && (*(value + 1) != 0))
		value++;
	      *d++ = *value++;
	    }
	  *d = 0;

	  *type_end = 0;
	  dir.type = g_strdup (type_start);
	  if (relative)
	    {
	      dir.path = g_build_filename (g_get_home_dir (), unescaped, NULL);
	      g_free (unescaped);
	    }
	  else
	    dir.path = unescaped;

	  g_array_append_val (array, dir);
	}

      g_strfreev (lines);
    }

  g_free (config_file_free);

  return (XdgDirEntry *)g_array_free (array, FALSE);
}

static XdgDirEntry *cached_xdg_dirs = NULL;
static GFileMonitor *cached_xdg_dirs_monitor = NULL;

static void
xdg_dir_changed (NemoFile *file,
		 XdgDirEntry *dir)
{
	GFile *location, *dir_location;
	char *path;

	location = nemo_file_get_location (file);
	dir_location = g_file_new_for_path (dir->path);
	if (!g_file_equal (location, dir_location)) {
		path = g_file_get_path (location);

		if (path) {
			char *argv[5];
			int i;

			g_free (dir->path);
			dir->path = path;

			i = 0;
			argv[i++] = (char *)"xdg-user-dirs-update";
			argv[i++] = (char *)"--set";
			argv[i++] = dir->type;
			argv[i++] = dir->path;
			argv[i++] = NULL;

			/* We do this sync, to avoid possible race-conditions
			   if multiple dirs change at the same time. Its
			   blocking the main thread, but these updates should
			   be very rare and very fast. */
			g_spawn_sync (NULL,
				      argv, NULL,
				      G_SPAWN_SEARCH_PATH |
				      G_SPAWN_STDOUT_TO_DEV_NULL |
				      G_SPAWN_STDERR_TO_DEV_NULL,
				      NULL, NULL,
				      NULL, NULL, NULL, NULL);
			g_reload_user_special_dirs_cache ();
			schedule_user_dirs_changed ();
			desktop_dir_changed ();
			/* Icon might have changed */
			nemo_file_invalidate_attributes (file, NEMO_FILE_ATTRIBUTE_INFO);
		}
	}
	g_object_unref (location);
	g_object_unref (dir_location);
}

static void
xdg_dir_cache_changed_cb (GFileMonitor  *monitor,
			  GFile *file,
			  GFile *other_file,
			  GFileMonitorEvent event_type)
{
	if (event_type == G_FILE_MONITOR_EVENT_CHANGED ||
	    event_type == G_FILE_MONITOR_EVENT_CREATED) {
		update_xdg_dir_cache ();
	}
}

static int user_dirs_changed_tag = 0;

static gboolean
emit_user_dirs_changed_idle (gpointer data)
{
	g_signal_emit_by_name (nemo_signaller_get_current (),
			       "user_dirs_changed");
	user_dirs_changed_tag = 0;
	return FALSE;
}

static void
schedule_user_dirs_changed (void)
{
	if (user_dirs_changed_tag == 0) {
		user_dirs_changed_tag = g_idle_add (emit_user_dirs_changed_idle, NULL);
	}
}

static void
unschedule_user_dirs_changed (void)
{
	if (user_dirs_changed_tag != 0) {
		g_source_remove (user_dirs_changed_tag);
		user_dirs_changed_tag = 0;
	}
}

static void
free_xdg_dir_cache (void)
{
	int i;

	if (cached_xdg_dirs != NULL) {
		for (i = 0; cached_xdg_dirs[i].type != NULL; i++) {
			if (cached_xdg_dirs[i].file != NULL) {
				nemo_file_monitor_remove (cached_xdg_dirs[i].file,
							      &cached_xdg_dirs[i]);
				g_signal_handlers_disconnect_by_func (cached_xdg_dirs[i].file,
								      G_CALLBACK (xdg_dir_changed),
								      &cached_xdg_dirs[i]);
				nemo_file_unref (cached_xdg_dirs[i].file);
			}
			g_free (cached_xdg_dirs[i].type);
			g_free (cached_xdg_dirs[i].path);
		}
		g_free (cached_xdg_dirs);
	}
}

static void
destroy_xdg_dir_cache (void)
{
	free_xdg_dir_cache ();
	unschedule_user_dirs_changed ();
	desktop_dir_changed ();

	if (cached_xdg_dirs_monitor != NULL) {
		g_object_unref  (cached_xdg_dirs_monitor);
		cached_xdg_dirs_monitor = NULL;
	}
}

static void
update_xdg_dir_cache (void)
{
	GFile *file;
	char *config_file, *uri;
	int i;

	free_xdg_dir_cache ();
	g_reload_user_special_dirs_cache ();
	schedule_user_dirs_changed ();
	desktop_dir_changed ();

	cached_xdg_dirs = parse_xdg_dirs (NULL);

	for (i = 0 ; cached_xdg_dirs[i].type != NULL; i++) {
		cached_xdg_dirs[i].file = NULL;
		if (strcmp (cached_xdg_dirs[i].path, g_get_home_dir ()) != 0) {
			uri = g_filename_to_uri (cached_xdg_dirs[i].path, NULL, NULL);
			cached_xdg_dirs[i].file = nemo_file_get_by_uri (uri);
			nemo_file_monitor_add (cached_xdg_dirs[i].file,
						   &cached_xdg_dirs[i],
						   NEMO_FILE_ATTRIBUTE_INFO);
			g_signal_connect (cached_xdg_dirs[i].file,
					  "changed", G_CALLBACK (xdg_dir_changed), &cached_xdg_dirs[i]);
			g_free (uri);
		}
	}

	if (cached_xdg_dirs_monitor == NULL) {
		config_file = g_build_filename (g_get_user_config_dir (),
						     "user-dirs.dirs", NULL);
		file = g_file_new_for_path (config_file);
		cached_xdg_dirs_monitor = g_file_monitor_file (file, 0, NULL, NULL);
		g_signal_connect (cached_xdg_dirs_monitor, "changed",
				  G_CALLBACK (xdg_dir_cache_changed_cb), NULL);
		g_object_unref (file);
		g_free (config_file);

		eel_debug_call_at_shutdown (destroy_xdg_dir_cache);
	}
}

char *
nemo_get_xdg_dir (const char *type)
{
	int i;

	if (cached_xdg_dirs == NULL) {
		update_xdg_dir_cache ();
	}

	for (i = 0 ; cached_xdg_dirs != NULL && cached_xdg_dirs[i].type != NULL; i++) {
		if (strcmp (cached_xdg_dirs[i].type, type) == 0) {
			return g_strdup (cached_xdg_dirs[i].path);
		}
	}
	if (strcmp ("DESKTOP", type) == 0) {
		return g_build_filename (g_get_home_dir (), DESKTOP_DIRECTORY_NAME, NULL);
	}
	if (strcmp ("TEMPLATES", type) == 0) {
		return g_build_filename (g_get_home_dir (), "Templates", NULL);
	}

	return g_strdup (g_get_home_dir ());
}

static char *
get_desktop_path (void)
{
	if (g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_DESKTOP_IS_HOME_DIR)) {
		return g_strdup (g_get_home_dir());
	} else {
		return nemo_get_xdg_dir ("DESKTOP");
	}
}

/**
 * nemo_get_desktop_directory:
 *
 * Get the path for the directory containing files on the desktop.
 *
 * Return value: the directory path.
 **/
char *
nemo_get_desktop_directory (void)
{
	char *desktop_directory;

	desktop_directory = get_desktop_path ();

	/* Don't try to create a home directory */
	if (!g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_DESKTOP_IS_HOME_DIR)) {
		if (!g_file_test (desktop_directory, G_FILE_TEST_EXISTS)) {
			g_mkdir (desktop_directory, DEFAULT_DESKTOP_DIRECTORY_MODE);
			/* FIXME bugzilla.gnome.org 41286:
			 * How should we handle the case where this mkdir fails?
			 * Note that nemo_application_startup will refuse to launch if this
			 * directory doesn't get created, so that case is OK. But the directory
			 * could be deleted after Nemo was launched, and perhaps
			 * there is some bad side-effect of not handling that case.
			 */
		}
	}

	return desktop_directory;
}

GFile *
nemo_get_desktop_location (void)
{
	char *desktop_directory;
	GFile *res;

	desktop_directory = get_desktop_path ();

	res = g_file_new_for_path (desktop_directory);
	g_free (desktop_directory);
	return res;
}


/**
 * nemo_get_desktop_directory_uri:
 *
 * Get the uri for the directory containing files on the desktop.
 *
 * Return value: the directory path.
 **/
char *
nemo_get_desktop_directory_uri (void)
{
	char *desktop_path;
	char *desktop_uri;

	desktop_path = nemo_get_desktop_directory ();
	desktop_uri = g_filename_to_uri (desktop_path, NULL, NULL);
	g_free (desktop_path);

	return desktop_uri;
}

char *
nemo_get_desktop_directory_uri_no_create (void)
{
	char *desktop_path;
	char *desktop_uri;

	desktop_path = get_desktop_path ();
	desktop_uri = g_filename_to_uri (desktop_path, NULL, NULL);
	g_free (desktop_path);

	return desktop_uri;
}

char *
nemo_get_home_directory_uri (void)
{
	return  g_filename_to_uri (g_get_home_dir (), NULL, NULL);
}


gboolean
nemo_should_use_templates_directory (void)
{
	char *dir;
	gboolean res;

	dir = nemo_get_xdg_dir ("TEMPLATES");
	res = strcmp (dir, g_get_home_dir ()) != 0;
	g_free (dir);
	return res;
}

char *
nemo_get_templates_directory (void)
{
	return nemo_get_xdg_dir ("TEMPLATES");
}

void
nemo_create_templates_directory (void)
{
	char *dir;

	dir = nemo_get_templates_directory ();
	if (!g_file_test (dir, G_FILE_TEST_EXISTS)) {
		g_mkdir (dir, DEFAULT_NEMO_DIRECTORY_MODE);
	}
	g_free (dir);
}

char *
nemo_get_templates_directory_uri (void)
{
	char *directory, *uri;

	directory = nemo_get_templates_directory ();
	uri = g_filename_to_uri (directory, NULL, NULL);
	g_free (directory);
	return uri;
}

char *
nemo_get_searches_directory (void)
{
	char *user_dir;
	char *searches_dir;

	user_dir = nemo_get_user_directory ();
	searches_dir = g_build_filename (user_dir, "searches", NULL);
	g_free (user_dir);

	if (!g_file_test (searches_dir, G_FILE_TEST_EXISTS))
		g_mkdir (searches_dir, DEFAULT_NEMO_DIRECTORY_MODE);

	return searches_dir;
}

/* These need to be reset to NULL when desktop_is_home_dir changes */
static GFile *desktop_dir = NULL;
static GFile *desktop_dir_dir = NULL;
static char *desktop_dir_filename = NULL;
static gboolean desktop_dir_changed_callback_installed = FALSE;


static void
desktop_dir_changed (void)
{
	if (desktop_dir) {
		g_object_unref (desktop_dir);
	}
	if (desktop_dir_dir) {
		g_object_unref (desktop_dir_dir);
	}
	g_free (desktop_dir_filename);
	desktop_dir = NULL;
	desktop_dir_dir = NULL;
	desktop_dir_filename = NULL;
}

static void
desktop_dir_changed_callback (gpointer callback_data)
{
	desktop_dir_changed ();
}

static void
update_desktop_dir (void)
{
	char *path;
	char *dirname;

	path = get_desktop_path ();
	desktop_dir = g_file_new_for_path (path);

	dirname = g_path_get_dirname (path);
	desktop_dir_dir = g_file_new_for_path (dirname);
	g_free (dirname);
	desktop_dir_filename = g_path_get_basename (path);
	g_free (path);
}

gboolean
nemo_is_home_directory_file (GFile *dir,
				 const char *filename)
{
	char *dirname;
	static GFile *home_dir_dir = NULL;
	static char *home_dir_filename = NULL;

	if (home_dir_dir == NULL) {
		dirname = g_path_get_dirname (g_get_home_dir ());
		home_dir_dir = g_file_new_for_path (dirname);
		g_free (dirname);
		home_dir_filename = g_path_get_basename (g_get_home_dir ());
	}

	return (g_file_equal (dir, home_dir_dir) &&
		strcmp (filename, home_dir_filename) == 0);
}

gboolean
nemo_is_home_directory (GFile *dir)
{
	static GFile *home_dir = NULL;

	if (home_dir == NULL) {
		home_dir = g_file_new_for_path (g_get_home_dir ());
	}

	return g_file_equal (dir, home_dir);
}

gboolean
nemo_is_root_directory (GFile *dir)
{
	static GFile *root_dir = NULL;

	if (root_dir == NULL) {
		root_dir = g_file_new_for_path ("/");
	}

	return g_file_equal (dir, root_dir);
}


gboolean
nemo_is_desktop_directory_file (GFile *dir,
				    const char *file)
{

	if (!desktop_dir_changed_callback_installed) {
		g_signal_connect_swapped (nemo_preferences, "changed::" NEMO_PREFERENCES_DESKTOP_IS_HOME_DIR,
					  G_CALLBACK(desktop_dir_changed_callback),
					  NULL);
		desktop_dir_changed_callback_installed = TRUE;
	}

	if (desktop_dir == NULL) {
		update_desktop_dir ();
	}

	return (g_file_equal (dir, desktop_dir_dir) &&
		strcmp (file, desktop_dir_filename) == 0);
}

gboolean
nemo_is_desktop_directory (GFile *dir)
{

	if (!desktop_dir_changed_callback_installed) {
		g_signal_connect_swapped (nemo_preferences, "changed::" NEMO_PREFERENCES_DESKTOP_IS_HOME_DIR,
					  G_CALLBACK(desktop_dir_changed_callback),
					  NULL);
		desktop_dir_changed_callback_installed = TRUE;
	}

	if (desktop_dir == NULL) {
		update_desktop_dir ();
	}

	return g_file_equal (dir, desktop_dir);
}


/**
 * nemo_get_gmc_desktop_directory:
 *
 * Get the path for the directory containing the legacy gmc desktop.
 *
 * Return value: the directory path.
 **/
char *
nemo_get_gmc_desktop_directory (void)
{
	return g_build_filename (g_get_home_dir (), LEGACY_DESKTOP_DIRECTORY_NAME, NULL);
}

char *
nemo_get_data_file_path (const char *partial_path)
{
	char *path;
	char *user_directory;

	/* first try the user's home directory */
	user_directory = nemo_get_user_directory ();
	path = g_build_filename (user_directory, partial_path, NULL);
	g_free (user_directory);
	if (g_file_test (path, G_FILE_TEST_EXISTS)) {
		return path;
	}
	g_free (path);

	/* next try the shared directory */
	path = g_build_filename (NEMO_DATADIR, partial_path, NULL);
	if (g_file_test (path, G_FILE_TEST_EXISTS)) {
		return path;
	}
	g_free (path);

	return NULL;
}

char *
nemo_ensure_unique_file_name (const char *directory_uri,
				  const char *base_name,
				  const char *extension)
{
	GFileInfo *info;
	char *filename;
	GFile *dir, *child;
	int copy;
	char *res;

	dir = g_file_new_for_uri (directory_uri);

	info = g_file_query_info (dir, G_FILE_ATTRIBUTE_STANDARD_TYPE, 0, NULL, NULL);
	if (info == NULL) {
		g_object_unref (dir);
		return NULL;
	}
	g_object_unref (info);

	filename = g_strdup_printf ("%s%s",
				    base_name,
				    extension);
	child = g_file_get_child (dir, filename);
	g_free (filename);

	copy = 1;
	while ((info = g_file_query_info (child, G_FILE_ATTRIBUTE_STANDARD_TYPE, 0, NULL, NULL)) != NULL) {
		g_object_unref (info);
		g_object_unref (child);

		filename = g_strdup_printf ("%s-%d%s",
					    base_name,
					    copy,
					    extension);
		child = g_file_get_child (dir, filename);
		g_free (filename);

		copy++;
	}

	res = g_file_get_uri (child);
	g_object_unref (child);
	g_object_unref (dir);

	return res;
}

char *
nemo_unique_temporary_file_name (void)
{
	const char *prefix = "/tmp/nemo-temp-file";
	char *file_name;
	int fd;

	file_name = g_strdup_printf ("%sXXXXXX", prefix);

	fd = g_mkstemp (file_name);
	if (fd == -1) {
		g_free (file_name);
		file_name = NULL;
	} else {
		close (fd);
	}

	return file_name;
}

GFile *
nemo_find_existing_uri_in_hierarchy (GFile *location)
{
	GFileInfo *info;
	GFile *tmp;

	g_assert (location != NULL);

	location = g_object_ref (location);
	while (location != NULL) {
		info = g_file_query_info (location,
					  G_FILE_ATTRIBUTE_STANDARD_NAME,
					  0, NULL, NULL);
		g_object_unref (info);
		if (info != NULL) {
			return location;
		}
		tmp = location;
		location = g_file_get_parent (location);
		g_object_unref (tmp);
	}

	return location;
}

/**
 * nemo_find_file_insensitive
 *
 * Attempt to find a file case-insentively. If the path can be found, the
 * returned file maps directly to it. Otherwise, a file using the
 * originally-cased path is returned. This function performs might perform
 * I/O.
 *
 * Return value: a #GFile to a child specified by @name.
 **/
GFile *
nemo_find_file_insensitive (GFile *parent, const gchar *name)
{
	gchar **split_path;
	gchar *component;
	GFile *file, *next;
	gint i;

	split_path = g_strsplit (name, G_DIR_SEPARATOR_S, -1);

	file = g_object_ref (parent);

	for (i = 0; (component = split_path[i]) != NULL; i++) {
		if (!(next = nemo_find_file_insensitive_next (file,
		                                                  component))) {
			/* File does not exist */
			g_object_unref (file);
			file = NULL;
			break;
		}
		g_object_unref (file);
		file = next;
	}
	g_strfreev (split_path);

	if (file) {
		return file;
	}
	return g_file_get_child (parent, name);
}

static GFile *
nemo_find_file_insensitive_next (GFile *parent, const gchar *name)
{
	GFileEnumerator *children;
	GFileInfo *info;
	gboolean use_utf8, found;
	char *filename, *case_folded_name, *utf8_collation_key, *ascii_collation_key, *child_key;
	GFile *file;
	const char *child_name, *compare_key;

	/* First check the given version */
	file = g_file_get_child (parent, name);
	if (g_file_query_exists (file, NULL)) {
		return file;
	}
	g_object_unref (file);

	ascii_collation_key = g_ascii_strdown (name, -1);
	use_utf8 = g_utf8_validate (name, -1, NULL);
	utf8_collation_key = NULL;
	if (use_utf8) {
		case_folded_name = g_utf8_casefold (name, -1);
		utf8_collation_key = g_utf8_collate_key (case_folded_name, -1);
		g_free (case_folded_name);
	}

	/* Enumerate and compare insensitive */
	filename = NULL;
	children = g_file_enumerate_children (parent,
	                                      G_FILE_ATTRIBUTE_STANDARD_NAME,
	                                      0, NULL, NULL);
	if (children != NULL) {
		while ((info = g_file_enumerator_next_file (children, NULL, NULL))) {
			child_name = g_file_info_get_name (info);

			if (use_utf8 && g_utf8_validate (child_name, -1, NULL)) {
				gchar *case_folded;

				case_folded = g_utf8_casefold (child_name, -1);
				child_key = g_utf8_collate_key (case_folded, -1);
				g_free (case_folded);
				compare_key = utf8_collation_key;
			} else {
				child_key = g_ascii_strdown (child_name, -1);
				compare_key = ascii_collation_key;
			}

			found = strcmp (child_key, compare_key) == 0;
			g_free (child_key);
			if (found) {
				filename = g_strdup (child_name);
				break;
			}
		}
		g_file_enumerator_close (children, NULL, NULL);
		g_object_unref (children);
	}

	g_free (ascii_collation_key);
	g_free (utf8_collation_key);

	if (filename) {
		file = g_file_get_child (parent, filename);
		g_free (filename);
		return file;
	}

	return NULL;
}

static gboolean
have_program_in_path (const char *name)
{
	char *path;
	gboolean result;

	path = g_find_program_in_path (name);
	result = (path != NULL);
	g_free (path);
	return result;
}

gboolean
nemo_is_file_roller_installed (void)
{
	static int installed = - 1;

	if (installed < 0) {
		if (have_program_in_path ("file-roller")) {
			installed = 1;
		} else {
			installed = 0;
		}
	}

	return installed > 0 ? TRUE : FALSE;
}

#define GSM_NAME  "org.gnome.SessionManager"
#define GSM_PATH "/org/gnome/SessionManager"
#define GSM_INTERFACE "org.gnome.SessionManager"

/* The following values come from
 * http://www.gnome.org/~mccann/gnome-session/docs/gnome-session.html#org.gnome.SessionManager.Inhibit
 */
#define INHIBIT_LOGOUT (1U)
#define INHIBIT_SUSPEND (4U)

static GDBusConnection *
get_dbus_connection (void)
{
	static GDBusConnection *conn = NULL;

	if (conn == NULL) {
		GError *error = NULL;

	        conn = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

		if (conn == NULL) {
	                g_warning ("Could not connect to session bus: %s", error->message);
			g_error_free (error);
		}
	}

	return conn;
}

/**
 * nemo_inhibit_power_manager:
 * @message: a human readable message for the reason why power management
 *       is being suspended.
 *
 * Inhibits the power manager from logging out or suspending the machine
 * (e.g. whenever Nemo is doing file operations).
 *
 * Returns: an integer cookie, which must be passed to
 *    nemo_uninhibit_power_manager() to resume
 *    normal power management.
 */
int
nemo_inhibit_power_manager (const char *message)
{
	GDBusConnection *connection;
	GVariant *result;
	GError *error = NULL;
	guint cookie = 0;

	g_return_val_if_fail (message != NULL, -1);

        connection = get_dbus_connection ();

        if (connection == NULL) {
                return -1;
        }

	result = g_dbus_connection_call_sync (connection,
					      GSM_NAME,
					      GSM_PATH,
					      GSM_INTERFACE,
					      "Inhibit",
					      g_variant_new ("(susu)",
							     "Nemo",
							     (guint) 0,
							     message,
							     (guint) (INHIBIT_LOGOUT | INHIBIT_SUSPEND)),
					      G_VARIANT_TYPE ("(u)"),
					      G_DBUS_CALL_FLAGS_NO_AUTO_START,
					      -1,
					      NULL,
					      &error);

	if (error != NULL) {
		g_warning ("Could not inhibit power management: %s", error->message);
		g_error_free (error);
		return -1;
	}

	g_variant_get (result, "(u)", &cookie);
	g_variant_unref (result);

	return (int) cookie;
}

/**
 * nemo_uninhibit_power_manager:
 * @cookie: the cookie value returned by nemo_inhibit_power_manager()
 *
 * Uninhibits power management. This function must be called after the task
 * which inhibited power management has finished, or the system will not
 * return to normal power management.
 */
void
nemo_uninhibit_power_manager (gint cookie)
{
	GDBusConnection *connection;
	GVariant *result;
	GError *error = NULL;

	g_return_if_fail (cookie > 0);

	connection = get_dbus_connection ();

	if (connection == NULL) {
		return;
	}

	result = g_dbus_connection_call_sync (connection,
					      GSM_NAME,
					      GSM_PATH,
					      GSM_INTERFACE,
					      "Uninhibit",
					      g_variant_new ("(u)", (guint) cookie),
					      NULL,
					      G_DBUS_CALL_FLAGS_NO_AUTO_START,
					      -1,
					      NULL,
					      &error);

	if (result == NULL) {
		g_warning ("Could not uninhibit power management: %s", error->message);
		g_error_free (error);
		return;
	}

	g_variant_unref (result);
}

/* Returns TRUE if the file is in XDG_DATA_DIRS or
   in "~/.gnome2/". This is used for deciding
   if a desktop file is "trusted" based on the path */
gboolean
nemo_is_in_system_dir (GFile *file)
{
	const char * const * data_dirs;
	char *path, *gnome2;
	int i;
	gboolean res;

	if (!g_file_is_native (file)) {
		return FALSE;
	}

	path = g_file_get_path (file);

	res = FALSE;

	data_dirs = g_get_system_data_dirs ();
	for (i = 0; path != NULL && data_dirs[i] != NULL; i++) {
		if (g_str_has_prefix (path, data_dirs[i])) {
			res = TRUE;
			break;
		}

	}

	if (!res) {
		/* Panel desktop files are here, trust them */
		gnome2 = g_build_filename (g_get_home_dir (), ".gnome2", NULL);
		if (g_str_has_prefix (path, gnome2)) {
			res = TRUE;
		}
		g_free (gnome2);
	}
	g_free (path);

	return res;
}

GHashTable *
nemo_trashed_files_get_original_directories (GList *files,
						 GList **unhandled_files)
{
	GHashTable *directories;
	NemoFile *file, *original_file, *original_dir;
	GList *l, *m;

	directories = NULL;

	if (unhandled_files != NULL) {
		*unhandled_files = NULL;
	}

	for (l = files; l != NULL; l = l->next) {
		file = NEMO_FILE (l->data);
		original_file = nemo_file_get_trash_original_file (file);

		original_dir = NULL;
		if (original_file != NULL) {
			original_dir = nemo_file_get_parent (original_file);
		}

		if (original_dir != NULL) {
			if (directories == NULL) {
				directories = g_hash_table_new_full (g_direct_hash, g_direct_equal,
								     (GDestroyNotify) nemo_file_unref,
								     (GDestroyNotify) nemo_file_list_free);
			}
			nemo_file_ref (original_dir);
			m = g_hash_table_lookup (directories, original_dir);
			if (m != NULL) {
				g_hash_table_steal (directories, original_dir);
				nemo_file_unref (original_dir);
			}
			m = g_list_append (m, nemo_file_ref (file));
			g_hash_table_insert (directories, original_dir, m);
		} else if (unhandled_files != NULL) {
			*unhandled_files = g_list_append (*unhandled_files, nemo_file_ref (file));
		}

		if (original_file != NULL) {
			nemo_file_unref (original_file);
		}

		if (original_dir != NULL) {
			nemo_file_unref (original_dir);
		}
	}

	return directories;
}

static GList *
locations_from_file_list (GList *file_list)
{
	NemoFile *file;
	GList *l, *ret;

	ret = NULL;

	for (l = file_list; l != NULL; l = l->next) {
		file = NEMO_FILE (l->data);
		ret = g_list_prepend (ret, nemo_file_get_location (file));
	}

	return g_list_reverse (ret);
}

typedef struct {
	GHashTable *original_dirs_hash;
	GtkWindow  *parent_window;
} RestoreFilesData;

static void
ensure_dirs_task_ready_cb (GObject *_source,
			   GAsyncResult *res,
			   gpointer user_data)
{
	NemoFile *original_dir;
	GFile *original_dir_location;
	GList *original_dirs, *files, *locations, *l;
	RestoreFilesData *data = user_data;

	original_dirs = g_hash_table_get_keys (data->original_dirs_hash);
	for (l = original_dirs; l != NULL; l = l->next) {
		original_dir = NEMO_FILE (l->data);
		original_dir_location = nemo_file_get_location (original_dir);

		files = g_hash_table_lookup (data->original_dirs_hash, original_dir);
		locations = locations_from_file_list (files);

		nemo_file_operations_move
			(locations, NULL,
			 original_dir_location,
			 data->parent_window,
			 NULL, NULL);

		g_list_free_full (locations, g_object_unref);
		g_object_unref (original_dir_location);
	}

	g_list_free (original_dirs);

	g_hash_table_unref (data->original_dirs_hash);
	g_slice_free (RestoreFilesData, data);
}

static void
ensure_dirs_task_thread_func (GTask *task,
			      gpointer source,
			      gpointer task_data,
			      GCancellable *cancellable)
{
	RestoreFilesData *data = task_data;
	NemoFile *original_dir;
	GFile *original_dir_location;
	GList *original_dirs, *l;

	original_dirs = g_hash_table_get_keys (data->original_dirs_hash);
	for (l = original_dirs; l != NULL; l = l->next) {
		original_dir = NEMO_FILE (l->data);
		original_dir_location = nemo_file_get_location (original_dir);

		g_file_make_directory_with_parents (original_dir_location, cancellable, NULL);
		g_object_unref (original_dir_location);
	}

	g_task_return_pointer (task, NULL, NULL);
}

static void
restore_files_ensure_parent_directories (GHashTable *original_dirs_hash,
					 GtkWindow  *parent_window)
{
	RestoreFilesData *data;
	GTask *ensure_dirs_task;

	data = g_slice_new0 (RestoreFilesData);
	data->parent_window = parent_window;
	data->original_dirs_hash = g_hash_table_ref (original_dirs_hash);

	ensure_dirs_task = g_task_new (NULL, NULL, ensure_dirs_task_ready_cb, data);
	g_task_set_task_data (ensure_dirs_task, data, NULL);
	g_task_run_in_thread (ensure_dirs_task, ensure_dirs_task_thread_func);
	g_object_unref (ensure_dirs_task);
}

void
nemo_restore_files_from_trash (GList *files,
				   GtkWindow *parent_window)
{
	NemoFile *file;
	GHashTable *original_dirs_hash;
	GList *unhandled_files, *l;
	char *message, *file_name;

	original_dirs_hash = nemo_trashed_files_get_original_directories (files, &unhandled_files);

	for (l = unhandled_files; l != NULL; l = l->next) {
		file = NEMO_FILE (l->data);
		file_name = nemo_file_get_display_name (file);
		message = g_strdup_printf (_("Could not determine original location of \"%s\" "), file_name);
		g_free (file_name);

		eel_show_warning_dialog (message,
					 _("The item cannot be restored from trash"),
					 parent_window);
		g_free (message);
	}

	if (original_dirs_hash != NULL) {
		restore_files_ensure_parent_directories (original_dirs_hash, parent_window);
		g_hash_table_unref (original_dirs_hash);
	}

	nemo_file_list_unref (unhandled_files);
}

typedef struct {
	NemoMountGetContent callback;
	gpointer user_data;
} GetContentTypesData;

static void
get_types_cb (GObject *source_object,
	      GAsyncResult *res,
	      gpointer user_data)
{
	GetContentTypesData *data;
	char **types;

	data = user_data;
	types = g_mount_guess_content_type_finish (G_MOUNT (source_object), res, NULL);

	g_object_set_data_full (source_object,
				"nemo-content-type-cache",
				g_strdupv (types),
				(GDestroyNotify)g_strfreev);

	if (data->callback) {
		data->callback ((const char **) types, data->user_data);
	}
	g_strfreev (types);
	g_slice_free (GetContentTypesData, data);
}

void
nemo_get_x_content_types_for_mount_async (GMount *mount,
					      NemoMountGetContent callback,
					      GCancellable *cancellable,
					      gpointer user_data)
{
	char **cached;
	GetContentTypesData *data;

	if (mount == NULL) {
		if (callback) {
			callback (NULL, user_data);
		}
		return;
	}

	cached = g_object_get_data (G_OBJECT (mount), "nemo-content-type-cache");
	if (cached != NULL) {
		if (callback) {
			callback ((const char **) cached, user_data);
		}
		return;
	}

	data = g_slice_new0 (GetContentTypesData);
	data->callback = callback;
	data->user_data = user_data;

	g_mount_guess_content_type (mount,
				    FALSE,
				    cancellable,
				    get_types_cb,
				    data);
}

char **
nemo_get_cached_x_content_types_for_mount (GMount *mount)
{
	char **cached;

	if (mount == NULL) {
		return NULL;
	}

	cached = g_object_get_data (G_OBJECT (mount), "nemo-content-type-cache");
	if (cached != NULL) {
		return g_strdupv (cached);
	}

	return NULL;
}

static void
debug_icon_names (const gchar *format, ...)
{
    static gboolean debug_removable_device_icons = FALSE;
    static gsize once_init = 0;

    if (g_once_init_enter (&once_init)) {
        debug_removable_device_icons = g_getenv ("NEMO_DEBUG_DEVICE_ICONS") != NULL;

        g_once_init_leave (&once_init, 1);
    }

    if (!debug_removable_device_icons) {
        return;
    }

    va_list args;
    va_start (args, format);
    g_logv (NULL, G_LOG_LEVEL_MESSAGE, format, args);
    va_end(args);
}

static gboolean
icon_name_is_non_specific (const gchar *icon_name)
{
    gint i;

    static const char * non_specific_icon_names[] = { "drive-harddisk-usb-symbolic" };

    for (i = 0; i < G_N_ELEMENTS (non_specific_icon_names); i++) {
        if (g_strcmp0 (icon_name, non_specific_icon_names[i]) == 0) {
            return TRUE;
        }
    }

    return FALSE;
}

gchar *
nemo_get_mount_icon_name (GMount *mount)
{
    GDrive *drive;
    GIcon *gicon;
    gchar *icon_name;
    gchar *mount_icon_name;
    gchar *dev_name;

    g_return_val_if_fail (mount != NULL, NULL);

    dev_name = g_mount_get_name (mount);
    icon_name = NULL;
    mount_icon_name = NULL;

    gicon = g_mount_get_symbolic_icon (mount);

    if (G_IS_THEMED_ICON (gicon)) {
        mount_icon_name = g_strdup (g_themed_icon_get_names (G_THEMED_ICON (gicon))[0]);

        if (icon_name_is_non_specific (mount_icon_name)) {
            debug_icon_names ("mount %s: icon name '%s' too non-specific", dev_name, mount_icon_name);
        } else {
            icon_name = g_strdup (mount_icon_name);
        }
    }

    g_clear_object (&gicon);

    if (icon_name != NULL) {
        debug_icon_names ("mount %s: icon name '%s' is being used", dev_name, icon_name);

        g_free (dev_name);
        g_free (mount_icon_name);

        return icon_name;
    }

    drive = g_mount_get_drive (mount);

    if (drive != NULL) {
        gicon = g_drive_get_symbolic_icon (drive);
        g_object_unref (drive);

        if (G_IS_THEMED_ICON (gicon)) {
            icon_name = g_strdup (g_themed_icon_get_names (G_THEMED_ICON (gicon))[0]);
        }

        g_object_unref (gicon);
    }

    if (icon_name == NULL) {
        if (mount_icon_name) {
            icon_name = g_strdup (mount_icon_name);
        } else {
            icon_name = g_strdup ("folder-symbolic"); // any theme will have at least this?...
        }

        debug_icon_names ("mount %s: no other good icon name found, using fallback of '%s'", dev_name, icon_name);
    }

    g_free (dev_name);
    g_free (mount_icon_name);

    return icon_name;
}

gchar *
nemo_get_volume_icon_name (GVolume *volume)
{
    GDrive *drive;
    GIcon *gicon;
    gchar *icon_name;
    gchar *dev_name;
    gchar *volume_icon_name;

    g_return_val_if_fail (volume != NULL, NULL);

    dev_name = g_volume_get_name (volume);
    icon_name = NULL;
    volume_icon_name = NULL;

    gicon = g_volume_get_symbolic_icon (volume);

    if (G_IS_THEMED_ICON (gicon)) {
        volume_icon_name = g_strdup (g_themed_icon_get_names (G_THEMED_ICON (gicon))[0]);

        if (icon_name_is_non_specific (volume_icon_name)) {
            debug_icon_names ("volume %s: icon name '%s' too non-specific", dev_name, volume_icon_name);
        } else {
            icon_name = g_strdup (volume_icon_name);
        }
    }

    g_clear_object (&gicon);

    if (icon_name != NULL) {
        debug_icon_names ("volume %s: icon name '%s' is being used", dev_name, icon_name);

        g_free (dev_name);
        g_free (volume_icon_name);

        return icon_name;
    }

    drive = g_volume_get_drive (volume);

    if (drive != NULL) {
        gicon = g_drive_get_symbolic_icon (drive);
        g_object_unref (drive);

        if (G_IS_THEMED_ICON (gicon)) {
            icon_name = g_strdup (g_themed_icon_get_names (G_THEMED_ICON (gicon))[0]);
        }

        g_object_unref (gicon);
    }

    if (icon_name == NULL) {
        if (volume_icon_name) {
            icon_name = g_strdup (volume_icon_name);
        } else {
            icon_name = g_strdup ("folder-symbolic"); // any theme will have at least this?...

        }

        debug_icon_names ("volume %s: no good icon name found, using fallback of '%s'", dev_name, icon_name);
    }

    g_free (dev_name);
    g_free (volume_icon_name);

    return icon_name;
}

gchar *
nemo_get_drive_icon_name (GDrive *drive)
{
    GIcon *gicon;
    gchar *icon_name;
    gchar *dev_name;

    g_return_val_if_fail (drive != NULL, NULL);

    dev_name = g_drive_get_name (drive);

    gicon = g_drive_get_symbolic_icon (drive);

    if (G_IS_THEMED_ICON (gicon)) {
        icon_name = g_strdup (g_themed_icon_get_names (G_THEMED_ICON (gicon))[0]);
    } else {
        icon_name = g_strdup ("folder-symbolic"); // any theme will have at least this?...
    }

    g_object_unref (gicon);

    debug_icon_names ("drive %s: returning icon '%s'", dev_name, icon_name);

    g_free (dev_name);

    return icon_name;
}

gchar *
nemo_get_best_guess_file_mimetype (const gchar *filename,
                                   GFileInfo   *info,
                                   goffset      size)
{
    /* This is an attempt to do a better job at identifying file types than
     * the current gio implementation.
     *
     * The current behavior for empty (0 size) size files, is to return
     * "text/plain" so users can touch a file, or create an empty one in their
     * file manager, and immediately edit it.
     *
     * This behavior currently applies regardless of whether or not a file has
     * an extension.
     *
     * More discussion: https://bugzilla.gnome.org/show_bug.cgi?id=755795
     *
     * What we do here instead is take a file's extension into account if the file
     * is zero-length.  If, by doing so, we have a high confidence that a file is
     * a certain type, we go ahead and use that type.  We only fall back to Gio's
     * zero-length implementation if the extension is unknown, or it has none.
     *
     * - Files that are NOT zero-length are treated the same as before.
     * - Files that we are not certain about are treated the same as before.
     *
     * Again, this only has an effect on zero-length, known-extension files.  My
     * argument for this differing from the standing implementation is that if I
     * create a file with a particular extension, I did so for a reason, and I'm not
     * going to do something silly like make foo.mp3 and attempt to open it with my
     * media player.  I do, however, expect that if I touch a foo.h file and open it,
     * it will open up in my source code editor, and *not* my general purpose plain-
     * text handler.
     */

    g_return_val_if_fail (filename != NULL, g_strdup ("application/octet-stream"));
    g_return_val_if_fail (info != NULL, g_strdup ("application/octet-stream"));

    gchar *mime_type = NULL;

    if (size > 0) {
        /* Default behavior */
        mime_type = eel_ref_str_get_unique (g_file_info_get_content_type (info));
    } else {
        gboolean uncertain;
        gchar *guessed_type = NULL;

        /* Only give the file basename, not the full path.  a) We may not have it yet, and
         * b) we don't want g_content_type_guess to keep going and snoop the file.  This will
         * keep the guess based entirely on the extension, if there is one.
         */
        guessed_type = g_content_type_guess (filename, NULL, 0, &uncertain);

        /* Uncertain means, it's not a registered extension, so we fall back to our (gio's)
         * normal behavior - text/plain (currently at least.) */
        if (!uncertain) {
            mime_type = eel_ref_str_get_unique (guessed_type);
        } else {
            mime_type = eel_ref_str_get_unique (g_file_info_get_content_type (info));
        }

        g_free (guessed_type);
    }

    return mime_type;
}

static void
query_btime_async_thread (GTask         *task,
                          gpointer       object,
                          gpointer       task_data,
                          GCancellable  *cancellable)
{
    gchar *path;
    time_t btime;

    btime = 0;

    path = g_file_get_path (G_FILE (object));

    if (path != NULL) {
        btime = get_file_btime (path);
        g_free (path);
    }

    if (btime > 0) {
        /* Conveniently, gssize is the same as time_t */
        g_task_return_int (task, (gssize) btime);
    } else {
        g_task_return_error (task,
                             g_error_new (G_FILE_ERROR,
                                          G_FILE_ERROR_FAILED,
                                          "statx failed or not supported"));
    }
}

void
nemo_query_btime_async (GFile               *file,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  GTask *task;

  task = g_task_new (file, cancellable, callback, user_data);
  g_task_set_priority (task, G_PRIORITY_DEFAULT);
  g_task_run_in_thread (task, query_btime_async_thread);
  g_object_unref (task);
}

time_t
nemo_query_btime_finish (GFile         *file,
                             GAsyncResult  *res,
                             GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (res, file), -1);

  return (time_t) g_task_propagate_int (G_TASK (res), error);
}

gboolean
nemo_treating_root_as_normal (void)
{
    static gboolean root_is_normal = FALSE;
    static gsize once_init = 0;

    // We only need to set this at startup then cache the result, as we check
    // quite a bit in various parts of the code.
    if (g_once_init_enter (&once_init)) {
        GSettings *prefs = g_settings_new("org.nemo.preferences");

        root_is_normal = g_settings_get_boolean (prefs, "treat-root-as-normal");
        g_object_unref (prefs);

        g_once_init_leave (&once_init, 1);
    }

    return root_is_normal;
}

gboolean
nemo_user_is_root (void)
{
    static gboolean elevated = FALSE;
    static gsize once_init = 0;

    // We only need to set this at startup then cache the result, as we check
    // quite a bit in various parts of the code.
    if (g_once_init_enter (&once_init)) {
        elevated = (geteuid () == 0);
        g_once_init_leave (&once_init, 1);
    }

    return elevated;
}

/* End copied section */

#if !defined (NEMO_OMIT_SELF_CHECK)

void
nemo_self_check_file_utilities (void)
{
}

#endif /* !NEMO_OMIT_SELF_CHECK */

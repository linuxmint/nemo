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
#include "nemo-icon-names.h"
#include "nemo-lib-self-check-functions.h"
#include "nemo-metadata.h"
#include "nemo-file.h"
#include "nemo-file-operations.h"
#include "nemo-search-directory.h"
#include "nemo-signaller.h"
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


/* Allowed characters outside alphanumeric for unreserved. */
#define G_URI_OTHER_UNRESERVED "-._~"

/* This or something equivalent will eventually go into glib/guri.h */
gboolean
nemo_uri_parse (const char  *uri,
		    char       **host,
		    guint16     *port,
		    char       **userinfo)
{
  char *tmp_str;
  const char *start, *p;
  char c;

  g_return_val_if_fail (uri != NULL, FALSE);

  if (host)
    *host = NULL;

  if (port)
    *port = 0;

  if (userinfo)
    *userinfo = NULL;

  /* From RFC 3986 Decodes:
   * URI          = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
   * hier-part    = "//" authority path-abempty
   * path-abempty = *( "/" segment )
   * authority    = [ userinfo "@" ] host [ ":" port ]
   */

  /* Check we have a valid scheme */
  tmp_str = g_uri_parse_scheme (uri);

  if (tmp_str == NULL)
    return FALSE;

  g_free (tmp_str);

  /* Decode hier-part:
   *  hier-part   = "//" authority path-abempty
   */
  p = uri;
  start = strstr (p, "//");

  if (start == NULL)
    return FALSE;

  start += 2;

  if (strchr (start, '@') != NULL)
    {
      /* Decode userinfo:
       * userinfo      = *( unreserved / pct-encoded / sub-delims / ":" )
       * unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~"
       * pct-encoded   = "%" HEXDIG HEXDIG
       */
      p = start;
      while (1)
	{
	  c = *p++;

	  if (c == '@')
	    break;

	  /* pct-encoded */
	  if (c == '%')
	    {
	      if (!(g_ascii_isxdigit (p[0]) ||
		    g_ascii_isxdigit (p[1])))
		return FALSE;

	      p++;

	      continue;
	    }

	  /* unreserved /  sub-delims / : */
	  if (!(g_ascii_isalnum (c) ||
		strchr (G_URI_OTHER_UNRESERVED, c) ||
		strchr (G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS, c) ||
		c == ':'))
	    return FALSE;
	}

      if (userinfo)
	*userinfo = g_strndup (start, p - start - 1);

      start = p;
    }
  else
    {
      p = start;
    }


  /* decode host:
   * host          = IP-literal / IPv4address / reg-name
   * reg-name      = *( unreserved / pct-encoded / sub-delims )
   */

  /* If IPv6 or IPvFuture */
  if (*p == '[')
    {
      start++;
      p++;
      while (1)
	{
	  c = *p++;

	  if (c == ']')
	    break;

	  /* unreserved /  sub-delims */
	  if (!(g_ascii_isalnum (c) ||
		strchr (G_URI_OTHER_UNRESERVED, c) ||
		strchr (G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS, c) ||
		c == ':' ||
		c == '.'))
	    goto error;
	}
    }
  else
    {
      while (1)
	{
	  c = *p++;

	  if (c == ':' ||
	      c == '/' ||
	      c == '?' ||
	      c == '#' ||
	      c == '\0')
	    break;

	  /* pct-encoded */
	  if (c == '%')
	    {
	      if (!(g_ascii_isxdigit (p[0]) ||
		    g_ascii_isxdigit (p[1])))
		goto error;

	      p++;

	      continue;
	    }

	  /* unreserved /  sub-delims */
	  if (!(g_ascii_isalnum (c) ||
		strchr (G_URI_OTHER_UNRESERVED, c) ||
		strchr (G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS, c)))
	    goto error;
	}
    }

  if (host)
    *host = g_uri_unescape_segment (start, p - 1, NULL);

  if (c == ':')
    {
      /* Decode pot:
       *  port          = *DIGIT
       */
      guint tmp = 0;

      while (1)
	{
	  c = *p++;

	  if (c == '/' ||
	      c == '?' ||
	      c == '#' ||
	      c == '\0')
	    break;

	  if (!g_ascii_isdigit (c))
	    goto error;

	  tmp = (tmp * 10) + (c - '0');

	  if (tmp > 65535)
	    goto error;
	}
      if (port)
	*port = (guint16) tmp;
    }

  return TRUE;

error:
  if (host && *host)
    {
      g_free (*host);
      *host = NULL;
    }

  if (userinfo && *userinfo)
    {
      g_free (*userinfo);
      *userinfo = NULL;
    }

  return FALSE;
}

char *
nemo_compute_title_for_location (GFile *location)
{
	NemoFile *file;
	char *title;
	char *builder;
	/* TODO-gio: This doesn't really work all that great if the
	   info about the file isn't known atm... */

	if (!location) {
		return g_strdup (_("(none)"));
	}

	if (nemo_is_home_directory (location)) {
		return g_strdup (_("Home"));
	}
	
	builder = NULL;

	file = nemo_file_get (location);
	builder = nemo_file_get_description (file);
	if (builder == NULL) {
		builder = nemo_file_get_display_name (file);
	}
	nemo_file_unref (file);


	if (builder == NULL) {
		builder = g_file_get_basename (location);
	}

    if (g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_FULL_PATH_TITLES)) {
        file = nemo_file_get (location);
        char *path = g_filename_from_uri (nemo_file_get_uri (file), NULL, NULL);
        if (path != NULL) {
            title = g_strdup_printf("%s - %s", builder, path);
        } else {
            title = g_strdup(builder);
        }
        nemo_file_unref (file);
        g_free (path);
        g_free (builder);
    } else {
        title = g_strdup(builder);
        g_free (builder);
    }
    return title;
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
 * Return value: the filename path
 **/
char *
nemo_get_accel_map_file (void)
{
	return g_build_filename (g_get_user_config_dir (), "nemo", "accels", NULL);
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
			argv[i++] = "xdg-user-dirs-update";
			argv[i++] = "--set";
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
			       "user-dirs-changed");
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
	return nemo_get_xdg_dir ("DESKTOP");
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

	if (desktop_dir == NULL) {
		update_desktop_dir ();
	}

	return (g_file_equal (dir, desktop_dir_dir) &&
		strcmp (file, desktop_dir_filename) == 0);
}

gboolean
nemo_is_desktop_directory (GFile *dir)
{

	if (desktop_dir == NULL) {
		update_desktop_dir ();
	}

	return g_file_equal (dir, desktop_dir);
}

GMount *
nemo_get_mounted_mount_for_root (GFile *location)
{
	GVolumeMonitor *volume_monitor;
	GList *mounts;
	GList *l;
	GMount *mount;
	GMount *result = NULL;
	GFile *root = NULL;
	GFile *default_location = NULL;

	volume_monitor = g_volume_monitor_get ();
	mounts = g_volume_monitor_get_mounts (volume_monitor);

	for (l = mounts; l != NULL; l = l->next) {
		mount = l->data;

		if (g_mount_is_shadowed (mount)) {
			continue;
		}

		root = g_mount_get_root (mount);
		if (g_file_equal (location, root)) {
			result = g_object_ref (mount);
			break;
		}

		default_location = g_mount_get_default_location (mount);
		if (!g_file_equal (default_location, root) &&
		    g_file_equal (location, default_location)) {
			result = g_object_ref (mount);
			break;
		}
	}

	g_clear_object (&root);
	g_clear_object (&default_location);
	g_list_free_full (mounts, g_object_unref);

	return result;
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
		if (info != NULL) {
			g_object_unref (info);
			return location;
		}
		tmp = location;
		location = g_file_get_parent (location);
		g_object_unref (tmp);
	}
	
	return location;
}

static gboolean
have_program_in_path (const char *name)
{
        gchar *path;
        gboolean result;

        path = g_find_program_in_path (name);
        result = (path != NULL);
        g_free (path);
        return result;
}

static GIcon *
special_directory_get_icon (GUserDirectory directory,
			    gboolean symbolic)
{

#define ICON_CASE(x)							 \
	case G_USER_DIRECTORY_ ## x:					 \
		return (symbolic) ? g_themed_icon_new (NEMO_ICON_SYMBOLIC_FOLDER_ ## x) : g_themed_icon_new (NEMO_ICON_FULLCOLOR_FOLDER_ ## x);

	switch (directory) {

		ICON_CASE (DOCUMENTS);
		ICON_CASE (DOWNLOAD);
		ICON_CASE (MUSIC);
		ICON_CASE (PICTURES);
		ICON_CASE (PUBLIC_SHARE);
		ICON_CASE (TEMPLATES);
		ICON_CASE (VIDEOS);

	default:
		return (symbolic) ? g_themed_icon_new (NEMO_ICON_SYMBOLIC_FOLDER) : g_themed_icon_new (NEMO_ICON_FULLCOLOR_FOLDER);
	}

#undef ICON_CASE
}

GIcon *
nemo_special_directory_get_symbolic_icon (GUserDirectory directory)
{
	return special_directory_get_icon (directory, TRUE);
}

GIcon *
nemo_special_directory_get_icon (GUserDirectory directory)
{
	return special_directory_get_icon (directory, FALSE);
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

		nemo_file_unref (original_file);
		nemo_file_unref (original_dir);
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

#if GLIB_CHECK_VERSION(2,36,0)
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
#endif // GLIB_CHECK_VERSION(2,36,0)

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
		message = g_strdup_printf (_("Could not determine original location of “%s” "), file_name);
		g_free (file_name);

		eel_show_warning_dialog (message,
					 _("The item cannot be restored from trash"),
					 parent_window);
		g_free (message);
	}

	if (original_dirs_hash != NULL) {
#if GLIB_CHECK_VERSION(2,36,0)
		restore_files_ensure_parent_directories (original_dirs_hash, parent_window);
		g_hash_table_unref (original_dirs_hash);
#else
		GList *original_dirs = g_hash_table_get_keys (original_dirs_hash);
		for (l = original_dirs; l != NULL; l = l->next) {
			NemoFile *original_dir = NEMO_FILE (l->data);
			GFile *original_dir_location = nemo_file_get_location (original_dir);

			files = g_hash_table_lookup (original_dirs_hash, original_dir);
			GList *locations = locations_from_file_list (files);

			nemo_file_operations_move
				(locations, NULL, 
				 original_dir_location,
				 parent_window,
				 NULL, NULL);

			g_list_free_full (locations, g_object_unref);
			g_object_unref (original_dir_location);
		}

		g_list_free (original_dirs);
		g_hash_table_destroy (original_dirs_hash);
#endif 
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

gboolean
nemo_file_selection_equal (GList *selection_a,
			       GList *selection_b)
{
	GList *al, *bl;
	gboolean selection_matches;

	if (selection_a == NULL || selection_b == NULL) {
		return (selection_a == selection_b);
	}

	if (g_list_length (selection_a) != g_list_length (selection_b)) {
		return FALSE;
	}

	selection_matches = TRUE;

	for (al = selection_a; al; al = al->next) {
		GFile *a_location = nemo_file_get_location (NEMO_FILE (al->data));
		gboolean found = FALSE;

		for (bl = selection_b; bl; bl = bl->next) {
			GFile *b_location = nemo_file_get_location (NEMO_FILE (bl->data));
			found = g_file_equal (b_location, a_location);
			g_object_unref (b_location);

			if (found) {
				break;
			}
		}

		selection_matches = found;
		g_object_unref (a_location);

		if (!selection_matches) {
			break;
		}
	}

	return selection_matches;
}

#if !defined (NEMO_OMIT_SELF_CHECK)

void
nemo_self_check_file_utilities (void)
{
}

#endif /* !NEMO_OMIT_SELF_CHECK */

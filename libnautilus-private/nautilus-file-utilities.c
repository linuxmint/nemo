/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-utilities.c - implementation of file manipulation routines.

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
   see <http://www.gnu.org/licenses/>.

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "nautilus-file-utilities.h"

#include "nautilus-global-preferences.h"
#include "nautilus-icon-names.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-metadata.h"
#include "nautilus-file.h"
#include "nautilus-file-operations.h"
#include "nautilus-search-directory.h"
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

#define NAUTILUS_USER_DIRECTORY_NAME "nautilus"
#define DEFAULT_NAUTILUS_DIRECTORY_MODE (0755)
#define DEFAULT_DESKTOP_DIRECTORY_MODE (0755)

/* Allowed characters outside alphanumeric for unreserved. */
#define G_URI_OTHER_UNRESERVED "-._~"

/* This or something equivalent will eventually go into glib/guri.h */
gboolean
nautilus_uri_parse (const char  *uri,
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
nautilus_compute_title_for_location (GFile *location)
{
	NautilusFile *file;
	char *title;

	/* TODO-gio: This doesn't really work all that great if the
	   info about the file isn't known atm... */

	if (nautilus_is_home_directory (location)) {
		return g_strdup (_("Home"));
	}
	
	title = NULL;
	if (location) {
		file = nautilus_file_get (location);
		title = nautilus_file_get_description (file);
		if (title == NULL) {
			title = nautilus_file_get_display_name (file);
		}
		nautilus_file_unref (file);
	}

	if (title == NULL) {
		title = g_file_get_basename (location);
	}
	
	return title;
}


/**
 * nautilus_get_user_directory:
 * 
 * Get the path for the directory containing nautilus settings.
 *
 * Return value: the directory path.
 **/
char *
nautilus_get_user_directory (void)
{
	char *user_directory = NULL;

	user_directory = g_build_filename (g_get_user_config_dir (),
					   NAUTILUS_USER_DIRECTORY_NAME,
					   NULL);
	
	if (!g_file_test (user_directory, G_FILE_TEST_EXISTS)) {
		g_mkdir (user_directory, DEFAULT_NAUTILUS_DIRECTORY_MODE);
		/* FIXME bugzilla.gnome.org 41286: 
		 * How should we handle the case where this mkdir fails? 
		 * Note that nautilus_application_startup will refuse to launch if this 
		 * directory doesn't get created, so that case is OK. But the directory 
		 * could be deleted after Nautilus was launched, and perhaps
		 * there is some bad side-effect of not handling that case.
		 */
	}

	return user_directory;
}

/**
 * nautilus_get_accel_map_file:
 * 
 * Get the path for the filename containing nautilus accelerator map.
 * The filename need not exist.
 *
 * Return value: the filename path
 **/
char *
nautilus_get_accel_map_file (void)
{
	return g_build_filename (g_get_user_config_dir (), "nautilus", "accels", NULL);
}

/**
 * nautilus_get_scripts_directory_path:
 *
 * Get the path for the directory containing nautilus scripts.
 *
 * Return value: the directory path containing nautilus scripts
 **/
char *
nautilus_get_scripts_directory_path (void)
{
	return g_build_filename (g_get_user_data_dir (), "nautilus", "scripts", NULL);
}

static const char *
get_desktop_path (void)
{
	const char *desktop_path;

	desktop_path = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
	if (desktop_path == NULL) {
		desktop_path = g_get_home_dir ();
	}

	return desktop_path;
}

/**
 * nautilus_get_desktop_directory:
 * 
 * Get the path for the directory containing files on the desktop.
 *
 * Return value: the directory path.
 **/
char *
nautilus_get_desktop_directory (void)
{
	const char *desktop_directory;
	
	desktop_directory = get_desktop_path ();

	/* Don't try to create a home directory */
	if (!g_file_test (desktop_directory, G_FILE_TEST_EXISTS)) {
		g_mkdir (desktop_directory, DEFAULT_DESKTOP_DIRECTORY_MODE);
		/* FIXME bugzilla.gnome.org 41286: 
		 * How should we handle the case where this mkdir fails? 
		 * Note that nautilus_application_startup will refuse to launch if this 
		 * directory doesn't get created, so that case is OK. But the directory 
		 * could be deleted after Nautilus was launched, and perhaps
		 * there is some bad side-effect of not handling that case.
		 */
	}

	return g_strdup (desktop_directory);
}

GFile *
nautilus_get_desktop_location (void)
{
	return g_file_new_for_path (get_desktop_path ());
}

/**
 * nautilus_get_desktop_directory_uri:
 * 
 * Get the uri for the directory containing files on the desktop.
 *
 * Return value: the directory path.
 **/
char *
nautilus_get_desktop_directory_uri (void)
{
	char *desktop_path;
	char *desktop_uri;
	
	desktop_path = nautilus_get_desktop_directory ();
	desktop_uri = g_filename_to_uri (desktop_path, NULL, NULL);
	g_free (desktop_path);

	return desktop_uri;
}

char *
nautilus_get_home_directory_uri (void)
{
	return  g_filename_to_uri (g_get_home_dir (), NULL, NULL);
}


gboolean
nautilus_should_use_templates_directory (void)
{
	const char *dir;
	gboolean res;

	dir = g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES);
	res = dir && (g_strcmp0 (dir, g_get_home_dir ()) != 0);
	return res;
}

char *
nautilus_get_templates_directory (void)
{
	return g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES));
}

void
nautilus_create_templates_directory (void)
{
	char *dir;

	dir = nautilus_get_templates_directory ();
	if (!g_file_test (dir, G_FILE_TEST_EXISTS)) {
		g_mkdir (dir, DEFAULT_NAUTILUS_DIRECTORY_MODE);
	}
	g_free (dir);
}

char *
nautilus_get_templates_directory_uri (void)
{
	char *directory, *uri;

	directory = nautilus_get_templates_directory ();
	uri = g_filename_to_uri (directory, NULL, NULL);
	g_free (directory);
	return uri;
}

char *
nautilus_get_searches_directory (void)
{
	char *user_dir;
	char *searches_dir;

	user_dir = nautilus_get_user_directory ();
	searches_dir = g_build_filename (user_dir, "searches", NULL);
	g_free (user_dir);
	
	if (!g_file_test (searches_dir, G_FILE_TEST_EXISTS))
		g_mkdir (searches_dir, DEFAULT_NAUTILUS_DIRECTORY_MODE);

	return searches_dir;
}

/* These need to be reset to NULL when desktop_is_home_dir changes */
static GFile *desktop_dir = NULL;
static GFile *desktop_dir_dir = NULL;
static char *desktop_dir_filename = NULL;

static void
update_desktop_dir (void)
{
	const char *path;
	char *dirname;

	path = get_desktop_path ();
	desktop_dir = g_file_new_for_path (path);
	
	dirname = g_path_get_dirname (path);
	desktop_dir_dir = g_file_new_for_path (dirname);
	g_free (dirname);
	desktop_dir_filename = g_path_get_basename (path);
}

gboolean
nautilus_is_home_directory_file (GFile *dir,
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
nautilus_is_home_directory (GFile *dir)
{
	static GFile *home_dir = NULL;
	
	if (home_dir == NULL) {
		home_dir = g_file_new_for_path (g_get_home_dir ());
	}

	return g_file_equal (dir, home_dir);
}

gboolean
nautilus_is_root_directory (GFile *dir)
{
	static GFile *root_dir = NULL;
	
	if (root_dir == NULL) {
		root_dir = g_file_new_for_path ("/");
	}

	return g_file_equal (dir, root_dir);
}
		
		
gboolean
nautilus_is_desktop_directory_file (GFile *dir,
				    const char *file)
{

	if (desktop_dir == NULL) {
		update_desktop_dir ();
	}

	return (g_file_equal (dir, desktop_dir_dir) &&
		strcmp (file, desktop_dir_filename) == 0);
}

gboolean
nautilus_is_desktop_directory (GFile *dir)
{

	if (desktop_dir == NULL) {
		update_desktop_dir ();
	}

	return g_file_equal (dir, desktop_dir);
}

GMount *
nautilus_get_mounted_mount_for_root (GFile *location)
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
nautilus_ensure_unique_file_name (const char *directory_uri,
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
nautilus_find_existing_uri_in_hierarchy (GFile *location)
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
		return (symbolic) ? g_themed_icon_new (NAUTILUS_ICON_FOLDER_ ## x) : g_themed_icon_new (NAUTILUS_ICON_FULLCOLOR_FOLDER_ ## x);

	switch (directory) {

		ICON_CASE (DOCUMENTS);
		ICON_CASE (DOWNLOAD);
		ICON_CASE (MUSIC);
		ICON_CASE (PICTURES);
		ICON_CASE (PUBLIC_SHARE);
		ICON_CASE (TEMPLATES);
		ICON_CASE (VIDEOS);

	default:
		return (symbolic) ? g_themed_icon_new (NAUTILUS_ICON_FOLDER) : g_themed_icon_new (NAUTILUS_ICON_FULLCOLOR_FOLDER);
	}

#undef ICON_CASE
}

GIcon *
nautilus_special_directory_get_symbolic_icon (GUserDirectory directory)
{
	return special_directory_get_icon (directory, TRUE);
}

GIcon *
nautilus_special_directory_get_icon (GUserDirectory directory)
{
	return special_directory_get_icon (directory, FALSE);
}

gboolean
nautilus_is_file_roller_installed (void)
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

/* Returns TRUE if the file is in XDG_DATA_DIRS. This is used for
   deciding if a desktop file is "trusted" based on the path */
gboolean
nautilus_is_in_system_dir (GFile *file)
{
	const char * const * data_dirs; 
	char *path;
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

	g_free (path);

	return res;
}

GHashTable *
nautilus_trashed_files_get_original_directories (GList *files,
						 GList **unhandled_files)
{
	GHashTable *directories;
	NautilusFile *file, *original_file, *original_dir;
	GList *l, *m;

	directories = NULL;

	if (unhandled_files != NULL) {
		*unhandled_files = NULL;
	}

	for (l = files; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);
		original_file = nautilus_file_get_trash_original_file (file);

		original_dir = NULL;
		if (original_file != NULL) {
			original_dir = nautilus_file_get_parent (original_file);
		}

		if (original_dir != NULL) {
			if (directories == NULL) {
				directories = g_hash_table_new_full (g_direct_hash, g_direct_equal,
								     (GDestroyNotify) nautilus_file_unref,
								     (GDestroyNotify) nautilus_file_list_free);
			}
			nautilus_file_ref (original_dir);
			m = g_hash_table_lookup (directories, original_dir);
			if (m != NULL) {
				g_hash_table_steal (directories, original_dir);
				nautilus_file_unref (original_dir);
			}
			m = g_list_append (m, nautilus_file_ref (file));
			g_hash_table_insert (directories, original_dir, m);
		} else if (unhandled_files != NULL) {
			*unhandled_files = g_list_append (*unhandled_files, nautilus_file_ref (file));
		}

		nautilus_file_unref (original_file);
		nautilus_file_unref (original_dir);
	}

	return directories;
}

static GList *
locations_from_file_list (GList *file_list)
{
	NautilusFile *file;
	GList *l, *ret;

	ret = NULL;

	for (l = file_list; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);
		ret = g_list_prepend (ret, nautilus_file_get_location (file));
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
	NautilusFile *original_dir;
	GFile *original_dir_location;
	GList *original_dirs, *files, *locations, *l;
	RestoreFilesData *data = user_data;

	original_dirs = g_hash_table_get_keys (data->original_dirs_hash);
	for (l = original_dirs; l != NULL; l = l->next) {
		original_dir = NAUTILUS_FILE (l->data);
		original_dir_location = nautilus_file_get_location (original_dir);

		files = g_hash_table_lookup (data->original_dirs_hash, original_dir);
		locations = locations_from_file_list (files);

		nautilus_file_operations_move
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
	NautilusFile *original_dir;
	GFile *original_dir_location;
	GList *original_dirs, *l;

	original_dirs = g_hash_table_get_keys (data->original_dirs_hash);
	for (l = original_dirs; l != NULL; l = l->next) {
		original_dir = NAUTILUS_FILE (l->data);
		original_dir_location = nautilus_file_get_location (original_dir);

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
nautilus_restore_files_from_trash (GList *files,
				   GtkWindow *parent_window)
{
	NautilusFile *file;
	GHashTable *original_dirs_hash;
	GList *unhandled_files, *l;
	char *message, *file_name;

	original_dirs_hash = nautilus_trashed_files_get_original_directories (files, &unhandled_files);

	for (l = unhandled_files; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);
		file_name = nautilus_file_get_display_name (file);
		message = g_strdup_printf (_("Could not determine original location of “%s” "), file_name);
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

	nautilus_file_list_unref (unhandled_files);
}

typedef struct {
	NautilusMountGetContent callback;
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
				"nautilus-content-type-cache",
				g_strdupv (types),
				(GDestroyNotify)g_strfreev);

	if (data->callback) {
		data->callback ((const char **) types, data->user_data);
	}
	g_strfreev (types);
	g_slice_free (GetContentTypesData, data);
}

void
nautilus_get_x_content_types_for_mount_async (GMount *mount,
					      NautilusMountGetContent callback,
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

	cached = g_object_get_data (G_OBJECT (mount), "nautilus-content-type-cache");
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
nautilus_get_cached_x_content_types_for_mount (GMount *mount)
{
	char **cached;

	if (mount == NULL) {
		return NULL;
	}

	cached = g_object_get_data (G_OBJECT (mount), "nautilus-content-type-cache");
	if (cached != NULL) {
		return g_strdupv (cached);
	}

	return NULL;
}

gboolean
nautilus_file_selection_equal (GList *selection_a,
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
		GFile *a_location = nautilus_file_get_location (NAUTILUS_FILE (al->data));
		gboolean found = FALSE;

		for (bl = selection_b; bl; bl = bl->next) {
			GFile *b_location = nautilus_file_get_location (NAUTILUS_FILE (bl->data));
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

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_file_utilities (void)
{
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */

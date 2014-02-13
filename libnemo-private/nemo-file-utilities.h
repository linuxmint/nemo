/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-file-utilities.h - interface for file manipulation routines.

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

#ifndef NEMO_FILE_UTILITIES_H
#define NEMO_FILE_UTILITIES_H

#include <gio/gio.h>
#include <gtk/gtk.h>

#define NEMO_SAVED_SEARCH_EXTENSION ".savedSearch"
#define NEMO_SAVED_SEARCH_MIMETYPE "application/x-gnome-saved-search"

#define DEFAULT_NEMO_DIRECTORY_MODE (0755)
#define DEFAULT_DESKTOP_DIRECTORY_MODE (0755)

/* These functions all return something something that needs to be
 * freed with g_free, is not NULL, and is guaranteed to exist.
 */
char *   nemo_get_xdg_dir                        (const char *type);
char *   nemo_get_user_directory                 (void);
char *   nemo_get_desktop_directory              (void);
GFile *  nemo_get_desktop_location               (void);
char *   nemo_get_desktop_directory_uri          (void);
char *   nemo_get_home_directory_uri             (void);
gboolean nemo_is_desktop_directory_file          (GFile *dir,
						      const char *filename);
gboolean nemo_is_root_directory                  (GFile *dir);
gboolean nemo_is_desktop_directory               (GFile *dir);
gboolean nemo_is_home_directory                  (GFile *dir);
gboolean nemo_is_home_directory_file             (GFile *dir,
						      const char *filename);
gboolean nemo_is_in_system_dir                   (GFile *location);
GMount * nemo_get_mounted_mount_for_root         (GFile *location);

gboolean nemo_should_use_templates_directory     (void);
char *   nemo_get_templates_directory            (void);
char *   nemo_get_templates_directory_uri        (void);
void     nemo_create_templates_directory         (void);

char *   nemo_get_searches_directory             (void);

char *	 nemo_compute_title_for_location	     (GFile *file);

gboolean nemo_is_file_roller_installed           (void);

gboolean nemo_uri_parse                          (const char  *uri,
						      char       **host,
						      guint16     *port,
						      char       **userinfo);

/* Return an allocated file name that is guranteed to be unique, but
 * tries to make the name readable to users.
 * This isn't race-free, so don't use for security-related things
 */
char *   nemo_ensure_unique_file_name            (const char *directory_uri,
						      const char *base_name,
			                              const char *extension);

GFile *  nemo_find_existing_uri_in_hierarchy     (GFile *location);

char * nemo_get_accel_map_file (void);
char * nemo_get_scripts_directory_path (void);

GHashTable * nemo_trashed_files_get_original_directories (GList *files,
							      GList **unhandled_files);
void nemo_restore_files_from_trash (GList *files,
					GtkWindow *parent_window);

typedef void (*NemoMountGetContent) (const char **content, gpointer user_data);

char ** nemo_get_cached_x_content_types_for_mount (GMount *mount);
void nemo_get_x_content_types_for_mount_async (GMount *mount,
						   NemoMountGetContent callback,
						   GCancellable *cancellable,
						   gpointer user_data);

gboolean nemo_dir_has_children_now (GFile *dir, gboolean *has_subdirs);

#endif /* NEMO_FILE_UTILITIES_H */

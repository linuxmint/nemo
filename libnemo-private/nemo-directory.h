/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-directory.h: Nemo directory model.
 
   Copyright (C) 1999, 2000, 2001 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#ifndef NEMO_DIRECTORY_H
#define NEMO_DIRECTORY_H

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <libnemo-private/nemo-file-attributes.h>

/* NemoDirectory is a class that manages the model for a directory,
   real or virtual, for Nemo, mainly the file-manager component. The directory is
   responsible for managing both real data and cached metadata. On top of
   the file system independence provided by gio, the directory
   object also provides:
  
       1) A synchronization framework, which notifies via signals as the
          set of known files changes.
       2) An abstract interface for getting attributes and performing
          operations on files.
*/

#define NEMO_TYPE_DIRECTORY nemo_directory_get_type()
#define NEMO_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_DIRECTORY, NemoDirectory))
#define NEMO_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_DIRECTORY, NemoDirectoryClass))
#define NEMO_IS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_DIRECTORY))
#define NEMO_IS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_DIRECTORY))
#define NEMO_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_DIRECTORY, NemoDirectoryClass))

/* NemoFile is defined both here and in nemo-file.h. */
#ifndef NEMO_FILE_DEFINED
#define NEMO_FILE_DEFINED
typedef struct NemoFile NemoFile;
#endif

typedef struct NemoDirectoryDetails NemoDirectoryDetails;

typedef struct
{
	GObject object;
	NemoDirectoryDetails *details;
} NemoDirectory;

typedef void (*NemoDirectoryCallback) (NemoDirectory *directory,
					   GList             *files,
					   gpointer           callback_data);

typedef struct
{
	GObjectClass parent_class;

	/*** Notification signals for clients to connect to. ***/

	/* The files_added signal is emitted as the directory model 
	 * discovers new files.
	 */
	void     (* files_added)         (NemoDirectory          *directory,
					  GList                      *added_files);

	/* The files_changed signal is emitted as changes occur to
	 * existing files that are noticed by the synchronization framework,
	 * including when an old file has been deleted. When an old file
	 * has been deleted, this is the last chance to forget about these
	 * file objects, which are about to be unref'd. Use a call to
	 * nemo_file_is_gone () to test for this case.
	 */
	void     (* files_changed)       (NemoDirectory         *directory,
					  GList                     *changed_files);

	/* The done_loading signal is emitted when a directory load
	 * request completes. This is needed because, at least in the
	 * case where the directory is empty, the caller will receive
	 * no kind of notification at all when a directory load
	 * initiated by `nemo_directory_file_monitor_add' completes.
	 */
	void     (* done_loading)        (NemoDirectory         *directory);

	void     (* load_error)          (NemoDirectory         *directory,
					  GError                    *error);

	/*** Virtual functions for subclasses to override. ***/
	gboolean (* contains_file)       (NemoDirectory         *directory,
					  NemoFile              *file);
	void     (* call_when_ready)     (NemoDirectory         *directory,
					  NemoFileAttributes     file_attributes,
					  gboolean                   wait_for_file_list,
					  NemoDirectoryCallback  callback,
					  gpointer                   callback_data);
	void     (* cancel_callback)     (NemoDirectory         *directory,
					  NemoDirectoryCallback  callback,
					  gpointer                   callback_data);
	void     (* file_monitor_add)    (NemoDirectory          *directory,
					  gconstpointer              client,
					  gboolean                   monitor_hidden_files,
					  NemoFileAttributes     monitor_attributes,
					  NemoDirectoryCallback  initial_files_callback,
					  gpointer                   callback_data);
	void     (* file_monitor_remove) (NemoDirectory         *directory,
					  gconstpointer              client);
	void     (* force_reload)        (NemoDirectory         *directory);
	gboolean (* are_all_files_seen)  (NemoDirectory         *directory);
	gboolean (* is_not_empty)        (NemoDirectory         *directory);

	/* get_file_list is a function pointer that subclasses may override to
	 * customize collecting the list of files in a directory.
	 * For example, the NemoDesktopDirectory overrides this so that it can
	 * merge together the list of files in the $HOME/Desktop directory with
	 * the list of standard icons (Computer, Home, Trash) on the desktop.
	 */
	GList *	 (* get_file_list)	 (NemoDirectory *directory);

	/* Should return FALSE if the directory is read-only and doesn't
	 * allow setting of metadata.
	 * An example of this is the search directory.
	 */
	gboolean (* is_editable)         (NemoDirectory *directory);
} NemoDirectoryClass;

/* Basic GObject requirements. */
GType              nemo_directory_get_type                 (void);

/* Get a directory given a uri.
 * Creates the appropriate subclass given the uri mappings.
 * Returns a referenced object, not a floating one. Unref when finished.
 * If two windows are viewing the same uri, the directory object is shared.
 */
NemoDirectory *nemo_directory_get                      (GFile                     *location);
NemoDirectory *nemo_directory_get_by_uri               (const char                *uri);
NemoDirectory *nemo_directory_get_for_file             (NemoFile              *file);

/* Covers for g_object_ref and g_object_unref that provide two conveniences:
 * 1) Using these is type safe.
 * 2) You are allowed to call these with NULL,
 */
NemoDirectory *nemo_directory_ref                      (NemoDirectory         *directory);
void               nemo_directory_unref                    (NemoDirectory         *directory);

/* Access to a URI. */
char *             nemo_directory_get_uri                  (NemoDirectory         *directory);
GFile *            nemo_directory_get_location             (NemoDirectory         *directory);

/* Is this file still alive and in this directory? */
gboolean           nemo_directory_contains_file            (NemoDirectory         *directory,
								NemoFile              *file);

/* Get the uri of the file in the directory, NULL if not found */
char *             nemo_directory_get_file_uri             (NemoDirectory         *directory,
								const char                *file_name);

/* Get (and ref) a NemoFile object for this directory. */
NemoFile *     nemo_directory_get_corresponding_file   (NemoDirectory         *directory);

/* Waiting for data that's read asynchronously.
 * The file attribute and metadata keys are for files in the directory.
 */
void               nemo_directory_call_when_ready          (NemoDirectory         *directory,
								NemoFileAttributes     file_attributes,
								gboolean                   wait_for_all_files,
								NemoDirectoryCallback  callback,
								gpointer                   callback_data);
void               nemo_directory_cancel_callback          (NemoDirectory         *directory,
								NemoDirectoryCallback  callback,
								gpointer                   callback_data);


/* Monitor the files in a directory. */
void               nemo_directory_file_monitor_add         (NemoDirectory         *directory,
								gconstpointer              client,
								gboolean                   monitor_hidden_files,
								NemoFileAttributes     attributes,
								NemoDirectoryCallback  initial_files_callback,
								gpointer                   callback_data);
void               nemo_directory_file_monitor_remove      (NemoDirectory         *directory,
								gconstpointer              client);
void               nemo_directory_force_reload             (NemoDirectory         *directory);

/* Get a list of all files currently known in the directory. */
GList *            nemo_directory_get_file_list            (NemoDirectory         *directory);

GList *            nemo_directory_match_pattern            (NemoDirectory         *directory,
							        const char *glob);


/* Return true if the directory has information about all the files.
 * This will be false until the directory has been read at least once.
 */
gboolean           nemo_directory_are_all_files_seen       (NemoDirectory         *directory);

/* Return true if the directory is local. */
gboolean           nemo_directory_is_local                 (NemoDirectory         *directory);

gboolean           nemo_directory_is_in_trash              (NemoDirectory         *directory);

/* Return false if directory contains anything besides a Nemo metafile.
 * Only valid if directory is monitored. Used by the Trash monitor.
 */
gboolean           nemo_directory_is_not_empty             (NemoDirectory         *directory);

/* Convenience functions for dealing with a list of NemoDirectory objects that each have a ref.
 * These are just convenient names for functions that work on lists of GtkObject *.
 */
GList *            nemo_directory_list_ref                 (GList                     *directory_list);
void               nemo_directory_list_unref               (GList                     *directory_list);
void               nemo_directory_list_free                (GList                     *directory_list);
GList *            nemo_directory_list_copy                (GList                     *directory_list);
GList *            nemo_directory_list_sort_by_uri         (GList                     *directory_list);

/* Fast way to check if a directory is the desktop directory */
gboolean           nemo_directory_is_desktop_directory     (NemoDirectory         *directory);

gboolean           nemo_directory_is_editable              (NemoDirectory         *directory);


#endif /* NEMO_DIRECTORY_H */

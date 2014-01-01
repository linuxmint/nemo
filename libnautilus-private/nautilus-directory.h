/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-directory.h: Nautilus directory model.
 
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
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#ifndef NAUTILUS_DIRECTORY_H
#define NAUTILUS_DIRECTORY_H

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <libnautilus-private/nautilus-file-attributes.h>

/* NautilusDirectory is a class that manages the model for a directory,
   real or virtual, for Nautilus, mainly the file-manager component. The directory is
   responsible for managing both real data and cached metadata. On top of
   the file system independence provided by gio, the directory
   object also provides:
  
       1) A synchronization framework, which notifies via signals as the
          set of known files changes.
       2) An abstract interface for getting attributes and performing
          operations on files.
*/

#define NAUTILUS_TYPE_DIRECTORY nautilus_directory_get_type()
#define NAUTILUS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_DIRECTORY, NautilusDirectory))
#define NAUTILUS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_DIRECTORY, NautilusDirectoryClass))
#define NAUTILUS_IS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_DIRECTORY))
#define NAUTILUS_IS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_DIRECTORY))
#define NAUTILUS_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_DIRECTORY, NautilusDirectoryClass))

/* NautilusFile is defined both here and in nautilus-file.h. */
#ifndef NAUTILUS_FILE_DEFINED
#define NAUTILUS_FILE_DEFINED
typedef struct NautilusFile NautilusFile;
#endif

typedef struct NautilusDirectoryDetails NautilusDirectoryDetails;

typedef struct
{
	GObject object;
	NautilusDirectoryDetails *details;
} NautilusDirectory;

typedef void (*NautilusDirectoryCallback) (NautilusDirectory *directory,
					   GList             *files,
					   gpointer           callback_data);

typedef struct
{
	GObjectClass parent_class;

	/*** Notification signals for clients to connect to. ***/

	/* The files_added signal is emitted as the directory model 
	 * discovers new files.
	 */
	void     (* files_added)         (NautilusDirectory          *directory,
					  GList                      *added_files);

	/* The files_changed signal is emitted as changes occur to
	 * existing files that are noticed by the synchronization framework,
	 * including when an old file has been deleted. When an old file
	 * has been deleted, this is the last chance to forget about these
	 * file objects, which are about to be unref'd. Use a call to
	 * nautilus_file_is_gone () to test for this case.
	 */
	void     (* files_changed)       (NautilusDirectory         *directory,
					  GList                     *changed_files);

	/* The done_loading signal is emitted when a directory load
	 * request completes. This is needed because, at least in the
	 * case where the directory is empty, the caller will receive
	 * no kind of notification at all when a directory load
	 * initiated by `nautilus_directory_file_monitor_add' completes.
	 */
	void     (* done_loading)        (NautilusDirectory         *directory);

	void     (* load_error)          (NautilusDirectory         *directory,
					  GError                    *error);

	/*** Virtual functions for subclasses to override. ***/
	gboolean (* contains_file)       (NautilusDirectory         *directory,
					  NautilusFile              *file);
	void     (* call_when_ready)     (NautilusDirectory         *directory,
					  NautilusFileAttributes     file_attributes,
					  gboolean                   wait_for_file_list,
					  NautilusDirectoryCallback  callback,
					  gpointer                   callback_data);
	void     (* cancel_callback)     (NautilusDirectory         *directory,
					  NautilusDirectoryCallback  callback,
					  gpointer                   callback_data);
	void     (* file_monitor_add)    (NautilusDirectory          *directory,
					  gconstpointer              client,
					  gboolean                   monitor_hidden_files,
					  NautilusFileAttributes     monitor_attributes,
					  NautilusDirectoryCallback  initial_files_callback,
					  gpointer                   callback_data);
	void     (* file_monitor_remove) (NautilusDirectory         *directory,
					  gconstpointer              client);
	void     (* force_reload)        (NautilusDirectory         *directory);
	gboolean (* are_all_files_seen)  (NautilusDirectory         *directory);
	gboolean (* is_not_empty)        (NautilusDirectory         *directory);

	/* get_file_list is a function pointer that subclasses may override to
	 * customize collecting the list of files in a directory.
	 * For example, the NautilusDesktopDirectory overrides this so that it can
	 * merge together the list of files in the $HOME/Desktop directory with
	 * the list of standard icons (Home, Trash) on the desktop.
	 */
	GList *	 (* get_file_list)	 (NautilusDirectory *directory);

	/* Should return FALSE if the directory is read-only and doesn't
	 * allow setting of metadata.
	 * An example of this is the search directory.
	 */
	gboolean (* is_editable)         (NautilusDirectory *directory);
} NautilusDirectoryClass;

/* Basic GObject requirements. */
GType              nautilus_directory_get_type                 (void);

/* Get a directory given a uri.
 * Creates the appropriate subclass given the uri mappings.
 * Returns a referenced object, not a floating one. Unref when finished.
 * If two windows are viewing the same uri, the directory object is shared.
 */
NautilusDirectory *nautilus_directory_get                      (GFile                     *location);
NautilusDirectory *nautilus_directory_get_by_uri               (const char                *uri);
NautilusDirectory *nautilus_directory_get_for_file             (NautilusFile              *file);

/* Covers for g_object_ref and g_object_unref that provide two conveniences:
 * 1) Using these is type safe.
 * 2) You are allowed to call these with NULL,
 */
NautilusDirectory *nautilus_directory_ref                      (NautilusDirectory         *directory);
void               nautilus_directory_unref                    (NautilusDirectory         *directory);

/* Access to a URI. */
char *             nautilus_directory_get_uri                  (NautilusDirectory         *directory);
GFile *            nautilus_directory_get_location             (NautilusDirectory         *directory);

/* Is this file still alive and in this directory? */
gboolean           nautilus_directory_contains_file            (NautilusDirectory         *directory,
								NautilusFile              *file);

/* Get the uri of the file in the directory, NULL if not found */
char *             nautilus_directory_get_file_uri             (NautilusDirectory         *directory,
								const char                *file_name);

/* Get (and ref) a NautilusFile object for this directory. */
NautilusFile *     nautilus_directory_get_corresponding_file   (NautilusDirectory         *directory);

/* Waiting for data that's read asynchronously.
 * The file attribute and metadata keys are for files in the directory.
 */
void               nautilus_directory_call_when_ready          (NautilusDirectory         *directory,
								NautilusFileAttributes     file_attributes,
								gboolean                   wait_for_all_files,
								NautilusDirectoryCallback  callback,
								gpointer                   callback_data);
void               nautilus_directory_cancel_callback          (NautilusDirectory         *directory,
								NautilusDirectoryCallback  callback,
								gpointer                   callback_data);


/* Monitor the files in a directory. */
void               nautilus_directory_file_monitor_add         (NautilusDirectory         *directory,
								gconstpointer              client,
								gboolean                   monitor_hidden_files,
								NautilusFileAttributes     attributes,
								NautilusDirectoryCallback  initial_files_callback,
								gpointer                   callback_data);
void               nautilus_directory_file_monitor_remove      (NautilusDirectory         *directory,
								gconstpointer              client);
void               nautilus_directory_force_reload             (NautilusDirectory         *directory);

/* Get a list of all files currently known in the directory. */
GList *            nautilus_directory_get_file_list            (NautilusDirectory         *directory);

GList *            nautilus_directory_match_pattern            (NautilusDirectory         *directory,
							        const char *glob);


/* Return true if the directory has information about all the files.
 * This will be false until the directory has been read at least once.
 */
gboolean           nautilus_directory_are_all_files_seen       (NautilusDirectory         *directory);

/* Return true if the directory is local. */
gboolean           nautilus_directory_is_local                 (NautilusDirectory         *directory);

gboolean           nautilus_directory_is_in_trash              (NautilusDirectory         *directory);
gboolean           nautilus_directory_is_in_recent             (NautilusDirectory         *directory);

/* Return false if directory contains anything besides a Nautilus metafile.
 * Only valid if directory is monitored. Used by the Trash monitor.
 */
gboolean           nautilus_directory_is_not_empty             (NautilusDirectory         *directory);

/* Convenience functions for dealing with a list of NautilusDirectory objects that each have a ref.
 * These are just convenient names for functions that work on lists of GtkObject *.
 */
GList *            nautilus_directory_list_ref                 (GList                     *directory_list);
void               nautilus_directory_list_unref               (GList                     *directory_list);
void               nautilus_directory_list_free                (GList                     *directory_list);
GList *            nautilus_directory_list_copy                (GList                     *directory_list);
GList *            nautilus_directory_list_sort_by_uri         (GList                     *directory_list);

/* Fast way to check if a directory is the desktop directory */
gboolean           nautilus_directory_is_desktop_directory     (NautilusDirectory         *directory);

gboolean           nautilus_directory_is_editable              (NautilusDirectory         *directory);


#endif /* NAUTILUS_DIRECTORY_H */

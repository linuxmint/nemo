/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-file.h: Nemo file model.
 
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

#ifndef NEMO_FILE_H
#define NEMO_FILE_H

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <libnemo-private/nemo-file-attributes.h>
#include <libnemo-private/nemo-icon-info.h>

/* NemoFile is an object used to represent a single element of a
 * NemoDirectory. It's lightweight and relies on NemoDirectory
 * to do most of the work.
 */

/* NemoFile is defined both here and in nemo-directory.h. */
#ifndef NEMO_FILE_DEFINED
#define NEMO_FILE_DEFINED
typedef struct NemoFile NemoFile;
#endif

#define NEMO_TYPE_FILE nemo_file_get_type()
#define NEMO_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_FILE, NemoFile))
#define NEMO_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_FILE, NemoFileClass))
#define NEMO_IS_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_FILE))
#define NEMO_IS_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_FILE))
#define NEMO_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_FILE, NemoFileClass))

typedef enum {
	NEMO_FILE_SORT_NONE,
	NEMO_FILE_SORT_BY_DISPLAY_NAME,
	NEMO_FILE_SORT_BY_SIZE,
	NEMO_FILE_SORT_BY_TYPE,
    NEMO_FILE_SORT_BY_DETAILED_TYPE,
	NEMO_FILE_SORT_BY_MTIME,
        NEMO_FILE_SORT_BY_ATIME,
	NEMO_FILE_SORT_BY_TRASHED_TIME
} NemoFileSortType;	

typedef enum {
	NEMO_REQUEST_NOT_STARTED,
	NEMO_REQUEST_IN_PROGRESS,
	NEMO_REQUEST_DONE
} NemoRequestStatus;

typedef enum {
	NEMO_FILE_ICON_FLAGS_NONE = 0,
	NEMO_FILE_ICON_FLAGS_USE_THUMBNAILS = (1<<0),
	NEMO_FILE_ICON_FLAGS_IGNORE_VISITING = (1<<1),
	NEMO_FILE_ICON_FLAGS_EMBEDDING_TEXT = (1<<2),
	NEMO_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT = (1<<3),
	NEMO_FILE_ICON_FLAGS_FOR_OPEN_FOLDER = (1<<4),
	/* whether the thumbnail size must match the display icon size */
	NEMO_FILE_ICON_FLAGS_FORCE_THUMBNAIL_SIZE = (1<<5),
	/* uses the icon of the mount if present */
	NEMO_FILE_ICON_FLAGS_USE_MOUNT_ICON = (1<<6),
	/* render the mount icon as an emblem over the regular one */
	NEMO_FILE_ICON_FLAGS_USE_MOUNT_ICON_AS_EMBLEM = (1<<7)
} NemoFileIconFlags;	

typedef enum {
    NEMO_DATE_TYPE_MODIFIED,
    NEMO_DATE_TYPE_CHANGED,
    NEMO_DATE_TYPE_ACCESSED,
    NEMO_DATE_TYPE_PERMISSIONS_CHANGED,
    NEMO_DATE_TYPE_TRASHED
} NemoDateType;

typedef enum {
    NEMO_FILE_TOOLTIP_FLAGS_NONE = 0,
    NEMO_FILE_TOOLTIP_FLAGS_FILE_TYPE =  (1<<0),
    NEMO_FILE_TOOLTIP_FLAGS_MOD_DATE = (1<<1),
    NEMO_FILE_TOOLTIP_FLAGS_ACCESS_DATE = (1<<2),
    NEMO_FILE_TOOLTIP_FLAGS_PATH = (1<<3)
} NemoFileTooltipFlags;

/* Emblems sometimes displayed for NemoFiles. Do not localize. */ 
#define NEMO_FILE_EMBLEM_NAME_SYMBOLIC_LINK "symbolic-link"
#define NEMO_FILE_EMBLEM_NAME_CANT_READ "noread"
#define NEMO_FILE_EMBLEM_NAME_CANT_WRITE "nowrite"
#define NEMO_FILE_EMBLEM_NAME_TRASH "trash"
#define NEMO_FILE_EMBLEM_NAME_NOTE "note"

typedef void (*NemoFileCallback)          (NemoFile  *file,
				               gpointer       callback_data);
typedef void (*NemoFileListCallback)      (GList         *file_list,
				               gpointer       callback_data);
typedef void (*NemoFileOperationCallback) (NemoFile  *file,
					       GFile         *result_location,
					       GError        *error,
					       gpointer       callback_data);
typedef int (*NemoWidthMeasureCallback)   (const char    *string,
					       void	     *context);
typedef char * (*NemoTruncateCallback)    (const char    *string,
					       int	      width,
					       void	     *context);


#define NEMO_FILE_ATTRIBUTES_FOR_ICON (NEMO_FILE_ATTRIBUTE_INFO | NEMO_FILE_ATTRIBUTE_LINK_INFO | NEMO_FILE_ATTRIBUTE_THUMBNAIL)

typedef void NemoFileListHandle;

/* GObject requirements. */
GType                   nemo_file_get_type                          (void);

/* Getting at a single file. */
NemoFile *          nemo_file_get                               (GFile                          *location);
NemoFile *          nemo_file_get_by_uri                        (const char                     *uri);

/* Get a file only if the nemo version already exists */
NemoFile *          nemo_file_get_existing                      (GFile                          *location);
NemoFile *          nemo_file_get_existing_by_uri               (const char                     *uri);

/* Covers for g_object_ref and g_object_unref that provide two conveniences:
 * 1) Using these is type safe.
 * 2) You are allowed to call these with NULL,
 */
NemoFile *          nemo_file_ref                               (NemoFile                   *file);
void                    nemo_file_unref                             (NemoFile                   *file);

/* Monitor the file. */
void                    nemo_file_monitor_add                       (NemoFile                   *file,
									 gconstpointer                   client,
									 NemoFileAttributes          attributes);
void                    nemo_file_monitor_remove                    (NemoFile                   *file,
									 gconstpointer                   client);

/* Waiting for data that's read asynchronously.
 * This interface currently works only for metadata, but could be expanded
 * to other attributes as well.
 */
void                    nemo_file_call_when_ready                   (NemoFile                   *file,
									 NemoFileAttributes          attributes,
									 NemoFileCallback            callback,
									 gpointer                        callback_data);
void                    nemo_file_cancel_call_when_ready            (NemoFile                   *file,
									 NemoFileCallback            callback,
									 gpointer                        callback_data);
gboolean                nemo_file_check_if_ready                    (NemoFile                   *file,
									 NemoFileAttributes          attributes);
void                    nemo_file_invalidate_attributes             (NemoFile                   *file,
									 NemoFileAttributes          attributes);
void                    nemo_file_invalidate_all_attributes         (NemoFile                   *file);

void                    nemo_file_increment_thumbnail_try_count     (NemoFile                   *file);

/* Basic attributes for file objects. */
gboolean                nemo_file_contains_text                     (NemoFile                   *file);
char *                  nemo_file_get_display_name                  (NemoFile                   *file);
char *                  nemo_file_get_edit_name                     (NemoFile                   *file);
char *                  nemo_file_get_name                          (NemoFile                   *file);
GFile *                 nemo_file_get_location                      (NemoFile                   *file);
char *			 nemo_file_get_description			 (NemoFile			 *file);
char *                  nemo_file_get_uri                           (NemoFile                   *file);
char *                  nemo_file_get_path                          (NemoFile                   *file);
char *                  nemo_file_get_uri_scheme                    (NemoFile                   *file);
NemoFile *          nemo_file_get_parent                        (NemoFile                   *file);
GFile *                 nemo_file_get_parent_location               (NemoFile                   *file);
char *                  nemo_file_get_parent_uri                    (NemoFile                   *file);
char *                  nemo_file_get_parent_uri_for_display        (NemoFile                   *file);
gboolean                nemo_file_can_get_size                      (NemoFile                   *file);
goffset                 nemo_file_get_size                          (NemoFile                   *file);
time_t                  nemo_file_get_mtime                         (NemoFile                   *file);
time_t                  nemo_file_get_ctime                         (NemoFile                   *file);
GFileType               nemo_file_get_file_type                     (NemoFile                   *file);
char *                  nemo_file_get_mime_type                     (NemoFile                   *file);
gboolean                nemo_file_is_mime_type                      (NemoFile                   *file,
									 const char                     *mime_type);
gboolean                nemo_file_is_launchable                     (NemoFile                   *file);
gboolean                nemo_file_is_symbolic_link                  (NemoFile                   *file);
gboolean                nemo_file_is_mountpoint                     (NemoFile                   *file);
GMount *                nemo_file_get_mount                         (NemoFile                   *file);
char *                  nemo_file_get_volume_free_space             (NemoFile                   *file);
char *                  nemo_file_get_volume_name                   (NemoFile                   *file);
char *                  nemo_file_get_symbolic_link_target_path     (NemoFile                   *file);
char *                  nemo_file_get_symbolic_link_target_uri      (NemoFile                   *file);
gboolean                nemo_file_is_broken_symbolic_link           (NemoFile                   *file);
gboolean                nemo_file_is_nemo_link                  (NemoFile                   *file);
gboolean                nemo_file_is_executable                     (NemoFile                   *file);
gboolean                nemo_file_is_directory                      (NemoFile                   *file);
gboolean                nemo_file_is_user_special_directory         (NemoFile                   *file,
									 GUserDirectory                 special_directory);
gboolean		nemo_file_is_archive			(NemoFile			*file);
gboolean                nemo_file_is_in_trash                       (NemoFile                   *file);
gboolean                nemo_file_is_in_desktop                     (NemoFile                   *file);
gboolean		nemo_file_is_home				(NemoFile                   *file);
gboolean                nemo_file_is_desktop_directory              (NemoFile                   *file);
GError *                nemo_file_get_file_info_error               (NemoFile                   *file);
gboolean                nemo_file_get_directory_item_count          (NemoFile                   *file,
									 guint                          *count,
									 gboolean                       *count_unreadable);
void                    nemo_file_recompute_deep_counts             (NemoFile                   *file);
NemoRequestStatus   nemo_file_get_deep_counts                   (NemoFile                   *file,
									 guint                          *directory_count,
									 guint                          *file_count,
									 guint                          *unreadable_directory_count,
                                     guint                          *hidden_count,
									 goffset                         *total_size,
									 gboolean                        force);
gboolean                nemo_file_should_show_thumbnail             (NemoFile                   *file);
gboolean                nemo_file_should_show_directory_item_count  (NemoFile                   *file);
gboolean                nemo_file_should_show_type                  (NemoFile                   *file);
GList *                 nemo_file_get_keywords                      (NemoFile                   *file);
GList *                 nemo_file_get_emblem_icons                  (NemoFile                   *file);
char *                  nemo_file_get_top_left_text                 (NemoFile                   *file);
char *                  nemo_file_peek_top_left_text                (NemoFile                   *file,
									 gboolean                        need_large_text,
									 gboolean                       *got_top_left_text);
gboolean                nemo_file_get_directory_item_mime_types     (NemoFile                   *file,
									 GList                         **mime_list);

void                    nemo_file_set_attributes                    (NemoFile                   *file, 
									 GFileInfo                      *attributes,
									 NemoFileOperationCallback   callback,
									 gpointer                        callback_data);
GFilesystemPreviewType  nemo_file_get_filesystem_use_preview        (NemoFile *file);

char *                  nemo_file_get_filesystem_id                 (NemoFile                   *file);

NemoFile *          nemo_file_get_trash_original_file           (NemoFile                   *file);

/* Permissions. */
gboolean                nemo_file_can_get_permissions               (NemoFile                   *file);
gboolean                nemo_file_can_set_permissions               (NemoFile                   *file);
guint                   nemo_file_get_permissions                   (NemoFile                   *file);
gboolean                nemo_file_can_get_owner                     (NemoFile                   *file);
gboolean                nemo_file_can_set_owner                     (NemoFile                   *file);
gboolean                nemo_file_can_get_group                     (NemoFile                   *file);
gboolean                nemo_file_can_set_group                     (NemoFile                   *file);
char *                  nemo_file_get_owner_name                    (NemoFile                   *file);
char *                  nemo_file_get_group_name                    (NemoFile                   *file);
GList *                 nemo_get_user_names                         (void);
GList *                 nemo_get_all_group_names                    (void);
GList *                 nemo_file_get_settable_group_names          (NemoFile                   *file);
gboolean                nemo_file_can_get_selinux_context           (NemoFile                   *file);
char *                  nemo_file_get_selinux_context               (NemoFile                   *file);

/* "Capabilities". */
gboolean                nemo_file_can_read                          (NemoFile                   *file);
gboolean                nemo_file_can_write                         (NemoFile                   *file);
gboolean                nemo_file_can_execute                       (NemoFile                   *file);
gboolean                nemo_file_can_rename                        (NemoFile                   *file);
gboolean                nemo_file_can_delete                        (NemoFile                   *file);
gboolean                nemo_file_can_trash                         (NemoFile                   *file);

gboolean                nemo_file_can_mount                         (NemoFile                   *file);
gboolean                nemo_file_can_unmount                       (NemoFile                   *file);
gboolean                nemo_file_can_eject                         (NemoFile                   *file);
gboolean                nemo_file_can_start                         (NemoFile                   *file);
gboolean                nemo_file_can_start_degraded                (NemoFile                   *file);
gboolean                nemo_file_can_stop                          (NemoFile                   *file);
GDriveStartStopType     nemo_file_get_start_stop_type               (NemoFile                   *file);
gboolean                nemo_file_can_poll_for_media                (NemoFile                   *file);
gboolean                nemo_file_is_media_check_automatic          (NemoFile                   *file);

void                    nemo_file_mount                             (NemoFile                   *file,
									 GMountOperation                *mount_op,
									 GCancellable                   *cancellable,
									 NemoFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nemo_file_unmount                           (NemoFile                   *file,
									 GMountOperation                *mount_op,
									 GCancellable                   *cancellable,
									 NemoFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nemo_file_eject                             (NemoFile                   *file,
									 GMountOperation                *mount_op,
									 GCancellable                   *cancellable,
									 NemoFileOperationCallback   callback,
									 gpointer                        callback_data);

void                    nemo_file_start                             (NemoFile                   *file,
									 GMountOperation                *start_op,
									 GCancellable                   *cancellable,
									 NemoFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nemo_file_stop                              (NemoFile                   *file,
									 GMountOperation                *mount_op,
									 GCancellable                   *cancellable,
									 NemoFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nemo_file_poll_for_media                    (NemoFile                   *file);

/* Basic operations for file objects. */
void                    nemo_file_set_owner                         (NemoFile                   *file,
									 const char                     *user_name_or_id,
									 NemoFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nemo_file_set_group                         (NemoFile                   *file,
									 const char                     *group_name_or_id,
									 NemoFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nemo_file_set_permissions                   (NemoFile                   *file,
									 guint32                         permissions,
									 NemoFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nemo_file_rename                            (NemoFile                   *file,
									 const char                     *new_name,
									 NemoFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nemo_file_cancel                            (NemoFile                   *file,
									 NemoFileOperationCallback   callback,
									 gpointer                        callback_data);

/* Return true if this file has already been deleted.
 * This object will be unref'd after sending the files_removed signal,
 * but it could hang around longer if someone ref'd it.
 */
gboolean                nemo_file_is_gone                           (NemoFile                   *file);

/* Return true if this file is not confirmed to have ever really
 * existed. This is true when the NemoFile object has been created, but no I/O
 * has yet confirmed the existence of a file by that name.
 */
gboolean                nemo_file_is_not_yet_confirmed              (NemoFile                   *file);

/* Simple getting and setting top-level metadata. */
char *                  nemo_file_get_metadata                      (NemoFile                   *file,
									 const char                     *key,
									 const char                     *default_metadata);
GList *                 nemo_file_get_metadata_list                 (NemoFile                   *file,
									 const char                     *key);
void                    nemo_file_set_metadata                      (NemoFile                   *file,
									 const char                     *key,
									 const char                     *default_metadata,
									 const char                     *metadata);
void                    nemo_file_set_metadata_list                 (NemoFile                   *file,
									 const char                     *key,
									 GList                          *list);

/* Covers for common data types. */
gboolean                nemo_file_get_boolean_metadata              (NemoFile                   *file,
									 const char                     *key,
									 gboolean                        default_metadata);
void                    nemo_file_set_boolean_metadata              (NemoFile                   *file,
									 const char                     *key,
									 gboolean                        default_metadata,
									 gboolean                        metadata);
int                     nemo_file_get_integer_metadata              (NemoFile                   *file,
									 const char                     *key,
									 int                             default_metadata);
void                    nemo_file_set_integer_metadata              (NemoFile                   *file,
									 const char                     *key,
									 int                             default_metadata,
									 int                             metadata);

#define UNDEFINED_TIME ((time_t) (-1))

time_t                  nemo_file_get_time_metadata                 (NemoFile                  *file,
									 const char                    *key);
void                    nemo_file_set_time_metadata                 (NemoFile                  *file,
									 const char                    *key,
									 time_t                         time);


/* Attributes for file objects as user-displayable strings. */
char *                  nemo_file_get_string_attribute              (NemoFile                   *file,
									 const char                     *attribute_name);
char *                  nemo_file_get_string_attribute_q            (NemoFile                   *file,
									 GQuark                          attribute_q);
char *                  nemo_file_get_string_attribute_with_default (NemoFile                   *file,
									 const char                     *attribute_name);
char *                  nemo_file_get_string_attribute_with_default_q (NemoFile                  *file,
									 GQuark                          attribute_q);
char *			nemo_file_fit_modified_date_as_string	(NemoFile 			*file,
									 int				 width,
									 NemoWidthMeasureCallback    measure_callback,
									 NemoTruncateCallback	 truncate_callback,
									 void				*measure_truncate_context);

/* Matching with another URI. */
gboolean                nemo_file_matches_uri                       (NemoFile                   *file,
									 const char                     *uri);

/* Is the file local? */
gboolean                nemo_file_is_local                          (NemoFile                   *file);

/* Comparing two file objects for sorting */
NemoFileSortType    nemo_file_get_default_sort_type             (NemoFile                   *file,
									 gboolean                       *reversed);
const gchar *           nemo_file_get_default_sort_attribute        (NemoFile                   *file,
									 gboolean                       *reversed);

int                     nemo_file_compare_for_sort                  (NemoFile                   *file_1,
									 NemoFile                   *file_2,
									 NemoFileSortType            sort_type,
									 gboolean			 directories_first,
									 gboolean		  	 reversed);
int                     nemo_file_compare_for_sort_by_attribute     (NemoFile                   *file_1,
									 NemoFile                   *file_2,
									 const char                     *attribute,
									 gboolean                        directories_first,
									 gboolean                        reversed);
int                     nemo_file_compare_for_sort_by_attribute_q   (NemoFile                   *file_1,
									 NemoFile                   *file_2,
									 GQuark                          attribute,
									 gboolean                        directories_first,
									 gboolean                        reversed);
gboolean                nemo_file_is_date_sort_attribute_q          (GQuark                          attribute);

int                     nemo_file_compare_display_name              (NemoFile                   *file_1,
									 const char                     *pattern);
int                     nemo_file_compare_location                  (NemoFile                    *file_1,
                                                                         NemoFile                    *file_2);

/* filtering functions for use by various directory views */
gboolean                nemo_file_is_hidden_file                    (NemoFile                   *file);
gboolean                nemo_file_should_show                       (NemoFile                   *file,
									 gboolean                        show_hidden,
									 gboolean                        show_foreign);
GList                  *nemo_file_list_filter_hidden                (GList                          *files,
									 gboolean                        show_hidden);


/* Get the URI that's used when activating the file.
 * Getting this can require reading the contents of the file.
 */
gboolean                nemo_file_is_launcher                       (NemoFile                   *file);
gboolean                nemo_file_is_foreign_link                   (NemoFile                   *file);
gboolean                nemo_file_is_trusted_link                   (NemoFile                   *file);
gboolean                nemo_file_has_activation_uri                (NemoFile                   *file);
char *                  nemo_file_get_activation_uri                (NemoFile                   *file);
GFile *                 nemo_file_get_activation_location           (NemoFile                   *file);

char *                  nemo_file_get_drop_target_uri               (NemoFile                   *file);

GIcon *                 nemo_file_get_gicon                         (NemoFile                   *file,
									 NemoFileIconFlags           flags);
NemoIconInfo *      nemo_file_get_icon                          (NemoFile                   *file,
									 int                             size,
									 NemoFileIconFlags           flags);
GdkPixbuf *             nemo_file_get_icon_pixbuf                   (NemoFile                   *file,
									 int                             size,
									 gboolean                        force_size,
									 NemoFileIconFlags           flags);

gboolean                nemo_file_has_open_window                   (NemoFile                   *file);
void                    nemo_file_set_has_open_window               (NemoFile                   *file,
									 gboolean                        has_open_window);

/* Thumbnailing handling */
gboolean                nemo_file_is_thumbnailing                   (NemoFile                   *file);

/* Convenience functions for dealing with a list of NemoFile objects that each have a ref.
 * These are just convenient names for functions that work on lists of GtkObject *.
 */
GList *                 nemo_file_list_ref                          (GList                          *file_list);
void                    nemo_file_list_unref                        (GList                          *file_list);
void                    nemo_file_list_free                         (GList                          *file_list);
GList *                 nemo_file_list_copy                         (GList                          *file_list);
GList *                 nemo_file_list_from_uris                    (GList                          *uri_list);
GList *			nemo_file_list_sort_by_display_name		(GList				*file_list);
void                    nemo_file_list_call_when_ready              (GList                          *file_list,
									 NemoFileAttributes          attributes,
									 NemoFileListHandle        **handle,
									 NemoFileListCallback        callback,
									 gpointer                        callback_data);
void                    nemo_file_list_cancel_call_when_ready       (NemoFileListHandle         *handle);

char *   nemo_file_get_owner_as_string            (NemoFile          *file,
                                                          gboolean           include_real_name);
char *   nemo_file_get_type_as_string             (NemoFile          *file);
char *   nemo_file_get_detailed_type_as_string    (NemoFile          *file);

char *   nemo_file_get_date_as_string             (NemoFile *file, NemoDateType date_type);

gchar *  nemo_file_construct_tooltip              (NemoFile *file, NemoFileTooltipFlags flags);

/* Debugging */
void                    nemo_file_dump                              (NemoFile                   *file);

typedef struct NemoFileDetails NemoFileDetails;

struct NemoFile {
	GObject parent_slot;
	NemoFileDetails *details;
};

typedef struct {
	GObjectClass parent_slot;

	/* Subclasses can set this to something other than G_FILE_TYPE_UNKNOWN and
	   it will be used as the default file type. This is useful when creating
	   a "virtual" NemoFile subclass that you can't actually get real
	   information about. For exaple NemoDesktopDirectoryFile. */
	GFileType default_file_type; 
	
	/* Called when the file notices any change. */
	void                  (* changed)                (NemoFile *file);

	/* Called periodically while directory deep count is being computed. */
	void                  (* updated_deep_count_in_progress) (NemoFile *file);

	/* Virtual functions (mainly used for trash directory). */
	void                  (* monitor_add)            (NemoFile           *file,
							  gconstpointer           client,
							  NemoFileAttributes  attributes);
	void                  (* monitor_remove)         (NemoFile           *file,
							  gconstpointer           client);
	void                  (* call_when_ready)        (NemoFile           *file,
							  NemoFileAttributes  attributes,
							  NemoFileCallback    callback,
							  gpointer                callback_data);
	void                  (* cancel_call_when_ready) (NemoFile           *file,
							  NemoFileCallback    callback,
							  gpointer                callback_data);
	gboolean              (* check_if_ready)         (NemoFile           *file,
							  NemoFileAttributes  attributes);
	gboolean              (* get_item_count)         (NemoFile           *file,
							  guint                  *count,
							  gboolean               *count_unreadable);
	NemoRequestStatus (* get_deep_counts)        (NemoFile           *file,
							  guint                  *directory_count,
							  guint                  *file_count,
							  guint                  *unreadable_directory_count,
                              guint                  *hidden_count,
							  goffset       *total_size);
	gboolean              (* get_date)               (NemoFile           *file,
							  NemoDateType        type,
							  time_t                 *date);
	char *                (* get_where_string)       (NemoFile           *file);

	void                  (* set_metadata)           (NemoFile           *file,
							  const char             *key,
							  const char             *value);
	void                  (* set_metadata_as_list)   (NemoFile           *file,
							  const char             *key,
							  char                  **value);
	
	void                  (* mount)                  (NemoFile                   *file,
							  GMountOperation                *mount_op,
							  GCancellable                   *cancellable,
							  NemoFileOperationCallback   callback,
							  gpointer                        callback_data);
	void                 (* unmount)                 (NemoFile                   *file,
							  GMountOperation                *mount_op,
							  GCancellable                   *cancellable,
							  NemoFileOperationCallback   callback,
							  gpointer                        callback_data);
	void                 (* eject)                   (NemoFile                   *file,
							  GMountOperation                *mount_op,
							  GCancellable                   *cancellable,
							  NemoFileOperationCallback   callback,
							  gpointer                        callback_data);

	void                  (* start)                  (NemoFile                   *file,
							  GMountOperation                *start_op,
							  GCancellable                   *cancellable,
							  NemoFileOperationCallback   callback,
							  gpointer                        callback_data);
	void                 (* stop)                    (NemoFile                   *file,
							  GMountOperation                *mount_op,
							  GCancellable                   *cancellable,
							  NemoFileOperationCallback   callback,
							  gpointer                        callback_data);

	void                 (* poll_for_media)          (NemoFile                   *file);
} NemoFileClass;

#endif /* NEMO_FILE_H */

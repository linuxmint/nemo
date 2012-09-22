/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-directory-private.h: Nemo directory model.
 
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

#include <gio/gio.h>
#include <eel/eel-vfs-extensions.h>
#include <libnemo-private/nemo-directory.h>
#include <libnemo-private/nemo-file-queue.h>
#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-monitor.h>
#include <libnemo-extension/nemo-info-provider.h>
#include <libxml/tree.h>

typedef struct LinkInfoReadState LinkInfoReadState;
typedef struct TopLeftTextReadState TopLeftTextReadState;
typedef struct FileMonitors FileMonitors;
typedef struct DirectoryLoadState DirectoryLoadState;
typedef struct DirectoryCountState DirectoryCountState;
typedef struct DeepCountState DeepCountState;
typedef struct GetInfoState GetInfoState;
typedef struct NewFilesState NewFilesState;
typedef struct MimeListState MimeListState;
typedef struct ThumbnailState ThumbnailState;
typedef struct MountState MountState;
typedef struct FilesystemInfoState FilesystemInfoState;

typedef enum {
	REQUEST_LINK_INFO,
	REQUEST_DEEP_COUNT,
	REQUEST_DIRECTORY_COUNT,
	REQUEST_FILE_INFO,
	REQUEST_FILE_LIST, /* always FALSE if file != NULL */
	REQUEST_MIME_LIST,
	REQUEST_TOP_LEFT_TEXT,
	REQUEST_LARGE_TOP_LEFT_TEXT,
	REQUEST_EXTENSION_INFO,
	REQUEST_THUMBNAIL,
	REQUEST_MOUNT,
	REQUEST_FILESYSTEM_INFO,
	REQUEST_TYPE_LAST
} RequestType;

/* A request for information about one or more files. */
typedef guint32 Request;
typedef gint32 RequestCounter[REQUEST_TYPE_LAST];

#define REQUEST_WANTS_TYPE(request, type) ((request) & (1<<(type)))
#define REQUEST_SET_TYPE(request, type) (request) |= (1<<(type))

struct NemoDirectoryDetails
{
	/* The location. */
	GFile *location;

	/* The file objects. */
	NemoFile *as_file;
	GList *file_list;
	GHashTable *file_hash;

	/* Queues of files needing some I/O done. */
	NemoFileQueue *high_priority_queue;
	NemoFileQueue *low_priority_queue;
	NemoFileQueue *extension_queue;

	/* These lists are going to be pretty short.  If we think they
	 * are going to get big, we can use hash tables instead.
	 */
	GList *call_when_ready_list;
	RequestCounter call_when_ready_counters;
	GList *monitor_list;
	RequestCounter monitor_counters;
	guint call_ready_idle_id;

	NemoMonitor *monitor;
	gulong 		 mime_db_monitor;

	gboolean in_async_service_loop;
	gboolean state_changed;

	gboolean file_list_monitored;
	gboolean directory_loaded;
	gboolean directory_loaded_sent_notification;
	DirectoryLoadState *directory_load_in_progress;

	GList *pending_file_info; /* list of GnomeVFSFileInfo's that are pending */
	int confirmed_file_count;
        guint dequeue_pending_idle_id;

	GList *new_files_in_progress; /* list of NewFilesState * */

	DirectoryCountState *count_in_progress;

	NemoFile *deep_count_file;
	DeepCountState *deep_count_in_progress;

	MimeListState *mime_list_in_progress;

	NemoFile *get_info_file;
	GetInfoState *get_info_in_progress;

	NemoFile *extension_info_file;
	NemoInfoProvider *extension_info_provider;
	NemoOperationHandle *extension_info_in_progress;
	guint extension_info_idle;

	ThumbnailState *thumbnail_state;

	MountState *mount_state;

	FilesystemInfoState *filesystem_info_state;
	
	TopLeftTextReadState *top_left_read_state;

	LinkInfoReadState *link_info_read_state;

	GList *file_operations_in_progress; /* list of FileOperation * */

	GHashTable *hidden_file_hash;
};

NemoDirectory *nemo_directory_get_existing                    (GFile                     *location);

/* async. interface */
void               nemo_directory_async_state_changed             (NemoDirectory         *directory);
void               nemo_directory_call_when_ready_internal        (NemoDirectory         *directory,
								       NemoFile              *file,
								       NemoFileAttributes     file_attributes,
								       gboolean                   wait_for_file_list,
								       NemoDirectoryCallback  directory_callback,
								       NemoFileCallback       file_callback,
								       gpointer                   callback_data);
gboolean           nemo_directory_check_if_ready_internal         (NemoDirectory         *directory,
								       NemoFile              *file,
								       NemoFileAttributes     file_attributes);
void               nemo_directory_cancel_callback_internal        (NemoDirectory         *directory,
								       NemoFile              *file,
								       NemoDirectoryCallback  directory_callback,
								       NemoFileCallback       file_callback,
								       gpointer                   callback_data);
void               nemo_directory_monitor_add_internal            (NemoDirectory         *directory,
								       NemoFile              *file,
								       gconstpointer              client,
								       gboolean                   monitor_hidden_files,
								       NemoFileAttributes     attributes,
								       NemoDirectoryCallback  callback,
								       gpointer                   callback_data);
void               nemo_directory_monitor_remove_internal         (NemoDirectory         *directory,
								       NemoFile              *file,
								       gconstpointer              client);
void               nemo_directory_get_info_for_new_files          (NemoDirectory         *directory,
								       GList                     *vfs_uris);
NemoFile *     nemo_directory_get_existing_corresponding_file (NemoDirectory         *directory);
void               nemo_directory_invalidate_count_and_mime_list  (NemoDirectory         *directory);
gboolean           nemo_directory_is_file_list_monitored          (NemoDirectory         *directory);
gboolean           nemo_directory_is_anyone_monitoring_file_list  (NemoDirectory         *directory);
gboolean           nemo_directory_has_active_request_for_file     (NemoDirectory         *directory,
								       NemoFile              *file);
void               nemo_directory_remove_file_monitor_link        (NemoDirectory         *directory,
								       GList                     *link);
void               nemo_directory_schedule_dequeue_pending        (NemoDirectory         *directory);
void               nemo_directory_stop_monitoring_file_list       (NemoDirectory         *directory);
void               nemo_directory_cancel                          (NemoDirectory         *directory);
void               nemo_async_destroying_file                     (NemoFile              *file);
void               nemo_directory_force_reload_internal           (NemoDirectory         *directory,
								       NemoFileAttributes     file_attributes);
void               nemo_directory_cancel_loading_file_attributes  (NemoDirectory         *directory,
								       NemoFile              *file,
								       NemoFileAttributes     file_attributes);

/* Calls shared between directory, file, and async. code. */
void               nemo_directory_emit_files_added                (NemoDirectory         *directory,
								       GList                     *added_files);
void               nemo_directory_emit_files_changed              (NemoDirectory         *directory,
								       GList                     *changed_files);
void               nemo_directory_emit_change_signals             (NemoDirectory         *directory,
								       GList                     *changed_files);
void               emit_change_signals_for_all_files		      (NemoDirectory	 *directory);
void               emit_change_signals_for_all_files_in_all_directories (void);
void               nemo_directory_emit_done_loading               (NemoDirectory         *directory);
void               nemo_directory_emit_load_error                 (NemoDirectory         *directory,
								       GError                    *error);
NemoDirectory *nemo_directory_get_internal                    (GFile                     *location,
								       gboolean                   create);
char *             nemo_directory_get_name_for_self_as_new_file   (NemoDirectory         *directory);
Request            nemo_directory_set_up_request                  (NemoFileAttributes     file_attributes);

/* Interface to the file list. */
NemoFile *     nemo_directory_find_file_by_name               (NemoDirectory         *directory,
								       const char                *filename);
NemoFile *     nemo_directory_find_file_by_internal_filename  (NemoDirectory         *directory,
								       const char                *internal_filename);

void               nemo_directory_add_file                        (NemoDirectory         *directory,
								       NemoFile              *file);
void               nemo_directory_remove_file                     (NemoDirectory         *directory,
								       NemoFile              *file);
FileMonitors *     nemo_directory_remove_file_monitors            (NemoDirectory         *directory,
								       NemoFile              *file);
void               nemo_directory_add_file_monitors               (NemoDirectory         *directory,
								       NemoFile              *file,
								       FileMonitors              *monitors);
void               nemo_directory_add_file                        (NemoDirectory         *directory,
								       NemoFile              *file);
GList *            nemo_directory_begin_file_name_change          (NemoDirectory         *directory,
								       NemoFile              *file);
void               nemo_directory_end_file_name_change            (NemoDirectory         *directory,
								       NemoFile              *file,
								       GList                     *node);
void               nemo_directory_moved                           (const char                *from_uri,
								       const char                *to_uri);
/* Interface to the work queue. */

void               nemo_directory_add_file_to_work_queue          (NemoDirectory *directory,
								       NemoFile *file);
void               nemo_directory_remove_file_from_work_queue     (NemoDirectory *directory,
								       NemoFile *file);

/* KDE compatibility hacks */

void               nemo_set_kde_trash_name                        (const char *trash_dir);

/* debugging functions */
int                nemo_directory_number_outstanding              (void);

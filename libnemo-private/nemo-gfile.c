/* nemo-gfile.c
 *
 * Copyright (C) 2006-2023 Red Hat, Inc.
 * Copyright (C) 2026 Nemo Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#include "nemo-gfile.h"
#include "glibconfig.h"
#include <errno.h>
#include <fcntl.h>
#include <gio/gio.h> /* For GIOError, g_io_error_from_errno, etc. */
#include <glib.h> /* For GError, g_set_error, etc. */
#include <glib/gprintf.h>
#include <glib/gstdio.h> /* For g_file_error_from_errno (deprecated, but included for compatibility) */
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#include <linux/fs.h>
#include <sys/syscall.h>

/* Helper function to safely call pathconf */
static long
safe_pathconf (const char *path, int name)
{
	long result = pathconf (path, name);
	if (result == -1 && errno == 0) {
		return -1; /* Parameter not supported */
	}
	return result;
}

/* Helper function to get the minimum recommended buffer size from the
 * filesystem */
static size_t
get_min_buffer_size (const char *path)
{
	if (path == NULL) {
		return NEMO_G_FILE_MIN_BUFFER_SIZE;
	}

	/* Try POSIX recommended increment for file transfers */
	long rec_increment = safe_pathconf (path, _PC_REC_INCR_XFER_SIZE);
	if (rec_increment > 0) {
		return MIN (MAX (NEMO_G_FILE_MIN_BUFFER_SIZE,
				 (size_t)rec_increment),
			    NEMO_G_FILE_MAX_BUFFER_SIZE);
	}

	/* Try Linux-specific minimum allocation size */
	long alloc_size_min = safe_pathconf (path, _PC_ALLOC_SIZE_MIN);
	if (alloc_size_min > 0) {
		return MIN (MAX (NEMO_G_FILE_MIN_BUFFER_SIZE,
				 (size_t)alloc_size_min),
			    NEMO_G_FILE_MAX_BUFFER_SIZE);
	}

	/* Fall back to statvfs */
	struct statvfs fs_info;
	if (statvfs (path, &fs_info) == 0) {
		size_t block_size =
			fs_info.f_frsize; /* Filesystem fragment size */
		return MIN (MAX (NEMO_G_FILE_MIN_BUFFER_SIZE, block_size * 256),
			    NEMO_G_FILE_MAX_BUFFER_SIZE);
		/* At least 1MB, or 256x block size, but not exceeding NEMO_G_FILE_MAX_BUFFER_SIZE */
	}

	/* Final fallback */
	return NEMO_G_FILE_MIN_BUFFER_SIZE;
}

static void
copy_file_metadata (GFile *source,
		    GFile *destination,
		    GFileCopyFlags flags,
		    GCancellable *cancellable)
{
	/* No error parameter — metadata failure is never fatal */
	char *attrs_to_read = g_file_build_attribute_list_for_copy (
		destination, flags, cancellable, NULL);
	if (!attrs_to_read)
		return;

	GFileInfo *info =
		g_file_query_info (source,
				   attrs_to_read,
				   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				   cancellable,
				   NULL); /* errors ignored */
	g_free (attrs_to_read);

	if (!info)
		return;

	/* Ignore errors — failure to copy metadata is not a hard error */
	g_file_set_attributes_from_info (destination,
					 info,
					 G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					 cancellable,
					 NULL);

	g_object_unref (info);
}

static gboolean
files_on_same_filesystem (GFile *file1, GFile *file2, GError **error)
{
	struct stat stat1, stat2;
	gchar *path1 = NULL;
	gchar *path2 = NULL;
	gboolean ret = FALSE;

	path1 = g_file_get_path (file1);
	path2 = g_file_get_path (file2);

	if (!path1) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_FILENAME,
			     "Source file is not a local file");
		g_free (path2);
		return FALSE;
	}
	if (!path2) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_FILENAME,
			     "Destination file is not a local file");
		g_free (path1);
		return FALSE;
	}

	if (stat (path1, &stat1) != 0) {
		g_set_error (error,
			     G_IO_ERROR,
			     g_io_error_from_errno (errno),
			     "Failed to stat source file: %s",
			     g_strerror (errno));
		goto cleanup;
	}

	/* Try destination directly first; if it doesn't exist yet,
     * fall back to its parent directory, which must exist and
     * will be on the same filesystem as the eventual destination. */
	if (stat (path2, &stat2) != 0) {
		if (errno != ENOENT) {
			g_set_error (error,
				     G_IO_ERROR,
				     g_io_error_from_errno (errno),
				     "Failed to stat destination file: %s",
				     g_strerror (errno));
			goto cleanup;
		}

		/* Destination doesn't exist — stat its parent instead */
		GFile *dest_parent = g_file_get_parent (file2);
		if (!dest_parent) {
			g_set_error (
				error,
				G_IO_ERROR,
				G_IO_ERROR_INVALID_FILENAME,
				"Destination file has no parent directory");
			goto cleanup;
		}

		gchar *parent_path = g_file_get_path (dest_parent);
		g_object_unref (dest_parent);

		if (!parent_path) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_FILENAME,
				     "Destination parent is not a local path");
			goto cleanup;
		}

		if (stat (parent_path, &stat2) != 0) {
			g_set_error (
				error,
				G_IO_ERROR,
				g_io_error_from_errno (errno),
				"Failed to stat destination parent directory: %s",
				g_strerror (errno));
			g_free (parent_path);
			goto cleanup;
		}
		g_free (parent_path);
	}

	ret = (stat1.st_dev == stat2.st_dev);

cleanup:
	g_free (path1);
	g_free (path2);
	return ret;
}

gboolean
nemo_g_file_copy_to_blk_sync (GFile *source,
			      GFile *destination,
			      GFileCopyFlags flags,
			      GCancellable *cancellable,
			      NemoGFileProgressCallback progress_callback,
			      gpointer progress_callback_data,
			      GError **error)
{
	gchar *src_path, *dest_path;
	int src_fd = -1, dest_fd = -1;
	struct stat src_stat;
	goffset total_size = 0;
	goffset bytes_copied = 0;
	guchar *buffer = NULL;
	ssize_t bytes_read, bytes_written;
	gboolean success = FALSE;

	const guint64 target_chunk_time_us =
		NEMO_G_FILE_TARGET_MAX_CHUNK_TIME_US;
	guint64 last_progress_time_us = 0;
	guint64 last_chunk_time_us = 0;
	guint64 chunk_time_us = 0;
	size_t buffer_size;
	int buffer_adjustment_steps = 0;
	const int max_buffer_adjustment_steps = 7;
	gboolean buffer_size_found = FALSE;
	gboolean backup_created = FALSE;

	/* Resolve paths */
	src_path = g_file_get_path (source);
	if (!src_path) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_FILENAME,
			     "Source file is not a local file");
		return FALSE;
	}

	dest_path = g_file_get_path (destination);
	if (!dest_path) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_FILENAME,
			     "Destination file is not a local file");
		g_free (src_path);
		return FALSE;
	}

	/* Get minimum buffer size from filesystem */
	buffer_size = get_min_buffer_size (dest_path);

	/* Open source file */
	int src_flags = O_RDONLY;
	if (flags & G_FILE_COPY_NOFOLLOW_SYMLINKS) {
		src_flags |= O_NOFOLLOW;
	}
	src_fd = open (src_path, src_flags);
	if (src_fd == -1) {
		if (errno == ELOOP) {
			/* Source is a symlink and we're not following it —
			* block copy of symlinks is not supported */
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_SUPPORTED,
				     "Cannot block-copy a symlink source: %s",
				     src_path);
		} else {
			g_set_error (error,
				     G_IO_ERROR,
				     g_io_error_from_errno (errno),
				     "Failed to open source file: %s",
				     g_strerror (errno));
		}
		g_free (src_path);
		g_free (dest_path);
		return FALSE;
	}

	/* Get source file size */
	if (fstat (src_fd, &src_stat) == -1) {
		g_set_error (error,
			     G_IO_ERROR,
			     g_io_error_from_errno (errno),
			     "Failed to stat source file: %s",
			     g_strerror (errno));
		goto cleanup;
	}
	total_size = src_stat.st_size;

	/* Handle backup before opening destination */
	if ((flags & G_FILE_COPY_OVERWRITE) && (flags & G_FILE_COPY_BACKUP)) {
		struct stat dest_stat;
		if (stat (dest_path, &dest_stat) == 0) {
			gchar *backup_path = g_strconcat (dest_path, "~", NULL);

			if (unlink (backup_path) != 0 && errno != ENOENT) {
				g_set_error (
					error,
					G_IO_ERROR,
					g_io_error_from_errno (errno),
					"Failed to remove existing backup file: %s",
					g_strerror (errno));
				g_free (backup_path);
				goto cleanup;
			}

			if (rename (dest_path, backup_path) != 0) {
				g_set_error (error,
					     G_IO_ERROR,
					     g_io_error_from_errno (errno),
					     "Failed to create backup file: %s",
					     g_strerror (errno));
				g_free (backup_path);
				goto cleanup;
			}

			g_free (backup_path);

			/* Destination has been moved to backup — create fresh
             * instead of truncating */
			backup_created = TRUE;
		}
		/* If destination doesn't exist, no backup needed */
	}

	/* Open destination file */
	int dest_flags = O_WRONLY | O_CREAT;
	if ((flags & G_FILE_COPY_OVERWRITE) && !backup_created) {
		dest_flags |= O_TRUNC;
	} else {
		dest_flags |= O_EXCL;
	}
	dest_flags |=
		O_SYNC; /* Synchronous writes (report only actually written bytes) */

	/* Set destination file permissions */
	mode_t dest_mode = (flags & G_FILE_COPY_TARGET_DEFAULT_PERMS) ?
				   0666 :
				   (src_stat.st_mode & 0777);
	dest_fd = open (dest_path, dest_flags, dest_mode);
	if (dest_fd == -1) {
		g_set_error (error,
			     G_IO_ERROR,
			     g_io_error_from_errno (errno),
			     "Failed to open destination file: %s",
			     g_strerror (errno));
		goto cleanup;
	}

	/* Allocate buffer */
	buffer = g_malloc (buffer_size);
	if (!buffer) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "Failed to allocate buffer");
		goto cleanup;
	}

	g_debug ("Initial buffer size for blk copy: %zu bytes (%zu MB)",
		 buffer_size,
		 buffer_size / 1024 / 1024);

	last_progress_time_us = g_get_monotonic_time ();
	last_chunk_time_us = last_progress_time_us;

	/* Copy data in chunks */
	while ((bytes_read = read (src_fd, buffer, buffer_size)) > 0) {
		/* Check for cancellation */
		if (cancellable &&
		    g_cancellable_set_error_if_cancelled (cancellable, error)) {
			goto cleanup;
		}

		/* Handle partial writes */
		size_t bytes_to_write = bytes_read;
		size_t total_written = 0;
		while (total_written < bytes_to_write) {
			bytes_written = write (dest_fd,
					       buffer + total_written,
					       bytes_to_write - total_written);
			if (bytes_written == -1) {
				if (errno == EINTR) {
					continue; /* Retry on interrupt */
				}
				g_set_error (
					error,
					G_IO_ERROR,
					g_io_error_from_errno (errno),
					"Failed to write to destination: %s",
					g_strerror (errno));
				goto cleanup;
			}
			total_written += bytes_written;
		}
		bytes_copied += total_written;

		/* Measure chunk time and adapt buffer size */
		guint64 current_time_us = g_get_monotonic_time ();
		chunk_time_us = (current_time_us - last_chunk_time_us);
		last_chunk_time_us = current_time_us;

		/* Adaptive buffer sizing */
		if (!buffer_size_found &&
			(goffset) buffer_size < total_size &&
		    chunk_time_us < target_chunk_time_us &&
		    buffer_adjustment_steps < max_buffer_adjustment_steps &&
		    buffer_size * 2 <= NEMO_G_FILE_MAX_BUFFER_SIZE &&
		    bytes_copied < total_size) {
			size_t new_buffer_size = buffer_size * 2;
			g_free (buffer);
			buffer = g_malloc (new_buffer_size);
			if (!buffer) {
				g_set_error (error,
					     G_IO_ERROR,
					     G_IO_ERROR_FAILED,
					     "Failed to reallocate buffer");
				goto cleanup;
			}
			buffer_size = new_buffer_size;
			buffer_adjustment_steps++;
		} else if (!buffer_size_found) {
			buffer_size_found = TRUE;
			g_debug ("Final buffer size used for blk copy: %zu bytes (%zu MB)",
				 buffer_size,
				 buffer_size / 1024 / 1024);
		}

		if (progress_callback && buffer_size_found) {
			guint64 elapsed_us =
				current_time_us - last_progress_time_us;
			if (elapsed_us >= target_chunk_time_us ||
			    bytes_copied == total_size) {
				progress_callback (bytes_copied,
						   total_size,
						   progress_callback_data);
				last_progress_time_us = current_time_us;
			}
		}
	}

	if (bytes_read == -1) {
		g_set_error (error,
			     G_IO_ERROR,
			     g_io_error_from_errno (errno),
			     "Failed to read from source: %s",
			     g_strerror (errno));
		goto cleanup;
	}

	/* Copy metadata if requested */
	copy_file_metadata (source, destination, flags, cancellable);

	success = TRUE;

cleanup:
	if (src_fd != -1)
		close (src_fd);
	if (dest_fd != -1) {
		if (!success &&
		    (!(flags & G_FILE_COPY_OVERWRITE) || backup_created))
			unlink (dest_path);
		close (dest_fd);
	}

	/* Restore backup if copy failed */
	if (!success && backup_created) {
		gchar *backup_path = g_strconcat (dest_path, "~", NULL);
		if (rename (backup_path, dest_path) != 0) {
			g_warning ("Failed to restore backup '%s' to '%s': %s",
				   backup_path,
				   dest_path,
				   g_strerror (errno));
		}
		g_free (backup_path);
	}

	g_free (buffer);
	g_free (src_path);
	g_free (dest_path);

	/* Ensure 100% progress is always reported on success */
	if (success && progress_callback)
		progress_callback (total_size, total_size, progress_callback_data);

	return success;
}

gboolean
nemo_g_file_move_to_blk_sync (GFile *source,
			      GFile *destination,
			      GFileCopyFlags flags,
			      GCancellable *cancellable,
			      NemoGFileProgressCallback progress_callback,
			      gpointer progress_callback_data,
			      GError **error)
{
	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	if (flags & G_FILE_COPY_NO_FALLBACK_FOR_MOVE) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_SUPPORTED,
				     "Operation not supported");
		return FALSE;
	}

	/* Attempt atomic rename if on the same filesystem */
	GError *same_fs_error = NULL;
	if (files_on_same_filesystem (source, destination, &same_fs_error)) {
		gchar *src_path = g_file_get_path (source);
		gchar *dest_path = g_file_get_path (destination);

		if (!src_path || !dest_path) {
			g_free (src_path);
			g_free (dest_path);
			g_set_error (
				error,
				G_IO_ERROR,
				G_IO_ERROR_INVALID_FILENAME,
				"Source or destination is not a local file");
			return FALSE;
		}

		gboolean renamed = FALSE;

		if (flags & G_FILE_COPY_OVERWRITE) {
			/* Plain rename — atomically replaces destination if it exists */
			if (rename (src_path, dest_path) == 0) {
				renamed = TRUE;
			} else {
				g_debug (
					"rename() failed (%s), falling back to copy+delete",
					g_strerror (errno));
			}
		} else {
			/* Use renameat2 with RENAME_NOREPLACE for atomic no-clobber */
			if (syscall (SYS_renameat2,
				     AT_FDCWD,
				     src_path,
				     AT_FDCWD,
				     dest_path,
				     RENAME_NOREPLACE) == 0) {
				renamed = TRUE;
			} else if (errno == EEXIST) {
				g_free (src_path);
				g_free (dest_path);
				g_set_error (error,
					     G_IO_ERROR,
					     G_IO_ERROR_EXISTS,
					     "Destination file already exists");
				return FALSE;
			} else if (errno == ENOSYS) {
				g_debug (
					"renameat2 not available, falling back to copy+delete");
			} else {
				g_debug (
					"renameat2() failed (%s), falling back to copy+delete",
					g_strerror (errno));
			}
		}

		g_free (src_path);
		g_free (dest_path);

		if (renamed) {
			if (progress_callback)
				progress_callback (
					1, 1, progress_callback_data);
			return TRUE;
		}

	} else if (same_fs_error) {
		g_debug (
			"files_on_same_filesystem failed: %s, falling back to copy+delete",
			same_fs_error->message);
		g_clear_error (&same_fs_error);
	}

	/* Fall back to copy+delete for cross-filesystem moves or
     * when rename is unavailable */
	flags |= G_FILE_COPY_ALL_METADATA | G_FILE_COPY_NOFOLLOW_SYMLINKS;
	if (!nemo_g_file_copy_to_blk_sync (source,
					   destination,
					   flags,
					   cancellable,
					   progress_callback,
					   progress_callback_data,
					   error))
		return FALSE;

	return g_file_delete (source, cancellable, error);
}
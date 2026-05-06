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
#include <errno.h>
#include <fcntl.h>
#include <gio/gio.h> /* For GIOError, g_io_error_from_errno, etc. */
#include <glib.h>    /* For GError, g_set_error, etc. */
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
        return MAX (NEMO_G_FILE_MIN_BUFFER_SIZE, (size_t)rec_increment);
    }

    /* Try Linux-specific minimum allocation size */
    long alloc_size_min = safe_pathconf (path, _PC_ALLOC_SIZE_MIN);
    if (alloc_size_min > 0) {
        return MAX (NEMO_G_FILE_MIN_BUFFER_SIZE, (size_t)alloc_size_min);
    }

    /* Fall back to statvfs */
    struct statvfs fs_info;
    if (statvfs (path, &fs_info) == 0) {
        size_t block_size = fs_info.f_frsize; /* Filesystem fragment size */
        return MAX (NEMO_G_FILE_MIN_BUFFER_SIZE,
                    block_size * 256); /* At least 1MB, or 256x block size */
    }

    /* Final fallback */
    return NEMO_G_FILE_MIN_BUFFER_SIZE;
}

/* Helper function to check if a file is a symlink */
static gboolean
file_is_symlink (GFile *file)
{
    return g_file_query_file_type (file,
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   NULL) == G_FILE_TYPE_SYMBOLIC_LINK;
}

/* Helper function to copy file metadata using GFileInfo */
static gboolean
copy_file_metadata (GFile *source,
                    GFile *destination,
                    GFileCopyFlags flags,
                    GError **error)
{
    if (flags & G_FILE_COPY_ALL_METADATA) {
        /* Query source file attributes */
        GFileInfo *source_info = g_file_query_info (
            source,
            G_FILE_ATTRIBUTE_TIME_MODIFIED "," G_FILE_ATTRIBUTE_TIME_ACCESS
                                           "," G_FILE_ATTRIBUTE_UNIX_MODE,
            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
            NULL,
            error);
        if (!source_info) {
            return FALSE;
        }

        GFileInfo *dest_info = g_file_info_new ();

        if (g_file_info_has_attribute (source_info,
                                       G_FILE_ATTRIBUTE_TIME_MODIFIED)) {
            guint64 mtime = g_file_info_get_attribute_uint64 (
                source_info,
                G_FILE_ATTRIBUTE_TIME_MODIFIED);
            g_file_info_set_attribute_uint64 (dest_info,
                                              G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                              mtime);
        }
        if (g_file_info_has_attribute (source_info,
                                       G_FILE_ATTRIBUTE_TIME_ACCESS)) {
            guint64 atime =
                g_file_info_get_attribute_uint64 (source_info,
                                                  G_FILE_ATTRIBUTE_TIME_ACCESS);
            g_file_info_set_attribute_uint64 (dest_info,
                                              G_FILE_ATTRIBUTE_TIME_ACCESS,
                                              atime);
        }

        /* Copy permissions if not using default */
        if (!(flags & G_FILE_COPY_TARGET_DEFAULT_PERMS) &&
            g_file_info_has_attribute (source_info,
                                       G_FILE_ATTRIBUTE_UNIX_MODE)) {
            guint32 mode =
                g_file_info_get_attribute_uint32 (source_info,
                                                  G_FILE_ATTRIBUTE_UNIX_MODE);
            g_file_info_set_attribute_uint32 (dest_info,
                                              G_FILE_ATTRIBUTE_UNIX_MODE,
                                              mode);
        }

        /* Apply the metadata to the destination file */
        if (!g_file_set_attributes_from_info (
                destination,
                dest_info,
                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                NULL,
                error)) {
            g_object_unref (source_info);
            g_object_unref (dest_info);
            return FALSE;
        }

        g_object_unref (source_info);
        g_object_unref (dest_info);
    }
    return TRUE;
}

/* Helper function to check if two files are on the same filesystem */
static gboolean
files_on_same_filesystem (GFile *file1, GFile *file2, GError **error)
{
    struct stat stat1, stat2;

    if (stat (g_file_get_path (file1), &stat1) != 0) {
        g_set_error (error,
                     G_IO_ERROR,
                     g_io_error_from_errno (errno),
                     "Failed to stat source file: %s",
                     g_strerror (errno));
        return FALSE;
    }

    if (stat (g_file_get_path (file2), &stat2) != 0) {
        g_set_error (error,
                     G_IO_ERROR,
                     g_io_error_from_errno (errno),
                     "Failed to stat destination file: %s",
                     g_strerror (errno));
        return FALSE;
    }

    return stat1.st_dev == stat2.st_dev;
}

/* Copy function with adaptive buffer sizing */
gboolean
nemo_g_file_copy_synchronous (GFile *source,
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

    /* Adaptive buffer sizing */
    const guint64 target_chunk_time_us = NEMO_G_FILE_TARGET_MAX_CHUNK_TIME_US;
    guint64 last_progress_time_us = 0;
    guint64 last_chunk_time_us = 0;
    guint64 chunk_time_us = 0;
    size_t buffer_size;
    int buffer_adjustment_steps = 0;
    const int max_buffer_adjustment_steps = 10;
    gboolean buffer_size_found = FALSE;

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

    /* Check if source is a symlink and NOFOLLOW_SYMLINKS is set */
    if ((flags & G_FILE_COPY_NOFOLLOW_SYMLINKS) && file_is_symlink (source)) {
        g_set_error (error,
                     G_IO_ERROR,
                     G_IO_ERROR_INVALID_FILENAME,
                     "Source file is a symbolic link");
        g_free (src_path);
        g_free (dest_path);
        return FALSE;
    }

    /* Check if destination is a symlink and NOFOLLOW_SYMLINKS is set */
    if ((flags & G_FILE_COPY_NOFOLLOW_SYMLINKS) &&
        g_file_query_exists (destination, NULL)) {
        if (file_is_symlink (destination)) {
            g_set_error (error,
                         G_IO_ERROR,
                         G_IO_ERROR_INVALID_FILENAME,
                         "Destination file is a symbolic link");
            g_free (src_path);
            g_free (dest_path);
            return FALSE;
        }
    }

    /* Get minimum buffer size from filesystem */
    buffer_size = get_min_buffer_size (dest_path);

    /* Open source file */
    src_fd = open (src_path, O_RDONLY);
    if (src_fd == -1) {
        g_set_error (error,
                     G_IO_ERROR,
                     g_io_error_from_errno (errno),
                     "Failed to open source file: %s",
                     g_strerror (errno));
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

    /* Open destination file */
    int dest_flags = O_WRONLY | O_CREAT;
    if (flags & G_FILE_COPY_OVERWRITE) {
        dest_flags |= O_TRUNC;
    }
    else {
        /* If not overwriting, fail if destination exists (already checked
         * above) */
        dest_flags |= O_EXCL;
    }
    if (flags & G_FILE_COPY_NOFOLLOW_SYMLINKS) {
        dest_flags |= O_NOFOLLOW;
    }
    dest_flags |= O_SYNC; /* Synchronous writes */

    /* Set destination file permissions */
    mode_t dest_mode = (flags & G_FILE_COPY_TARGET_DEFAULT_PERMS)
                           ? 0666
                           : (src_stat.st_mode & 0777);
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

    g_print ("Initial buffer size: %zu bytes (%zu MB)\n",
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
                g_set_error (error,
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

        /* Adaptive buffer sizing: Double buffer if chunk was too fast (<
           target_chunk_time_us) and we haven't exceeded max steps */
        if (!buffer_size_found && chunk_time_us < target_chunk_time_us &&
            buffer_adjustment_steps < max_buffer_adjustment_steps &&
            bytes_copied < total_size) {
            /* Increase buffer size */
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
        }
        else {
            buffer_size_found = TRUE; /* Stop further increases */
        }

        if (progress_callback && buffer_size_found) {
            guint64 elapsed_us = current_time_us - last_progress_time_us;
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
    if (!copy_file_metadata (source, destination, flags, error)) {
        goto cleanup;
    }

    success = TRUE;

    /* Print final buffer size used */
    g_print ("Final buffer size used: %zu bytes (%zu MB)\n",
             buffer_size,
             buffer_size / 1024 / 1024);

cleanup:
    if (src_fd != -1)
        close (src_fd);
    if (dest_fd != -1) {
        if (!success)
            close (dest_fd);
        else
            fsync (dest_fd), close (dest_fd);
    }
    g_free (buffer);
    g_free (src_path);
    g_free (dest_path);
    return success;
}

/* Move function with full GLib compatibility */
gboolean
nemo_g_file_move_synchronous (GFile *source,
                              GFile *destination,
                              GFileCopyFlags flags,
                              GCancellable *cancellable,
                              NemoGFileProgressCallback progress_callback,
                              gpointer progress_callback_data,
                              GError **error)
{
    gchar *src_path, *dest_path;
    gboolean same_filesystem = FALSE;
    gboolean success = FALSE;

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

    /* Check if source is a symlink and NOFOLLOW_SYMLINKS is set */
    if ((flags & G_FILE_COPY_NOFOLLOW_SYMLINKS) && file_is_symlink (source)) {
        g_set_error (error,
                     G_IO_ERROR,
                     G_IO_ERROR_INVALID_FILENAME,
                     "Source file is a symbolic link");
        g_free (src_path);
        g_free (dest_path);
        return FALSE;
    }

    /* Check if destination is a symlink and NOFOLLOW_SYMLINKS is set */
    if ((flags & G_FILE_COPY_NOFOLLOW_SYMLINKS) &&
        g_file_query_exists (destination, NULL)) {
        if (file_is_symlink (destination)) {
            g_set_error (error,
                         G_IO_ERROR,
                         G_IO_ERROR_INVALID_FILENAME,
                         "Destination file is a symbolic link");
            g_free (src_path);
            g_free (dest_path);
            return FALSE;
        }
    }

    /* Check if destination exists and OVERWRITE is not set */
    if (g_file_query_exists (destination, NULL) &&
        !(flags & G_FILE_COPY_OVERWRITE)) {
        g_set_error (error,
                     G_IO_ERROR,
                     G_IO_ERROR_EXISTS,
                     "Destination file already exists");
        g_free (src_path);
        g_free (dest_path);
        return FALSE;
    }

    /* Check if files are on the same filesystem */
    same_filesystem = files_on_same_filesystem (source, destination, error);
    if (error && *error) {
        g_free (src_path);
        g_free (dest_path);
        return FALSE;
    }

    if (!same_filesystem) {
        /* Cross-device move: fall back to copy + delete */
        success = nemo_g_file_copy_synchronous (source,
                                                destination,
                                                flags,
                                                cancellable,
                                                progress_callback,
                                                progress_callback_data,
                                                error);
        if (!success) {
            goto cleanup;
        }

        /* Delete the source file */
        if (!g_file_delete (source, cancellable, error)) {
            goto cleanup;
        }
    }
    else {
        /* Same filesystem: use rename for atomic move */
        if (rename (src_path, dest_path) == -1) {
            if (errno == EXDEV) {
                /* Cross-device link: fall back to copy + delete */
                success = nemo_g_file_copy_synchronous (source,
                                                        destination,
                                                        flags,
                                                        cancellable,
                                                        progress_callback,
                                                        progress_callback_data,
                                                        error);
                if (!success) {
                    goto cleanup;
                }
                if (!g_file_delete (source, cancellable, error)) {
                    goto cleanup;
                }
            }
            else {
                g_set_error (error,
                             G_IO_ERROR,
                             g_io_error_from_errno (errno),
                             "Failed to move file: %s",
                             g_strerror (errno));
                goto cleanup;
            }
        }

        /* Report progress if callback is provided (100% complete) */
        if (progress_callback) {
            struct stat stat_buf;
            if (stat (dest_path, &stat_buf) == 0) {
                progress_callback (stat_buf.st_size,
                                   stat_buf.st_size,
                                   progress_callback_data);
            }
        }
    }

    success = TRUE;
cleanup:
    g_free (src_path);
    g_free (dest_path);
    return success;
}
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
   nemo-writeback-pacer.c - keeps copies to slow destinations in step
   with the device

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
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <gio/gunixmounts.h>

#include "nemo-writeback-pacer.h"

/* Each full window of written data is handed to a helper thread, which syncs
 * the filesystem while the copy fills the next one. The copy only waits if
 * the previous window is still syncing, so at most two windows are unsynced
 * and progress follows the device rather than the page cache.
 *
 * A window holds about WINDOW_DURATION of data at the rate of the last sync,
 * clamped to MIN/MAX_WINDOW_SIZE and growing at most 2x per sync.
 *
 * A sync can't be interrupted, so a cancelled job stops waiting for it and
 * leaves the helper to finish on its own. */
#define WINDOW_DURATION (G_USEC_PER_SEC / 2)
#define MIN_WINDOW_SIZE (1024 * 1024)
#define MAX_WINDOW_SIZE (64 * 1024 * 1024)
#define CANCEL_CHECK_INTERVAL (G_USEC_PER_SEC / 10)

struct NemoWritebackPacer {
    gint ref_count;
    int fd;
    GThread *thread;
    GCancellable *cancellable;

    GMutex mutex;
    GCond cond;
    guint64 requested;
    guint64 completed;
    gboolean stopping;
    int sync_errno;
    gboolean error_reported;
    goffset handed_over;
    goffset synced;
    goffset last_sync_bytes;
    gint64 last_sync_usec;

    goffset window_size;
    goffset window_bytes;
    gboolean written;
};

/* The network types and the FUSE rule are KIO's, from KMountPoint. */
static gboolean
is_on_network_filesystem (const char *path)
{
    const char * const network_types[] = {
        "cifs", "smb3", "smbfs", "nfs", "nfs3", "nfs4", "afs", "ncpfs",
        "glusterfs", "ceph", "fuse.curlftpfs", "fuse.sshfs", "fuse.ceph", "9p",
        NULL
    };
    GUnixMountEntry *mount;
    const char *type, *device;
    gboolean network;

    mount = g_unix_mount_for (path, NULL);

    if (mount == NULL) {
        return FALSE;
    }

    type = g_unix_mount_get_fs_type (mount);
    device = g_unix_mount_get_device_path (mount);
    network = g_strv_contains (network_types, type) ||
              (g_str_has_prefix (type, "fuse.") && !g_str_has_prefix (device, "/dev/"));

    g_unix_mount_free (mount);

    return network;
}

static gboolean
is_on_removable_drive (const char *path)
{
    GFile *directory;
    GMount *mount;
    GDrive *drive;
    gboolean removable;

    directory = g_file_new_for_path (path);
    mount = g_file_find_enclosing_mount (directory, NULL, NULL);
    g_object_unref (directory);
    if (mount == NULL) {
        return FALSE;
    }

    drive = g_mount_get_drive (mount);
    removable = drive != NULL && g_drive_is_removable (drive);

    g_clear_object (&drive);
    g_object_unref (mount);

    return removable;
}

static void
pacer_unref (NemoWritebackPacer *pacer)
{
    if (!g_atomic_int_dec_and_test (&pacer->ref_count)) {
        return;
    }

    close (pacer->fd);
    g_object_unref (pacer->cancellable);
    g_mutex_clear (&pacer->mutex);
    g_cond_clear (&pacer->cond);
    g_free (pacer);
}

static gpointer
sync_thread_func (gpointer data)
{
    NemoWritebackPacer *pacer = data;

    g_mutex_lock (&pacer->mutex);

    while (TRUE) {
        guint64 generation;
        goffset bytes;
        gint64 start;
        int result;

        while (pacer->completed == pacer->requested && !pacer->stopping) {
            g_cond_wait (&pacer->cond, &pacer->mutex);
        }

        if (pacer->completed == pacer->requested ||
            g_cancellable_is_cancelled (pacer->cancellable)) {
            break;
        }

        generation = pacer->requested;
        bytes = pacer->handed_over - pacer->synced;
        pacer->synced = pacer->handed_over;
        g_mutex_unlock (&pacer->mutex);

        start = g_get_monotonic_time ();
        result = syncfs (pacer->fd) == 0 ? 0 : errno;

        g_mutex_lock (&pacer->mutex);
        if (result != 0 && pacer->sync_errno == 0) {
            pacer->sync_errno = result;
        }
        pacer->last_sync_bytes = bytes;
        pacer->last_sync_usec = MAX (g_get_monotonic_time () - start, 1);
        pacer->completed = generation;
        g_cond_broadcast (&pacer->cond);
    }

    g_mutex_unlock (&pacer->mutex);
    pacer_unref (pacer);

    return NULL;
}

/* Called with the mutex held. Waits until the generation is synced or the job
 * is cancelled, then returns FALSE if a failed sync hasn't been reported yet. */
static gboolean
wait_for_sync (NemoWritebackPacer  *pacer,
               guint64              generation,
               GError             **error)
{
    while (pacer->completed < generation &&
           !g_cancellable_is_cancelled (pacer->cancellable)) {
        g_cond_wait_until (&pacer->cond, &pacer->mutex,
                           g_get_monotonic_time () + CANCEL_CHECK_INTERVAL);
    }

    if (pacer->sync_errno == 0 || pacer->error_reported) {
        return TRUE;
    }

    pacer->error_reported = TRUE;
    g_set_error_literal (error,
                         G_IO_ERROR,
                         g_io_error_from_errno (pacer->sync_errno),
                         g_strerror (pacer->sync_errno));
    return FALSE;
}

NemoWritebackPacer *
nemo_writeback_pacer_new (GFile        *directory,
                          GCancellable *cancellable)
{
    NemoWritebackPacer *pacer;
    char *path, *real_path;
    int fd;

    if (!g_file_is_native (directory) ||
        g_getenv ("NEMO_DISABLE_WRITEBACK_PACING") != NULL) {
        return NULL;
    }

    path = g_file_get_path (directory);
    real_path = realpath (path, NULL);
    g_free (path);

    if (real_path == NULL) {
        return NULL;
    }

    fd = -1;
    if (g_getenv ("NEMO_FORCE_WRITEBACK_PACING") != NULL ||
        is_on_removable_drive (real_path) ||
        is_on_network_filesystem (real_path)) {
        fd = open (real_path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    }
    free (real_path);

    if (fd < 0) {
        return NULL;
    }

    pacer = g_new0 (NemoWritebackPacer, 1);
    pacer->ref_count = 2;
    pacer->fd = fd;
    pacer->cancellable = g_object_ref (cancellable);
    pacer->window_size = MIN_WINDOW_SIZE;
    g_mutex_init (&pacer->mutex);
    g_cond_init (&pacer->cond);
    pacer->thread = g_thread_new ("nemo-writeback", sync_thread_func, pacer);

    return pacer;
}

gboolean
nemo_writeback_pacer_add (NemoWritebackPacer  *pacer,
                          goffset              bytes_written,
                          GError             **error)
{
    goffset target;
    gboolean ret;

    pacer->written = TRUE;
    pacer->window_bytes += bytes_written;
    if (pacer->window_bytes < pacer->window_size) {
        return TRUE;
    }

    g_mutex_lock (&pacer->mutex);
    pacer->handed_over += pacer->window_bytes;
    pacer->requested++;
    g_cond_broadcast (&pacer->cond);
    ret = wait_for_sync (pacer, pacer->requested - 1, error);

    if (pacer->last_sync_usec > 0) {
        target = (goffset) ((gdouble) pacer->last_sync_bytes * WINDOW_DURATION / pacer->last_sync_usec);
        pacer->window_size = CLAMP (target,
                                    MIN_WINDOW_SIZE,
                                    MIN (pacer->window_size * 2, MAX_WINDOW_SIZE));
        pacer->last_sync_usec = 0;
    }
    g_mutex_unlock (&pacer->mutex);

    pacer->window_bytes = 0;

    return ret;
}

gboolean
nemo_writeback_pacer_finish (NemoWritebackPacer  *pacer,
                             GError             **error)
{
    gboolean ret;

    g_mutex_lock (&pacer->mutex);
    if (pacer->written) {
        pacer->requested++;
    }
    pacer->stopping = TRUE;
    g_cond_broadcast (&pacer->cond);
    ret = wait_for_sync (pacer, pacer->requested, error);
    g_mutex_unlock (&pacer->mutex);

    g_thread_unref (pacer->thread);
    pacer_unref (pacer);

    return ret;
}

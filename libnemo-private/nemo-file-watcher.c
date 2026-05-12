// this is not working - just for illustration

#include "nemo-file-watcher.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <unistd.h>

#define WATCHER_POLL_INTERVAL_MS 500

static gboolean
watcher_read_inotify_events (FileWatcher *watcher,
                              gboolean    *found_tempfile,
                              gboolean    *found_rename)
{
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    ssize_t  len;
    char    *ptr;

    *found_tempfile = FALSE;
    *found_rename   = FALSE;

    len = read (watcher->inotify_fd, buf, sizeof (buf));
    if (len <= 0)
        return TRUE;

    ptr = buf;
    while (ptr < buf + len) {
        struct inotify_event *event = (struct inotify_event *) ptr;

        if (event->len > 0) {
            if ((event->mask & IN_CREATE) &&
                g_str_has_prefix (event->name, ".goutputstream-")) {
                if (watcher->temp_file == NULL) {
                    watcher->temp_file = g_file_get_child (watcher->dest_parent,
                                                           event->name);
                    *found_tempfile = TRUE;
                }
            }

            if ((event->mask & IN_MOVED_TO) &&
                strcmp (event->name, watcher->dest_basename) == 0) {
                *found_rename = TRUE;
            }
        }

        ptr += sizeof (struct inotify_event) + event->len;
    }

    return TRUE;
}

static gpointer
file_watcher_thread (gpointer user_data)
{
    FileWatcher  *watcher = user_data;
    WatcherState  state   = WATCHER_STATE_WAITING_FOR_TEMPFILE;
    struct pollfd pfd;

    /* arm inotify first — must happen before copy thread calls g_file_copy */
    char *parent_path = g_file_get_path (watcher->dest_parent);
    watcher->watch_descriptor = inotify_add_watch (watcher->inotify_fd,
                                                    parent_path,
                                                    IN_CREATE | IN_MOVED_TO);
    g_free (parent_path);

    /* signal the copy thread it can proceed */
    g_mutex_lock (&watcher->ready_mutex);
    watcher->watcher_ready = TRUE;
    g_cond_signal (&watcher->ready_cond);
    g_mutex_unlock (&watcher->ready_mutex);

    nemo_progress_info_start (watcher->progress);
    
    pfd.fd     = watcher->inotify_fd;
    pfd.events = POLLIN;

    while (state != WATCHER_STATE_DONE) {

        poll (&pfd, 1, WATCHER_POLL_INTERVAL_MS);

        if (g_cancellable_is_cancelled (watcher->cancellable)) {
            state = WATCHER_STATE_DONE;
            break;
        }

        gboolean found_tempfile = FALSE;
        gboolean found_rename   = FALSE;

        if (pfd.revents & POLLIN) {
            watcher_read_inotify_events (watcher, &found_tempfile, &found_rename);
        }

        g_mutex_lock (&watcher->mutex);
        gboolean copy_done = watcher->copy_done;
        g_mutex_unlock (&watcher->mutex);

        switch (state) {

        case WATCHER_STATE_WAITING_FOR_TEMPFILE:
            if (found_tempfile) {
                state = WATCHER_STATE_MONITORING_TEMPFILE;
                /* fall through to stat immediately */
            } else if (copy_done) {
                /* very small file — copy finished before temp file was seen */
                state = WATCHER_STATE_FLUSH_WAIT;
                break;
            } else {
                break;
            }
            /* fall through */

        case WATCHER_STATE_MONITORING_TEMPFILE: {
            GError    *error = NULL;
            GFileInfo *info  = g_file_query_info (watcher->temp_file,
                                                   G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                                   G_FILE_QUERY_INFO_NONE,
                                                   watcher->cancellable,
                                                   &error);
            if (info) {
                goffset actual_size = g_file_info_get_size (info);
                g_object_unref (info);

                g_mutex_lock (&watcher->mutex);
                watcher->error = FALSE;
                g_mutex_unlock (&watcher->mutex);

                if (watcher->expected_total_bytes > 0) {
                    nemo_progress_info_set_progress (watcher->progress,
                                                     actual_size,
                                                     watcher->expected_total_bytes);
                }
            } else {
                g_mutex_lock (&watcher->mutex);
                watcher->error = TRUE;
                g_mutex_unlock (&watcher->mutex);

                if (error) g_error_free (error);
            }

            if (found_rename || copy_done) {
                g_clear_object (&watcher->temp_file);
                state = WATCHER_STATE_FLUSH_WAIT;
            }
            break;
        }

        case WATCHER_STATE_FLUSH_WAIT: {
            GError    *error = NULL;
            GFileInfo *info  = g_file_query_info (watcher->dest,
                                                   G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                                   G_FILE_QUERY_INFO_NONE,
                                                   watcher->cancellable,
                                                   &error);
            if (info) {
                goffset actual_size = g_file_info_get_size (info);
                g_object_unref (info);

                g_mutex_lock (&watcher->mutex);
                watcher->error = FALSE;
                g_mutex_unlock (&watcher->mutex);

                if (actual_size >= watcher->expected_total_bytes) {
                    nemo_progress_info_set_progress (watcher->progress, 1.0, 1.0);
                    state = WATCHER_STATE_DONE;
                } else {
                    nemo_progress_info_take_status (watcher->progress,
                                                    g_strdup (_("Flushing to device...")));
                    if (watcher->expected_total_bytes > 0) {
                        nemo_progress_info_set_progress (watcher->progress,
                                                         actual_size,
                                                         watcher->expected_total_bytes);
                    }
                }
            } else {
                g_mutex_lock (&watcher->mutex);
                watcher->error = TRUE;
                gboolean fallback_has_data = (watcher->total_num_bytes > 0);
                goffset  fallback_current  = watcher->current_num_bytes;
                goffset  fallback_total    = watcher->total_num_bytes;
                g_mutex_unlock (&watcher->mutex);

                if (error) g_error_free (error);

                if (fallback_has_data) {
                    nemo_progress_info_set_progress (watcher->progress,
                                                     fallback_current,
                                                     fallback_total);
                }

                if (copy_done) {
                    state = WATCHER_STATE_DONE;
                }
            }
            break;
        }

        case WATCHER_STATE_DONE:
            break;
        }
    }

    *watcher->progress_done = TRUE;
    nemo_progress_info_finish (watcher->progress);

    g_mutex_lock (&watcher->done_mutex);
    watcher->watcher_done = TRUE;
    g_cond_signal (&watcher->done_cond);
    g_mutex_unlock (&watcher->done_mutex);

    return NULL;
}

FileWatcher *
file_watcher_new (GFile            *dest,
                  goffset           expected_total_bytes,
                  GCancellable     *cancellable,
                  NemoProgressInfo *progress,
                  gboolean         *progress_done)
{
    FileWatcher *watcher = g_new0 (FileWatcher, 1);

    g_mutex_init (&watcher->mutex);
    g_mutex_init (&watcher->ready_mutex);
    g_mutex_init (&watcher->done_mutex);
    g_cond_init  (&watcher->ready_cond);
    g_cond_init  (&watcher->done_cond);

    watcher->dest                 = g_object_ref (dest);
    watcher->dest_parent          = g_file_get_parent (dest);
    watcher->dest_basename        = g_file_get_basename (dest);
    watcher->expected_total_bytes = expected_total_bytes;
    watcher->cancellable          = g_object_ref (cancellable);
    watcher->progress             = g_object_ref (progress);
    watcher->progress_done        = progress_done;
    watcher->temp_file            = NULL;

    watcher->inotify_fd           = inotify_init1 (IN_NONBLOCK);
    watcher->watch_descriptor     = -1;

    watcher->watcher_ready        = FALSE;
    watcher->watcher_done         = FALSE;
    watcher->copy_done            = FALSE;
    watcher->error                = FALSE;
    watcher->current_num_bytes    = 0;
    watcher->total_num_bytes      = 0;

    return watcher;
}

void
file_watcher_start (FileWatcher *watcher)
{
    g_return_if_fail (watcher != NULL);

    watcher->thread = g_thread_new ("file-watcher",
                                    file_watcher_thread,
                                    watcher);
}

void
file_watcher_wait_ready (FileWatcher *watcher)
{
    g_return_if_fail (watcher != NULL);

    g_mutex_lock (&watcher->ready_mutex);
    while (!watcher->watcher_ready) {
        g_cond_wait (&watcher->ready_cond, &watcher->ready_mutex);
    }
    g_mutex_unlock (&watcher->ready_mutex);
}

void
file_watcher_set_copy_done (FileWatcher *watcher)
{
    g_return_if_fail (watcher != NULL);

    g_mutex_lock (&watcher->mutex);
    watcher->copy_done = TRUE;
    g_mutex_unlock (&watcher->mutex);
}

void
file_watcher_wait (FileWatcher *watcher)
{
    g_return_if_fail (watcher != NULL);

    g_mutex_lock (&watcher->done_mutex);
    while (!watcher->watcher_done) {
        g_cond_wait (&watcher->done_cond, &watcher->done_mutex);
    }
    g_mutex_unlock (&watcher->done_mutex);

    g_thread_join (watcher->thread);
    watcher->thread = NULL;
}

void
file_watcher_free (FileWatcher *watcher)
{
    g_return_if_fail (watcher != NULL);

    if (watcher->watch_descriptor != -1) {
        inotify_rm_watch (watcher->inotify_fd, watcher->watch_descriptor);
    }
    close (watcher->inotify_fd);

    g_clear_object (&watcher->temp_file);
    g_clear_object (&watcher->dest_parent);
    g_free (watcher->dest_basename);

    g_mutex_clear (&watcher->mutex);
    g_mutex_clear (&watcher->ready_mutex);
    g_mutex_clear (&watcher->done_mutex);
    g_cond_clear  (&watcher->ready_cond);
    g_cond_clear  (&watcher->done_cond);

    g_object_unref (watcher->dest);
    g_object_unref (watcher->cancellable);
    g_object_unref (watcher->progress);

    g_free (watcher);
}
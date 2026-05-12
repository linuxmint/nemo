// this is not working - just for illustration

/*The `FileWatcher` approach fails because the problem it tries to solve — tracking how many bytes have actually been physically written to the device — has no reliable per-file kernel interface on Linux.

The specific failure chain:

1. **inotify** correctly detects the `.goutputstream-*` temp file and `WATCHER_STATE_MONITORING_TEMPFILE` correctly stats its size during the copy phase

2. **`WATCHER_STATE_FLUSH_WAIT` exits immediately** because `g_file_query_info` returns the full file size as soon as GIO renames the temp file to the final destination — the inode size is updated at rename time, not when the data is physically flushed. So `actual_size >= expected_total_bytes` is true instantly and the watcher finishes without waiting for the real flush

3. **No per-file flush progress exists** in the kernel. The alternatives all have disqualifying problems:
   - `/proc/self/io` `write_bytes` is process-wide, polluted by other threads
   - `/sys/block/<dev>/stat` is device-wide, polluted by other processes
   - `mincore()` reports pages in cache, not pages flushed to device
   - `sync_file_range()` works on ext4/XFS but is unreliable or a no-op on FAT32/exFAT/NTFS — exactly the filesystems found on consumer USB devices

4. **The only reliable completion signal is `fsync()`** but it blocks entirely with no intermediate progress

The root cause is that the kernel page cache is intentionally opaque at the per-file level — it was never designed to expose per-file writeback progress to userspace. The synchronous copy approach sidesteps this entirely by never letting data enter the page cache unbounded — `fdatasync` per chunk means the byte counter is always ground truth.*/


#ifndef NEMO_FILE_WATCHER_H
#define NEMO_FILE_WATCHER_H

#include <glib.h>
#include <gio/gio.h>
#include "nemo-progress-info.h"

G_BEGIN_DECLS

typedef enum {
    WATCHER_STATE_WAITING_FOR_TEMPFILE,
    WATCHER_STATE_MONITORING_TEMPFILE,
    WATCHER_STATE_FLUSH_WAIT,
    WATCHER_STATE_DONE
} WatcherState;

typedef struct {
    GMutex            mutex;

    /* written by progress callback (copy job thread), read by watcher thread */
    goffset           current_num_bytes;
    goffset           total_num_bytes;

    /* written by watcher thread on query failure, read by progress callback */
    gboolean          error;

    /* set by file_watcher_set_copy_done after g_file_copy/move returns */
    gboolean          copy_done;

    /* watcher thread */
    GThread          *thread;
    GFile            *dest;
    GFile            *dest_parent;
    char             *dest_basename;
    GFile            *temp_file;
    goffset           expected_total_bytes;
    GCancellable     *cancellable;
    NemoProgressInfo *progress;
    gboolean         *progress_done;

    /* inotify */
    int               inotify_fd;
    int               watch_descriptor;

    /* copy thread blocks on this until inotify is armed */
    GMutex            ready_mutex;
    GCond             ready_cond;
    gboolean          watcher_ready;

    /* copy job thread blocks on this after g_file_copy/move returns */
    GMutex            done_mutex;
    GCond             done_cond;
    gboolean          watcher_done;
} FileWatcher;

FileWatcher *file_watcher_new           (GFile            *dest,
                                         goffset           expected_total_bytes,
                                         GCancellable     *cancellable,
                                         NemoProgressInfo *progress,
                                         gboolean         *progress_done);

void         file_watcher_start         (FileWatcher      *watcher);
void         file_watcher_wait_ready    (FileWatcher      *watcher);
void         file_watcher_set_copy_done (FileWatcher      *watcher);
void         file_watcher_wait          (FileWatcher      *watcher);
void         file_watcher_free          (FileWatcher      *watcher);

G_END_DECLS

#endif /* NEMO_FILE_WATCHER_H */
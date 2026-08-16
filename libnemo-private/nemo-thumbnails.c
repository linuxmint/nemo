/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-thumbnail-cache.h: Thumbnail code for icon factory.
 
   Copyright (C) 2000, 2001 Eazel, Inc.
   Copyright (C) 2002, 2003 Red Hat, Inc.
  
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
  
   Author: Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include "nemo-thumbnails.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include "nemo-directory-notify.h"
#include "nemo-global-preferences.h"
#include "nemo-file-utilities.h"
#include <math.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-string.h>
#include <eel/eel-debug.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtk.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <libcinnamon-desktop/gnome-desktop-thumbnail.h>

#include <gio/gfiledescriptorbased.h>
#include <fcntl.h>

#ifdef HAVE_EXIF
  #include <libexif/exif-data.h>
#endif

#define DEBUG_FLAG NEMO_DEBUG_THUMBNAILS
#include <libnemo-private/nemo-debug.h>

#include "nemo-file-private.h"

#define DEBUG_THREADS 0

/* Pixel size the factory below is created with (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE). */
#define THUMBNAIL_PIXEL_SIZE 256

/* An EXIF APP1 segment is at most 64K, and starts within the first few bytes of
 * the file, so this always covers it. */
#define EMBEDDED_THUMBNAIL_PROBE_BYTES (80 * 1024)

/* How far the embedded thumbnail's shape may differ from the dimensions EXIF
 * records for the main image before we stop trusting it. Catches files that
 * were cropped by something that did not refresh the embedded thumbnail. */
#define EMBEDDED_THUMBNAIL_ASPECT_TOLERANCE 0.05

/* Should never be a reasonable actual mtime */
#define INVALID_MTIME 0

/* Cool-off period between last file modification time and thumbnail creation */
#define RECENT_MTIME_COOLDOWN 2

#define NEMO_THUMBNAIL_FRAME_LEFT 3
#define NEMO_THUMBNAIL_FRAME_TOP 3
#define NEMO_THUMBNAIL_FRAME_RIGHT 3
#define NEMO_THUMBNAIL_FRAME_BOTTOM 3


typedef enum {
    THUMBNAIL_ADD,
    THUMBNAIL_REMOVE,
    THUMBNAIL_BUMP,
    THUMBNAIL_THREAD_EXIT
} ThumbnailCommandType;

/* multipurpose structure used for making thumbnails, associating a uri with where the thumbnail is to be stored */
typedef struct {
    char *image_uri;
    char *mime_type;
    time_t original_file_mtime;
    gint64 add_time;
    ThumbnailCommandType cmd_type;
    guint cancelled : 1;
} NemoThumbnailInfo;

/* How it works:
 * 
 * When nemo_create_thumbnail(), nemo_thumbnail_remove_from_queue or nemo_thumbnail_prioritize are called,
 * a new NemoThumbnailInfo is made, with info->cmd_type set based on the method called.
 *
 * These are added to the feeder queue, which feeds the feeder_task thread. When a new info arrives, it gets
 * processed based on info->cmd_type.

 * - nemo_create_thumbnail (THUMBNAIL_ADD): The info is looked up by uri in thumbnails_to_make_hash. If
 *   the info is already there, the existing info's mtime is updated, and the info gets pushed to the front
 *   of the threadpool queue. Otherwise, the incoming info is added to thumbnails_to_make_hash, and pushed
 *   to the threadpool queue.
 *
 * - nemo_thumbnail_remove_from_queue (THUMBNAIL_REMOVE): The info is looked up by uri in thumbnails_to_make_hash.
 *   If the info is found, it gets removed from thumbnails_to_make_hash, and info->cancelled is set to TRUE, so when
 *   it comes up in the threadpool queue, it is ignored and freed.
 *
 * - nemo_thumbnail_prioritize (THUMBNAIL_BUMP): The info is looked up by uri in thumbnails_to_make_hash. If found,
 *   it gets moved to the front of the threadpool queue.
 *
 *
 * - No mutex locking occurs in the public methods, only in the feeder and threadpool threads.
 * - NemoThumbnailInfos are garbage-collected in the threadpool worker only.
 */

/* Workers that actually make the thumbnail. */
static volatile GThreadPool *tpool = NULL;

/* Table of uris queued to the thread pool (tpool) */
static GMutex thumbnails_mutex;
static GHashTable *thumbnails_to_make_hash = NULL;

/* Every action goes thru the feeder queue. It gets processed in the feeder_task's thread. */
static GAsyncQueue *feeder_queue = NULL;
static GTask *feeder_task = NULL;

/* Causes the feeder_task to end. Only called when Nemo is shutting down. */
GCancellable *cancellable = NULL;

static GnomeDesktopThumbnailFactory *thumbnail_factory = NULL;

static gint
get_max_threads (void) {
    gint max_threads = 1;
    gint num_processors = g_get_num_processors ();

    gint pref = g_settings_get_int (nemo_preferences, NEMO_PREFERENCES_MAX_THUMBNAIL_THREADS);

    if (pref == -1) {
        /* Thumbnailing is CPU-bound (decode, scale, re-encode), so the pool
         * should scale with the machine. Leave a couple of cores for the UI
         * and the rest of the session, and cap it so that very large machines
         * don't just thrash memory bandwidth and the thumbnail cache dir.
         */
        max_threads = CLAMP (num_processors - 2, 1, 8);
    } else {
        max_threads = pref;
    }

    max_threads = MAX (1, max_threads);

#if DEBUG_THREADS
    g_message ("Thumbnailer threads: %d (setting: %d, system count: %d)", max_threads, pref, num_processors);
#else
    DEBUG ("Thumbnailer threads: %d (setting: %d, system count: %d)", max_threads, pref, num_processors);
#endif

    return max_threads;
}

static gint
lifo_sorter (gconstpointer a,
             gconstpointer b,
             gpointer      data)
{
    gint64 ta = ((const NemoThumbnailInfo *) a)->add_time;
    gint64 tb = ((const NemoThumbnailInfo *) b)->add_time;

    return tb > ta ? +1 : ta == tb ? 0 : -1;
}

static gboolean
get_file_mtime (const char *file_uri, time_t* mtime)
{
    GFile *file;
    GFileInfo *info;
    gboolean ret;

    ret = FALSE;
    *mtime = INVALID_MTIME;

    file = g_file_new_for_uri (file_uri);
    info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED, 0, NULL, NULL);
    if (info) {
        if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED)) {
            *mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
            ret = TRUE;
        }

        g_object_unref (info);
    }
    g_object_unref (file);

    return ret;
}

static void
free_thumbnail_info (NemoThumbnailInfo *info)
{
    g_free (info->image_uri);
    g_free (info->mime_type);
    g_free (info);
}

static GnomeDesktopThumbnailFactory *
get_thumbnail_factory (void)
{
    static gsize once_init = 0;

    if (g_once_init_enter (&once_init)) {
        thumbnail_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);

        g_once_init_leave (&once_init, 1);
    }

    return thumbnail_factory;
}

static GdkPixbuf *
nemo_get_thumbnail_frame (void)
{
    static GdkPixbuf *thumbnail_frame = NULL;
    static gsize once_init = 0;

    if (g_once_init_enter (&once_init)) {
        GInputStream *stream = g_resources_open_stream ("/org/nemo/icons/thumbnail_frame.png", 0, NULL);
        if (stream != NULL) {
            thumbnail_frame = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
            g_object_unref (stream);
        }

        g_once_init_leave (&once_init, 1);
    }

    return thumbnail_frame;
}

static GHashTable *
get_types_table (void)
{
    static GHashTable *image_mime_types = NULL;
    GSList *format_list, *l;
    char **types;
    int i;

    static gsize once_init = 0;

    if (g_once_init_enter (&once_init)) {
        image_mime_types = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free, NULL);
        format_list = gdk_pixbuf_get_formats ();
        for (l = format_list; l; l = l->next) {
            types = gdk_pixbuf_format_get_mime_types (l->data);

            for (i = 0; types[i] != NULL; i++) {
                g_hash_table_add (image_mime_types, types[i]);
            }

            g_free (types);
        }

        g_slist_free (format_list);

        g_once_init_leave (&once_init, 1);
    }

    return image_mime_types;
}

static gboolean
pixbuf_can_load_type (const char *mime_type)
{
    GHashTable *image_mime_types;

    image_mime_types = get_types_table ();

    return g_hash_table_contains (image_mime_types, mime_type);
}

/* This is a one-shot idle callback called from the main loop to call
   notify_file_changed() for a thumbnail. It frees the uri afterwards.
   We do this in an idle callback as I don't think nemo_file_changed() is
   thread-safe. */
static gboolean
thumbnail_thread_notify_file_changed (gpointer image_uri)
{
    NemoFile *file;

    file = nemo_file_get_by_uri ((char *) image_uri);

    DEBUG ("(Thumbnail Thread) Notifying file changed file: %p uri: %s", file, (char*) image_uri);

    if (file != NULL) {
        char *path;

        /* Ask the factory where it just wrote the thumbnail rather than
         * re-reading the file's info purely to learn thumbnail::path back. That
         * query is a network round trip per thumbnail on a remote share, it is
         * serialised one-at-a-time per directory, and for types whose content
         * type is ambiguous from the name (.png on many systems) it makes GLib
         * re-read the file to sniff it. The lookup here is local: a hash of the
         * uri and a stat in the thumbnail cache. Without this, thumbnails for
         * the files on screen can be generated but not appear for many seconds.
         */
        path = gnome_desktop_thumbnail_factory_lookup (get_thumbnail_factory (),
                                                       (char *) image_uri,
                                                       nemo_file_get_mtime (file));

        if (path != NULL) {
            g_free (file->details->thumbnail_path);
            file->details->thumbnail_path = path;

            /* Leave is_thumbnailing set: the job is not finished until the
             * result has been read back in, and clearing it here would drop the
             * indicator to the generic icon for that moment. thumbnail_done()
             * clears it once the pixbuf is in hand, or the attempt fails. */
            nemo_file_invalidate_attributes (file, NEMO_FILE_ATTRIBUTE_THUMBNAIL);
        } else {
            /* Nothing was produced, so there is no read to wait for. */
            nemo_file_set_is_thumbnailing (file, FALSE);

            nemo_file_invalidate_attributes (file,
                                             NEMO_FILE_ATTRIBUTE_THUMBNAIL |
                                             NEMO_FILE_ATTRIBUTE_INFO);
        }

        nemo_file_unref (file);
    }

    g_free (image_uri);

    return G_SOURCE_REMOVE;
}

/* Always on thumbnail thread */
static void
remove_from_hash_table (NemoThumbnailInfo *info)
{
    g_mutex_lock (&thumbnails_mutex);
    g_hash_table_remove (thumbnails_to_make_hash, info->image_uri);
    g_mutex_unlock (&thumbnails_mutex);

    free_thumbnail_info (info);
}

#ifdef HAVE_EXIF
static gboolean
exif_get_long_tag (ExifData *ed,
                   ExifTag   tag,
                   glong    *out)
{
    ExifEntry *entry;
    ExifByteOrder order;
    int i;

    order = exif_data_get_byte_order (ed);

    for (i = 0; i < EXIF_IFD_COUNT; i++) {
        entry = exif_content_get_entry (ed->ifd[i], tag);

        if (entry == NULL) {
            continue;
        }

        if (entry->format == EXIF_FORMAT_SHORT) {
            *out = exif_get_short (entry->data, order);
            return TRUE;
        }

        if (entry->format == EXIF_FORMAT_LONG) {
            *out = exif_get_long (entry->data, order);
            return TRUE;
        }
    }

    return FALSE;
}
#endif /* HAVE_EXIF */

/* A JPEG written by a camera or phone almost always carries a postcard-sized
 * copy of itself in its EXIF header. Decoding that costs a ~20KB read instead of
 * pulling the whole multi-megabyte original, which is the difference between a
 * folder of photos thumbnailing in seconds and in minutes when it lives on a
 * network share.
 *
 * Returns NULL whenever the embedded copy cannot be trusted to be as good as
 * decoding the original -- wrong format, absent, too small for the thumbnail we
 * are about to store, or a different shape from the image it claims to preview.
 * The caller then falls back to the normal full-file thumbnailer.
 */
static GdkPixbuf *
try_embedded_thumbnail (const char *uri,
                        const char *mime_type,
                        int         target_size)
{
#ifdef HAVE_EXIF
    GFile *file;
    GFileInputStream *stream;
    GdkPixbufLoader *loader;
    GdkPixbuf *pixbuf = NULL;
    GdkPixbuf *rotated;
    ExifData *ed;
    guchar *buf;
    gssize len;
    glong orientation, main_w, main_h;
    int w, h;

    if (g_strcmp0 (mime_type, "image/jpeg") != 0) {
        return NULL;
    }

    file = g_file_new_for_uri (uri);
    stream = g_file_read (file, NULL, NULL);
    g_object_unref (file);

    if (stream == NULL) {
        return NULL;
    }

    /* Only the header is wanted. Without this the kernel reads ahead to satisfy
     * a sequential-access guess, on shares with larger rsize configuration, an 80KB read
     * would could drag megabytes per file over the wire and undo the entire saving. */
    if (G_IS_FILE_DESCRIPTOR_BASED (stream)) {
        int fd = g_file_descriptor_based_get_fd (G_FILE_DESCRIPTOR_BASED (stream));

        if (fd >= 0) {
            posix_fadvise (fd, 0, 0, POSIX_FADV_RANDOM);
        }
    }

    buf = g_malloc (EMBEDDED_THUMBNAIL_PROBE_BYTES);
    len = g_input_stream_read (G_INPUT_STREAM (stream),
                               buf, EMBEDDED_THUMBNAIL_PROBE_BYTES, NULL, NULL);
    g_input_stream_close (G_INPUT_STREAM (stream), NULL, NULL);
    g_object_unref (stream);

    if (len <= 0) {
        g_free (buf);
        return NULL;
    }

    ed = exif_data_new_from_data (buf, (unsigned int) len);
    g_free (buf);

    if (ed == NULL) {
        return NULL;
    }

    if (ed->data == NULL || ed->size == 0) {
        exif_data_unref (ed);
        return NULL;
    }

    loader = gdk_pixbuf_loader_new_with_mime_type ("image/jpeg", NULL);

    if (loader != NULL) {
        if (gdk_pixbuf_loader_write (loader, ed->data, ed->size, NULL)) {
            if (gdk_pixbuf_loader_close (loader, NULL)) {
                pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

                if (pixbuf != NULL) {
                    g_object_ref (pixbuf);
                }
            }
        } else {
            gdk_pixbuf_loader_close (loader, NULL);
        }

        g_object_unref (loader);
    }

    if (pixbuf == NULL) {
        exif_data_unref (ed);
        return NULL;
    }

    w = gdk_pixbuf_get_width (pixbuf);
    h = gdk_pixbuf_get_height (pixbuf);

    /* Never trade quality for speed: if the embedded copy is smaller than the
     * thumbnail we would store, decode the original instead. */
    if (MAX (w, h) < target_size) {
        DEBUG ("(Thumbnail Thread) Embedded thumbnail too small (%dx%d < %d): %s",
               w, h, target_size, uri);
        g_object_unref (pixbuf);
        exif_data_unref (ed);
        return NULL;
    }

    if (exif_get_long_tag (ed, EXIF_TAG_PIXEL_X_DIMENSION, &main_w) &&
        exif_get_long_tag (ed, EXIF_TAG_PIXEL_Y_DIMENSION, &main_h) &&
        main_w > 0 && main_h > 0) {
        double embedded_aspect = (double) w / (double) h;
        double main_aspect = (double) main_w / (double) main_h;

        /* Orientation may have the two rotated relative to each other. */
        if (fabs (embedded_aspect - main_aspect) > EMBEDDED_THUMBNAIL_ASPECT_TOLERANCE * main_aspect &&
            fabs (embedded_aspect - 1.0 / main_aspect) > EMBEDDED_THUMBNAIL_ASPECT_TOLERANCE / main_aspect) {
            DEBUG ("(Thumbnail Thread) Embedded thumbnail shape %dx%d does not match image %ldx%ld: %s",
                   w, h, main_w, main_h, uri);
            g_object_unref (pixbuf);
            exif_data_unref (ed);
            return NULL;
        }
    }

    /* The orientation tag describes the main image; the embedded copy is stored
     * the same way round, so the same rotation applies. */
    if (exif_get_long_tag (ed, EXIF_TAG_ORIENTATION, &orientation) &&
        orientation >= 1 && orientation <= 8) {
        char value[2] = { '0' + (char) orientation, '\0' };

        gdk_pixbuf_set_option (pixbuf, "orientation", value);
    }

    exif_data_unref (ed);

    rotated = gdk_pixbuf_apply_embedded_orientation (pixbuf);
    g_object_unref (pixbuf);
    pixbuf = rotated;

    w = gdk_pixbuf_get_width (pixbuf);
    h = gdk_pixbuf_get_height (pixbuf);

    /* Store it at the same size the factory would have produced. */
    if (MAX (w, h) > target_size) {
        double scale = (double) target_size / (double) MAX (w, h);

        rotated = gdk_pixbuf_scale_simple (pixbuf,
                                           MAX (w * scale, 1),
                                           MAX (h * scale, 1),
                                           GDK_INTERP_BILINEAR);
        g_object_unref (pixbuf);
        pixbuf = rotated;
    }

    DEBUG ("(Thumbnail Thread) Used embedded thumbnail: %s", uri);

    return pixbuf;
#else
    return NULL;
#endif /* HAVE_EXIF */
}

/* Thumbnail thread */
static void
thumbnail_thread (gpointer data,
                  gpointer user_data)
{
    NemoThumbnailInfo *info = (NemoThumbnailInfo *) data;
    GdkPixbuf *pixbuf;
    time_t current_time;
    gchar *image_uri = info->image_uri;
    gboolean free_uri = FALSE;

    if (g_cancellable_is_cancelled (cancellable) || info->cancelled) {
        DEBUG ("Skipping cancelled file: %s", info->image_uri);
        remove_from_hash_table (info);
        return;
    }

    time (&current_time);

    /* Don't try to create a thumbnail if the file was modified recently.
       This prevents constant re-thumbnailing of changing files. */ 
    if (current_time < info->original_file_mtime + RECENT_MTIME_COOLDOWN) {
        DEBUG ("(Thumbnail Thread) Skipping for %d seconds: %s",
               RECENT_MTIME_COOLDOWN, info->image_uri);

        /* Reschedule thumbnailing via a change notification */
        g_timeout_add_seconds (RECENT_MTIME_COOLDOWN, thumbnail_thread_notify_file_changed,
                               g_strdup (info->image_uri));
        remove_from_hash_table (info);
        return;
    }

    /* Create the thumbnail. */
    DEBUG ("(Thumbnail Thread) Creating thumbnail: %s", info->image_uri);

    if (eel_uri_is_network (info->image_uri)) {
        GFile *file = g_file_new_for_uri (info->image_uri);
        GError *err = NULL;
        free_uri = TRUE;

        image_uri = g_filename_to_uri (g_file_peek_path (file), NULL, &err);

        if (err) {
            DEBUG ("(Thumbnail Thread) Failed to convert local_filepath to uri: %s", err->message);
            g_error_free (err);

            image_uri = g_strdup (info->image_uri); // revert back to our original image_uri
        }

        g_object_unref (file);
    }

    /**
     * the following function internally uses g_filename_to_uri whenever it finds a %i in the thumbnailer config file
     * because of that we have to convert our path from the network URI to a local file:// URI or else any
     * thumbnailers that use %i wont generate thumbnails correctly
     */
    pixbuf = try_embedded_thumbnail (image_uri, info->mime_type, THUMBNAIL_PIXEL_SIZE);

    if (pixbuf == NULL) {
        pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (thumbnail_factory,
                                                                     image_uri,
                                                                     info->mime_type);
    }
    if (free_uri) {
        g_free (image_uri);
    }

    if (pixbuf) {
        gnome_desktop_thumbnail_factory_save_thumbnail (thumbnail_factory,
                                                        pixbuf,
                                                        info->image_uri,
                                                        info->original_file_mtime);
        g_object_unref (pixbuf);
    } else {
        gnome_desktop_thumbnail_factory_create_failed_thumbnail (thumbnail_factory, 
                                                                 info->image_uri,
                                                                 info->original_file_mtime);
    }

    /* We need to call nemo_file_changed(), but I don't think that is
       thread safe. So add an idle handler and do it from the main loop. */
    g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                     thumbnail_thread_notify_file_changed,
                     g_strdup (info->image_uri), NULL);

#if DEBUG_THREADS
    g_message ("%u unprocessed (Done) (%u threads free)",
               g_thread_pool_unprocessed ((GThreadPool *) tpool),
               g_thread_pool_get_num_unused_threads ());
#endif
    remove_from_hash_table (info);
}

/* Mainloop */
static  void
feeder_task_complete (GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
    g_task_propagate_boolean (G_TASK (res), NULL);
    DEBUG ("(Finalize) Feeder task done");
}

/* Feeder thread */
static void
feeder_thread (GTask        *task,
                gpointer      source,
                gpointer      task_data,
                GCancellable *cancellable)
{
    gpointer data;

    while (!g_cancellable_is_cancelled (cancellable) && (data = g_async_queue_pop (feeder_queue))) {

#if DEBUG_THREADS
    g_message ("Pop from feeder (Add) %i items in feeder", g_async_queue_length (feeder_queue));
#endif
        NemoThumbnailInfo *feeder_info = (NemoThumbnailInfo *) data;
        NemoThumbnailInfo *existing_info = NULL;

        switch (feeder_info->cmd_type) {
            case THUMBNAIL_ADD:
                DEBUG ("(Add thumbnail) Locking mutex");
                g_mutex_lock (&thumbnails_mutex);
                existing_info = g_hash_table_lookup (thumbnails_to_make_hash, feeder_info->image_uri);

                if (existing_info == NULL) {
                    DEBUG ("(Main Thread) Adding new file to thumbnail: %s", feeder_info->image_uri);
#if DEBUG_THREADS
                    g_message ("%u unprocessed (Add)", g_thread_pool_unprocessed ((GThreadPool *) tpool));
#endif
                    g_hash_table_insert (thumbnails_to_make_hash, feeder_info->image_uri, feeder_info);
                    g_thread_pool_push ((GThreadPool *) tpool, feeder_info, NULL);

                    // Don't free this later.
                    feeder_info = NULL;
                } else {
                    DEBUG ("(Main Thread) Updating existing file mtime and prioritizing: %s", feeder_info->image_uri);

                    /* The file in the queue might need a new original mtime */
                    existing_info->original_file_mtime = feeder_info->original_file_mtime;
                    existing_info->add_time = g_get_monotonic_time ();
                    g_thread_pool_move_to_front ((GThreadPool *) tpool, existing_info);
                }
                DEBUG ("(Add thumbnail) Unlocking mutex");
                g_mutex_unlock (&thumbnails_mutex);
                break;
            case THUMBNAIL_REMOVE:
                if (!thumbnails_to_make_hash)
                    break;

                DEBUG ("(Remove from queue) Locking mutex");
                g_mutex_lock (&thumbnails_mutex);
                existing_info = g_hash_table_lookup (thumbnails_to_make_hash, feeder_info->image_uri);

                if (existing_info) {
                    DEBUG ("(Remove from queue) Removing %s", feeder_info->image_uri);
                    g_hash_table_remove (thumbnails_to_make_hash, feeder_info->image_uri);
                    existing_info->cancelled = TRUE;
                }
                DEBUG ("(Remove from queue) Unlocking mutex");
                g_mutex_unlock (&thumbnails_mutex);
                break;
            case THUMBNAIL_BUMP:
                if (!thumbnails_to_make_hash)
                    break;

                DEBUG ("(Prioritize) Locking mutex");
                g_mutex_lock (&thumbnails_mutex);
                existing_info = g_hash_table_lookup (thumbnails_to_make_hash, feeder_info->image_uri);

                if (existing_info) {
                    DEBUG ("(Prioritize) Moving to front: %s", feeder_info->image_uri);
                    existing_info->add_time = g_get_monotonic_time ();
                    g_thread_pool_move_to_front ((GThreadPool *) tpool, existing_info);
                }
                DEBUG ("(Prioritize) Unlocking mutex");
                g_mutex_unlock (&thumbnails_mutex);
                break;
            case THUMBNAIL_THREAD_EXIT:
                DEBUG ("(Finalize) Received THUMBNAIL_THREAD_EXIT, cancelling");
                g_cancellable_cancel (cancellable);
                break;
        }

        g_clear_pointer (&feeder_info, free_thumbnail_info);
    }

    g_task_return_boolean (task, TRUE);
}

static void
finalize_thumbnailer (void)
{
    NemoThumbnailInfo *info;
    gpointer data;

    if (feeder_queue == NULL)
        return;

    DEBUG ("(Finalize) Shutdown thumbnailer.");

    info = g_new0 (NemoThumbnailInfo, 1);
    info->cmd_type = THUMBNAIL_THREAD_EXIT;

    g_async_queue_push (feeder_queue, info);

    while (!g_task_get_completed (feeder_task)) {
        gtk_main_iteration ();
    }

    while ((data = g_async_queue_try_pop (feeder_queue))) {
        if (!data) {
            break;
        }
        NemoThumbnailInfo *existing_info = (NemoThumbnailInfo *) data;
        free_thumbnail_info (existing_info);
    }

    g_object_unref (feeder_task);
    g_async_queue_unref (feeder_queue);

    // This will drain and free any remaining infos.
    g_thread_pool_free ((GThreadPool *) tpool, FALSE, TRUE);

    g_hash_table_destroy (thumbnails_to_make_hash);
}

/* Mainloop */
void
nemo_create_thumbnail (NemoFile *file)
{
    time_t file_mtime = 0;
    static gsize once_init = 0;
    if (g_once_init_enter (&once_init)) {
        thumbnails_to_make_hash = g_hash_table_new (g_str_hash, g_str_equal);
        DEBUG ("Initialize thread pool");

        tpool = g_thread_pool_new ((GFunc) thumbnail_thread, NULL,
                                   get_max_threads (),
                                   FALSE, NULL);
        g_thread_pool_set_sort_function ((GThreadPool *) tpool, (GCompareDataFunc) lifo_sorter, NULL);

        feeder_queue = g_async_queue_new ();
        cancellable = g_cancellable_new ();
        feeder_task = g_task_new (NULL, cancellable, feeder_task_complete, NULL);
        g_task_run_in_thread (feeder_task, feeder_thread);

        eel_debug_call_at_shutdown ((EelFunction) finalize_thumbnailer);

        g_once_init_leave (&once_init, 1);
    }

    /* The gdk-pixbuf-thumbnailer tool has special hardcoded handling for recent: and trash: uris.
     * we need to find the activation uri here instead */
    if (nemo_file_is_in_favorites (file)) {
        NemoFile *real_file;
        gchar *uri;

        uri = nemo_file_get_symbolic_link_target_uri (file);

        real_file = nemo_file_get_by_uri (uri);
        if (real_file != NULL) {
            nemo_create_thumbnail (real_file);
            nemo_file_unref (real_file);
        }

        g_free (uri);
        return;
    }

    gchar *file_uri = nemo_file_get_uri (file);

    /* Hopefully the NemoFile will already have the image file mtime,
       so we can just use that. Otherwise we have to get it ourselves. */
    if (file->details->got_file_info &&
        file->details->file_info_is_up_to_date &&
        file->details->mtime != 0) {
        file_mtime = file->details->mtime;
    } else {
        get_file_mtime (file_uri, &file_mtime);
    }

    NemoThumbnailInfo *info;
    info = g_new0 (NemoThumbnailInfo, 1);
    info->image_uri = file_uri;
    info->mime_type = nemo_file_get_mime_type (file);
    info->original_file_mtime = file_mtime;
    info->add_time = g_get_monotonic_time ();
    info->cmd_type = THUMBNAIL_ADD;

    nemo_file_set_is_thumbnailing (file, TRUE);

#if DEBUG_THREADS
    g_message ("Push to feeder (Add) %i items in feeder", g_async_queue_length (feeder_queue));
#endif

    g_async_queue_push (feeder_queue, info);
}

/* Mainloop */
void
nemo_thumbnail_remove_from_queue (const char *file_uri)
{
    if (feeder_queue == NULL)
        return;

    NemoThumbnailInfo *info;

    info = g_new0 (NemoThumbnailInfo, 1);
    info->image_uri = g_strdup (file_uri);
    info->cmd_type = THUMBNAIL_REMOVE;

#if DEBUG_THREADS
    g_message ("Push to feeder (Remove) %i items in feeder", g_async_queue_length (feeder_queue));
#endif

    g_async_queue_push (feeder_queue, info);
}

/* Mainloop */
void
nemo_thumbnail_prioritize (const char *file_uri)
{
    if (feeder_queue == NULL)
        return;

    NemoThumbnailInfo *info;

    info = g_new0 (NemoThumbnailInfo, 1);
    info->image_uri = g_strdup (file_uri);
    info->cmd_type = THUMBNAIL_BUMP;

#if DEBUG_THREADS
    g_message ("Push to feeder (Bump) %i items in feeder", g_async_queue_length (feeder_queue));
#endif

    g_async_queue_push (feeder_queue, info);
}

gboolean
nemo_can_thumbnail_internally (NemoFile *file)
{
    g_autofree gchar *mime_type = NULL;

    mime_type = nemo_file_get_mime_type (file);
    return pixbuf_can_load_type (mime_type);
}

gboolean
nemo_can_thumbnail (NemoFile *file)
{
    GnomeDesktopThumbnailFactory *factory;
    g_autofree gchar *mime_type = NULL;
    g_autofree gchar *uri = NULL;
    time_t mtime;
    gboolean res;

    uri = nemo_file_get_uri (file);
    mime_type = nemo_file_get_mime_type (file);
    mtime = nemo_file_get_mtime (file);
    
    factory = get_thumbnail_factory ();
    res = gnome_desktop_thumbnail_factory_can_thumbnail (factory,
                                                         uri,
                                                         mime_type,
                                                         mtime);
    return res;
}


void
nemo_thumbnail_frame_image (GdkPixbuf **pixbuf)
{
    GdkPixbuf *pixbuf_with_frame, *frame;

    /* The pixbuf isn't already framed (i.e., it was not made by
     * an old Nemo), so we must embed it in a frame.
     */

    frame = nemo_get_thumbnail_frame ();
    if (frame == NULL) {
        return;
    }

    pixbuf_with_frame = eel_embed_image_in_frame (*pixbuf, frame,
                                                  NEMO_THUMBNAIL_FRAME_LEFT,
                                                  NEMO_THUMBNAIL_FRAME_TOP,
                                                  NEMO_THUMBNAIL_FRAME_RIGHT,
                                                  NEMO_THUMBNAIL_FRAME_BOTTOM);
    g_object_unref (*pixbuf);
    *pixbuf = pixbuf_with_frame;
}

void
nemo_thumbnail_pad_top_and_bottom (GdkPixbuf **pixbuf,
                                   gint        extra_height)
{
    GdkPixbuf *pixbuf_with_padding;
    GdkRectangle rect;
    GdkRGBA transparent = { 0, 0, 0, 0.0 };
    cairo_surface_t *surface;
    cairo_t *cr;
    gint width, height;

    width = gdk_pixbuf_get_width (*pixbuf);
    height = gdk_pixbuf_get_height (*pixbuf);

    surface = gdk_window_create_similar_image_surface (NULL,
                                                       CAIRO_FORMAT_ARGB32,
                                                       width,
                                                       height + extra_height,
                                                       0);

    cr = cairo_create (surface);

    rect.x = 0;
    rect.y = 0;
    rect.width = width;
    rect.height = height + extra_height;

    gdk_cairo_rectangle (cr, &rect);
    gdk_cairo_set_source_rgba (cr, &transparent);
    cairo_fill (cr);

    gdk_cairo_set_source_pixbuf (cr,
                                 *pixbuf,
                                 0,
                                 extra_height / 2);
    cairo_paint (cr);

    pixbuf_with_padding = gdk_pixbuf_get_from_surface (surface,
                                                       0,
                                                       0,
                                                       width,
                                                       height + extra_height);

    g_object_unref (*pixbuf);
    cairo_surface_destroy (surface);
    cairo_destroy (cr);

    *pixbuf = pixbuf_with_padding;
}

gboolean
nemo_thumbnail_factory_check_status (void)
{
    return gnome_desktop_thumbnail_cache_check_permissions (get_thumbnail_factory (), TRUE);
}

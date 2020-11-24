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

#define DEBUG_FLAG NEMO_DEBUG_THUMBNAILS
#include <libnemo-private/nemo-debug.h>

#include "nemo-file-private.h"

/* Should never be a reasonable actual mtime */
#define INVALID_MTIME 0

/* Cool-off period between last file modification time and thumbnail creation */
#define THUMBNAIL_CREATION_DELAY_SECS 3

#define NEMO_THUMBNAIL_FRAME_LEFT 3
#define NEMO_THUMBNAIL_FRAME_TOP 3
#define NEMO_THUMBNAIL_FRAME_RIGHT 3
#define NEMO_THUMBNAIL_FRAME_BOTTOM 3

/* structure used for making thumbnails, associating a uri with where the thumbnail is to be stored */

typedef struct {
    char *image_uri;
    char *mime_type;
    time_t original_file_mtime;
    gint throttle_count;
} NemoThumbnailInfo;

/*
 * Thumbnail thread state.
 */

/* The id of the idle handler used to start the thumbnail thread, or 0 if no
   idle handler is currently registered. */
static guint thumbnail_thread_starter_id = 0;

/* Our mutex used when accessing data shared between the main thread and the
   thumbnail thread, i.e. the thumbnail_thread_is_running flag and the
   thumbnails_to_make list. */
static GMutex thumbnails_mutex;
static GCancellable *thumbnails_cancellable;

/* A flag to indicate whether a thumbnail thread is running, so we don't
   start more than one. Lock thumbnails_mutex when accessing this. */
static volatile gboolean thumbnail_thread_is_running = FALSE;

/* The list of NemoThumbnailInfo structs containing information about the
   thumbnails we are making. Lock thumbnails_mutex when accessing this. */
static volatile GQueue thumbnails_to_make = G_QUEUE_INIT;

/* Quickly check if uri is in thumbnails_to_make list */
static GHashTable *thumbnails_to_make_hash = NULL;

/* The currently thumbnailed icon. it also exists in the thumbnails_to_make list
 * to avoid adding it again. Lock thumbnails_mutex when accessing this. */
static NemoThumbnailInfo *currently_thumbnailing = NULL;

static GnomeDesktopThumbnailFactory *thumbnail_factory = NULL;

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
    if (thumbnail_factory == NULL) {
        thumbnail_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
    }

    return thumbnail_factory;
}

static GdkPixbuf *
nemo_get_thumbnail_frame (void)
{
    static GdkPixbuf *thumbnail_frame = NULL;

    if (thumbnail_frame == NULL) {
        GInputStream *stream = g_resources_open_stream ("/org/nemo/icons/thumbnail_frame.png", 0, NULL);
        if (stream != NULL) {
            thumbnail_frame = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
            g_object_unref (stream);
        }
    }
    
    return thumbnail_frame;
}

void
nemo_thumbnail_frame_image (GdkPixbuf **pixbuf)
{
    GdkPixbuf *pixbuf_with_frame, *frame;
    int left_offset, top_offset, right_offset, bottom_offset;
        
    /* The pixbuf isn't already framed (i.e., it was not made by
     * an old Nemo), so we must embed it in a frame.
     */

    frame = nemo_get_thumbnail_frame ();
    if (frame == NULL) {
        return;
    }
    
    left_offset = NEMO_THUMBNAIL_FRAME_LEFT;
    top_offset = NEMO_THUMBNAIL_FRAME_TOP;
    right_offset = NEMO_THUMBNAIL_FRAME_RIGHT;
    bottom_offset = NEMO_THUMBNAIL_FRAME_BOTTOM;
    
    pixbuf_with_frame = eel_embed_image_in_frame
        (*pixbuf, frame,
         left_offset, top_offset, right_offset, bottom_offset);
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

static GHashTable *
get_types_table (void)
{
    static GHashTable *image_mime_types = NULL;
    GSList *format_list, *l;
    char **types;
    int i;

    if (image_mime_types == NULL) {
        image_mime_types =
            g_hash_table_new_full (g_str_hash, g_str_equal,
                           g_free, NULL);

        format_list = gdk_pixbuf_get_formats ();
        for (l = format_list; l; l = l->next) {
            types = gdk_pixbuf_format_get_mime_types (l->data);

            for (i = 0; types[i] != NULL; i++) {
                g_hash_table_insert (image_mime_types,
                             types [i],
                             GUINT_TO_POINTER (1));
            }

            g_free (types);
        }

        g_slist_free (format_list);
    }

    return image_mime_types;
}

static gboolean
pixbuf_can_load_type (const char *mime_type)
{
    GHashTable *image_mime_types;

    image_mime_types = get_types_table ();
    if (g_hash_table_lookup (image_mime_types, mime_type)) {
        return TRUE;
    }

    return FALSE;
}

gboolean
nemo_can_thumbnail_internally (NemoFile *file)
{
    char *mime_type;
    gboolean res;

    mime_type = nemo_file_get_mime_type (file);
    res = pixbuf_can_load_type (mime_type);
    g_free (mime_type);
    return res;
}

gboolean
nemo_thumbnail_is_mimetype_limited_by_size (const char *mime_type)
{
    return pixbuf_can_load_type (mime_type);
}

gboolean
nemo_can_thumbnail (NemoFile *file)
{
    GnomeDesktopThumbnailFactory *factory;
    gboolean res;
    char *uri;
    time_t mtime;
    char *mime_type;

    uri = nemo_file_get_uri (file);
    mime_type = nemo_file_get_mime_type (file);
    mtime = nemo_file_get_mtime (file);
    
    factory = get_thumbnail_factory ();
    res = gnome_desktop_thumbnail_factory_can_thumbnail (factory,
                                 uri,
                                 mime_type,
                                 mtime);
    g_free (mime_type);
    g_free (uri);

    return res;
}

/***************************************************************************
 * Thumbnail Thread Functions.
 ***************************************************************************/

void
nemo_thumbnail_remove_from_queue (const char *file_uri)
{
    GList *node;

    if (DEBUGGING) {
        g_message ("(Remove from queue) Locking mutex");
    }

    g_mutex_lock (&thumbnails_mutex);

    /*********************************
     * MUTEX LOCKED
     *********************************/

    if (thumbnails_to_make_hash) {
        node = g_hash_table_lookup (thumbnails_to_make_hash, file_uri);
        
        if (node && node->data != currently_thumbnailing) {
            g_hash_table_remove (thumbnails_to_make_hash, file_uri);
            free_thumbnail_info (node->data);
            g_queue_delete_link ((GQueue *)&thumbnails_to_make, node);
        }
    }

    /*********************************
     * MUTEX UNLOCKED
     *********************************/

    if (DEBUGGING) {
        g_message ("(Remove from queue) Unlocking mutex");
    }

    g_mutex_unlock (&thumbnails_mutex);
}

void
nemo_thumbnail_prioritize (const char *file_uri)
{
    GList *node;

    if (DEBUGGING) {
        g_message ("(Prioritize) Locking mutex");
    }

    g_mutex_lock (&thumbnails_mutex);

    /*********************************
     * MUTEX LOCKED
     *********************************/

    if (thumbnails_to_make_hash) {
        node = g_hash_table_lookup (thumbnails_to_make_hash, file_uri);

        if (node && node->data != currently_thumbnailing) {
            g_queue_unlink ((GQueue *)&thumbnails_to_make, node);
            g_queue_push_head_link ((GQueue *)&thumbnails_to_make, node);
        }
    }
    
    /*********************************
     * MUTEX UNLOCKED
     *********************************/
    
    if (DEBUGGING) {
        g_message ("(Prioritize) Unlocking mutex");
    }

    g_mutex_unlock (&thumbnails_mutex);
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

    if (DEBUGGING) {
        g_message ("(Thumbnail Thread) Notifying file changed file:%p uri: %s", file, (char*) image_uri);
    }

    if (file != NULL) {
        nemo_file_set_is_thumbnailing (file, FALSE);
        nemo_file_invalidate_attributes (file,
                             NEMO_FILE_ATTRIBUTE_THUMBNAIL |
                             NEMO_FILE_ATTRIBUTE_INFO);
        nemo_file_unref (file);
    }
    g_free (image_uri);
    // g_printerr ("length: %d  REMOVE\n" , g_hash_table_size (thumbnails_to_make_hash));

    return FALSE;
}

static void
on_thumbnail_thread_finished (GObject      *source,
                              GAsyncResult *res,
                              gpointer      user_data)
{
    GError *error;

    error = NULL;
    g_task_propagate_boolean (G_TASK (res), &error);

    if (error != NULL) {
        g_warning ("Error thumbnailing: %s", error->message);
        g_error_free (error);
    }

    if (DEBUGGING) {
        g_message ("(Main Thread) Thumbnail thread finished");
    }

    /* Thread is no longer running, no need to lock mutex */
    thumbnail_thread_is_running = FALSE;
}

/* thumbnail_thread is invoked as a separate thread to to make thumbnails. */
static void
thumbnail_thread (GTask        *task,
                        gpointer      source_object,
                        gpointer      task_data,
                        GCancellable *cancellable)
{
    NemoThumbnailInfo *info = NULL;
    GdkPixbuf *pixbuf;
    time_t current_orig_mtime = 0;
    time_t current_time;
    GList *node;

    /* We loop until there are no more thumbails to make, at which point
       we exit the thread. */
    for (;;) {
        if (DEBUGGING) {
            g_message ("(Thumbnail Thread) Locking mutex");
        }

        g_mutex_lock (&thumbnails_mutex);

        /*********************************
         * MUTEX LOCKED
         *********************************/

        /* Pop the last thumbnail we just made off the head of the
           list and free it. I did this here so we only have to lock
           the mutex once per thumbnail, rather than once before
           creating it and once after.
           Don't pop the thumbnail off the queue if the original file
           mtime of the request changed. Then we need to redo the thumbnail.
        */
        if (currently_thumbnailing &&
            currently_thumbnailing->original_file_mtime == current_orig_mtime) {
            g_assert (info == currently_thumbnailing);

            node = g_hash_table_lookup (thumbnails_to_make_hash, info->image_uri);

            g_assert (node != NULL);

            g_hash_table_remove (thumbnails_to_make_hash, info->image_uri);
            free_thumbnail_info (info);
            g_queue_delete_link ((GQueue *)&thumbnails_to_make, node);
        }

        currently_thumbnailing = NULL;

        /* If there are no more thumbnails to make, reset the
           thumbnail_thread_is_running flag, unlock the mutex, and
           exit the thread. */
        if (g_queue_is_empty ((GQueue *)&thumbnails_to_make)) {
            if (DEBUGGING) {
                g_message ("(Thumbnail Thread) Exiting");
            }

            g_mutex_unlock (&thumbnails_mutex);
            g_task_return_boolean (task, TRUE);
            return;
        }

        /* Get the next one to make. We leave it on the list until it
           is created so the main thread doesn't add it again while we
           are creating it. */
        info = g_queue_peek_head ((GQueue *)&thumbnails_to_make);
        currently_thumbnailing = info;
        current_orig_mtime = info->original_file_mtime;

        /*********************************
         * MUTEX UNLOCKED
         *********************************/

        if (DEBUGGING) {
            g_message ("(Thumbnail Thread) Unlocking mutex");
        }

        g_mutex_unlock (&thumbnails_mutex);

        time (&current_time);

        /* Don't try to create a thumbnail if the file was modified recently.
           This prevents constant re-thumbnailing of changing files. */ 
        if (current_time < current_orig_mtime + (THUMBNAIL_CREATION_DELAY_SECS * info->throttle_count) &&
            current_time >= current_orig_mtime) {
            if (DEBUGGING) {
                g_message ("(Thumbnail Thread) Skipping for %d seconds: %s",
                           THUMBNAIL_CREATION_DELAY_SECS * info->throttle_count,
                           info->image_uri);
            }

            /* Reschedule thumbnailing via a change notification */
            g_timeout_add_seconds (THUMBNAIL_CREATION_DELAY_SECS * info->throttle_count, thumbnail_thread_notify_file_changed,
                       g_strdup (info->image_uri));
            continue;
        }

        /* Create the thumbnail. */
        if (DEBUGGING) {
            g_message ("(Thumbnail Thread) Creating thumbnail: %s",
                       info->image_uri);
        }

        pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (thumbnail_factory,
                                         info->image_uri,
                                         info->mime_type);

        if (pixbuf) {
            gnome_desktop_thumbnail_factory_save_thumbnail (thumbnail_factory,
                                    pixbuf,
                                    info->image_uri,
                                    current_orig_mtime);
            g_object_unref (pixbuf);
        } else {
            gnome_desktop_thumbnail_factory_create_failed_thumbnail (thumbnail_factory, 
                                         info->image_uri,
                                         current_orig_mtime);
        }

        /* We need to call nemo_file_changed(), but I don't think that is
           thread safe. So add an idle handler and do it from the main loop. */
        g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                 thumbnail_thread_notify_file_changed,
                 g_strdup (info->image_uri), NULL);
    }

    g_task_return_boolean (task, TRUE);
}

/* This function is added as a very low priority idle function to start the
   thread to create any needed thumbnails. It is added with a very low priority
   so that it doesn't delay showing the directory in the icon/list views.
   We want to show the files in the directory as quickly as possible. */
static gboolean
thumbnail_thread_starter_cb (gpointer data)
{
    GTask *thumbnail_task;

    /* Don't do this in thread, since g_object_ref is not threadsafe */
    if (thumbnail_factory == NULL) {
        thumbnail_factory = get_thumbnail_factory ();
    }

    thumbnails_cancellable = g_cancellable_new ();

    if (DEBUGGING) {
        g_message ("(Main Thread) Creating thumbnails thread");
    }

    thumbnail_task = g_task_new (thumbnail_factory,
                                 thumbnails_cancellable,
                                 on_thumbnail_thread_finished,
                                 NULL);

    /* We set a flag to indicate the thread is running, so we don't create
       a new one. We don't need to lock a mutex here, as the thumbnail
       thread isn't running yet. And we know we won't create the thread
       twice, as we also check thumbnail_thread_starter_id before
       scheduling this idle function. */
    thumbnail_thread_is_running = TRUE;

    g_task_run_in_thread (thumbnail_task, thumbnail_thread);
    g_object_unref (thumbnail_task);

    thumbnail_thread_starter_id = 0;
    return FALSE;
}

void
nemo_create_thumbnail (NemoFile      *file,
                       gint           throttle_count,
                       gboolean       prioritize)
{
    time_t file_mtime = 0;
    NemoThumbnailInfo *info;
    NemoThumbnailInfo *existing_info;
    GList *existing, *node;

    /* The gdk-pixbuf-thumbnailer tool has special hardcoded handling for recent: and trash: uris.
     * we need to find the activation uri here instead */
    if (nemo_file_is_in_favorites (file)) {
        NemoFile *real_file;
        gchar *uri;

        uri = nemo_file_get_symbolic_link_target_uri (file);

        real_file = nemo_file_get_by_uri (uri);
        nemo_create_thumbnail (real_file, 0, FALSE);

        nemo_file_unref (real_file);
        return;
    }

    nemo_file_set_is_thumbnailing (file, TRUE);

    info = g_new0 (NemoThumbnailInfo, 1);
    info->image_uri = nemo_file_get_uri (file);

    info->mime_type = nemo_file_get_mime_type (file);    // info->image_uri = nemo_file_is_in_favorites (file) ? nemo_file_get_activation_uri (file) :
    info->throttle_count = MIN (10, throttle_count);
    
    /* Hopefully the NemoFile will already have the image file mtime,
       so we can just use that. Otherwise we have to get it ourselves. */
    if (file->details->got_file_info &&
        file->details->file_info_is_up_to_date &&
        file->details->mtime != 0) {
        file_mtime = file->details->mtime;
    } else {
        get_file_mtime (info->image_uri, &file_mtime);
    }
    
    info->original_file_mtime = file_mtime;

    if (DEBUGGING) {
        g_message ("(Main Thread) Locking mutex");
    }

    g_mutex_lock (&thumbnails_mutex);

    /*********************************
     * MUTEX LOCKED
     *********************************/

    if (thumbnails_to_make_hash == NULL) {
        thumbnails_to_make_hash = g_hash_table_new (g_str_hash,
                                g_str_equal);
    }
    // g_printerr ("length: %d  ADD\n" , g_hash_table_size (thumbnails_to_make_hash));
    /* Check if it is already in the list of thumbnails to make. */
    existing = g_hash_table_lookup (thumbnails_to_make_hash, info->image_uri);

    if (existing == NULL) {
        /* Add the thumbnail to the list. */

        if (DEBUGGING) {
            g_message ("(Main Thread) Adding thumbnail: %s",
                       info->image_uri);
        }

        if (prioritize) {
            g_queue_push_head ((GQueue *)&thumbnails_to_make, info);
            node = g_queue_peek_head_link ((GQueue *)&thumbnails_to_make);
        } else {
            g_queue_push_tail ((GQueue *)&thumbnails_to_make, info);
            node = g_queue_peek_tail_link ((GQueue *)&thumbnails_to_make);
        }

        g_hash_table_insert (thumbnails_to_make_hash,
                     info->image_uri,
                     node);

        /* If the thumbnail thread isn't running, and we haven't
           scheduled an idle function to start it up, do that now.
           We don't want to start it until all the other work is done,
           so the GUI will be updated as quickly as possible.*/

        if (thumbnail_thread_is_running == FALSE &&
            thumbnail_thread_starter_id == 0) {
            thumbnail_thread_starter_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                                           thumbnail_thread_starter_cb,
                                                           NULL, NULL);
        }
    } else {
        if (DEBUGGING) {
            g_message ("(Main Thread) Updating non-current mtime: %s",
                       info->image_uri);
        }

        /* The file in the queue might need a new original mtime */
        existing_info = existing->data;
        existing_info->original_file_mtime = info->original_file_mtime;
        free_thumbnail_info (info);

        if (existing && existing->data != currently_thumbnailing) {
            g_queue_unlink ((GQueue *)&thumbnails_to_make, existing);
            g_queue_push_head_link ((GQueue *)&thumbnails_to_make, existing);
        }
    }

    /*********************************
     * MUTEX UNLOCKED
     *********************************/

    if (DEBUGGING) {
        g_message ("(Main Thread) Unlocking mutex");
    }

    g_mutex_unlock (&thumbnails_mutex);
}

gboolean
nemo_thumbnail_factory_check_status (void)
{
    return gnome_desktop_thumbnail_cache_check_permissions (get_thumbnail_factory (), TRUE);
}

/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
 * nemo-preview-media-info.c - Background probe of media files for the preview pane
 *
 * Copyright (C) 2026 Linux Mint
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#include <config.h>
#include "nemo-preview-media-info.h"
#include "nemo-preview-video.h"

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <string.h>

#define DISCOVER_TIMEOUT   (5 * GST_SECOND)
#define FRAME_GRAB_TIMEOUT (5 * GST_SECOND)
#define FRAME_SEEK_PERCENTAGE 10
#define MAX_KEYFRAME_SCAN 300
#define MAX_COVER_WIDTH 1920

/* Tracks the first keyframe at or after the seek position as encoded buffers enter the decoder */
typedef struct {
    GMutex mutex;
    GstClockTime target;
    GstClockTime keyframe;
} KeyframeTracker;

typedef struct {
    gchar *uri;
    gboolean want_cover;
} FetchData;

static void
fetch_data_free (FetchData *data)
{
    g_free (data->uri);
    g_free (data);
}

void
nemo_preview_media_info_free (NemoPreviewMediaInfo *info)
{
    if (info == NULL) {
        return;
    }

    g_free (info->duration);
    g_free (info->dimensions);
    g_clear_object (&info->cover);
    g_free (info);
}

gboolean
nemo_preview_media_info_supported (NemoFile *file)
{
    gchar *mime_type;
    gboolean is_media;

    mime_type = nemo_file_get_mime_type (file);
    is_media = mime_type != NULL &&
               (g_str_has_prefix (mime_type, "video/") || g_str_has_prefix (mime_type, "audio/"));
    g_free (mime_type);

    return is_media && nemo_preview_video_available ();
}

static GdkPixbuf *
pixbuf_from_tags (const GstTagList *tags)
{
    GstSample *sample = NULL;
    GstBuffer *buffer;
    GstMapInfo map;
    GdkPixbufLoader *loader;
    GdkPixbuf *pixbuf = NULL;

    if (tags == NULL) {
        return NULL;
    }

    if (!gst_tag_list_get_sample (tags, GST_TAG_IMAGE, &sample) &&
        !gst_tag_list_get_sample (tags, GST_TAG_PREVIEW_IMAGE, &sample)) {
        return NULL;
    }

    buffer = gst_sample_get_buffer (sample);

    if (buffer != NULL && gst_buffer_map (buffer, &map, GST_MAP_READ)) {
        loader = gdk_pixbuf_loader_new ();

        if (gdk_pixbuf_loader_write (loader, map.data, map.size, NULL) &&
            gdk_pixbuf_loader_close (loader, NULL)) {
            pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

            if (pixbuf != NULL) {
                g_object_ref (pixbuf);
            }
        } else {
            gdk_pixbuf_loader_close (loader, NULL);
        }

        g_object_unref (loader);
        gst_buffer_unmap (buffer, &map);
    }

    gst_sample_unref (sample);

    return pixbuf;
}

static GdkPixbuf *
cover_from_tags (GstDiscovererInfo *info)
{
    GstDiscovererStreamInfo *top;
    GList *streams, *l;
    GdkPixbuf *pixbuf = NULL;

    top = gst_discoverer_info_get_stream_info (info);

    if (top != NULL) {
        pixbuf = pixbuf_from_tags (gst_discoverer_stream_info_get_tags (top));

#if GST_CHECK_VERSION (1, 20, 0)
        /* MP4/MKV keep the art in container-level tags */
        if (pixbuf == NULL && GST_IS_DISCOVERER_CONTAINER_INFO (top)) {
            pixbuf = pixbuf_from_tags (gst_discoverer_container_info_get_tags (GST_DISCOVERER_CONTAINER_INFO (top)));
        }
#endif

        gst_discoverer_stream_info_unref (top);
    }

    if (pixbuf == NULL) {
        streams = gst_discoverer_info_get_audio_streams (info);

        for (l = streams; l != NULL && pixbuf == NULL; l = l->next) {
            pixbuf = pixbuf_from_tags (gst_discoverer_stream_info_get_tags (l->data));
        }

        gst_discoverer_stream_info_list_free (streams);
    }

    if (pixbuf == NULL) {
        pixbuf = pixbuf_from_tags (gst_discoverer_info_get_tags (info));
    }

    return pixbuf;
}

static GdkPixbuf *
pixbuf_from_video_sample (GstSample *sample)
{
    GstVideoInfo video_info;
    GstVideoFrame frame;
    GdkPixbuf *pixbuf;
    const guint8 *src;
    guint8 *dst;
    gint width, height, src_stride, dst_stride, y;

    if (!gst_video_info_from_caps (&video_info, gst_sample_get_caps (sample)) ||
        !gst_video_frame_map (&frame, &video_info, gst_sample_get_buffer (sample), GST_MAP_READ)) {
        return NULL;
    }

    width = GST_VIDEO_FRAME_WIDTH (&frame);
    height = GST_VIDEO_FRAME_HEIGHT (&frame);
    src = GST_VIDEO_FRAME_PLANE_DATA (&frame, 0);
    src_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, 0);

    pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    dst = gdk_pixbuf_get_pixels (pixbuf);
    dst_stride = gdk_pixbuf_get_rowstride (pixbuf);

    for (y = 0; y < height; y++) {
        memcpy (dst + y * dst_stride, src + y * src_stride, width * 3);
    }

    gst_video_frame_unmap (&frame);

    if (width > MAX_COVER_WIDTH) {
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple (pixbuf, MAX_COVER_WIDTH,
                                                     height * MAX_COVER_WIDTH / width,
                                                     GDK_INTERP_BILINEAR);
        g_object_unref (pixbuf);
        pixbuf = scaled;
    }

    return pixbuf;
}

static GstPadProbeReturn
encoded_buffer_probe (GstPad          *pad,
                      GstPadProbeInfo *info,
                      gpointer         user_data)
{
    KeyframeTracker *tracker = user_data;
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);
    GstClockTime pts = GST_BUFFER_PTS (buffer);

    if (!GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT) &&
        GST_CLOCK_TIME_IS_VALID (pts) && pts >= tracker->target) {
        g_mutex_lock (&tracker->mutex);

        if (!GST_CLOCK_TIME_IS_VALID (tracker->keyframe) || pts < tracker->keyframe) {
            tracker->keyframe = pts;
        }

        g_mutex_unlock (&tracker->mutex);
    }

    return GST_PAD_PROBE_OK;
}

static void
video_decoder_added (GstBin          *bin,
                     GstBin          *sub_bin,
                     GstElement      *element,
                     KeyframeTracker *tracker)
{
    const gchar *klass = gst_element_get_metadata (element, GST_ELEMENT_METADATA_KLASS);
    GstPad *pad;

    if (klass == NULL || strstr (klass, "Decoder") == NULL || strstr (klass, "Video") == NULL) {
        return;
    }

    pad = gst_element_get_static_pad (element, "sink");

    if (pad != NULL) {
        gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, encoded_buffer_probe, tracker, NULL);
        gst_object_unref (pad);
    }
}

static gboolean
sample_is_keyframe (GstSample       *sample,
                    KeyframeTracker *tracker)
{
    GstClockTime pts = GST_BUFFER_PTS (gst_sample_get_buffer (sample));
    GstClockTime keyframe;

    g_mutex_lock (&tracker->mutex);
    keyframe = tracker->keyframe;
    g_mutex_unlock (&tracker->mutex);

    return GST_CLOCK_TIME_IS_VALID (keyframe) && GST_CLOCK_TIME_IS_VALID (pts) && pts >= keyframe;
}

/* Demuxers that honour TRICKMODE_KEY_UNITS preroll on the keyframe directly; the
 * others are scanned forward from the seek position, which is kept as the fallback. */
static GstSample *
pull_keyframe_sample (GstElement      *pipeline,
                      GstAppSink      *sink,
                      KeyframeTracker *tracker)
{
    GstSample *sample, *fallback;
    gint scanned;

    fallback = gst_app_sink_pull_preroll (sink);
    sample = fallback != NULL ? gst_sample_ref (fallback) : NULL;

    for (scanned = 0; sample != NULL && !sample_is_keyframe (sample, tracker); scanned++) {
        if (scanned == 0) {
            gst_element_set_state (pipeline, GST_STATE_PLAYING);
        }

        gst_sample_unref (sample);
        sample = scanned < MAX_KEYFRAME_SCAN ? gst_app_sink_try_pull_sample (sink, FRAME_GRAB_TIMEOUT) : NULL;
    }

    if (sample == NULL) {
        return fallback;
    }

    g_clear_pointer (&fallback, gst_sample_unref);

    return sample;
}

/* Same frame as ffmpegthumbnailer: the first keyframe at or after 10% of the duration. */
static GdkPixbuf *
grab_video_frame (const gchar  *uri,
                  GstClockTime  duration)
{
    GstElement *pipeline, *source, *sink;
    GstSample *sample;
    KeyframeTracker tracker = { 0, };
    GdkPixbuf *pixbuf = NULL;
    GError *error = NULL;

    pipeline = gst_parse_launch ("uridecodebin name=source ! videoconvert ! videoscale ! "
                                 "video/x-raw,format=RGB,pixel-aspect-ratio=1/1 ! "
                                 "appsink name=sink sync=false max-buffers=1",
                                 &error);

    if (pipeline == NULL) {
        g_debug ("Preview frame grab pipeline failed: %s", error->message);
        g_error_free (error);
        return NULL;
    }

    g_mutex_init (&tracker.mutex);
    tracker.keyframe = GST_CLOCK_TIME_NONE;

    if (GST_CLOCK_TIME_IS_VALID (duration)) {
        tracker.target = (duration / GST_SECOND) * FRAME_SEEK_PERCENTAGE / 100 * GST_SECOND;
    }

    g_signal_connect (pipeline, "deep-element-added", G_CALLBACK (video_decoder_added), &tracker);

    source = gst_bin_get_by_name (GST_BIN (pipeline), "source");
    sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
    g_object_set (source, "uri", uri, NULL);

    if (gst_element_set_state (pipeline, GST_STATE_PAUSED) != GST_STATE_CHANGE_FAILURE &&
        gst_element_get_state (pipeline, NULL, NULL, FRAME_GRAB_TIMEOUT) == GST_STATE_CHANGE_SUCCESS) {
        if (tracker.target > 0) {
            gst_element_seek_simple (pipeline, GST_FORMAT_TIME,
                                     GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE |
                                     GST_SEEK_FLAG_TRICKMODE_KEY_UNITS | GST_SEEK_FLAG_TRICKMODE_NO_AUDIO,
                                     tracker.target);
            gst_element_get_state (pipeline, NULL, NULL, FRAME_GRAB_TIMEOUT);
        }

        sample = pull_keyframe_sample (pipeline, GST_APP_SINK (sink), &tracker);

        if (sample != NULL) {
            pixbuf = pixbuf_from_video_sample (sample);
            gst_sample_unref (sample);
        }
    }

    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (source);
    gst_object_unref (sink);
    gst_object_unref (pipeline);
    g_mutex_clear (&tracker.mutex);

    return pixbuf;
}

static void
fetch_thread (GTask        *task,
              gpointer      source_object,
              gpointer      task_data,
              GCancellable *cancellable)
{
    FetchData *data = task_data;
    const gchar *uri = data->uri;
    GstDiscoverer *discoverer;
    GstDiscovererInfo *discoverer_info;
    NemoPreviewMediaInfo *info;
    GError *error = NULL;

    discoverer = gst_discoverer_new (DISCOVER_TIMEOUT, &error);

    if (discoverer == NULL) {
        g_task_return_error (task, error);
        return;
    }

    discoverer_info = gst_discoverer_discover_uri (discoverer, uri, &error);
    g_object_unref (discoverer);

    if (discoverer_info == NULL) {
        g_task_return_error (task, error);
        return;
    }

    info = g_new0 (NemoPreviewMediaInfo, 1);

    if (gst_discoverer_info_get_result (discoverer_info) == GST_DISCOVERER_OK) {
        GstClockTime duration = gst_discoverer_info_get_duration (discoverer_info);
        GList *video_streams;

        if (GST_CLOCK_TIME_IS_VALID (duration) && duration > 0) {
            info->duration = nemo_preview_video_format_time (duration);
        }

        video_streams = gst_discoverer_info_get_video_streams (discoverer_info);

        if (video_streams != NULL) {
            GstDiscovererVideoInfo *video_info = video_streams->data;

            info->dimensions = g_strdup_printf ("%u × %u",
                                                gst_discoverer_video_info_get_width (video_info),
                                                gst_discoverer_video_info_get_height (video_info));
        }

        if (data->want_cover) {
            if (video_streams != NULL && !g_cancellable_is_cancelled (cancellable)) {
                info->cover = grab_video_frame (uri, duration);
            }

            if (info->cover == NULL) {
                info->cover = cover_from_tags (discoverer_info);
            }
        }

        gst_discoverer_stream_info_list_free (video_streams);
    }

    gst_discoverer_info_unref (discoverer_info);

    g_task_return_pointer (task, info, (GDestroyNotify) nemo_preview_media_info_free);
}

void
nemo_preview_media_info_fetch (NemoFile            *file,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
    GTask *task;
    FetchData *data;
    gchar *mime_type;

    data = g_new0 (FetchData, 1);
    data->uri = nemo_file_get_uri (file);

    /* An existing video thumbnail (film strip included) stays as the poster */
    mime_type = nemo_file_get_mime_type (file);
    data->want_cover = !(g_str_has_prefix (mime_type, "video/") && nemo_file_has_loaded_thumbnail (file));
    g_free (mime_type);

    task = g_task_new (NULL, cancellable, callback, user_data);
    g_task_set_task_data (task, data, (GDestroyNotify) fetch_data_free);
    g_task_run_in_thread (task, fetch_thread);
    g_object_unref (task);
}

NemoPreviewMediaInfo *
nemo_preview_media_info_fetch_finish (GAsyncResult  *result,
                                      GError       **error)
{
    return g_task_propagate_pointer (G_TASK (result), error);
}

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

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <glib/gi18n.h>
#include <string.h>

#define DEBUG_FLAG NEMO_DEBUG_PREVIEW_PANE
#include "nemo-debug.h"

#define DISCOVER_TIMEOUT   (5 * GST_SECOND)
#define FRAME_GRAB_TIMEOUT (5 * GST_SECOND)
#define FRAME_SEEK_PERCENTAGE 10
#define MAX_KEYFRAME_SCAN 300
#define MAX_COVER_WIDTH 1920

/* Tracks the first keyframe at or after the seek position as encoded buffers enter the decoder.
 * The keyframe is written from the decoder's streaming thread and read from the fetch thread. */
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

/* Playlists are audio/video types, but playbin can't play them */
static gboolean
is_playlist (const gchar *mime_type)
{
    static const gchar *playlist_types[] = {
        "audio/x-mpegurl",
        "video/vnd.mpegurl",
        "audio/x-scpls",
        "audio/x-ms-asx",
        "audio/x-iriver-pla"
    };
    guint i;

    for (i = 0; i < G_N_ELEMENTS (playlist_types); i++) {
        if (g_content_type_is_a (mime_type, playlist_types[i])) {
            return TRUE;
        }
    }

    return FALSE;
}

/* Metadata only needs core GStreamer and pbutils; playback has its own check */
gboolean
nemo_preview_media_info_available (void)
{
    static gsize initialized = 0;
    static gboolean available = FALSE;

    if (g_once_init_enter (&initialized)) {
        GError *error = NULL;

        if (gst_init_check (NULL, NULL, &error)) {
            gst_pb_utils_init ();
            available = TRUE;
        } else {
            DEBUG ("GStreamer failed to initialize, media previews disabled: %s",
                   error != NULL ? error->message : "unknown error");
            g_clear_error (&error);
        }

        g_once_init_leave (&initialized, 1);
    }

    return available;
}

gboolean
nemo_preview_media_info_supported (NemoFile *file)
{
    gchar *mime_type;
    gboolean is_media;

    mime_type = nemo_file_get_mime_type (file);
    is_media = mime_type != NULL &&
               (g_str_has_prefix (mime_type, "video/") || g_str_has_prefix (mime_type, "audio/")) &&
               !is_playlist (mime_type);
    g_free (mime_type);

    return is_media && nemo_preview_media_info_available ();
}

static GBytes *
cover_bytes_from_tag_list (const GstTagList *tags)
{
    GstSample *sample = NULL;
    GstBuffer *buffer;
    GstMapInfo map;
    GBytes *bytes = NULL;

    if (tags == NULL) {
        return NULL;
    }

    if (!gst_tag_list_get_sample (tags, GST_TAG_IMAGE, &sample) &&
        !gst_tag_list_get_sample (tags, GST_TAG_PREVIEW_IMAGE, &sample)) {
        return NULL;
    }

    buffer = gst_sample_get_buffer (sample);

    if (buffer != NULL && gst_buffer_map (buffer, &map, GST_MAP_READ)) {
        if (map.size > 0) {
            bytes = g_bytes_new (map.data, map.size);
        }

        gst_buffer_unmap (buffer, &map);
    }

    gst_sample_unref (sample);

    return bytes;
}

/* The art stays encoded so the image widget can decode it at its own size */
static GBytes *
cover_bytes_from_tags (GstDiscovererInfo *info)
{
    GstDiscovererStreamInfo *top;
    GList *streams, *l;
    GBytes *bytes = NULL;

    top = gst_discoverer_info_get_stream_info (info);

    if (top != NULL) {
        bytes = cover_bytes_from_tag_list (gst_discoverer_stream_info_get_tags (top));

#if GST_CHECK_VERSION (1, 20, 0)
        /* MP4/MKV keep the art in container-level tags */
        if (bytes == NULL && GST_IS_DISCOVERER_CONTAINER_INFO (top)) {
            bytes = cover_bytes_from_tag_list (gst_discoverer_container_info_get_tags (GST_DISCOVERER_CONTAINER_INFO (top)));
        }
#endif

        gst_discoverer_stream_info_unref (top);
    }

    if (bytes == NULL) {
        streams = gst_discoverer_info_get_audio_streams (info);

        for (l = streams; l != NULL && bytes == NULL; l = l->next) {
            bytes = cover_bytes_from_tag_list (gst_discoverer_stream_info_get_tags (l->data));
        }

        gst_discoverer_stream_info_list_free (streams);
    }

#if !GST_CHECK_VERSION (1, 20, 0)
    if (bytes == NULL) {
        bytes = cover_bytes_from_tag_list (gst_discoverer_info_get_tags (info));
    }
#endif

    return bytes;
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

    if (pixbuf == NULL) {
        DEBUG ("Unable to allocate a %dx%d frame", width, height);
        gst_video_frame_unmap (&frame);
        return NULL;
    }

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
 * others are scanned forward from the prerolled frame, which is kept as the fallback. */
static GstSample *
pull_keyframe_sample (GstElement      *pipeline,
                      GstAppSink      *sink,
                      KeyframeTracker *tracker,
                      GCancellable    *cancellable)
{
    GstSample *sample, *fallback;
    gint scanned;

    fallback = gst_app_sink_try_pull_preroll (sink, FRAME_GRAB_TIMEOUT);
    sample = fallback != NULL ? gst_sample_ref (fallback) : NULL;

    for (scanned = 0; sample != NULL && !sample_is_keyframe (sample, tracker); scanned++) {
        /* PAUSED only yields the preroll sample; run (unsynced) to pull more */
        if (scanned == 0) {
            gst_element_set_state (pipeline, GST_STATE_PLAYING);
        }

        gst_sample_unref (sample);
        sample = scanned < MAX_KEYFRAME_SCAN && !g_cancellable_is_cancelled (cancellable) ?
                 gst_app_sink_try_pull_sample (sink, FRAME_GRAB_TIMEOUT) : NULL;
    }

    if (sample == NULL) {
        return fallback;
    }

    g_clear_pointer (&fallback, gst_sample_unref);

    return sample;
}

/* A zero target or a rejected seek grabs the first frame; a seek that doesn't preroll
 * in time, or errors, ends the grab. */
static gboolean
seek_to_target (GstElement   *pipeline,
                GstClockTime  target)
{
    if (target == 0 ||
        !gst_element_seek_simple (pipeline, GST_FORMAT_TIME,
                                  GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE |
                                  GST_SEEK_FLAG_TRICKMODE_KEY_UNITS | GST_SEEK_FLAG_TRICKMODE_NO_AUDIO,
                                  target)) {
        return TRUE;
    }

    return gst_element_get_state (pipeline, NULL, NULL, FRAME_GRAB_TIMEOUT) == GST_STATE_CHANGE_SUCCESS;
}

/* Roughly matches ffmpegthumbnailer: the first keyframe at or after FRAME_SEEK_PERCENTAGE
 * of the duration, in whole seconds. */
static GdkPixbuf *
grab_video_frame (const gchar  *uri,
                  GstClockTime  duration,
                  GCancellable *cancellable)
{
    GstElement *pipeline, *source, *sink;
    GstSample *sample = NULL;
    KeyframeTracker tracker = { 0, };
    GdkPixbuf *pixbuf = NULL;
    GError *error = NULL;

    pipeline = gst_parse_launch_full ("uridecodebin name=source ! videoconvert ! videoscale ! "
                                      "video/x-raw,format=RGB,pixel-aspect-ratio=1/1 ! "
                                      "appsink name=sink sync=false max-buffers=1",
                                      NULL,
                                      GST_PARSE_FLAG_FATAL_ERRORS,
                                      &error);

    if (pipeline == NULL) {
        DEBUG ("Frame grab pipeline failed: %s", error->message);
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
        gst_element_get_state (pipeline, NULL, NULL, FRAME_GRAB_TIMEOUT) == GST_STATE_CHANGE_SUCCESS &&
        !g_cancellable_is_cancelled (cancellable) &&
        seek_to_target (pipeline, tracker.target)) {
        sample = pull_keyframe_sample (pipeline, GST_APP_SINK (sink), &tracker, cancellable);
    }

    if (sample != NULL) {
        pixbuf = pixbuf_from_video_sample (sample);
        gst_sample_unref (sample);
    } else if (!g_cancellable_is_cancelled (cancellable)) {
        DEBUG ("Frame grab of '%s' failed or timed out", uri);
    }

    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (source);
    gst_object_unref (sink);
    gst_object_unref (pipeline);
    g_mutex_clear (&tracker.mutex);

    return pixbuf;
}

static gchar *
describe_codec (GstDiscovererStreamInfo *stream)
{
    GstCaps *caps = gst_discoverer_stream_info_get_caps (stream);
    gchar *description = NULL;

    if (caps != NULL) {
        description = gst_pb_utils_get_codec_description (caps);
        gst_caps_unref (caps);
    }

    return description;
}

static gchar *
describe_audio_stream (GstDiscovererAudioInfo *audio_info)
{
    GPtrArray *parts = g_ptr_array_new_with_free_func (g_free);
    guint sample_rate = gst_discoverer_audio_info_get_sample_rate (audio_info);
    guint channels = gst_discoverer_audio_info_get_channels (audio_info);
    guint bitrate = gst_discoverer_audio_info_get_bitrate (audio_info);
    gchar *codec;

    codec = describe_codec (GST_DISCOVERER_STREAM_INFO (audio_info));

    if (codec != NULL) {
        g_ptr_array_add (parts, codec);
    }

    if (sample_rate > 0) {
        /* Translators: an audio sample rate, e.g. "44.1 kHz" */
        g_ptr_array_add (parts, g_strdup_printf (_("%g kHz"), sample_rate / 1000.0));
    }

    if (channels == 1) {
        g_ptr_array_add (parts, g_strdup (_("Mono")));
    } else if (channels == 2) {
        g_ptr_array_add (parts, g_strdup (_("Stereo")));
    } else if (channels > 2) {
        /* Translators: the number of audio channels, only used for more than two */
        g_ptr_array_add (parts, g_strdup_printf (ngettext ("%d channel", "%d channels", channels), (gint) channels));
    }

    if (bitrate > 0) {
        /* Translators: an audio bitrate, e.g. "320 kbps" */
        g_ptr_array_add (parts, g_strdup_printf (_("%d kbps"), (gint) ((bitrate + 500) / 1000)));
    }

    return nemo_preview_join_parts (parts);
}

static gchar *
get_tag_string (const GstTagList *tags,
                const gchar      *tag)
{
    gchar *value = NULL;

    gst_tag_list_get_string (tags, tag, &value);

    return nemo_preview_take_stripped (value);
}

static gchar *
get_tag_year (const GstTagList *tags)
{
    GstDateTime *date_time;
    GDate *date;
    gint year = 0;

    if (gst_tag_list_get_date_time (tags, GST_TAG_DATE_TIME, &date_time)) {
        if (gst_date_time_has_year (date_time)) {
            year = gst_date_time_get_year (date_time);
        }

        gst_date_time_unref (date_time);
    } else if (gst_tag_list_get_date (tags, GST_TAG_DATE, &date)) {
        if (g_date_valid (date)) {
            year = g_date_get_year (date);
        }

        g_date_free (date);
    }

    return year > 0 ? g_strdup_printf ("%d", year) : NULL;
}

static gchar *
get_tag_track (const GstTagList *tags)
{
    guint track, count;

    if (!gst_tag_list_get_uint (tags, GST_TAG_TRACK_NUMBER, &track) || track == 0) {
        return NULL;
    }

    if (gst_tag_list_get_uint (tags, GST_TAG_TRACK_COUNT, &count) && count >= track) {
        /* Translators: a track number and the album's track count, e.g. "3 of 12" */
        return g_strdup_printf (_("%d of %d"), (gint) track, (gint) count);
    }

    return g_strdup_printf ("%d", (gint) track);
}

/* File-level tags (title, artist...) live on the container for MP4/MKV, and on
 * the audio stream for MP3/FLAC/Ogg. Container values win when both have one. */
static GstTagList *
get_file_tags (GstDiscovererInfo *info)
{
#if GST_CHECK_VERSION (1, 20, 0)
    GstDiscovererStreamInfo *top;
    GList *audio_streams;
    const GstTagList *container_tags = NULL;
    const GstTagList *stream_tags = NULL;
    GstTagList *tags;

    top = gst_discoverer_info_get_stream_info (info);

    if (top != NULL && GST_IS_DISCOVERER_CONTAINER_INFO (top)) {
        container_tags = gst_discoverer_container_info_get_tags (GST_DISCOVERER_CONTAINER_INFO (top));
    }

    audio_streams = gst_discoverer_info_get_audio_streams (info);

    if (audio_streams != NULL) {
        stream_tags = gst_discoverer_stream_info_get_tags (audio_streams->data);
    }

    tags = gst_tag_list_merge (container_tags, stream_tags, GST_TAG_MERGE_KEEP);

    gst_discoverer_stream_info_list_free (audio_streams);

    if (top != NULL) {
        gst_discoverer_stream_info_unref (top);
    }

    return tags;
#else
    const GstTagList *tags = gst_discoverer_info_get_tags (info);

    return tags != NULL ? gst_tag_list_copy (tags) : NULL;
#endif
}

static void
read_audio_tags (const GstTagList  *tags,
                 gchar            **rows)
{
    if (tags == NULL) {
        return;
    }

    rows[NEMO_PREVIEW_ROW_TITLE] = get_tag_string (tags, GST_TAG_TITLE);
    rows[NEMO_PREVIEW_ROW_ARTIST] = get_tag_string (tags, GST_TAG_ARTIST);
    rows[NEMO_PREVIEW_ROW_ALBUM] = get_tag_string (tags, GST_TAG_ALBUM);
    rows[NEMO_PREVIEW_ROW_YEAR] = get_tag_year (tags);
    rows[NEMO_PREVIEW_ROW_TRACK] = get_tag_track (tags);
    rows[NEMO_PREVIEW_ROW_GENRE] = get_tag_string (tags, GST_TAG_GENRE);
}

static void
read_video_stream (GstDiscovererVideoInfo  *video_info,
                   gchar                  **rows)
{
    guint rate_num = gst_discoverer_video_info_get_framerate_num (video_info);
    guint rate_denom = gst_discoverer_video_info_get_framerate_denom (video_info);

    rows[NEMO_PREVIEW_ROW_DIMENSIONS] = nemo_preview_format_dimensions (gst_discoverer_video_info_get_width (video_info),
                                                                               gst_discoverer_video_info_get_height (video_info));

    if (rate_num > 0 && rate_denom > 0) {
        /* Translators: a video frame rate, e.g. "29.97 fps" */
        rows[NEMO_PREVIEW_ROW_FRAME_RATE] = g_strdup_printf (_("%.4g fps"), (gdouble) rate_num / rate_denom);
    }

    rows[NEMO_PREVIEW_ROW_VIDEO_CODEC] = describe_codec (GST_DISCOVERER_STREAM_INFO (video_info));
}

/* Embedded cover art shows up as a still-image video stream */
static GstDiscovererVideoInfo *
find_moving_video (GList *video_streams)
{
    GList *l;

    for (l = video_streams; l != NULL; l = l->next) {
        if (!gst_discoverer_video_info_is_image (l->data)) {
            return l->data;
        }
    }

    return NULL;
}

static void
log_discoverer_result (const gchar        *uri,
                       GstDiscovererInfo  *discoverer_info)
{
    const gchar **missing;
    gint i;

    switch (gst_discoverer_info_get_result (discoverer_info)) {
    case GST_DISCOVERER_OK:
        break;
    case GST_DISCOVERER_MISSING_PLUGINS:
        DEBUG ("Discovery of %s is missing plugins", uri);
        missing = gst_discoverer_info_get_missing_elements_installer_details (discoverer_info);

        for (i = 0; missing != NULL && missing[i] != NULL; i++) {
            DEBUG ("    %s", missing[i]);
        }
        break;
    case GST_DISCOVERER_TIMEOUT:
        DEBUG ("Discovery of %s timed out", uri);
        break;
    default:
        DEBUG ("Discovery of %s failed with result %d", uri, gst_discoverer_info_get_result (discoverer_info));
        break;
    }
}

/* Installer details are "gstreamer|1.0|application|description|detail" */
static gchar *
describe_missing_plugins (GstDiscovererInfo *discoverer_info)
{
    const gchar **details = gst_discoverer_info_get_missing_elements_installer_details (discoverer_info);
    GPtrArray *parts = g_ptr_array_new_with_free_func (g_free);
    gint i;

    for (i = 0; details != NULL && details[i] != NULL; i++) {
        gchar **fields = g_strsplit (details[i], "|", 5);

        if (g_strv_length (fields) >= 4 && *fields[3] != '\0') {
            g_ptr_array_add (parts, g_strdup (fields[3]));
        }

        g_strfreev (fields);
    }

    return nemo_preview_join_parts (parts);
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
    GstDiscovererResult result;
    GstClockTime duration;
    GList *video_streams, *audio_streams;
    GstDiscovererVideoInfo *video_info;
    NemoPreviewInfo *info;
    GError *error = NULL;

    /* Skip fetches cancelled while still queued (e.g. arrowing through a folder) */
    if (g_task_return_error_if_cancelled (task)) {
        return;
    }

    discoverer = gst_discoverer_new (DISCOVER_TIMEOUT, &error);

    if (discoverer == NULL) {
        DEBUG ("Unable to create discoverer: %s", error->message);
        g_task_return_error (task, error);
        return;
    }

    discoverer_info = gst_discoverer_discover_uri (discoverer, uri, &error);
    g_object_unref (discoverer);

    if (discoverer_info == NULL) {
        DEBUG ("Discovery of %s failed: %s", uri, error->message);
        g_task_return_error (task, error);
        return;
    }

    /* Also set alongside a partial result, e.g. for missing plugins */
    if (error != NULL) {
        DEBUG ("Discovery of %s: %s", uri, error->message);
        g_clear_error (&error);
    }

    log_discoverer_result (uri, discoverer_info);

    result = gst_discoverer_info_get_result (discoverer_info);

    /* With missing plugins, the streams that can be decoded are still described */
    if (result != GST_DISCOVERER_OK && result != GST_DISCOVERER_MISSING_PLUGINS) {
        gst_discoverer_info_unref (discoverer_info);
        g_task_return_new_error (task, G_IO_ERROR,
                                 result == GST_DISCOVERER_TIMEOUT ? G_IO_ERROR_TIMED_OUT : G_IO_ERROR_FAILED,
                                 "Discovery of %s failed with result %d", uri, result);
        return;
    }

    info = g_new0 (NemoPreviewInfo, 1);
    duration = gst_discoverer_info_get_duration (discoverer_info);

    if (GST_CLOCK_TIME_IS_VALID (duration) && duration > 0) {
        info->duration = duration;
        info->rows[NEMO_PREVIEW_ROW_DURATION] = nemo_preview_format_time (duration);
    }

    video_streams = gst_discoverer_info_get_video_streams (discoverer_info);
    audio_streams = gst_discoverer_info_get_audio_streams (discoverer_info);
    video_info = find_moving_video (video_streams);

    if (video_info != NULL) {
        read_video_stream (video_info, info->rows);
    } else {
        GstTagList *tags = get_file_tags (discoverer_info);

        read_audio_tags (tags, info->rows);
        g_clear_pointer (&tags, gst_tag_list_unref);
    }

    if (audio_streams != NULL) {
        info->rows[video_info != NULL ? NEMO_PREVIEW_ROW_AUDIO : NEMO_PREVIEW_ROW_FORMAT] =
            describe_audio_stream (audio_streams->data);
    }

    if (data->want_cover) {
        /* Missing plugins may include the video decoder, so only grab when discovery fully succeeded */
        if (video_info != NULL && result == GST_DISCOVERER_OK && !g_cancellable_is_cancelled (cancellable)) {
            info->cover = grab_video_frame (uri, duration, cancellable);
        }

        if (info->cover == NULL) {
            info->cover_bytes = cover_bytes_from_tags (discoverer_info);
        }
    }

    gst_discoverer_stream_info_list_free (video_streams);
    gst_discoverer_stream_info_list_free (audio_streams);

    if (result == GST_DISCOVERER_MISSING_PLUGINS) {
        info->rows[NEMO_PREVIEW_ROW_MISSING_CODECS] = describe_missing_plugins (discoverer_info);
    }

    gst_discoverer_info_unref (discoverer_info);

    g_task_return_pointer (task, info, (GDestroyNotify) nemo_preview_info_free);
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

    g_return_if_fail (NEMO_IS_FILE (file));

    data = g_new0 (FetchData, 1);
    data->uri = nemo_file_get_activation_uri (file);

    /* An existing video thumbnail (film strip included) stays as the poster */
    mime_type = nemo_file_get_mime_type (file);
    data->want_cover = !(g_str_has_prefix (mime_type, "video/") && nemo_file_has_loaded_thumbnail (file));
    g_free (mime_type);

    task = g_task_new (NULL, cancellable, callback, user_data);
    g_task_set_source_tag (task, nemo_preview_media_info_fetch);
    g_task_set_task_data (task, data, (GDestroyNotify) fetch_data_free);
    g_task_run_in_thread (task, fetch_thread);
    g_object_unref (task);
}

NemoPreviewInfo *
nemo_preview_media_info_fetch_finish (GAsyncResult  *result,
                                      GError       **error)
{
    g_return_val_if_fail (g_task_is_valid (result, NULL), NULL);
    g_return_val_if_fail (g_async_result_is_tagged (result, nemo_preview_media_info_fetch), NULL);

    return g_task_propagate_pointer (G_TASK (result), error);
}

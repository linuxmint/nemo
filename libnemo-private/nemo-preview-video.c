/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
 * nemo-preview-video.c - Video playback widget for the preview pane
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
#include "nemo-preview-video.h"
#include "nemo-preview-image.h"

#include <gst/gst.h>
#include <glib/gi18n.h>

#define POSITION_UPDATE_MS 200
#define AUTOPLAY_DELAY_MS 1000
#define POSTER_PLAY_ICON_MARGIN 8

#define PAGE_POSTER  "poster"
#define PAGE_VIDEO   "video"
#define PAGE_MESSAGE "message"

struct _NemoPreviewVideo {
    GtkBox parent;
};

typedef struct {
    GtkWidget *stack;
    GtkWidget *poster;
    GtkWidget *sink_widget;
    GtkWidget *message_label;
    GtkWidget *play_button;
    GtkWidget *play_image;
    GtkWidget *seek_scale;
    GtkWidget *time_label;
    GtkWidget *mute_button;
    GtkWidget *mute_image;
    GtkWidget *autoplay_button;
    GtkWidget *poster_play_button;

    NemoFile *file;

    GstElement *sink;
    GstElement *pipeline;
    guint bus_watch_id;
    guint position_timeout_id;
    guint autoplay_timeout_id;
    gint64 duration;
    gboolean playing;
    gboolean scrubbing;
} NemoPreviewVideoPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NemoPreviewVideo, nemo_preview_video, GTK_TYPE_BOX)

/* Auto-play is a session-only setting shared by every preview in the process. */
static gboolean autoplay_enabled = FALSE;
static GList *instances = NULL;

gboolean
nemo_preview_video_available (void)
{
    static gsize initialized = 0;
    static gboolean available = FALSE;

    if (g_once_init_enter (&initialized)) {
        if (gst_init_check (NULL, NULL, NULL)) {
            GstElement *sink, *playbin;

            sink = gst_element_factory_make ("gtksink", NULL);
            playbin = gst_element_factory_make ("playbin", NULL);
            available = sink != NULL && playbin != NULL;

            if (sink != NULL) {
                gst_object_unref (gst_object_ref_sink (sink));
            }
            if (playbin != NULL) {
                gst_object_unref (gst_object_ref_sink (playbin));
            }
        }

        g_once_init_leave (&initialized, 1);
    }

    return available;
}

gchar *
nemo_preview_video_format_time (gint64 nanoseconds)
{
    gint64 seconds = MAX (nanoseconds, 0) / GST_SECOND;
    gint hours = seconds / 3600;
    gint minutes = (seconds % 3600) / 60;
    gint secs = seconds % 60;

    if (hours > 0) {
        return g_strdup_printf ("%d:%02d:%02d", hours, minutes, secs);
    }

    return g_strdup_printf ("%d:%02d", minutes, secs);
}

static void
update_time_label (NemoPreviewVideo *video,
                   gint64            position)
{
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);
    gchar *pos_str, *dur_str, *text;

    pos_str = nemo_preview_video_format_time (position);
    dur_str = nemo_preview_video_format_time (priv->duration);
    text = g_strdup_printf ("%s / %s", pos_str, dur_str);

    gtk_label_set_text (GTK_LABEL (priv->time_label), text);

    g_free (pos_str);
    g_free (dur_str);
    g_free (text);
}

static void
update_duration (NemoPreviewVideo *video)
{
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);
    gint64 duration;

    if (priv->pipeline == NULL) {
        return;
    }

    if (gst_element_query_duration (priv->pipeline, GST_FORMAT_TIME, &duration) && duration > 0) {
        priv->duration = duration;
        gtk_range_set_range (GTK_RANGE (priv->seek_scale), 0, (gdouble) duration / GST_SECOND);
        gtk_widget_set_sensitive (priv->seek_scale, TRUE);
    }
}

static void
refresh_position (NemoPreviewVideo *video)
{
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);
    gint64 position;

    if (priv->pipeline == NULL) {
        return;
    }

    if (priv->duration <= 0) {
        update_duration (video);
    }

    if (gst_element_query_position (priv->pipeline, GST_FORMAT_TIME, &position)) {
        if (!priv->scrubbing) {
            gtk_range_set_value (GTK_RANGE (priv->seek_scale), (gdouble) position / GST_SECOND);
        }
        update_time_label (video, position);
    }
}

static gboolean
position_timeout (gpointer user_data)
{
    refresh_position (NEMO_PREVIEW_VIDEO (user_data));

    return G_SOURCE_CONTINUE;
}

static void
set_playing (NemoPreviewVideo *video,
             gboolean          playing)
{
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);

    const gchar *icon_name = playing ? "xsi-media-playback-pause-symbolic" : "xsi-media-playback-start-symbolic";

    priv->playing = playing;

    gtk_image_set_from_icon_name (GTK_IMAGE (priv->play_image), icon_name, GTK_ICON_SIZE_BUTTON);

    if (playing) {
        gtk_widget_hide (priv->poster_play_button);
    }

    if (playing && priv->position_timeout_id == 0) {
        priv->position_timeout_id = g_timeout_add (POSITION_UPDATE_MS, position_timeout, video);
    } else if (!playing && priv->position_timeout_id != 0) {
        g_source_remove (priv->position_timeout_id);
        priv->position_timeout_id = 0;
    }
}

static void
cancel_autoplay (NemoPreviewVideo *video)
{
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);

    if (priv->autoplay_timeout_id != 0) {
        g_source_remove (priv->autoplay_timeout_id);
        priv->autoplay_timeout_id = 0;
    }
}

static gboolean
autoplay_timeout (gpointer user_data)
{
    NemoPreviewVideo *video = NEMO_PREVIEW_VIDEO (user_data);
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);

    priv->autoplay_timeout_id = 0;

    if (priv->pipeline == NULL) {
        nemo_preview_video_toggle_playback (video);
    }

    return G_SOURCE_REMOVE;
}

/* Delay so that stepping through a list of media doesn't start each one. */
static void
schedule_autoplay (NemoPreviewVideo *video)
{
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);

    cancel_autoplay (video);

    if (autoplay_enabled && priv->file != NULL && priv->pipeline == NULL) {
        priv->autoplay_timeout_id = g_timeout_add (AUTOPLAY_DELAY_MS, autoplay_timeout, video);
    }
}

static void
stop_pipeline (NemoPreviewVideo *video)
{
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);

    cancel_autoplay (video);
    set_playing (video, FALSE);

    if (priv->pipeline != NULL) {
        gst_element_set_state (priv->pipeline, GST_STATE_NULL);
        g_clear_pointer (&priv->pipeline, gst_object_unref);
    }

    if (priv->bus_watch_id != 0) {
        g_source_remove (priv->bus_watch_id);
        priv->bus_watch_id = 0;
    }

    priv->duration = 0;
    priv->scrubbing = FALSE;

    gtk_range_set_range (GTK_RANGE (priv->seek_scale), 0, 1);
    gtk_range_set_value (GTK_RANGE (priv->seek_scale), 0);
    gtk_widget_set_sensitive (priv->seek_scale, FALSE);
    update_time_label (video, 0);

    gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), PAGE_POSTER);
}

static gboolean
bus_callback (GstBus     *bus,
              GstMessage *message,
              gpointer    user_data)
{
    NemoPreviewVideo *video = NEMO_PREVIEW_VIDEO (user_data);
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);

    switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR: {
        GError *error = NULL;
        gchar *debug = NULL;

        gst_message_parse_error (message, &error, &debug);
        g_debug ("Preview playback failed: %s (%s)", error->message, debug != NULL ? debug : "");

        stop_pipeline (video);
        gtk_widget_set_tooltip_text (priv->message_label, error->message);
        gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), PAGE_MESSAGE);

        g_error_free (error);
        g_free (debug);
        break;
    }
    case GST_MESSAGE_EOS:
        stop_pipeline (video);
        break;
    case GST_MESSAGE_STATE_CHANGED:
        if (GST_MESSAGE_SRC (message) == GST_OBJECT (priv->pipeline)) {
            GstState new_state;

            gst_message_parse_state_changed (message, NULL, &new_state, NULL);

            if (new_state >= GST_STATE_PAUSED) {
                gint n_video = 0;

                /* Audio-only files keep showing the poster */
                g_object_get (priv->pipeline, "n-video", &n_video, NULL);

                if (n_video > 0) {
                    gtk_stack_set_visible_child_name (GTK_STACK (priv->stack), PAGE_VIDEO);
                }

                update_duration (video);
            }

            set_playing (video, new_state == GST_STATE_PLAYING);
        }
        break;
    case GST_MESSAGE_DURATION_CHANGED:
        priv->duration = 0;
        update_duration (video);
        break;
    case GST_MESSAGE_ASYNC_DONE:
        refresh_position (video);
        break;
    default:
        break;
    }

    return G_SOURCE_CONTINUE;
}

static void
ensure_pipeline (NemoPreviewVideo *video)
{
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);
    GstBus *bus;
    gchar *uri;

    if (priv->pipeline != NULL) {
        return;
    }

    uri = nemo_file_get_uri (priv->file);

    priv->pipeline = gst_element_factory_make ("playbin", NULL);
    g_object_set (priv->pipeline,
                  "uri", uri,
                  "video-sink", priv->sink,
                  "mute", gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->mute_button)),
                  NULL);

    bus = gst_element_get_bus (priv->pipeline);
    priv->bus_watch_id = gst_bus_add_watch (bus, bus_callback, video);
    gst_object_unref (bus);

    g_free (uri);
}

gboolean
nemo_preview_video_toggle_playback (NemoPreviewVideo *video)
{
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);

    if (priv->file == NULL) {
        return FALSE;
    }

    ensure_pipeline (video);
    gst_element_set_state (priv->pipeline, priv->playing ? GST_STATE_PAUSED : GST_STATE_PLAYING);

    return TRUE;
}

static void
play_button_clicked (GtkButton *button,
                     gpointer   user_data)
{
    nemo_preview_video_toggle_playback (NEMO_PREVIEW_VIDEO (user_data));
}

static gboolean
sink_widget_button_press (GtkWidget      *widget,
                          GdkEventButton *event,
                          gpointer        user_data)
{
    if (event->button == GDK_BUTTON_PRIMARY && event->type == GDK_BUTTON_PRESS) {
        nemo_preview_video_toggle_playback (NEMO_PREVIEW_VIDEO (user_data));
        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static void
mute_button_toggled (GtkToggleButton *button,
                     gpointer         user_data)
{
    NemoPreviewVideo *video = NEMO_PREVIEW_VIDEO (user_data);
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);
    gboolean muted = gtk_toggle_button_get_active (button);

    gtk_image_set_from_icon_name (GTK_IMAGE (priv->mute_image),
                                  muted ? "xsi-audio-volume-muted-symbolic" : "xsi-audio-volume-high-symbolic",
                                  GTK_ICON_SIZE_BUTTON);

    if (priv->pipeline != NULL) {
        g_object_set (priv->pipeline, "mute", muted, NULL);
    }
}

static void autoplay_button_toggled (GtkToggleButton *button, gpointer user_data);

static void
sync_autoplay_button (NemoPreviewVideo *video)
{
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);

    g_signal_handlers_block_by_func (priv->autoplay_button, autoplay_button_toggled, video);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->autoplay_button), autoplay_enabled);
    g_signal_handlers_unblock_by_func (priv->autoplay_button, autoplay_button_toggled, video);
}

static void
autoplay_button_toggled (GtkToggleButton *button,
                         gpointer         user_data)
{
    GList *l;

    autoplay_enabled = gtk_toggle_button_get_active (button);

    for (l = instances; l != NULL; l = l->next) {
        NemoPreviewVideo *other = NEMO_PREVIEW_VIDEO (l->data);

        if (other != user_data) {
            sync_autoplay_button (other);
        }

        schedule_autoplay (other);
    }
}

static gboolean
seek_scale_change_value (GtkRange      *range,
                         GtkScrollType  scroll,
                         gdouble        value,
                         gpointer       user_data)
{
    NemoPreviewVideo *video = NEMO_PREVIEW_VIDEO (user_data);
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);
    gint64 position;

    if (priv->pipeline == NULL || priv->duration <= 0) {
        return GDK_EVENT_STOP;
    }

    position = CLAMP (value, 0, (gdouble) priv->duration / GST_SECOND) * GST_SECOND;

    gst_element_seek_simple (priv->pipeline, GST_FORMAT_TIME,
                             GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, position);
    update_time_label (video, position);

    return GDK_EVENT_PROPAGATE;
}

static gboolean
seek_scale_button_press (GtkWidget      *widget,
                         GdkEventButton *event,
                         gpointer        user_data)
{
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (NEMO_PREVIEW_VIDEO (user_data));

    priv->scrubbing = TRUE;

    return GDK_EVENT_PROPAGATE;
}

static gboolean
seek_scale_button_release (GtkWidget      *widget,
                           GdkEventButton *event,
                           gpointer        user_data)
{
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (NEMO_PREVIEW_VIDEO (user_data));

    priv->scrubbing = FALSE;

    return GDK_EVENT_PROPAGATE;
}

static GtkWidget *
create_poster_page (NemoPreviewVideo *video)
{
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);
    GtkWidget *overlay, *button;

    overlay = gtk_overlay_new ();

    priv->poster = nemo_preview_image_new ();
    gtk_container_add (GTK_CONTAINER (overlay), priv->poster);

    button = gtk_button_new_from_icon_name ("xsi-media-playback-start-symbolic", GTK_ICON_SIZE_DND);
    priv->poster_play_button = button;
    g_object_set (gtk_button_get_image (GTK_BUTTON (button)), "margin", POSTER_PLAY_ICON_MARGIN, NULL);
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
    gtk_widget_set_halign (button, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_widget_set_can_focus (button, FALSE);
    gtk_style_context_add_class (gtk_widget_get_style_context (button), "osd");
    g_signal_connect (button, "clicked", G_CALLBACK (play_button_clicked), video);
    gtk_overlay_add_overlay (GTK_OVERLAY (overlay), button);

    gtk_widget_show_all (overlay);

    return overlay;
}

static GtkWidget *
create_controls (NemoPreviewVideo *video)
{
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);
    GtkWidget *box;

    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_set_border_width (GTK_CONTAINER (box), 4);

    priv->play_button = gtk_button_new ();
    priv->play_image = gtk_image_new_from_icon_name ("xsi-media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (priv->play_button), priv->play_image);
    gtk_button_set_relief (GTK_BUTTON (priv->play_button), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus (priv->play_button, FALSE);
    g_signal_connect (priv->play_button, "clicked", G_CALLBACK (play_button_clicked), video);
    gtk_box_pack_start (GTK_BOX (box), priv->play_button, FALSE, FALSE, 0);

    priv->seek_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 1, 1);
    gtk_scale_set_draw_value (GTK_SCALE (priv->seek_scale), FALSE);
    gtk_widget_set_sensitive (priv->seek_scale, FALSE);
    gtk_widget_set_can_focus (priv->seek_scale, FALSE);
    g_signal_connect (priv->seek_scale, "change-value", G_CALLBACK (seek_scale_change_value), video);
    g_signal_connect (priv->seek_scale, "button-press-event", G_CALLBACK (seek_scale_button_press), video);
    g_signal_connect (priv->seek_scale, "button-release-event", G_CALLBACK (seek_scale_button_release), video);
    gtk_box_pack_start (GTK_BOX (box), priv->seek_scale, TRUE, TRUE, 0);

    priv->time_label = gtk_label_new (NULL);
    gtk_style_context_add_class (gtk_widget_get_style_context (priv->time_label), "dim-label");
    gtk_box_pack_start (GTK_BOX (box), priv->time_label, FALSE, FALSE, 0);

    priv->mute_button = gtk_toggle_button_new ();
    priv->mute_image = gtk_image_new_from_icon_name ("xsi-audio-volume-high-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (priv->mute_button), priv->mute_image);
    gtk_button_set_relief (GTK_BUTTON (priv->mute_button), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus (priv->mute_button, FALSE);
    gtk_widget_set_tooltip_text (priv->mute_button, _("Mute"));
    g_signal_connect (priv->mute_button, "toggled", G_CALLBACK (mute_button_toggled), video);
    gtk_box_pack_start (GTK_BOX (box), priv->mute_button, FALSE, FALSE, 0);

    priv->autoplay_button = gtk_toggle_button_new ();
    gtk_button_set_image (GTK_BUTTON (priv->autoplay_button),
                          gtk_image_new_from_icon_name ("xsi-media-playlist-consecutive-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_relief (GTK_BUTTON (priv->autoplay_button), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus (priv->autoplay_button, FALSE);
    gtk_widget_set_tooltip_text (priv->autoplay_button, _("Play automatically when selected"));
    sync_autoplay_button (video);
    g_signal_connect (priv->autoplay_button, "toggled", G_CALLBACK (autoplay_button_toggled), video);
    gtk_box_pack_start (GTK_BOX (box), priv->autoplay_button, FALSE, FALSE, 0);

    gtk_widget_show_all (box);

    return box;
}

static void
nemo_preview_video_init (NemoPreviewVideo *video)
{
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);

    priv->sink = gst_object_ref_sink (gst_element_factory_make ("gtksink", NULL));
    /* Leaves the letterbox area unpainted so the themed background shows instead of black */
    g_object_set (priv->sink, "ignore-alpha", FALSE, NULL);
    g_object_get (priv->sink, "widget", &priv->sink_widget, NULL);

    gtk_widget_add_events (priv->sink_widget, GDK_BUTTON_PRESS_MASK);
    g_signal_connect (priv->sink_widget, "button-press-event",
                      G_CALLBACK (sink_widget_button_press), video);

    priv->message_label = gtk_label_new (_("(Could not play video)"));
    gtk_style_context_add_class (gtk_widget_get_style_context (priv->message_label), "dim-label");

    priv->stack = gtk_stack_new ();
    gtk_stack_add_named (GTK_STACK (priv->stack), create_poster_page (video), PAGE_POSTER);
    gtk_stack_add_named (GTK_STACK (priv->stack), priv->sink_widget, PAGE_VIDEO);
    gtk_stack_add_named (GTK_STACK (priv->stack), priv->message_label, PAGE_MESSAGE);
    g_object_unref (priv->sink_widget);

    gtk_box_pack_start (GTK_BOX (video), priv->stack, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (video), create_controls (video), FALSE, FALSE, 0);

    gtk_widget_show_all (priv->stack);
    update_time_label (video, 0);

    instances = g_list_prepend (instances, video);
}

static void
nemo_preview_video_dispose (GObject *object)
{
    NemoPreviewVideo *video = NEMO_PREVIEW_VIDEO (object);
    NemoPreviewVideoPrivate *priv = nemo_preview_video_get_instance_private (video);

    instances = g_list_remove (instances, video);

    if (priv->sink != NULL) {
        stop_pipeline (video);
        g_clear_pointer (&priv->sink, gst_object_unref);
    }

    g_clear_pointer (&priv->file, nemo_file_unref);

    G_OBJECT_CLASS (nemo_preview_video_parent_class)->dispose (object);
}

static void
nemo_preview_video_class_init (NemoPreviewVideoClass *klass)
{
    G_OBJECT_CLASS (klass)->dispose = nemo_preview_video_dispose;
}

GtkWidget *
nemo_preview_video_new (void)
{
    g_return_val_if_fail (nemo_preview_video_available (), NULL);

    return g_object_new (NEMO_TYPE_PREVIEW_VIDEO,
                         "orientation", GTK_ORIENTATION_VERTICAL,
                         "spacing", 0,
                         NULL);
}

void
nemo_preview_video_set_file (NemoPreviewVideo *video,
                             NemoFile         *file)
{
    NemoPreviewVideoPrivate *priv;

    g_return_if_fail (NEMO_IS_PREVIEW_VIDEO (video));

    priv = nemo_preview_video_get_instance_private (video);

    if (priv->file == file) {
        return;
    }

    stop_pipeline (video);

    g_clear_pointer (&priv->file, nemo_file_unref);
    priv->file = file != NULL ? nemo_file_ref (file) : NULL;

    nemo_preview_image_set_file (NEMO_PREVIEW_IMAGE (priv->poster), file);
    gtk_widget_show (priv->poster_play_button);
    schedule_autoplay (video);
}

void
nemo_preview_video_set_cover (NemoPreviewVideo *video,
                              GdkPixbuf        *cover)
{
    NemoPreviewVideoPrivate *priv;

    g_return_if_fail (NEMO_IS_PREVIEW_VIDEO (video));

    priv = nemo_preview_video_get_instance_private (video);

    nemo_preview_image_set_pixbuf (NEMO_PREVIEW_IMAGE (priv->poster), cover);
}

void
nemo_preview_video_clear (NemoPreviewVideo *video)
{
    NemoPreviewVideoPrivate *priv;

    g_return_if_fail (NEMO_IS_PREVIEW_VIDEO (video));

    priv = nemo_preview_video_get_instance_private (video);

    stop_pipeline (video);
    g_clear_pointer (&priv->file, nemo_file_unref);
    nemo_preview_image_clear (NEMO_PREVIEW_IMAGE (priv->poster));
}

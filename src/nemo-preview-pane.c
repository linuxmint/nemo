/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
 * nemo-preview-pane.c - Container widget for preview pane
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
#include "nemo-preview-pane.h"
#include <libnemo-private/nemo-preview-image.h>
#include <libnemo-private/nemo-preview-details.h>
#include <libnemo-private/nemo-preview-image-info.h>
#ifdef HAVE_GSTREAMER
#include <libnemo-private/nemo-preview-video.h>
#include <libnemo-private/nemo-preview-media-info.h>
#endif
#include <libnemo-private/nemo-global-preferences.h>
#include <glib/gi18n.h>

#define PREVIEW_IMAGE_HEIGHT 200

struct _NemoPreviewPane {
    GtkBox parent;
};

typedef enum {
    PREVIEW_KIND_OTHER,
    PREVIEW_KIND_IMAGE,
    PREVIEW_KIND_MEDIA
} PreviewKind;

typedef struct {
    GtkWidget *vpaned;
    GtkWidget *top_stack;
    GtkWidget *image_widget;
    GtkWidget *video_widget;
    GtkWidget *details_widget;
    GtkWidget *empty_label;

    NemoFile *current_file;
    PreviewKind current_kind;
    time_t current_mtime;
    gulong file_changed_id;
    GCancellable *info_cancellable;

    gboolean initial_position_set;
    guint details_height_save_id;
} NemoPreviewPanePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NemoPreviewPane, nemo_preview_pane, GTK_TYPE_BOX)

static void
vpaned_size_allocate_callback (GtkWidget *widget, GtkAllocation *allocation, gpointer user_data)
{
    NemoPreviewPane *pane = NEMO_PREVIEW_PANE (user_data);
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);
    gint saved_height, position;

    if (priv->initial_position_set) {
        return;
    }

    saved_height = g_settings_get_int (nemo_preview_pane_preferences, NEMO_PREFERENCES_PREVIEW_PANE_DETAILS_HEIGHT);
    if (saved_height > 50 && allocation->height > saved_height) {
        /* Position is from top, so subtract details height from total */
        position = allocation->height - saved_height;
        gtk_paned_set_position (GTK_PANED (widget), position);
    } else {
        gtk_paned_set_position (GTK_PANED (widget), PREVIEW_IMAGE_HEIGHT);
    }

    /* Set afterwards, so a fallback position doesn't overwrite the saved height */
    priv->initial_position_set = TRUE;
}

static gboolean
save_details_height_cb (gpointer user_data)
{
    NemoPreviewPane *pane = NEMO_PREVIEW_PANE (user_data);
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);
    gint details_height;

    priv->details_height_save_id = 0;

    details_height = gtk_widget_get_allocated_height (priv->vpaned) - gtk_paned_get_position (GTK_PANED (priv->vpaned));

    if (details_height > 50 &&
        details_height != g_settings_get_int (nemo_preview_pane_preferences, NEMO_PREFERENCES_PREVIEW_PANE_DETAILS_HEIGHT)) {
        g_settings_set_int (nemo_preview_pane_preferences, NEMO_PREFERENCES_PREVIEW_PANE_DETAILS_HEIGHT, details_height);
    }

    return G_SOURCE_REMOVE;
}

static void
details_pane_position_changed_callback (GObject *paned, GParamSpec *pspec, gpointer user_data)
{
    NemoPreviewPane *pane = NEMO_PREVIEW_PANE (user_data);
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);

    if (!priv->initial_position_set) {
        return;
    }

    g_clear_handle_id (&priv->details_height_save_id, g_source_remove);
    priv->details_height_save_id = g_timeout_add (100, save_details_height_cb, pane);
}

static void
cancel_info_fetch (NemoPreviewPane *pane)
{
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);

    if (priv->info_cancellable != NULL) {
        g_cancellable_cancel (priv->info_cancellable);
        g_clear_object (&priv->info_cancellable);
    }
}

#ifdef HAVE_GSTREAMER
/* Without playback, the cover goes to the image widget instead of the video poster */
static void
show_media_info (NemoPreviewPane *pane,
                 NemoPreviewInfo *info)
{
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);
    NemoPreviewImage *image = NEMO_PREVIEW_IMAGE (priv->image_widget);

    nemo_preview_details_set_info (NEMO_PREVIEW_DETAILS (priv->details_widget), info);

    if (priv->video_widget != NULL &&
        gtk_stack_get_visible_child (GTK_STACK (priv->top_stack)) == priv->video_widget) {
        nemo_preview_video_set_info (NEMO_PREVIEW_VIDEO (priv->video_widget), info);
    } else {
        nemo_preview_image_set_cover (image, info);
    }
}

/* A cancelled fetch is for a file no longer shown, so only other failures are reported */
static void
media_info_ready (GObject      *source_object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    NemoPreviewPane *pane = NEMO_PREVIEW_PANE (user_data);
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);
    g_autoptr(GError) error = NULL;
    g_autoptr(NemoPreviewInfo) info = nemo_preview_media_info_fetch_finish (result, &error);

    if (info != NULL) {
        show_media_info (pane, info);
    } else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        nemo_preview_details_set_row (NEMO_PREVIEW_DETAILS (priv->details_widget),
                                      NEMO_PREVIEW_ROW_FORMAT,
                                      _("Unable to read media information"));
    }

    g_object_unref (pane);
}
#endif

/* The preview image decodes the file anyway, so it supplies the dimensions. Images
 * only shown through a thumbnail get no Dimensions row. */
static gchar *
get_image_dimensions (NemoPreviewPane *pane)
{
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);
    gint width, height;

    if (!nemo_preview_image_get_image_size (NEMO_PREVIEW_IMAGE (priv->image_widget), &width, &height)) {
        return NULL;
    }

    return nemo_preview_format_dimensions (width, height);
}

static void
image_size_changed (NemoPreviewImage *image,
                    gpointer          user_data)
{
    NemoPreviewPane *pane = NEMO_PREVIEW_PANE (user_data);
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);
    g_autofree gchar *dimensions = get_image_dimensions (pane);

    nemo_preview_details_set_row (NEMO_PREVIEW_DETAILS (priv->details_widget),
                                  NEMO_PREVIEW_ROW_DIMENSIONS, dimensions);
}

static void
image_info_ready (GObject      *source_object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    NemoPreviewPane *pane = NEMO_PREVIEW_PANE (user_data);
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);
    g_autoptr(NemoPreviewInfo) info = nemo_preview_image_info_fetch_finish (result, NULL);

    if (info != NULL) {
        g_free (info->rows[NEMO_PREVIEW_ROW_DIMENSIONS]);
        info->rows[NEMO_PREVIEW_ROW_DIMENSIONS] = get_image_dimensions (pane);
        nemo_preview_details_set_info (NEMO_PREVIEW_DETAILS (priv->details_widget), info);
    }

    g_object_unref (pane);
}

static PreviewKind
classify_file (NemoFile *file)
{
#ifdef HAVE_GSTREAMER
    if (nemo_preview_media_info_supported (file)) {
        return PREVIEW_KIND_MEDIA;
    }
#endif

    if (nemo_preview_image_info_supported (file)) {
        return PREVIEW_KIND_IMAGE;
    }

    return PREVIEW_KIND_OTHER;
}

static void
start_info_fetch (NemoPreviewPane *pane,
                  NemoFile        *file)
{
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);

    cancel_info_fetch (pane);

    switch (priv->current_kind) {
#ifdef HAVE_GSTREAMER
    case PREVIEW_KIND_MEDIA:
        priv->info_cancellable = g_cancellable_new ();
        nemo_preview_media_info_fetch (file, priv->info_cancellable, media_info_ready, g_object_ref (pane));
        break;
#endif
    case PREVIEW_KIND_IMAGE:
        priv->info_cancellable = g_cancellable_new ();
        nemo_preview_image_info_fetch (file, priv->info_cancellable, image_info_ready, g_object_ref (pane));
        break;
    default:
        break;
    }
}

static void
update_top_widget (NemoPreviewPane *pane,
                   NemoFile        *file)
{
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);

#ifdef HAVE_GSTREAMER
    if (priv->current_kind == PREVIEW_KIND_MEDIA && nemo_preview_video_available ()) {
        if (priv->video_widget == NULL) {
            priv->video_widget = nemo_preview_video_new ();
            gtk_stack_add_named (GTK_STACK (priv->top_stack), priv->video_widget, "video");
            gtk_widget_show_all (priv->video_widget);
        }

        nemo_preview_image_set_file (NEMO_PREVIEW_IMAGE (priv->image_widget), NULL);
        nemo_preview_video_set_file (NEMO_PREVIEW_VIDEO (priv->video_widget), file);
        gtk_stack_set_visible_child (GTK_STACK (priv->top_stack), priv->video_widget);
        return;
    }

    if (priv->video_widget != NULL) {
        nemo_preview_video_set_file (NEMO_PREVIEW_VIDEO (priv->video_widget), NULL);
    }
#endif

    nemo_preview_image_set_file (NEMO_PREVIEW_IMAGE (priv->image_widget), file);
    gtk_stack_set_visible_child (GTK_STACK (priv->top_stack), priv->image_widget);
}

/* Updated in place: show_file() ignores the current file, and re-setting it would restart playback */
static void
file_changed_callback (NemoFile *file, gpointer user_data)
{
    NemoPreviewPane *pane = NEMO_PREVIEW_PANE (user_data);
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);
    time_t mtime = nemo_file_get_mtime (file);
    gboolean content_changed = mtime != priv->current_mtime;
    PreviewKind kind = content_changed ? classify_file (file) : priv->current_kind;

    priv->current_mtime = mtime;

    nemo_preview_details_refresh (NEMO_PREVIEW_DETAILS (priv->details_widget));

    /* Rewriting the file can change its type */
    if (kind != priv->current_kind) {
        priv->current_kind = kind;
        update_top_widget (pane, file);
    }

    nemo_preview_image_refresh (NEMO_PREVIEW_IMAGE (priv->image_widget), content_changed);
#ifdef HAVE_GSTREAMER
    if (priv->video_widget != NULL) {
        nemo_preview_video_refresh (NEMO_PREVIEW_VIDEO (priv->video_widget), content_changed);
    }
#endif

    if (content_changed) {
        start_info_fetch (pane, file);
    }
}

static void
set_current_file (NemoPreviewPane *pane,
                  NemoFile        *file)
{
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);

    if (priv->current_file != NULL) {
        g_signal_handler_disconnect (priv->current_file, priv->file_changed_id);
        priv->file_changed_id = 0;
        g_clear_pointer (&priv->current_file, nemo_file_unref);
    }

    priv->current_kind = file != NULL ? classify_file (file) : PREVIEW_KIND_OTHER;

    if (file != NULL) {
        priv->current_file = nemo_file_ref (file);
        priv->current_mtime = nemo_file_get_mtime (file);
        priv->file_changed_id = g_signal_connect (file, "changed",
                                                  G_CALLBACK (file_changed_callback), pane);
    }
}

static void
nemo_preview_pane_dispose (GObject *object)
{
    NemoPreviewPane *pane = NEMO_PREVIEW_PANE (object);
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);

    /* In-flight fetches hold a ref on the pane, so finalize can't run until
     * they complete - cancel them here, before our children are destroyed. */
    cancel_info_fetch (pane);

    g_clear_handle_id (&priv->details_height_save_id, g_source_remove);
    set_current_file (pane, NULL);

    G_OBJECT_CLASS (nemo_preview_pane_parent_class)->dispose (object);
}

static void
nemo_preview_pane_init (NemoPreviewPane *pane)
{
    NemoPreviewPanePrivate *priv;
    GtkWidget *scrolled;

    priv = nemo_preview_pane_get_instance_private (pane);

    priv->empty_label = gtk_label_new (_("No file selected"));
    gtk_widget_set_halign (priv->empty_label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (priv->empty_label, GTK_ALIGN_CENTER);
    gtk_style_context_add_class (gtk_widget_get_style_context (priv->empty_label),
                                 "dim-label");
    gtk_box_pack_start (GTK_BOX (pane), priv->empty_label, TRUE, TRUE, 0);
    gtk_widget_show (priv->empty_label);

    priv->vpaned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start (GTK_BOX (pane), priv->vpaned, TRUE, TRUE, 0);

    priv->top_stack = gtk_stack_new ();
    gtk_paned_pack1 (GTK_PANED (priv->vpaned), priv->top_stack, FALSE, FALSE);

    priv->image_widget = nemo_preview_image_new ();
    g_signal_connect (priv->image_widget, "image-size-changed", G_CALLBACK (image_size_changed), pane);
    gtk_stack_add_named (GTK_STACK (priv->top_stack), priv->image_widget, "image");

    gtk_widget_show_all (priv->top_stack);

    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_AUTOMATIC);

    priv->details_widget = nemo_preview_details_new ();
    gtk_container_add (GTK_CONTAINER (scrolled), priv->details_widget);
    gtk_widget_show (priv->details_widget);

    gtk_paned_pack2 (GTK_PANED (priv->vpaned),
                     scrolled, TRUE, FALSE);
    gtk_widget_show (scrolled);

    g_signal_connect (priv->vpaned, "size-allocate",
                      G_CALLBACK (vpaned_size_allocate_callback),
                      pane);

    g_signal_connect (priv->vpaned, "notify::position",
                      G_CALLBACK (details_pane_position_changed_callback),
                      pane);
}

static void
nemo_preview_pane_class_init (NemoPreviewPaneClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = nemo_preview_pane_dispose;
}

GtkWidget *
nemo_preview_pane_new (void)
{
    return g_object_new (NEMO_TYPE_PREVIEW_PANE,
                         "orientation", GTK_ORIENTATION_VERTICAL,
                         "spacing", 0,
                         NULL);
}

static void
show_file (NemoPreviewPane *pane,
           NemoFile        *file)
{
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);

    if (file == priv->current_file) {
        return;
    }

    set_current_file (pane, file);
    update_top_widget (pane, file);
    nemo_preview_details_set_file (NEMO_PREVIEW_DETAILS (priv->details_widget), file);
    start_info_fetch (pane, file);

    gtk_widget_hide (priv->empty_label);
    gtk_widget_show (priv->vpaned);
}

static void
show_empty_state (NemoPreviewPane *pane,
                  const gchar     *message)
{
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);

    cancel_info_fetch (pane);
    set_current_file (pane, NULL);

    nemo_preview_image_set_file (NEMO_PREVIEW_IMAGE (priv->image_widget), NULL);
#ifdef HAVE_GSTREAMER
    if (priv->video_widget != NULL) {
        nemo_preview_video_set_file (NEMO_PREVIEW_VIDEO (priv->video_widget), NULL);
    }
#endif
    nemo_preview_details_set_file (NEMO_PREVIEW_DETAILS (priv->details_widget), NULL);

    gtk_widget_hide (priv->vpaned);
    gtk_label_set_text (GTK_LABEL (priv->empty_label), message);
    gtk_widget_show (priv->empty_label);
}

void
nemo_preview_pane_set_selection (NemoPreviewPane *pane,
                                 GList           *selection)
{
    g_return_if_fail (NEMO_IS_PREVIEW_PANE (pane));

    if (selection == NULL) {
        show_empty_state (pane, _("No file selected"));
    } else if (selection->next != NULL) {
        show_empty_state (pane, _("Multiple files selected"));
    } else {
        show_file (pane, NEMO_FILE (selection->data));
    }
}

gboolean
nemo_preview_pane_toggle_playback (NemoPreviewPane *pane)
{
    g_return_val_if_fail (NEMO_IS_PREVIEW_PANE (pane), FALSE);

#ifdef HAVE_GSTREAMER
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);

    if (priv->video_widget != NULL &&
        gtk_stack_get_visible_child (GTK_STACK (priv->top_stack)) == priv->video_widget) {
        return nemo_preview_video_toggle_playback (NEMO_PREVIEW_VIDEO (priv->video_widget));
    }
#endif

    return FALSE;
}

/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
 * nemo-preview-image.c - Widget for displaying image preview in preview pane
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
#include "nemo-preview-image.h"
#include "nemo-file-attributes.h"
#include "nemo-icon-info.h"
#include "nemo-thumbnails.h"
#include <glib/gi18n.h>

#define DEBUG_FLAG NEMO_DEBUG_PREVIEW_PANE
#include "nemo-debug.h"

/* Most themes top out here for mimetype icons; larger just upscales */
#define MAX_ICON_SIZE 128

#define RESIZE_DEBOUNCE_MS 150

struct _NemoPreviewImage {
    GtkBox parent;
};

typedef struct _LoadImageData LoadImageData;

typedef struct {
    GtkWidget *frame;
    GtkWidget *drawing_area;
    GtkWidget *message_label;
    NemoFile *file;

    guint resize_timeout_id;
    gint current_width;
    gint current_height;

    cairo_surface_t *current_surface;
    gint surface_width;
    gint surface_height;

    gboolean showing_icon;

    GCancellable *load_cancellable;
    GdkPixbuf *pixbuf;
    GBytes *bytes;

    gint image_width;
    gint image_height;
} NemoPreviewImagePrivate;

enum {
    IMAGE_SIZE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

struct _LoadImageData {
    GBytes *bytes;
    gchar *file_path;
    gchar *thumbnail_path;
    gint width;
    gint height;
    gint ui_scale;
    gint image_width;
    gint image_height;
};

G_DEFINE_TYPE_WITH_PRIVATE (NemoPreviewImage, nemo_preview_image, GTK_TYPE_BOX)

static void on_size_allocate (GtkWidget *widget, GtkAllocation *allocation, gpointer user_data);
static gboolean on_drawing_area_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data);

static LoadImageData *
load_image_data_new (GBytes      *bytes,
                     const gchar *file_path,
                     const gchar *thumbnail_path,
                     gint         width,
                     gint         height,
                     gint         ui_scale)
{
    LoadImageData *data;

    data = g_new0 (LoadImageData, 1);
    data->bytes = bytes != NULL ? g_bytes_ref (bytes) : NULL;
    data->file_path = g_strdup (file_path);
    data->thumbnail_path = g_strdup (thumbnail_path);
    data->width = width;
    data->height = height;
    data->ui_scale = ui_scale;

    return data;
}

static void
load_image_data_free (LoadImageData *data)
{
    if (data == NULL) {
        return;
    }

    g_clear_pointer (&data->bytes, g_bytes_unref);
    g_free (data->file_path);
    g_free (data->thumbnail_path);
    g_free (data);
}

static void
cancel_load (NemoPreviewImagePrivate *priv)
{
    if (priv->load_cancellable != NULL) {
        g_cancellable_cancel (priv->load_cancellable);
        g_clear_object (&priv->load_cancellable);
    }
}

static void
nemo_preview_image_dispose (GObject *object)
{
    NemoPreviewImagePrivate *priv = nemo_preview_image_get_instance_private (NEMO_PREVIEW_IMAGE (object));

    /* A pending load's callback holds a ref on us, so finalize can't run until
     * it completes - cancel it here, before our children are destroyed. */
    cancel_load (priv);

    g_clear_handle_id (&priv->resize_timeout_id, g_source_remove);

    G_OBJECT_CLASS (nemo_preview_image_parent_class)->dispose (object);
}

static void
nemo_preview_image_finalize (GObject *object)
{
    NemoPreviewImage *preview;
    NemoPreviewImagePrivate *priv;

    preview = NEMO_PREVIEW_IMAGE (object);
    priv = nemo_preview_image_get_instance_private (preview);

    if (priv->current_surface != NULL) {
        cairo_surface_destroy (priv->current_surface);
        priv->current_surface = NULL;
    }

    if (priv->file != NULL) {
        nemo_file_unref (priv->file);
        priv->file = NULL;
    }

    g_clear_object (&priv->pixbuf);
    g_clear_pointer (&priv->bytes, g_bytes_unref);

    G_OBJECT_CLASS (nemo_preview_image_parent_class)->finalize (object);
}

static void
nemo_preview_image_init (NemoPreviewImage *preview)
{
    NemoPreviewImagePrivate *priv;

    priv = nemo_preview_image_get_instance_private (preview);

    priv->frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (priv->frame), GTK_SHADOW_NONE);
    gtk_widget_set_halign (priv->frame, GTK_ALIGN_FILL);
    gtk_widget_set_valign (priv->frame, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand (priv->frame, TRUE);
    gtk_widget_set_vexpand (priv->frame, TRUE);

    gtk_box_pack_start (GTK_BOX (preview), priv->frame, TRUE, TRUE, 0);

    priv->drawing_area = gtk_drawing_area_new ();
    gtk_widget_set_halign (priv->drawing_area, GTK_ALIGN_FILL);
    gtk_widget_set_valign (priv->drawing_area, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand (priv->drawing_area, TRUE);
    gtk_widget_set_vexpand (priv->drawing_area, TRUE);
    g_signal_connect (priv->drawing_area, "draw",
                      G_CALLBACK (on_drawing_area_draw), preview);
    gtk_container_add (GTK_CONTAINER (priv->frame), priv->drawing_area);

    priv->message_label = gtk_label_new ("");
    gtk_widget_set_halign (priv->message_label, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (priv->message_label, GTK_ALIGN_CENTER);
    gtk_style_context_add_class (gtk_widget_get_style_context (priv->message_label),
                                 "dim-label");
    gtk_box_pack_start (GTK_BOX (preview), priv->message_label, TRUE, TRUE, 0);

    gtk_container_set_border_width (GTK_CONTAINER (preview), 4);
    gtk_widget_show_all (GTK_WIDGET (preview));
    g_signal_connect (preview, "size-allocate",
                      G_CALLBACK (on_size_allocate), NULL);
}

static void
nemo_preview_image_class_init (NemoPreviewImageClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = nemo_preview_image_dispose;
    object_class->finalize = nemo_preview_image_finalize;

    signals[IMAGE_SIZE_CHANGED] = g_signal_new ("image-size-changed",
                                                G_TYPE_FROM_CLASS (klass),
                                                G_SIGNAL_RUN_LAST,
                                                0, NULL, NULL, NULL,
                                                G_TYPE_NONE, 0);
}

GtkWidget *
nemo_preview_image_new (void)
{
    return g_object_new (NEMO_TYPE_PREVIEW_IMAGE,
                         "orientation", GTK_ORIENTATION_VERTICAL,
                         "spacing", 0,
                         NULL);
}

static gboolean
on_drawing_area_draw (GtkWidget *widget,
                      cairo_t   *cr,
                      gpointer   user_data)
{
    NemoPreviewImage *preview = NEMO_PREVIEW_IMAGE (user_data);
    NemoPreviewImagePrivate *priv;
    gint widget_width, widget_height;
    gdouble scale_x, scale_y, scale;
    gdouble scaled_width, scaled_height;
    gdouble x_offset, y_offset;

    priv = nemo_preview_image_get_instance_private (preview);

    if (priv->current_surface == NULL || priv->surface_width <= 0 || priv->surface_height <= 0) {
        return FALSE;
    }

    widget_width = gtk_widget_get_allocated_width (widget);
    widget_height = gtk_widget_get_allocated_height (widget);

    scale_x = (gdouble)widget_width / priv->surface_width;
    scale_y = (gdouble)widget_height / priv->surface_height;
    scale = MIN (scale_x, scale_y);

    if (priv->showing_icon) {
        scale = MIN (scale, 1.0);
    }

    scaled_width = priv->surface_width * scale;
    scaled_height = priv->surface_height * scale;

    x_offset = (widget_width - scaled_width) / 2.0;
    y_offset = (widget_height - scaled_height) / 2.0;

    cairo_save (cr);
    cairo_translate (cr, x_offset, y_offset);
    cairo_scale (cr, scale, scale);
    cairo_set_source_surface (cr, priv->current_surface, 0, 0);
    cairo_paint (cr);
    cairo_restore (cr);

    if (!priv->showing_icon) {
        GtkStyleContext *style_context;
        GdkRGBA border_color;
        gdouble border_width = 1.0;
        gdouble inset = 0.5;

        style_context = gtk_widget_get_style_context (priv->frame);
        gtk_style_context_get_border_color (style_context, GTK_STATE_FLAG_NORMAL, &border_color);

        cairo_set_source_rgba (cr, border_color.red, border_color.green,
                               border_color.blue, border_color.alpha);
        cairo_set_line_width (cr, border_width);
        cairo_rectangle (cr, x_offset + inset, y_offset + inset,
                         scaled_width - (inset * 2), scaled_height - (inset * 2));
        cairo_stroke (cr);
    }

    return TRUE;
}

/* Thread-safe alternative to gdk_cairo_surface_create_from_pixbuf(), which can
 * touch the X display. The device scale must be applied after painting, or the
 * pixbuf gets scaled up and only its top-left portion lands on the surface. */
static cairo_surface_t *
create_surface_from_pixbuf (GdkPixbuf *pixbuf,
                            gint       scale_factor)
{
    cairo_surface_t *surface;
    cairo_t *cr;

    surface = cairo_image_surface_create (gdk_pixbuf_get_has_alpha (pixbuf) ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24,
                                          gdk_pixbuf_get_width (pixbuf),
                                          gdk_pixbuf_get_height (pixbuf));

    cr = cairo_create (surface);
    gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    cairo_destroy (cr);

    cairo_surface_set_device_scale (surface, scale_factor, scale_factor);

    return surface;
}

/* Takes ownership of the surface */
static void
show_surface (NemoPreviewImage *widget,
              cairo_surface_t  *surface,
              gint              ui_scale,
              gboolean          is_icon)
{
    NemoPreviewImagePrivate *priv = nemo_preview_image_get_instance_private (widget);

    g_clear_pointer (&priv->current_surface, cairo_surface_destroy);

    priv->current_surface = surface;
    priv->showing_icon = is_icon;
    priv->surface_width = cairo_image_surface_get_width (surface) / ui_scale;
    priv->surface_height = cairo_image_surface_get_height (surface) / ui_scale;

    gtk_widget_hide (priv->message_label);
    gtk_widget_show (priv->drawing_area);
    gtk_widget_queue_draw (priv->drawing_area);
}

static void
show_message (NemoPreviewImage *widget,
              const gchar      *message)
{
    NemoPreviewImagePrivate *priv = nemo_preview_image_get_instance_private (widget);

    gtk_label_set_text (GTK_LABEL (priv->message_label), message);
    gtk_widget_show (priv->message_label);
    gtk_widget_hide (priv->drawing_area);
}

/* The embedded orientation is only known once decoding is done, so pick a size
 * that fills the area whether or not the image gets rotated a quarter turn. */
static void
loader_size_prepared (GdkPixbufLoader *loader,
                      gint             width,
                      gint             height,
                      gpointer         user_data)
{
    LoadImageData *data = user_data;
    gdouble area_width = data->width * data->ui_scale;
    gdouble area_height = data->height * data->ui_scale;
    gdouble scale;

    data->image_width = width;
    data->image_height = height;

    scale = MAX (MIN (area_width / width, area_height / height),
                 MIN (area_width / height, area_height / width));

    gdk_pixbuf_loader_set_size (loader, MAX (1, width * scale), MAX (1, height * scale));
}

static GdkPixbuf *
load_pixbuf_from_stream (GInputStream  *stream,
                         LoadImageData *data,
                         GCancellable  *cancellable,
                         GError       **error)
{
    GdkPixbufLoader *loader;
    GdkPixbuf *pixbuf = NULL;
    guchar buffer[65536];
    gssize n_read;
    gboolean ok = TRUE;

    loader = gdk_pixbuf_loader_new ();
    g_signal_connect (loader, "size-prepared", G_CALLBACK (loader_size_prepared), data);

    while (ok && (n_read = g_input_stream_read (stream, buffer, sizeof (buffer), cancellable, error)) > 0) {
        ok = gdk_pixbuf_loader_write (loader, buffer, n_read, error);
    }

    ok = ok && n_read == 0;

    /* The loader must always be closed, but its error only matters if nothing failed before it */
    if (gdk_pixbuf_loader_close (loader, ok ? error : NULL) && ok) {
        pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

        if (pixbuf != NULL) {
            g_object_ref (pixbuf);
        }
    }

    g_object_unref (loader);

    return pixbuf;
}

static GdkPixbuf *
load_pixbuf_at_size (const gchar   *path,
                     LoadImageData *data,
                     GCancellable  *cancellable,
                     GError       **error)
{
    GFileInputStream *stream;
    GFile *file;
    GdkPixbuf *pixbuf;

    file = g_file_new_for_path (path);
    stream = g_file_read (file, cancellable, error);
    g_object_unref (file);

    if (stream == NULL) {
        return NULL;
    }

    pixbuf = load_pixbuf_from_stream (G_INPUT_STREAM (stream), data, cancellable, error);
    g_object_unref (stream);

    return pixbuf;
}

static GdkPixbuf *
load_pixbuf_from_bytes (GBytes        *bytes,
                        LoadImageData *data,
                        GCancellable  *cancellable,
                        GError       **error)
{
    GInputStream *stream;
    GdkPixbuf *pixbuf;

    stream = g_memory_input_stream_new_from_bytes (bytes);
    pixbuf = load_pixbuf_from_stream (stream, data, cancellable, error);
    g_object_unref (stream);

    return pixbuf;
}

static void
log_load_error (const gchar  *source,
                GError      **error)
{
    if (*error != NULL && !g_error_matches (*error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        DEBUG ("Unable to load '%s': %s", source, (*error)->message);
    }

    g_clear_error (error);
}

static void
load_image_thread (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
    LoadImageData *data = task_data;
    GdkPixbuf *pixbuf = NULL;
    GdkPixbuf *oriented_pixbuf = NULL;
    cairo_surface_t *surface = NULL;
    GError *error = NULL;
    const gchar *orientation;

    if (data->bytes != NULL) {
        pixbuf = load_pixbuf_from_bytes (data->bytes, data, cancellable, &error);
        data->image_width = data->image_height = 0;
        log_load_error ("embedded image", &error);
    }

    if (pixbuf == NULL && data->file_path != NULL && !g_cancellable_is_cancelled (cancellable)) {
        pixbuf = load_pixbuf_at_size (data->file_path, data, cancellable, &error);
        log_load_error (data->file_path, &error);
    }

    if (g_cancellable_is_cancelled (cancellable)) {
        if (pixbuf != NULL) {
            g_object_unref (pixbuf);
        }
        g_task_return_error_if_cancelled (task);
        return;
    }

    if (pixbuf == NULL && data->thumbnail_path != NULL) {
        pixbuf = load_pixbuf_at_size (data->thumbnail_path, data, cancellable, &error);
        data->image_width = data->image_height = 0;
        log_load_error (data->thumbnail_path, &error);
    }

    if (pixbuf != NULL && !g_cancellable_is_cancelled (cancellable)) {
        orientation = gdk_pixbuf_get_option (pixbuf, "orientation");

        /* Orientations 5-8 turn the image a quarter, so report the size as displayed */
        if (orientation != NULL && g_ascii_strtoll (orientation, NULL, 10) >= 5) {
            gint image_width = data->image_width;

            data->image_width = data->image_height;
            data->image_height = image_width;
        }

        oriented_pixbuf = gdk_pixbuf_apply_embedded_orientation (pixbuf);
        surface = create_surface_from_pixbuf (oriented_pixbuf, data->ui_scale);
        g_object_unref (oriented_pixbuf);
    }

    g_clear_object (&pixbuf);

    if (g_task_return_error_if_cancelled (task)) {
        g_clear_pointer (&surface, cairo_surface_destroy);
        return;
    }

    g_task_return_pointer (task, surface, surface ? (GDestroyNotify) cairo_surface_destroy : NULL);
}

static void load_icon_at_size (NemoPreviewImage *widget, gint width, gint height);

static void
set_image_size (NemoPreviewImage *widget,
                gint              width,
                gint              height)
{
    NemoPreviewImagePrivate *priv = nemo_preview_image_get_instance_private (widget);

    if (width == priv->image_width && height == priv->image_height) {
        return;
    }

    priv->image_width = width;
    priv->image_height = height;

    g_signal_emit (widget, signals[IMAGE_SIZE_CHANGED], 0);
}

static void
load_image_callback (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
    g_autoptr(NemoPreviewImage) widget = NEMO_PREVIEW_IMAGE (user_data);
    NemoPreviewImagePrivate *priv = nemo_preview_image_get_instance_private (widget);
    GTask *task = G_TASK (result);
    LoadImageData *data = g_task_get_task_data (task);
    cairo_surface_t *surface;

    /* Starting another load cancels this one, so an uncancelled task is the current load */
    if (g_cancellable_is_cancelled (g_task_get_cancellable (task))) {
        return;
    }

    g_clear_object (&priv->load_cancellable);

    surface = g_task_propagate_pointer (task, NULL);

    if (surface != NULL) {
        show_surface (widget, surface, data->ui_scale, FALSE);
        set_image_size (widget, data->image_width, data->image_height);
        return;
    }

    /* The file was an image gdk-pixbuf should handle, so tell it apart from an unpreviewable type */
    if (data->file_path != NULL) {
        show_message (widget, _("(Unable to load image)"));
    } else {
        load_icon_at_size (widget, data->width, data->height);
    }

    set_image_size (widget, 0, 0);
}

static void
load_icon_at_size (NemoPreviewImage *widget,
                   gint              width,
                   gint              height)
{
    NemoPreviewImagePrivate *priv;
    NemoIconInfo *icon_info = NULL;
    GdkPixbuf *icon_pixbuf = NULL;
    gint ui_scale;
    gint icon_size;
    GError *error = NULL;

    priv = nemo_preview_image_get_instance_private (widget);

    if (priv->file == NULL) {
        return;
    }

    if (width <= 1 || height <= 1) {
        return;
    }

    cancel_load (priv);
    ui_scale = gtk_widget_get_scale_factor (GTK_WIDGET (widget));

    icon_size = MIN (MIN (width, height), MAX_ICON_SIZE);
    icon_info = nemo_file_get_icon (priv->file, icon_size, 0, ui_scale, 0);

    if (icon_info != NULL) {
        if (icon_info->icon_name != NULL) {
            icon_pixbuf = gtk_icon_theme_load_icon_for_scale (gtk_icon_theme_get_default (),
                                                              icon_info->icon_name,
                                                              icon_size,
                                                              ui_scale,
                                                              GTK_ICON_LOOKUP_FORCE_SIZE,
                                                              &error);

            if (error != NULL) {
                g_warning ("Failed to load icon '%s': %s", icon_info->icon_name, error->message);
                g_error_free (error);
            }
        } else {
            /* Custom and other loadable icons come with a pixbuf but no name */
            icon_pixbuf = nemo_icon_info_get_pixbuf_nodefault_at_size (icon_info, icon_size * ui_scale);
        }

        nemo_icon_info_unref (icon_info);
    }

    if (icon_pixbuf != NULL) {
        show_surface (widget, gdk_cairo_surface_create_from_pixbuf (icon_pixbuf, ui_scale, NULL), ui_scale, TRUE);
        g_object_unref (icon_pixbuf);
    } else {
        show_message (widget, _("(No icon available)"));
    }
}

static void
load_image_at_size (NemoPreviewImage *widget,
                    gint              width,
                    gint              height)
{
    NemoPreviewImagePrivate *priv;
    GTask *task;
    LoadImageData *data;
    gchar *file_path = NULL;
    gchar *thumbnail_path = NULL;
    gint ui_scale;

    priv = nemo_preview_image_get_instance_private (widget);

    /* Even when nothing new is started, a pending load must not land afterwards */
    cancel_load (priv);

    if (priv->file == NULL) {
        return;
    }

    if (width <= 1 || height <= 1) {
        return;
    }

    if (nemo_can_thumbnail_internally (priv->file)) {
        file_path = nemo_file_get_activation_path (priv->file);
    }

    if (nemo_file_has_loaded_thumbnail (priv->file)) {
        thumbnail_path = nemo_file_get_thumbnail_path (priv->file);
    }

    if (priv->bytes == NULL && file_path == NULL && thumbnail_path == NULL) {
        load_icon_at_size (widget, width, height);
        return;
    }

    ui_scale = gtk_widget_get_scale_factor (GTK_WIDGET (widget));

    data = load_image_data_new (priv->bytes, file_path, thumbnail_path, width, height, ui_scale);
    g_free (file_path);
    g_free (thumbnail_path);

    priv->load_cancellable = g_cancellable_new ();

    /* Not the source object: the task may be finalized on the worker thread, which
     * would then finalize the widget off the main thread */
    task = g_task_new (NULL, priv->load_cancellable, load_image_callback, g_object_ref (widget));
    g_task_set_task_data (task, data, (GDestroyNotify) load_image_data_free);
    g_task_run_in_thread (task, load_image_thread);
    g_object_unref (task);
}

static void
show_pixbuf (NemoPreviewImage *widget)
{
    NemoPreviewImagePrivate *priv = nemo_preview_image_get_instance_private (widget);
    gint ui_scale = gtk_widget_get_scale_factor (GTK_WIDGET (widget));

    show_surface (widget, gdk_cairo_surface_create_from_pixbuf (priv->pixbuf, ui_scale, NULL), ui_scale, FALSE);
}

static void
reload_at_size (NemoPreviewImage *widget,
                gint              width,
                gint              height)
{
    NemoPreviewImagePrivate *priv;

    priv = nemo_preview_image_get_instance_private (widget);

    priv->current_width = width;
    priv->current_height = height;

    /* The draw handler scales the surface, so a set pixbuf never needs reconverting */
    if (priv->pixbuf != NULL) {
        if (priv->current_surface == NULL) {
            show_pixbuf (widget);
        }

        return;
    }

    load_image_at_size (widget, width, height);
}

static gboolean
on_resize_timeout (gpointer user_data)
{
    NemoPreviewImage *widget = NEMO_PREVIEW_IMAGE (user_data);
    NemoPreviewImagePrivate *priv;
    GtkAllocation allocation;

    priv = nemo_preview_image_get_instance_private (widget);
    priv->resize_timeout_id = 0;

    gtk_widget_get_allocation (GTK_WIDGET (widget), &allocation);
    reload_at_size (widget, allocation.width, allocation.height);

    return G_SOURCE_REMOVE;
}

static void
on_size_allocate (GtkWidget     *widget,
                  GtkAllocation *allocation,
                  gpointer       user_data)
{
    NemoPreviewImage *preview = NEMO_PREVIEW_IMAGE (widget);
    NemoPreviewImagePrivate *priv;

    priv = nemo_preview_image_get_instance_private (preview);

    if (priv->current_surface != NULL) {
        gtk_widget_queue_draw (priv->drawing_area);
    }

    g_clear_handle_id (&priv->resize_timeout_id, g_source_remove);

    /* Showing or hiding children re-allocates without a real size change */
    if (allocation->width == priv->current_width && allocation->height == priv->current_height) {
        return;
    }

    priv->resize_timeout_id = g_timeout_add (RESIZE_DEBOUNCE_MS,
                                             on_resize_timeout,
                                             preview);
}

void
nemo_preview_image_set_file (NemoPreviewImage *widget,
                              NemoFile         *file)
{
    NemoPreviewImagePrivate *priv;
    GtkAllocation allocation;

    g_return_if_fail (NEMO_IS_PREVIEW_IMAGE (widget));

    priv = nemo_preview_image_get_instance_private (widget);

    if (priv->file == file) {
        return;
    }

    cancel_load (priv);
    g_clear_handle_id (&priv->resize_timeout_id, g_source_remove);
    g_clear_pointer (&priv->file, nemo_file_unref);
    g_clear_object (&priv->pixbuf);
    g_clear_pointer (&priv->bytes, g_bytes_unref);
    g_clear_pointer (&priv->current_surface, cairo_surface_destroy);

    gtk_widget_hide (priv->message_label);
    gtk_widget_hide (priv->drawing_area);
    priv->current_width = 0;
    priv->current_height = 0;
    priv->surface_width = 0;
    priv->surface_height = 0;
    priv->showing_icon = FALSE;
    priv->image_width = 0;
    priv->image_height = 0;

    if (file != NULL) {
        priv->file = nemo_file_ref (file);
        gtk_widget_get_allocation (GTK_WIDGET (widget), &allocation);
        reload_at_size (widget, allocation.width, allocation.height);
    }
}

/* Thumbnails and icons are cheap to reload, so they always are - that picks
 * up a thumbnail that finished after the icon was shown. A full image or a
 * set pixbuf or bytes are only replaced when the file content changed. */
void
nemo_preview_image_refresh (NemoPreviewImage *widget,
                            gboolean          content_changed)
{
    NemoPreviewImagePrivate *priv;
    GtkAllocation allocation;

    g_return_if_fail (NEMO_IS_PREVIEW_IMAGE (widget));

    priv = nemo_preview_image_get_instance_private (widget);

    if (priv->file == NULL) {
        return;
    }

    if (content_changed) {
        g_clear_object (&priv->pixbuf);
        g_clear_pointer (&priv->bytes, g_bytes_unref);
    } else if (priv->pixbuf != NULL || priv->bytes != NULL || nemo_can_thumbnail_internally (priv->file)) {
        return;
    }

    gtk_widget_get_allocation (GTK_WIDGET (widget), &allocation);
    reload_at_size (widget, allocation.width, allocation.height);
}

void
nemo_preview_image_set_pixbuf (NemoPreviewImage *widget,
                               GdkPixbuf        *pixbuf)
{
    NemoPreviewImagePrivate *priv;

    g_return_if_fail (NEMO_IS_PREVIEW_IMAGE (widget));
    g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

    priv = nemo_preview_image_get_instance_private (widget);

    cancel_load (priv);
    g_clear_pointer (&priv->bytes, g_bytes_unref);
    g_set_object (&priv->pixbuf, pixbuf);
    show_pixbuf (widget);
}

/* Encoded image data, decoded at the widget's size like a file */
void
nemo_preview_image_set_bytes (NemoPreviewImage *widget,
                              GBytes           *bytes)
{
    NemoPreviewImagePrivate *priv;

    g_return_if_fail (NEMO_IS_PREVIEW_IMAGE (widget));
    g_return_if_fail (bytes != NULL);

    priv = nemo_preview_image_get_instance_private (widget);

    g_clear_object (&priv->pixbuf);
    g_clear_pointer (&priv->bytes, g_bytes_unref);
    priv->bytes = g_bytes_ref (bytes);

    load_image_at_size (widget, priv->current_width, priv->current_height);
}

void
nemo_preview_image_set_cover (NemoPreviewImage      *widget,
                              const NemoPreviewInfo *info)
{
    g_return_if_fail (NEMO_IS_PREVIEW_IMAGE (widget));
    g_return_if_fail (info != NULL);

    if (info->cover != NULL) {
        nemo_preview_image_set_pixbuf (widget, info->cover);
    } else if (info->cover_bytes != NULL) {
        nemo_preview_image_set_bytes (widget, info->cover_bytes);
    }
}

/* Pixel size after EXIF orientation; only known when the file itself (not a
 * thumbnail, embedded art or icon) was decoded */
gboolean
nemo_preview_image_get_image_size (NemoPreviewImage *widget,
                                   gint             *width,
                                   gint             *height)
{
    NemoPreviewImagePrivate *priv;

    g_return_val_if_fail (NEMO_IS_PREVIEW_IMAGE (widget), FALSE);

    priv = nemo_preview_image_get_instance_private (widget);

    *width = priv->image_width;
    *height = priv->image_height;

    return priv->image_width > 0 && priv->image_height > 0;
}

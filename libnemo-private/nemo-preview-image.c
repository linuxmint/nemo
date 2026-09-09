/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-preview-image.c - Widget for displaying image preview in preview pane
 *
 * Copyright (C) 2025 Linux Mint
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

#include "nemo-preview-image.h"
#include "nemo-file-attributes.h"
#include "nemo-icon-info.h"
#include "nemo-thumbnails.h"
#include <glib/gi18n.h>

#define RESIZE_DEBOUNCE_MS 150
#define MIN_SIZE_CHANGE 10

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

	LoadImageData *current_load_data;
} NemoPreviewImagePrivate;

struct _LoadImageData {
	gchar *file_path;        /* For direct loading (can be NULL) */
	gchar *thumbnail_path;   /* For thumbnail loading (can be NULL) */
	gint width;
	gint height;
	gint ui_scale;
	GCancellable *cancellable;
};

G_DEFINE_TYPE_WITH_PRIVATE (NemoPreviewImage, nemo_preview_image, GTK_TYPE_BOX)

static void on_size_allocate (GtkWidget *widget, GtkAllocation *allocation, gpointer user_data);
static gboolean on_drawing_area_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data);

static LoadImageData *
load_image_data_new (const gchar *file_path,
                     const gchar *thumbnail_path,
                     gint         width,
                     gint         height,
                     gint         ui_scale)
{
	LoadImageData *data;

	data = g_new0 (LoadImageData, 1);
	data->file_path = g_strdup (file_path);
	data->thumbnail_path = g_strdup (thumbnail_path);
	data->width = width;
	data->height = height;
	data->ui_scale = ui_scale;
	data->cancellable = g_cancellable_new ();

	return data;
}

static void
load_image_data_free (LoadImageData *data)
{
	if (data == NULL) {
		return;
	}

	g_free (data->file_path);
	g_free (data->thumbnail_path);

	if (data->cancellable != NULL) {
		g_object_unref (data->cancellable);
	}

	g_free (data);
}

static void
nemo_preview_image_finalize (GObject *object)
{
	NemoPreviewImage *preview;
	NemoPreviewImagePrivate *priv;

	preview = NEMO_PREVIEW_IMAGE (object);
	priv = nemo_preview_image_get_instance_private (preview);

	if (priv->current_load_data != NULL) {
		g_cancellable_cancel (priv->current_load_data->cancellable);
		priv->current_load_data = NULL;  // Forget about it, GTask will clean it up
	}

	if (priv->resize_timeout_id != 0) {
		g_source_remove (priv->resize_timeout_id);
		priv->resize_timeout_id = 0;
	}

	if (priv->current_surface != NULL) {
		cairo_surface_destroy (priv->current_surface);
		priv->current_surface = NULL;
	}

	if (priv->file != NULL) {
		nemo_file_unref (priv->file);
		priv->file = NULL;
	}

	G_OBJECT_CLASS (nemo_preview_image_parent_class)->finalize (object);
}

static void
nemo_preview_image_init (NemoPreviewImage *preview)
{
	NemoPreviewImagePrivate *priv;

	priv = nemo_preview_image_get_instance_private (preview);

	priv->resize_timeout_id = 0;
	priv->current_width = 0;
	priv->current_height = 0;
	priv->current_surface = NULL;
	priv->surface_width = 0;
	priv->surface_height = 0;
	priv->showing_icon = FALSE;
	priv->current_load_data = NULL;

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
	object_class->finalize = nemo_preview_image_finalize;
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

	if (priv->current_surface == NULL) {
		return FALSE;
	}

	widget_width = gtk_widget_get_allocated_width (widget);
	widget_height = gtk_widget_get_allocated_height (widget);

	scale_x = (gdouble)widget_width / priv->surface_width;
	scale_y = (gdouble)widget_height / priv->surface_height;
	scale = MIN (scale_x, scale_y);

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

	/* If showing an image (not an icon), draw a border on top of it */
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

static cairo_surface_t *
create_surface_from_pixbuf (GdkPixbuf *pixbuf,
                             gint       scale_factor)
{
    cairo_surface_t *surface;
    cairo_t *cr;
    gint width, height;

    g_return_val_if_fail (pixbuf != NULL, NULL);

    width = gdk_pixbuf_get_width (pixbuf);
    height = gdk_pixbuf_get_height (pixbuf);

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    cairo_surface_set_device_scale (surface, scale_factor, scale_factor);

    cr = cairo_create (surface);
    gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
    cairo_paint (cr);
    cairo_destroy (cr);

    return surface;
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

	if (data->file_path != NULL) {
		pixbuf = gdk_pixbuf_new_from_file_at_scale (data->file_path,
		                                            data->width * data->ui_scale,
		                                            data->height * data->ui_scale,
		                                            TRUE,
		                                            &error);
		if (error != NULL) {
			g_clear_error (&error);
		}
	}

	if (g_cancellable_is_cancelled (cancellable)) {
		if (pixbuf != NULL) {
			g_object_unref (pixbuf);
		}
		g_task_return_error_if_cancelled (task);
		return;
	}

	if (pixbuf == NULL && data->thumbnail_path != NULL) {
		pixbuf = gdk_pixbuf_new_from_file_at_scale (data->thumbnail_path,
		                                            data->width * data->ui_scale,
		                                            data->height * data->ui_scale,
		                                            TRUE,
		                                            &error);
		if (error != NULL) {
			g_clear_error (&error);
		}
	}

	if (pixbuf != NULL && !g_cancellable_is_cancelled (cancellable)) {
		oriented_pixbuf = gdk_pixbuf_apply_embedded_orientation (pixbuf);
		g_object_unref (pixbuf);
		pixbuf = oriented_pixbuf;
	}

	if (pixbuf != NULL && !g_cancellable_is_cancelled (cancellable)) {
		surface = create_surface_from_pixbuf (pixbuf, data->ui_scale);
		g_object_unref (pixbuf);
	}

	if (!g_cancellable_is_cancelled (cancellable)) {
		g_task_return_pointer (task, surface, surface ? (GDestroyNotify)cairo_surface_destroy : NULL);
	} else {
		if (surface != NULL) {
			cairo_surface_destroy (surface);
		}
		g_task_return_error_if_cancelled (task);
	}
}

static void
load_image_callback (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
	NemoPreviewImage *widget = NEMO_PREVIEW_IMAGE (source_object);
	NemoPreviewImagePrivate *priv;
	GTask *task;
	LoadImageData *data;
	cairo_surface_t *surface;
	gint scale_factor;
	GError *error = NULL;

	priv = nemo_preview_image_get_instance_private (widget);
	task = G_TASK (result);
	data = g_task_get_task_data (task);

	if (priv->current_load_data == data) {
		priv->current_load_data = NULL;
	}

    if (g_cancellable_is_cancelled (data->cancellable))
        return;

    if (error != NULL) {
        g_error_free (error);
        return;
    }

	surface = g_task_propagate_pointer (task, &error);

	if (surface != NULL) {
		if (priv->current_surface != NULL) {
			cairo_surface_destroy (priv->current_surface);
		}
		priv->current_surface = surface;

		scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (widget));
		priv->surface_width = cairo_image_surface_get_width (surface) / scale_factor;
		priv->surface_height = cairo_image_surface_get_height (surface) / scale_factor;

		gtk_widget_show (priv->drawing_area);
		gtk_widget_queue_draw (priv->drawing_area);
		gtk_widget_hide (priv->message_label);
	} else {
		gtk_label_set_text (GTK_LABEL (priv->message_label),
		                    _("(Failed to load image)"));
		gtk_widget_show (priv->message_label);
		gtk_widget_hide (priv->drawing_area);
	}
}

static void
load_icon_at_size (NemoPreviewImage *widget,
                   gint              width,
                   gint              height)
{
	NemoPreviewImagePrivate *priv;
	NemoIconInfo *icon_info = NULL;
	GtkIconTheme *icon_theme;
	GdkPixbuf *icon_pixbuf = NULL;
	cairo_surface_t *surface = NULL;
	gint ui_scale;
	gint icon_size;
	const char *icon_name;
	GError *error = NULL;

	priv = nemo_preview_image_get_instance_private (widget);

	if (priv->file == NULL) {
		return;
	}

	if (width <= 1 || height <= 1) {
		return;
	}

	ui_scale = gtk_widget_get_scale_factor (GTK_WIDGET (widget));

	icon_size = MIN (width, height) * ui_scale;
	icon_info = nemo_file_get_icon (priv->file, icon_size, 0, ui_scale, 0);

	if (icon_info != NULL && icon_info->icon_name != NULL) {
		icon_name = icon_info->icon_name;
		icon_theme = gtk_icon_theme_get_default ();

		icon_pixbuf = gtk_icon_theme_load_icon_for_scale (icon_theme,
		                                                   icon_name,
		                                                   icon_size / ui_scale,
		                                                   ui_scale,
		                                                   GTK_ICON_LOOKUP_FORCE_SIZE,
		                                                   &error);

		if (icon_pixbuf != NULL) {
			surface = create_surface_from_pixbuf (icon_pixbuf, ui_scale);

			if (surface != NULL) {
				if (priv->current_surface != NULL) {
					cairo_surface_destroy (priv->current_surface);
				}
				priv->current_surface = surface;

				priv->surface_width = cairo_image_surface_get_width (surface) / ui_scale;
				priv->surface_height = cairo_image_surface_get_height (surface) / ui_scale;

				gtk_widget_show (priv->drawing_area);
				gtk_widget_queue_draw (priv->drawing_area);
			}

			g_object_unref (icon_pixbuf);
			gtk_widget_hide (priv->message_label);
		} else {
			if (error != NULL) {
				g_warning ("Failed to load icon '%s': %s", icon_name, error->message);
				g_error_free (error);
			}
			gtk_label_set_text (GTK_LABEL (priv->message_label),
			                    _("(Failed to load icon)"));
			gtk_widget_show (priv->message_label);
			gtk_widget_hide (priv->drawing_area);
		}

		nemo_icon_info_unref (icon_info);
	} else {
		if (icon_info != NULL) {
			nemo_icon_info_unref (icon_info);
		}
		gtk_label_set_text (GTK_LABEL (priv->message_label),
		                    _("(No icon available)"));
		gtk_widget_show (priv->message_label);
		gtk_widget_hide (priv->drawing_area);
	}

	priv->current_width = width;
	priv->current_height = height;
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

	if (priv->file == NULL) {
		return;
	}

	if (width <= 1 || height <= 1) {
		return;
	}

	if (priv->current_load_data != NULL) {
		g_cancellable_cancel (priv->current_load_data->cancellable);
		priv->current_load_data = NULL;  /* Forget about it, callback will clean up */
	}

	if (nemo_can_thumbnail_internally (priv->file)) {
		file_path = nemo_file_get_path (priv->file);
	}

	if (nemo_file_has_loaded_thumbnail (priv->file)) {
		thumbnail_path = nemo_file_get_thumbnail_path (priv->file);
	}

	if (file_path == NULL && thumbnail_path == NULL) {
		gtk_label_set_text (GTK_LABEL (priv->message_label),
		                    _("(No preview available)"));
		gtk_widget_show (priv->message_label);
		gtk_widget_hide (priv->drawing_area);
		return;
	}

	ui_scale = gtk_widget_get_scale_factor (GTK_WIDGET (widget));

	data = load_image_data_new (file_path, thumbnail_path, width, height, ui_scale);
	g_free (file_path);
	g_free (thumbnail_path);

	priv->current_load_data = data;

	task = g_task_new (widget, data->cancellable, load_image_callback, NULL);
	g_task_set_task_data (task, data, (GDestroyNotify) load_image_data_free);
	g_task_run_in_thread (task, load_image_thread);
	g_object_unref (task);

	priv->current_width = width;
	priv->current_height = height;
}

static void
reload_at_size (NemoPreviewImage *widget,
                gint              width,
                gint              height)
{
	NemoPreviewImagePrivate *priv;

	priv = nemo_preview_image_get_instance_private (widget);

	if (priv->file == NULL) {
		return;
	}

	if (nemo_can_thumbnail_internally (priv->file) || nemo_file_has_loaded_thumbnail (priv->file)) {
		priv->showing_icon = FALSE;
		load_image_at_size (widget, width, height);
	} else {
		priv->showing_icon = TRUE;
		load_icon_at_size (widget, width, height);
	}
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

	if (priv->resize_timeout_id != 0) {
		g_source_remove (priv->resize_timeout_id);
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

	if (priv->current_load_data != NULL) {
		g_cancellable_cancel (priv->current_load_data->cancellable);
		priv->current_load_data = NULL;
	}

	if (priv->resize_timeout_id != 0) {
		g_source_remove (priv->resize_timeout_id);
		priv->resize_timeout_id = 0;
	}

	if (priv->file != NULL) {
		nemo_file_unref (priv->file);
	}

	priv->file = file;

	if (priv->current_surface != NULL) {
		cairo_surface_destroy (priv->current_surface);
		priv->current_surface = NULL;
	}
	gtk_widget_hide (priv->message_label);
	gtk_widget_hide (priv->drawing_area);
	priv->current_width = 0;
	priv->current_height = 0;
	priv->surface_width = 0;
	priv->surface_height = 0;

	if (file != NULL) {
		nemo_file_ref (file);
		gtk_widget_get_allocation (GTK_WIDGET (widget), &allocation);
		reload_at_size (widget, allocation.width, allocation.height);
	}
}

void
nemo_preview_image_clear (NemoPreviewImage *widget)
{
	NemoPreviewImagePrivate *priv;

	g_return_if_fail (NEMO_IS_PREVIEW_IMAGE (widget));

	priv = nemo_preview_image_get_instance_private (widget);

	if (priv->current_load_data != NULL) {
		g_cancellable_cancel (priv->current_load_data->cancellable);
		priv->current_load_data = NULL;
	}

	if (priv->resize_timeout_id != 0) {
		g_source_remove (priv->resize_timeout_id);
		priv->resize_timeout_id = 0;
	}

	if (priv->current_surface != NULL) {
		cairo_surface_destroy (priv->current_surface);
		priv->current_surface = NULL;
	}

	if (priv->file != NULL) {
		nemo_file_unref (priv->file);
		priv->file = NULL;
	}

	gtk_widget_hide (priv->drawing_area);
	gtk_widget_hide (priv->message_label);
	priv->current_width = 0;
	priv->current_height = 0;
	priv->surface_width = 0;
	priv->surface_height = 0;
	priv->showing_icon = FALSE;
}

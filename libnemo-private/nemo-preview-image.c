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
#include <glib/gi18n.h>

#define RESIZE_DEBOUNCE_MS 150
#define MIN_SIZE_CHANGE 10

/* Required for G_DECLARE_FINAL_TYPE */
struct _NemoPreviewImage {
	GtkBox parent;
};

typedef struct {
	GtkWidget *image;
	GtkWidget *message_label;
	NemoFile *file;

	/* For resize handling */
	guint resize_timeout_id;
	gint current_width;
	gint current_height;

	/* Keep reference to current pixbuf for quick scaling */
	GdkPixbuf *current_pixbuf;
} NemoPreviewImagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NemoPreviewImage, nemo_preview_image, GTK_TYPE_BOX)

/* Forward declarations */
static void on_size_allocate (GtkWidget *widget, GtkAllocation *allocation, gpointer user_data);

static void
nemo_preview_image_finalize (GObject *object)
{
	NemoPreviewImage *preview;
	NemoPreviewImagePrivate *priv;

	preview = NEMO_PREVIEW_IMAGE (object);
	priv = nemo_preview_image_get_instance_private (preview);

	if (priv->resize_timeout_id != 0) {
		g_source_remove (priv->resize_timeout_id);
		priv->resize_timeout_id = 0;
	}

	if (priv->current_pixbuf != NULL) {
		g_object_unref (priv->current_pixbuf);
		priv->current_pixbuf = NULL;
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

	/* Initialize resize tracking */
	priv->resize_timeout_id = 0;
	priv->current_width = 0;
	priv->current_height = 0;
	priv->current_pixbuf = NULL;

	/* Create image widget */
	priv->image = gtk_image_new ();
	gtk_widget_set_halign (priv->image, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (priv->image, GTK_ALIGN_CENTER);
	gtk_box_pack_start (GTK_BOX (preview), priv->image, TRUE, TRUE, 0);

	/* Create message label (hidden by default) */
	priv->message_label = gtk_label_new ("");
	gtk_widget_set_halign (priv->message_label, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (priv->message_label, GTK_ALIGN_CENTER);
	gtk_style_context_add_class (gtk_widget_get_style_context (priv->message_label),
	                              "dim-label");
	gtk_box_pack_start (GTK_BOX (preview), priv->message_label, TRUE, TRUE, 0);

	/* Connect size-allocate signal for resize handling */
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
is_image_file (NemoFile *file)
{
	gchar *mime_type;
	gboolean is_image;

	if (file == NULL || nemo_file_is_directory (file)) {
		return FALSE;
	}

	mime_type = nemo_file_get_mime_type (file);
	is_image = mime_type != NULL && g_str_has_prefix (mime_type, "image/");
	g_free (mime_type);

	return is_image;
}

static void
load_image_at_size (NemoPreviewImage *widget,
                    gint              width,
                    gint              height)
{
	NemoPreviewImagePrivate *priv;
	GFile *location;
	gchar *path;
	GdkPixbuf *pixbuf = NULL;
	cairo_surface_t *surface = NULL;
	gint ui_scale;
	GError *error = NULL;

	priv = nemo_preview_image_get_instance_private (widget);

	if (priv->file == NULL || !is_image_file (priv->file)) {
		return;
	}

	if (width <= 1 || height <= 1) {
		return;
	}

	location = nemo_file_get_location (priv->file);
	path = g_file_get_path (location);
	g_object_unref (location);

	if (path == NULL) {
		return;
	}

	ui_scale = gtk_widget_get_scale_factor (GTK_WIDGET (widget));

	/* Load image directly at the exact size we need */
	pixbuf = gdk_pixbuf_new_from_file_at_scale (path,
	                                             width * ui_scale,
	                                             height * ui_scale,
	                                             TRUE,
	                                             &error);

	if (pixbuf != NULL) {
		/* Save pixbuf for quick scaling during resize */
		if (priv->current_pixbuf != NULL) {
			g_object_unref (priv->current_pixbuf);
		}
		priv->current_pixbuf = g_object_ref (pixbuf);

		surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, ui_scale, NULL);

		if (surface != NULL) {
			gtk_image_set_from_surface (GTK_IMAGE (priv->image), surface);
			gtk_widget_show (priv->image);
			cairo_surface_destroy (surface);
		}

		g_object_unref (pixbuf);
		gtk_widget_hide (priv->message_label);
	} else {
		/* Failed to load image */
		if (error != NULL) {
			g_warning ("Failed to load image: %s", error->message);
			g_error_free (error);
		}
		gtk_label_set_text (GTK_LABEL (priv->message_label),
		                    _("(Failed to load image)"));
		gtk_widget_show (priv->message_label);
		gtk_widget_hide (priv->image);
	}

	g_free (path);

	priv->current_width = width;
	priv->current_height = height;
}

static void
scale_current_pixbuf_to_size (NemoPreviewImage *widget,
                               gint              width,
                               gint              height)
{
	NemoPreviewImagePrivate *priv;
	GdkPixbuf *scaled_pixbuf;
	cairo_surface_t *surface;
	gint ui_scale;
	gint orig_width, orig_height;
	gint target_width, target_height;
	gdouble scale_factor;

	priv = nemo_preview_image_get_instance_private (widget);

	if (priv->current_pixbuf == NULL) {
		return;
	}

	ui_scale = gtk_widget_get_scale_factor (GTK_WIDGET (widget));
	orig_width = gdk_pixbuf_get_width (priv->current_pixbuf);
	orig_height = gdk_pixbuf_get_height (priv->current_pixbuf);

	/* Calculate scaled dimensions maintaining aspect ratio */
	scale_factor = MIN ((gdouble)(width * ui_scale) / orig_width,
	                    (gdouble)(height * ui_scale) / orig_height);

	target_width = (gint)(orig_width * scale_factor);
	target_height = (gint)(orig_height * scale_factor);

	if (target_width < 1 || target_height < 1) {
		return;
	}

	/* Scale pixbuf and display */
	scaled_pixbuf = gdk_pixbuf_scale_simple (priv->current_pixbuf,
	                                          target_width,
	                                          target_height,
	                                          GDK_INTERP_BILINEAR);

	if (scaled_pixbuf != NULL) {
		surface = gdk_cairo_surface_create_from_pixbuf (scaled_pixbuf, ui_scale, NULL);

		if (surface != NULL) {
			gtk_image_set_from_surface (GTK_IMAGE (priv->image), surface);
			cairo_surface_destroy (surface);
		}

		g_object_unref (scaled_pixbuf);
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
	load_image_at_size (widget, allocation.width, allocation.height);

	return G_SOURCE_REMOVE;
}

static void
on_size_allocate (GtkWidget     *widget,
                  GtkAllocation *allocation,
                  gpointer       user_data)
{
	NemoPreviewImage *preview = NEMO_PREVIEW_IMAGE (widget);
	NemoPreviewImagePrivate *priv;
	gint width_diff, height_diff;
	gboolean getting_smaller;

	priv = nemo_preview_image_get_instance_private (preview);

	/* Check if size changed significantly */
	width_diff = ABS (allocation->width - priv->current_width);
	height_diff = ABS (allocation->height - priv->current_height);

	if (width_diff < MIN_SIZE_CHANGE && height_diff < MIN_SIZE_CHANGE) {
		return;
	}

	/* Check if we're getting smaller */
	getting_smaller = (allocation->width < priv->current_width ||
	                   allocation->height < priv->current_height);

	/* If getting smaller, immediately scale down the current pixbuf for responsive UI */
	if (getting_smaller && priv->current_pixbuf != NULL) {
		scale_current_pixbuf_to_size (preview, allocation->width, allocation->height);
	}

	/* Clear existing timeout */
	if (priv->resize_timeout_id != 0) {
		g_source_remove (priv->resize_timeout_id);
	}

	/* Schedule reload with debouncing to get optimal quality */
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

	/* Clear any pending resize timeout */
	if (priv->resize_timeout_id != 0) {
		g_source_remove (priv->resize_timeout_id);
		priv->resize_timeout_id = 0;
	}

	if (priv->file != NULL) {
		nemo_file_unref (priv->file);
	}

	priv->file = file;

	/* Clear current image */
	gtk_image_clear (GTK_IMAGE (priv->image));
	gtk_widget_hide (priv->message_label);
	gtk_widget_hide (priv->image);
	priv->current_width = 0;
	priv->current_height = 0;

	if (priv->current_pixbuf != NULL) {
		g_object_unref (priv->current_pixbuf);
		priv->current_pixbuf = NULL;
	}

	if (file != NULL) {
		nemo_file_ref (file);

		if (is_image_file (file)) {
			/* Load the image at current widget size */
			gtk_widget_get_allocation (GTK_WIDGET (widget), &allocation);
			load_image_at_size (widget, allocation.width, allocation.height);
		} else if (nemo_file_is_directory (file)) {
			/* Show folder icon via nemo_file API */
			GdkPixbuf *icon_pixbuf;
			cairo_surface_t *surface;
			gint ui_scale = gtk_widget_get_scale_factor (GTK_WIDGET (widget));

			icon_pixbuf = nemo_file_get_icon_pixbuf (file, 64, TRUE, ui_scale, 0);
			if (icon_pixbuf != NULL) {
				surface = gdk_cairo_surface_create_from_pixbuf (icon_pixbuf, ui_scale, NULL);
				if (surface != NULL) {
					gtk_image_set_from_surface (GTK_IMAGE (priv->image), surface);
					cairo_surface_destroy (surface);
				}
				g_object_unref (icon_pixbuf);
			}

			gtk_label_set_text (GTK_LABEL (priv->message_label), _("(Folder)"));
			gtk_widget_show (priv->image);
			gtk_widget_show (priv->message_label);
		} else {
			/* Non-image file: show file icon */
			GdkPixbuf *icon_pixbuf;
			cairo_surface_t *surface;
			gint ui_scale = gtk_widget_get_scale_factor (GTK_WIDGET (widget));

			icon_pixbuf = nemo_file_get_icon_pixbuf (file, 64, TRUE, ui_scale, 0);
			if (icon_pixbuf != NULL) {
				surface = gdk_cairo_surface_create_from_pixbuf (icon_pixbuf, ui_scale, NULL);
				if (surface != NULL) {
					gtk_image_set_from_surface (GTK_IMAGE (priv->image), surface);
					cairo_surface_destroy (surface);
				}
				g_object_unref (icon_pixbuf);
			}

			gtk_label_set_text (GTK_LABEL (priv->message_label),
			                    _("(Not an image file)"));
			gtk_widget_show (priv->image);
			gtk_widget_show (priv->message_label);
		}
	}
}

void
nemo_preview_image_clear (NemoPreviewImage *widget)
{
	NemoPreviewImagePrivate *priv;

	g_return_if_fail (NEMO_IS_PREVIEW_IMAGE (widget));

	priv = nemo_preview_image_get_instance_private (widget);

	/* Clear any pending resize timeout */
	if (priv->resize_timeout_id != 0) {
		g_source_remove (priv->resize_timeout_id);
		priv->resize_timeout_id = 0;
	}

	if (priv->current_pixbuf != NULL) {
		g_object_unref (priv->current_pixbuf);
		priv->current_pixbuf = NULL;
	}

	if (priv->file != NULL) {
		nemo_file_unref (priv->file);
		priv->file = NULL;
	}

	gtk_image_clear (GTK_IMAGE (priv->image));
	gtk_widget_hide (priv->image);
	gtk_widget_hide (priv->message_label);
	priv->current_width = 0;
	priv->current_height = 0;
}

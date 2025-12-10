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

#define MAX_PREVIEW_SIZE 400

struct _NemoPreviewImage {
	GtkBox parent;
};

typedef struct {
	GtkWidget *image;
	GtkWidget *message_label;
	NemoFile *file;
} NemoPreviewImagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NemoPreviewImage, nemo_preview_image, GTK_TYPE_BOX)

static void
nemo_preview_image_finalize (GObject *object)
{
	NemoPreviewImage *preview;
	NemoPreviewImagePrivate *priv;

	preview = NEMO_PREVIEW_IMAGE (object);
	priv = nemo_preview_image_get_instance_private (preview);

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

static GdkPixbuf *
scale_pixbuf_to_fit (GdkPixbuf *pixbuf, gint max_width, gint max_height)
{
	gint orig_width, orig_height;
	gint new_width, new_height;
	gdouble scale;

	if (pixbuf == NULL) {
		return NULL;
	}

	orig_width = gdk_pixbuf_get_width (pixbuf);
	orig_height = gdk_pixbuf_get_height (pixbuf);

	/* If image is smaller than max size, don't scale up */
	if (orig_width <= max_width && orig_height <= max_height) {
		return g_object_ref (pixbuf);
	}

	/* Calculate scale factor to fit within max dimensions */
	scale = MIN ((gdouble) max_width / orig_width,
	             (gdouble) max_height / orig_height);

	new_width = (gint) (orig_width * scale);
	new_height = (gint) (orig_height * scale);

	return gdk_pixbuf_scale_simple (pixbuf, new_width, new_height,
	                                 GDK_INTERP_BILINEAR);
}

void
nemo_preview_image_set_file (NemoPreviewImage *widget,
                              NemoFile         *file)
{
	NemoPreviewImagePrivate *priv;
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *scaled_pixbuf = NULL;
	cairo_surface_t *surface = NULL;
	gint ui_scale;

	g_return_if_fail (NEMO_IS_PREVIEW_IMAGE (widget));

	priv = nemo_preview_image_get_instance_private (widget);

	if (priv->file == file) {
		return;
	}

	if (priv->file != NULL) {
		nemo_file_unref (priv->file);
	}

	priv->file = file;

	/* Clear current image */
	gtk_image_clear (GTK_IMAGE (priv->image));
	gtk_widget_hide (priv->message_label);
	gtk_widget_hide (priv->image);

	if (file != NULL) {
		nemo_file_ref (file);

		ui_scale = gtk_widget_get_scale_factor (GTK_WIDGET (widget));

		/* Try to get thumbnail/icon */
		pixbuf = nemo_file_get_icon_pixbuf (file,
		                                    MAX_PREVIEW_SIZE,
		                                    TRUE,
		                                    ui_scale,
		                                    NEMO_FILE_ICON_FLAGS_USE_THUMBNAILS);

		if (pixbuf != NULL) {
			/* Scale pixbuf to fit within max dimensions */
			scaled_pixbuf = scale_pixbuf_to_fit (pixbuf, MAX_PREVIEW_SIZE, MAX_PREVIEW_SIZE);

			if (scaled_pixbuf != NULL) {
				surface = gdk_cairo_surface_create_from_pixbuf (scaled_pixbuf, ui_scale, NULL);

				if (surface != NULL) {
					gtk_image_set_from_surface (GTK_IMAGE (priv->image), surface);
					gtk_widget_show (priv->image);
					cairo_surface_destroy (surface);
				}

				g_object_unref (scaled_pixbuf);
			}

			g_object_unref (pixbuf);

			/* Check if this is a real image file or just an icon */
			/* If it's just an icon (not a thumbnail), show a message */
			if (nemo_file_is_directory (file)) {
				gtk_label_set_text (GTK_LABEL (priv->message_label),
				                    _("(Folder)"));
				gtk_widget_show (priv->message_label);
			}
		} else {
			/* No preview available */
			gtk_label_set_text (GTK_LABEL (priv->message_label),
			                    _("(Image preview not available)"));
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

	if (priv->file != NULL) {
		nemo_file_unref (priv->file);
		priv->file = NULL;
	}

	gtk_image_clear (GTK_IMAGE (priv->image));
	gtk_widget_hide (priv->image);
	gtk_widget_hide (priv->message_label);
}

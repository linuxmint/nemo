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
#include <glib/gi18n.h>

#define RESIZE_DEBOUNCE_MS 150
#define MIN_SIZE_CHANGE 10

/* Required for G_DECLARE_FINAL_TYPE */
struct _NemoPreviewImage {
	GtkBox parent;
};

typedef struct {
	GtkWidget *frame;
	GtkWidget *drawing_area;
	GtkWidget *message_label;
	NemoFile *file;

	/* For resize handling */
	guint resize_timeout_id;
	gint current_width;
	gint current_height;

	/* Keep reference to current pixbuf for quick scaling */
	GdkPixbuf *current_pixbuf;

	/* Current surface to draw */
	cairo_surface_t *current_surface;

	/* Track if showing an icon vs image */
	gboolean showing_icon;
} NemoPreviewImagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NemoPreviewImage, nemo_preview_image, GTK_TYPE_BOX)

/* Forward declarations */
static void on_size_allocate (GtkWidget *widget, GtkAllocation *allocation, gpointer user_data);
static gboolean on_drawing_area_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data);

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

	/* Initialize resize tracking */
	priv->resize_timeout_id = 0;
	priv->current_width = 0;
	priv->current_height = 0;
	priv->current_pixbuf = NULL;
	priv->current_surface = NULL;
	priv->showing_icon = FALSE;

	/* Create frame to hold drawing area */
	priv->frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (priv->frame), GTK_SHADOW_NONE);
	gtk_widget_set_halign (priv->frame, GTK_ALIGN_FILL);
	gtk_widget_set_valign (priv->frame, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (priv->frame, TRUE);
	gtk_widget_set_vexpand (priv->frame, TRUE);

	gtk_box_pack_start (GTK_BOX (preview), priv->frame, TRUE, TRUE, 0);

	/* Create drawing area widget */
	priv->drawing_area = gtk_drawing_area_new ();
	gtk_widget_set_halign (priv->drawing_area, GTK_ALIGN_FILL);
	gtk_widget_set_valign (priv->drawing_area, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (priv->drawing_area, TRUE);
	gtk_widget_set_vexpand (priv->drawing_area, TRUE);
	g_signal_connect (priv->drawing_area, "draw",
	                  G_CALLBACK (on_drawing_area_draw), preview);
	gtk_container_add (GTK_CONTAINER (priv->frame), priv->drawing_area);

	/* Create message label (hidden by default) */
	priv->message_label = gtk_label_new ("");
	gtk_widget_set_halign (priv->message_label, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (priv->message_label, GTK_ALIGN_CENTER);
	gtk_style_context_add_class (gtk_widget_get_style_context (priv->message_label),
	                              "dim-label");
	gtk_box_pack_start (GTK_BOX (preview), priv->message_label, TRUE, TRUE, 0);

    gtk_container_set_border_width (GTK_CONTAINER (preview), 4);
    gtk_widget_show_all (GTK_WIDGET (preview));
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
on_drawing_area_draw (GtkWidget *widget,
                      cairo_t   *cr,
                      gpointer   user_data)
{
	NemoPreviewImage *preview = NEMO_PREVIEW_IMAGE (user_data);
	NemoPreviewImagePrivate *priv;
	gint widget_width, widget_height;
	gint surface_width, surface_height;
	gdouble x_offset, y_offset;
	gint scale_factor;

	priv = nemo_preview_image_get_instance_private (preview);

	if (priv->current_surface == NULL) {
		return FALSE;
	}

	widget_width = gtk_widget_get_allocated_width (widget);
	widget_height = gtk_widget_get_allocated_height (widget);

	/* Get surface dimensions - works for image surfaces created from pixbufs */
	scale_factor = gtk_widget_get_scale_factor (widget);
	surface_width = cairo_image_surface_get_width (priv->current_surface) / scale_factor;
	surface_height = cairo_image_surface_get_height (priv->current_surface) / scale_factor;

	/* Center the image in the drawing area */
	x_offset = (widget_width - surface_width) / 2.0;
	y_offset = (widget_height - surface_height) / 2.0;

	/* Draw the image first */
	cairo_set_source_surface (cr, priv->current_surface, x_offset, y_offset);
	cairo_paint (cr);

	/* If showing an image (not an icon), draw a border on top of it */
	if (!priv->showing_icon) {
		GtkStyleContext *style_context;
		GdkRGBA border_color;
		gdouble border_width = 1.0;
		gdouble inset = 0.5;  /* Inset border slightly inside image bounds */

		/* Get the border color from the frame's style context */
		style_context = gtk_widget_get_style_context (priv->frame);
		gtk_style_context_get_border_color (style_context, GTK_STATE_FLAG_NORMAL, &border_color);

		/* Draw border rectangle inset from the image edge */
		cairo_set_source_rgba (cr, border_color.red, border_color.green,
		                       border_color.blue, border_color.alpha);
		cairo_set_line_width (cr, border_width);
		cairo_rectangle (cr, x_offset + inset, y_offset + inset,
		                 surface_width - (inset * 2), surface_height - (inset * 2));
		cairo_stroke (cr);
	}

	return TRUE;
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

	/* Calculate icon size - use the smaller dimension to fit in the space */
	icon_size = MIN (width, height) * ui_scale;

	/* Get the icon info from the file */
	icon_info = nemo_file_get_icon (priv->file, icon_size, 0, ui_scale, 0);

	if (icon_info != NULL && icon_info->icon_name != NULL) {
		icon_name = icon_info->icon_name;
		icon_theme = gtk_icon_theme_get_default ();

		/* Load the icon at the exact size we need */
		icon_pixbuf = gtk_icon_theme_load_icon_for_scale (icon_theme,
		                                                   icon_name,
		                                                   icon_size / ui_scale,
		                                                   ui_scale,
		                                                   GTK_ICON_LOOKUP_FORCE_SIZE,
		                                                   &error);

		if (icon_pixbuf != NULL) {
			/* Save pixbuf for quick scaling during resize */
			if (priv->current_pixbuf != NULL) {
				g_object_unref (priv->current_pixbuf);
			}
			priv->current_pixbuf = g_object_ref (icon_pixbuf);

			surface = gdk_cairo_surface_create_from_pixbuf (icon_pixbuf, ui_scale, NULL);

			if (surface != NULL) {
				/* Replace old surface with new one */
				if (priv->current_surface != NULL) {
					cairo_surface_destroy (priv->current_surface);
				}
				priv->current_surface = surface;
				gtk_widget_show (priv->drawing_area);
				gtk_widget_queue_draw (priv->drawing_area);
			}

			g_object_unref (icon_pixbuf);
			gtk_widget_hide (priv->message_label);
		} else {
			/* Failed to load icon */
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
		/* No icon info available */
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
			/* Replace old surface with new one */
			if (priv->current_surface != NULL) {
				cairo_surface_destroy (priv->current_surface);
			}
			priv->current_surface = surface;
			gtk_widget_show (priv->drawing_area);
			gtk_widget_queue_draw (priv->drawing_area);
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
		gtk_widget_hide (priv->drawing_area);
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
			/* Replace old surface with new one */
			if (priv->current_surface != NULL) {
				cairo_surface_destroy (priv->current_surface);
			}
			priv->current_surface = surface;
			gtk_widget_queue_draw (priv->drawing_area);
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

	if (priv->showing_icon) {
		load_icon_at_size (widget, allocation.width, allocation.height);
	} else {
		load_image_at_size (widget, allocation.width, allocation.height);
	}

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
	if (priv->current_surface != NULL) {
		cairo_surface_destroy (priv->current_surface);
		priv->current_surface = NULL;
	}
	gtk_widget_hide (priv->message_label);
	gtk_widget_hide (priv->drawing_area);
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
			priv->showing_icon = FALSE;
			gtk_widget_get_allocation (GTK_WIDGET (widget), &allocation);
			load_image_at_size (widget, allocation.width, allocation.height);
		} else {
			/* Load folder or file icon at current widget size */
			priv->showing_icon = TRUE;
			gtk_widget_get_allocation (GTK_WIDGET (widget), &allocation);
			load_icon_at_size (widget, allocation.width, allocation.height);
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
	priv->showing_icon = FALSE;
}

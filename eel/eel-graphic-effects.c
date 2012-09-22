/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Eel - pixbuf manipulation routines for graphical effects.
 *
 * Copyright (C) 2000 Eazel, Inc
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

/* This file contains pixbuf manipulation routines used for graphical effects like pre-lighting
   and selection hilighting */

#include <config.h>

#include "eel-graphic-effects.h"
#include "eel-glib-extensions.h"

#include <string.h>

/* shared utility to create a new pixbuf from the passed-in one */

static GdkPixbuf *
create_new_pixbuf (GdkPixbuf *src)
{
	g_assert (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB);
	g_assert ((!gdk_pixbuf_get_has_alpha (src)
			       && gdk_pixbuf_get_n_channels (src) == 3)
			      || (gdk_pixbuf_get_has_alpha (src)
				  && gdk_pixbuf_get_n_channels (src) == 4));

	return gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src),
			       gdk_pixbuf_get_has_alpha (src),
			       gdk_pixbuf_get_bits_per_sample (src),
			       gdk_pixbuf_get_width (src),
			       gdk_pixbuf_get_height (src));
}

/* utility routine to bump the level of a color component with pinning */

static guchar
lighten_component (guchar cur_value)
{
	int new_value = cur_value;
	new_value += 24 + (new_value >> 3);
	if (new_value > 255) {
		new_value = 255;
	}
	return (guchar) new_value;
}

GdkPixbuf *
eel_create_spotlight_pixbuf (GdkPixbuf* src)
{
	GdkPixbuf *dest;
	int i, j;
	int width, height, has_alpha, src_row_stride, dst_row_stride;
	guchar *target_pixels, *original_pixels;
	guchar *pixsrc, *pixdest;

	g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
			       && gdk_pixbuf_get_n_channels (src) == 3)
			      || (gdk_pixbuf_get_has_alpha (src)
				  && gdk_pixbuf_get_n_channels (src) == 4), NULL);
	g_return_val_if_fail (gdk_pixbuf_get_bits_per_sample (src) == 8, NULL);

	dest = create_new_pixbuf (src);
	
	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	dst_row_stride = gdk_pixbuf_get_rowstride (dest);
	src_row_stride = gdk_pixbuf_get_rowstride (src);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i * dst_row_stride;
		pixsrc = original_pixels + i * src_row_stride;
		for (j = 0; j < width; j++) {		
			*pixdest++ = lighten_component (*pixsrc++);
			*pixdest++ = lighten_component (*pixsrc++);
			*pixdest++ = lighten_component (*pixsrc++);
			if (has_alpha) {
				*pixdest++ = *pixsrc++;
			}
		}
	}
	return dest;
}


/* the following routine was stolen from the panel to darken a pixbuf, by manipulating the saturation */

/* saturation is 0-255, darken is 0-255 */

GdkPixbuf *
eel_create_darkened_pixbuf (GdkPixbuf *src, int saturation, int darken)
{
	gint i, j;
	gint width, height, src_row_stride, dest_row_stride;
	gboolean has_alpha;
	guchar *target_pixels, *original_pixels;
	guchar *pixsrc, *pixdest;
	guchar intensity;
	guchar alpha;
	guchar negalpha;
	guchar r, g, b;
	GdkPixbuf *dest;

	g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
			       && gdk_pixbuf_get_n_channels (src) == 3)
			      || (gdk_pixbuf_get_has_alpha (src)
				  && gdk_pixbuf_get_n_channels (src) == 4), NULL);
	g_return_val_if_fail (gdk_pixbuf_get_bits_per_sample (src) == 8, NULL);

	dest = create_new_pixbuf (src);

	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	dest_row_stride = gdk_pixbuf_get_rowstride (dest);
	src_row_stride = gdk_pixbuf_get_rowstride (src);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i * dest_row_stride;
		pixsrc = original_pixels + i * src_row_stride;
		for (j = 0; j < width; j++) {
			r = *pixsrc++;
			g = *pixsrc++;
			b = *pixsrc++;
			intensity = (r * 77 + g * 150 + b * 28) >> 8;
			negalpha = ((255 - saturation) * darken) >> 8;
			alpha = (saturation * darken) >> 8;
			*pixdest++ = (negalpha * intensity + alpha * r) >> 8;
			*pixdest++ = (negalpha * intensity + alpha * g) >> 8;
			*pixdest++ = (negalpha * intensity + alpha * b) >> 8;
			if (has_alpha) {
				*pixdest++ = *pixsrc++;
			}
		}
	}
	return dest;
}

/* this routine colorizes the passed-in pixbuf by multiplying each pixel with the passed in color */

GdkPixbuf *
eel_create_colorized_pixbuf (GdkPixbuf *src,
			     GdkRGBA *color)
{
	int i, j;
	int width, height, has_alpha, src_row_stride, dst_row_stride;
	guchar *target_pixels;
	guchar *original_pixels;
	guchar *pixsrc;
	guchar *pixdest;
	GdkPixbuf *dest;
	gint red_value, green_value, blue_value;

	g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
			       && gdk_pixbuf_get_n_channels (src) == 3)
			      || (gdk_pixbuf_get_has_alpha (src)
				  && gdk_pixbuf_get_n_channels (src) == 4), NULL);
	g_return_val_if_fail (gdk_pixbuf_get_bits_per_sample (src) == 8, NULL);

	red_value = eel_round (color->red * 255);
	green_value = eel_round (color->green * 255);
	blue_value = eel_round (color->blue * 255);	

	dest = create_new_pixbuf (src);
	
	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	src_row_stride = gdk_pixbuf_get_rowstride (src);
	dst_row_stride = gdk_pixbuf_get_rowstride (dest);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i*dst_row_stride;
		pixsrc = original_pixels + i*src_row_stride;
		for (j = 0; j < width; j++) {		
			*pixdest++ = (*pixsrc++ * red_value) >> 8;
			*pixdest++ = (*pixsrc++ * green_value) >> 8;
			*pixdest++ = (*pixsrc++ * blue_value) >> 8;
			if (has_alpha) {
				*pixdest++ = *pixsrc++;
			}
		}
	}
	return dest;
}

/* utility to stretch a frame to the desired size */

static void
draw_frame_row (GdkPixbuf *frame_image, int target_width, int source_width, int source_v_position, int dest_v_position, GdkPixbuf *result_pixbuf, int left_offset, int height)
{
	int remaining_width, h_offset, slab_width;
	
	remaining_width = target_width;
	h_offset = 0;
	while (remaining_width > 0) {	
		slab_width = remaining_width > source_width ? source_width : remaining_width;
		gdk_pixbuf_copy_area (frame_image, left_offset, source_v_position, slab_width, height, result_pixbuf, left_offset + h_offset, dest_v_position);
		remaining_width -= slab_width;
		h_offset += slab_width; 
	}
}

/* utility to draw the middle section of the frame in a loop */
static void
draw_frame_column (GdkPixbuf *frame_image, int target_height, int source_height, int source_h_position, int dest_h_position, GdkPixbuf *result_pixbuf, int top_offset, int width)
{
	int remaining_height, v_offset, slab_height;
	
	remaining_height = target_height;
	v_offset = 0;
	while (remaining_height > 0) {	
		slab_height = remaining_height > source_height ? source_height : remaining_height;
		gdk_pixbuf_copy_area (frame_image, source_h_position, top_offset, width, slab_height, result_pixbuf, dest_h_position, top_offset + v_offset);
		remaining_height -= slab_height;
		v_offset += slab_height; 
	}
}

GdkPixbuf *
eel_stretch_frame_image (GdkPixbuf *frame_image, int left_offset, int top_offset, int right_offset, int bottom_offset,
			 int dest_width, int dest_height, gboolean fill_flag)
{
	GdkPixbuf *result_pixbuf;
	guchar *pixels_ptr;
	int frame_width, frame_height;
	int y, row_stride;
	int target_width, target_frame_width;
	int target_height, target_frame_height;
	
	frame_width  = gdk_pixbuf_get_width  (frame_image);
	frame_height = gdk_pixbuf_get_height (frame_image );
	
	if (fill_flag) {
		result_pixbuf = gdk_pixbuf_scale_simple (frame_image, dest_width, dest_height, GDK_INTERP_NEAREST);
	} else {
		result_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, dest_width, dest_height);
	}
	row_stride = gdk_pixbuf_get_rowstride (result_pixbuf);
	pixels_ptr = gdk_pixbuf_get_pixels (result_pixbuf);
	
	/* clear the new pixbuf */
	if (!fill_flag) {
		for (y = 0; y < dest_height; y++) {
			memset (pixels_ptr, 255, row_stride);
			pixels_ptr += row_stride; 
		}
	}
	
	target_width  = dest_width - left_offset - right_offset;
	target_frame_width = frame_width - left_offset - right_offset;
	
	target_height  = dest_height - top_offset - bottom_offset;
	target_frame_height = frame_height - top_offset - bottom_offset;
	
	/* draw the left top corner  and top row */
	gdk_pixbuf_copy_area (frame_image, 0, 0, left_offset, top_offset, result_pixbuf, 0,  0);
	draw_frame_row (frame_image, target_width, target_frame_width, 0, 0, result_pixbuf, left_offset, top_offset);
	
	/* draw the right top corner and left column */
	gdk_pixbuf_copy_area (frame_image, frame_width - right_offset, 0, right_offset, top_offset, result_pixbuf, dest_width - right_offset,  0);
	draw_frame_column (frame_image, target_height, target_frame_height, 0, 0, result_pixbuf, top_offset, left_offset);

	/* draw the bottom right corner and bottom row */
	gdk_pixbuf_copy_area (frame_image, frame_width - right_offset, frame_height - bottom_offset, right_offset, bottom_offset, result_pixbuf, dest_width - right_offset,  dest_height - bottom_offset);
	draw_frame_row (frame_image, target_width, target_frame_width, frame_height - bottom_offset, dest_height - bottom_offset, result_pixbuf, left_offset, bottom_offset);
		
	/* draw the bottom left corner and the right column */
	gdk_pixbuf_copy_area (frame_image, 0, frame_height - bottom_offset, left_offset, bottom_offset, result_pixbuf, 0,  dest_height - bottom_offset);
	draw_frame_column (frame_image, target_height, target_frame_height, frame_width - right_offset, dest_width - right_offset, result_pixbuf, top_offset, right_offset);
	
	return result_pixbuf;
}


/* draw an arbitrary frame around an image, with the result passed back in a newly allocated pixbuf */
GdkPixbuf *
eel_embed_image_in_frame (GdkPixbuf *source_image, GdkPixbuf *frame_image, int left_offset, int top_offset, int right_offset, int bottom_offset)
{
	GdkPixbuf *result_pixbuf;
	int source_width, source_height;
	int dest_width, dest_height;
	
	source_width  = gdk_pixbuf_get_width  (source_image);
	source_height = gdk_pixbuf_get_height (source_image);

	dest_width  = source_width  + left_offset + right_offset;
	dest_height = source_height + top_offset  + bottom_offset;
	
	result_pixbuf = eel_stretch_frame_image (frame_image, left_offset, top_offset, right_offset, bottom_offset, 
						      dest_width, dest_height, FALSE);
		
	/* Finally, copy the source image into the framed area */
	gdk_pixbuf_copy_area (source_image, 0, 0, source_width, source_height, result_pixbuf, left_offset,  top_offset);

	return result_pixbuf;
}

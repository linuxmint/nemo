/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gdk-extensions.c: Graphics routines to augment what's in gdk.

   Copyright (C) 1999, 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Darin Adler <darin@eazel.com>, 
            Pavel Cisler <pavel@eazel.com>,
            Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "eel-gdk-extensions.h"

#include "eel-glib-extensions.h"
#include "eel-string.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <stdlib.h>
#include <pango/pango.h>

EelGdkGeometryFlags
eel_gdk_parse_geometry (const char *string, int *x_return, int *y_return,
			     guint *width_return, guint *height_return)
{
	int x11_flags;
	EelGdkGeometryFlags gdk_flags;

	g_return_val_if_fail (string != NULL, EEL_GDK_NO_VALUE);
	g_return_val_if_fail (x_return != NULL, EEL_GDK_NO_VALUE);
	g_return_val_if_fail (y_return != NULL, EEL_GDK_NO_VALUE);
	g_return_val_if_fail (width_return != NULL, EEL_GDK_NO_VALUE);
	g_return_val_if_fail (height_return != NULL, EEL_GDK_NO_VALUE);

	x11_flags = XParseGeometry (string, x_return, y_return,
				    width_return, height_return);

	gdk_flags = EEL_GDK_NO_VALUE;
	if (x11_flags & XValue) {
		gdk_flags |= EEL_GDK_X_VALUE;
	}
	if (x11_flags & YValue) {
		gdk_flags |= EEL_GDK_Y_VALUE;
	}
	if (x11_flags & WidthValue) {
		gdk_flags |= EEL_GDK_WIDTH_VALUE;
	}
	if (x11_flags & HeightValue) {
		gdk_flags |= EEL_GDK_HEIGHT_VALUE;
	}
	if (x11_flags & XNegative) {
		gdk_flags |= EEL_GDK_X_NEGATIVE;
	}
	if (x11_flags & YNegative) {
		gdk_flags |= EEL_GDK_Y_NEGATIVE;
	}

	return gdk_flags;
}

#define CLAMP_COLOR(v) (t = (v), CLAMP (t, 0, 1))
#define SATURATE(v) ((1.0 - saturation) * intensity + saturation * (v))

void
eel_make_color_inactive (GdkRGBA *color)
{
	double intensity, saturation;
	gdouble t;

	saturation = 0.7;
	intensity = color->red * 0.30 + color->green * 0.59 + color->blue * 0.11;
	color->red = SATURATE (color->red);
	color->green = SATURATE (color->green);
	color->blue = SATURATE (color->blue);

	if (intensity > 0.5) {
		color->red *= 0.9;
		color->green *= 0.9;
		color->blue *= 0.9;
	} else {
		color->red *= 1.25;
		color->green *= 1.25;
		color->blue *= 1.25;
	}

	color->red = CLAMP_COLOR (color->red);
	color->green = CLAMP_COLOR (color->green);
	color->blue = CLAMP_COLOR (color->blue);
}

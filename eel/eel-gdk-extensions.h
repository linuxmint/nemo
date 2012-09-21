// /* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gdk-extensions.h: Graphics routines to augment what's in gdk.

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
            Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef EEL_GDK_EXTENSIONS_H
#define EEL_GDK_EXTENSIONS_H

#include <gdk/gdk.h>

/* Bits returned by eel_gdk_parse_geometry */
typedef enum {
	EEL_GDK_NO_VALUE     = 0x00,
	EEL_GDK_X_VALUE      = 0x01,
	EEL_GDK_Y_VALUE      = 0x02,
	EEL_GDK_WIDTH_VALUE  = 0x04,
	EEL_GDK_HEIGHT_VALUE = 0x08,
	EEL_GDK_ALL_VALUES   = 0x0f,
	EEL_GDK_X_NEGATIVE   = 0x10,
	EEL_GDK_Y_NEGATIVE   = 0x20
} EelGdkGeometryFlags;

/* Wrapper for XParseGeometry */
EelGdkGeometryFlags eel_gdk_parse_geometry                 (const char          *string,
							    int                 *x_return,
							    int                 *y_return,
							    guint               *width_return,
							    guint               *height_return);
void                eel_make_color_inactive                (GdkRGBA             *color);

#endif /* EEL_GDK_EXTENSIONS_H */

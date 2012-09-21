/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-art-extensions.c - implementation of libart extension functions.

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: Darin Adler <darin@eazel.com>
            Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>

#include "eel-art-extensions.h"
#include "eel-lib-self-check-functions.h"
#include <math.h>

const EelDRect eel_drect_empty = { 0.0, 0.0, 0.0, 0.0 };
const EelIRect eel_irect_empty = { 0, 0, 0, 0 };

void
eel_irect_copy (EelIRect *dest, const EelIRect *src)
{
	dest->x0 = src->x0;
	dest->y0 = src->y0;
	dest->x1 = src->x1;
	dest->y1 = src->y1;
}

void
eel_irect_union (EelIRect *dest,
		  const EelIRect *src1,
		  const EelIRect *src2) {
	if (eel_irect_is_empty (src1)) {
		eel_irect_copy (dest, src2);
	} else if (eel_irect_is_empty (src2)) {
		eel_irect_copy (dest, src1);
	} else {
		dest->x0 = MIN (src1->x0, src2->x0);
		dest->y0 = MIN (src1->y0, src2->y0);
		dest->x1 = MAX (src1->x1, src2->x1);
		dest->y1 = MAX (src1->y1, src2->y1);
	}
}

void
eel_irect_intersect (EelIRect *dest,
		     const EelIRect *src1,
		     const EelIRect *src2)
{
	dest->x0 = MAX (src1->x0, src2->x0);
	dest->y0 = MAX (src1->y0, src2->y0);
	dest->x1 = MIN (src1->x1, src2->x1);
	dest->y1 = MIN (src1->y1, src2->y1);
}

gboolean
eel_irect_is_empty (const EelIRect *src)
{
	return (src->x1 <= src->x0 ||
		src->y1 <= src->y0);
}

/**
 * eel_irect_get_width:
 * 
 * @rectangle: An EelIRect.
 *
 * Returns: The width of the rectangle.
 * 
 */
int
eel_irect_get_width (EelIRect rectangle)
{
	return rectangle.x1 - rectangle.x0;
}

/**
 * eel_irect_get_height:
 * 
 * @rectangle: An EelIRect.
 *
 * Returns: The height of the rectangle.
 * 
 */
int
eel_irect_get_height (EelIRect rectangle)
{
	return rectangle.y1 - rectangle.y0;
}


static void
eel_drect_copy (EelDRect *dest,
		const EelDRect *src)
{
	dest->x0 = src->x0;
	dest->y0 = src->y0;
	dest->x1 = src->x1;
	dest->y1 = src->y1;
}

static gboolean
eel_drect_is_empty (const EelDRect *src)
{
	return (src->x1 <= src->x0 || src->y1 <= src->y0);
}

void
eel_drect_union (EelDRect *dest,
		 const EelDRect *src1,
		 const EelDRect *src2)
{
	if (eel_drect_is_empty (src1)) {
		eel_drect_copy (dest, src2);
	} else if (eel_drect_is_empty (src2)) {
		eel_drect_copy (dest, src1);
	} else {
		dest->x0 = MIN (src1->x0, src2->x0);
		dest->y0 = MIN (src1->y0, src2->y0);
		dest->x1 = MAX (src1->x1, src2->x1);
		dest->y1 = MAX (src1->y1, src2->y1);
	}
}


/**
 * eel_irect_contains_point:
 * 
 * @rectangle: An EelIRect.
 * @x: X coordinate to test.
 * @y: Y coordinate to test.
 *
 * Returns: A boolean value indicating whether the rectangle 
 *          contains the x,y coordinate.
 * 
 */
gboolean
eel_irect_contains_point (EelIRect rectangle,
			  int x,
			  int y)
{
	return x >= rectangle.x0
		&& x <= rectangle.x1
		&& y >= rectangle.y0
		&& y <= rectangle.y1;
}

gboolean
eel_irect_hits_irect (EelIRect rectangle_a,
			  EelIRect rectangle_b)
{
	EelIRect intersection;
	eel_irect_intersect (&intersection, &rectangle_a, &rectangle_b);
	return !eel_irect_is_empty (&intersection);
}

gboolean
eel_irect_equal (EelIRect rectangle_a,
		     EelIRect rectangle_b)
{
	return rectangle_a.x0 == rectangle_b.x0
		&& rectangle_a.y0 == rectangle_b.y0
		&& rectangle_a.x1 == rectangle_b.x1
		&& rectangle_a.y1 == rectangle_b.y1;
}

EelIRect 
eel_irect_offset_by (EelIRect rectangle, int x, int y)
{
	rectangle.x0 += x;
	rectangle.x1 += x;
	rectangle.y0 += y;
	rectangle.y1 += y;
	
	return rectangle;
}

EelIRect 
eel_irect_scale_by (EelIRect rectangle, double scale)
{
	rectangle.x0 *= scale;
	rectangle.x1 *= scale;
	rectangle.y0 *= scale;
	rectangle.y1 *= scale;
	
	return rectangle;
}

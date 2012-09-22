/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-art-extensions.h - interface of libart extension functions.

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

#ifndef EEL_ART_EXTENSIONS_H
#define EEL_ART_EXTENSIONS_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct  {
  double x0, y0, x1, y1;
} EelDRect;

typedef struct  {
  /*< public >*/
  int x0, y0, x1, y1;
} EelIRect;

extern const EelDRect eel_drect_empty;
extern const EelIRect eel_irect_empty;

void     eel_irect_copy              (EelIRect       *dest,
				      const EelIRect *src);
void     eel_irect_union             (EelIRect       *dest,
				      const EelIRect *src1,
				      const EelIRect *src2);
void     eel_irect_intersect         (EelIRect       *dest,
				      const EelIRect *src1,
				      const EelIRect *src2);
gboolean eel_irect_equal             (EelIRect        rectangle_a,
				      EelIRect        rectangle_b);
gboolean eel_irect_hits_irect        (EelIRect        rectangle_a,
				      EelIRect        rectangle_b);
EelIRect eel_irect_offset_by         (EelIRect        rectangle,
				      int             x,
				      int             y);
EelIRect eel_irect_scale_by          (EelIRect        rectangle,
				      double          scale);
gboolean eel_irect_is_empty          (const EelIRect *rectangle);
gboolean eel_irect_contains_point    (EelIRect        outer_rectangle,
				      int             x,
				      int             y);
int      eel_irect_get_width         (EelIRect        rectangle);
int      eel_irect_get_height        (EelIRect        rectangle);

void eel_drect_union (EelDRect       *dest,
		      const EelDRect *src1,
		      const EelDRect *src2);

G_END_DECLS

#endif /* EEL_ART_EXTENSIONS_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-icon-container-private.h

   Copyright (C) 1999, 2000 Free Software Foundation
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

   Author: Ettore Perazzoli <ettore@gnu.org>
*/

#ifndef NEMO_PLACEMENT_GRID_H
#define NEMO_PLACEMENT_GRID_H

#include <glib.h>
#include "nemo-icon-private.h"
#include "nemo-icon.h"
#include <eel/eel-art-extensions.h>

typedef struct {
    NemoIconContainer *container;
    int **icon_grid;
    int *grid_memory;
    int num_rows;
    int num_columns;
    gboolean tight;
} NemoPlacementGrid;

NemoPlacementGrid *nemo_placement_grid_new               (NemoIconContainer *container, gboolean tight);
void               nemo_placement_grid_free              (NemoPlacementGrid *grid);
gboolean           nemo_placement_grid_position_is_free  (NemoPlacementGrid *grid, EelIRect pos);
void               nemo_placement_grid_mark              (NemoPlacementGrid *grid, EelIRect pos);
void               nemo_placement_grid_canvas_position_to_grid_position (NemoPlacementGrid *grid, EelIRect canvas_position, EelIRect *grid_position);
void               nemo_placement_grid_mark_icon         (NemoPlacementGrid *grid, NemoIcon *icon);

#endif /* NEMO_PLACEMENT_GRID_H */

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
#include <gtk/gtk.h>
#include "nemo-icon-private.h"
#include "nemo-icon.h"
#include <eel/eel-art-extensions.h>

typedef struct {
    NemoIconContainer *container;
    int **icon_grid;
    int *grid_memory;
    int num_rows;
    int num_columns;
    GtkBorder *borders;
    gboolean horizontal;
} NemoCenteredPlacementGrid;

NemoCenteredPlacementGrid *nemo_centered_placement_grid_new               (NemoIconContainer *container, gboolean horizontal);
void               nemo_centered_placement_grid_free              (NemoCenteredPlacementGrid *grid);
gboolean           nemo_centered_placement_grid_position_is_free  (NemoCenteredPlacementGrid *grid, gint grid_x, gint grid_y);
void               nemo_centered_placement_grid_mark              (NemoCenteredPlacementGrid *grid, gint grid_x, gint grid_y);

void               nemo_centered_placement_grid_nominal_to_icon_position (NemoCenteredPlacementGrid *grid,
                                                                          NemoIcon                  *icon,
                                                                          gint                       x_nominal,
                                                                          gint                       y_nominal,
                                                                          gint                      *x_adjusted,
                                                                          gint                      *y_adjusted);
void               nemo_centered_placement_grid_icon_position_to_nominal (NemoCenteredPlacementGrid *grid,
                                                                          NemoIcon                  *icon,
                                                                          gint                       x_adjusted,
                                                                          gint                       y_adjusted,
                                                                          gint                      *x_nominal,
                                                                          gint                      *y_nominal);
void               nemo_centered_placement_grid_mark_icon         (NemoCenteredPlacementGrid *grid, NemoIcon *icon);
void               nemo_centered_placement_grid_get_next_position (NemoCenteredPlacementGrid *grid,
                                                                   NemoIcon                  *icon,
                                                                   gint                      *x_out,
                                                                   gint                      *y_out);
void               nemo_centered_placement_grid_find_empty_position (NemoCenteredPlacementGrid *grid,
                                                                     NemoIcon                  *icon,
                                                                     gint                       x_nominal,
                                                                     gint                       y_nominal,
                                                                     gint                      *x_new,
                                                                     gint                      *y_new);
void               nemo_centered_placement_grid_pre_populate        (NemoCenteredPlacementGrid *grid,
                                                                     GList                     *icons);
#endif /* NEMO_PLACEMENT_GRID_H */

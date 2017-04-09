/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* Copyright (C) 1999, 2000 Free Software Foundation
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


#include "math.h"

#include "nemo-placement-grid.h"

NemoPlacementGrid *
nemo_placement_grid_new (NemoIconContainer *container, gboolean tight)
{
    NemoPlacementGrid *grid;
    int width, height;
    int num_columns;
    int num_rows;
    int i;
    GtkAllocation allocation;

    /* Get container dimensions */
    gtk_widget_get_allocation (GTK_WIDGET (container), &allocation);
    width  = nemo_icon_container_get_canvas_width (container, allocation);
    height = nemo_icon_container_get_canvas_height (container, allocation);
    num_columns = width / GET_VIEW_CONSTANT (container, snap_size_x);
    num_rows = height / GET_VIEW_CONSTANT (container, snap_size_y);

    if (num_columns == 0 || num_rows == 0) {
        return NULL;
    }

    grid = g_new0 (NemoPlacementGrid, 1);
    grid->container = container;
    grid->tight = tight;
    grid->num_columns = num_columns;
    grid->num_rows = num_rows;

    grid->grid_memory = g_new0 (int, (num_rows * num_columns));
    grid->icon_grid = g_new0 (int *, num_columns);
    
    for (i = 0; i < num_columns; i++) {
        grid->icon_grid[i] = grid->grid_memory + (i * num_rows);
    }
    
    return grid;
}

void
nemo_placement_grid_free (NemoPlacementGrid *grid)
{
    g_free (grid->icon_grid);
    g_free (grid->grid_memory);
    g_free (grid);
}

gboolean
nemo_placement_grid_position_is_free (NemoPlacementGrid *grid, EelIRect pos)
{
    int x, y;
    
    g_assert (pos.x0 >= 0 && pos.x0 < grid->num_columns);
    g_assert (pos.y0 >= 0 && pos.y0 < grid->num_rows);
    g_assert (pos.x1 >= 0 && pos.x1 < grid->num_columns);
    g_assert (pos.y1 >= 0 && pos.y1 < grid->num_rows);

    for (x = pos.x0; x <= pos.x1; x++) {
        for (y = pos.y0; y <= pos.y1; y++) {
            if (grid->icon_grid[x][y] != 0) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

void
nemo_placement_grid_mark (NemoPlacementGrid *grid, EelIRect pos)
{
    int x, y;
    
    g_assert (pos.x0 >= 0 && pos.x0 < grid->num_columns);
    g_assert (pos.y0 >= 0 && pos.y0 < grid->num_rows);
    g_assert (pos.x1 >= 0 && pos.x1 < grid->num_columns);
    g_assert (pos.y1 >= 0 && pos.y1 < grid->num_rows);

    for (x = pos.x0; x <= pos.x1; x++) {
        for (y = pos.y0; y <= pos.y1; y++) {
            grid->icon_grid[x][y] = 1;
        }
    }
}

void
nemo_placement_grid_canvas_position_to_grid_position (NemoPlacementGrid *grid,
                                                      EelIRect canvas_position,
                                                      EelIRect *grid_position)
{
    /* The first causes minimal moving around during a snap, but
     * can end up with partially overlapping icons.  The second one won't
     * allow any overlapping, but can cause more movement to happen 
     * during a snap. */
    NemoIconContainer *container = grid->container;

    if (grid->tight) {
        grid_position->x0 = ceil ((double)(canvas_position.x0 - GET_VIEW_CONSTANT (container, desktop_pad_horizontal)) / GET_VIEW_CONSTANT (container, snap_size_x));
        grid_position->y0 = ceil ((double)(canvas_position.y0 - GET_VIEW_CONSTANT (container, desktop_pad_vertical)) / GET_VIEW_CONSTANT (container, snap_size_y));
        grid_position->x1 = floor ((double)(canvas_position.x1 - GET_VIEW_CONSTANT (container, desktop_pad_horizontal)) / GET_VIEW_CONSTANT (container, snap_size_x));
        grid_position->y1 = floor ((double)(canvas_position.y1 - GET_VIEW_CONSTANT (container, desktop_pad_vertical)) / GET_VIEW_CONSTANT (container, snap_size_y));
    } else {
        grid_position->x0 = floor ((double)(canvas_position.x0 - GET_VIEW_CONSTANT (container, desktop_pad_horizontal)) / GET_VIEW_CONSTANT (container, snap_size_x));
        grid_position->y0 = floor ((double)(canvas_position.y0 - GET_VIEW_CONSTANT (container, desktop_pad_vertical)) / GET_VIEW_CONSTANT (container, snap_size_y));
        grid_position->x1 = floor ((double)(canvas_position.x1 - GET_VIEW_CONSTANT (container, desktop_pad_horizontal)) / GET_VIEW_CONSTANT (container, snap_size_x));
        grid_position->y1 = floor ((double)(canvas_position.y1 - GET_VIEW_CONSTANT (container, desktop_pad_vertical)) / GET_VIEW_CONSTANT (container, snap_size_y));
    }

    grid_position->x0 = CLAMP (grid_position->x0, 0, grid->num_columns - 1);
    grid_position->y0 = CLAMP (grid_position->y0, 0, grid->num_rows - 1);
    grid_position->x1 = CLAMP (grid_position->x1, grid_position->x0, grid->num_columns - 1);
    grid_position->y1 = CLAMP (grid_position->y1, grid_position->y0, grid->num_rows - 1);
}

void
nemo_placement_grid_mark_icon (NemoPlacementGrid *grid, NemoIcon *icon)
{
    EelIRect icon_pos;
    EelIRect grid_pos;
    
    nemo_icon_container_icon_get_bounding_box (grid->container, icon,
                   &icon_pos.x0, &icon_pos.y0,
                   &icon_pos.x1, &icon_pos.y1,
                   BOUNDS_USAGE_FOR_LAYOUT);
    nemo_placement_grid_canvas_position_to_grid_position (grid, 
                      icon_pos,
                      &grid_pos);

    nemo_placement_grid_mark (grid, grid_pos);
}

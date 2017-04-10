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
*/


#include "math.h"

#include "nemo-centered-placement-grid.h"

NemoCenteredPlacementGrid *
nemo_centered_placement_grid_new (NemoIconContainer *container, gboolean horizontal)
{
    NemoCenteredPlacementGrid *grid;
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

    grid = g_new0 (NemoCenteredPlacementGrid, 1);
    grid->container = container;
    grid->horizontal = horizontal;
    grid->num_columns = num_columns;
    grid->num_rows = num_rows;
    grid->borders = gtk_border_new ();

    grid->grid_memory = g_new0 (int, (num_rows * num_columns));
    grid->icon_grid = g_new0 (int *, num_columns);
    
    for (i = 0; i < num_columns; i++) {
        grid->icon_grid[i] = grid->grid_memory + (i * num_rows);
    }

    grid->borders->left = (width - (num_columns * GET_VIEW_CONSTANT (container, snap_size_x))) / 2;
    grid->borders->right = (width - (num_columns * GET_VIEW_CONSTANT (container, snap_size_x))) / 2;
    grid->borders->top = (height - (num_rows * GET_VIEW_CONSTANT (container, snap_size_y))) / 2;
    grid->borders->bottom = (height - (num_rows * GET_VIEW_CONSTANT (container, snap_size_y))) / 2;

    return grid;
}

void
nemo_centered_placement_grid_free (NemoCenteredPlacementGrid *grid)
{
    g_free (grid->icon_grid);
    g_free (grid->grid_memory);
    gtk_border_free (grid->borders);
    g_free (grid);
}

gboolean
nemo_centered_placement_grid_position_is_free (NemoCenteredPlacementGrid *grid,
                                               gint                       grid_x,
                                               gint                       grid_y)
{
    g_assert (grid_x >= 0 && grid_x < grid->num_columns);
    g_assert (grid_y >= 0 && grid_y < grid->num_rows);

    return grid->icon_grid[grid_x][grid_y] == 0;
}

void
nemo_centered_placement_grid_mark (NemoCenteredPlacementGrid *grid,
                                   gint                       grid_x,
                                   gint                       grid_y)
{
    g_assert (grid_x >= 0 && grid_x < grid->num_columns);
    g_assert (grid_y >= 0 && grid_y < grid->num_rows);

    grid->icon_grid[grid_x][grid_y] = 1;
}

void
nemo_centered_placement_grid_nominal_to_icon_position (NemoCenteredPlacementGrid *grid,
                                                       NemoIcon                  *icon,
                                                       gint                       x_nominal,
                                                       gint                       y_nominal,
                                                       gint                      *x_adjusted,
                                                       gint                      *y_adjusted)
{
    EelDRect icon_rect;

    icon_rect = nemo_icon_canvas_item_get_icon_rectangle (icon->item);

    *x_adjusted =   x_nominal
                  + (GET_VIEW_CONSTANT (grid->container, snap_size_x) / 2)
                  - ((icon_rect.x1 - icon_rect.x0) / 2);

    *y_adjusted =   y_nominal
                  + (GET_VIEW_CONSTANT (grid->container, snap_size_y) / 2)
                  - ((icon_rect.y1 - icon_rect.y0) / 2);
}

void
nemo_centered_placement_grid_icon_position_to_nominal (NemoCenteredPlacementGrid *grid,
                                                       NemoIcon                  *icon,
                                                       gint                       x_adjusted,
                                                       gint                       y_adjusted,
                                                       gint                      *x_nominal,
                                                       gint                      *y_nominal)
{
    EelDRect icon_rect;

    icon_rect = nemo_icon_canvas_item_get_icon_rectangle (icon->item);

    *x_nominal =   x_adjusted
                 + ((icon_rect.x1 - icon_rect.x0) / 2)
                 - (GET_VIEW_CONSTANT (grid->container, snap_size_x) / 2);

    *y_nominal =   y_adjusted
                 + ((icon_rect.y1 - icon_rect.y0) / 2)
                 - (GET_VIEW_CONSTANT (grid->container, snap_size_y) / 2);
}

void
nemo_centered_placement_grid_mark_icon (NemoCenteredPlacementGrid *grid, NemoIcon *icon)
{
    EelDRect icon_rect;
    gint nom_x, nom_y, grid_x, grid_y;

    icon_rect = nemo_icon_canvas_item_get_icon_rectangle (icon->item);

    nemo_centered_placement_grid_icon_position_to_nominal (grid, icon,
                                                           icon_rect.x0,
                                                           icon_rect.y0,
                                                           &nom_x, &nom_y);

    grid_x = (nom_x - grid->borders->left) / GET_VIEW_CONSTANT (grid->container, snap_size_x);
    grid_y = (nom_y - grid->borders->top) / GET_VIEW_CONSTANT (grid->container, snap_size_y);

    nemo_centered_placement_grid_mark (grid, grid_x, grid_y);
}

void
nemo_centered_placement_grid_get_next_position (NemoCenteredPlacementGrid *grid,
                                                NemoIcon                  *icon,
                                                gint                      *x_out,
                                                gint                      *y_out)
{
    gint x, y, x_ret, y_ret;

    x_ret = -1;
    y_ret = -1;

    if (grid->horizontal) {
        for (y = 0; y < grid->num_rows; y++) {
            for (x = 0; x < grid->num_columns; x++) {
                if (nemo_centered_placement_grid_position_is_free (grid, x, y)) {
                    nemo_centered_placement_grid_mark (grid, x, y);
                    x_ret = grid->borders->left + (x * GET_VIEW_CONSTANT (grid->container, snap_size_x));
                    y_ret = grid->borders->top + (y * GET_VIEW_CONSTANT (grid->container, snap_size_y));
                    goto out;
                }
            }
        }
    } else {
        for (x = 0; x < grid->num_columns; x++) {
            for (y = 0; y < grid->num_rows; y++) {
                if (nemo_centered_placement_grid_position_is_free (grid, x, y)) {
                    nemo_centered_placement_grid_mark (grid, x, y);
                    x_ret = grid->borders->left + (x * GET_VIEW_CONSTANT (grid->container, snap_size_x));
                    y_ret = grid->borders->top + (y * GET_VIEW_CONSTANT (grid->container, snap_size_y));
                    goto out;
                }
            }
        }
    }

out:
    nemo_centered_placement_grid_nominal_to_icon_position (grid, icon,
                                                           x_ret, y_ret,
                                                           &x_ret, &y_ret);
    *x_out = x_ret;
    *y_out = y_ret;
}

void
nemo_centered_placement_grid_find_empty_position (NemoCenteredPlacementGrid *grid,
                                                  NemoIcon                  *icon,
                                                  gint                       x_orig,
                                                  gint                       y_orig,
                                                  gint                      *x_new,
                                                  gint                      *y_new)
{
    gint x, y, cur_x, cur_y, x_half_snap, y_half_snap;
    gint last_empty_x, last_empty_y;
    gboolean ideal_position_occupied;
    gboolean match;

    x_half_snap = GET_VIEW_CONSTANT (grid->container, snap_size_x) / 2;
    y_half_snap = GET_VIEW_CONSTANT (grid->container, snap_size_y) / 2;

    ideal_position_occupied = FALSE;
    match = FALSE;
    cur_x = cur_y = ICON_UNPOSITIONED_VALUE;
    last_empty_x = last_empty_y = ICON_UNPOSITIONED_VALUE;

    if (grid->horizontal) {
        for (y = 0; y < grid->num_rows; y++) {
            for (x = 0; x < grid->num_columns; x++) {
                cur_x = grid->borders->left + (x * GET_VIEW_CONSTANT (grid->container, snap_size_x));
                cur_y = grid->borders->top + (y * GET_VIEW_CONSTANT (grid->container, snap_size_y));

                if ((x_orig >= cur_x - x_half_snap && x_orig <= cur_x + x_half_snap) &&
                    (y_orig >= cur_y - y_half_snap && y_orig <= cur_y + y_half_snap)) {
                    match = TRUE;
                }

                if (!nemo_centered_placement_grid_position_is_free (grid, x, y)) {
                    gint icon_nom_x, icon_nom_y, grid_x, grid_y;

                    nemo_centered_placement_grid_icon_position_to_nominal (grid, icon, icon->x, icon->y, &icon_nom_x, &icon_nom_y);
                    grid_x = (icon_nom_x - grid->borders->left) / GET_VIEW_CONSTANT (grid->container, snap_size_x);
                    grid_y = (icon_nom_y - grid->borders->top) / GET_VIEW_CONSTANT (grid->container, snap_size_y);

                    if (match) {
                        ideal_position_occupied = TRUE;

                        /* If the current position is the same as the supplied icon's position, consider
                         * the position free.  This allows dragging an icon, then dropping it back where it was.
                         */
                        if (grid_x != cur_x || grid_y != cur_y) {
                            *x_new = cur_x;
                            *y_new = cur_y;
                            return;
                        }
                    }

                    continue;
                } else {
                    if (last_empty_x == ICON_UNPOSITIONED_VALUE) {
                        last_empty_x = cur_x;
                        last_empty_y = cur_y;
                    }
                }

                if (match || ideal_position_occupied) {
                    *x_new = cur_x;
                    *y_new = cur_y;
                    return;
                }
            }
        }
    } else {
        for (x = 0; x < grid->num_columns; x++) {
            for (y = 0; y < grid->num_rows; y++) {
                cur_x = grid->borders->left + (x * GET_VIEW_CONSTANT (grid->container, snap_size_x));
                cur_y = grid->borders->top + (y * GET_VIEW_CONSTANT (grid->container, snap_size_y));

                if ((x_orig >= cur_x - x_half_snap && x_orig <= cur_x + x_half_snap) &&
                    (y_orig >= cur_y - y_half_snap && y_orig <= cur_y + y_half_snap)) {
                    match = TRUE;
                }

                if (!nemo_centered_placement_grid_position_is_free (grid, x, y)) {
                    gint icon_nom_x, icon_nom_y, grid_x, grid_y;

                    nemo_centered_placement_grid_icon_position_to_nominal (grid, icon, icon->x, icon->y, &icon_nom_x, &icon_nom_y);
                    grid_x = (icon_nom_x - grid->borders->left) / GET_VIEW_CONSTANT (grid->container, snap_size_x);
                    grid_y = (icon_nom_y - grid->borders->top) / GET_VIEW_CONSTANT (grid->container, snap_size_y);

                    if (match) {
                        ideal_position_occupied = TRUE;

                        /* If the current position is the same as the supplied icon's position, consider
                         * the position free.  This allows dragging an icon, then dropping it back where it was.
                         */
                        if (grid_x != cur_x || grid_y != cur_y) {
                            *x_new = cur_x;
                            *y_new = cur_y;
                            return;
                        }
                    }

                    continue;
                } else {
                    if (last_empty_x == ICON_UNPOSITIONED_VALUE) {
                        last_empty_x = cur_x;
                        last_empty_y = cur_y;
                    }
                }

                if (match || ideal_position_occupied) {
                    *x_new = cur_x;
                    *y_new = cur_y;
                    return;
                }
            }
        }
    }

    if (last_empty_x != ICON_UNPOSITIONED_VALUE) {
        *x_new = last_empty_x;
        *y_new = last_empty_y;
    } else {
        *x_new = cur_x;
        *y_new = cur_y;
    }
}

void
nemo_centered_placement_grid_pre_populate (NemoCenteredPlacementGrid *grid,
                                           GList                     *icons)
{
    GList *p;
    NemoIcon *icon;

    for (p = icons; p != NULL; p = p->next) {
            icon = p->data;

            if (nemo_icon_container_icon_is_positioned (icon) && !icon->has_lazy_position) {
                nemo_centered_placement_grid_mark_icon (grid, icon);
            }
        }
}

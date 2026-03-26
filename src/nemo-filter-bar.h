/* nemo-filter-bar.h
 *
 * Copyright (C) 2026 Linux Mint
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NEMO_FILTER_BAR_H
#define NEMO_FILTER_BAR_H

#include <gtk/gtk.h>

#define NEMO_TYPE_FILTER_BAR nemo_filter_bar_get_type ()

G_DECLARE_FINAL_TYPE (NemoFilterBar, nemo_filter_bar, NEMO, FILTER_BAR, GtkBox)

GtkWidget  *nemo_filter_bar_new             (void);
const char *nemo_filter_bar_get_text        (NemoFilterBar *bar);
void        nemo_filter_bar_set_text        (NemoFilterBar *bar,
                                             const char    *text);

#endif /* NEMO_FILTER_BAR_H */

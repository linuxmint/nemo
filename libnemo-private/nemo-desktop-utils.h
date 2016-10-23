/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 */

#ifndef NEMO_DESKTOP_UTILS_H
#define NEMO_DESKTOP_UTILS_H

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void nemo_desktop_utils_get_monitor_work_rect (gint num, GdkRectangle *rect);
void nemo_desktop_utils_get_monitor_geometry (gint num, GdkRectangle *rect);
gint nemo_desktop_utils_get_primary_monitor (void);
gint nemo_desktop_utils_get_monitor_for_widget (GtkWidget *widget);
gboolean nemo_desktop_utils_get_monitor_cloned (gint monitor, gint x_primary);

G_END_DECLS

#endif

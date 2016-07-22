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

#include "nemo-desktop-utils.h"

static GdkScreen *default_screen = NULL;

static void
ensure_screen (void)
{
    if (!default_screen)
        default_screen = gdk_screen_get_default ();
}

void
nemo_desktop_utils_get_monitor_work_rect (gint monitor, GdkRectangle *rect)
{
    ensure_screen ();

    g_return_if_fail (monitor >= 0 && monitor < gdk_screen_get_n_monitors (default_screen));

    gdk_screen_get_monitor_workarea (default_screen, monitor, rect);
}

gint
nemo_desktop_utils_get_monitor_for_widget (GtkWidget *widget)
{
    ensure_screen ();

    g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

    GdkWindow *window = gtk_widget_get_window (widget);

    if (window == NULL)
        return 0;

    gint monitor = gdk_screen_get_monitor_at_window (default_screen, window);

    return monitor;
}

gboolean
nemo_desktop_utils_get_monitor_cloned (gint monitor, gint x_primary)
{
    GdkRectangle rect_primary;
    GdkRectangle rect_test;

    ensure_screen ();

    gint n_monitors = gdk_screen_get_n_monitors (default_screen);

	g_return_val_if_fail (monitor >= 0 && monitor < n_monitors, FALSE);
	g_return_val_if_fail (x_primary >= 0 && x_primary < n_monitors, FALSE);

    gdk_screen_get_monitor_geometry(default_screen, x_primary, &rect_primary);
    gdk_screen_get_monitor_geometry(default_screen, monitor, &rect_test);

    if (rect_primary.x == rect_test.x &&
    		rect_primary.y == rect_test.y) {
    	return TRUE;
    }
    return FALSE;
}



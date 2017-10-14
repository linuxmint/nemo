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

#if GTK_CHECK_VERSION (3, 22, 0)
static GdkDisplay *default_display = NULL;

static void
ensure_display (void)
{
    if (!default_display)
        default_display = gdk_display_get_default ();
}
#endif

void
nemo_desktop_utils_get_monitor_work_rect (gint num, GdkRectangle *rect)
{
#if GTK_CHECK_VERSION (3, 22, 0)
    ensure_display ();
    g_return_if_fail (num >= 0 && num < gdk_display_get_n_monitors (default_display));

    GdkMonitor *monitor = gdk_display_get_monitor (default_display, num);

    gdk_monitor_get_workarea (monitor, rect);
#else
    ensure_screen ();
    g_return_if_fail (num >= 0 && num < gdk_screen_get_n_monitors (default_screen));
    gdk_screen_get_monitor_workarea (default_screen, num, rect);
#endif
}

void
nemo_desktop_utils_get_monitor_geometry (gint num, GdkRectangle *rect)
{
#if GTK_CHECK_VERSION (3, 22, 0)
    ensure_display ();
    g_return_if_fail (num >= 0 && num < gdk_display_get_n_monitors (default_display));

    GdkMonitor *monitor = gdk_display_get_monitor (default_display, num);

    gdk_monitor_get_geometry (monitor, rect);
#else
    ensure_screen ();
    g_return_if_fail (num >= 0 && num < gdk_screen_get_n_monitors (default_screen));

    gdk_screen_get_monitor_geometry (default_screen, num, rect);
#endif
}

gint
nemo_desktop_utils_get_primary_monitor (void)
{
#if GTK_CHECK_VERSION (3, 22, 0)
    gint n_mon, i;

    ensure_display ();

    n_mon = gdk_display_get_n_monitors (default_display);

    for (i = 0; i < n_mon; i ++) {
        GdkMonitor *monitor = gdk_display_get_monitor (default_display, i);
        if (gdk_monitor_is_primary (monitor)) {
            return i;
        }
    }
    return 0;
#else
    ensure_screen ();

    return gdk_screen_get_primary_monitor (default_screen);
#endif
}

gint
nemo_desktop_utils_get_monitor_for_widget (GtkWidget *widget)
{
    GdkWindow *window;
    GtkWidget *toplevel;
    gint monitor;

    g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

    toplevel = gtk_widget_get_toplevel (widget);

    if (toplevel != NULL &&
        g_object_get_data (G_OBJECT (toplevel), "is_desktop_window")) {
        monitor = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (toplevel), "monitor_number"));
        return monitor;
    }

    ensure_screen ();

    window = gtk_widget_get_window (widget);

    if (window == NULL) {
        return 0;
    }

    monitor = gdk_screen_get_monitor_at_window (default_screen, window);

    return monitor;
}

gint
nemo_desktop_utils_get_num_monitors (void)
{
    ensure_screen ();

    return gdk_screen_get_n_monitors (default_screen);
}

gboolean
nemo_desktop_utils_get_monitor_cloned (gint monitor, gint x_primary)
{
    GdkRectangle rect_primary;
    GdkRectangle rect_test;
    gint n_monitors;

    ensure_screen ();

    n_monitors = gdk_screen_get_n_monitors (default_screen);

	g_return_val_if_fail (monitor >= 0 && monitor < n_monitors, FALSE);
	g_return_val_if_fail (x_primary >= 0 && x_primary < n_monitors, FALSE);

    gdk_screen_get_monitor_geometry(default_screen, x_primary, &rect_primary);
    gdk_screen_get_monitor_geometry(default_screen, monitor, &rect_test);

    if (rect_primary.x == rect_test.x && rect_primary.y == rect_test.y) {
        return TRUE;
    }

    return FALSE;
}



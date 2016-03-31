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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Daniel Schürmann <daschuer@gmx.de>
 */

#include "nemo-background-window.h"

#include <X11/Xatom.h>
#include <gdk/gdkx.h>

#include <libnemo-private/nemo-desktop-background.h>

struct NemoBackgroundWindowDetails {
	gulong size_changed_id;
	NemoDesktopBackground *background;
};

G_DEFINE_TYPE (NemoBackgroundWindow, nemo_background_window, 
	       GTK_TYPE_WINDOW);

static void
nemo_background_window_screen_size_changed (GdkScreen *screen,
					     NemoBackgroundWindow *window)
{
	int width_request, height_request;

	width_request = gdk_screen_get_width (screen);
	height_request = gdk_screen_get_height (screen);

	g_object_set (window,
		      "width_request", width_request,
		      "height_request", height_request,
		      NULL);
}

static void
nemo_background_window_constructed (GObject *obj)
{
	NemoBackgroundWindow *window = NEMO_BACKGROUND_WINDOW (obj);

	G_OBJECT_CLASS (nemo_background_window_parent_class)->constructed (obj);

	GdkScreen *screen = gdk_screen_get_default ();
	nemo_background_window_screen_size_changed(screen, window);
	gtk_window_move (GTK_WINDOW (window), 0, 0);


	/* shouldn't really be needed given our semantic type
	 * of _NET_WM_TYPE_DESKTOP, but why not
	 */
	gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
	gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
	gtk_window_set_type_hint(GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DESKTOP);
	g_object_set_data (G_OBJECT (window), "is_desktop_window", 
			   GINT_TO_POINTER (1));

	window->details->background = nemo_desktop_background_new (GTK_WIDGET (window));
	gtk_widget_show (GTK_WIDGET (window));

 	/* We realize it immediately so that the NEMO_DESKTOP_WINDOW_ID
 	 * property is set so gnome-settings-daemon doesn't try to set
	 * background. And we do a gdk_flush() to be sure X gets it.
	 */
 	gtk_widget_realize (GTK_WIDGET (window));
	gdk_flush ();
}

static void
nemo_background_window_init (NemoBackgroundWindow *window)
{
	window->details = G_TYPE_INSTANCE_GET_PRIVATE (window, NEMO_TYPE_BACKGROUND_WINDOW,
						       NemoBackgroundWindowDetails);
}

static NemoBackgroundWindow *
nemo_background_window_new (void)
{
	NemoBackgroundWindow *window;

	window = g_object_new (NEMO_TYPE_BACKGROUND_WINDOW,
            "accept-focus", FALSE,
            "app-paintable", TRUE,
            "skip-pager-hint", TRUE,
            "skip-taskbar-hint", TRUE,
            "type", GTK_WINDOW_TOPLEVEL,
			NULL);

	return window;
}

static NemoBackgroundWindow *the_background_window = NULL;

void
nemo_background_window_ensure (void)
{
	NemoBackgroundWindow *window;

	if (!the_background_window) {
		window = nemo_background_window_new ();
		g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &the_background_window);
		the_background_window = window;
	}
}

GtkWidget *
nemo_background_window_get (void)
{
	return GTK_WIDGET (the_background_window);
}

static gboolean
nemo_background_window_delete_event (GtkWidget *widget,
				      GdkEventAny *event)
{
	/* Returning true tells GTK+ not to delete the window. */
	return TRUE;
}

static void
map (GtkWidget *widget)
{
	/* Chain up to realize our children */
	GTK_WIDGET_CLASS (nemo_background_window_parent_class)->map (widget);
	gdk_window_lower (gtk_widget_get_window (widget));

    GdkWindow *window = gtk_widget_get_window (widget);
    GdkRGBA transparent = { 0, 0, 0, 0 };
    gdk_window_set_background_rgba (window, &transparent);
}

static void
unrealize (GtkWidget *widget)
{
	NemoBackgroundWindow *window;
	NemoBackgroundWindowDetails *details;

	window = NEMO_BACKGROUND_WINDOW (widget);
	details = window->details;

	if (details->size_changed_id != 0) {
		g_signal_handler_disconnect (gtk_window_get_screen (GTK_WINDOW (window)),
					     details->size_changed_id);
		details->size_changed_id = 0;
	}

	if (window->details->background != NULL) {
		g_object_unref (window->details->background);
		window->details->background = NULL;
	}

	GTK_WIDGET_CLASS (nemo_background_window_parent_class)->unrealize (widget);
}

static void
set_wmspec_desktop_hint (GdkWindow *window)
{
	GdkAtom atom;

	atom = gdk_atom_intern ("_NET_WM_WINDOW_TYPE_DESKTOP", FALSE);
        
	gdk_property_change (window,
			     gdk_atom_intern ("_NET_WM_WINDOW_TYPE", FALSE),
			     gdk_x11_xatom_to_atom (XA_ATOM), 32,
			     GDK_PROP_MODE_REPLACE, (guchar *) &atom, 1);
}

static void
realize (GtkWidget *widget)
{
	NemoBackgroundWindow *window;
	GdkVisual *visual;

	window = NEMO_BACKGROUND_WINDOW (widget);

	visual = gdk_screen_get_rgba_visual (gtk_widget_get_screen (widget));
	if (visual) {
		gtk_widget_set_visual (widget, visual);
	}

	/* Make sure we get keyboard events */
	gtk_widget_set_events (widget, gtk_widget_get_events (widget) 
			      | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

	/* Do the work of realizing. */
	GTK_WIDGET_CLASS (nemo_background_window_parent_class)->realize (widget);

	/* This is the new way to set up the desktop window */
	set_wmspec_desktop_hint (gtk_widget_get_window (widget));

	window->details->size_changed_id =
		g_signal_connect (gtk_window_get_screen (GTK_WINDOW (window)), "size-changed",
				  G_CALLBACK (nemo_background_window_screen_size_changed), window);
}

static void
nemo_background_window_class_init (NemoBackgroundWindowClass *klass)
{
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	oclass->constructed = nemo_background_window_constructed;

	wclass->realize = realize;
	wclass->unrealize = unrealize;
	wclass->map = map;
	wclass->delete_event = nemo_background_window_delete_event;

	g_type_class_add_private (klass, sizeof (NemoBackgroundWindowDetails));
}

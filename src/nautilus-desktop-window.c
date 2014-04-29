/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "nautilus-desktop-window.h"
#include "nautilus-window-private.h"
#include "nautilus-actions.h"

#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include <eel/eel-vfs-extensions.h>
#include <libnautilus-private/nautilus-desktop-link-monitor.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-icon-names.h>
#include <libnautilus-private/nautilus-global-preferences.h>

struct NautilusDesktopWindowDetails {
	gulong size_changed_id;

	gboolean loaded;

	GtkWidget *desktop_selection;
};

G_DEFINE_TYPE (NautilusDesktopWindow, nautilus_desktop_window, 
	       NAUTILUS_TYPE_WINDOW);

static void
nautilus_desktop_window_update_directory (NautilusDesktopWindow *window)
{
	GFile *location;

	g_assert (NAUTILUS_IS_DESKTOP_WINDOW (window));

	window->details->loaded = FALSE;
	location = g_file_new_for_uri (EEL_DESKTOP_URI);
	nautilus_window_go_to (NAUTILUS_WINDOW (window), location);
	window->details->loaded = TRUE;

	g_object_unref (location);
}

static void
nautilus_desktop_window_finalize (GObject *obj)
{
	nautilus_desktop_link_monitor_shutdown ();

	G_OBJECT_CLASS (nautilus_desktop_window_parent_class)->finalize (obj);
}

static void
nautilus_desktop_window_init_actions (NautilusDesktopWindow *window)
{
	GtkAction *action;
	GtkActionGroup *action_group;

	action_group = nautilus_window_get_main_action_group (NAUTILUS_WINDOW (window));

	/* Don't allow close action on desktop */
	action = gtk_action_group_get_action (action_group,
					      NAUTILUS_ACTION_CLOSE);
	gtk_action_set_sensitive (action, FALSE);

	/* Don't allow new tab on desktop */
	action = gtk_action_group_get_action (action_group,
					      NAUTILUS_ACTION_NEW_TAB);
	gtk_action_set_sensitive (action, FALSE);

	/* Don't allow search on desktop */
	action = gtk_action_group_get_action (action_group,
					      NAUTILUS_ACTION_SEARCH);
	gtk_action_set_sensitive (action, FALSE);
}

static void
selection_get_cb (GtkWidget          *widget,
		  GtkSelectionData   *selection_data,
		  guint               info,
		  guint               time)
{
	/* No extra targets atm */
}

static gboolean
selection_clear_event_cb (GtkWidget	        *widget,
			  GdkEventSelection     *event,
			  NautilusDesktopWindow *window)
{
	gtk_widget_destroy (GTK_WIDGET (window));

	return TRUE;
}

static void
nautilus_desktop_window_init_selection (NautilusDesktopWindow *window)
{
	char selection_name[32];
	GdkAtom selection_atom;
	Window selection_owner;
	GdkDisplay *display;
	GtkWidget *selection_widget;
	GdkScreen *screen;

	screen = gdk_screen_get_default ();

	g_snprintf (selection_name, sizeof (selection_name),
		    "_NET_DESKTOP_MANAGER_S%d", gdk_screen_get_number (screen));
	selection_atom = gdk_atom_intern (selection_name, FALSE);
	display = gdk_screen_get_display (screen);

	selection_owner = XGetSelectionOwner (GDK_DISPLAY_XDISPLAY (display),
					      gdk_x11_atom_to_xatom_for_display (display,
										 selection_atom));
	if (selection_owner != None) {
		g_critical ("Another desktop manager in use; desktop window won't be created");
		return;
	}

	selection_widget = gtk_invisible_new_for_screen (screen);
	/* We need this for gdk_x11_get_server_time() */
	gtk_widget_add_events (selection_widget, GDK_PROPERTY_CHANGE_MASK);

	if (!gtk_selection_owner_set_for_display (display,
						  selection_widget,
						  selection_atom,
						  gdk_x11_get_server_time (gtk_widget_get_window (selection_widget)))) {
		gtk_widget_destroy (selection_widget);
		g_critical ("Can't set ourselves as selection owner for desktop manager; "
			    "desktop window won't be created");
		return;
	}

	g_signal_connect (selection_widget, "selection-get",
			  G_CALLBACK (selection_get_cb), window);
	g_signal_connect (selection_widget, "selection-clear-event",
			  G_CALLBACK (selection_clear_event_cb), window);

	window->details->desktop_selection = selection_widget;
}

static void
nautilus_desktop_window_constructed (GObject *obj)
{
	AtkObject *accessible;
	NautilusDesktopWindow *window = NAUTILUS_DESKTOP_WINDOW (obj);
        GdkRGBA transparent = {0, 0, 0, 0};

	G_OBJECT_CLASS (nautilus_desktop_window_parent_class)->constructed (obj);

	/* Initialize the desktop link monitor singleton */
	nautilus_desktop_link_monitor_get ();

	gtk_window_move (GTK_WINDOW (window), 0, 0);

	/* shouldn't really be needed given our semantic type
	 * of _NET_WM_TYPE_DESKTOP, but why not
	 */
	gtk_window_set_resizable (GTK_WINDOW (window),
				  FALSE);
	gtk_window_set_decorated (GTK_WINDOW (window),
				  FALSE);

	g_object_set_data (G_OBJECT (window), "is_desktop_window", 
			   GINT_TO_POINTER (1));

	nautilus_desktop_window_init_selection (window);
	nautilus_desktop_window_init_actions (window);

	/* Set the accessible name so that it doesn't inherit the cryptic desktop URI. */
	accessible = gtk_widget_get_accessible (GTK_WIDGET (window));

	if (accessible) {
		atk_object_set_name (accessible, _("Desktop"));
	}

	/* Special sawmill setting */
	gtk_window_set_wmclass (GTK_WINDOW (window), "desktop_window", "Nautilus");

	/* Point window at the desktop folder.
	 * Note that nautilus_desktop_window_init is too early to do this.
	 */
	nautilus_desktop_window_update_directory (window);
        gtk_widget_override_background_color (GTK_WIDGET (window), 0, &transparent);

	/* We realize it immediately so that the NAUTILUS_DESKTOP_WINDOW_ID
	 * property is set so gnome-settings-daemon doesn't try to set
	 * background. And we do a gdk_flush() to be sure X gets it.
	 */
	gtk_widget_realize (GTK_WIDGET (window));
	gdk_flush ();

}

static void
nautilus_desktop_window_init (NautilusDesktopWindow *window)
{
	window->details = G_TYPE_INSTANCE_GET_PRIVATE (window, NAUTILUS_TYPE_DESKTOP_WINDOW,
						       NautilusDesktopWindowDetails);
}

static void
nautilus_desktop_window_screen_size_changed (GdkScreen             *screen,
					     NautilusDesktopWindow *window)
{
	int width_request, height_request;

	width_request = gdk_screen_get_width (screen);
	height_request = gdk_screen_get_height (screen);
	
	g_object_set (window,
		      "width_request", width_request,
		      "height_request", height_request,
		      NULL);
}

static NautilusDesktopWindow *
nautilus_desktop_window_new (void)
{
	GdkScreen *screen;
	GApplication *application;
	NautilusDesktopWindow *window;
	int width_request, height_request;

	application = g_application_get_default ();
	screen = gdk_screen_get_default ();
	width_request = gdk_screen_get_width (screen);
	height_request = gdk_screen_get_height (screen);

	window = g_object_new (NAUTILUS_TYPE_DESKTOP_WINDOW,
			       "application", application,
			       "disable-chrome", TRUE,
			       "width_request", width_request,
			       "height_request", height_request,
			       NULL);

	return window;
}

static NautilusDesktopWindow *the_desktop_window = NULL;

void
nautilus_desktop_window_ensure (void)
{
	NautilusDesktopWindow *window;

	if (!the_desktop_window) {
		window = nautilus_desktop_window_new ();
		g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &the_desktop_window);
		the_desktop_window = window;
	}
}

GtkWidget *
nautilus_desktop_window_get (void)
{
	return GTK_WIDGET (the_desktop_window);
}

static gboolean
nautilus_desktop_window_delete_event (GtkWidget *widget,
				      GdkEventAny *event)
{
	/* Returning true tells GTK+ not to delete the window. */
	return TRUE;
}

static void
map (GtkWidget *widget)
{
	/* Chain up to realize our children */
	GTK_WIDGET_CLASS (nautilus_desktop_window_parent_class)->map (widget);
	gdk_window_lower (gtk_widget_get_window (widget));
}

static void
unrealize (GtkWidget *widget)
{
	NautilusDesktopWindow *window;
	NautilusDesktopWindowDetails *details;

	window = NAUTILUS_DESKTOP_WINDOW (widget);
	details = window->details;

	if (details->size_changed_id != 0) {
		g_signal_handler_disconnect (gtk_window_get_screen (GTK_WINDOW (window)),
					     details->size_changed_id);
		details->size_changed_id = 0;
	}

	gtk_widget_destroy (details->desktop_selection);

	GTK_WIDGET_CLASS (nautilus_desktop_window_parent_class)->unrealize (widget);
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
	NautilusDesktopWindow *window;
	NautilusDesktopWindowDetails *details;
	GdkVisual *visual;

	window = NAUTILUS_DESKTOP_WINDOW (widget);
	details = window->details;

	/* Make sure we get keyboard events */
	gtk_widget_set_events (widget, gtk_widget_get_events (widget) 
			      | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
			      
	visual = gdk_screen_get_rgba_visual (gtk_widget_get_screen (widget));
	if (visual) {
		gtk_widget_set_visual (widget, visual);
	}

	/* Do the work of realizing. */
	GTK_WIDGET_CLASS (nautilus_desktop_window_parent_class)->realize (widget);

	/* This is the new way to set up the desktop window */
	set_wmspec_desktop_hint (gtk_widget_get_window (widget));

	details->size_changed_id =
		g_signal_connect (gtk_window_get_screen (GTK_WINDOW (window)), "size-changed",
				  G_CALLBACK (nautilus_desktop_window_screen_size_changed), window);
}

static void
real_sync_title (NautilusWindow *window,
		 NautilusWindowSlot *slot)
{
	/* hardcode "Desktop" */
	gtk_window_set_title (GTK_WINDOW (window), _("Desktop"));
}

static void
real_window_close (NautilusWindow *window)
{
	/* stub, does nothing */
	return;
}

static void
nautilus_desktop_window_class_init (NautilusDesktopWindowClass *klass)
{
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
	NautilusWindowClass *nclass = NAUTILUS_WINDOW_CLASS (klass);
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	oclass->constructed = nautilus_desktop_window_constructed;
	oclass->finalize = nautilus_desktop_window_finalize;

	wclass->realize = realize;
	wclass->unrealize = unrealize;
	wclass->map = map;
	wclass->delete_event = nautilus_desktop_window_delete_event;

	nclass->sync_title = real_sync_title;
	nclass->close = real_window_close;

	g_type_class_add_private (klass, sizeof (NautilusDesktopWindowDetails));
}

gboolean
nautilus_desktop_window_loaded (NautilusDesktopWindow *window)
{
	return window->details->loaded;
}

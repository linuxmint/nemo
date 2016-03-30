/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 2000 Eazel, Inc.
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
 * Authors: Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "nemo-desktop-window.h"
#include "nemo-window-private.h"
#include "nemo-actions.h"

#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include <eel/eel-vfs-extensions.h>
#include <libnemo-private/nemo-desktop-link-monitor.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-icon-names.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-desktop-utils.h>

enum {
    PROP_MONITOR = 1,
    NUM_PROPERTIES
};

struct NemoDesktopWindowDetails {
	gulong size_changed_id;
    gint monitor;
	gboolean loaded;

	GtkWidget *desktop_selection;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NemoDesktopWindow, nemo_desktop_window, 
	       NEMO_TYPE_WINDOW);

static void
nemo_desktop_window_update_directory (NemoDesktopWindow *window)
{
	GFile *location;

	g_assert (NEMO_IS_DESKTOP_WINDOW (window));

	window->details->loaded = FALSE;
	location = g_file_new_for_uri (EEL_DESKTOP_URI);
	nemo_window_go_to (NEMO_WINDOW (window), location);
	window->details->loaded = TRUE;

	g_object_unref (location);
}

static void
nemo_desktop_window_finalize (GObject *obj)
{
	nemo_desktop_link_monitor_shutdown ();

	G_OBJECT_CLASS (nemo_desktop_window_parent_class)->finalize (obj);
}

static void
nemo_desktop_window_init_actions (NemoDesktopWindow *window)
{
	GtkAction *action;
	GtkActionGroup *action_group;

	action_group = nemo_window_get_main_action_group (NEMO_WINDOW (window));

	/* Don't allow close action on desktop */
	action = gtk_action_group_get_action (action_group,
					      NEMO_ACTION_CLOSE);
	gtk_action_set_sensitive (action, FALSE);

	/* Don't allow new tab on desktop */
	action = gtk_action_group_get_action (action_group,
					      NEMO_ACTION_NEW_TAB);
	gtk_action_set_sensitive (action, FALSE);

	/* Don't allow search on desktop */
	action = gtk_action_group_get_action (action_group,
					      NEMO_ACTION_SEARCH);
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
			  NemoDesktopWindow *window)
{
	gtk_widget_destroy (GTK_WIDGET (window));

	return TRUE;
}

static void
nemo_desktop_window_init_selection (NemoDesktopWindow *window)
{
	char selection_name[32];
	GdkAtom selection_atom;
	Window selection_owner;
	GdkDisplay *display;
	GtkWidget *selection_widget;
	GdkScreen *screen;

	window->details->desktop_selection = NULL; 
 
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
nemo_desktop_window_constructed (GObject *obj)
{
	AtkObject *accessible;
	NemoDesktopWindow *window = NEMO_DESKTOP_WINDOW (obj);
	GdkRGBA transparent = {0, 0, 0, 0};

    gtk_widget_override_background_color (GTK_WIDGET (window), 0, &transparent);

	G_OBJECT_CLASS (nemo_desktop_window_parent_class)->constructed (obj);

	NemoWindow *nwindow = NEMO_WINDOW (obj);
	gtk_widget_hide (nwindow->details->statusbar);
	gtk_widget_hide (nwindow->details->menubar);

	/* Initialize the desktop link monitor singleton */
	nemo_desktop_link_monitor_get ();

    GdkRectangle rect;

    nemo_desktop_utils_get_monitor_work_rect (window->details->monitor, &rect);

    g_printerr ("NemoDesktopWindow monitor:%d: x:%d, y:%d, w:%d, h:%d\n", window->details->monitor, rect.x, rect.y, rect.width, rect.height);

    gtk_window_move (GTK_WINDOW (window), rect.x, rect.y);
    gtk_widget_set_size_request (GTK_WIDGET (window), rect.width, rect.height);

    gtk_window_set_resizable (GTK_WINDOW (window),
                  FALSE);
	gtk_window_set_decorated (GTK_WINDOW (window),
				  FALSE);

	g_object_set_data (G_OBJECT (window), "is_desktop_window", 
			   GINT_TO_POINTER (1));

	nemo_desktop_window_init_selection (window);
	nemo_desktop_window_init_actions (window);

	/* Set the accessible name so that it doesn't inherit the cryptic desktop URI. */
	accessible = gtk_widget_get_accessible (GTK_WIDGET (window));

	if (accessible) {
		atk_object_set_name (accessible, _("Desktop"));
	}

	/* Special sawmill setting */
	gtk_window_set_wmclass (GTK_WINDOW (window), "desktop_window", "Nemo");

	nemo_desktop_window_update_directory (window);
}

static void
nemo_desktop_window_get_property (GObject *object,
                                     guint property_id,
                                   GValue *value,
                               GParamSpec *pspec)
{
    NemoDesktopWindow *window = NEMO_DESKTOP_WINDOW (object);

    switch (property_id) {
    case PROP_MONITOR:
        g_value_set_int (value, window->details->monitor);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
nemo_desktop_window_set_property (GObject *object,
                                     guint property_id,
                             const GValue *value,
                               GParamSpec *pspec)
{
    NemoDesktopWindow *window = NEMO_DESKTOP_WINDOW (object);

    switch (property_id) {
    case PROP_MONITOR:
        window->details->monitor = g_value_get_int (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
nemo_desktop_window_init (NemoDesktopWindow *window)
{
	window->details = G_TYPE_INSTANCE_GET_PRIVATE (window, NEMO_TYPE_DESKTOP_WINDOW,
						       NemoDesktopWindowDetails);
}

NemoDesktopWindow *
nemo_desktop_window_new (gint monitor)
{
	GApplication *application;
	NemoDesktopWindow *window;

	application = g_application_get_default ();

    window = g_object_new (NEMO_TYPE_DESKTOP_WINDOW,
			               "application", application,                           
                           "disable-chrome", TRUE,
                           "monitor", monitor,
                           NULL);
	return window;
}

static gboolean
nemo_desktop_window_delete_event (GtkWidget *widget,
				      GdkEventAny *event)
{
	/* Returning true tells GTK+ not to delete the window. */
	return TRUE;
}

static void
map (GtkWidget *widget)
{
	/* Chain up to realize our children */
	GTK_WIDGET_CLASS (nemo_desktop_window_parent_class)->map (widget);
	gdk_window_lower (gtk_widget_get_window (widget));

    GdkWindow *window;
    GdkRGBA transparent = { 0, 0, 0, 0 };

    window = gtk_widget_get_window (widget);
    gdk_window_set_background_rgba (window, &transparent);
}

static void
unrealize (GtkWidget *widget)
{
	NemoDesktopWindow *window;
	NemoDesktopWindowDetails *details;

	window = NEMO_DESKTOP_WINDOW (widget);
	details = window->details;

	if (window->details->desktop_selection) { 
		gtk_widget_destroy (details->desktop_selection);
	}

	GTK_WIDGET_CLASS (nemo_desktop_window_parent_class)->unrealize (widget);
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
	GdkVisual *visual;

	/* Make sure we get keyboard events */
	gtk_widget_set_events (widget, gtk_widget_get_events (widget) 
			      | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

    visual = gdk_screen_get_rgba_visual (gtk_widget_get_screen (widget));
    if (visual) {
        gtk_widget_set_visual (widget, visual);
    }

	/* Do the work of realizing. */
	GTK_WIDGET_CLASS (nemo_desktop_window_parent_class)->realize (widget);

	/* This is the new way to set up the desktop window */
	set_wmspec_desktop_hint (gtk_widget_get_window (widget));
}

static void
real_sync_title (NemoWindow *window,
		 NemoWindowSlot *slot)
{
	/* hardcode "Desktop" */
	gtk_window_set_title (GTK_WINDOW (window), _("Desktop"));
}

static void
real_window_close (NemoWindow *window)
{
	/* stub, does nothing */
	return;
}

static void
nemo_desktop_window_class_init (NemoDesktopWindowClass *klass)
{
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
	NemoWindowClass *nclass = NEMO_WINDOW_CLASS (klass);
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	oclass->constructed = nemo_desktop_window_constructed;
	oclass->finalize = nemo_desktop_window_finalize;
    oclass->set_property = nemo_desktop_window_set_property;
    oclass->get_property = nemo_desktop_window_get_property;

	wclass->realize = realize;
	wclass->unrealize = unrealize;
	wclass->map = map;
	wclass->delete_event = nemo_desktop_window_delete_event;

	nclass->sync_title = real_sync_title;
	nclass->close = real_window_close;

    properties[PROP_MONITOR] =
        g_param_spec_int ("monitor",
                          "Monitor number",
                          "The monitor number this window is assigned to",
                          G_MININT, G_MAXINT, 0,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_type_class_add_private (klass, sizeof (NemoDesktopWindowDetails));
    g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

gboolean
nemo_desktop_window_loaded (NemoDesktopWindow *window)
{
	return window->details->loaded;
}

gint
nemo_desktop_window_get_monitor (NemoDesktopWindow *window)
{
    return window->details->monitor;
}

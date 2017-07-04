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

#include <config.h>
#include "nemo-blank-desktop-window.h"

#include <X11/Xatom.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include <libnemo-private/nemo-desktop-utils.h>
#include <libnemo-private/nemo-action.h>
#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-file-utilities.h>

#include <eel/eel-gtk-extensions.h>

#include "nemo-plugin-manager.h"

#define DEBUG_FLAG NEMO_DEBUG_DESKTOP
#include <libnemo-private/nemo-debug.h>

enum {
    PROP_MONITOR = 1,
    NUM_PROPERTIES
};

enum {
    PLUGIN_MANAGER,
    LAST_SIGNAL
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

struct NemoBlankDesktopWindowDetails {
    gint monitor;
    GtkWidget *popup_menu;
    gulong actions_changed_id;
};

G_DEFINE_TYPE (NemoBlankDesktopWindow, nemo_blank_desktop_window, 
               GTK_TYPE_WINDOW);

static void
action_activated_callback (GtkMenuItem *item, NemoAction *action)
{
    GFile *desktop_location = nemo_get_desktop_location ();
    NemoFile *desktop_file = nemo_file_get (desktop_location);
    g_object_unref (desktop_location);

    nemo_action_activate (NEMO_ACTION (action), NULL, desktop_file);
}

static void
actions_changed_cb (NemoBlankDesktopWindow *window)
{
    g_clear_pointer (&window->details->popup_menu, gtk_widget_destroy);
}

static void
build_menu (NemoBlankDesktopWindow *window)
{
    if (window->details->popup_menu) {
        return;
    }

    NemoActionManager *desktop_action_manager = nemo_desktop_manager_get_action_manager ();

    if (window->details->actions_changed_id == 0) {
        window->details->actions_changed_id = g_signal_connect_swapped (desktop_action_manager,
                                                                        "changed",
                                                                        G_CALLBACK (actions_changed_cb),
                                                                        window);
    }

    GList *action_list = nemo_action_manager_list_actions (desktop_action_manager);

    if (g_list_length (action_list) == 0)
        return;

    window->details->popup_menu = gtk_menu_new ();

    gboolean show;
    g_object_get (gtk_settings_get_default (), "gtk-menu-images", &show, NULL);

    gtk_menu_attach_to_widget (GTK_MENU (window->details->popup_menu),
                               GTK_WIDGET (window),
                               NULL);

    GtkWidget *item;
    GList *l;
    NemoAction *action;

    for (l = action_list; l != NULL; l = l->next) {
        action = l->data;

        if (action->show_in_blank_desktop && action->dbus_satisfied) {
            gchar *label = nemo_action_get_label (action, NULL, NULL);
            item = gtk_image_menu_item_new_with_mnemonic (label);
            g_free (label);

            const gchar *stock_id = gtk_action_get_stock_id (GTK_ACTION (action));
            const gchar *icon_name = gtk_action_get_icon_name (GTK_ACTION (action));

            if (stock_id || icon_name) {
                GtkWidget *image = stock_id ? gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU) :
                                              gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);

                gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
                gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (item), show);
            }

            gtk_widget_set_visible (item, TRUE);
            g_signal_connect (item, "activate", G_CALLBACK (action_activated_callback), action);
            gtk_menu_shell_append (GTK_MENU_SHELL (window->details->popup_menu), item);
        }
    }
}

static void
do_popup_menu (NemoBlankDesktopWindow *window, GdkEventButton *event)
{
    build_menu (window);
    eel_pop_up_context_menu (GTK_MENU(window->details->popup_menu),
                             event);
}

static gboolean
on_popup_menu (GtkWidget *widget, NemoBlankDesktopWindow *window)
{
    do_popup_menu (window, NULL);
    return TRUE;
}

static gboolean
on_button_press (GtkWidget *widget, GdkEventButton *event, NemoBlankDesktopWindow *window)
{
    if (event->type != GDK_BUTTON_PRESS) {
        /* ignore multiple clicks */
        return TRUE;
    }

    if (event->button == 3) {
        do_popup_menu (window, event);
    }

    return FALSE;
}

static void
nemo_blank_desktop_window_dispose (GObject *obj)
{
    NemoBlankDesktopWindow *window = NEMO_BLANK_DESKTOP_WINDOW (obj);

    if (window->details->actions_changed_id > 0) {
        g_signal_handler_disconnect (nemo_desktop_manager_get_action_manager (),
                             window->details->actions_changed_id);
        window->details->actions_changed_id = 0;
    }

    G_OBJECT_CLASS (nemo_blank_desktop_window_parent_class)->dispose (obj);
}

static void
nemo_blank_desktop_window_finalize (GObject *obj)
{
    G_OBJECT_CLASS (nemo_blank_desktop_window_parent_class)->finalize (obj);
}

static void
nemo_blank_desktop_window_constructed (GObject *obj)
{
	AtkObject *accessible;
	NemoBlankDesktopWindow *window = NEMO_BLANK_DESKTOP_WINDOW (obj);

	G_OBJECT_CLASS (nemo_blank_desktop_window_parent_class)->constructed (obj);

	/* Set the accessible name so that it doesn't inherit the cryptic desktop URI. */
	accessible = gtk_widget_get_accessible (GTK_WIDGET (window));

	if (accessible) {
		atk_object_set_name (accessible, _("Desktop"));
	}

    GdkRectangle rect;

    nemo_desktop_utils_get_monitor_geometry (window->details->monitor, &rect);

    DEBUG ("NemoBlankDesktopWindow monitor:%d: x:%d, y:%d, w:%d, h:%d",
           window->details->monitor,
           rect.x, rect.y,
           rect.width, rect.height);

    gtk_window_move (GTK_WINDOW (window), rect.x, rect.y);
    gtk_widget_set_size_request (GTK_WIDGET (window), rect.width, rect.height);

    gtk_window_set_resizable (GTK_WINDOW (window),
                  FALSE);

    gtk_widget_show_all (GTK_WIDGET (window));

    g_signal_connect (GTK_WIDGET (window), "button-press-event", G_CALLBACK (on_button_press), window);
    g_signal_connect (GTK_WIDGET (window), "popup-menu", G_CALLBACK (on_popup_menu), window);
}

static void
nemo_blank_desktop_window_get_property (GObject *object,
                                           guint property_id,
                                         GValue *value,
                                     GParamSpec *pspec)
{
    NemoBlankDesktopWindow *window = NEMO_BLANK_DESKTOP_WINDOW (object);

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
nemo_blank_desktop_window_set_property (GObject *object,
                                           guint property_id,
                                   const GValue *value,
                                     GParamSpec *pspec)
{
    NemoBlankDesktopWindow *window = NEMO_BLANK_DESKTOP_WINDOW (object);

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
nemo_blank_desktop_window_init (NemoBlankDesktopWindow *window)
{
	window->details = G_TYPE_INSTANCE_GET_PRIVATE (window, NEMO_TYPE_BLANK_DESKTOP_WINDOW,
						       NemoBlankDesktopWindowDetails);

    window->details->popup_menu = NULL;
    window->details->actions_changed_id = 0;

    /* Make it easier for themes authors to style the desktop window separately */
    gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (window)), "nemo-desktop-window");
}

NemoBlankDesktopWindow *
nemo_blank_desktop_window_new (gint monitor)
{
	NemoBlankDesktopWindow *window;

	window = g_object_new (NEMO_TYPE_BLANK_DESKTOP_WINDOW,
                           "monitor", monitor,
                           NULL);

    GdkRGBA transparent = {0, 0, 0, 0};
    gtk_widget_override_background_color (GTK_WIDGET (window), 0, &transparent);

	return window;
}

static gboolean
nemo_blank_desktop_window_delete_event (GtkWidget *widget,
                                        GdkEventAny *event)
{
	/* Returning true tells GTK+ not to delete the window. */
	return TRUE;
}

static void
map (GtkWidget *widget)
{
	/* Chain up to realize our children */
	GTK_WIDGET_CLASS (nemo_blank_desktop_window_parent_class)->map (widget);
	gdk_window_lower (gtk_widget_get_window (widget));

    GdkWindow *window;
    GdkRGBA transparent = { 0, 0, 0, 0 };

    window = gtk_widget_get_window (widget);
    gdk_window_set_background_rgba (window, &transparent);
}

static void
unrealize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (nemo_blank_desktop_window_parent_class)->unrealize (widget);
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

    visual = gdk_screen_get_rgba_visual (gtk_widget_get_screen (widget));
    if (visual) {
        gtk_widget_set_visual (widget, visual);
    }

	GTK_WIDGET_CLASS (nemo_blank_desktop_window_parent_class)->realize (widget);

	/* This is the new way to set up the desktop window */
	set_wmspec_desktop_hint (gtk_widget_get_window (widget));
}

static void
nemo_blank_desktop_window_class_init (NemoBlankDesktopWindowClass *klass)
{
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	oclass->constructed = nemo_blank_desktop_window_constructed;
    oclass->dispose = nemo_blank_desktop_window_dispose;
	oclass->finalize = nemo_blank_desktop_window_finalize;
    oclass->set_property = nemo_blank_desktop_window_set_property;
    oclass->get_property = nemo_blank_desktop_window_get_property;

	wclass->realize = realize;
	wclass->unrealize = unrealize;
	wclass->map = map;
	wclass->delete_event = nemo_blank_desktop_window_delete_event;

    properties[PROP_MONITOR] =
        g_param_spec_int ("monitor",
                          "Monitor number",
                          "The monitor number this window is assigned to",
                          G_MININT, G_MAXINT, 0,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_type_class_add_private (klass, sizeof (NemoBlankDesktopWindowDetails));

    g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

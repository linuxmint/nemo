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
#include "nemo-desktop-manager.h"

#include <X11/Xatom.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include <libnemo-private/nemo-desktop-utils.h>
#include <libnemo-private/nemo-action.h>
#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-ui-utilities.h>

#include <eel/eel-gtk-extensions.h>

#include "nemo-plugin-manager.h"

#define DEBUG_FLAG NEMO_DEBUG_DESKTOP
#include <libnemo-private/nemo-debug.h>

enum {
    PROP_MONITOR = 1,
    NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

struct NemoBlankDesktopWindowDetails {
    gint monitor;
    GtkWidget *event_box;
    GtkWidget *popup_menu;
    gulong actions_changed_id;
    guint actions_changed_idle_id;

    gboolean actions_need_update;

    NemoActionManager *action_manager;
    GtkUIManager *ui_manager;
    GtkActionGroup *action_group;
    guint actions_merge_id;
};

G_DEFINE_TYPE (NemoBlankDesktopWindow, nemo_blank_desktop_window, 
               GTK_TYPE_WINDOW);

static void update_actions_visibility (NemoBlankDesktopWindow *window);
static void update_actions_menu (NemoBlankDesktopWindow *window);
static void actions_changed (gpointer user_data);

static void
action_show_overlay (GtkAction *action, gpointer user_data)
{
    g_return_if_fail (NEMO_IS_BLANK_DESKTOP_WINDOW (user_data));
    NemoBlankDesktopWindow *window = NEMO_BLANK_DESKTOP_WINDOW (user_data);

    nemo_desktop_manager_show_desktop_overlay (nemo_desktop_manager_get (),
                                               window->details->monitor);
}

static const GtkActionEntry entries[] = {
    { "Desktop Overlay", NULL, N_("_Customize"), NULL, N_("Adjust the desktop layout for this monitor"), G_CALLBACK (action_show_overlay) }
    //
    //
};

static void
reset_popup_menu (NemoBlankDesktopWindow *window)
{
    window->details->actions_need_update = TRUE;
    update_actions_menu (window);
}

static void
do_popup_menu (NemoBlankDesktopWindow *window, GdkEventButton *event)
{
    if (window->details->popup_menu == NULL) {
        return;
    }

    update_actions_visibility (window);

    eel_pop_up_context_menu (GTK_MENU(window->details->popup_menu),
                             (GdkEvent *) event,
                             GTK_WIDGET (window));
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
run_action_callback (NemoAction *action, gpointer user_data)
{
    nemo_action_activate (action, NULL, NULL, GTK_WINDOW (user_data));
}

static void
update_actions_visibility (NemoBlankDesktopWindow *window)
{
    nemo_action_manager_update_action_states (window->details->action_manager,
                                              window->details->action_group,
                                              NULL,
                                              NULL,
                                              FALSE,
                                              GTK_WINDOW (window));
}

static void
add_action_to_ui (NemoActionManager    *manager,
                  GtkAction            *action,
                  GtkUIManagerItemType  type,
                  const gchar          *path,
                  gpointer              user_data)
{
    NemoBlankDesktopWindow *window = NEMO_BLANK_DESKTOP_WINDOW (user_data);

    if (type != GTK_UI_MANAGER_SEPARATOR) {
        if (type == GTK_UI_MANAGER_MENUITEM) {
            g_signal_handlers_disconnect_by_func (action,
                                                  run_action_callback,
                                                  window);

            g_signal_connect (action, "activate",
                              G_CALLBACK (run_action_callback),
                              window);
        }

        gtk_action_group_add_action (window->details->action_group,
                                     action);
        gtk_action_set_visible (GTK_ACTION (action), FALSE);
    }

    const gchar *placeholder = "/background/BlankDesktopActionsPlaceholder";

    g_autofree gchar *full_path = NULL;
    const gchar *name;

    if (path != NULL) {
        full_path = g_strdup_printf ("%s/%s", placeholder, path);
    }
    else {
        full_path = g_strdup (placeholder);
    }


    if (type == GTK_UI_MANAGER_SEPARATOR) {
        name = NULL;
    }
    else {
        name = gtk_action_get_name (action);
    }

    gtk_ui_manager_add_ui (window->details->ui_manager,
                           window->details->actions_merge_id,
                           full_path,
                           name,
                           name,
                           type,
                           FALSE);
}

static void
clear_ui (NemoBlankDesktopWindow *window)
{
    if (window->details->actions_merge_id > 0) {
        gtk_ui_manager_remove_ui (window->details->ui_manager,
                                  window->details->actions_merge_id);
        window->details->actions_merge_id = 0;;

        gtk_ui_manager_remove_action_group (window->details->ui_manager,
                                            window->details->action_group);
        g_object_unref (window->details->action_group);
    }
}

static void
update_actions_menu (NemoBlankDesktopWindow *window)
{
    if (!gtk_widget_get_realized (GTK_WIDGET (window))) {
        return;
    }

    if (!window->details->actions_need_update) {
        return;
    }

    clear_ui (window);

    window->details->action_group = gtk_action_group_new ("NemoBlankDesktopActions");
    gtk_action_group_set_translation_domain (window->details->action_group, GETTEXT_PACKAGE);
    gtk_ui_manager_insert_action_group (window->details->ui_manager, window->details->action_group, 0);

    window->details->actions_merge_id =
            gtk_ui_manager_add_ui_from_resource (window->details->ui_manager, "/org/nemo/nemo-blank-desktop-window-ui.xml", NULL);

    nemo_action_manager_iterate_actions (window->details->action_manager,
                                         (NemoActionManagerIterFunc) add_action_to_ui,
                                         window);

    gtk_action_group_add_actions (window->details->action_group,
                                  entries,
                                  G_N_ELEMENTS (entries),
                                  window);

    if (window->details->popup_menu == NULL) {
        GtkWidget *menu = gtk_ui_manager_get_widget (window->details->ui_manager, "/background");
        gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (GTK_WIDGET (window)));
        window->details->popup_menu = menu;

        gtk_widget_show (menu);
    }

    window->details->actions_need_update = FALSE;
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

    clear_ui (window);

    g_signal_handlers_disconnect_by_func (nemo_menu_config_preferences, reset_popup_menu, window);

    G_OBJECT_CLASS (nemo_blank_desktop_window_parent_class)->dispose (obj);
}

static void
nemo_blank_desktop_window_finalize (GObject *obj)
{
    NemoBlankDesktopWindow *window = NEMO_BLANK_DESKTOP_WINDOW (obj);

    g_object_unref (window->details->ui_manager);

    G_OBJECT_CLASS (nemo_blank_desktop_window_parent_class)->finalize (obj);
}

static gboolean
actions_changed_idle_cb (gpointer user_data)
{
    NemoBlankDesktopWindow *window = NEMO_BLANK_DESKTOP_WINDOW (user_data);

    reset_popup_menu (window);

    window->details->actions_changed_idle_id = 0;
    return G_SOURCE_REMOVE;
}

static void
actions_changed (gpointer user_data)
{
    NemoBlankDesktopWindow *window = NEMO_BLANK_DESKTOP_WINDOW (user_data);

    g_clear_handle_id (&window->details->actions_changed_idle_id, g_source_remove);
    window->details->actions_changed_idle_id = g_idle_add (actions_changed_idle_cb, window);
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

    window->details->ui_manager = gtk_ui_manager_new ();
    window->details->action_manager = nemo_action_manager_new ();

    if (window->details->actions_changed_id == 0) {
        window->details->actions_changed_id = g_signal_connect_swapped (window->details->action_manager,
                                                                        "changed",
                                                                        G_CALLBACK (actions_changed),
                                                                        window);
    }

    nemo_blank_desktop_window_update_geometry (window);

    gtk_window_set_resizable (GTK_WINDOW (window),
                  FALSE);
    gtk_window_set_decorated (GTK_WINDOW (window),
                  FALSE);

    window->details->event_box = gtk_event_box_new ();
    gtk_event_box_set_visible_window (GTK_EVENT_BOX (window->details->event_box), FALSE);
    gtk_container_add (GTK_CONTAINER (window), window->details->event_box);

    gtk_widget_show_all (GTK_WIDGET (window));

    g_signal_connect (window->details->event_box, "button-press-event", G_CALLBACK (on_button_press), window);
    g_signal_connect (window->details->event_box, "popup-menu", G_CALLBACK (on_popup_menu), window);

    g_signal_connect_swapped (nemo_menu_config_preferences,
                              "changed::desktop-menu-customize",
                              G_CALLBACK (reset_popup_menu),
                              window);
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
    gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DESKTOP);

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
realize (GtkWidget *widget)
{
    GdkVisual *visual;

    visual = gdk_screen_get_rgba_visual (gtk_widget_get_screen (widget));
    if (visual) {
        gtk_widget_set_visual (widget, visual);
    }

	GTK_WIDGET_CLASS (nemo_blank_desktop_window_parent_class)->realize (widget);
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

void
nemo_blank_desktop_window_update_geometry (NemoBlankDesktopWindow *window)
{
    GdkRectangle rect;

    nemo_desktop_manager_get_window_rect_for_monitor (nemo_desktop_manager_get (),
                                                      window->details->monitor,
                                                      &rect);

    DEBUG ("NemoBlankDesktopWindow monitor:%d: x:%d, y:%d, w:%d, h:%d",
           window->details->monitor,
           rect.x, rect.y,
           rect.width, rect.height);

    gtk_window_move (GTK_WINDOW (window), rect.x, rect.y);
    gtk_widget_set_size_request (GTK_WIDGET (window), rect.width, rect.height);
}


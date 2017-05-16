/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-desktop-icon-view.c - implementation of icon view for managing the desktop.

   Copyright (C) 2000, 2001 Eazel, Inc.mou

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

   Authors: Mike Engber <engber@eazel.com>
   	    Gene Z. Ragan <gzr@eazel.com>
	    Miguel de Icaza <miguel@ximian.com>
*/

#include <config.h>

#include "nemo-desktop-icon-grid-view.h"

#include "nemo-actions.h"
#include "nemo-application.h"
#include "nemo-desktop-manager.h"
#include "nemo-desktop-window.h"
#include "nemo-icon-view-grid-container.h"
#include "nemo-view-factory.h"
#include "nemo-view.h"

#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <fcntl.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <libnemo-private/nemo-desktop-icon-file.h>
#include <libnemo-private/nemo-directory-notify.h>
#include <libnemo-private/nemo-file-changes-queue.h>
#include <libnemo-private/nemo-file-operations.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-ui-utilities.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-link.h>
#include <libnemo-private/nemo-metadata.h>
#include <libnemo-private/nemo-monitor.h>
#include <libnemo-private/nemo-program-choosing.h>
#include <libnemo-private/nemo-trash-monitor.h>
#include <libnemo-private/nemo-desktop-utils.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Timeout to check the desktop directory for updates */
#define RESCAN_TIMEOUT 4

struct NemoDesktopIconGridViewDetails
{
	GdkWindow *root_window;
	GtkActionGroup *desktop_action_group;
	guint desktop_merge_id;

	/* For the desktop rescanning
	 */
	gulong delayed_init_signal;
	guint reload_desktop_timeout;
	gboolean pending_rescan;
    gboolean updating_menus;
};

typedef enum {
    DESKTOP_ARRANGE_VERTICAL,
    DESKTOP_ARRANGE_HORIZONTAL
} NemoDesktopLayoutDirection;

typedef struct {
    const char *action;
    const char *metadata_text;
    const NemoFileSortType sort_type;
} DesktopSortCriterion;

static const DesktopSortCriterion sort_criteria[] = {
    {
        "Desktop Sort by Name",
        "name",
        NEMO_FILE_SORT_BY_DISPLAY_NAME
    },
    {
        "Desktop Sort by Size",
        "size",
        NEMO_FILE_SORT_BY_SIZE
    },
    {
        "Desktop Sort by Type",
        "detailed_type",
        NEMO_FILE_SORT_BY_DETAILED_TYPE
    },
    {
        "Desktop Sort by Date",
        "modification date",
        NEMO_FILE_SORT_BY_MTIME
    }
};

static void     real_merge_menus                                  (NemoView        *view);
static void     real_update_menus                                 (NemoView        *view);
static void     nemo_desktop_icon_grid_view_update_icon_container_fonts  (NemoDesktopIconGridView      *view);
static void     font_changed_callback                             (gpointer                callback_data);
static void     nemo_desktop_icon_grid_view_constructed (NemoDesktopIconGridView *desktop_icon_grid_view);

G_DEFINE_TYPE (NemoDesktopIconGridView, nemo_desktop_icon_grid_view, NEMO_TYPE_ICON_VIEW)

static char *desktop_directory;
static time_t desktop_dir_modify_time;

#define get_icon_container(w) nemo_icon_view_get_icon_container(NEMO_ICON_VIEW (w))

static void
desktop_directory_changed_callback (gpointer callback_data)
{
	g_free (desktop_directory);
	desktop_directory = nemo_get_desktop_directory ();
}

static void
update_margins (NemoDesktopIconGridView *icon_view)
{
    NemoIconContainer *icon_container;
    GdkRectangle geometry, work_rect;
    gint current_monitor;
    gint l, r, t, b;

    icon_container = get_icon_container (icon_view);

    g_object_get (NEMO_DESKTOP_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (icon_view))),
                  "monitor", &current_monitor,
                  NULL);

    /* _NET_WORKAREA only applies to the primary monitor - use it to adjust
       container margins on the primary icon container only.  For any others,
       add a sane amount of padding for any likely chrome. */
    if (current_monitor != nemo_desktop_utils_get_primary_monitor ()) {
        nemo_icon_container_set_margins (icon_container, 50, 50, 50, 50);
        return;
    }

    nemo_desktop_utils_get_monitor_geometry (current_monitor, &geometry);
    nemo_desktop_utils_get_monitor_work_rect (current_monitor, &work_rect);

    l = work_rect.x - geometry.x;
    r = (geometry.x + geometry.width) - (work_rect.x + work_rect.width);
    t = work_rect.y - geometry.y;
    b = (geometry.y + geometry.height) - (work_rect.y + work_rect.height);

    nemo_icon_container_set_margins (icon_container, l, r, t, b);
}

static GdkFilterReturn
gdk_filter_func (GdkXEvent *gdk_xevent,
                 GdkEvent  *event,
                 gpointer   data)
{
    XEvent *xevent = gdk_xevent;
    NemoDesktopIconGridView *icon_view;

    icon_view = NEMO_DESKTOP_ICON_GRID_VIEW (data);

    switch (xevent->type) {
        case PropertyNotify:
            if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name ("_NET_WORKAREA")) {
                update_margins (icon_view);
            }
            break;
        default:
            break;
    }

    return GDK_FILTER_CONTINUE;
}

static const char *
real_get_id (NemoView *view)
{
	return NEMO_DESKTOP_ICON_GRID_VIEW_IID;
}

static gboolean
should_show_file_on_current_monitor (NemoView *view, NemoFile *file)
{
    gint current_monitor = nemo_desktop_utils_get_monitor_for_widget (GTK_WIDGET (view));
    gint file_monitor = nemo_file_get_monitor_number (file);

    NemoDesktopManager *dm = nemo_desktop_manager_get ();

    if (current_monitor == file_monitor) {
        nemo_file_set_is_desktop_orphan (file, FALSE);
        return TRUE;
    }

    if (file_monitor > -1 &&
        !g_settings_get_boolean (nemo_desktop_preferences, NEMO_PREFERENCES_SHOW_ORPHANED_DESKTOP_ICONS)) {
        return FALSE;
    }

    if (file_monitor == -1) {
        /* New file, no previous metadata - this should go on the primary monitor */
        return nemo_desktop_manager_get_monitor_is_primary (dm, current_monitor);
    }

    if (!nemo_desktop_manager_get_monitor_is_active (dm, file_monitor)) {
        nemo_file_set_is_desktop_orphan (file, TRUE);
        if (nemo_desktop_manager_get_monitor_is_primary (dm, current_monitor)) {
            return TRUE;
        }
    }

    return FALSE;
}

static void
nemo_desktop_icon_grid_view_remove_file (NemoView *view, NemoFile *file, NemoDirectory *directory)
{
    /* This used to assert that 'directory == nemo_view_get_model (view)', but that
     * resulted in a lot of crash reports (bug #352592). I don't see how that trace happens.
     * It seems that somehow we get a files_changed event sent to the view from a directory
     * that isn't the model, but the code disables the monitor and signal callback handlers when
     * changing directories. Maybe we can get some more information when this happens.
     * Further discussion in bug #368178.
     */
    if (directory != nemo_view_get_model (view)) {
        char *file_uri, *dir_uri, *model_uri;
        file_uri = nemo_file_get_uri (file);
        dir_uri = nemo_directory_get_uri (directory);
        model_uri = nemo_directory_get_uri (nemo_view_get_model (view));
        g_warning ("nemo_icon_view_remove_file() - directory not icon view model, shouldn't happen.\n"
               "file: %p:%s, dir: %p:%s, model: %p:%s, view loading: %d\n"
               "If you see this, please add this info to http://bugzilla.gnome.org/show_bug.cgi?id=368178",
               file, file_uri, directory, dir_uri, nemo_view_get_model (view), model_uri, nemo_view_get_loading (view));
        g_free (file_uri);
        g_free (dir_uri);
        g_free (model_uri);
    }
    
    if (nemo_icon_container_remove (get_icon_container (view), NEMO_ICON_CONTAINER_ICON_DATA (file))) {
        nemo_file_unref (file);
    }
}

static void
nemo_desktop_icon_grid_view_add_file (NemoView *view, NemoFile *file, NemoDirectory *directory)
{
    NemoIconView *icon_view;
    NemoIconContainer *icon_container;

    g_assert (directory == nemo_view_get_model (view));
    
    icon_view = NEMO_ICON_VIEW (view);
    icon_container = get_icon_container (icon_view);

    if (!should_show_file_on_current_monitor (view, file)) {
        return;
    }

    if (nemo_icon_container_add (icon_container, NEMO_ICON_CONTAINER_ICON_DATA (file))) {
        nemo_file_ref (file);
    }
}

static void
nemo_desktop_icon_grid_view_file_changed (NemoView *view, NemoFile *file, NemoDirectory *directory)
{
    NemoIconView *icon_view;

    g_assert (directory == nemo_view_get_model (view));
    
    g_return_if_fail (view != NULL);
    icon_view = NEMO_ICON_VIEW (view);

    
    if (!should_show_file_on_current_monitor (view, file)) {
        nemo_desktop_icon_grid_view_remove_file (view, file, directory);
    } else {
        nemo_icon_container_request_update (get_icon_container (icon_view),
                                            NEMO_ICON_CONTAINER_ICON_DATA (file));
    }
}

static void
unrealized_callback (GtkWidget *widget, NemoDesktopIconGridView *desktop_icon_grid_view)
{
  g_return_if_fail (desktop_icon_grid_view->details->root_window != NULL);

  gdk_window_remove_filter (desktop_icon_grid_view->details->root_window,
                            gdk_filter_func,
                            desktop_icon_grid_view);

  desktop_icon_grid_view->details->root_window = NULL;
}

static void
realized_callback (GtkWidget *widget, NemoDesktopIconGridView *desktop_icon_grid_view)
{
  GdkWindow *root_window;
  GdkScreen *screen;

  g_return_if_fail (desktop_icon_grid_view->details->root_window == NULL);

  screen = gtk_widget_get_screen (widget);

  root_window = gdk_screen_get_root_window (screen);

  desktop_icon_grid_view->details->root_window = root_window;

  update_margins (desktop_icon_grid_view);

  /* Setup the property filter */
  gdk_window_set_events (root_window, GDK_PROPERTY_CHANGE_MASK);
  gdk_window_add_filter (root_window,
                         gdk_filter_func,
                         desktop_icon_grid_view);
}

static void
nemo_desktop_icon_grid_view_dispose (GObject *object)
{
	NemoDesktopIconGridView *icon_view;
	GtkUIManager *ui_manager;

	icon_view = NEMO_DESKTOP_ICON_GRID_VIEW (object);

	/* Remove desktop rescan timeout. */
	if (icon_view->details->reload_desktop_timeout != 0) {
		g_source_remove (icon_view->details->reload_desktop_timeout);
		icon_view->details->reload_desktop_timeout = 0;
	}

	ui_manager = nemo_view_get_ui_manager (NEMO_VIEW (icon_view));
	if (ui_manager != NULL) {
		nemo_ui_unmerge_ui (ui_manager,
					&icon_view->details->desktop_merge_id,
					&icon_view->details->desktop_action_group);
	}

	g_signal_handlers_disconnect_by_func (nemo_desktop_preferences,
					      font_changed_callback,
					      icon_view);

	g_signal_handlers_disconnect_by_func (nemo_preferences,
					      desktop_directory_changed_callback,
					      NULL);

	g_signal_handlers_disconnect_by_func (gnome_lockdown_preferences,
					      nemo_view_update_menus,
					      icon_view);

	G_OBJECT_CLASS (nemo_desktop_icon_grid_view_parent_class)->dispose (object);
}

static void
nemo_desktop_icon_grid_view_class_init (NemoDesktopIconGridViewClass *class)
{
	NemoViewClass *vclass;

	vclass = NEMO_VIEW_CLASS (class);

    G_OBJECT_CLASS (class)->dispose = nemo_desktop_icon_grid_view_dispose;
	G_OBJECT_CLASS (class)->constructed = nemo_desktop_icon_grid_view_constructed;

    NEMO_ICON_VIEW_CLASS (class)->use_grid_container = TRUE;

	vclass->merge_menus = real_merge_menus;
	vclass->update_menus = real_update_menus;
	vclass->get_view_id = real_get_id;
    vclass->add_file = nemo_desktop_icon_grid_view_add_file;
    vclass->file_changed = nemo_desktop_icon_grid_view_file_changed;
    vclass->remove_file = nemo_desktop_icon_grid_view_remove_file;

#if GTK_CHECK_VERSION(3, 21, 0)
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (class);
	gtk_widget_class_set_css_name (wclass, "nemo-desktop-icon-view");
#endif
	g_type_class_add_private (class, sizeof (NemoDesktopIconGridViewDetails));
}

static void
nemo_desktop_icon_grid_view_handle_middle_click (NemoIconContainer *icon_container,
						GdkEventButton *event,
						NemoDesktopIconGridView *desktop_icon_grid_view)
{
	XButtonEvent x_event;
	GdkDevice *keyboard = NULL, *pointer = NULL, *cur;
	GdkDeviceManager *manager;
	GList *list, *l;

	manager = gdk_display_get_device_manager (gtk_widget_get_display (GTK_WIDGET (icon_container)));
	list = gdk_device_manager_list_devices (manager, GDK_DEVICE_TYPE_MASTER);

	for (l = list; l != NULL; l = l->next) {
		cur = l->data;

		if (pointer == NULL && (gdk_device_get_source (cur) == GDK_SOURCE_MOUSE)) {
			pointer = cur;
		}

		if (keyboard == NULL && (gdk_device_get_source (cur) == GDK_SOURCE_KEYBOARD)) {
			keyboard = cur;
		}

		if (pointer != NULL && keyboard != NULL) {
			break;
		}
	}

	g_list_free (list);

	/* During a mouse click we have the pointer and keyboard grab.
	 * We will send a fake event to the root window which will cause it
	 * to try to get the grab so we need to let go ourselves.
	 */

	if (pointer != NULL) {
		gdk_device_ungrab (pointer, GDK_CURRENT_TIME);
	}

	
	if (keyboard != NULL) {
		gdk_device_ungrab (keyboard, GDK_CURRENT_TIME);
	}

	/* Stop the event because we don't want anyone else dealing with it. */	
	gdk_flush ();
	g_signal_stop_emission_by_name (icon_container, "middle_click");

	/* build an X event to represent the middle click. */
	x_event.type = ButtonPress;
	x_event.send_event = True;
	x_event.display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	x_event.window = GDK_ROOT_WINDOW ();
	x_event.root = GDK_ROOT_WINDOW ();
	x_event.subwindow = 0;
	x_event.time = event->time;
	x_event.x = event->x;
	x_event.y = event->y;
	x_event.x_root = event->x_root;
	x_event.y_root = event->y_root;
	x_event.state = event->state;
	x_event.button = event->button;
	x_event.same_screen = True;
	
	/* Send it to the root window, the window manager will handle it. */
	XSendEvent (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), GDK_ROOT_WINDOW (), True,
		    ButtonPressMask, (XEvent *) &x_event);
}

static void
desktop_icon_container_realize (GtkWidget *widget,
                                NemoDesktopIconGridView *desktop_icon_grid_view)
{
    GdkWindow *bin_window;
    GdkRGBA transparent = { 0, 0, 0, 0 };

    bin_window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
    gdk_window_set_background_rgba (bin_window, &transparent);
}

static gboolean
do_desktop_rescan (gpointer data)
{
	NemoDesktopIconGridView *desktop_icon_grid_view;
	struct stat buf;

	desktop_icon_grid_view = NEMO_DESKTOP_ICON_GRID_VIEW (data);
	if (desktop_icon_grid_view->details->pending_rescan) {
		return TRUE;
	}
	
	if (stat (desktop_directory, &buf) == -1) {
		return TRUE;
	}

	if (buf.st_ctime == desktop_dir_modify_time) {
		return TRUE;
	}

	desktop_icon_grid_view->details->pending_rescan = TRUE;

	nemo_directory_force_reload
		(nemo_view_get_model (NEMO_VIEW (desktop_icon_grid_view)));

	return TRUE;
}

static void
done_loading (NemoDirectory *model,
	      NemoDesktopIconGridView *desktop_icon_grid_view)
{
	struct stat buf;

	desktop_icon_grid_view->details->pending_rescan = FALSE;
	if (stat (desktop_directory, &buf) == -1) {
		return;
	}

	desktop_dir_modify_time = buf.st_ctime;
}

/* This function is used because the NemoDirectory model does not
 * exist always in the desktop_icon_grid_view, so we wait until it has been
 * instantiated.
 */
static void
delayed_init (NemoDesktopIconGridView *desktop_icon_grid_view)
{
	/* Keep track of the load time. */
	g_signal_connect_object (nemo_view_get_model (NEMO_VIEW (desktop_icon_grid_view)),
				 "done_loading",
				 G_CALLBACK (done_loading), desktop_icon_grid_view, 0);

	/* Monitor desktop directory. */
	desktop_icon_grid_view->details->reload_desktop_timeout =
		g_timeout_add_seconds (RESCAN_TIMEOUT, do_desktop_rescan, desktop_icon_grid_view);

	g_signal_handler_disconnect (desktop_icon_grid_view,
				     desktop_icon_grid_view->details->delayed_init_signal);

	desktop_icon_grid_view->details->delayed_init_signal = 0;
    desktop_icon_grid_view->details->updating_menus = FALSE;
}

static void
font_changed_callback (gpointer callback_data)
{
 	g_return_if_fail (NEMO_IS_DESKTOP_ICON_GRID_VIEW (callback_data));
	
	nemo_desktop_icon_grid_view_update_icon_container_fonts (NEMO_DESKTOP_ICON_GRID_VIEW (callback_data));
}

static void
nemo_desktop_icon_grid_view_update_icon_container_fonts (NemoDesktopIconGridView *icon_view)
{
	NemoIconContainer *icon_container;
	char *font;

	icon_container = get_icon_container (icon_view);
	g_assert (icon_container != NULL);

	font = g_settings_get_string (nemo_desktop_preferences,
                                  NEMO_PREFERENCES_DESKTOP_FONT);

	nemo_icon_container_set_font (icon_container, font);

	g_free (font);
}

static void
nemo_desktop_icon_grid_view_init (NemoDesktopIconGridView *desktop_icon_grid_view)
{
    desktop_icon_grid_view->details = G_TYPE_INSTANCE_GET_PRIVATE (desktop_icon_grid_view,
                                                                   NEMO_TYPE_DESKTOP_ICON_GRID_VIEW,
                                                                   NemoDesktopIconGridViewDetails);
}

static void
nemo_desktop_icon_grid_view_constructed (NemoDesktopIconGridView *desktop_icon_grid_view)
{
    NemoIconContainer *icon_container;
    GtkAllocation allocation;
    GtkAdjustment *hadj, *vadj;

    G_OBJECT_CLASS (nemo_desktop_icon_grid_view_parent_class)->constructed (G_OBJECT (desktop_icon_grid_view));

    if (desktop_directory == NULL) {
        g_signal_connect_swapped (nemo_preferences, "changed::" NEMO_PREFERENCES_DESKTOP_IS_HOME_DIR,
                      G_CALLBACK(desktop_directory_changed_callback),
                      NULL);
        desktop_directory_changed_callback (NULL);
    }

    icon_container = get_icon_container (desktop_icon_grid_view);
    nemo_icon_container_set_use_drop_shadows (icon_container, TRUE);
    nemo_icon_view_grid_container_set_sort_desktop (NEMO_ICON_VIEW_GRID_CONTAINER (icon_container), TRUE);

    /* Do a reload on the desktop if we don't have FAM, a smarter
     * way to keep track of the items on the desktop.
     */
    if (!nemo_monitor_active ()) {
        desktop_icon_grid_view->details->delayed_init_signal = g_signal_connect_object
            (desktop_icon_grid_view, "begin_loading",
             G_CALLBACK (delayed_init), desktop_icon_grid_view, 0);
    }
    
    nemo_icon_container_set_is_fixed_size (icon_container, TRUE);
    nemo_icon_container_set_is_desktop (icon_container, TRUE);

    NEMO_ICON_VIEW_GRID_CONTAINER (icon_container)->horizontal = FALSE;
    NEMO_ICON_VIEW_GRID_CONTAINER (icon_container)->manual_sort_dirty = TRUE;

    nemo_icon_container_set_store_layout_timestamps (icon_container, TRUE);

    /* Set allocation to be at 0, 0 */
    gtk_widget_get_allocation (GTK_WIDGET (icon_container), &allocation);
    allocation.x = 0;
    allocation.y = 0;
    gtk_widget_set_allocation (GTK_WIDGET (icon_container), &allocation);
    
    gtk_widget_queue_resize (GTK_WIDGET (icon_container));

    hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (icon_container));
    vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (icon_container));

    gtk_adjustment_set_value (hadj, 0);
    gtk_adjustment_set_value (vadj, 0);

    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (desktop_icon_grid_view),
                         GTK_SHADOW_NONE);

    nemo_view_ignore_hidden_file_preferences
        (NEMO_VIEW (desktop_icon_grid_view));

    nemo_view_set_show_foreign (NEMO_VIEW (desktop_icon_grid_view),
                    FALSE);

    g_signal_connect_object (icon_container, "middle_click",
                 G_CALLBACK (nemo_desktop_icon_grid_view_handle_middle_click), desktop_icon_grid_view, 0);
    g_signal_connect_object (icon_container, "realize",
                 G_CALLBACK (desktop_icon_container_realize), desktop_icon_grid_view, 0);

    g_signal_connect_object (desktop_icon_grid_view, "realize",
                             G_CALLBACK (realized_callback), desktop_icon_grid_view, 0);
    g_signal_connect_object (desktop_icon_grid_view, "unrealize",
                             G_CALLBACK (unrealized_callback), desktop_icon_grid_view, 0);

    g_signal_connect_swapped (nemo_desktop_preferences,
                  "changed::" NEMO_PREFERENCES_DESKTOP_FONT,
                  G_CALLBACK (font_changed_callback),
                  desktop_icon_grid_view);

    nemo_desktop_icon_grid_view_update_icon_container_fonts (desktop_icon_grid_view);

    g_signal_connect_swapped (gnome_lockdown_preferences,
                  "changed::" NEMO_PREFERENCES_LOCKDOWN_COMMAND_LINE,
                  G_CALLBACK (nemo_view_update_menus),
                  desktop_icon_grid_view);
}

static void
action_empty_trash_conditional_callback (GtkAction *action,
                                         gpointer   data)
{
    g_assert (NEMO_IS_VIEW (data));

    nemo_file_operations_empty_trash (GTK_WIDGET (data));
}

static void
clear_orphan_states (NemoDesktopIconGridView *view)
{
    GList *icons;

    for (icons = get_icon_container (view)->details->icons; icons != NULL; icons = icons->next) {
        NemoFile *file;
        NemoIcon *icon;

        icon = icons->data;

        file = NEMO_FILE (icon->data);
        nemo_file_set_is_desktop_orphan (file, FALSE);
    }
}

static void
action_align_grid_callback (GtkAction *action,
                            NemoDesktopIconGridView *view)
{
    NemoFile *file;
    gboolean keep_aligned;

    g_assert (NEMO_IS_VIEW (view));

    if (view->details->updating_menus) {
        return;
    }

    clear_orphan_states (view);

    if (!nemo_icon_container_is_auto_layout (get_icon_container (view))) {
        NEMO_ICON_VIEW_GRID_CONTAINER (get_icon_container (view))->manual_sort_dirty = TRUE;
    }

    keep_aligned = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

    file = nemo_view_get_directory_as_file (NEMO_VIEW (view));

    nemo_icon_view_set_directory_keep_aligned (NEMO_ICON_VIEW (view), file, keep_aligned);

    nemo_icon_container_set_keep_aligned (get_icon_container (view), keep_aligned);
}

static void
action_auto_arrange_callback (GtkAction *action,
                              NemoDesktopIconGridView *view)
{
    gboolean new;

    g_assert (NEMO_IS_VIEW (view));

    if (view->details->updating_menus) {
        return;
    }

    new = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

    clear_orphan_states (view);
    nemo_icon_view_set_sort_reversed (NEMO_ICON_VIEW (view), FALSE, TRUE);

    nemo_icon_container_set_auto_layout (get_icon_container (view), new);

    if (new == TRUE) {
        nemo_icon_container_set_keep_aligned (get_icon_container (view), TRUE);
    }
}

static void
set_sort_type (NemoDesktopIconGridView *view,
               GtkAction               *action,
               NemoFileSortType         type)
{
    NemoFile *file;
    gint i;
    gchar *old_sort_name;

    if (view->details->updating_menus) {
        return;
    }

    clear_orphan_states (view);

    file = nemo_view_get_directory_as_file (NEMO_VIEW (view));

    old_sort_name = nemo_icon_view_get_directory_sort_by (NEMO_ICON_VIEW (view), file);

    if (!NEMO_ICON_VIEW_GRID_CONTAINER (get_icon_container (view))->manual_sort_dirty) {
        for (i = 0; i < G_N_ELEMENTS (sort_criteria); i++) {
            if (sort_criteria[i].sort_type == type &&
                g_strcmp0 (sort_criteria[i].metadata_text, old_sort_name) == 0) {
                GList *selection;

                nemo_icon_view_flip_sort_reversed (NEMO_ICON_VIEW (view));

                nemo_icon_container_sort (get_icon_container (view));

                selection = nemo_view_get_selection (view);

                /* Make sure at least one of the selected items is scrolled into view */
                if (selection != NULL) {
                    nemo_icon_container_reveal (get_icon_container (NEMO_ICON_VIEW (view)), selection->data);
                }

                nemo_file_list_free (selection);

                nemo_view_update_menus (NEMO_VIEW (view));
                return;
            }
        }
    }

    g_free (old_sort_name);

    NEMO_ICON_VIEW_GRID_CONTAINER (get_icon_container (view))->manual_sort_dirty = FALSE;

    nemo_icon_view_set_sort_reversed (NEMO_ICON_VIEW (view), FALSE, TRUE);
    nemo_icon_view_set_sort_criterion_by_sort_type (NEMO_ICON_VIEW (view), type);

    nemo_view_update_menus (NEMO_VIEW (view));
}

static void
action_sort_name (GtkAction               *action,
                  NemoDesktopIconGridView *view)
{
    g_assert (NEMO_IS_VIEW (view));

    set_sort_type (view, action, NEMO_FILE_SORT_BY_DISPLAY_NAME);
}

static void
action_sort_size (GtkAction               *action,
                  NemoDesktopIconGridView *view)
{
    g_assert (NEMO_IS_VIEW (view));

    set_sort_type (view, action, NEMO_FILE_SORT_BY_SIZE);
}

static void
action_sort_type (GtkAction               *action,
                  NemoDesktopIconGridView *view)
{
    g_assert (NEMO_IS_VIEW (view));

    set_sort_type (view, action, NEMO_FILE_SORT_BY_DETAILED_TYPE);
}

static void
action_sort_date (GtkAction               *action,
                  NemoDesktopIconGridView *view)
{
    g_assert (NEMO_IS_VIEW (view));

    set_sort_type (view, action, NEMO_FILE_SORT_BY_MTIME);
}

static void
set_direction (NemoDesktopIconGridView *view,
               gboolean                 horizontal)
{
    NemoFile *file;
    NemoIconContainer *container;

    if (view->details->updating_menus) {
        return;
    }

    clear_orphan_states (view);

    container = get_icon_container (view);
    file = nemo_view_get_directory_as_file (NEMO_VIEW (view));

    nemo_icon_container_set_horizontal_layout (container, horizontal);
    container->details->needs_resort = TRUE;

    if (nemo_icon_container_is_auto_layout (container)) {
        nemo_icon_view_set_sort_reversed (NEMO_ICON_VIEW (view), FALSE, TRUE);
        nemo_icon_container_sort (get_icon_container (view));
        nemo_icon_container_redo_layout (get_icon_container (view));
    } else {
        NEMO_ICON_VIEW_GRID_CONTAINER (get_icon_container (view))->manual_sort_dirty = TRUE;
    }

    nemo_icon_view_set_directory_horizontal_layout (NEMO_ICON_VIEW (view), file, horizontal);

    nemo_view_update_menus (NEMO_VIEW (view));
}

static void
action_horizontal_layout (GtkAction               *action,
                          NemoDesktopIconGridView *view)
{
    g_assert (NEMO_IS_VIEW (view));

    set_direction (view, TRUE);
}

static void
action_vertical_layout (GtkAction               *action,
                        NemoDesktopIconGridView *view)
{
    g_assert (NEMO_IS_VIEW (view));

    set_direction (view, FALSE);
}

static void
action_desktop_size_callback (GtkAction               *action,
                              GtkRadioAction          *current,
                              NemoDesktopIconGridView *view)
{
    NemoZoomLevel level;
    NemoIconContainer *container;

    if (view->details->updating_menus) {
        return;
    }

    level = gtk_radio_action_get_current_value (current);

    nemo_view_zoom_to_level (NEMO_VIEW (view), level);

    clear_orphan_states (view);

    /* TODO: Instead of switching back to defaults, re-align the existing icons
     * into the new slots.  This is complicated, due to how the redo_layout_internal
     * function works. */

    nemo_icon_view_set_sort_reversed (NEMO_ICON_VIEW (view), FALSE, TRUE);

    container = get_icon_container (view);

    if (!container->details->auto_layout) {
        set_sort_type (view, action, NEMO_FILE_SORT_BY_DISPLAY_NAME);
    }

    nemo_view_update_menus (NEMO_VIEW (view));
}

static gboolean
trash_link_is_selection (NemoView *view)
{
	GList *selection;
	NemoDesktopLink *link;
	gboolean result;

	result = FALSE;
	
	selection = nemo_view_get_selection (view);

	if ((g_list_length (selection) == 1) &&
	    NEMO_IS_DESKTOP_ICON_FILE (selection->data)) {
		link = nemo_desktop_icon_file_get_link (NEMO_DESKTOP_ICON_FILE (selection->data));
		/* link may be NULL if the link was recently removed (unmounted) */
		if (link != NULL &&
		    nemo_desktop_link_get_link_type (link) == NEMO_DESKTOP_LINK_TRASH) {
			result = TRUE;
		}
		if (link) {
			g_object_unref (link);
		}
	}
	
	nemo_file_list_free (selection);

	return result;
}

static void
real_update_menus (NemoView *view)
{
	NemoDesktopIconGridView *desktop_view;
    NemoIconContainer *container;
    NemoFile *file;
    NemoZoomLevel zoom_level;
	char *label;
	gboolean include_empty_trash;
    gboolean horizontal_layout;
    gboolean auto_arrange, reversed;

    GtkUIManager *ui_manager;
	GtkAction *action;
    GList *groups, *l;

	g_assert (NEMO_IS_DESKTOP_ICON_GRID_VIEW (view));

    container = get_icon_container (view);

    desktop_view = NEMO_DESKTOP_ICON_GRID_VIEW (view);
    desktop_view->details->updating_menus = TRUE;

	NEMO_VIEW_CLASS (nemo_desktop_icon_grid_view_parent_class)->update_menus (view);

    /* Empty Trash */
    include_empty_trash = trash_link_is_selection (view);
    action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                          NEMO_ACTION_EMPTY_TRASH_CONDITIONAL);
    gtk_action_set_visible (action, include_empty_trash);

    if (include_empty_trash) {
        label = g_strdup (_("E_mpty Trash"));
        g_object_set (action , "label", label, NULL);
        gtk_action_set_sensitive (action, !nemo_trash_monitor_is_empty ());
        g_free (label);
    }

    file = nemo_view_get_directory_as_file (NEMO_VIEW (desktop_view));

    horizontal_layout = nemo_icon_view_get_directory_horizontal_layout (NEMO_ICON_VIEW (desktop_view), file);

    action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                          "Horizontal Layout");
    gtk_action_set_icon_name (action, horizontal_layout ? "menu-bullet" : "menu-none");

    action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                          "Vertical Layout");
    gtk_action_set_icon_name (action, horizontal_layout ? "menu-none" : "menu-bullet");

    action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                          "Desktop Align to Grid");

    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                                  nemo_icon_container_is_keep_aligned (container));

    auto_arrange = nemo_icon_container_is_auto_layout (container);

    gtk_action_set_sensitive (action, !auto_arrange);

    if (auto_arrange) {
        NEMO_ICON_VIEW_GRID_CONTAINER (container)->manual_sort_dirty = !auto_arrange;
    }

    action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                          "Desktop Autoarrange");
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), auto_arrange);

    gint i;
    gchar *order;

    order = nemo_icon_view_get_directory_sort_by (NEMO_ICON_VIEW (desktop_view), file);
    reversed = nemo_icon_view_get_directory_sort_reversed (NEMO_ICON_VIEW (desktop_view), file);

    for (i = 0; i < G_N_ELEMENTS (sort_criteria); i++) {
        action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                              sort_criteria[i].action);
        gtk_action_set_always_show_image (action, TRUE);
        if (!NEMO_ICON_VIEW_GRID_CONTAINER (container)->manual_sort_dirty &&
             g_strcmp0 (order, sort_criteria[i].metadata_text) == 0) {
            if (horizontal_layout) {
                if (auto_arrange) {
                    gtk_action_set_icon_name (action,
                                              reversed ? "menu-sort-left" : "menu-sort-right");
                } else {
                    gtk_action_set_icon_name (action,
                                              reversed ? "menu-sort-left-free" : "menu-sort-right-free");
                }
            } else {
                if (auto_arrange) {
                    gtk_action_set_icon_name (action,
                                              reversed ? "menu-sort-up" : "menu-sort-down");
                } else {
                    gtk_action_set_icon_name (action,
                                              reversed ? "menu-sort-up-free" : "menu-sort-down-free");
                }
            }
        } else {
            gtk_action_set_icon_name (action, "menu-none");
        }
    }

    g_free (order);

    /* Update zoom radio */

    switch (nemo_view_get_zoom_level (NEMO_VIEW (desktop_view))) {
        case NEMO_ZOOM_LEVEL_SMALL:
            action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                                  "Desktop Small");
            break;
        case NEMO_ZOOM_LEVEL_LARGE:
            action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                                  "Desktop Large");
            break;
        case NEMO_ZOOM_LEVEL_STANDARD:
        default:
            action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                                  "Desktop Normal");
            break;
    }

    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

    desktop_view->details->updating_menus = FALSE;

    ui_manager = nemo_view_get_ui_manager (view);
    groups = gtk_ui_manager_get_action_groups (ui_manager);

    /* These actions are set up in NemoIconView, but we want to replace it all with the
     * desktop submenu.  This could be avoided by creating an additional subclass of NemoIconView
     * to implement these things, but this is simpler for now.
     */
    for (l = groups; l != NULL; l = l->next) {
        GtkActionGroup *group;

        group = GTK_ACTION_GROUP (l->data);

        action = gtk_action_group_get_action (group, NEMO_ACTION_CLEAN_UP);
        if (action != NULL) {
            gtk_action_set_visible (action, FALSE);
        }
        action = gtk_action_group_get_action (group, NEMO_ACTION_KEEP_ALIGNED);
        if (action != NULL) {
            gtk_action_set_visible (action, FALSE);
        }
        action = gtk_action_group_get_action (group, NEMO_ACTION_ARRANGE_ITEMS);
        if (action != NULL) {
            gtk_action_set_visible (action, FALSE);
        }
    }
}

static const GtkToggleActionEntry desktop_grid_toggle_entries[] = {
  /* name, stock id */      { "Desktop Align to Grid", NULL,
  /* label, accelerator */    N_("Align to _grid"), NULL,
  /* tooltip */               N_("Keep icons aligned to a grid"),
                              G_CALLBACK (action_align_grid_callback),
                              0 },
  /* name, stock id */      { "Desktop Autoarrange", NULL,
  /* label, accelerator */    N_("_Auto-arrange"), NULL,
  /* tooltip */               N_("Keep icons automatically arranged"),
                              G_CALLBACK (action_auto_arrange_callback),
                              0 }
};

static const GtkRadioActionEntry desktop_size_radio_entries[] = {
  { "Desktop Small", NULL,
    N_("Smaller"), NULL,
    N_("Display smaller icons"),
    NEMO_ZOOM_LEVEL_SMALL },
  { "Desktop Normal", NULL,
    N_("Normal"), NULL,
    N_("Display normal-sized icons"),
    NEMO_ZOOM_LEVEL_STANDARD },
  { "Desktop Large", NULL,
    N_("Larger"), NULL,
    N_("Display larger icons"),
    NEMO_ZOOM_LEVEL_LARGE }
};

static const GtkActionEntry desktop_grid_entries[] = {
    /* name, stock id, label */  { "Desktop Submenu", NULL, N_("_Desktop") }, 
    /* name, stock id, label */  { "Desktop Zoom", NULL, N_("_Icon Size") }, 
    /* name, stock id */
    { "Empty Trash Conditional", NULL,
      /* label, accelerator */
      N_("Empty Trash"), NULL,
      /* tooltip */
      N_("Delete all items in the Trash"),
      G_CALLBACK (action_empty_trash_conditional_callback) },
    { "Desktop Sort by Name", NULL,
      N_("By _Name"), NULL,
      N_("Keep icons sorted by name in rows"),
      G_CALLBACK (action_sort_name) },
    { "Desktop Sort by Size", NULL,
      N_("By _Size"), NULL,
      N_("Keep icons sorted by size in rows"),
      G_CALLBACK (action_sort_size) },
    { "Desktop Sort by Type", NULL,
      N_("By _Detailed Type"), NULL,
      N_("Keep icons sorted by detailed type in rows"),
      G_CALLBACK (action_sort_type) },
    { "Desktop Sort by Date", NULL,
      N_("By Modification _Date"), NULL,
      N_("Keep icons sorted by modification date in rows"),
      G_CALLBACK (action_sort_date) },
    { "Vertical Layout", NULL,
      N_("_Vertical"), NULL,
      N_("Arrange icons vertically in stacks"),
      G_CALLBACK (action_vertical_layout) },
    { "Horizontal Layout", NULL,
      N_("_Horizontal"), NULL,
      N_("Arrange icons horizontally in rows"),
      G_CALLBACK (action_horizontal_layout) }
};

static void
real_merge_menus (NemoView *view)
{
	NemoDesktopIconGridView *desktop_view;
    GtkUIManager *ui_manager;
	GtkActionGroup *action_group;

	NEMO_VIEW_CLASS (nemo_desktop_icon_grid_view_parent_class)->merge_menus (view);

	desktop_view = NEMO_DESKTOP_ICON_GRID_VIEW (view);

	ui_manager = nemo_view_get_ui_manager (view);

	action_group = gtk_action_group_new ("DesktopViewActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	desktop_view->details->desktop_action_group = action_group;

    gtk_action_group_add_actions (action_group,
                                  desktop_grid_entries,
                                  G_N_ELEMENTS (desktop_grid_entries),
                                  view);

    gtk_action_group_add_toggle_actions (action_group,
                                         desktop_grid_toggle_entries,
                                         G_N_ELEMENTS (desktop_grid_toggle_entries),
                                         view);

    gtk_action_group_add_radio_actions (action_group,
                                        desktop_size_radio_entries,
                                        G_N_ELEMENTS (desktop_size_radio_entries),
                                        -1,
                                        G_CALLBACK (action_desktop_size_callback),
                                        view);

    gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
    g_object_unref (action_group); /* owned by ui manager */

	desktop_view->details->desktop_merge_id =
		gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/nemo/nemo-desktop-icon-grid-view-ui.xml", NULL);
}

static NemoView *
nemo_desktop_icon_grid_view_create (NemoWindowSlot *slot)
{
	NemoIconView *view;

	view = g_object_new (NEMO_TYPE_DESKTOP_ICON_GRID_VIEW,
			     "window-slot", slot,
			     "supports-zooming", TRUE,
			     "supports-auto-layout", TRUE,
			     "is-desktop", TRUE,
			     "supports-keep-aligned", TRUE,
			     "supports-labels-beside-icons", FALSE,
			     NULL);
	return NEMO_VIEW (view);
}

static gboolean
nemo_desktop_icon_grid_view_supports_uri (const char *uri,
				   GFileType file_type,
				   const char *mime_type)
{
	if (g_str_has_prefix (uri, EEL_DESKTOP_URI)) {
		return TRUE;
	}

	return FALSE;
}

static NemoViewInfo nemo_desktop_icon_grid_view = {
	NEMO_DESKTOP_ICON_GRID_VIEW_IID,
	"Desktop Grid View",
	"_Desktop",
	N_("The desktop view encountered an error."),
	N_("The desktop view encountered an error while starting up."),
	"Display this location with the desktop grid view.",
	nemo_desktop_icon_grid_view_create,
	nemo_desktop_icon_grid_view_supports_uri
};

void
nemo_desktop_icon_grid_view_register (void)
{
	nemo_desktop_icon_grid_view.error_label = _(nemo_desktop_icon_grid_view.error_label);
	nemo_desktop_icon_grid_view.startup_error_label = _(nemo_desktop_icon_grid_view.startup_error_label);
	
	nemo_view_factory_register (&nemo_desktop_icon_grid_view);
}

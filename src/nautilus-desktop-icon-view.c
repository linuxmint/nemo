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
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Mike Engber <engber@eazel.com>
   	    Gene Z. Ragan <gzr@eazel.com>
	    Miguel de Icaza <miguel@ximian.com>
*/

#include <config.h>

#include "nautilus-desktop-icon-view.h"

#include "nautilus-actions.h"
#include "nautilus-icon-view-container.h"
#include "nautilus-view-factory.h"
#include "nautilus-view.h"

#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <fcntl.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <libnautilus-private/nautilus-desktop-background.h>
#include <libnautilus-private/nautilus-desktop-icon-file.h>
#include <libnautilus-private/nautilus-directory-notify.h>
#include <libnautilus-private/nautilus-file-changes-queue.h>
#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-link.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-monitor.h>
#include <libnautilus-private/nautilus-program-choosing.h>
#include <libnautilus-private/nautilus-trash-monitor.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Timeout to check the desktop directory for updates */
#define RESCAN_TIMEOUT 4

struct NautilusDesktopIconViewDetails
{
	GdkWindow *root_window;
	GtkActionGroup *desktop_action_group;
	guint desktop_merge_id;

	/* For the desktop rescanning
	 */
	gulong delayed_init_signal;
	guint reload_desktop_timeout;
	gboolean pending_rescan;

	NautilusDesktopBackground *background;
};

static void     default_zoom_level_changed                        (gpointer                user_data);
static void     real_merge_menus                                  (NautilusView        *view);
static void     real_update_menus                                 (NautilusView        *view);
static void     nautilus_desktop_icon_view_update_icon_container_fonts  (NautilusDesktopIconView      *view);
static void     font_changed_callback                             (gpointer                callback_data);

G_DEFINE_TYPE (NautilusDesktopIconView, nautilus_desktop_icon_view, NAUTILUS_TYPE_ICON_VIEW)

static char *desktop_directory;
static time_t desktop_dir_modify_time;

#define get_icon_container(w) nautilus_icon_view_get_icon_container(NAUTILUS_ICON_VIEW (w))

static void
desktop_directory_changed_callback (gpointer callback_data)
{
	g_free (desktop_directory);
	desktop_directory = nautilus_get_desktop_directory ();
}

static void
icon_container_set_workarea (NautilusIconContainer *icon_container,
			     GdkScreen             *screen,
			     long                  *workareas,
			     int                    n_items)
{
	int left, right, top, bottom;
	int screen_width, screen_height;
	int i;

	left = right = top = bottom = 0;

	screen_width  = gdk_screen_get_width (screen);
	screen_height = gdk_screen_get_height (screen);

	for (i = 0; i < n_items; i += 4) {
		int x      = workareas [i];
		int y      = workareas [i + 1];
		int width  = workareas [i + 2];
		int height = workareas [i + 3];

		if ((x + width) > screen_width || (y + height) > screen_height)
			continue;

		left   = MAX (left, x);
		right  = MAX (right, screen_width - width - x);
		top    = MAX (top, y);
		bottom = MAX (bottom, screen_height - height - y);
	}

	nautilus_icon_container_set_margins (icon_container,
					     left, right, top, bottom);
}

static void
net_workarea_changed (NautilusDesktopIconView *icon_view,
		      GdkWindow         *window)
{
	long *nworkareas = NULL;
	long *workareas = NULL;
	GdkAtom type_returned;
	int format_returned;
	int length_returned;
	NautilusIconContainer *icon_container;
	GdkScreen *screen;

	g_return_if_fail (NAUTILUS_IS_DESKTOP_ICON_VIEW (icon_view));

	icon_container = get_icon_container (icon_view);

	/* Find the number of desktops so we know how long the
	 * workareas array is going to be (each desktop will have four
	 * elements in the workareas array describing
	 * x,y,width,height) */
	gdk_error_trap_push ();
	if (!gdk_property_get (window,
			       gdk_atom_intern ("_NET_NUMBER_OF_DESKTOPS", FALSE),
			       gdk_x11_xatom_to_atom (XA_CARDINAL),
			       0, 4, FALSE,
			       &type_returned,
			       &format_returned,
			       &length_returned,
			       (guchar **) &nworkareas)) {
		g_warning("Can not calculate _NET_NUMBER_OF_DESKTOPS");
	}
	if (gdk_error_trap_pop()
	    || nworkareas == NULL
	    || type_returned != gdk_x11_xatom_to_atom (XA_CARDINAL)
	    || format_returned != 32)
		g_warning("Can not calculate _NET_NUMBER_OF_DESKTOPS");
	
	/* Note : gdk_property_get() is broken (API documents admit
	 * this).  As a length argument, it expects the number of
	 * _bytes_ of data you require.  Internally, gdk_property_get
	 * converts that value to a count of 32 bit (4 byte) elements.
	 * However, the length returned is in bytes, but is calculated
	 * via the count of returned elements * sizeof(long).  This
	 * means on a 64 bit system, the number of bytes you have to
	 * request does not correspond to the number of bytes you get
	 * back, and is the reason for the workaround below.
	 */ 
	gdk_error_trap_push ();
	if (nworkareas == NULL || (*nworkareas < 1) 
	    || !gdk_property_get (window,
				  gdk_atom_intern ("_NET_WORKAREA", FALSE),
				  gdk_x11_xatom_to_atom (XA_CARDINAL),
				  0, ((*nworkareas) * 4 * 4), FALSE,
				  &type_returned,
				  &format_returned,
				  &length_returned,
				  (guchar **) &workareas)) {
		g_warning("Can not get _NET_WORKAREA");
		workareas = NULL;
	}

	if (gdk_error_trap_pop ()
	    || workareas == NULL
	    || type_returned != gdk_x11_xatom_to_atom (XA_CARDINAL)
	    || ((*nworkareas) * 4 * sizeof(long)) != length_returned
	    || format_returned != 32) {
		g_warning("Can not determine workarea, guessing at layout");
		nautilus_icon_container_set_margins (icon_container,
						     0, 0, 0, 0);
	} else {
		screen = gdk_window_get_screen (window);

		icon_container_set_workarea
			(icon_container, screen, workareas, length_returned / sizeof (long));
	}

	if (nworkareas != NULL)
		g_free (nworkareas);

	if (workareas != NULL)
		g_free (workareas);
}

static GdkFilterReturn
desktop_icon_view_property_filter (GdkXEvent *gdk_xevent,
				   GdkEvent *event,
				   gpointer data)
{
	XEvent *xevent = gdk_xevent;
	NautilusDesktopIconView *icon_view;

	icon_view = NAUTILUS_DESKTOP_ICON_VIEW (data);
  
	switch (xevent->type) {
	case PropertyNotify:
		if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name ("_NET_WORKAREA"))
			net_workarea_changed (icon_view, event->any.window);
		break;
	default:
		break;
	}

	return GDK_FILTER_CONTINUE;
}

static void
real_begin_loading (NautilusView *object)
{
	NautilusIconContainer *icon_container;
	NautilusDesktopIconView *view;

	view = NAUTILUS_DESKTOP_ICON_VIEW (object);

	icon_container = get_icon_container (view);
	if (view->details->background == NULL) {
		view->details->background = nautilus_desktop_background_new (icon_container);
	}

	NAUTILUS_VIEW_CLASS (nautilus_desktop_icon_view_parent_class)->begin_loading (object);
}

static const char *
real_get_id (NautilusView *view)
{
	return NAUTILUS_DESKTOP_ICON_VIEW_ID;
}

static void
nautilus_desktop_icon_view_dispose (GObject *object)
{
	NautilusDesktopIconView *icon_view;
	GtkUIManager *ui_manager;

	icon_view = NAUTILUS_DESKTOP_ICON_VIEW (object);

	/* Remove desktop rescan timeout. */
	if (icon_view->details->reload_desktop_timeout != 0) {
		g_source_remove (icon_view->details->reload_desktop_timeout);
		icon_view->details->reload_desktop_timeout = 0;
	}

	ui_manager = nautilus_view_get_ui_manager (NAUTILUS_VIEW (icon_view));
	if (ui_manager != NULL) {
		nautilus_ui_unmerge_ui (ui_manager,
					&icon_view->details->desktop_merge_id,
					&icon_view->details->desktop_action_group);
	}

	g_signal_handlers_disconnect_by_func (nautilus_icon_view_preferences,
					      default_zoom_level_changed,
					      icon_view);
	g_signal_handlers_disconnect_by_func (nautilus_preferences,
					      font_changed_callback,
					      icon_view);

	g_signal_handlers_disconnect_by_func (nautilus_preferences,
					      desktop_directory_changed_callback,
					      NULL);

	g_signal_handlers_disconnect_by_func (gnome_lockdown_preferences,
					      nautilus_view_update_menus,
					      icon_view);

	if (icon_view->details->background != NULL) {
		g_object_unref (icon_view->details->background);
		icon_view->details->background = NULL;
	}

	G_OBJECT_CLASS (nautilus_desktop_icon_view_parent_class)->dispose (object);
}

static void
nautilus_desktop_icon_view_class_init (NautilusDesktopIconViewClass *class)
{
	NautilusViewClass *vclass;

	vclass = NAUTILUS_VIEW_CLASS (class);

	G_OBJECT_CLASS (class)->dispose = nautilus_desktop_icon_view_dispose;

	vclass->begin_loading = real_begin_loading;
	vclass->merge_menus = real_merge_menus;
	vclass->update_menus = real_update_menus;
	vclass->get_view_id = real_get_id;

	g_type_class_add_private (class, sizeof (NautilusDesktopIconViewDetails));
}

static void
nautilus_desktop_icon_view_handle_middle_click (NautilusIconContainer *icon_container,
						GdkEventButton *event,
						NautilusDesktopIconView *desktop_icon_view)
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
unrealized_callback (GtkWidget *widget, NautilusDesktopIconView *desktop_icon_view)
{
	g_return_if_fail (desktop_icon_view->details->root_window != NULL);

	/* Remove the property filter */
	gdk_window_remove_filter (desktop_icon_view->details->root_window,
				  desktop_icon_view_property_filter,
				  desktop_icon_view);
	desktop_icon_view->details->root_window = NULL;
}

static void
realized_callback (GtkWidget *widget, NautilusDesktopIconView *desktop_icon_view)
{
	GdkWindow *root_window;
	GdkScreen *screen;
	GtkAllocation allocation;

	g_return_if_fail (desktop_icon_view->details->root_window == NULL);

	screen = gtk_widget_get_screen (widget);

	/* Ugly HACK for the problem that the views realize at the
	 * wrong size and then get resized. (This is a problem with
	 * BonoboPlug.) This was leading to problems where initial
	 * layout was done at 60x60 stacking all desktop icons in
	 * the top left corner.
	 */
	allocation.x = 0;
	allocation.y = 0;
	allocation.width = gdk_screen_get_width (screen);
	allocation.height = gdk_screen_get_height (screen);
	gtk_widget_size_allocate (GTK_WIDGET(get_icon_container(desktop_icon_view)),
				  &allocation);
	
	root_window = gdk_screen_get_root_window (screen);

	desktop_icon_view->details->root_window = root_window;

	/* Read out the workarea geometry and update the icon container accordingly */
	net_workarea_changed (desktop_icon_view, root_window);

	/* Setup the property filter */
	gdk_window_set_events (root_window, GDK_PROPERTY_CHANGE_MASK);
	gdk_window_add_filter (root_window,
			       desktop_icon_view_property_filter,
			       desktop_icon_view);
}

static NautilusZoomLevel
get_default_zoom_level (void)
{
	NautilusZoomLevel default_zoom_level;

	default_zoom_level = g_settings_get_enum (nautilus_icon_view_preferences,
						  NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL);

	return CLAMP (default_zoom_level, NAUTILUS_ZOOM_LEVEL_SMALLEST, NAUTILUS_ZOOM_LEVEL_LARGEST);
}

static void
default_zoom_level_changed (gpointer user_data)
{
	NautilusZoomLevel new_level;
	NautilusDesktopIconView *desktop_icon_view;

	desktop_icon_view = NAUTILUS_DESKTOP_ICON_VIEW (user_data);
	new_level = get_default_zoom_level ();

	nautilus_icon_container_set_zoom_level (get_icon_container (desktop_icon_view),
						new_level);
}

static gboolean
do_desktop_rescan (gpointer data)
{
	NautilusDesktopIconView *desktop_icon_view;
	struct stat buf;

	desktop_icon_view = NAUTILUS_DESKTOP_ICON_VIEW (data);
	if (desktop_icon_view->details->pending_rescan) {
		return TRUE;
	}
	
	if (stat (desktop_directory, &buf) == -1) {
		return TRUE;
	}

	if (buf.st_ctime == desktop_dir_modify_time) {
		return TRUE;
	}

	desktop_icon_view->details->pending_rescan = TRUE;

	nautilus_directory_force_reload
		(nautilus_view_get_model (NAUTILUS_VIEW (desktop_icon_view)));

	return TRUE;
}

static void
done_loading (NautilusDirectory *model,
	      NautilusDesktopIconView *desktop_icon_view)
{
	struct stat buf;

	desktop_icon_view->details->pending_rescan = FALSE;
	if (stat (desktop_directory, &buf) == -1) {
		return;
	}

	desktop_dir_modify_time = buf.st_ctime;
}

/* This function is used because the NautilusDirectory model does not
 * exist always in the desktop_icon_view, so we wait until it has been
 * instantiated.
 */
static void
delayed_init (NautilusDesktopIconView *desktop_icon_view)
{
	/* Keep track of the load time. */
	g_signal_connect_object (nautilus_view_get_model (NAUTILUS_VIEW (desktop_icon_view)),
				 "done_loading",
				 G_CALLBACK (done_loading), desktop_icon_view, 0);

	/* Monitor desktop directory. */
	desktop_icon_view->details->reload_desktop_timeout =
		g_timeout_add_seconds (RESCAN_TIMEOUT, do_desktop_rescan, desktop_icon_view);

	g_signal_handler_disconnect (desktop_icon_view,
				     desktop_icon_view->details->delayed_init_signal);

	desktop_icon_view->details->delayed_init_signal = 0;
}

static void
font_changed_callback (gpointer callback_data)
{
 	g_return_if_fail (NAUTILUS_IS_DESKTOP_ICON_VIEW (callback_data));
	
	nautilus_desktop_icon_view_update_icon_container_fonts (NAUTILUS_DESKTOP_ICON_VIEW (callback_data));
}

static void
nautilus_desktop_icon_view_update_icon_container_fonts (NautilusDesktopIconView *icon_view)
{
	NautilusIconContainer *icon_container;
	char *font;

	icon_container = get_icon_container (icon_view);
	g_assert (icon_container != NULL);

	font = g_settings_get_string (nautilus_desktop_preferences,
				      NAUTILUS_PREFERENCES_DESKTOP_FONT);

	nautilus_icon_container_set_font (icon_container, font);

	g_free (font);
}

static void
nautilus_desktop_icon_view_init (NautilusDesktopIconView *desktop_icon_view)
{
	NautilusIconContainer *icon_container;
	GtkAllocation allocation;
	GtkAdjustment *hadj, *vadj;

	desktop_icon_view->details = G_TYPE_INSTANCE_GET_PRIVATE (desktop_icon_view,
								  NAUTILUS_TYPE_DESKTOP_ICON_VIEW,
								  NautilusDesktopIconViewDetails);

	if (desktop_directory == NULL) {
		g_signal_connect_swapped (nautilus_preferences, "changed::" NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR,
					  G_CALLBACK(desktop_directory_changed_callback),
					  NULL);
		desktop_directory_changed_callback (NULL);
	}

	nautilus_icon_view_filter_by_screen (NAUTILUS_ICON_VIEW (desktop_icon_view), TRUE);
	icon_container = get_icon_container (desktop_icon_view);
	nautilus_icon_container_set_use_drop_shadows (icon_container, TRUE);
	nautilus_icon_view_container_set_sort_desktop (NAUTILUS_ICON_VIEW_CONTAINER (icon_container), TRUE);

	/* Do a reload on the desktop if we don't have FAM, a smarter
	 * way to keep track of the items on the desktop.
	 */
	if (!nautilus_monitor_active ()) {
		desktop_icon_view->details->delayed_init_signal = g_signal_connect_object
			(desktop_icon_view, "begin_loading",
			 G_CALLBACK (delayed_init), desktop_icon_view, 0);
	}
	
	nautilus_icon_container_set_is_fixed_size (icon_container, TRUE);
	nautilus_icon_container_set_is_desktop (icon_container, TRUE);
	nautilus_icon_container_set_store_layout_timestamps (icon_container, TRUE);

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

	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (desktop_icon_view),
					     GTK_SHADOW_NONE);

	nautilus_view_ignore_hidden_file_preferences
		(NAUTILUS_VIEW (desktop_icon_view));

	nautilus_view_set_show_foreign (NAUTILUS_VIEW (desktop_icon_view),
					FALSE);
	
	/* Set our default layout mode */
	nautilus_icon_container_set_layout_mode (icon_container,
						 gtk_widget_get_direction (GTK_WIDGET(icon_container)) == GTK_TEXT_DIR_RTL ?
						 NAUTILUS_ICON_LAYOUT_T_B_R_L :
						 NAUTILUS_ICON_LAYOUT_T_B_L_R);

	g_signal_connect_object (icon_container, "middle_click",
				 G_CALLBACK (nautilus_desktop_icon_view_handle_middle_click), desktop_icon_view, 0);
	g_signal_connect_object (desktop_icon_view, "realize",
				 G_CALLBACK (realized_callback), desktop_icon_view, 0);
	g_signal_connect_object (desktop_icon_view, "unrealize",
				 G_CALLBACK (unrealized_callback), desktop_icon_view, 0);

	g_signal_connect_swapped (nautilus_icon_view_preferences,
				  "changed::" NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
				  G_CALLBACK (default_zoom_level_changed),
				  desktop_icon_view);

	g_signal_connect_swapped (nautilus_desktop_preferences,
				  "changed::" NAUTILUS_PREFERENCES_DESKTOP_FONT,
				  G_CALLBACK (font_changed_callback),
				  desktop_icon_view);

	default_zoom_level_changed (desktop_icon_view);
	nautilus_desktop_icon_view_update_icon_container_fonts (desktop_icon_view);

	g_signal_connect_swapped (gnome_lockdown_preferences,
				  "changed::" NAUTILUS_PREFERENCES_LOCKDOWN_COMMAND_LINE,
				  G_CALLBACK (nautilus_view_update_menus),
				  desktop_icon_view);
}

static void
action_change_background_callback (GtkAction *action, 
				   gpointer data)
{
        g_assert (NAUTILUS_VIEW (data));

	nautilus_launch_application_from_command (gtk_widget_get_screen (GTK_WIDGET (data)),
						  "gnome-control-center",
						  FALSE,
						  "background", NULL);
}

static void
action_empty_trash_conditional_callback (GtkAction *action,
					 gpointer data)
{
        g_assert (NAUTILUS_IS_VIEW (data));

	nautilus_file_operations_empty_trash (GTK_WIDGET (data));
}

static gboolean
trash_link_is_selection (NautilusView *view)
{
	GList *selection;
	NautilusDesktopLink *link;
	gboolean result;

	result = FALSE;
	
	selection = nautilus_view_get_selection (view);

	if (eel_g_list_exactly_one_item (selection) &&
	    NAUTILUS_IS_DESKTOP_ICON_FILE (selection->data)) {
		link = nautilus_desktop_icon_file_get_link (NAUTILUS_DESKTOP_ICON_FILE (selection->data));
		/* link may be NULL if the link was recently removed (unmounted) */
		if (link != NULL &&
		    nautilus_desktop_link_get_link_type (link) == NAUTILUS_DESKTOP_LINK_TRASH) {
			result = TRUE;
		}
		if (link) {
			g_object_unref (link);
		}
	}
	
	nautilus_file_list_free (selection);

	return result;
}

static void
real_update_menus (NautilusView *view)
{
	NautilusDesktopIconView *desktop_view;
	char *label;
	gboolean include_empty_trash;
	GtkAction *action;

	g_assert (NAUTILUS_IS_DESKTOP_ICON_VIEW (view));

	NAUTILUS_VIEW_CLASS (nautilus_desktop_icon_view_parent_class)->update_menus (view);

	desktop_view = NAUTILUS_DESKTOP_ICON_VIEW (view);

	/* Empty Trash */
	include_empty_trash = trash_link_is_selection (view);
	action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
					      NAUTILUS_ACTION_EMPTY_TRASH_CONDITIONAL);
	gtk_action_set_visible (action,
				include_empty_trash);
	if (include_empty_trash) {
		label = g_strdup (_("E_mpty Trash"));
		g_object_set (action , "label", label, NULL);
		gtk_action_set_sensitive (action,
					  !nautilus_trash_monitor_is_empty ());
		g_free (label);
	}
}

static const GtkActionEntry desktop_view_entries[] = {
	/* name, stock id */
	{ "Change Background", NULL,
	  /* label, accelerator */
	  N_("Change Desktop _Background"), NULL,
	  /* tooltip */
	  N_("Show a window that lets you set your desktop background's pattern or color"),
	  G_CALLBACK (action_change_background_callback) },
	/* name, stock id */
	{ "Empty Trash Conditional", NULL,
	  /* label, accelerator */
	  N_("Empty Trash"), NULL,
	  /* tooltip */
	  N_("Delete all items in the Trash"),
	  G_CALLBACK (action_empty_trash_conditional_callback) },
};

static void
real_merge_menus (NautilusView *view)
{
	NautilusDesktopIconView *desktop_view;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;

	NAUTILUS_VIEW_CLASS (nautilus_desktop_icon_view_parent_class)->merge_menus (view);

	desktop_view = NAUTILUS_DESKTOP_ICON_VIEW (view);

	ui_manager = nautilus_view_get_ui_manager (view);

	action_group = gtk_action_group_new ("DesktopViewActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	desktop_view->details->desktop_action_group = action_group;
	gtk_action_group_add_actions (action_group, 
				      desktop_view_entries, G_N_ELEMENTS (desktop_view_entries),
				      view);

	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui manager */

	desktop_view->details->desktop_merge_id =
		gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/gnome/nautilus/nautilus-desktop-icon-view-ui.xml", NULL);
}

static NautilusView *
nautilus_desktop_icon_view_create (NautilusWindowSlot *slot)
{
	NautilusIconView *view;

	view = g_object_new (NAUTILUS_TYPE_DESKTOP_ICON_VIEW,
			     "window-slot", slot,
			     "supports-zooming", FALSE,
			     "supports-auto-layout", FALSE,
			     "supports-scaling", TRUE,
			     "supports-keep-aligned", TRUE,
			     "supports-labels-beside-icons", FALSE,
			     NULL);
	return NAUTILUS_VIEW (view);
}

static gboolean
nautilus_desktop_icon_view_supports_uri (const char *uri,
				   GFileType file_type,
				   const char *mime_type)
{
	if (g_str_has_prefix (uri, EEL_DESKTOP_URI)) {
		return TRUE;
	}

	return FALSE;
}

static NautilusViewInfo nautilus_desktop_icon_view = {
	NAUTILUS_DESKTOP_ICON_VIEW_ID,
	"Desktop View",
	"_Desktop",
	N_("The desktop view encountered an error."),
	N_("The desktop view encountered an error while starting up."),
	"Display this location with the desktop view.",
	nautilus_desktop_icon_view_create,
	nautilus_desktop_icon_view_supports_uri
};

void
nautilus_desktop_icon_view_register (void)
{
	nautilus_desktop_icon_view.error_label = _(nautilus_desktop_icon_view.error_label);
	nautilus_desktop_icon_view.startup_error_label = _(nautilus_desktop_icon_view.startup_error_label);
	
	nautilus_view_factory_register (&nautilus_desktop_icon_view);
}

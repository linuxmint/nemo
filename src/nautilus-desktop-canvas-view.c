/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-desktop-canvas-view.c - implementation of canvas view for managing the desktop.

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

#include "nautilus-desktop-canvas-view.h"

#include "nautilus-actions.h"
#include "nautilus-canvas-view-container.h"
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

struct NautilusDesktopCanvasViewDetails
{
	GdkWindow *root_window;
	GtkActionGroup *desktop_action_group;
	guint desktop_merge_id;

	/* For the desktop rescanning
	 */
	gulong delayed_init_signal;
	guint reload_desktop_timeout;
	gboolean pending_rescan;
};

static void     default_zoom_level_changed                        (gpointer                user_data);
static void     real_merge_menus                                  (NautilusView        *view);
static void     real_update_menus                                 (NautilusView        *view);
static void     nautilus_desktop_canvas_view_update_canvas_container_fonts  (NautilusDesktopCanvasView      *view);
static void     font_changed_callback                             (gpointer                callback_data);

G_DEFINE_TYPE (NautilusDesktopCanvasView, nautilus_desktop_canvas_view, NAUTILUS_TYPE_CANVAS_VIEW)

static char *desktop_directory;
static time_t desktop_dir_modify_time;

#define get_canvas_container(w) nautilus_canvas_view_get_canvas_container(NAUTILUS_CANVAS_VIEW (w))

#define POPUP_PATH_CANVAS_APPEARANCE		"/selection/Canvas Appearance Items"

static void
canvas_container_set_workarea (NautilusCanvasContainer *canvas_container,
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

	nautilus_canvas_container_set_margins (canvas_container,
					     left, right, top, bottom);
}

static void
net_workarea_changed (NautilusDesktopCanvasView *canvas_view,
		      GdkWindow         *window)
{
	long *nworkareas = NULL;
	long *workareas = NULL;
	GdkAtom type_returned;
	int format_returned;
	int length_returned;
	NautilusCanvasContainer *canvas_container;
	GdkScreen *screen;

	g_return_if_fail (NAUTILUS_IS_DESKTOP_CANVAS_VIEW (canvas_view));

	canvas_container = get_canvas_container (canvas_view);

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
		nautilus_canvas_container_set_margins (canvas_container,
						     0, 0, 0, 0);
	} else {
		screen = gdk_window_get_screen (window);

		canvas_container_set_workarea
			(canvas_container, screen, workareas, length_returned / sizeof (long));
	}

	if (nworkareas != NULL)
		g_free (nworkareas);

	if (workareas != NULL)
		g_free (workareas);
}

static GdkFilterReturn
desktop_canvas_view_property_filter (GdkXEvent *gdk_xevent,
				   GdkEvent *event,
				   gpointer data)
{
	XEvent *xevent = gdk_xevent;
	NautilusDesktopCanvasView *canvas_view;

	canvas_view = NAUTILUS_DESKTOP_CANVAS_VIEW (data);
  
	switch (xevent->type) {
	case PropertyNotify:
		if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name ("_NET_WORKAREA"))
			net_workarea_changed (canvas_view, event->any.window);
		break;
	default:
		break;
	}

	return GDK_FILTER_CONTINUE;
}

static const char *
real_get_id (NautilusView *view)
{
	return NAUTILUS_DESKTOP_CANVAS_VIEW_ID;
}

static void
nautilus_desktop_canvas_view_dispose (GObject *object)
{
	NautilusDesktopCanvasView *canvas_view;
	GtkUIManager *ui_manager;

	canvas_view = NAUTILUS_DESKTOP_CANVAS_VIEW (object);

	/* Remove desktop rescan timeout. */
	if (canvas_view->details->reload_desktop_timeout != 0) {
		g_source_remove (canvas_view->details->reload_desktop_timeout);
		canvas_view->details->reload_desktop_timeout = 0;
	}

	ui_manager = nautilus_view_get_ui_manager (NAUTILUS_VIEW (canvas_view));
	if (ui_manager != NULL) {
		nautilus_ui_unmerge_ui (ui_manager,
					&canvas_view->details->desktop_merge_id,
					&canvas_view->details->desktop_action_group);
	}

	g_signal_handlers_disconnect_by_func (nautilus_icon_view_preferences,
					      default_zoom_level_changed,
					      canvas_view);
	g_signal_handlers_disconnect_by_func (nautilus_preferences,
					      font_changed_callback,
					      canvas_view);
	g_signal_handlers_disconnect_by_func (gnome_lockdown_preferences,
					      nautilus_view_update_menus,
					      canvas_view);

	G_OBJECT_CLASS (nautilus_desktop_canvas_view_parent_class)->dispose (object);
}

static void
nautilus_desktop_canvas_view_class_init (NautilusDesktopCanvasViewClass *class)
{
	NautilusViewClass *vclass;

	vclass = NAUTILUS_VIEW_CLASS (class);

	G_OBJECT_CLASS (class)->dispose = nautilus_desktop_canvas_view_dispose;

	vclass->merge_menus = real_merge_menus;
	vclass->update_menus = real_update_menus;
	vclass->get_view_id = real_get_id;

	g_type_class_add_private (class, sizeof (NautilusDesktopCanvasViewDetails));
}

static void
unrealized_callback (GtkWidget *widget, NautilusDesktopCanvasView *desktop_canvas_view)
{
	g_return_if_fail (desktop_canvas_view->details->root_window != NULL);

	/* Remove the property filter */
	gdk_window_remove_filter (desktop_canvas_view->details->root_window,
				  desktop_canvas_view_property_filter,
				  desktop_canvas_view);
	desktop_canvas_view->details->root_window = NULL;
}

static void
realized_callback (GtkWidget *widget, NautilusDesktopCanvasView *desktop_canvas_view)
{
	GdkWindow *root_window;
	GdkScreen *screen;

	g_return_if_fail (desktop_canvas_view->details->root_window == NULL);

	screen = gtk_widget_get_screen (widget);
	root_window = gdk_screen_get_root_window (screen);

	desktop_canvas_view->details->root_window = root_window;

	/* Read out the workarea geometry and update the icon container accordingly */
	net_workarea_changed (desktop_canvas_view, root_window);

	/* Setup the property filter */
	gdk_window_set_events (root_window, GDK_PROPERTY_CHANGE_MASK);
	gdk_window_add_filter (root_window,
			       desktop_canvas_view_property_filter,
			       desktop_canvas_view);
}

static void
desktop_canvas_container_realize (GtkWidget *widget,
				  NautilusDesktopCanvasView *desktop_canvas_view)
{
	GdkWindow *bin_window;
	GdkRGBA transparent = { 0, 0, 0, 0 };

	bin_window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
	gdk_window_set_background_rgba (bin_window, &transparent);
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
	NautilusDesktopCanvasView *desktop_canvas_view;

	desktop_canvas_view = NAUTILUS_DESKTOP_CANVAS_VIEW (user_data);
	new_level = get_default_zoom_level ();

	nautilus_canvas_container_set_zoom_level (get_canvas_container (desktop_canvas_view),
						new_level);
}

static gboolean
do_desktop_rescan (gpointer data)
{
	NautilusDesktopCanvasView *desktop_canvas_view;
	struct stat buf;

	desktop_canvas_view = NAUTILUS_DESKTOP_CANVAS_VIEW (data);
	if (desktop_canvas_view->details->pending_rescan) {
		return TRUE;
	}
	
	if (stat (desktop_directory, &buf) == -1) {
		return TRUE;
	}

	if (buf.st_ctime == desktop_dir_modify_time) {
		return TRUE;
	}

	desktop_canvas_view->details->pending_rescan = TRUE;

	nautilus_directory_force_reload
		(nautilus_view_get_model (NAUTILUS_VIEW (desktop_canvas_view)));

	return TRUE;
}

static void
done_loading (NautilusDirectory *model,
	      NautilusDesktopCanvasView *desktop_canvas_view)
{
	struct stat buf;

	desktop_canvas_view->details->pending_rescan = FALSE;
	if (stat (desktop_directory, &buf) == -1) {
		return;
	}

	desktop_dir_modify_time = buf.st_ctime;
}

/* This function is used because the NautilusDirectory model does not
 * exist always in the desktop_canvas_view, so we wait until it has been
 * instantiated.
 */
static void
delayed_init (NautilusDesktopCanvasView *desktop_canvas_view)
{
	/* Keep track of the load time. */
	g_signal_connect_object (nautilus_view_get_model (NAUTILUS_VIEW (desktop_canvas_view)),
				 "done-loading",
				 G_CALLBACK (done_loading), desktop_canvas_view, 0);

	/* Monitor desktop directory. */
	desktop_canvas_view->details->reload_desktop_timeout =
		g_timeout_add_seconds (RESCAN_TIMEOUT, do_desktop_rescan, desktop_canvas_view);

	g_signal_handler_disconnect (desktop_canvas_view,
				     desktop_canvas_view->details->delayed_init_signal);

	desktop_canvas_view->details->delayed_init_signal = 0;
}

static void
font_changed_callback (gpointer callback_data)
{
 	g_return_if_fail (NAUTILUS_IS_DESKTOP_CANVAS_VIEW (callback_data));
	
	nautilus_desktop_canvas_view_update_canvas_container_fonts (NAUTILUS_DESKTOP_CANVAS_VIEW (callback_data));
}

static void
nautilus_desktop_canvas_view_update_canvas_container_fonts (NautilusDesktopCanvasView *canvas_view)
{
	NautilusCanvasContainer *canvas_container;
	char *font;

	canvas_container = get_canvas_container (canvas_view);
	g_assert (canvas_container != NULL);

	font = g_settings_get_string (nautilus_desktop_preferences,
				      NAUTILUS_PREFERENCES_DESKTOP_FONT);

	nautilus_canvas_container_set_font (canvas_container, font);

	g_free (font);
}

static void
nautilus_desktop_canvas_view_init (NautilusDesktopCanvasView *desktop_canvas_view)
{
	NautilusCanvasContainer *canvas_container;
	GtkAllocation allocation;
	GtkAdjustment *hadj, *vadj;

	desktop_canvas_view->details = G_TYPE_INSTANCE_GET_PRIVATE (desktop_canvas_view,
								  NAUTILUS_TYPE_DESKTOP_CANVAS_VIEW,
								  NautilusDesktopCanvasViewDetails);

	if (desktop_directory == NULL) {
		desktop_directory = nautilus_get_desktop_directory ();
	}

	nautilus_canvas_view_filter_by_screen (NAUTILUS_CANVAS_VIEW (desktop_canvas_view), TRUE);
	canvas_container = get_canvas_container (desktop_canvas_view);
	nautilus_canvas_container_set_use_drop_shadows (canvas_container, TRUE);
	nautilus_canvas_view_container_set_sort_desktop (NAUTILUS_CANVAS_VIEW_CONTAINER (canvas_container), TRUE);

	/* Do a reload on the desktop if we don't have FAM, a smarter
	 * way to keep track of the items on the desktop.
	 */
	if (!nautilus_monitor_active ()) {
		desktop_canvas_view->details->delayed_init_signal = g_signal_connect_object
			(desktop_canvas_view, "begin-loading",
			 G_CALLBACK (delayed_init), desktop_canvas_view, 0);
	}
	
	nautilus_canvas_container_set_is_fixed_size (canvas_container, TRUE);
	nautilus_canvas_container_set_is_desktop (canvas_container, TRUE);
	nautilus_canvas_container_set_store_layout_timestamps (canvas_container, TRUE);

	/* Set allocation to be at 0, 0 */
	gtk_widget_get_allocation (GTK_WIDGET (canvas_container), &allocation);
	allocation.x = 0;
	allocation.y = 0;
	gtk_widget_set_allocation (GTK_WIDGET (canvas_container), &allocation);
	
	gtk_widget_queue_resize (GTK_WIDGET (canvas_container));

	hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (canvas_container));
	vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (canvas_container));

	gtk_adjustment_set_value (hadj, 0);
	gtk_adjustment_set_value (vadj, 0);

	nautilus_view_ignore_hidden_file_preferences
		(NAUTILUS_VIEW (desktop_canvas_view));

	nautilus_view_set_show_foreign (NAUTILUS_VIEW (desktop_canvas_view),
					FALSE);

	g_signal_connect_object (canvas_container, "realize",
				 G_CALLBACK (desktop_canvas_container_realize), desktop_canvas_view, 0);

	g_signal_connect_object (desktop_canvas_view, "realize",
				 G_CALLBACK (realized_callback), desktop_canvas_view, 0);
	g_signal_connect_object (desktop_canvas_view, "unrealize",
				 G_CALLBACK (unrealized_callback), desktop_canvas_view, 0);

	g_signal_connect_swapped (nautilus_icon_view_preferences,
				  "changed::" NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
				  G_CALLBACK (default_zoom_level_changed),
				  desktop_canvas_view);

	g_signal_connect_swapped (nautilus_desktop_preferences,
				  "changed::" NAUTILUS_PREFERENCES_DESKTOP_FONT,
				  G_CALLBACK (font_changed_callback),
				  desktop_canvas_view);

	default_zoom_level_changed (desktop_canvas_view);
	nautilus_desktop_canvas_view_update_canvas_container_fonts (desktop_canvas_view);

	g_signal_connect_swapped (gnome_lockdown_preferences,
				  "changed::" NAUTILUS_PREFERENCES_LOCKDOWN_COMMAND_LINE,
				  G_CALLBACK (nautilus_view_update_menus),
				  desktop_canvas_view);
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

static void
action_stretch_callback (GtkAction *action,
			 gpointer callback_data)
{
	nautilus_canvas_container_show_stretch_handles
		(get_canvas_container (callback_data));
}

static void
action_unstretch_callback (GtkAction *action,
			   gpointer callback_data)
{
	nautilus_canvas_container_unstretch (get_canvas_container (callback_data));
}

static void
action_clean_up_callback (GtkAction *action,
			  gpointer callback_data)
{
	nautilus_canvas_view_clean_up_by_name (NAUTILUS_CANVAS_VIEW (callback_data));
}

static gboolean
trash_link_is_selection (NautilusView *view)
{
	GList *selection;
	NautilusDesktopLink *link;
	gboolean result;

	result = FALSE;
	
	selection = nautilus_view_get_selection (view);

	if ((g_list_length (selection) == 1) &&
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
	NautilusDesktopCanvasView *desktop_view;
        NautilusCanvasContainer *canvas_container;
	gboolean include_empty_trash;
	char *label;
	GtkAction *action;
        int selection_count;

	g_assert (NAUTILUS_IS_DESKTOP_CANVAS_VIEW (view));

	NAUTILUS_VIEW_CLASS (nautilus_desktop_canvas_view_parent_class)->update_menus (view);

	desktop_view = NAUTILUS_DESKTOP_CANVAS_VIEW (view);

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

	/* Stretch */
        selection_count = nautilus_view_get_selection_count (view);
        canvas_container = get_canvas_container (desktop_view);

	action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
					      NAUTILUS_ACTION_STRETCH);
	gtk_action_set_sensitive (action,
				  selection_count == 1
				  && canvas_container != NULL
				  && !nautilus_canvas_container_has_stretch_handles (canvas_container));
	gtk_action_set_visible (action, TRUE);

	/* Unstretch */
	action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
					      NAUTILUS_ACTION_UNSTRETCH);
	g_object_set (action, "label",
		      (selection_count > 1)
		      ? _("Restore Icons' Original Si_zes")
		      : _("Restore Icon's Original Si_ze"),
		      NULL);
	gtk_action_set_sensitive (action,
				  canvas_container != NULL
				  && nautilus_canvas_container_is_stretched (canvas_container));
	gtk_action_set_visible (action, TRUE);
}

static const GtkActionEntry desktop_view_entries[] = {
	/* name, stock id */
	{ NAUTILUS_ACTION_CHANGE_BACKGROUND, NULL,
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
	/* name, stock id */
	{ NAUTILUS_ACTION_CLEAN_UP, NULL,
	  /* label, accelerator */
	  N_("_Organize Desktop by Name"), NULL,
	  /* tooltip */
	  N_("Reposition icons to better fit in the window and avoid overlapping"),
	  G_CALLBACK (action_clean_up_callback) },
	/* name, stock id */
         { "Stretch", NULL,
	   /* label, accelerator */
	   N_("Resize Icon…"), NULL,
	   /* tooltip */
	   N_("Make the selected icons resizable"),
	   G_CALLBACK (action_stretch_callback) },
	/* name, stock id */
	{ "Unstretch", NULL,
	  /* label, accelerator */
	  N_("Restore Icons' Original Si_zes"), NULL,
	  /* tooltip */
	  N_("Restore each selected icons to its original size"),
	  G_CALLBACK (action_unstretch_callback) },
};

static void
real_merge_menus (NautilusView *view)
{
	NautilusDesktopCanvasView *desktop_view;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GtkAction *action;
	gchar *control_center_path;

	NAUTILUS_VIEW_CLASS (nautilus_desktop_canvas_view_parent_class)->merge_menus (view);

	desktop_view = NAUTILUS_DESKTOP_CANVAS_VIEW (view);

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
		gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/gnome/nautilus/nautilus-desktop-canvas-view-ui.xml", NULL);

	gtk_ui_manager_add_ui (ui_manager,
			       desktop_view->details->desktop_merge_id,
			       POPUP_PATH_CANVAS_APPEARANCE,
			       NAUTILUS_ACTION_STRETCH,
			       NAUTILUS_ACTION_STRETCH,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);
	gtk_ui_manager_add_ui (ui_manager,
			       desktop_view->details->desktop_merge_id,
			       POPUP_PATH_CANVAS_APPEARANCE,
			       NAUTILUS_ACTION_UNSTRETCH,
			       NAUTILUS_ACTION_UNSTRETCH,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);

	control_center_path = g_find_program_in_path ("gnome-control-center");
	if (control_center_path == NULL) {
		action = gtk_action_group_get_action (action_group, NAUTILUS_ACTION_CHANGE_BACKGROUND);
		gtk_action_set_visible (action, FALSE);
	}

	g_free (control_center_path);
}

static NautilusView *
nautilus_desktop_canvas_view_create (NautilusWindowSlot *slot)
{
	NautilusCanvasView *view;

	view = g_object_new (NAUTILUS_TYPE_DESKTOP_CANVAS_VIEW,
			     "window-slot", slot,
			     "supports-zooming", FALSE,
			     "supports-auto-layout", FALSE,
			     "supports-manual-layout", TRUE,
			     "supports-scaling", TRUE,
			     "supports-keep-aligned", TRUE,
			     NULL);
	return NAUTILUS_VIEW (view);
}

static gboolean
nautilus_desktop_canvas_view_supports_uri (const char *uri,
				   GFileType file_type,
				   const char *mime_type)
{
	if (g_str_has_prefix (uri, EEL_DESKTOP_URI)) {
		return TRUE;
	}

	return FALSE;
}

static NautilusViewInfo nautilus_desktop_canvas_view = {
	NAUTILUS_DESKTOP_CANVAS_VIEW_ID,
	"Desktop View",
	"_Desktop",
	N_("The desktop view encountered an error."),
	N_("The desktop view encountered an error while starting up."),
	"Display this location with the desktop view.",
	nautilus_desktop_canvas_view_create,
	nautilus_desktop_canvas_view_supports_uri
};

void
nautilus_desktop_canvas_view_register (void)
{
	nautilus_desktop_canvas_view.error_label = _(nautilus_desktop_canvas_view.error_label);
	nautilus_desktop_canvas_view.startup_error_label = _(nautilus_desktop_canvas_view.startup_error_label);
	
	nautilus_view_factory_register (&nautilus_desktop_canvas_view);
}

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-desktop-canvas-view.c - implementation of canvas view for managing the desktop.

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

#include "nemo-desktop-canvas-view.h"

#include "nemo-actions.h"
#include "nemo-canvas-view-container.h"
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
#include <libnemo-private/nemo-desktop-background.h>
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
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Timeout to check the desktop directory for updates */
#define RESCAN_TIMEOUT 4

struct NemoDesktopCanvasViewDetails
{
	GdkWindow *root_window;
	GtkActionGroup *desktop_action_group;
	guint desktop_merge_id;

	/* For the desktop rescanning
	 */
	gulong delayed_init_signal;
	guint reload_desktop_timeout;
	gboolean pending_rescan;

	NemoDesktopBackground *background;
};

static void     default_zoom_level_changed                        (gpointer                user_data);
static void     real_merge_menus                                  (NemoView        *view);
static void     real_update_menus                                 (NemoView        *view);
static void     nemo_desktop_canvas_view_update_canvas_container_fonts  (NemoDesktopCanvasView      *view);
static void     font_changed_callback                             (gpointer                callback_data);

G_DEFINE_TYPE (NemoDesktopCanvasView, nemo_desktop_canvas_view, NEMO_TYPE_CANVAS_VIEW)

static char *desktop_directory;
static time_t desktop_dir_modify_time;

#define get_canvas_container(w) nemo_canvas_view_get_canvas_container(NEMO_CANVAS_VIEW (w))

#define POPUP_PATH_CANVAS_APPEARANCE		"/selection/Canvas Appearance Items"

static void
canvas_container_set_workarea (NemoCanvasContainer *canvas_container,
			     GdkScreen             *screen,
			     long                  *workareas,
			     int                    n_items)
{
	int left, right, top, bottom;
	int screen_width, screen_height;
	int i;
    int ui_scale;

	left = right = top = bottom = 0;

	screen_width  = gdk_screen_get_width (screen);
	screen_height = gdk_screen_get_height (screen);

    ui_scale = gtk_widget_get_scale_factor (GTK_WIDGET (canvas_container));

    for (i = 0; i < n_items; i += 4) {
        int x      = workareas [i] / ui_scale;
        int y      = workareas [i + 1] / ui_scale;
        int width  = workareas [i + 2] / ui_scale;
        int height = workareas [i + 3] / ui_scale;
        if ((x + width) > screen_width || (y + height) > screen_height)
            continue;

		left   = MAX (left, x);
		right  = MAX (right, screen_width - width - x);
		top    = MAX (top, y);
		bottom = MAX (bottom, screen_height - height - y);
	}

	nemo_canvas_container_set_margins (canvas_container,
					     left, right, top, bottom);
}

static void
net_workarea_changed (NemoDesktopCanvasView *canvas_view,
		      GdkWindow         *window)
{
	long *nworkareas = NULL;
	long *workareas = NULL;
    Atom type_returned;
	int format_returned;
    unsigned long length_returned;
    unsigned long length_after_returned;
    NemoCanvasContainer *canvas_container;
	GdkScreen *screen;
    GdkDisplay *display = gdk_window_get_display (window);

	g_return_if_fail (NEMO_IS_DESKTOP_CANVAS_VIEW (canvas_view));

	canvas_container = get_canvas_container (canvas_view);

	/* Find the number of desktops so we know how long the
	 * workareas array is going to be (each desktop will have four
	 * elements in the workareas array describing
	 * x,y,width,height) */
	gdk_error_trap_push ();
    if (XGetWindowProperty (gdk_x11_display_get_xdisplay (display),
                            gdk_x11_window_get_xid (window),
                            gdk_x11_atom_to_xatom (gdk_atom_intern ("_NET_NUMBER_OF_DESKTOPS", FALSE)),
                            0, 4, False,
                            XA_CARDINAL,
                            &type_returned,
                            &format_returned,
                            &length_returned,
                            &length_after_returned,
                            (guchar **) &nworkareas
                            ) != Success) {
		g_warning("Can not calculate _NET_NUMBER_OF_DESKTOPS");
	}

	if (gdk_error_trap_pop()
	    || nworkareas == NULL
        || type_returned != XA_CARDINAL
	    || format_returned != 32)
		g_warning("Can not calculate _NET_NUMBER_OF_DESKTOPS");

	gdk_error_trap_push ();

	if (nworkareas == NULL || (*nworkareas < 1) 
        || XGetWindowProperty (gdk_x11_display_get_xdisplay (display),
                               gdk_x11_window_get_xid (window),
                               gdk_x11_atom_to_xatom (gdk_atom_intern ("_NET_WORKAREA", FALSE)),
                               0, ((*nworkareas) * 4 * 4), False,
                               XA_CARDINAL,
                               &type_returned,
                               &format_returned,
                               &length_returned,
                               &length_after_returned,
                               (guchar **) &workareas
                               ) != Success) {
		g_warning("Can not get _NET_WORKAREA");
		workareas = NULL;
	}

    if (gdk_error_trap_pop ()
        || workareas == NULL
        || type_returned != XA_CARDINAL
        || ((*nworkareas) * 4 != length_returned)
        || format_returned != 32) {
        g_warning("Can not determine workarea, guessing at layout");
        nemo_canvas_container_set_margins (canvas_container, 0, 0, 0, 0);
	} else {
		screen = gdk_window_get_screen (window);

		canvas_container_set_workarea (canvas_container, screen, workareas, length_returned);
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
	NemoDesktopCanvasView *canvas_view;

	canvas_view = NEMO_DESKTOP_CANVAS_VIEW (data);
  
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

static void
real_begin_loading (NemoView *object)
{
	NemoCanvasContainer *canvas_container;
	NemoDesktopCanvasView *view;

	view = NEMO_DESKTOP_CANVAS_VIEW (object);

	canvas_container = get_canvas_container (view);
	if (view->details->background == NULL) {
		view->details->background = nemo_desktop_background_new (canvas_container);
	}

	NEMO_VIEW_CLASS (nemo_desktop_canvas_view_parent_class)->begin_loading (object);
}

static const char *
real_get_id (NemoView *view)
{
	return NEMO_DESKTOP_CANVAS_VIEW_ID;
}

static void
nemo_desktop_canvas_view_dispose (GObject *object)
{
	NemoDesktopCanvasView *canvas_view;
	GtkUIManager *ui_manager;

	canvas_view = NEMO_DESKTOP_CANVAS_VIEW (object);

	/* Remove desktop rescan timeout. */
	if (canvas_view->details->reload_desktop_timeout != 0) {
		g_source_remove (canvas_view->details->reload_desktop_timeout);
		canvas_view->details->reload_desktop_timeout = 0;
	}

	ui_manager = nemo_view_get_ui_manager (NEMO_VIEW (canvas_view));
	if (ui_manager != NULL) {
		nemo_ui_unmerge_ui (ui_manager,
					&canvas_view->details->desktop_merge_id,
					&canvas_view->details->desktop_action_group);
	}

	g_signal_handlers_disconnect_by_func (nemo_canvas_view_preferences,
					      default_zoom_level_changed,
					      canvas_view);
	g_signal_handlers_disconnect_by_func (nemo_preferences,
					      font_changed_callback,
					      canvas_view);
	g_signal_handlers_disconnect_by_func (gnome_lockdown_preferences,
					      nemo_view_update_menus,
					      canvas_view);

	if (canvas_view->details->background != NULL) {
		g_object_unref (canvas_view->details->background);
		canvas_view->details->background = NULL;
	}

	G_OBJECT_CLASS (nemo_desktop_canvas_view_parent_class)->dispose (object);
}

static void
nemo_desktop_canvas_view_class_init (NemoDesktopCanvasViewClass *class)
{
	NemoViewClass *vclass;

	vclass = NEMO_VIEW_CLASS (class);

	G_OBJECT_CLASS (class)->dispose = nemo_desktop_canvas_view_dispose;

	vclass->begin_loading = real_begin_loading;
	vclass->merge_menus = real_merge_menus;
	vclass->update_menus = real_update_menus;
	vclass->get_view_id = real_get_id;

	g_type_class_add_private (class, sizeof (NemoDesktopCanvasViewDetails));
}

static void
unrealized_callback (GtkWidget *widget, NemoDesktopCanvasView *desktop_canvas_view)
{
	g_return_if_fail (desktop_canvas_view->details->root_window != NULL);

	/* Remove the property filter */
	gdk_window_remove_filter (desktop_canvas_view->details->root_window,
				  desktop_canvas_view_property_filter,
				  desktop_canvas_view);
	desktop_canvas_view->details->root_window = NULL;
}

static void
realized_callback (GtkWidget *widget, NemoDesktopCanvasView *desktop_canvas_view)
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

static NemoZoomLevel
get_default_zoom_level (void)
{
	NemoZoomLevel default_zoom_level;

	default_zoom_level = g_settings_get_enum (nemo_canvas_view_preferences,
						  NEMO_PREFERENCES_CANVAS_VIEW_DEFAULT_ZOOM_LEVEL);

	return CLAMP (default_zoom_level, NEMO_ZOOM_LEVEL_SMALLEST, NEMO_ZOOM_LEVEL_LARGEST);
}

static void
default_zoom_level_changed (gpointer user_data)
{
	NemoZoomLevel new_level;
	NemoDesktopCanvasView *desktop_canvas_view;

	desktop_canvas_view = NEMO_DESKTOP_CANVAS_VIEW (user_data);
	new_level = get_default_zoom_level ();

	nemo_canvas_container_set_zoom_level (get_canvas_container (desktop_canvas_view),
						new_level);
}

static gboolean
do_desktop_rescan (gpointer data)
{
	NemoDesktopCanvasView *desktop_canvas_view;
	struct stat buf;

	desktop_canvas_view = NEMO_DESKTOP_CANVAS_VIEW (data);
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

	nemo_directory_force_reload
		(nemo_view_get_model (NEMO_VIEW (desktop_canvas_view)));

	return TRUE;
}

static void
done_loading (NemoDirectory *model,
	      NemoDesktopCanvasView *desktop_canvas_view)
{
	struct stat buf;

	desktop_canvas_view->details->pending_rescan = FALSE;
	if (stat (desktop_directory, &buf) == -1) {
		return;
	}

	desktop_dir_modify_time = buf.st_ctime;
}

/* This function is used because the NemoDirectory model does not
 * exist always in the desktop_canvas_view, so we wait until it has been
 * instantiated.
 */
static void
delayed_init (NemoDesktopCanvasView *desktop_canvas_view)
{
	/* Keep track of the load time. */
	g_signal_connect_object (nemo_view_get_model (NEMO_VIEW (desktop_canvas_view)),
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
 	g_return_if_fail (NEMO_IS_DESKTOP_CANVAS_VIEW (callback_data));
	
	nemo_desktop_canvas_view_update_canvas_container_fonts (NEMO_DESKTOP_CANVAS_VIEW (callback_data));
}

static void
nemo_desktop_canvas_view_update_canvas_container_fonts (NemoDesktopCanvasView *canvas_view)
{
	NemoCanvasContainer *canvas_container;
	char *font;

	canvas_container = get_canvas_container (canvas_view);
	g_assert (canvas_container != NULL);

	font = g_settings_get_string (nemo_desktop_preferences,
				      NEMO_PREFERENCES_DESKTOP_FONT);

	nemo_canvas_container_set_font (canvas_container, font);

	g_free (font);
}

static void
nemo_desktop_canvas_view_init (NemoDesktopCanvasView *desktop_canvas_view)
{
	NemoCanvasContainer *canvas_container;
	GtkAllocation allocation;
	GtkAdjustment *hadj, *vadj;

	desktop_canvas_view->details = G_TYPE_INSTANCE_GET_PRIVATE (desktop_canvas_view,
								  NEMO_TYPE_DESKTOP_CANVAS_VIEW,
								  NemoDesktopCanvasViewDetails);

	if (desktop_directory == NULL) {
		desktop_directory = nemo_get_desktop_directory ();
	}

	canvas_container = get_canvas_container (desktop_canvas_view);
	nemo_canvas_container_set_use_drop_shadows (canvas_container, TRUE);
	nemo_canvas_view_container_set_sort_desktop (NEMO_CANVAS_VIEW_CONTAINER (canvas_container), TRUE);

	/* Do a reload on the desktop if we don't have FAM, a smarter
	 * way to keep track of the items on the desktop.
	 */
	if (!nemo_monitor_active ()) {
		desktop_canvas_view->details->delayed_init_signal = g_signal_connect_object
			(desktop_canvas_view, "begin-loading",
			 G_CALLBACK (delayed_init), desktop_canvas_view, 0);
	}
	
	nemo_canvas_container_set_is_fixed_size (canvas_container, TRUE);
	nemo_canvas_container_set_is_desktop (canvas_container, TRUE);
	nemo_canvas_container_set_store_layout_timestamps (canvas_container, TRUE);

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

	nemo_view_ignore_hidden_file_preferences
		(NEMO_VIEW (desktop_canvas_view));

	nemo_view_set_show_foreign (NEMO_VIEW (desktop_canvas_view),
					FALSE);
	
	/* Set our default layout mode */
	nemo_canvas_container_set_layout_mode (canvas_container,
						 gtk_widget_get_direction (GTK_WIDGET(canvas_container)) == GTK_TEXT_DIR_RTL ?
						 NEMO_CANVAS_LAYOUT_T_B_R_L :
						 NEMO_CANVAS_LAYOUT_T_B_L_R);

	g_signal_connect_object (canvas_container, "realize",
				 G_CALLBACK (realized_callback), desktop_canvas_view, 0);
	g_signal_connect_object (desktop_canvas_view, "unrealize",
				 G_CALLBACK (unrealized_callback), desktop_canvas_view, 0);

	g_signal_connect_swapped (nemo_canvas_view_preferences,
				  "changed::" NEMO_PREFERENCES_CANVAS_VIEW_DEFAULT_ZOOM_LEVEL,
				  G_CALLBACK (default_zoom_level_changed),
				  desktop_canvas_view);

	g_signal_connect_swapped (nemo_desktop_preferences,
				  "changed::" NEMO_PREFERENCES_DESKTOP_FONT,
				  G_CALLBACK (font_changed_callback),
				  desktop_canvas_view);

	default_zoom_level_changed (desktop_canvas_view);
	nemo_desktop_canvas_view_update_canvas_container_fonts (desktop_canvas_view);

	g_signal_connect_swapped (gnome_lockdown_preferences,
				  "changed::" NEMO_PREFERENCES_LOCKDOWN_COMMAND_LINE,
				  G_CALLBACK (nemo_view_update_menus),
				  desktop_canvas_view);
}

static void
action_empty_trash_conditional_callback (GtkAction *action,
					 gpointer data)
{
        g_assert (NEMO_IS_VIEW (data));

	nemo_file_operations_empty_trash (GTK_WIDGET (data));
}

static void
action_stretch_callback (GtkAction *action,
			 gpointer callback_data)
{
	nemo_canvas_container_show_stretch_handles
		(get_canvas_container (callback_data));
}

static void
action_unstretch_callback (GtkAction *action,
			   gpointer callback_data)
{
	nemo_canvas_container_unstretch (get_canvas_container (callback_data));
}

static void
action_clean_up_callback (GtkAction *action,
			  gpointer callback_data)
{
	nemo_canvas_view_clean_up_by_name (NEMO_CANVAS_VIEW (callback_data));
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
	NemoDesktopCanvasView *desktop_view;
        NemoCanvasContainer *canvas_container;
	gboolean include_empty_trash;
	char *label;
	GtkAction *action;
        int selection_count;

	g_assert (NEMO_IS_DESKTOP_CANVAS_VIEW (view));

	NEMO_VIEW_CLASS (nemo_desktop_canvas_view_parent_class)->update_menus (view);

	desktop_view = NEMO_DESKTOP_CANVAS_VIEW (view);

	/* Empty Trash */
	include_empty_trash = trash_link_is_selection (view);
	action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
					      NEMO_ACTION_EMPTY_TRASH_CONDITIONAL);
	gtk_action_set_visible (action,
				include_empty_trash);
	if (include_empty_trash) {
		label = g_strdup (_("E_mpty Trash"));
		g_object_set (action , "label", label, NULL);
		gtk_action_set_sensitive (action,
					  !nemo_trash_monitor_is_empty ());
		g_free (label);
	}

	/* Stretch */
        selection_count = nemo_view_get_selection_count (view);
        canvas_container = get_canvas_container (desktop_view);

	action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
					      NEMO_ACTION_STRETCH);
	gtk_action_set_sensitive (action,
				  selection_count == 1
				  && canvas_container != NULL
				  && !nemo_canvas_container_has_stretch_handles (canvas_container));
	gtk_action_set_visible (action, TRUE);

	/* Unstretch */
	action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
					      NEMO_ACTION_UNSTRETCH);
	g_object_set (action, "label",
		      (selection_count > 1)
		      ? _("Restore Icons' Original Si_zes")
		      : _("Restore Icon's Original Si_ze"),
		      NULL);
	gtk_action_set_sensitive (action,
				  canvas_container != NULL
				  && nemo_canvas_container_is_stretched (canvas_container));
	gtk_action_set_visible (action, TRUE);
}

static const GtkActionEntry desktop_view_entries[] = {

	/* name, stock id */
	{ "Empty Trash Conditional", NULL,
	  /* label, accelerator */
	  N_("Empty Trash"), NULL,
	  /* tooltip */
	  N_("Delete all items in the Trash"),
	  G_CALLBACK (action_empty_trash_conditional_callback) },
	/* name, stock id */
	{ NEMO_ACTION_CLEAN_UP, NULL,
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
real_merge_menus (NemoView *view)
{
	NemoDesktopCanvasView *desktop_view;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;

	NEMO_VIEW_CLASS (nemo_desktop_canvas_view_parent_class)->merge_menus (view);

	desktop_view = NEMO_DESKTOP_CANVAS_VIEW (view);

	ui_manager = nemo_view_get_ui_manager (view);

	action_group = gtk_action_group_new ("DesktopViewActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	desktop_view->details->desktop_action_group = action_group;
	gtk_action_group_add_actions (action_group, 
				      desktop_view_entries, G_N_ELEMENTS (desktop_view_entries),
				      view);

	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui manager */

	desktop_view->details->desktop_merge_id =
		gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/nemo/nemo-desktop-canvas-view-ui.xml", NULL);

	gtk_ui_manager_add_ui (ui_manager,
			       desktop_view->details->desktop_merge_id,
			       POPUP_PATH_CANVAS_APPEARANCE,
			       NEMO_ACTION_STRETCH,
			       NEMO_ACTION_STRETCH,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);
	gtk_ui_manager_add_ui (ui_manager,
			       desktop_view->details->desktop_merge_id,
			       POPUP_PATH_CANVAS_APPEARANCE,
			       NEMO_ACTION_UNSTRETCH,
			       NEMO_ACTION_UNSTRETCH,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);
}

static NemoView *
nemo_desktop_canvas_view_create (NemoWindowSlot *slot)
{
	NemoCanvasView *view;

	view = g_object_new (NEMO_TYPE_DESKTOP_CANVAS_VIEW,
			     "window-slot", slot,
			     "supports-zooming", FALSE,
			     "supports-auto-layout", FALSE,
			     "supports-manual-layout", TRUE,
			     "supports-scaling", TRUE,
			     "supports-keep-aligned", TRUE,
			     "supports-labels-beside-icons", FALSE,
			     NULL);
	return NEMO_VIEW (view);
}

static gboolean
nemo_desktop_canvas_view_supports_uri (const char *uri,
				   GFileType file_type,
				   const char *mime_type)
{
	if (g_str_has_prefix (uri, EEL_DESKTOP_URI)) {
		return TRUE;
	}

	return FALSE;
}

static NemoViewInfo nemo_desktop_canvas_view = {
	NEMO_DESKTOP_CANVAS_VIEW_ID,
	"Desktop View",
	"_Desktop",
	N_("The desktop view encountered an error."),
	N_("The desktop view encountered an error while starting up."),
	"Display this location with the desktop view.",
	nemo_desktop_canvas_view_create,
	nemo_desktop_canvas_view_supports_uri
};

void
nemo_desktop_canvas_view_register (void)
{
	nemo_desktop_canvas_view.error_label = _(nemo_desktop_canvas_view.error_label);
	nemo_desktop_canvas_view.startup_error_label = _(nemo_desktop_canvas_view.startup_error_label);
	
	nemo_view_factory_register (&nemo_desktop_canvas_view);
}

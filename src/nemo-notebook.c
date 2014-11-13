/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright © 2002 Christophe Fergeau
 *  Copyright © 2003, 2004 Marco Pesenti Gritti
 *  Copyright © 2003, 2004, 2005 Christian Persch
 *    (ephy-notebook.c)
 *
 *  Copyright © 2008 Free Software Foundation, Inc.
 *    (nemo-notebook.c)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "nemo-notebook.h"

#include "nemo-window.h"
#include "nemo-window-manage-views.h"
#include "nemo-window-private.h"
#include "nemo-window-slot.h"
#include "nemo-window-slot-dnd.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#define AFTER_ALL_TABS -1
#define NOT_IN_APP_WINDOWS -2

static int  nemo_notebook_insert_page	 (GtkNotebook *notebook,
					  GtkWidget *child,
					  GtkWidget *tab_label,
					  GtkWidget *menu_label,
					  int position);
static void nemo_notebook_remove	 (GtkContainer *container,
					  GtkWidget *tab_widget);

static const GtkTargetEntry url_drag_types[] =
{
	{ NEMO_ICON_DND_GNOME_ICON_LIST_TYPE, 0, NEMO_ICON_DND_GNOME_ICON_LIST },
	{ NEMO_ICON_DND_URI_LIST_TYPE, 0, NEMO_ICON_DND_URI_LIST },
};

enum
{
	TAB_CLOSE_REQUEST,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (NemoNotebook, nemo_notebook, GTK_TYPE_NOTEBOOK);

static void
nemo_notebook_class_init (NemoNotebookClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
	GtkNotebookClass *notebook_class = GTK_NOTEBOOK_CLASS (klass);

	container_class->remove = nemo_notebook_remove;

	notebook_class->insert_page = nemo_notebook_insert_page;

	signals[TAB_CLOSE_REQUEST] =
		g_signal_new ("tab-close-request",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NemoNotebookClass, tab_close_request),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      NEMO_TYPE_WINDOW_SLOT);
}


/* FIXME remove when gtknotebook's func for this becomes public, bug #.... */
static NemoNotebook *
find_notebook_at_pointer (gint abs_x, gint abs_y)
{
	GdkDeviceManager *manager;
	GdkDevice *pointer;
	GdkWindow *win_at_pointer, *toplevel_win;
	gpointer toplevel = NULL;
	gint x, y;

	/* FIXME multi-head */
	manager = gdk_display_get_device_manager (gdk_display_get_default ());
	pointer = gdk_device_manager_get_client_pointer (manager);
	win_at_pointer = gdk_device_get_window_at_position (pointer, &x, &y);

	if (win_at_pointer == NULL)
	{
		/* We are outside all windows containing a notebook */
		return NULL;
	}

	toplevel_win = gdk_window_get_toplevel (win_at_pointer);

	/* get the GtkWidget which owns the toplevel GdkWindow */
	gdk_window_get_user_data (toplevel_win, &toplevel);

	/* toplevel should be an NemoWindow */
	if (toplevel != NULL && NEMO_IS_WINDOW (toplevel))
	{
		return NEMO_NOTEBOOK (NEMO_WINDOW (toplevel)->details->active_pane->notebook);
	}

	return NULL;
}

static gboolean
is_in_notebook_window (NemoNotebook *notebook,
		       gint abs_x, gint abs_y)
{
	NemoNotebook *nb_at_pointer;

	nb_at_pointer = find_notebook_at_pointer (abs_x, abs_y);

	return nb_at_pointer == notebook;
}

gint
nemo_notebook_find_tab_num_at_pos (NemoNotebook *notebook,
				   gint 	 abs_x,
				   gint 	 abs_y)
{
	GtkPositionType tab_pos;
	int page_num = 0;
	GtkNotebook *nb = GTK_NOTEBOOK (notebook);
	GtkWidget *page;
	GtkAllocation allocation;

	tab_pos = gtk_notebook_get_tab_pos (GTK_NOTEBOOK (notebook));

	if (gtk_notebook_get_n_pages (nb) == 0)
	{
		return AFTER_ALL_TABS;
	}

	/* For some reason unfullscreen + quick click can
	   cause a wrong click event to be reported to the tab */
	if (!is_in_notebook_window(notebook, abs_x, abs_y))
	{
		return NOT_IN_APP_WINDOWS;
	}

	while ((page = gtk_notebook_get_nth_page (nb, page_num)))
	{
		GtkWidget *tab;
		gint max_x, max_y;
		gint x_root, y_root;

		tab = gtk_notebook_get_tab_label (nb, page);
		g_return_val_if_fail (tab != NULL, -1);

		if (!gtk_widget_get_mapped (GTK_WIDGET (tab)))
		{
			page_num++;
			continue;
		}

		gdk_window_get_origin (gtk_widget_get_window (tab),
				       &x_root, &y_root);
		gtk_widget_get_allocation (tab, &allocation);

		max_x = x_root + allocation.x + allocation.width;
		max_y = y_root + allocation.y + allocation.height;

		if (((tab_pos == GTK_POS_TOP)
		     || (tab_pos == GTK_POS_BOTTOM))
		    &&(abs_x<=max_x))
		{
			return page_num;
		}
		else if (((tab_pos == GTK_POS_LEFT)
			  || (tab_pos == GTK_POS_RIGHT))
			 && (abs_y<=max_y))
		{
			return page_num;
		}

		page_num++;
	}
	return AFTER_ALL_TABS;
}

static gboolean ctrl_key_is_down = FALSE;

/* user_data = if this is a callback for a keydown (else a keyup) */
static gboolean
control_key_checker_cb(NemoNotebook *notebook, GdkEventKey *event, gpointer user_data)
{
	if (event->keyval == GDK_KEY_Control_L || event->keyval == GDK_KEY_Control_R)
		ctrl_key_is_down = GPOINTER_TO_INT (user_data);

	return FALSE;
}

static gboolean
notebook_tab_shortcut_cb(NemoNotebook *notebook, GtkDirectionType direction, gpointer user_data)
{
	/* the "focus" event is fired if tab/shift+tab is pressed, so we only
	 * need to check if ctrl is pressed down here.
	 */
	if (ctrl_key_is_down)
	{
		/* change the selected tab. work out if we need to do any wrap-around */
		int last    = gtk_notebook_get_n_pages (GTK_NOTEBOOK(user_data)) - 1;
		int current = gtk_notebook_get_current_page (GTK_NOTEBOOK(user_data));
		int next;

		if (direction)
			next = (current == 0 ? last : current - 1);
		else
			next = (current == last ? 0 : current + 1);

		gtk_notebook_set_current_page (GTK_NOTEBOOK(user_data), next);
	}

	return TRUE;
}

static void
nemo_notebook_init (NemoNotebook *notebook)
{
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);

	/* Make it so that pressing ctrl+tab/ctrl+shift+tab switches the currently
	 * focused tab.
	 */
	/* Gtk internally gobbles up tab/shift+tab keyboard events for widget
	 * focus switching but we can override this with a little trickery.
	 */
	g_signal_connect (notebook, "focus",
			  G_CALLBACK(notebook_tab_shortcut_cb), (gpointer)notebook);
	g_signal_connect (notebook, "key-press-event",
			  G_CALLBACK(control_key_checker_cb), GINT_TO_POINTER (TRUE));
	g_signal_connect (notebook, "key-release-event",
			  G_CALLBACK(control_key_checker_cb), GINT_TO_POINTER (FALSE));

	/* Set up drag-and-drop target */
	/* TODO this would be used for opening a new tab.
	 * It will only work properly as soon as GtkNotebook
	 * supports to find out whether a particular point
	 * is on a tab button or not.
	 */
#if 0
	gtk_drag_dest_set (GTK_WIDGET (notebook), 0,
			   url_drag_types, G_N_ELEMENTS (url_drag_types),
			   GDK_ACTION_LINK);
	gtk_drag_dest_set_track_motion (GTK_WIDGET (notebook), TRUE);
#endif
}

void
nemo_notebook_sync_loading (NemoNotebook *notebook,
				NemoWindowSlot *slot)
{
	GtkWidget *tab_label, *spinner, *icon;
	gboolean active;

	g_return_if_fail (NEMO_IS_NOTEBOOK (notebook));
	g_return_if_fail (NEMO_IS_WINDOW_SLOT (slot));

	tab_label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebook),
						GTK_WIDGET (slot));
	g_return_if_fail (GTK_IS_WIDGET (tab_label));

	spinner = GTK_WIDGET (g_object_get_data (G_OBJECT (tab_label), "spinner"));
	icon = GTK_WIDGET (g_object_get_data (G_OBJECT (tab_label), "icon"));
	g_return_if_fail (spinner != NULL && icon != NULL);

	active = FALSE;
	g_object_get (spinner, "active", &active, NULL);
	if (active == slot->allow_stop)	{
		return;
	}

	if (slot->allow_stop) {
		gtk_widget_hide (icon);
		gtk_widget_show (spinner);
		gtk_spinner_start (GTK_SPINNER (spinner));
	} else {
		gtk_spinner_stop (GTK_SPINNER (spinner));
		gtk_widget_hide (spinner);
		gtk_widget_show (icon);
	}
}

void
nemo_notebook_sync_tab_label (NemoNotebook *notebook,
				  NemoWindowSlot *slot)
{
	GtkWidget *hbox, *label;
	char *location_name;

	g_return_if_fail (NEMO_IS_NOTEBOOK (notebook));
	g_return_if_fail (NEMO_IS_WINDOW_SLOT (slot));

	hbox = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebook), GTK_WIDGET (slot));
	g_return_if_fail (GTK_IS_WIDGET (hbox));

	label = GTK_WIDGET (g_object_get_data (G_OBJECT (hbox), "label"));
	g_return_if_fail (GTK_IS_WIDGET (label));

	gtk_label_set_text (GTK_LABEL (label), slot->title);

	if (slot->location != NULL) {
		/* Set the tooltip on the label's parent (the tab label hbox),
		 * so it covers all of the tab label.
		 */
		location_name = g_file_get_parse_name (slot->location);
		gtk_widget_set_tooltip_text (gtk_widget_get_parent (label), location_name);
		g_free (location_name);
	} else {
		gtk_widget_set_tooltip_text (gtk_widget_get_parent (label), NULL);
	}
}

static void
close_button_clicked_cb (GtkWidget *widget,
			 NemoWindowSlot *slot)
{
	GtkWidget *notebook;

	notebook = gtk_widget_get_ancestor (GTK_WIDGET (slot), NEMO_TYPE_NOTEBOOK);
	if (notebook != NULL) {
		g_signal_emit (notebook, signals[TAB_CLOSE_REQUEST], 0, slot);
	}
}

static GtkWidget *
build_tab_label (NemoNotebook *nb, NemoWindowSlot *slot)
{
	GtkWidget *hbox, *label, *close_button, *image, *spinner, *icon;

	/* set hbox spacing and label padding (see below) so that there's an
	 * equal amount of space around the label */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_show (hbox);

	/* setup load feedback */
	spinner = gtk_spinner_new ();
	gtk_box_pack_start (GTK_BOX (hbox), spinner, FALSE, FALSE, 0);

	/* setup site icon, empty by default */
	icon = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
	/* don't show the icon */

	/* setup label */
	label = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_label_set_single_line_mode (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 0, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	/* setup close button */
	close_button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (close_button),
			       GTK_RELIEF_NONE);
	/* don't allow focus on the close button */
	gtk_button_set_focus_on_click (GTK_BUTTON (close_button), FALSE);

	gtk_widget_set_name (close_button, "nemo-tab-close-button");

	image = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU);
	gtk_widget_set_tooltip_text (close_button, _("Close tab"));
	g_signal_connect_object (close_button, "clicked",
				 G_CALLBACK (close_button_clicked_cb), slot, 0);

	gtk_container_add (GTK_CONTAINER (close_button), image);
	gtk_widget_show (image);

	gtk_box_pack_start (GTK_BOX (hbox), close_button, FALSE, FALSE, 0);
	gtk_widget_show (close_button);

	nemo_drag_slot_proxy_init (hbox, NULL, slot);

	g_object_set_data (G_OBJECT (hbox), "label", label);
	g_object_set_data (G_OBJECT (hbox), "spinner", spinner);
	g_object_set_data (G_OBJECT (hbox), "icon", icon);
	g_object_set_data (G_OBJECT (hbox), "close-button", close_button);

	return hbox;
}

static int
nemo_notebook_insert_page (GtkNotebook *gnotebook,
			       GtkWidget *tab_widget,
			       GtkWidget *tab_label,
			       GtkWidget *menu_label,
			       int position)
{
	g_assert (GTK_IS_WIDGET (tab_widget));

	position = GTK_NOTEBOOK_CLASS (nemo_notebook_parent_class)->insert_page (gnotebook,
										     tab_widget,
										     tab_label,
										     menu_label,
										     position);

	gtk_notebook_set_show_tabs (gnotebook,
				    gtk_notebook_get_n_pages (gnotebook) > 1);
	gtk_notebook_set_tab_reorderable (gnotebook, tab_widget, TRUE);
	gtk_notebook_set_tab_detachable (gnotebook, tab_widget, TRUE);

	return position;
}

int
nemo_notebook_add_tab (NemoNotebook *notebook,
			   NemoWindowSlot *slot,
			   int position,
			   gboolean jump_to)
{
	GtkNotebook *gnotebook = GTK_NOTEBOOK (notebook);
	GtkWidget *tab_label;

	g_return_val_if_fail (NEMO_IS_NOTEBOOK (notebook), -1);
	g_return_val_if_fail (NEMO_IS_WINDOW_SLOT (slot), -1);

	tab_label = build_tab_label (notebook, slot);

	position = gtk_notebook_insert_page (GTK_NOTEBOOK (notebook),
					     GTK_WIDGET (slot),
					     tab_label,
					     position);

	gtk_container_child_set (GTK_CONTAINER (notebook),
				 GTK_WIDGET (slot),
				 "tab-expand", TRUE,
				 NULL);

	nemo_notebook_sync_tab_label (notebook, slot);
	nemo_notebook_sync_loading (notebook, slot);

	if (jump_to) {
		gtk_notebook_set_current_page (gnotebook, position);
	}

	return position;
}

static void
nemo_notebook_remove (GtkContainer *container,
			  GtkWidget *tab_widget)
{
	GtkNotebook *gnotebook = GTK_NOTEBOOK (container);
	GTK_CONTAINER_CLASS (nemo_notebook_parent_class)->remove (container, tab_widget);

	gtk_notebook_set_show_tabs (gnotebook,
				    gtk_notebook_get_n_pages (gnotebook) > 1);

}

void
nemo_notebook_reorder_child_relative (NemoNotebook *notebook,
				      int    	    page_num,
				      int 	    offset)
{
	GtkNotebook *gnotebook;
	GtkWidget *page;

	g_return_if_fail (NEMO_IS_NOTEBOOK (notebook));
	g_return_if_fail (page_num != -1);
	if (!nemo_notebook_can_reorder_child_relative (
		notebook, page_num, offset)) {
		return;
	}

	gnotebook = GTK_NOTEBOOK (notebook);
	page = gtk_notebook_get_nth_page (gnotebook, page_num);
	g_return_if_fail (page != NULL);
	gtk_notebook_reorder_child (gnotebook, page, page_num + offset);
}

static gboolean
nemo_notebook_is_valid_relative_position (NemoNotebook *notebook,
					  int		page_num,
					  int 		offset)
{
	GtkNotebook *gnotebook;
	int n_pages;

	gnotebook = GTK_NOTEBOOK (notebook);
	n_pages = gtk_notebook_get_n_pages (gnotebook) - 1;
	if (page_num < 0 || (offset < 0 && page_num < -offset) ||
	    (offset > 0 && page_num > n_pages - offset)) {
		return FALSE;
	}

	return TRUE;
}

gboolean
nemo_notebook_can_reorder_child_relative (NemoNotebook *notebook,
					  int		page_num,
					  int 		offset)
{
	g_return_val_if_fail (NEMO_IS_NOTEBOOK (notebook), FALSE);

	return nemo_notebook_is_valid_relative_position (
		notebook, page_num, offset);
}

gboolean
nemo_notebook_can_set_current_page_relative (NemoNotebook *notebook,
						 int offset)
{
	int page_num;

	g_return_val_if_fail (NEMO_IS_NOTEBOOK (notebook), FALSE);

	page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	return nemo_notebook_is_valid_relative_position (
		notebook, page_num, offset);
}

void
nemo_notebook_next_page (NemoNotebook *notebook)
{
	gint current_page, n_pages;

	g_return_if_fail (NEMO_IS_NOTEBOOK (notebook));

	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
	n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));

	if (current_page < n_pages - 1)
		gtk_notebook_next_page (GTK_NOTEBOOK (notebook));
	else {
		gboolean  wrap_around;

		g_object_get (gtk_widget_get_settings (GTK_WIDGET (notebook)),
			      "gtk-keynav-wrap-around", &wrap_around,
			      NULL);

		if (wrap_around)
			gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);
	}
}

void
nemo_notebook_prev_page (NemoNotebook *notebook)
{
	gint current_page;

	g_return_if_fail (NEMO_IS_NOTEBOOK (notebook));

	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));

	if (current_page > 0)
		gtk_notebook_prev_page (GTK_NOTEBOOK (notebook));
	else {
		gboolean  wrap_around;

		g_object_get (gtk_widget_get_settings (GTK_WIDGET (notebook)),
			      "gtk-keynav-wrap-around", &wrap_around,
			      NULL);

		if (wrap_around)
			gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), -1);
	}
}

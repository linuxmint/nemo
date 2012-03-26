/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Paolo Borelli <pborelli@katamail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Paolo Borelli <pborelli@katamail.com>
 *
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "nautilus-trash-bar.h"

#include "nautilus-view.h"
#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-trash-monitor.h>

#define NAUTILUS_TRASH_BAR_GET_PRIVATE(o)\
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), NAUTILUS_TYPE_TRASH_BAR, NautilusTrashBarPrivate))

enum {
	PROP_VIEW = 1,
	NUM_PROPERTIES
};

enum {
	TRASH_BAR_RESPONSE_EMPTY = 1,
	TRASH_BAR_RESPONSE_RESTORE
};

struct NautilusTrashBarPrivate
{
	NautilusView *view;
	gulong selection_handler_id;
};

G_DEFINE_TYPE (NautilusTrashBar, nautilus_trash_bar, GTK_TYPE_INFO_BAR);

static void
selection_changed_cb (NautilusView *view,
		      NautilusTrashBar *bar)
{
	int count;

	count = nautilus_view_get_selection_count (view);

	gtk_info_bar_set_response_sensitive (GTK_INFO_BAR (bar),
					     TRASH_BAR_RESPONSE_RESTORE,
					     (count > 0));
}

static void
connect_view_and_update_button (NautilusTrashBar *bar)
{
	bar->priv->selection_handler_id =
		g_signal_connect (bar->priv->view, "selection-changed",
				  G_CALLBACK (selection_changed_cb), bar);

	selection_changed_cb (bar->priv->view, bar);
}

static void
nautilus_trash_bar_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	NautilusTrashBar *bar;

	bar = NAUTILUS_TRASH_BAR (object);

	switch (prop_id) {
	case PROP_VIEW:
		bar->priv->view = g_value_get_object (value);
		connect_view_and_update_button (bar);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nautilus_trash_bar_finalize (GObject *obj)
{
	NautilusTrashBar *bar;

	bar = NAUTILUS_TRASH_BAR (obj);

	if (bar->priv->selection_handler_id) {
		g_signal_handler_disconnect (bar->priv->view, bar->priv->selection_handler_id);
	}

	G_OBJECT_CLASS (nautilus_trash_bar_parent_class)->finalize (obj);
}

static void
nautilus_trash_bar_trash_state_changed (NautilusTrashMonitor *trash_monitor,
					gboolean              state,
					gpointer              data)
{
	NautilusTrashBar *bar;

	bar = NAUTILUS_TRASH_BAR (data);

	gtk_info_bar_set_response_sensitive (GTK_INFO_BAR (bar),
					     TRASH_BAR_RESPONSE_EMPTY,
					     !nautilus_trash_monitor_is_empty ());
}

static void
nautilus_trash_bar_class_init (NautilusTrashBarClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = nautilus_trash_bar_set_property;
	object_class->finalize = nautilus_trash_bar_finalize;

	g_object_class_install_property (object_class,
					 PROP_VIEW,
					 g_param_spec_object ("view",
							      "view",
							      "the NautilusView",
							      NAUTILUS_TYPE_VIEW,
							      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));

	g_type_class_add_private (klass, sizeof (NautilusTrashBarPrivate));
}

static void
trash_bar_response_cb (GtkInfoBar *infobar,
		       gint response_id,
		       gpointer user_data)
{
	NautilusTrashBar *bar;
	GtkWidget *window;
	GList *files;

	bar = NAUTILUS_TRASH_BAR (infobar);
	window = gtk_widget_get_toplevel (GTK_WIDGET (bar));

	switch (response_id) {
	case TRASH_BAR_RESPONSE_EMPTY:
		nautilus_file_operations_empty_trash (window);
		break;
	case TRASH_BAR_RESPONSE_RESTORE:
		files = nautilus_view_get_selection (bar->priv->view);
		nautilus_restore_files_from_trash (files, GTK_WINDOW (window));
		nautilus_file_list_free (files);
		break;
	default:
		break;
	}
}

static void
nautilus_trash_bar_init (NautilusTrashBar *bar)
{
	GtkWidget *content_area, *action_area, *w;
	GtkWidget *label;

	bar->priv = NAUTILUS_TRASH_BAR_GET_PRIVATE (bar);
	content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (bar));
	action_area = gtk_info_bar_get_action_area (GTK_INFO_BAR (bar));

	gtk_orientable_set_orientation (GTK_ORIENTABLE (action_area),
					GTK_ORIENTATION_HORIZONTAL);

	label = gtk_label_new (_("Trash"));
	gtk_style_context_add_class (gtk_widget_get_style_context (label),
				     "nautilus-cluebar-label");
	gtk_widget_show (label);
	gtk_container_add (GTK_CONTAINER (content_area), label);

	w = gtk_info_bar_add_button (GTK_INFO_BAR (bar),
				     _("Restore Selected Items"),
				     TRASH_BAR_RESPONSE_RESTORE);
	gtk_widget_set_tooltip_text (w,
				     _("Restore selected items to their original position"));

	w = gtk_info_bar_add_button (GTK_INFO_BAR (bar),
				     _("Empty _Trash"),
				     TRASH_BAR_RESPONSE_EMPTY);
	gtk_widget_set_tooltip_text (w,
				     _("Delete all items in the Trash"));

	g_signal_connect_object (nautilus_trash_monitor_get (),
				 "trash_state_changed",
				 G_CALLBACK (nautilus_trash_bar_trash_state_changed),
				 bar,
				 0);
	nautilus_trash_bar_trash_state_changed (nautilus_trash_monitor_get (),
						FALSE, bar);

	g_signal_connect (bar, "response",
			  G_CALLBACK (trash_bar_response_cb), bar);
}

GtkWidget *
nautilus_trash_bar_new (NautilusView *view)
{
	return g_object_new (NAUTILUS_TYPE_TRASH_BAR,
			     "view", view,
			     NULL);
}

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Novell, Inc.
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
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#include <config.h>
#include "nemo-search-bar.h"

#include <libnemo-private/nemo-icon-info.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

struct NemoSearchBarDetails {
	GtkWidget *entry;
	gboolean entry_borrowed;
};

enum {
       ACTIVATE,
       CANCEL,
       LAST_SIGNAL
}; 

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (NemoSearchBar, nemo_search_bar, GTK_TYPE_BOX);

static gboolean
nemo_search_bar_draw (GtkWidget *widget,
			  cairo_t *cr)
{
	GtkStyleContext *context;

	context = gtk_widget_get_style_context (widget);

	gtk_render_background (context, cr, 0, 0,
			       gtk_widget_get_allocated_width (widget),
			       gtk_widget_get_allocated_height (widget));

	gtk_render_frame (context, cr, 0, 0,
			  gtk_widget_get_allocated_width (widget),
			  gtk_widget_get_allocated_height (widget));

	GTK_WIDGET_CLASS (nemo_search_bar_parent_class)->draw (widget, cr);

	return FALSE;
}

static void
nemo_search_bar_class_init (NemoSearchBarClass *class)
{
	GtkBindingSet *binding_set;
	GtkWidgetClass *wclass;

	wclass = GTK_WIDGET_CLASS (class);
	wclass->draw = nemo_search_bar_draw;

	signals[ACTIVATE] =
		g_signal_new ("activate",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NemoSearchBarClass, activate),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[CANCEL] =
		g_signal_new ("cancel",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (NemoSearchBarClass, cancel),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	binding_set = gtk_binding_set_by_class (class);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "cancel", 0);

	g_type_class_add_private (class, sizeof (NemoSearchBarDetails));
}

static gboolean
entry_has_text (NemoSearchBar *bar)
{
       const char *text;

       text = gtk_entry_get_text (GTK_ENTRY (bar->details->entry));

       return text != NULL && text[0] != '\0';
}

static void
entry_icon_release_cb (GtkEntry *entry,
		       GtkEntryIconPosition position,
		       GdkEvent *event,
		       NemoSearchBar *bar)
{
	g_signal_emit_by_name (entry, "activate", 0);
}

static void
entry_activate_cb (GtkWidget *entry, NemoSearchBar *bar)
{
       if (entry_has_text (bar) && !bar->details->entry_borrowed) {
               g_signal_emit (bar, signals[ACTIVATE], 0);
       }
}

static void
nemo_search_bar_init (NemoSearchBar *bar)
{
	GtkWidget *label;
	GtkWidget *align;
	PangoAttrList *attrs;

	bar->details =
		G_TYPE_INSTANCE_GET_PRIVATE (bar, NEMO_TYPE_SEARCH_BAR,
					     NemoSearchBarDetails);

	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (bar)),
				     GTK_STYLE_CLASS_QUESTION);
	gtk_widget_set_redraw_on_allocate (GTK_WIDGET (bar), TRUE);

	attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
	label = gtk_label_new (_("Search:"));
	gtk_label_set_attributes (GTK_LABEL (label), attrs);
	pango_attr_list_unref (attrs);

	gtk_box_pack_start (GTK_BOX (bar), label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	g_object_set (label,
		      "margin-left", 6,
		      NULL);

	align = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (align),
				   6, 6, 0, 6);
	gtk_box_pack_start (GTK_BOX (bar), align, TRUE, TRUE, 0);
	gtk_widget_show (align);

	bar->details->entry = gtk_entry_new ();
	gtk_entry_set_icon_from_icon_name (GTK_ENTRY (bar->details->entry),
					   GTK_ENTRY_ICON_SECONDARY,
					   "edit-find");
	gtk_container_add (GTK_CONTAINER (align), bar->details->entry);

	g_signal_connect (bar->details->entry, "activate",
			  G_CALLBACK (entry_activate_cb), bar);
	g_signal_connect (bar->details->entry, "icon-release",
			  G_CALLBACK (entry_icon_release_cb), bar);

	gtk_widget_show (bar->details->entry);
}

GtkWidget *
nemo_search_bar_get_entry (NemoSearchBar *bar)
{
	return bar->details->entry;
}

GtkWidget *
nemo_search_bar_borrow_entry (NemoSearchBar *bar)
{
	GtkBindingSet *binding_set;
	
	bar->details->entry_borrowed = TRUE;

	binding_set = gtk_binding_set_by_class (G_OBJECT_GET_CLASS (bar));
	gtk_binding_entry_remove (binding_set, GDK_KEY_Escape, 0);
	return bar->details->entry;
}

void
nemo_search_bar_return_entry (NemoSearchBar *bar)
{
	GtkBindingSet *binding_set;
	
	bar->details->entry_borrowed = FALSE;
	
	binding_set = gtk_binding_set_by_class (G_OBJECT_GET_CLASS (bar));
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "cancel", 0);
}

GtkWidget *
nemo_search_bar_new (void)
{
	GtkWidget *bar;

	bar = g_object_new (NEMO_TYPE_SEARCH_BAR,
			    "orientation", GTK_ORIENTATION_HORIZONTAL,
			    "spacing", 6,
			    NULL);

	return bar;
}

NemoQuery *
nemo_search_bar_get_query (NemoSearchBar *bar)
{
	const char *query_text;
	NemoQuery *query;

	query_text = gtk_entry_get_text (GTK_ENTRY (bar->details->entry));

	/* Empty string is a NULL query */
	if (query_text && query_text[0] == '\0') {
		return NULL;
	}
	
	query = nemo_query_new ();
	nemo_query_set_text (query, query_text);

	return query;
}

void
nemo_search_bar_grab_focus (NemoSearchBar *bar)
{
	gtk_widget_grab_focus (bar->details->entry);
}

void
nemo_search_bar_clear (NemoSearchBar *bar)
{
	gtk_entry_set_text (GTK_ENTRY (bar->details->entry), "");
}

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* eel-accessibility.h - Utility functions for accessibility

   Copyright (C) 2002 Anders Carlsson, Sun Microsystems, Inc.

   The Eel Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Eel Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Eel Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors:
	Anders Carlsson <andersca@gnu.org>
	Michael Meeks   <michael@ximian.com>
*/
#include <config.h>
#include <gtk/gtk.h>
#include <atk/atkrelationset.h>
#include <eel/eel-accessibility.h>

void
eel_accessibility_set_up_label_widget_relation (GtkWidget *label, GtkWidget *widget)
{
	AtkObject *atk_widget, *atk_label;

	atk_label = gtk_widget_get_accessible (label);
	atk_widget = gtk_widget_get_accessible (widget);

	/* Create the label -> widget relation */
	atk_object_add_relationship (atk_label, ATK_RELATION_LABEL_FOR, atk_widget);

	/* Create the widget -> label relation */
	atk_object_add_relationship (atk_widget, ATK_RELATION_LABELLED_BY, atk_label);
}

static GQuark
get_quark_gobject (void)
{
	static GQuark quark_accessible_gobject = 0;

	if (!quark_accessible_gobject) {
		quark_accessible_gobject = g_quark_from_static_string
			("object-for-accessible");
	}

	return quark_accessible_gobject;
}

/**
 * eel_accessibility_get_gobject:
 * @object: an AtkObject
 * 
 * gets the GObject associated with the AtkObject, for which
 * @object provides accessibility support.
 * 
 * Return value: the accessible's associated GObject
 **/
gpointer
eel_accessibility_get_gobject (AtkObject *object)
{
	return g_object_get_qdata (G_OBJECT (object), get_quark_gobject ());
}

static GailTextUtil *
get_simple_text (gpointer object)
{
	GObject *gobject;
	EelAccessibleTextIface *aif;

	if (GTK_IS_ACCESSIBLE (object)) {
		gobject = G_OBJECT (gtk_accessible_get_widget (GTK_ACCESSIBLE (object)));
	} else {
		gobject = eel_accessibility_get_gobject (object);
	}

	if (!gobject) {
		return NULL;
	}

	aif = EEL_ACCESSIBLE_TEXT_GET_IFACE (gobject);
	if (!aif) {
		g_warning ("No accessible text inferface on '%s'",
			   g_type_name_from_instance ((gpointer) gobject));

	} else if (aif->get_text) {
		return aif->get_text (gobject);
	}

	return NULL;
}

char *
eel_accessibility_text_get_text (AtkText *text,
				 gint     start_pos,
				 gint     end_pos)
{
	GailTextUtil *util = get_simple_text (text);
	g_return_val_if_fail (util != NULL, NULL);

	return gail_text_util_get_substring (util, start_pos, end_pos);
}

gunichar 
eel_accessibility_text_get_character_at_offset (AtkText *text,
						gint     offset)
{
	char *txt, *index;
	gint sucks1 = 0, sucks2 = -1;
	gunichar c;
	GailTextUtil *util = get_simple_text (text);
	g_return_val_if_fail (util != NULL, 0);

	txt = gail_text_util_get_substring (util, sucks1, sucks2);

	index = g_utf8_offset_to_pointer (txt, offset);
	c = g_utf8_get_char (index);
	g_free (txt);

	return c;
}

char *
eel_accessibility_text_get_text_before_offset (AtkText	      *text,
					       gint            offset,
					       AtkTextBoundary boundary_type,
					       gint           *start_offset,
					       gint           *end_offset)
{
	GailTextUtil *util = get_simple_text (text);
	g_return_val_if_fail (util != NULL, NULL);

	return gail_text_util_get_text (
		util, NULL, GAIL_BEFORE_OFFSET, 
		boundary_type, offset, start_offset, end_offset);
}

char *
eel_accessibility_text_get_text_at_offset (AtkText        *text,
					   gint            offset,
					   AtkTextBoundary boundary_type,
					   gint           *start_offset,
					   gint           *end_offset)
{
	GailTextUtil *util = get_simple_text (text);
	g_return_val_if_fail (util != NULL, NULL);

	return gail_text_util_get_text (
		util, NULL, GAIL_AT_OFFSET,
		boundary_type, offset, start_offset, end_offset);
}

gchar*
eel_accessibility_text_get_text_after_offset  (AtkText	      *text,
					       gint            offset,
					       AtkTextBoundary boundary_type,
					       gint           *start_offset,
					       gint           *end_offset)
{
	GailTextUtil *util = get_simple_text (text);
	g_return_val_if_fail (util != NULL, NULL);

	return gail_text_util_get_text (
		util, NULL, GAIL_AFTER_OFFSET, 
		boundary_type, offset, start_offset, end_offset);
}

gint
eel_accessibility_text_get_character_count (AtkText *text)
{
	GailTextUtil *util = get_simple_text (text);
	g_return_val_if_fail (util != NULL, -1);

	return gtk_text_buffer_get_char_count (util->buffer);
}

GType
eel_accessible_text_get_type (void)
{
	static GType type = 0;

	if (!type) {
		const GTypeInfo tinfo = {
			sizeof (AtkTextIface),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) NULL,
			(GClassFinalizeFunc) NULL
		};

		type = g_type_register_static (
			G_TYPE_INTERFACE, "EelAccessibleText", &tinfo, 0);
	}

	return type;
}

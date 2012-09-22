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
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

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

/*
 * Hacks to make re-using gail somewhat easier.
 */

/**
 * eel_accessibility_create_derived_type:
 * @type_name: the name for the new accessible type eg. NemoIconCanvasItemAccessible
 * @existing_gobject_with_proxy: the GType of an object that has a registered factory that
 *      manufactures the type we want to inherit from. ie. to inherit from a GailCanvasItem
 *      we need to pass GNOME_TYPE_CANVAS_ITEM - since GailCanvasItem is registered against
 *      that type.
 * @opt_gail_parent_class: the name of the Gail class to derive from eg. GailCanvasItem
 * @class_init: the init function to run for this class
 * 
 * This should be run to register the type, it can subsequently be run with
 * the same name and will not re-register it, but simply return it.
 *
 * NB. to do instance init, you prolly want to override AtkObject::initialize
 * 
 * Return value: the registered type, or 0 on failure.
 **/
GType
eel_accessibility_create_derived_type (const char *type_name,
				       GType existing_gobject_with_proxy,
				       EelAccessibilityClassInitFn class_init)
{
	GType type;
	GType parent_atk_type;
	GTypeInfo tinfo = { 0 };
	GTypeQuery query;
	AtkObjectFactory *factory;

	if ((type = g_type_from_name (type_name))) {
		return type;
	}

	factory = atk_registry_get_factory
		(atk_get_default_registry (),
		 existing_gobject_with_proxy);
	if (!factory) {
		return G_TYPE_INVALID;
	}
	
	parent_atk_type = atk_object_factory_get_accessible_type (factory);
	if (!parent_atk_type) {
		return G_TYPE_INVALID;
	}

	/*
	 * Figure out the size of the class and instance 
	 * we are deriving from
	 */
	g_type_query (parent_atk_type, &query);

	if (class_init) {
		tinfo.class_init = (GClassInitFunc) class_init;
	}

	tinfo.class_size    = query.class_size;
	tinfo.instance_size = query.instance_size;

	/* Register the type */
	type = g_type_register_static (
		parent_atk_type, type_name, &tinfo, 0);

	return type;
}


static GQuark
get_quark_accessible (void)
{
	static GQuark quark_accessible_object = 0;

	if (!quark_accessible_object) {
		quark_accessible_object = g_quark_from_static_string
			("accessible-object");
	}

	return quark_accessible_object;
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
 * eel_accessibility_get_atk_object:
 * @object: a GObject of some sort
 * 
 * gets an AtkObject associated with a GObject
 * 
 * Return value: the associated accessible if one exists or NULL
 **/
AtkObject *
eel_accessibility_get_atk_object (gpointer object)
{
	return g_object_get_qdata (object, get_quark_accessible ());
}

/**
 * eel_accessibilty_for_object:
 * @object: a GObject of some sort
 * 
 * gets an AtkObject associated with a GObject and if it doesn't
 * exist creates a suitable accessible object.
 * 
 * Return value: an associated accessible.
 **/
AtkObject *
eel_accessibility_for_object (gpointer object)
{
	if (GTK_IS_WIDGET (object))
		return gtk_widget_get_accessible (object);

	return atk_gobject_accessible_for_object (object);
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

static void
eel_accessibility_destroy (gpointer data,
			   GObject *where_the_object_was)
{
	g_object_set_qdata
		(G_OBJECT (data), get_quark_gobject (), NULL);
	atk_object_notify_state_change
		(ATK_OBJECT (data), ATK_STATE_DEFUNCT, TRUE);
	g_object_unref (data);
}

/**
 * eel_accessibility_set_atk_object_return:
 * @object: a GObject
 * @atk_object: it's AtkObject
 * 
 * used to register and return a new accessible object for something
 * 
 * Return value: @atk_object.
 **/
AtkObject *
eel_accessibility_set_atk_object_return (gpointer   object,
					 AtkObject *atk_object)
{
	atk_object_initialize (atk_object, object);

	if (!ATK_IS_GOBJECT_ACCESSIBLE (atk_object)) {
		g_object_set_qdata_full
			(object, get_quark_accessible (), atk_object,
			 (GDestroyNotify)eel_accessibility_destroy);
		g_object_set_qdata
			(G_OBJECT (atk_object), get_quark_gobject (), object);
	}

	return atk_object;
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

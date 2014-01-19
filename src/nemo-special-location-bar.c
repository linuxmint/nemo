/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
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
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "nemo-special-location-bar.h"
#include "nemo-enum-types.h"

#define NEMO_SPECIAL_LOCATION_BAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NEMO_TYPE_SPECIAL_LOCATION_BAR, NemoSpecialLocationBarPrivate))

struct NemoSpecialLocationBarPrivate
{
	GtkWidget *label;
	NemoSpecialLocation special_location;
};

enum {
	PROP_0,
	PROP_SPECIAL_LOCATION,
};

G_DEFINE_TYPE (NemoSpecialLocationBar, nemo_special_location_bar, GTK_TYPE_INFO_BAR)

static char *
get_message_for_special_location (NemoSpecialLocation location)
{
	char *message;

	switch (location) {
	case NEMO_SPECIAL_LOCATION_TEMPLATES:
		message = g_strdup (_("Files in this folder will appear in the Create Document menu."));
		break;
	case NEMO_SPECIAL_LOCATION_SCRIPTS:
		message = g_strdup (_("Executable files in this folder will appear in the Scripts menu."));
		break;
	default:
		g_assert_not_reached ();
	}

	return message;
}

static void
set_special_location (NemoSpecialLocationBar *bar,
		      NemoSpecialLocation     location)
{
	char *message;

	message = get_message_for_special_location (location);

	gtk_label_set_text (GTK_LABEL (bar->priv->label), message);
	g_free (message);

	gtk_widget_show (bar->priv->label);
}

static void
nemo_special_location_bar_set_property (GObject      *object,
					    guint         prop_id,
					    const GValue *value,
					    GParamSpec   *pspec)
{
	NemoSpecialLocationBar *bar;

	bar = NEMO_SPECIAL_LOCATION_BAR (object);

	switch (prop_id) {
	case PROP_SPECIAL_LOCATION:
		set_special_location (bar, g_value_get_enum (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nemo_special_location_bar_get_property (GObject    *object,
					    guint       prop_id,
					    GValue     *value,
					    GParamSpec *pspec)
{
	NemoSpecialLocationBar *bar;

	bar = NEMO_SPECIAL_LOCATION_BAR (object);

	switch (prop_id) {
	case PROP_SPECIAL_LOCATION:
		g_value_set_enum (value, bar->priv->special_location);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nemo_special_location_bar_class_init (NemoSpecialLocationBarClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = nemo_special_location_bar_get_property;
	object_class->set_property = nemo_special_location_bar_set_property;

	g_type_class_add_private (klass, sizeof (NemoSpecialLocationBarPrivate));

	g_object_class_install_property (object_class,
					 PROP_SPECIAL_LOCATION,
					 g_param_spec_enum ("special-location",
							    "special-location",
							    "special-location",
							    NEMO_TYPE_SPECIAL_LOCATION,
							    NEMO_SPECIAL_LOCATION_TEMPLATES,
							    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
nemo_special_location_bar_init (NemoSpecialLocationBar *bar)
{
	GtkWidget *location_area;
	GtkWidget *action_area;
	PangoAttrList *attrs;

	bar->priv = NEMO_SPECIAL_LOCATION_BAR_GET_PRIVATE (bar);
	location_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (bar));
	action_area = gtk_info_bar_get_action_area (GTK_INFO_BAR (bar));

	gtk_orientable_set_orientation (GTK_ORIENTABLE (action_area), GTK_ORIENTATION_HORIZONTAL);

	attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
	bar->priv->label = gtk_label_new (NULL);
	gtk_label_set_attributes (GTK_LABEL (bar->priv->label), attrs);
	pango_attr_list_unref (attrs);

	gtk_label_set_ellipsize (GTK_LABEL (bar->priv->label), PANGO_ELLIPSIZE_END);
	gtk_container_add (GTK_CONTAINER (location_area), bar->priv->label);
}

GtkWidget *
nemo_special_location_bar_new (NemoSpecialLocation location)
{
	return g_object_new (NEMO_TYPE_SPECIAL_LOCATION_BAR,
			     "message-type", GTK_MESSAGE_QUESTION,
			     "special-location", location,
			     NULL);
}

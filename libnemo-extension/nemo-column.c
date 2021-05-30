/*
 *  nemo-column.c - Info columns exported by NemoColumnProvider
 *                      objects.
 *
 *  Copyright (C) 2003 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 * 
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

#include <config.h>
#include "nemo-column.h"
#include "nemo-extension-i18n.h"

enum {
	PROP_0,
	PROP_NAME,
	PROP_ATTRIBUTE,
	PROP_ATTRIBUTE_Q,
	PROP_LABEL,
	PROP_DESCRIPTION,
	PROP_XALIGN,
    PROP_WIDTH_CHARS,
    PROP_ELLIPSIZE,
	LAST_PROP
};

typedef struct
{
    char *name;
    GQuark attribute;
    char *label;
    char *description;
    float xalign;
    gint width_chars;
    gboolean ellipsize;
 } NemoColumnPrivate;

struct _NemoColumn
{
    GObject parent_object;

    NemoColumnPrivate *details;
};

G_DEFINE_TYPE_WITH_PRIVATE (NemoColumn, nemo_column, G_TYPE_OBJECT)

/**
 * SECTION:nemo-column
 * @Title: NemoColumn
 * @Short_description: A column used in Nemo's  list view.
 *
 * A column is linked to a particular file attribute to display in the view.
 * Many of these are built in to Nemo, but they can also be provided by
 * a #NemoColumnProvider/#NemoInfoProvider extension.
 **/

/**
 * nemo_column_new:
 * @name: identifier of the column
 * @attribute: the file attribute to be displayed in the column
 * @label: the user-visible label for the column
 * @description: a user-visible description of the column
 *
 * Creates a new column
 *
 * Returns: (transfer full): a newly created #NemoColumn
 */
NemoColumn *
nemo_column_new (const char *name,
                 const char *attribute,
		     const char *label,
		     const char *description)
{
	NemoColumn *column;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (attribute != NULL, NULL);
	g_return_val_if_fail (label != NULL, NULL);
	g_return_val_if_fail (description != NULL, NULL);
	
	column = g_object_new (NEMO_TYPE_COLUMN, 
			       "name", name,
			       "attribute", attribute,
			       "label", label,
			       "description", description,
			       NULL);

	return column;
}


/**
 * nemo_column_new2:
 * @name: identifier of the column
 * @attribute: the file attribute to be displayed in the column
 * @label: the user-visible label for the column
 * @description: a user-visible description of the column
 * @width_chars: the default width of the column (-1 for auto-calc)
 * @ellipsize: PangoEllipsizeMode to set when truncating column
 *
 * Creates a new column
 *
 * Returns: (transfer full): a newly created #NemoColumn
 */
NemoColumn *
nemo_column_new2 (const char         *name,
                  const char         *attribute,
                  const char         *label,
                  const char         *description,
                  gint                width_chars,
                  PangoEllipsizeMode  ellipsize)
{
    NemoColumn *column;

    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (attribute != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (description != NULL, NULL);

    column = g_object_new (NEMO_TYPE_COLUMN, 
                           "name", name,
                           "attribute", attribute,
                           "label", label,
                           "description", description,
                           "width-chars", width_chars,
                           "ellipsize", ellipsize,
                           NULL);

    return column;
}

static void
nemo_column_get_property (GObject *object,
			      guint param_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	NemoColumn *column;
	
	column = NEMO_COLUMN (object);
	
	switch (param_id) {
	case PROP_NAME :
		g_value_set_string (value, column->details->name);
		break;
	case PROP_ATTRIBUTE :
		g_value_set_string (value, g_quark_to_string (column->details->attribute));
		break;
	case PROP_ATTRIBUTE_Q :
		g_value_set_uint (value, column->details->attribute);
		break;
	case PROP_LABEL :
		g_value_set_string (value, column->details->label);
		break;
	case PROP_DESCRIPTION :
		g_value_set_string (value, column->details->description);
		break;
	case PROP_XALIGN :
		g_value_set_float (value, column->details->xalign);
		break;
    case PROP_WIDTH_CHARS :
        g_value_set_int (value, column->details->width_chars);
        break;
    case PROP_ELLIPSIZE :
        g_value_set_enum (value, column->details->ellipsize);
        break;
    
	default :
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
nemo_column_set_property (GObject *object,
				 guint param_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
	NemoColumn *column;
	
	column = NEMO_COLUMN (object);

	switch (param_id) {
	case PROP_NAME :
		g_free (column->details->name);
		column->details->name = g_strdup (g_value_get_string (value));
		g_object_notify (object, "name");
		break;
	case PROP_ATTRIBUTE :
		column->details->attribute = g_quark_from_string (g_value_get_string (value));
		g_object_notify (object, "attribute");
		g_object_notify (object, "attribute_q");
		break;
	case PROP_LABEL :
		g_free (column->details->label);
		column->details->label = g_strdup (g_value_get_string (value));
		g_object_notify (object, "label");
		break;
	case PROP_DESCRIPTION :
		g_free (column->details->description);
		column->details->description = g_strdup (g_value_get_string (value));
		g_object_notify (object, "description");
		break;
	case PROP_XALIGN :
		column->details->xalign = g_value_get_float (value);
		g_object_notify (object, "xalign");		
		break;
    case PROP_WIDTH_CHARS :
        column->details->width_chars = g_value_get_int (value);
        g_object_notify (object, "width-chars");
        break;
    case PROP_ELLIPSIZE :
        column->details->ellipsize = g_value_get_enum (value);
        g_object_notify (object, "ellipsize");
        break;

	default :
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
nemo_column_finalize (GObject *object)
{
	NemoColumn *column;
	
	column = NEMO_COLUMN (object);

	g_free (column->details->name);
	g_free (column->details->label);
	g_free (column->details->description);

	g_free (column->details);

	G_OBJECT_CLASS (nemo_column_parent_class)->finalize (object);
}

static void
nemo_column_init (NemoColumn *column)
{
    column->details = G_TYPE_INSTANCE_GET_PRIVATE (column, NEMO_TYPE_COLUMN, NemoColumnPrivate);

    column->details->xalign = 0.0;
}

static void
nemo_column_class_init (NemoColumnClass *class)
{
	G_OBJECT_CLASS (class)->finalize = nemo_column_finalize;
	G_OBJECT_CLASS (class)->get_property = nemo_column_get_property;
	G_OBJECT_CLASS (class)->set_property = nemo_column_set_property;

	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_NAME,
					 g_param_spec_string ("name",
							      "Name",
							      "Name of the column",
							      NULL,
							      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_ATTRIBUTE,
					 g_param_spec_string ("attribute",
							      "Attribute",
							      "The attribute name to display",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_ATTRIBUTE_Q,
					 g_param_spec_uint ("attribute_q",
							    "Attribute quark",
							    "The attribute name to display, in quark form",
							    0, G_MAXUINT, 0,
							    G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_LABEL,
					 g_param_spec_string ("label",
							      "Label",
							      "Label to display in the column",
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_DESCRIPTION,
					 g_param_spec_string ("description",
							      "Description",
							      "A user-visible description of the column",
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_XALIGN,
					 g_param_spec_float ("xalign",
							     "xalign",
							     "The x-alignment of the column",
							     0.0,
							     1.0,
							     0.0,
							     G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                     PROP_WIDTH_CHARS,
                     g_param_spec_int ("width-chars",
                                "Default column width",
                                "Default column width",
                                -1, G_MAXINT, -1,
                                G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                     PROP_ELLIPSIZE,
                     g_param_spec_enum ("ellipsize",
                                "Pango ellipsize mode when text won't fit",
                                "Pango ellipsize mode when text won't fit",
                                PANGO_TYPE_ELLIPSIZE_MODE,
                                PANGO_ELLIPSIZE_NONE,
                                G_PARAM_READWRITE));
}


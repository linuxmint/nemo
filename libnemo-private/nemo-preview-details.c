/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-preview-details.c - Widget for displaying file details in preview pane
 *
 * Copyright (C) 2025 Linux Mint
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
 */

#include "nemo-preview-details.h"
#include <glib/gi18n.h>

struct _NemoPreviewDetails {
	GtkBox parent;
};

typedef struct {
	GtkWidget *grid;
	GtkWidget *name_value_label;
	GtkWidget *size_value_label;
	GtkWidget *type_value_label;
	GtkWidget *modified_value_label;
	GtkWidget *permissions_value_label;
	GtkWidget *location_value_label;

	NemoFile *file;
} NemoPreviewDetailsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NemoPreviewDetails, nemo_preview_details, GTK_TYPE_BOX)

static void
nemo_preview_details_finalize (GObject *object)
{
	NemoPreviewDetails *details;
	NemoPreviewDetailsPrivate *priv;

	details = NEMO_PREVIEW_DETAILS (object);
	priv = nemo_preview_details_get_instance_private (details);

	if (priv->file != NULL) {
		nemo_file_unref (priv->file);
		priv->file = NULL;
	}

	G_OBJECT_CLASS (nemo_preview_details_parent_class)->finalize (object);
}

static GtkWidget *
create_label_pair (GtkGrid *grid, const gchar *label_text, gint row)
{
	GtkWidget *label;
	GtkWidget *value;

	/* Create the label (left column) */
	label = gtk_label_new (label_text);
	gtk_widget_set_halign (label, GTK_ALIGN_END);
	gtk_widget_set_valign (label, GTK_ALIGN_START);
	gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
	gtk_grid_attach (grid, label, 0, row, 1, 1);
	gtk_widget_show (label);

	/* Create the value label (right column) */
	value = gtk_label_new ("");
	gtk_widget_set_halign (value, GTK_ALIGN_START);
	gtk_widget_set_valign (value, GTK_ALIGN_START);
	gtk_label_set_selectable (GTK_LABEL (value), TRUE);
	gtk_label_set_ellipsize (GTK_LABEL (value), PANGO_ELLIPSIZE_MIDDLE);
	gtk_grid_attach (grid, value, 1, row, 1, 1);
	gtk_widget_show (value);

	return value;
}

static void
nemo_preview_details_init (NemoPreviewDetails *details)
{
	NemoPreviewDetailsPrivate *priv;
	GtkGrid *grid;

	priv = nemo_preview_details_get_instance_private (details);

	/* Create the grid for label pairs */
	grid = GTK_GRID (gtk_grid_new ());
	gtk_grid_set_row_spacing (grid, 6);
	gtk_grid_set_column_spacing (grid, 12);
	gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
	gtk_widget_set_margin_end (GTK_WIDGET (grid), 12);
	gtk_widget_set_margin_top (GTK_WIDGET (grid), 12);
	gtk_widget_set_margin_bottom (GTK_WIDGET (grid), 12);
	priv->grid = GTK_WIDGET (grid);

	/* Create all the label pairs */
	priv->name_value_label = create_label_pair (grid, _("Name:"), 0);
	priv->size_value_label = create_label_pair (grid, _("Size:"), 1);
	priv->type_value_label = create_label_pair (grid, _("Type:"), 2);
	priv->modified_value_label = create_label_pair (grid, _("Modified:"), 3);
	priv->permissions_value_label = create_label_pair (grid, _("Permissions:"), 4);
	priv->location_value_label = create_label_pair (grid, _("Location:"), 5);

	gtk_box_pack_start (GTK_BOX (details), GTK_WIDGET (grid), FALSE, FALSE, 0);
	gtk_widget_show (GTK_WIDGET (grid));
}

static void
nemo_preview_details_class_init (NemoPreviewDetailsClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = nemo_preview_details_finalize;
}

GtkWidget *
nemo_preview_details_new (void)
{
	return g_object_new (NEMO_TYPE_PREVIEW_DETAILS,
			     "orientation", GTK_ORIENTATION_VERTICAL,
			     "spacing", 0,
			     NULL);
}

void
nemo_preview_details_set_file (NemoPreviewDetails *widget,
                                NemoFile           *file)
{
	NemoPreviewDetailsPrivate *priv;
	gchar *str;
	GFile *location;
	GFile *parent;
	gchar *parent_path;

	g_return_if_fail (NEMO_IS_PREVIEW_DETAILS (widget));

	priv = nemo_preview_details_get_instance_private (widget);

	if (priv->file == file) {
		return;
	}

	if (priv->file != NULL) {
		nemo_file_unref (priv->file);
	}

	priv->file = file;

	if (file != NULL) {
		nemo_file_ref (file);

		/* Name */
		str = nemo_file_get_display_name (file);
		gtk_label_set_text (GTK_LABEL (priv->name_value_label), str);
		g_free (str);

		/* Size */
		str = nemo_file_get_string_attribute (file, "size");
		if (str != NULL) {
			gtk_label_set_text (GTK_LABEL (priv->size_value_label), str);
			g_free (str);
		} else {
			gtk_label_set_text (GTK_LABEL (priv->size_value_label), "—");
		}

		/* Type */
		str = nemo_file_get_string_attribute (file, "type");
		if (str != NULL) {
			gtk_label_set_text (GTK_LABEL (priv->type_value_label), str);
			g_free (str);
		} else {
			gtk_label_set_text (GTK_LABEL (priv->type_value_label), "—");
		}

		/* Modified */
		str = nemo_file_get_string_attribute (file, "date_modified");
		if (str != NULL) {
			gtk_label_set_text (GTK_LABEL (priv->modified_value_label), str);
			g_free (str);
		} else {
			gtk_label_set_text (GTK_LABEL (priv->modified_value_label), "—");
		}

		/* Permissions */
		str = nemo_file_get_string_attribute (file, "permissions");
		if (str != NULL) {
			gtk_label_set_text (GTK_LABEL (priv->permissions_value_label), str);
			g_free (str);
		} else {
			gtk_label_set_text (GTK_LABEL (priv->permissions_value_label), "—");
		}

		/* Location */
		location = nemo_file_get_location (file);
		parent = g_file_get_parent (location);
		if (parent != NULL) {
			parent_path = g_file_get_parse_name (parent);
			gtk_label_set_text (GTK_LABEL (priv->location_value_label), parent_path);
			g_free (parent_path);
			g_object_unref (parent);
		} else {
			gtk_label_set_text (GTK_LABEL (priv->location_value_label), "—");
		}
		g_object_unref (location);
	}
}

void
nemo_preview_details_clear (NemoPreviewDetails *widget)
{
	NemoPreviewDetailsPrivate *priv;

	g_return_if_fail (NEMO_IS_PREVIEW_DETAILS (widget));

	priv = nemo_preview_details_get_instance_private (widget);

	if (priv->file != NULL) {
		nemo_file_unref (priv->file);
		priv->file = NULL;
	}

	gtk_label_set_text (GTK_LABEL (priv->name_value_label), "");
	gtk_label_set_text (GTK_LABEL (priv->size_value_label), "");
	gtk_label_set_text (GTK_LABEL (priv->type_value_label), "");
	gtk_label_set_text (GTK_LABEL (priv->modified_value_label), "");
	gtk_label_set_text (GTK_LABEL (priv->permissions_value_label), "");
	gtk_label_set_text (GTK_LABEL (priv->location_value_label), "");
}

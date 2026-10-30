/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
 * nemo-preview-details.c - Widget for displaying file details in preview pane
 *
 * Copyright (C) 2026 Linux Mint
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

#include <config.h>
#include "nemo-preview-details.h"
#include <glib/gi18n.h>

struct _NemoPreviewDetails {
    GtkBox parent;
};

typedef struct {
    GtkWidget *name_value_label;
    GtkWidget *size_value_label;
    GtkWidget *type_value_label;
    GtkWidget *row_labels[NEMO_PREVIEW_N_ROWS];
    GtkWidget *row_value_labels[NEMO_PREVIEW_N_ROWS];
    GtkWidget *modified_value_label;

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
create_label_pair (GtkGrid *grid, const gchar *label_text, gint row, GtkWidget **label_out)
{
    GtkWidget *label;
    GtkWidget *value;

    label = gtk_label_new (label_text);
    gtk_widget_set_halign (label, GTK_ALIGN_END);
    gtk_widget_set_valign (label, GTK_ALIGN_START);
    gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
    gtk_grid_attach (grid, label, 0, row, 1, 1);
    gtk_widget_show (label);

    value = gtk_label_new ("");
    gtk_widget_set_halign (value, GTK_ALIGN_START);
    gtk_widget_set_valign (value, GTK_ALIGN_START);
    gtk_label_set_selectable (GTK_LABEL (value), TRUE);
    gtk_label_set_ellipsize (GTK_LABEL (value), PANGO_ELLIPSIZE_MIDDLE);
    gtk_grid_attach (grid, value, 1, row, 1, 1);
    gtk_widget_show (value);

    if (label_out != NULL) {
        *label_out = label;
    }

    return value;
}

static void
set_optional_row (GtkWidget *label, GtkWidget *value, const gchar *text)
{
    gtk_label_set_text (GTK_LABEL (value), text != NULL ? text : "");
    gtk_widget_set_visible (label, text != NULL);
    gtk_widget_set_visible (value, text != NULL);
}

static void
clear_type_rows (NemoPreviewDetails *details)
{
    NemoPreviewDetailsPrivate *priv = nemo_preview_details_get_instance_private (details);
    gint i;

    for (i = 0; i < NEMO_PREVIEW_N_ROWS; i++) {
        set_optional_row (priv->row_labels[i], priv->row_value_labels[i], NULL);
    }
}

static void
nemo_preview_details_init (NemoPreviewDetails *details)
{
    NemoPreviewDetailsPrivate *priv;
    GtkGrid *grid;
    gint i, row;
    const gchar *row_titles[NEMO_PREVIEW_N_ROWS] = {
        [NEMO_PREVIEW_ROW_TITLE] = _("Title:"),
        [NEMO_PREVIEW_ROW_ARTIST] = _("Artist:"),
        [NEMO_PREVIEW_ROW_ALBUM] = _("Album:"),
        [NEMO_PREVIEW_ROW_YEAR] = _("Year:"),
        [NEMO_PREVIEW_ROW_TRACK] = _("Track:"),
        [NEMO_PREVIEW_ROW_GENRE] = _("Genre:"),
        [NEMO_PREVIEW_ROW_DURATION] = _("Duration:"),
        [NEMO_PREVIEW_ROW_DIMENSIONS] = _("Dimensions:"),
        [NEMO_PREVIEW_ROW_FRAME_RATE] = _("Frame Rate:"),
        [NEMO_PREVIEW_ROW_VIDEO_CODEC] = _("Video Codec:"),
        [NEMO_PREVIEW_ROW_AUDIO] = _("Audio:"),
        [NEMO_PREVIEW_ROW_FORMAT] = _("Format:"),
        [NEMO_PREVIEW_ROW_MISSING_CODECS] = _("Missing Codecs:"),
        [NEMO_PREVIEW_ROW_DATE_TAKEN] = _("Date Taken:"),
        [NEMO_PREVIEW_ROW_CAMERA] = _("Camera:"),
        [NEMO_PREVIEW_ROW_EXPOSURE] = _("Exposure:"),
    };

    priv = nemo_preview_details_get_instance_private (details);

    grid = GTK_GRID (gtk_grid_new ());
    gtk_grid_set_row_spacing (grid, 6);
    gtk_grid_set_column_spacing (grid, 12);
    gtk_widget_set_margin_start (GTK_WIDGET (grid), 12);
    gtk_widget_set_margin_end (GTK_WIDGET (grid), 12);
    gtk_widget_set_margin_top (GTK_WIDGET (grid), 12);
    gtk_widget_set_margin_bottom (GTK_WIDGET (grid), 12);

    priv->name_value_label = create_label_pair (grid, _("Name:"), 0, NULL);
    priv->size_value_label = create_label_pair (grid, _("Size:"), 1, NULL);
    priv->type_value_label = create_label_pair (grid, _("Type:"), 2, NULL);

    row = 3;

    for (i = 0; i < NEMO_PREVIEW_N_ROWS; i++) {
        priv->row_value_labels[i] = create_label_pair (grid, row_titles[i], row++, &priv->row_labels[i]);
    }

    priv->modified_value_label = create_label_pair (grid, _("Modified:"), row, NULL);

    clear_type_rows (details);

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

static void
take_value_label_text (GtkWidget *label,
                       gchar     *str)
{
    gtk_label_set_text (GTK_LABEL (label), str != NULL ? str : "—");
    g_free (str);
}

static void
update_file_labels (NemoPreviewDetails *widget)
{
    NemoPreviewDetailsPrivate *priv = nemo_preview_details_get_instance_private (widget);

    take_value_label_text (priv->name_value_label, nemo_file_get_display_name (priv->file));
    take_value_label_text (priv->size_value_label, nemo_file_get_string_attribute (priv->file, "size"));
    take_value_label_text (priv->type_value_label, nemo_file_get_string_attribute (priv->file, "type"));
    take_value_label_text (priv->modified_value_label, nemo_file_get_string_attribute (priv->file, "date_modified"));
}

void
nemo_preview_details_set_file (NemoPreviewDetails *widget,
                               NemoFile           *file)
{
    NemoPreviewDetailsPrivate *priv;

    g_return_if_fail (NEMO_IS_PREVIEW_DETAILS (widget));

    priv = nemo_preview_details_get_instance_private (widget);

    if (priv->file == file) {
        return;
    }

    g_clear_pointer (&priv->file, nemo_file_unref);
    clear_type_rows (widget);

    if (file != NULL) {
        priv->file = nemo_file_ref (file);
        update_file_labels (widget);
    } else {
        gtk_label_set_text (GTK_LABEL (priv->name_value_label), "");
        gtk_label_set_text (GTK_LABEL (priv->size_value_label), "");
        gtk_label_set_text (GTK_LABEL (priv->type_value_label), "");
        gtk_label_set_text (GTK_LABEL (priv->modified_value_label), "");
    }
}

/* Re-reads the basic file attributes, keeping the type-specific rows */
void
nemo_preview_details_refresh (NemoPreviewDetails *widget)
{
    NemoPreviewDetailsPrivate *priv;

    g_return_if_fail (NEMO_IS_PREVIEW_DETAILS (widget));

    priv = nemo_preview_details_get_instance_private (widget);

    if (priv->file != NULL) {
        update_file_labels (widget);
    }
}

void
nemo_preview_details_set_info (NemoPreviewDetails    *widget,
                               const NemoPreviewInfo *info)
{
    NemoPreviewDetailsPrivate *priv;
    gint i;

    g_return_if_fail (NEMO_IS_PREVIEW_DETAILS (widget));
    g_return_if_fail (info != NULL);

    priv = nemo_preview_details_get_instance_private (widget);

    for (i = 0; i < NEMO_PREVIEW_N_ROWS; i++) {
        set_optional_row (priv->row_labels[i], priv->row_value_labels[i], info->rows[i]);
    }
}

void
nemo_preview_details_set_row (NemoPreviewDetails *widget,
                              NemoPreviewRow      row,
                              const gchar        *value)
{
    NemoPreviewDetailsPrivate *priv;

    g_return_if_fail (NEMO_IS_PREVIEW_DETAILS (widget));
    g_return_if_fail (row < NEMO_PREVIEW_N_ROWS);

    priv = nemo_preview_details_get_instance_private (widget);

    set_optional_row (priv->row_labels[row], priv->row_value_labels[row], value);
}

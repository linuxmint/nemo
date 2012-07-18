/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nemo-progress-info-widget.h: file operation progress user interface.
 *
 * Copyright (C) 2007, 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "nemo-progress-info-widget.h"

struct _NemoProgressInfoWidgetPriv {
	NemoProgressInfo *info;

	GtkWidget *status; /* GtkLabel */
	GtkWidget *details; /* GtkLabel */
	GtkWidget *progress_bar;
};

enum {
	PROP_INFO = 1,
	NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL };

G_DEFINE_TYPE (NemoProgressInfoWidget, nemo_progress_info_widget,
               GTK_TYPE_BOX);

static void
info_finished (NemoProgressInfoWidget *self)
{
	gtk_widget_destroy (GTK_WIDGET (self));
}

static void
update_data (NemoProgressInfoWidget *self)
{
	char *status, *details;
	char *markup;

	status = nemo_progress_info_get_status (self->priv->info);
	gtk_label_set_text (GTK_LABEL (self->priv->status), status);
	g_free (status);

	details = nemo_progress_info_get_details (self->priv->info);
	markup = g_markup_printf_escaped ("<span size='small'>%s</span>", details);
	gtk_label_set_markup (GTK_LABEL (self->priv->details), markup);
	g_free (details);
	g_free (markup);
}

static void
update_progress (NemoProgressInfoWidget *self)
{
	double progress;

	progress = nemo_progress_info_get_progress (self->priv->info);
	if (progress < 0) {
		gtk_progress_bar_pulse (GTK_PROGRESS_BAR (self->priv->progress_bar));
	} else {
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->priv->progress_bar), progress);
	}
}

static void
cancel_clicked (GtkWidget *button,
		NemoProgressInfoWidget *self)
{
	nemo_progress_info_cancel (self->priv->info);
	gtk_widget_set_sensitive (button, FALSE);
}

static void
nemo_progress_info_widget_constructed (GObject *obj)
{
	GtkWidget *label, *progress_bar, *hbox, *box, *button, *image;
	NemoProgressInfoWidget *self = NEMO_PROGRESS_INFO_WIDGET (obj);

	G_OBJECT_CLASS (nemo_progress_info_widget_parent_class)->constructed (obj);

	label = gtk_label_new ("status");
	gtk_widget_set_size_request (label, 500, -1);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD_CHAR);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (self),
			    label,
			    TRUE, FALSE,
			    0);
	self->priv->status = label;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);

	progress_bar = gtk_progress_bar_new ();
	self->priv->progress_bar = progress_bar;
	gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR (progress_bar), 0.05);
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX (box),
			   progress_bar,
			   TRUE, FALSE,
			   0);
	gtk_box_pack_start(GTK_BOX (hbox),
			   box,
			   TRUE, TRUE,
			   0);

	image = gtk_image_new_from_stock (GTK_STOCK_CANCEL,
					  GTK_ICON_SIZE_BUTTON);
	button = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (button), image);
	gtk_box_pack_start (GTK_BOX (hbox),
			    button,
			    FALSE,FALSE,
			    0);
	g_signal_connect (button, "clicked",
			  G_CALLBACK (cancel_clicked), self);

	gtk_box_pack_start (GTK_BOX (self),
			    hbox,
			    FALSE,FALSE,
			    0);

	label = gtk_label_new ("details");
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (self),
			    label,
			    TRUE, FALSE,
			    0);
	self->priv->details = label;
	
	gtk_widget_show_all (GTK_WIDGET (self));

	update_data (self);
	update_progress (self);

	g_signal_connect_swapped (self->priv->info,
				  "changed",
				  G_CALLBACK (update_data), self);
	g_signal_connect_swapped (self->priv->info,
				  "progress-changed",
				  G_CALLBACK (update_progress), self);
	g_signal_connect_swapped (self->priv->info,
				  "finished",
				  G_CALLBACK (info_finished), self);
}

static void
nemo_progress_info_widget_dispose (GObject *obj)
{
	NemoProgressInfoWidget *self = NEMO_PROGRESS_INFO_WIDGET (obj);

	g_clear_object (&self->priv->info);

	G_OBJECT_CLASS (nemo_progress_info_widget_parent_class)->dispose (obj);
}

static void
nemo_progress_info_widget_set_property (GObject *object,
					    guint property_id,
					    const GValue *value,
					    GParamSpec *pspec)
{
	NemoProgressInfoWidget *self = NEMO_PROGRESS_INFO_WIDGET (object);

	switch (property_id) {
	case PROP_INFO:
		self->priv->info = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}		
}

static void
nemo_progress_info_widget_init (NemoProgressInfoWidget *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NEMO_TYPE_PROGRESS_INFO_WIDGET,
						  NemoProgressInfoWidgetPriv);

	
}

static void
nemo_progress_info_widget_class_init (NemoProgressInfoWidgetClass *klass)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (klass);
	oclass->set_property = nemo_progress_info_widget_set_property;
	oclass->constructed = nemo_progress_info_widget_constructed;
	oclass->dispose = nemo_progress_info_widget_dispose;

	properties[PROP_INFO] =
		g_param_spec_object ("info",
				     "NemoProgressInfo",
				     "The NemoProgressInfo associated with this widget",
				     NEMO_TYPE_PROGRESS_INFO,
				     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
				     G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);

	g_type_class_add_private (klass, sizeof (NemoProgressInfoWidgetPriv));
}

GtkWidget *
nemo_progress_info_widget_new (NemoProgressInfo *info)
{
	return g_object_new (NEMO_TYPE_PROGRESS_INFO_WIDGET,
			     "info", info,
			     "orientation", GTK_ORIENTATION_VERTICAL,
			     "homogeneous", FALSE,
			     "spacing", 5,
			     NULL);
}

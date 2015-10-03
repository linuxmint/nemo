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
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>
#include <glib/gi18n.h>

#include "nemo-progress-info-widget.h"
#include "nemo-job-queue.h"

enum {
	PROP_INFO = 1,
	NUM_PROPERTIES
};

#define START_ICON "media-playback-start-symbolic"
#define STOP_ICON "media-playback-stop-symbolic"

static GParamSpec *properties[NUM_PROPERTIES] = { NULL };

G_DEFINE_TYPE (NemoProgressInfoWidget, nemo_progress_info_widget,
               GTK_TYPE_BOX);

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
on_info_started (NemoProgressInfoWidget *self) {
    gtk_stack_set_visible_child_name (GTK_STACK (self->priv->stack), "running");
}

static void
on_info_finished (NemoProgressInfoWidget *self)
{
    gtk_widget_destroy (GTK_WIDGET (self));
}

static void
cancel_clicked (GtkWidget *button,
		NemoProgressInfoWidget *self)
{
	nemo_progress_info_cancel (self->priv->info);

    NemoJobQueue *queue = nemo_job_queue_get ();
    nemo_job_queue_start_job_by_info (queue, self->priv->info);

	gtk_widget_set_sensitive (button, FALSE);
}

static void
start_clicked (GtkWidget *button,
               NemoProgressInfoWidget *self)
{
    NemoJobQueue *queue = nemo_job_queue_get ();
    nemo_job_queue_start_job_by_info (queue, self->priv->info);
}

static void
nemo_progress_info_widget_constructed (GObject *obj)
{
	GtkWidget *label, *progress_bar, *hbox, *vbox, *button, *view, *bb, *main_box;
	NemoProgressInfoWidget *self = NEMO_PROGRESS_INFO_WIDGET (obj);
    NemoProgressInfoWidgetPriv *priv = NEMO_PROGRESS_INFO_WIDGET (obj)->priv;

	G_OBJECT_CLASS (nemo_progress_info_widget_parent_class)->constructed (obj);

    main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add (GTK_CONTAINER (self), main_box);

    priv->separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start (GTK_BOX (main_box), priv->separator, FALSE, FALSE, 5);

    priv->stack = gtk_stack_new ();
    gtk_stack_set_transition_type (GTK_STACK (priv->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration (GTK_STACK (priv->stack), 200);
    gtk_box_pack_start (GTK_BOX (main_box), priv->stack, FALSE, FALSE, 0);

    gtk_widget_show_all (main_box);

    /* contruct the initial stack page (job just queued, minimal info and
     * start & cancel buttons) */

    view = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_stack_add_named (GTK_STACK (priv->stack), view, "pending");

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (view), hbox, TRUE, TRUE, 0);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

    gchar *initial_details = nemo_progress_info_get_initial_details (self->priv->info);
    gchar *markup = g_markup_printf_escaped ("<span size='small'>%s</span>", initial_details);

    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), markup);

    g_free (initial_details);
    g_free (markup);

    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD_CHAR);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 50);
    gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 2);
    priv->pre_info = label;

    bb = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign (bb, GTK_ALIGN_START);
    gtk_widget_set_valign (bb, GTK_ALIGN_CENTER);
    gtk_box_pack_end (GTK_BOX (hbox), bb, FALSE, FALSE, 0);

    button = gtk_button_new_from_icon_name (START_ICON, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
    gtk_box_pack_start (GTK_BOX (bb), button, FALSE, FALSE, 2);
    g_signal_connect (button, "clicked", G_CALLBACK (start_clicked), self);

    button = gtk_button_new_from_icon_name (STOP_ICON, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
    gtk_box_pack_start (GTK_BOX (bb), button, FALSE, FALSE, 2);
    g_signal_connect (button, "clicked", G_CALLBACK (cancel_clicked), self);

    gtk_widget_show_all (view);

    /* construct normal in-progress stack page */

    view = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_stack_add_named (GTK_STACK (priv->stack), view, "running");

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (view), hbox, TRUE, TRUE, 0);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

	label = gtk_label_new ("status");
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD_CHAR);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 40);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 2);
	priv->status = label;

	progress_bar = gtk_progress_bar_new ();
	priv->progress_bar = progress_bar;
	gtk_progress_bar_set_pulse_step (GTK_PROGRESS_BAR (progress_bar), 0.05);
	gtk_box_pack_start(GTK_BOX (vbox), progress_bar, TRUE, FALSE, 2);

    label = gtk_label_new ("details");
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 50);

    gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 2);
    priv->details = label;

    bb = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign (bb, GTK_ALIGN_START);
    gtk_widget_set_valign (bb, GTK_ALIGN_CENTER);
    gtk_box_pack_end (GTK_BOX (hbox), bb, FALSE, FALSE, 0);

    button = gtk_button_new_from_icon_name (START_ICON, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
    gtk_widget_set_sensitive (button, FALSE);
    gtk_box_pack_start (GTK_BOX (bb), button, FALSE, FALSE, 2);

	button = gtk_button_new_from_icon_name (STOP_ICON, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
    gtk_box_pack_start (GTK_BOX (bb), button, FALSE, FALSE, 2);
    g_signal_connect (button, "clicked", G_CALLBACK (cancel_clicked), self);

    gtk_widget_show_all (view);

	update_data (self);
	update_progress (self);

    g_signal_connect_swapped (priv->info, "changed", G_CALLBACK (update_data), self);
    g_signal_connect_swapped (priv->info, "progress-changed", G_CALLBACK (update_progress), self);

    gboolean started = nemo_progress_info_get_is_started (self->priv->info);
    gtk_stack_set_visible_child_name (GTK_STACK (self->priv->stack), started ? "running" : "pending");
    g_signal_connect_swapped (priv->info, "started", G_CALLBACK (on_info_started), self);

    g_signal_connect_swapped (priv->info, "finished", G_CALLBACK (on_info_finished), self);
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

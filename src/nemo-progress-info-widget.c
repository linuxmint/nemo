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
#include <string.h>
#include <glib/gi18n.h>

#include "nemo-progress-info-widget.h"
#include "nemo-job-queue.h"
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-speed-graph-math.h>

enum {
	PROP_INFO = 1,
	NUM_PROPERTIES
};

#define SPEED_SMOOTH_MAX 64     /* ring size; the average spans ~1 second */
#define GRAPH_HEIGHT 80

#define START_ICON "xsi-media-playback-start-symbolic"
#define STOP_ICON "xsi-media-playback-stop-symbolic"
#define PAUSE_ICON "xsi-media-playback-pause-symbolic"

static GParamSpec *properties[NUM_PROPERTIES] = { NULL };

G_DEFINE_TYPE (NemoProgressInfoWidget, nemo_progress_info_widget,
               GTK_TYPE_BOX);


static void
speed_graph_text (GtkWidget *widget, cairo_t *cr, const GdkRGBA *color,
                  const char *text, gboolean at_right, double y, gboolean below)
{
    PangoLayout *layout = gtk_widget_create_pango_layout (widget, text);
    PangoFontDescription *fd = pango_font_description_from_string ("Sans 10");
    int tw, th;

    pango_layout_set_font_description (layout, fd);
    pango_layout_get_pixel_size (layout, &tw, &th);
    cairo_set_source_rgba (cr, color->red, color->green, color->blue, 0.9);
    cairo_move_to (cr, at_right ? gtk_widget_get_allocated_width (widget) - tw - 3 : 3,
                   below ? y + 1 : y - th - 1);
    pango_cairo_show_layout (cr, layout);
    pango_font_description_free (fd);
    g_object_unref (layout);
}

static gboolean
speed_graph_draw_cb (GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    NemoProgressInfoWidgetPriv *priv = NEMO_PROGRESS_INFO_WIDGET (user_data)->priv;
    GtkStyleContext *context = gtk_widget_get_style_context (widget);
    int w = gtk_widget_get_allocated_width (widget);
    int h = gtk_widget_get_allocated_height (widget);
    guint n = priv->speed_history->len;
    guint64 *data = (guint64 *) priv->speed_history->data;
    GdkRGBA color, accent;
    guint col, columns;
    double scale, y;
    double dashes[] = { 4.0, 3.0 };

    gtk_render_background (context, cr, 0, 0, w, h);
    gtk_render_frame (context, cr, 0, 0, w, h);

    if (n < 2 || priv->speed_peak == 0) {
        return FALSE;
    }

    gtk_style_context_get_color (context, gtk_style_context_get_state (context), &color);
    /* the theme's accent colour, same as the progress bar; text colour if absent */
    if (!gtk_style_context_lookup_color (context, "theme_selected_bg_color", &accent)) {
        accent = color;
    }

    /* The whole history is stretched over the full width and shrinks as it
     * grows; with more samples than pixels each column is the mean of its
     * bucket. */
    columns = MIN (n, (guint) w);
    /* y-axis: ceiling at a round value just above the peak, changing only in
     * steps; the peak line itself sits at its true height below it */
    priv->graph_max = nemo_speed_nice_ceiling (priv->speed_peak);
    scale = (double) (h - 6) / (double) priv->graph_max;

    cairo_move_to (cr, 0, h);
    for (col = 0; col < columns; col++) {
        double v = nemo_speed_bucket_mean (data, n, columns, col);
        double x = columns > 1 ? (double) col * w / (columns - 1) : 0;
        cairo_line_to (cr, x, h - 1 - v * scale);
    }
    cairo_line_to (cr, w, h);
    cairo_line_to (cr, 0, h);
    cairo_close_path (cr);
    cairo_set_source_rgba (cr, accent.red, accent.green, accent.blue, 0.35);
    cairo_fill_preserve (cr);
    cairo_set_source_rgba (cr, accent.red, accent.green, accent.blue, 1.0);
    cairo_set_line_width (cr, 1.5);
    cairo_stroke (cr);

    /* dashed peak (top) and average lines */
    cairo_set_dash (cr, dashes, 2, 0);
    cairo_set_source_rgba (cr, color.red, color.green, color.blue, 0.6);

    y = h - 1 - priv->speed_peak * scale;
    cairo_move_to (cr, 0, y + 0.5);
    cairo_line_to (cr, w, y + 0.5);
    cairo_stroke (cr);
    {
        g_autofree gchar *pk = g_format_size (priv->speed_peak);
        g_autofree gchar *pt = g_strdup_printf (_("peak: %s/s"), pk);
        speed_graph_text (widget, cr, &color, pt, FALSE, y, TRUE);
    }

    if (priv->avg_speed > 0) {
        g_autofree gchar *av = g_format_size (priv->avg_speed);
        g_autofree gchar *at = g_strdup_printf (_("avg: %s/s"), av);
        PangoLayout *layout = gtk_widget_create_pango_layout (widget, at);
        PangoFontDescription *fd = pango_font_description_from_string ("Sans 10");
        int tw, th;
        double x0;

        pango_layout_set_font_description (layout, fd);
        pango_layout_get_pixel_size (layout, &tw, &th);
        x0 = MAX (w - tw - 3, 0);

        /* the label sits on the average line and interrupts it */
        y = h - 1 - MIN (priv->avg_speed, priv->speed_peak) * scale;
        y = CLAMP (y, th / 2.0 + 1, h - th / 2.0 - 1);
        cairo_set_source_rgba (cr, color.red, color.green, color.blue, 0.6);
        cairo_move_to (cr, 0, y + 0.5);
        cairo_line_to (cr, MAX (x0 - 4, 0), y + 0.5);
        cairo_stroke (cr);

        cairo_set_source_rgba (cr, color.red, color.green, color.blue, 0.9);
        cairo_move_to (cr, x0, y - th / 2.0);
        pango_cairo_show_layout (cr, layout);
        pango_font_description_free (fd);
        g_object_unref (layout);
    }
    cairo_set_dash (cr, NULL, 0, 0);

    return FALSE;
}

/* current speed in the strip next to the graph: number above, unit below */
static void
speed_graph_set_now (NemoProgressInfoWidgetPriv *priv, guint64 speed)
{
    g_autofree gchar *size = g_format_size (speed);
    g_autofree gchar *num = NULL;
    g_autofree gchar *markup = NULL;
    const char *unit = "";
    const char *sep = NULL, *q;

    for (q = size; *q != '\0'; q++) {
        if (*q == ' ' || ((guchar) q[0] == 0xC2 && (guchar) q[1] == 0xA0)) {
            sep = q;
        }
    }
    if (sep != NULL) {
        num = g_strndup (size, sep - size);
        unit = sep + ((guchar) sep[0] == 0xC2 ? 2 : 1);
    } else {
        num = g_strdup (size);
    }
    markup = g_markup_printf_escaped ("<span size='large' weight='bold'>%s</span>\n%s/s", num, unit);
    gtk_label_set_markup (GTK_LABEL (priv->now_label), markup);
}

static void
speed_graph_queue_redraw (NemoProgressInfoWidgetPriv *priv)
{
    if (gtk_widget_get_mapped (priv->graph)) {
        gtk_widget_queue_draw (priv->graph);
    }
}

static gboolean
speed_sample_cb (gpointer user_data)
{
    NemoProgressInfoWidgetPriv *priv = NEMO_PROGRESS_INFO_WIDGET (user_data)->priv;
    gint64 live, delta;
    guint64 speed;
    gboolean paused = nemo_progress_info_get_is_paused (priv->info);

    /* size of the file being written, read off the main loop */
    live = nemo_progress_info_get_live_bytes (priv->info);

    /* A paused operation transfers nothing: freeze the graph and the average
     * instead of appending zeros that keep moving everything. */
    delta = nemo_speed_counters_step (&priv->counters,
                                      nemo_progress_info_get_bytes_done (priv->info),
                                      live, paused);
    if (paused) {
        if (priv->pause_start == 0) {
            priv->pause_start = g_get_monotonic_time ();
        }
        return G_SOURCE_CONTINUE;
    }
    if (priv->pause_start != 0) {
        priv->paused_total += g_get_monotonic_time () - priv->pause_start;
        priv->pause_start = 0;
        priv->recent_count = 0;
        priv->recent_pos = 0;
    }

    priv->recent[priv->recent_pos] = (guint64) delta;
    priv->recent_pos = (priv->recent_pos + 1) % priv->smooth_n;
    if (priv->recent_count < priv->smooth_n) {
        priv->recent_count++;
    }
    speed = nemo_speed_average (priv->recent, priv->recent_count, priv->interval_ms);

    if (priv->speed_history->len >= priv->history_max) {
        g_array_remove_index (priv->speed_history, 0);
    }
    g_array_append_val (priv->speed_history, speed);
    if (speed > priv->speed_peak) {
        priv->speed_peak = speed;
    }

    priv->avg_speed = nemo_speed_overall_average (priv->counters.total,
                                                  g_get_monotonic_time () - priv->start_time - priv->paused_total);

    speed_graph_set_now (priv, speed);
    speed_graph_queue_redraw (priv);

    return G_SOURCE_CONTINUE;
}

static void
speed_graph_stop (NemoProgressInfoWidgetPriv *priv)
{
    if (priv->speed_timeout_id != 0) {
        g_source_remove (priv->speed_timeout_id);
        priv->speed_timeout_id = 0;
    }
}

static void
speed_graph_start (NemoProgressInfoWidget *self)
{
    NemoProgressInfoWidgetPriv *priv = self->priv;

    if (priv->speed_timeout_id != 0) {
        return;
    }
    {
        /* interval in seconds from the preferences, clamped to a sane range */
        double sec = g_settings_get_double (nemo_preferences, NEMO_PREFERENCES_COPY_SPEED_GRAPH_INTERVAL);
        sec = CLAMP (sec, 0.05, 5.0);
        priv->interval_ms = (guint) (sec * 1000.0);
        priv->smooth_n = CLAMP (1000 / priv->interval_ms, 1, SPEED_SMOOTH_MAX);
        priv->history_max = 3600 * 1000 / priv->interval_ms;
    }
    g_array_set_size (priv->speed_history, 0);
    priv->speed_peak = 0;
    priv->avg_speed = 0;
    priv->graph_max = 0;
    gtk_label_set_text (GTK_LABEL (priv->now_label), "");
    priv->recent_count = 0;
    priv->recent_pos = 0;
    /* baselines: bytes already moved before we started looking */
    nemo_speed_counters_init (&priv->counters, nemo_progress_info_get_bytes_done (priv->info));
    priv->start_time = g_get_monotonic_time ();
    priv->pause_start = 0;
    priv->paused_total = 0;
    speed_graph_queue_redraw (priv);
    priv->speed_timeout_id = g_timeout_add (priv->interval_ms, speed_sample_cb, self);
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
on_info_started (NemoProgressInfoWidget *self) {
    gtk_stack_set_visible_child_name (GTK_STACK (self->priv->stack), "running");
    speed_graph_start (self);
}

static void
on_info_finished (NemoProgressInfoWidget *self)
{
    speed_graph_stop (self->priv);
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
    gtk_widget_set_sensitive (self->priv->pending_start_pause_button, FALSE);
    gtk_widget_set_sensitive (self->priv->running_start_pause_button, FALSE);
}

static void
start_pause_clicked (GtkWidget *button,
               NemoProgressInfoWidget *self)
{
    NemoJobQueue *queue = nemo_job_queue_get ();

    if (nemo_progress_info_get_is_paused (self->priv->info)) {
        nemo_progress_info_resume (self->priv->info);
        gtk_image_set_from_icon_name (GTK_IMAGE (gtk_button_get_image (GTK_BUTTON (button))), PAUSE_ICON, GTK_ICON_SIZE_BUTTON);
    } else
    if (nemo_progress_info_get_is_started (self->priv->info)) {
        nemo_progress_info_pause (self->priv->info);
        gtk_image_set_from_icon_name (GTK_IMAGE (gtk_button_get_image (GTK_BUTTON (button))), START_ICON, GTK_ICON_SIZE_BUTTON);
    } else {
        nemo_job_queue_start_job_by_info (queue, self->priv->info);
    }
}

static void
graph_expanded_cb (GObject *expander, GParamSpec *pspec, gpointer data)
{
    GtkWidget *column = g_object_get_data (expander, "nemo-text-column");
    GtkWidget *holder = g_object_get_data (expander, "nemo-button-holder");

    /* notify fires before the relayout: when opening, the column still has
     * its collapsed height, which is what the buttons stay centred on */
    if (gtk_expander_get_expanded (GTK_EXPANDER (expander))) {
        gtk_widget_set_size_request (holder, -1, gtk_widget_get_allocated_height (column));
        gtk_widget_set_valign (holder, GTK_ALIGN_START);
        gtk_widget_set_vexpand (holder, FALSE);
    } else {
        gtk_widget_set_vexpand (holder, TRUE);
    }
}

static void
nemo_progress_info_widget_constructed (GObject *obj)
{
	GtkWidget *label, *progress_bar, *hbox, *vbox, *button, *view, *bb, *main_box, *holder;
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

    priv->pending_start_pause_button = gtk_button_new_from_icon_name (START_ICON, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_relief (GTK_BUTTON (priv->pending_start_pause_button), GTK_RELIEF_NONE);
    gtk_box_pack_start (GTK_BOX (bb), priv->pending_start_pause_button, FALSE, FALSE, 2);
    g_signal_connect (priv->pending_start_pause_button, "clicked", G_CALLBACK (start_pause_clicked), self);

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

    priv->details = label;

    /* the original "N of M - X left" line doubles as the header of a
     * collapsible transfer speed graph */
    priv->graph_expander = gtk_expander_new (NULL);
    gtk_expander_set_label_widget (GTK_EXPANDER (priv->graph_expander), label);
    gtk_box_pack_start (GTK_BOX (vbox), priv->graph_expander, TRUE, FALSE, 2);

    priv->graph = gtk_drawing_area_new ();
    gtk_widget_set_size_request (priv->graph, -1, GRAPH_HEIGHT);
    gtk_widget_set_margin_top (priv->graph, 4);
    gtk_style_context_add_class (gtk_widget_get_style_context (priv->graph), "frame");
    g_signal_connect (priv->graph, "draw", G_CALLBACK (speed_graph_draw_cb), self);

    gtk_container_add (GTK_CONTAINER (priv->graph_expander), priv->graph);
    atk_object_set_name (gtk_widget_get_accessible (priv->graph), _("Transfer speed"));

    /* The buttons are centred against the collapsed height of the text column
     * and keep it when the graph opens: the holder takes that height from the
     * moment just before the expander opens. */
    holder = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign (holder, GTK_ALIGN_FILL);
    {
        /* right column: the buttons on top, the current speed below them,
         * centred vertically next to the graph while it is open */
        GtkWidget *side = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

        priv->now_label = gtk_label_new ("");
        gtk_label_set_justify (GTK_LABEL (priv->now_label), GTK_JUSTIFY_CENTER);
        gtk_widget_set_halign (priv->now_label, GTK_ALIGN_CENTER);
        gtk_widget_set_valign (priv->now_label, GTK_ALIGN_CENTER);
        gtk_widget_set_vexpand (priv->now_label, TRUE);
        /* gtk_widget_show_all() must not undo the binding below */
        gtk_widget_set_no_show_all (priv->now_label, TRUE);
        gtk_box_pack_end (GTK_BOX (hbox), side, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (side), holder, FALSE, FALSE, 0);
        gtk_widget_set_vexpand (holder, TRUE);
        gtk_box_pack_start (GTK_BOX (side), priv->now_label, TRUE, TRUE, 0);
        g_object_bind_property (priv->graph_expander, "expanded",
                                priv->now_label, "visible", G_BINDING_SYNC_CREATE);
    }
    g_object_set_data (G_OBJECT (priv->graph_expander), "nemo-text-column", vbox);
    g_object_set_data (G_OBJECT (priv->graph_expander), "nemo-button-holder", holder);
    g_signal_connect (priv->graph_expander, "notify::expanded", G_CALLBACK (graph_expanded_cb), NULL);

    bb = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign (bb, GTK_ALIGN_START);
    gtk_widget_set_valign (bb, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand (bb, TRUE);
    gtk_box_pack_start (GTK_BOX (holder), bb, TRUE, TRUE, 0);

    priv->running_start_pause_button = gtk_button_new_from_icon_name (PAUSE_ICON, GTK_ICON_SIZE_BUTTON);
    gtk_button_set_relief (GTK_BUTTON (priv->running_start_pause_button), GTK_RELIEF_NONE);
    gtk_box_pack_start (GTK_BOX (bb), priv->running_start_pause_button, FALSE, FALSE, 2);
    g_signal_connect (priv->running_start_pause_button, "clicked", G_CALLBACK (start_pause_clicked), self);

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

    if (started) {
        speed_graph_start (self);
    }
}

static void
nemo_progress_info_widget_dispose (GObject *obj)
{
	NemoProgressInfoWidget *self = NEMO_PROGRESS_INFO_WIDGET (obj);

	speed_graph_stop (self->priv);
	g_clear_pointer (&self->priv->speed_history, g_array_unref);
	g_clear_pointer (&self->priv->recent, g_free);
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

	self->priv->speed_history = g_array_sized_new (FALSE, FALSE, sizeof (guint64), 1024);
	self->priv->recent = g_new0 (guint64, SPEED_SMOOTH_MAX);
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

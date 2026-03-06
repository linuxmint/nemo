/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-overview.c - Disk usage overview page with donut charts.
 *
 * Copyright (C) 2026 Nemo contributors
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include <config.h>
#include "nemo-overview.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <math.h>

#include "nemo-window-slot.h"
#include "nemo-window.h"

/* ── Layout constants ──────────────────────────────────────────── */
#define DONUT_SIZE         160   /* widget allocation per donut    */
#define DONUT_OUTER_R       60   /* outer radius in px             */
#define DONUT_INNER_R       38   /* inner radius (hole)            */
#define DONUT_PADDING       20   /* space around each card         */
#define CARD_PADDING        16   /* padding inside card            */
#define LABEL_GAP            8   /* gap between donut and text     */

/* ── Colours ───────────────────────────────────────────────────── */
typedef struct { double r, g, b; } Rgb;

/* Used-space colour palette – one per drive, cycled */
static const Rgb used_colours[] = {
	{ 0.33, 0.63, 0.91 },  /* blue    */
	{ 0.42, 0.78, 0.44 },  /* green   */
	{ 0.94, 0.60, 0.22 },  /* orange  */
	{ 0.84, 0.36, 0.36 },  /* red     */
	{ 0.62, 0.42, 0.82 },  /* purple  */
	{ 0.24, 0.79, 0.76 },  /* teal    */
};
#define N_COLOURS G_N_ELEMENTS (used_colours)

/* Free-space colour: semi-transparent grey */
static const Rgb free_colour = { 0.75, 0.75, 0.75 };

/* ── Per-volume data ───────────────────────────────────────────── */
typedef struct {
	char    *name;       /* display name                      */
	char    *mount_path; /* mount point, e.g. "/"             */
	char    *device;     /* device node, e.g. "/dev/sda1"     */
	char    *fs_type;    /* filesystem type, e.g. "ext4"      */
	guint64  total;      /* total bytes                       */
	guint64  free_bytes; /* free bytes                        */
	guint64  used;       /* used bytes                        */
	double   fraction;   /* used / total (0.0 – 1.0)         */
	int      colour_idx; /* index into used_colours[]         */
} VolumeInfo;

/* ── Widget ────────────────────────────────────────────────────── */
struct _NemoOverview {
	GtkScrolledWindow parent;
	GtkWidget  *flow_box;
	GArray     *volumes;      /* array of VolumeInfo */
};

G_DEFINE_TYPE (NemoOverview, nemo_overview, GTK_TYPE_SCROLLED_WINDOW)

/* ── Helpers ───────────────────────────────────────────────────── */

static char *
format_size_short (guint64 bytes)
{
	return g_format_size (bytes);
}

static void
volume_info_clear (VolumeInfo *v)
{
	g_free (v->name);
	g_free (v->mount_path);
	g_free (v->device);
	g_free (v->fs_type);
}

/* ── Donut drawing ─────────────────────────────────────────────── */

static gboolean
donut_draw_cb (GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	VolumeInfo *v = user_data;
	int w = gtk_widget_get_allocated_width (widget);
	int h = gtk_widget_get_allocated_height (widget);
	double cx = w / 2.0;
	double cy = h / 2.0;
	double outer = MIN (w, h) / 2.0 - 4;
	double inner = outer * 0.62;
	const Rgb *uc = &used_colours[v->colour_idx % N_COLOURS];

	double start = -M_PI / 2.0;              /* 12 o'clock   */
	double used_end = start + v->fraction * 2.0 * M_PI;

	/* ── used arc ── */
	cairo_new_path (cr);
	cairo_arc (cr, cx, cy, outer, start, used_end);
	cairo_arc_negative (cr, cx, cy, inner, used_end, start);
	cairo_close_path (cr);
	cairo_set_source_rgb (cr, uc->r, uc->g, uc->b);
	cairo_fill (cr);

	/* ── free arc ── */
	if (v->fraction < 0.999) {
		cairo_new_path (cr);
		cairo_arc (cr, cx, cy, outer, used_end, start + 2.0 * M_PI);
		cairo_arc_negative (cr, cx, cy, inner, start + 2.0 * M_PI, used_end);
		cairo_close_path (cr);
		cairo_set_source_rgba (cr, free_colour.r, free_colour.g, free_colour.b, 0.30);
		cairo_fill (cr);
	}

	/* ── percentage text ── */
	{
		PangoLayout *layout;
		PangoFontDescription *fd;
		int tw, th;
		char buf[16];

		g_snprintf (buf, sizeof buf, "%d%%", (int) round (v->fraction * 100.0));

		layout = pango_cairo_create_layout (cr);
		pango_layout_set_text (layout, buf, -1);

		fd = pango_font_description_from_string ("Sans Bold 16");
		pango_layout_set_font_description (layout, fd);
		pango_font_description_free (fd);

		pango_layout_get_pixel_size (layout, &tw, &th);

		/* Use the used-space colour for the percentage text */
		cairo_set_source_rgb (cr, uc->r, uc->g, uc->b);
		cairo_move_to (cr, cx - tw / 2.0, cy - th / 2.0);
		pango_cairo_show_layout (cr, layout);
		g_object_unref (layout);
	}

	return FALSE;
}

/* ── Build a single card widget for one volume ─────────────────── */

static GtkWidget *
create_volume_card (VolumeInfo *v)
{
	GtkWidget *card, *vbox, *donut_area, *name_label, *detail_label;
	char *used_str, *total_str, *free_str, *detail;

	/* Outer frame-like box */
	card = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_margin_start (card, DONUT_PADDING);
	gtk_widget_set_margin_end (card, DONUT_PADDING);
	gtk_widget_set_margin_top (card, DONUT_PADDING);
	gtk_widget_set_margin_bottom (card, DONUT_PADDING);

	/* Style the card with a CSS class */
	GtkStyleContext *sc = gtk_widget_get_style_context (card);
	gtk_style_context_add_class (sc, "overview-card");

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, LABEL_GAP);
	gtk_widget_set_margin_start (vbox, CARD_PADDING);
	gtk_widget_set_margin_end (vbox, CARD_PADDING);
	gtk_widget_set_margin_top (vbox, CARD_PADDING);
	gtk_widget_set_margin_bottom (vbox, CARD_PADDING);
	gtk_box_pack_start (GTK_BOX (card), vbox, TRUE, TRUE, 0);

	/* Donut drawing area */
	donut_area = gtk_drawing_area_new ();
	gtk_widget_set_size_request (donut_area, DONUT_SIZE, DONUT_SIZE);
	g_signal_connect (donut_area, "draw", G_CALLBACK (donut_draw_cb), v);
	gtk_box_pack_start (GTK_BOX (vbox), donut_area, FALSE, FALSE, 0);

	/* Volume name */
	name_label = gtk_label_new (v->name);
	PangoAttrList *attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
	pango_attr_list_insert (attrs, pango_attr_scale_new (1.15));
	gtk_label_set_attributes (GTK_LABEL (name_label), attrs);
	pango_attr_list_unref (attrs);
	gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_MIDDLE);
	gtk_label_set_max_width_chars (GTK_LABEL (name_label), 20);
	gtk_box_pack_start (GTK_BOX (vbox), name_label, FALSE, FALSE, 0);

	/* Detail line: "12.4 GB / 50.0 GB  —  37.6 GB free" */
	used_str  = format_size_short (v->used);
	total_str = format_size_short (v->total);
	free_str  = format_size_short (v->free_bytes);

	detail = g_strdup_printf ("%s / %s  —  %s free", used_str, total_str, free_str);

	detail_label = gtk_label_new (detail);
	gtk_label_set_ellipsize (GTK_LABEL (detail_label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_opacity (detail_label, 0.7);
	gtk_box_pack_start (GTK_BOX (vbox), detail_label, FALSE, FALSE, 0);

	g_free (used_str);
	g_free (total_str);
	g_free (free_str);
	g_free (detail);

	/* Mount point */
	if (v->mount_path != NULL) {
		GtkWidget *mount_label;
		char *mount_text = g_strdup_printf ("%s", v->mount_path);
		mount_label = gtk_label_new (mount_text);
		gtk_widget_set_opacity (mount_label, 0.5);
		gtk_label_set_ellipsize (GTK_LABEL (mount_label), PANGO_ELLIPSIZE_MIDDLE);
		gtk_label_set_max_width_chars (GTK_LABEL (mount_label), 24);
		gtk_box_pack_start (GTK_BOX (vbox), mount_label, FALSE, FALSE, 0);
		g_free (mount_text);
	}

	/* Store mount_path so child-activated can navigate to it */
	if (v->mount_path != NULL)
		g_object_set_data_full (G_OBJECT (card), "mount-path",
		                        g_strdup (v->mount_path), g_free);

	gtk_widget_show_all (card);
	return card;
}

/* ── Gather volume data ────────────────────────────────────────── */

static void
gather_volumes (NemoOverview *self)
{
	GVolumeMonitor *monitor;
	GList *mounts, *l;
	int colour = 0;
	gboolean have_root = FALSE;

	/* Clear old data */
	if (self->volumes->len > 0) {
		g_array_set_size (self->volumes, 0);
	}

	/* Always include the root filesystem first */
	{
		GFile *root_file = g_file_new_for_path ("/");
		GFileInfo *rinfo = g_file_query_filesystem_info (root_file,
			G_FILE_ATTRIBUTE_FILESYSTEM_SIZE ","
			G_FILE_ATTRIBUTE_FILESYSTEM_FREE ","
			G_FILE_ATTRIBUTE_FILESYSTEM_USED ","
			G_FILE_ATTRIBUTE_FILESYSTEM_TYPE,
			NULL, NULL);
		if (rinfo != NULL) {
			VolumeInfo rv = { 0 };
			rv.total = g_file_info_get_attribute_uint64 (rinfo, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
			if (rv.total > 0) {
				rv.free_bytes = g_file_info_get_attribute_uint64 (rinfo, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
				if (g_file_info_has_attribute (rinfo, G_FILE_ATTRIBUTE_FILESYSTEM_USED))
					rv.used = g_file_info_get_attribute_uint64 (rinfo, G_FILE_ATTRIBUTE_FILESYSTEM_USED);
				else
					rv.used = rv.total - rv.free_bytes;
				rv.fraction = (double) rv.used / (double) rv.total;
				rv.colour_idx = colour++;
				rv.name = g_strdup (_("File System"));
				rv.mount_path = g_strdup ("/");
				if (g_file_info_has_attribute (rinfo, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE))
					rv.fs_type = g_strdup (g_file_info_get_attribute_string (rinfo, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE));
				else
					rv.fs_type = g_strdup ("unknown");
				rv.device = g_strdup ("/");
				g_array_append_val (self->volumes, rv);
				have_root = TRUE;
			}
			g_object_unref (rinfo);
		}
		g_object_unref (root_file);
	}

	monitor = g_volume_monitor_get ();
	mounts = g_volume_monitor_get_mounts (monitor);

	for (l = mounts; l != NULL; l = l->next) {
		GMount *mount = G_MOUNT (l->data);
		GFile *root;
		GFileInfo *info;
		VolumeInfo v = { 0 };

		/* Skip shadowed mounts (e.g. snap loopbacks) */
		if (g_mount_is_shadowed (mount))
			continue;

		/* Skip root if we already added it manually */
		if (have_root) {
			GFile *mroot = g_mount_get_root (mount);
			char *mpath = g_file_get_path (mroot);
			g_object_unref (mroot);
			if (mpath != NULL && g_strcmp0 (mpath, "/") == 0) {
				g_free (mpath);
				continue;
			}
			g_free (mpath);
		}

		root = g_mount_get_root (mount);
		info = g_file_query_filesystem_info (root,
			G_FILE_ATTRIBUTE_FILESYSTEM_SIZE ","
			G_FILE_ATTRIBUTE_FILESYSTEM_FREE ","
			G_FILE_ATTRIBUTE_FILESYSTEM_USED ","
			G_FILE_ATTRIBUTE_FILESYSTEM_TYPE,
			NULL, NULL);

		if (info == NULL) {
			g_object_unref (root);
			continue;
		}

		v.total = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
		if (v.total == 0) {
			g_object_unref (info);
			g_object_unref (root);
			continue;
		}

		v.free_bytes = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);

		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_FILESYSTEM_USED))
			v.used = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_USED);
		else
			v.used = v.total - v.free_bytes;

		v.fraction = (double) v.used / (double) v.total;
		v.colour_idx = colour++;

		v.name = g_mount_get_name (mount);
		v.mount_path = g_file_get_path (root);
		if (v.mount_path == NULL)
			v.mount_path = g_file_get_uri (root);

		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE))
			v.fs_type = g_strdup (g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE));
		else
			v.fs_type = g_strdup ("unknown");

		/* Device node from the GVolume if available */
		{
			GVolume *gvol = g_mount_get_volume (mount);
			if (gvol != NULL) {
				v.device = g_volume_get_identifier (gvol, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
				g_object_unref (gvol);
			}
			if (v.device == NULL)
				v.device = g_strdup ("");
		}

		g_array_append_val (self->volumes, v);

		g_object_unref (info);
		g_object_unref (root);
	}

	g_list_free_full (mounts, g_object_unref);
	g_object_unref (monitor);
}

/* ── Build / rebuild UI ────────────────────────────────────────── */

/* Callback: double-click / Enter on a card navigates to that volume */
static void
card_activated_cb (GtkFlowBox      *box,
                   GtkFlowBoxChild *child,
                   gpointer         user_data)
{
	GtkWidget *card;
	const char *mount_path;
	GFile *location;
	GtkWidget *toplevel;
	NemoWindowSlot *slot;

	(void) user_data;

	card = gtk_bin_get_child (GTK_BIN (child));
	if (card == NULL)
		return;

	mount_path = (const char *) g_object_get_data (G_OBJECT (card), "mount-path");
	if (mount_path == NULL)
		return;

	location = g_file_new_for_path (mount_path);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (box));
	if (NEMO_IS_WINDOW (toplevel)) {
		slot = nemo_window_get_active_slot (NEMO_WINDOW (toplevel));
		if (slot != NULL)
			nemo_window_slot_open_location (slot, location, 0);
	}

	g_object_unref (location);
}

static void
rebuild_ui (NemoOverview *self)
{
	guint i;

	/* Clear existing children */
	gtk_container_foreach (GTK_CONTAINER (self->flow_box),
	                       (GtkCallback) gtk_widget_destroy, NULL);

	if (self->volumes->len == 0) {
		GtkWidget *label = gtk_label_new (_("No mounted volumes found."));
		gtk_widget_set_margin_top (label, 40);
		gtk_container_add (GTK_CONTAINER (self->flow_box), label);
		gtk_widget_show (label);
		return;
	}

	for (i = 0; i < self->volumes->len; i++) {
		VolumeInfo *v = &g_array_index (self->volumes, VolumeInfo, i);
		GtkWidget *card = create_volume_card (v);
		gtk_flow_box_insert (GTK_FLOW_BOX (self->flow_box), card, -1);
	}
}

/* ── Public API ────────────────────────────────────────────────── */

void
nemo_overview_refresh (NemoOverview *self)
{
	gather_volumes (self);
	rebuild_ui (self);
}

/* ── GObject boilerplate ───────────────────────────────────────── */

static void
nemo_overview_finalize (GObject *obj)
{
	NemoOverview *self = NEMO_OVERVIEW (obj);
	g_array_unref (self->volumes);
	G_OBJECT_CLASS (nemo_overview_parent_class)->finalize (obj);
}

static void
nemo_overview_class_init (NemoOverviewClass *klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS (klass);
	oclass->finalize = nemo_overview_finalize;
}

static void
nemo_overview_init (NemoOverview *self)
{
	GtkWidget *viewport;

	/* Setup scrolled window */
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self),
	                                GTK_POLICY_NEVER,
	                                GTK_POLICY_AUTOMATIC);

	/* FlowBox for responsive grid layout */
	self->flow_box = gtk_flow_box_new ();
	gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (self->flow_box), TRUE);
	gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (self->flow_box), GTK_SELECTION_SINGLE);
	gtk_flow_box_set_activate_on_single_click (GTK_FLOW_BOX (self->flow_box), FALSE);
	g_signal_connect (self->flow_box, "child-activated",
	                  G_CALLBACK (card_activated_cb), NULL);
	gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (self->flow_box), 6);
	gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (self->flow_box), 1);
	gtk_flow_box_set_column_spacing (GTK_FLOW_BOX (self->flow_box), 0);
	gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (self->flow_box), 0);
	gtk_widget_set_valign (self->flow_box, GTK_ALIGN_START);
	gtk_widget_set_halign (self->flow_box, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_top (self->flow_box, 24);
	gtk_widget_set_margin_bottom (self->flow_box, 24);
	gtk_widget_set_margin_start (self->flow_box, 24);
	gtk_widget_set_margin_end (self->flow_box, 24);

	/* The scrolled window needs a viewport for non-scrollable children */
	viewport = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (viewport), self->flow_box);
	gtk_container_add (GTK_CONTAINER (self), viewport);

	/* Volume data array */
	self->volumes = g_array_new (FALSE, TRUE, sizeof (VolumeInfo));
	g_array_set_clear_func (self->volumes, (GDestroyNotify) volume_info_clear);

	gtk_widget_show_all (GTK_WIDGET (self));
}

GtkWidget *
nemo_overview_new (void)
{
	NemoOverview *self = g_object_new (NEMO_TYPE_OVERVIEW, NULL);
	nemo_overview_refresh (self);
	return GTK_WIDGET (self);
}

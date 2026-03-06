/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-overview.c - Disk usage overview page with donut charts
 *                   and lazy Pareto directory-size bar charts.
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
#include <sys/stat.h>
#include <dirent.h>

#include "nemo-window-slot.h"
#include "nemo-window.h"

/* ── Layout constants ──────────────────────────────────────────── */
#define DONUT_SIZE         110   /* widget allocation per donut    */
#define DONUT_PADDING       12   /* space around each card         */
#define CARD_PADDING        10   /* padding inside card            */
#define LABEL_GAP            6   /* gap between donut and text     */

#define PARETO_BAR_H        22   /* height of each horizontal bar  */
#define PARETO_BAR_GAP       4   /* gap between bars               */
#define PARETO_MAX_DIRS      8   /* show top N directories         */
#define PARETO_LABEL_W     160   /* width reserved for dir name    */
#define PARETO_SIZE_W       80   /* width reserved for size text   */

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
	char    *name;
	char    *mount_path;
	char    *device;
	char    *fs_type;
	guint64  total;
	guint64  free_bytes;
	guint64  used;
	double   fraction;
	int      colour_idx;
} VolumeInfo;

/* ── Per-directory size entry ──────────────────────────────────── */
typedef struct {
	char    *name;
	guint64  size;
} DirSizeEntry;

/* ── Data attached to a pareto drawing area (freed with widget) ── */
typedef struct {
	int     colour_idx;
	GArray *entries;        /* DirSizeEntry, sorted desc by size */
} ParetoDrawData;

/* ── Scan result pushed from bg thread via g_idle_add ──────────── */
typedef struct {
	NemoOverview *self;     /* strong ref – prevents finalize    */
	GCancellable *cancel;   /* strong ref – shared with widget   */
	char         *volume_name;
	char         *mount_path;
	int           colour_idx;
	GArray       *entries;  /* DirSizeEntry, sorted desc         */
} ScanResult;

/* ── Data passed to the background scan thread ─────────────────── */
typedef struct {
	NemoOverview *self;     /* strong ref                        */
	GCancellable *cancel;   /* strong ref                        */
	guint         n_volumes;
	char        **mount_paths;
	char        **volume_names;
	int          *colour_idxs;
} ScanThreadData;

/* ── Widget ────────────────────────────────────────────────────── */
struct _NemoOverview {
	GtkScrolledWindow parent;
	GtkWidget    *main_box;       /* vertical: donuts + sep + pareto */
	GtkWidget    *flow_box;       /* donut cards (row 1)             */
	GtkWidget    *pareto_sep;     /* separator, hidden initially     */
	GtkWidget    *pareto_box;     /* vertical box for bar charts     */
	GArray       *volumes;        /* array of VolumeInfo             */
	GCancellable *scan_cancel;    /* cancel bg scan on destroy       */
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

static void
dir_size_entry_clear (DirSizeEntry *e)
{
	g_free (e->name);
}

static void
pareto_draw_data_free (gpointer data)
{
	ParetoDrawData *pd = data;
	if (pd == NULL) return;
	if (pd->entries)
		g_array_unref (pd->entries);
	g_free (pd);
}

static void
scan_result_free (ScanResult *sr)
{
	if (sr == NULL) return;
	g_clear_object (&sr->cancel);
	g_free (sr->volume_name);
	g_free (sr->mount_path);
	if (sr->entries)
		g_array_unref (sr->entries);
	g_free (sr);
}

static void
scan_thread_data_free (ScanThreadData *td)
{
	guint i;
	if (td == NULL) return;
	g_clear_object (&td->cancel);
	for (i = 0; i < td->n_volumes; i++) {
		g_free (td->mount_paths[i]);
		g_free (td->volume_names[i]);
	}
	g_free (td->mount_paths);
	g_free (td->volume_names);
	g_free (td->colour_idxs);
	g_free (td);
}

static gint
dir_size_compare_desc (gconstpointer a, gconstpointer b)
{
	const DirSizeEntry *ea = a;
	const DirSizeEntry *eb = b;
	if (ea->size > eb->size) return -1;
	if (ea->size < eb->size) return  1;
	return 0;
}

/* ── Recursive directory size (runs on background thread) ──────── */

static guint64
compute_dir_size (const char *path, dev_t dev, GCancellable *cancel)
{
	DIR *dp;
	struct dirent *entry;
	guint64 total = 0;

	if (g_cancellable_is_cancelled (cancel))
		return 0;

	dp = opendir (path);
	if (dp == NULL)
		return 0;

	while ((entry = readdir (dp)) != NULL) {
		struct stat st;
		char *child;

		if (g_cancellable_is_cancelled (cancel))
			break;

		if (g_strcmp0 (entry->d_name, ".") == 0 ||
		    g_strcmp0 (entry->d_name, "..") == 0)
			continue;

		child = g_build_filename (path, entry->d_name, NULL);

		if (lstat (child, &st) == 0) {
			if (S_ISREG (st.st_mode)) {
				total += (guint64) st.st_size;
			} else if (S_ISDIR (st.st_mode) && st.st_dev == dev) {
				total += compute_dir_size (child, dev, cancel);
			}
		}

		g_free (child);
	}

	closedir (dp);
	return total;
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

	double start = -M_PI / 2.0;
	double used_end = start + v->fraction * 2.0 * M_PI;

	/* used arc */
	cairo_new_path (cr);
	cairo_arc (cr, cx, cy, outer, start, used_end);
	cairo_arc_negative (cr, cx, cy, inner, used_end, start);
	cairo_close_path (cr);
	cairo_set_source_rgb (cr, uc->r, uc->g, uc->b);
	cairo_fill (cr);

	/* free arc */
	if (v->fraction < 0.999) {
		cairo_new_path (cr);
		cairo_arc (cr, cx, cy, outer, used_end, start + 2.0 * M_PI);
		cairo_arc_negative (cr, cx, cy, inner, start + 2.0 * M_PI, used_end);
		cairo_close_path (cr);
		cairo_set_source_rgba (cr, free_colour.r, free_colour.g, free_colour.b, 0.30);
		cairo_fill (cr);
	}

	/* percentage text */
	{
		PangoLayout *layout;
		PangoFontDescription *fd;
		int tw, th;
		char buf[16];

		g_snprintf (buf, sizeof buf, "%d%%", (int) round (v->fraction * 100.0));

		layout = pango_cairo_create_layout (cr);
		pango_layout_set_text (layout, buf, -1);

		fd = pango_font_description_from_string ("Sans Bold 13");
		pango_layout_set_font_description (layout, fd);
		pango_font_description_free (fd);

		pango_layout_get_pixel_size (layout, &tw, &th);

		cairo_set_source_rgb (cr, uc->r, uc->g, uc->b);
		cairo_move_to (cr, cx - tw / 2.0, cy - th / 2.0);
		pango_cairo_show_layout (cr, layout);
		g_object_unref (layout);
	}

	return FALSE;
}

/* ── Build a single donut card widget ──────────────────────────── */

static GtkWidget *
create_volume_card (VolumeInfo *v)
{
	GtkWidget *card, *vbox, *donut_area, *name_label, *detail_label;
	char *used_str, *total_str, *free_str, *detail;

	card = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_margin_start (card, DONUT_PADDING);
	gtk_widget_set_margin_end (card, DONUT_PADDING);
	gtk_widget_set_margin_top (card, DONUT_PADDING);
	gtk_widget_set_margin_bottom (card, DONUT_PADDING);

	GtkStyleContext *sc = gtk_widget_get_style_context (card);
	gtk_style_context_add_class (sc, "overview-card");

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, LABEL_GAP);
	gtk_widget_set_margin_start (vbox, CARD_PADDING);
	gtk_widget_set_margin_end (vbox, CARD_PADDING);
	gtk_widget_set_margin_top (vbox, CARD_PADDING);
	gtk_widget_set_margin_bottom (vbox, CARD_PADDING);
	gtk_box_pack_start (GTK_BOX (card), vbox, TRUE, TRUE, 0);

	/* Donut */
	donut_area = gtk_drawing_area_new ();
	gtk_widget_set_size_request (donut_area, DONUT_SIZE, DONUT_SIZE);
	g_signal_connect (donut_area, "draw", G_CALLBACK (donut_draw_cb), v);
	gtk_box_pack_start (GTK_BOX (vbox), donut_area, FALSE, FALSE, 0);

	/* Volume name */
	name_label = gtk_label_new (v->name);
	PangoAttrList *attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
	pango_attr_list_insert (attrs, pango_attr_scale_new (1.05));
	gtk_label_set_attributes (GTK_LABEL (name_label), attrs);
	pango_attr_list_unref (attrs);
	gtk_label_set_ellipsize (GTK_LABEL (name_label), PANGO_ELLIPSIZE_MIDDLE);
	gtk_label_set_max_width_chars (GTK_LABEL (name_label), 18);
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
		GtkWidget *mount_label = gtk_label_new (v->mount_path);
		gtk_widget_set_opacity (mount_label, 0.5);
		gtk_label_set_ellipsize (GTK_LABEL (mount_label), PANGO_ELLIPSIZE_MIDDLE);
		gtk_label_set_max_width_chars (GTK_LABEL (mount_label), 22);
		gtk_box_pack_start (GTK_BOX (vbox), mount_label, FALSE, FALSE, 0);
	}

	/* Store mount_path for navigation */
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

/* ── Pareto bar chart drawing ──────────────────────────────────── */

static gboolean
pareto_draw_cb (GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	ParetoDrawData *pd = user_data;
	int w = gtk_widget_get_allocated_width (widget);
	const Rgb *colour = &used_colours[pd->colour_idx % N_COLOURS];
	guint count = MIN (pd->entries->len, (guint) PARETO_MAX_DIRS);
	guint i;

	if (count == 0) return FALSE;

	guint64 max_size = g_array_index (pd->entries, DirSizeEntry, 0).size;
	if (max_size == 0) return FALSE;

	int bar_start = PARETO_LABEL_W;
	int bar_end   = w - PARETO_SIZE_W;
	int bar_w     = MAX (bar_end - bar_start, 40);

	for (i = 0; i < count; i++) {
		DirSizeEntry *e = &g_array_index (pd->entries, DirSizeEntry, i);
		double y = i * (PARETO_BAR_H + PARETO_BAR_GAP);
		double frac = (double) e->size / (double) max_size;
		double bw = frac * bar_w;

		/* Directory name (left-aligned) */
		{
			PangoLayout *lay = pango_cairo_create_layout (cr);
			PangoFontDescription *fd = pango_font_description_from_string ("Sans 11");
			int tw, th;

			pango_layout_set_font_description (lay, fd);
			pango_font_description_free (fd);
			pango_layout_set_text (lay, e->name, -1);
			pango_layout_set_width (lay, (PARETO_LABEL_W - 12) * PANGO_SCALE);
			pango_layout_set_ellipsize (lay, PANGO_ELLIPSIZE_END);

			pango_layout_get_pixel_size (lay, &tw, &th);

			cairo_set_source_rgba (cr, 0.75, 0.75, 0.75, 1.0);
			cairo_move_to (cr, 0, y + (PARETO_BAR_H - th) / 2.0);
			pango_cairo_show_layout (cr, lay);
			g_object_unref (lay);
		}

		/* Horizontal bar */
		cairo_set_source_rgba (cr, colour->r, colour->g, colour->b, 0.65);
		cairo_rectangle (cr, bar_start, y + 2, bw, PARETO_BAR_H - 4);
		cairo_fill (cr);

		/* Size text (right of bar) */
		{
			char *sz = g_format_size (e->size);
			PangoLayout *lay = pango_cairo_create_layout (cr);
			PangoFontDescription *fd = pango_font_description_from_string ("Sans 10");
			int tw, th;

			pango_layout_set_font_description (lay, fd);
			pango_font_description_free (fd);
			pango_layout_set_text (lay, sz, -1);

			pango_layout_get_pixel_size (lay, &tw, &th);

			cairo_set_source_rgba (cr, 0.65, 0.65, 0.65, 1.0);
			cairo_move_to (cr, bar_end + 8, y + (PARETO_BAR_H - th) / 2.0);
			pango_cairo_show_layout (cr, lay);
			g_object_unref (lay);
			g_free (sz);
		}
	}

	return FALSE;
}

/* ── Idle callback: add one volume's pareto chart to the UI ────── */

static gboolean
pareto_idle_cb (gpointer data)
{
	ScanResult *sr = data;
	NemoOverview *self;
	GtkWidget *heading, *draw_area;
	ParetoDrawData *pd;
	guint count;
	int chart_h;
	char *heading_text;

	/* If scan was cancelled (widget being destroyed), bail out.
	 * Both checks run on the main thread, so no race with
	 * gtk_widget_destroy. */
	if (g_cancellable_is_cancelled (sr->cancel)) {
		g_object_unref (sr->self);
		scan_result_free (sr);
		return G_SOURCE_REMOVE;
	}

	self = sr->self;
	count = MIN (sr->entries->len, (guint) PARETO_MAX_DIRS);

	if (count == 0) {
		g_object_unref (sr->self);
		scan_result_free (sr);
		return G_SOURCE_REMOVE;
	}

	/* Show separator + pareto box on first result */
	if (!gtk_widget_get_visible (self->pareto_sep)) {
		GtkWidget *section_lbl;
		PangoAttrList *sa;

		section_lbl = gtk_label_new (_("Largest Directories"));
		sa = pango_attr_list_new ();
		pango_attr_list_insert (sa, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
		pango_attr_list_insert (sa, pango_attr_scale_new (1.2));
		gtk_label_set_attributes (GTK_LABEL (section_lbl), sa);
		pango_attr_list_unref (sa);
		gtk_widget_set_halign (section_lbl, GTK_ALIGN_START);
		gtk_widget_set_margin_start (section_lbl, 8);
		gtk_widget_set_margin_top (section_lbl, 12);
		gtk_widget_set_margin_bottom (section_lbl, 4);
		gtk_box_pack_start (GTK_BOX (self->pareto_box), section_lbl, FALSE, FALSE, 0);
		gtk_widget_show (section_lbl);

		gtk_widget_show (self->pareto_sep);
		gtk_widget_show (self->pareto_box);
	}

	/* Volume heading */
	heading_text = g_strdup_printf ("%s  (%s)", sr->volume_name, sr->mount_path);
	heading = gtk_label_new (heading_text);
	g_free (heading_text);

	{
		PangoAttrList *ha = pango_attr_list_new ();
		pango_attr_list_insert (ha, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
		pango_attr_list_insert (ha, pango_attr_scale_new (1.05));
		gtk_label_set_attributes (GTK_LABEL (heading), ha);
		pango_attr_list_unref (ha);
	}

	gtk_widget_set_halign (heading, GTK_ALIGN_START);
	gtk_widget_set_margin_start (heading, 8);
	gtk_widget_set_margin_top (heading, 16);
	gtk_widget_set_margin_bottom (heading, 4);
	gtk_box_pack_start (GTK_BOX (self->pareto_box), heading, FALSE, FALSE, 0);
	gtk_widget_show (heading);

	/* Drawing area for bar chart */
	chart_h = count * (PARETO_BAR_H + PARETO_BAR_GAP);
	draw_area = gtk_drawing_area_new ();
	gtk_widget_set_size_request (draw_area, -1, chart_h);
	gtk_widget_set_hexpand (draw_area, TRUE);
	gtk_widget_set_margin_start (draw_area, 8);
	gtk_widget_set_margin_end (draw_area, 8);
	gtk_widget_set_margin_bottom (draw_area, 8);

	/* Attach draw data (freed when drawing area is destroyed) */
	pd = g_new0 (ParetoDrawData, 1);
	pd->colour_idx = sr->colour_idx;
	pd->entries = g_array_ref (sr->entries);
	g_object_set_data_full (G_OBJECT (draw_area), "pareto-data",
	                        pd, pareto_draw_data_free);
	g_signal_connect (draw_area, "draw", G_CALLBACK (pareto_draw_cb), pd);

	gtk_box_pack_start (GTK_BOX (self->pareto_box), draw_area, FALSE, FALSE, 0);
	gtk_widget_show (draw_area);

	g_object_unref (sr->self);
	scan_result_free (sr);
	return G_SOURCE_REMOVE;
}

/* ── Background scan thread ────────────────────────────────────── */

static gpointer
scan_thread_func (gpointer data)
{
	ScanThreadData *td = data;
	guint i;

	for (i = 0; i < td->n_volumes; i++) {
		DIR *dp;
		struct dirent *entry;
		struct stat mount_st;
		GArray *entries;
		ScanResult *sr;

		if (g_cancellable_is_cancelled (td->cancel))
			break;

		if (stat (td->mount_paths[i], &mount_st) != 0)
			continue;

		entries = g_array_new (FALSE, TRUE, sizeof (DirSizeEntry));
		g_array_set_clear_func (entries, (GDestroyNotify) dir_size_entry_clear);

		dp = opendir (td->mount_paths[i]);
		if (dp == NULL) {
			g_array_unref (entries);
			continue;
		}

		while ((entry = readdir (dp)) != NULL) {
			DirSizeEntry de = { 0 };
			char *child_path;
			struct stat st;

			if (g_cancellable_is_cancelled (td->cancel))
				break;

			if (g_strcmp0 (entry->d_name, ".") == 0 ||
			    g_strcmp0 (entry->d_name, "..") == 0)
				continue;

			child_path = g_build_filename (td->mount_paths[i],
			                               entry->d_name, NULL);

			if (lstat (child_path, &st) == 0 &&
			    S_ISDIR (st.st_mode) &&
			    st.st_dev == mount_st.st_dev) {
				de.name = g_strdup (entry->d_name);
				de.size = compute_dir_size (child_path,
				                            mount_st.st_dev,
				                            td->cancel);
				if (de.size > 0)
					g_array_append_val (entries, de);
				else
					g_free (de.name);
			}

			g_free (child_path);
		}

		closedir (dp);

		if (g_cancellable_is_cancelled (td->cancel)) {
			g_array_unref (entries);
			break;
		}

		g_array_sort (entries, dir_size_compare_desc);

		/* Push result to main thread */
		sr = g_new0 (ScanResult, 1);
		sr->self        = g_object_ref (td->self);
		sr->cancel      = g_object_ref (td->cancel);
		sr->volume_name = g_strdup (td->volume_names[i]);
		sr->mount_path  = g_strdup (td->mount_paths[i]);
		sr->colour_idx  = td->colour_idxs[i];
		sr->entries     = entries;   /* ownership transferred */

		g_idle_add (pareto_idle_cb, sr);
	}

	g_object_unref (td->self);
	scan_thread_data_free (td);
	return NULL;
}

/* ── Kick off the background scan ──────────────────────────────── */

static void
start_background_scan (NemoOverview *self)
{
	ScanThreadData *td;
	guint i;

	if (self->volumes->len == 0)
		return;

	/* Cancel any previous scan */
	if (self->scan_cancel != NULL) {
		g_cancellable_cancel (self->scan_cancel);
		g_object_unref (self->scan_cancel);
	}
	self->scan_cancel = g_cancellable_new ();

	/* Hide pareto section (fresh scan) */
	gtk_widget_hide (self->pareto_sep);
	gtk_container_foreach (GTK_CONTAINER (self->pareto_box),
	                       (GtkCallback) gtk_widget_destroy, NULL);
	gtk_widget_hide (self->pareto_box);

	/* Build thread data with copies of volume info */
	td = g_new0 (ScanThreadData, 1);
	td->self         = g_object_ref (self);
	td->cancel       = g_object_ref (self->scan_cancel);
	td->n_volumes    = self->volumes->len;
	td->mount_paths  = g_new0 (char *, td->n_volumes);
	td->volume_names = g_new0 (char *, td->n_volumes);
	td->colour_idxs  = g_new0 (int, td->n_volumes);

	for (i = 0; i < td->n_volumes; i++) {
		VolumeInfo *v = &g_array_index (self->volumes, VolumeInfo, i);
		td->mount_paths[i]  = g_strdup (v->mount_path);
		td->volume_names[i] = g_strdup (v->name);
		td->colour_idxs[i]  = v->colour_idx;
	}

	g_thread_unref (g_thread_new ("overview-scan", scan_thread_func, td));
}

/* ── Build / rebuild donut UI ──────────────────────────────────── */

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

	/* Clear donut cards */
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
	start_background_scan (self);
}

/* ── GObject boilerplate ───────────────────────────────────────── */

static void
nemo_overview_dispose (GObject *obj)
{
	NemoOverview *self = NEMO_OVERVIEW (obj);

	/* Cancel background scan so idle callbacks won't touch dead widgets */
	if (self->scan_cancel != NULL) {
		g_cancellable_cancel (self->scan_cancel);
		g_clear_object (&self->scan_cancel);
	}

	G_OBJECT_CLASS (nemo_overview_parent_class)->dispose (obj);
}

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
	oclass->dispose  = nemo_overview_dispose;
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

	/* Main vertical box holds everything */
	self->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	/* ── Row 1: FlowBox for donut cards ── */
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
	gtk_widget_set_margin_bottom (self->flow_box, 12);
	gtk_widget_set_margin_start (self->flow_box, 24);
	gtk_widget_set_margin_end (self->flow_box, 24);
	gtk_box_pack_start (GTK_BOX (self->main_box), self->flow_box, FALSE, FALSE, 0);

	/* ── Separator (hidden until pareto results arrive) ── */
	self->pareto_sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_margin_start (self->pareto_sep, 32);
	gtk_widget_set_margin_end (self->pareto_sep, 32);
	gtk_widget_set_margin_top (self->pareto_sep, 4);
	gtk_widget_set_margin_bottom (self->pareto_sep, 4);
	gtk_widget_set_no_show_all (self->pareto_sep, TRUE);
	gtk_box_pack_start (GTK_BOX (self->main_box), self->pareto_sep, FALSE, FALSE, 0);

	/* ── Row 2: Pareto bar charts (hidden until scan completes) ── */
	self->pareto_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_margin_start (self->pareto_box, 24);
	gtk_widget_set_margin_end (self->pareto_box, 24);
	gtk_widget_set_margin_bottom (self->pareto_box, 24);
	gtk_widget_set_no_show_all (self->pareto_box, TRUE);
	gtk_box_pack_start (GTK_BOX (self->main_box), self->pareto_box, FALSE, FALSE, 0);

	/* Viewport wraps main_box inside the scrolled window */
	viewport = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (viewport), self->main_box);
	gtk_container_add (GTK_CONTAINER (self), viewport);

	/* Volume data array */
	self->volumes = g_array_new (FALSE, TRUE, sizeof (VolumeInfo));
	g_array_set_clear_func (self->volumes, (GDestroyNotify) volume_info_clear);

	self->scan_cancel = NULL;

	gtk_widget_show_all (GTK_WIDGET (self));
}

GtkWidget *
nemo_overview_new (void)
{
	NemoOverview *self = g_object_new (NEMO_TYPE_OVERVIEW, NULL);
	nemo_overview_refresh (self);
	return GTK_WIDGET (self);
}

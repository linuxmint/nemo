/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-overview.c - Disk usage overview page with donut charts
 *                   and lazy Pareto directory-size vertical bar charts.
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
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#include "nemo-window-slot.h"
#include "nemo-window.h"

/* ── Donut layout ──────────────────────────────────────────────── */
#define DONUT_SIZE         110
#define DONUT_PADDING       12
#define CARD_PADDING        10
#define LABEL_GAP            6

/* ── Vertical-bar Pareto layout ────────────────────────────────── */
#define VBAR_W              28   /* width of each bar              */
#define VBAR_GAP             6   /* gap between bars               */
#define VBAR_MAX_H         130   /* max bar height (tallest bar)   */
#define VBAR_LABEL_H        65   /* label area below bars          */
#define VBAR_SIZE_H         18   /* size-text area above bars      */
#define VBAR_TOTAL_H       (VBAR_SIZE_H + VBAR_MAX_H + VBAR_LABEL_H)
#define VBAR_MAX_BARS       10   /* max bars per chart             */
#define TOP_OFFENDERS_MAX   10   /* deep-scan ranked entries       */
#define CACHE_RESCAN_SECS  120   /* background cache refresh period */

/* ── Colours ───────────────────────────────────────────────────── */
typedef struct { double r, g, b; } Rgb;

static const Rgb used_colours[] = {
	{ 0.33, 0.63, 0.91 },  /* blue    */
	{ 0.42, 0.78, 0.44 },  /* green   */
	{ 0.94, 0.60, 0.22 },  /* orange  */
	{ 0.84, 0.36, 0.36 },  /* red     */
	{ 0.62, 0.42, 0.82 },  /* purple  */
	{ 0.24, 0.79, 0.76 },  /* teal    */
};
#define N_COLOURS G_N_ELEMENTS (used_colours)

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

/* ── Per-directory size entry (for Pareto charts) ──────────────── */
typedef struct {
	char    *name;       /* display name, e.g. "home" or "home/user" */
	char    *full_path;  /* absolute path for navigation             */
	guint64  size;
} DirSizeEntry;

/* ── Data attached to each Pareto drawing area ─────────────────── */
typedef struct {
	int     colour_idx;
	GArray *entries;        /* DirSizeEntry, sorted desc by size */
} ParetoDrawData;

/* ── Scan result pushed from bg thread via g_idle_add ──────────── */
typedef struct {
	NemoOverview *self;
	GCancellable *cancel;
	char         *volume_name;
	char         *mount_path;
	int           colour_idx;
	GArray       *deep;     /* full-depth top offenders          */
} ScanResult;

/* ── Data passed to the background scan thread ─────────────────── */
typedef struct {
	NemoOverview *self;
	GCancellable *cancel;
	guint         n_volumes;
	char        **mount_paths;
	char        **volume_names;
	int          *colour_idxs;
} ScanThreadData;

typedef struct {
	char   *volume_name;
	char   *mount_path;
	int     colour_idx;
	GArray *deep;
	gint64  updated_msec;
} CachedVolumeResult;

typedef struct {
	guint n_volumes;
	char **mount_paths;
	char **volume_names;
	int  *colour_idxs;
} CacheScanJob;

static GHashTable   *g_scan_cache = NULL; /* mount_path => CachedVolumeResult* */
static GMutex        g_scan_cache_lock;
static GCancellable *g_scan_cache_cancel = NULL;
static gboolean      g_scan_cache_running = FALSE;
static guint         g_scan_cache_timer_id = 0;

/* ── Widget ────────────────────────────────────────────────────── */
struct _NemoOverview {
	GtkScrolledWindow parent;
	GtkWidget    *main_box;
	GtkWidget    *flow_box;
	GtkWidget    *pareto_sep;
	GtkWidget    *pareto_box;
	GArray       *volumes;
	GCancellable *scan_cancel;
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
	g_free (e->full_path);
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
	if (sr->deep)
		g_array_unref (sr->deep);
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

static GArray *
dup_entries_array (GArray *src)
{
	GArray *dst;
	guint i;

	if (src == NULL)
		return NULL;

	dst = g_array_new (FALSE, TRUE, sizeof (DirSizeEntry));
	g_array_set_clear_func (dst, (GDestroyNotify) dir_size_entry_clear);

	for (i = 0; i < src->len; i++) {
		DirSizeEntry *s = &g_array_index (src, DirSizeEntry, i);
		DirSizeEntry d = { 0 };
		d.name = g_strdup (s->name);
		d.full_path = g_strdup (s->full_path);
		d.size = s->size;
		g_array_append_val (dst, d);
	}

	return dst;
}

static void
cached_volume_result_free (CachedVolumeResult *cv)
{
	if (cv == NULL)
		return;
	g_free (cv->volume_name);
	g_free (cv->mount_path);
	if (cv->deep)
		g_array_unref (cv->deep);
	g_free (cv);
}

static void
cache_scan_job_free (CacheScanJob *job)
{
	guint i;
	if (job == NULL)
		return;
	for (i = 0; i < job->n_volumes; i++) {
		g_free (job->mount_paths[i]);
		g_free (job->volume_names[i]);
	}
	g_free (job->mount_paths);
	g_free (job->volume_names);
	g_free (job->colour_idxs);
	g_free (job);
}

static void
scan_cache_init_once (void)
{
	g_mutex_lock (&g_scan_cache_lock);
	if (g_scan_cache == NULL)
		g_scan_cache = g_hash_table_new_full (g_str_hash,
		                                     g_str_equal,
		                                     g_free,
		                                     (GDestroyNotify) cached_volume_result_free);
	g_mutex_unlock (&g_scan_cache_lock);
}

static gboolean
scan_cache_lookup_copy (const char *mount_path, ScanResult *sr)
{
	CachedVolumeResult *cv;
	gboolean ok = FALSE;

	if (mount_path == NULL || sr == NULL)
		return FALSE;

	g_mutex_lock (&g_scan_cache_lock);
	cv = g_scan_cache != NULL ? g_hash_table_lookup (g_scan_cache, mount_path) : NULL;
	if (cv != NULL && cv->deep != NULL && cv->deep->len > 0) {
		sr->deep = dup_entries_array (cv->deep);
		ok = (sr->deep != NULL && sr->deep->len > 0);
	}
	g_mutex_unlock (&g_scan_cache_lock);

	return ok;
}

static void
scan_cache_store (const char *volume_name,
                  const char *mount_path,
                  int colour_idx,
                  GArray *deep)
{
	CachedVolumeResult *cv;

	if (mount_path == NULL || deep == NULL)
		return;

	scan_cache_init_once ();

	cv = g_new0 (CachedVolumeResult, 1);
	cv->volume_name = g_strdup (volume_name != NULL ? volume_name : mount_path);
	cv->mount_path = g_strdup (mount_path);
	cv->colour_idx = colour_idx;
	cv->deep = dup_entries_array (deep);
	cv->updated_msec = g_get_monotonic_time () / 1000;

	g_mutex_lock (&g_scan_cache_lock);
	g_hash_table_replace (g_scan_cache, g_strdup (mount_path), cv);
	g_mutex_unlock (&g_scan_cache_lock);
}

/* ── Deep scan helpers (background thread) ─────────────────────── */

static void
consider_top_offender (GArray *top,
                       const char *mount_path,
                       const char *full_path,
                       guint64 size)
{
	DirSizeEntry candidate = { 0 };
	guint i;
	const char *rel;

	if (size == 0 || top == NULL || full_path == NULL)
		return;

	candidate.full_path = g_strdup (full_path);
	if (g_strcmp0 (full_path, mount_path) == 0) {
		candidate.name = g_strdup (".");
	} else if (g_str_has_prefix (full_path, mount_path)) {
		rel = full_path + strlen (mount_path);
		while (*rel == '/')
			rel++;
		candidate.name = g_strdup_printf ("./%s", rel);
	} else {
		candidate.name = g_path_get_basename (full_path);
	}
	candidate.size = size;

	for (i = 0; i < top->len; i++) {
		DirSizeEntry *e = &g_array_index (top, DirSizeEntry, i);
		if (candidate.size > e->size)
			break;
	}

	if (i >= TOP_OFFENDERS_MAX) {
		dir_size_entry_clear (&candidate);
		return;
	}

	if (top->len < TOP_OFFENDERS_MAX) {
		g_array_append_val (top, candidate);
	} else {
		DirSizeEntry *last = &g_array_index (top, DirSizeEntry, top->len - 1);
		dir_size_entry_clear (last);
		*last = candidate;
	}

	for (i = top->len - 1; i > 0; i--) {
		DirSizeEntry *a = &g_array_index (top, DirSizeEntry, i - 1);
		DirSizeEntry *b = &g_array_index (top, DirSizeEntry, i);
		if (a->size >= b->size)
			break;
		DirSizeEntry tmp = *a;
		*a = *b;
		*b = tmp;
	}
}

static guint64
scan_path_collect_top (const char  *path,
	               const char  *mount_path,
	               dev_t        dev,
	               GCancellable *cancel,
	               GArray      *top,
	               gboolean     include_self)
{
	DIR *dp;
	struct dirent *entry;
	struct stat pst;
	guint64 total = 0;

	if (g_cancellable_is_cancelled (cancel))
		return 0;

	if (lstat (path, &pst) != 0)
		return 0;

	if (S_ISREG (pst.st_mode)) {
		total = (guint64) pst.st_size;
		consider_top_offender (top, mount_path, path, total);
		return total;
	}

	if (!S_ISDIR (pst.st_mode) || pst.st_dev != dev)
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
				consider_top_offender (top, mount_path, child,
				                      (guint64) st.st_size);
			} else if (S_ISDIR (st.st_mode) && st.st_dev == dev) {
				total += scan_path_collect_top (child,
				                               mount_path,
				                               dev,
				                               cancel,
				                               top,
				                               TRUE);
			}
		}

		g_free (child);
	}

	closedir (dp);

	if (include_self)
		consider_top_offender (top, mount_path, path, total);

	return total;
}

/* ── Donut drawing (thin ring) ─────────────────────────────────── */

static gboolean
donut_draw_cb (GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	VolumeInfo *v = user_data;
	int w = gtk_widget_get_allocated_width (widget);
	int h = gtk_widget_get_allocated_height (widget);
	double cx = w / 2.0;
	double cy = h / 2.0;
	double outer = MIN (w, h) / 2.0 - 4;
	double inner = outer * 0.78;   /* thinner ring */
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

	/* Detail line */
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

	if (self->volumes->len > 0)
		g_array_set_size (self->volumes, 0);

	/* Root filesystem */
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
				rv.fraction   = (double) rv.used / (double) rv.total;
				rv.colour_idx = colour++;
				rv.name       = g_strdup (_("File System"));
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
	mounts  = g_volume_monitor_get_mounts (monitor);

	for (l = mounts; l != NULL; l = l->next) {
		GMount *mount = G_MOUNT (l->data);
		GFile *root;
		GFileInfo *info;
		VolumeInfo v = { 0 };

		if (g_mount_is_shadowed (mount))
			continue;

		if (have_root) {
			GFile *mroot = g_mount_get_root (mount);
			char *mpath  = g_file_get_path (mroot);
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

		if (info == NULL) { g_object_unref (root); continue; }

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

		v.fraction   = (double) v.used / (double) v.total;
		v.colour_idx = colour++;
		v.name       = g_mount_get_name (mount);
		v.mount_path = g_file_get_path (root);
		if (v.mount_path == NULL)
			v.mount_path = g_file_get_uri (root);

		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE))
			v.fs_type = g_strdup (g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE));
		else
			v.fs_type = g_strdup ("unknown");

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

/* ── Vertical Pareto bar chart drawing ─────────────────────────── */

static gboolean
pareto_draw_cb (GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	ParetoDrawData *pd = user_data;
	const Rgb *colour = &used_colours[pd->colour_idx % N_COLOURS];
	guint count = MIN (pd->entries->len, (guint) VBAR_MAX_BARS);
	guint i;

	if (count == 0) return FALSE;

	guint64 max_size = g_array_index (pd->entries, DirSizeEntry, 0).size;
	if (max_size == 0) return FALSE;

	double baseline_y = VBAR_SIZE_H + VBAR_MAX_H;

	for (i = 0; i < count; i++) {
		DirSizeEntry *e = &g_array_index (pd->entries, DirSizeEntry, i);
		double frac = (double) e->size / (double) max_size;
		double bar_h = frac * VBAR_MAX_H;
		double x = i * (VBAR_W + VBAR_GAP);

		/* ── vertical bar (grows upward from baseline) ── */
		cairo_set_source_rgba (cr, colour->r, colour->g, colour->b, 0.70);
		cairo_rectangle (cr, x, baseline_y - bar_h, VBAR_W, bar_h);
		cairo_fill (cr);

		/* ── size text above bar ── */
		/* ── directory name on top (above bar) ── */
		{
			char *nm = e->name;
			PangoLayout *lay = pango_cairo_create_layout (cr);
			PangoFontDescription *fd =
				pango_font_description_from_string ("Sans 8");
			int tw, th;

			pango_layout_set_font_description (lay, fd);
			pango_font_description_free (fd);
			pango_layout_set_text (lay, nm, -1);
			pango_layout_set_width (lay, VBAR_W * PANGO_SCALE);
			pango_layout_set_ellipsize (lay, PANGO_ELLIPSIZE_END);
			pango_layout_get_pixel_size (lay, &tw, &th);

			cairo_set_source_rgba (cr, 0.65, 0.65, 0.65, 1.0);
			cairo_move_to (cr,
			               x + (VBAR_W - tw) / 2.0,
			               baseline_y - bar_h - th - 2);
			pango_cairo_show_layout (cr, lay);
			g_object_unref (lay);
		}

		/* ── size text below bar ── */
		{
			char *sz = g_format_size (e->size);
			PangoLayout *lay = pango_cairo_create_layout (cr);
			PangoFontDescription *fd =
				pango_font_description_from_string ("Sans 7");
			int tw, th;

			pango_layout_set_font_description (lay, fd);
			pango_font_description_free (fd);
			pango_layout_set_text (lay, e->name, -1);
			pango_layout_set_text (lay, sz, -1);
			pango_layout_get_pixel_size (lay, &tw, &th);

			cairo_set_source_rgba (cr, 0.65, 0.65, 0.65, 1.0);
			cairo_move_to (cr,
			               x + (VBAR_W - tw) / 2.0,
			               baseline_y + 2);
			pango_cairo_show_layout (cr, lay);
			g_object_unref (lay);
			g_free (sz);
		}
	}

	return FALSE;
}

/* ── Double-click on a bar → navigate to that directory ────────── */

static gboolean
pareto_button_press_cb (GtkWidget      *widget,
                        GdkEventButton *event,
                        gpointer        user_data)
{
	ParetoDrawData *pd = user_data;
	guint count;
	int bar_idx;
	DirSizeEntry *e;
	GFile *location;
	GtkWidget *toplevel;

	if (event->type != GDK_2BUTTON_PRESS || event->button != 1)
		return FALSE;

	count = MIN (pd->entries->len, (guint) VBAR_MAX_BARS);
	bar_idx = (int) (event->x) / (VBAR_W + VBAR_GAP);

	if (bar_idx < 0 || bar_idx >= (int) count)
		return FALSE;

	e = &g_array_index (pd->entries, DirSizeEntry, bar_idx);
	if (e->full_path == NULL)
		return FALSE;

	location = g_file_new_for_path (e->full_path);
	toplevel = gtk_widget_get_toplevel (widget);

	if (NEMO_IS_WINDOW (toplevel)) {
		NemoWindowSlot *slot =
			nemo_window_get_active_slot (NEMO_WINDOW (toplevel));
		if (slot != NULL)
			nemo_window_slot_open_location (slot, location, 0);
	}

	g_object_unref (location);
	return TRUE;
}

/* ── Hover tooltip on a bar: full path + size ─────────────────── */

static gboolean
pareto_query_tooltip_cb (GtkWidget  *widget,
                         gint        x,
                         gint        y,
                         gboolean    keyboard_mode,
                         GtkTooltip *tooltip,
                         gpointer    user_data)
{
	ParetoDrawData *pd = user_data;
	guint count;
	int bar_idx;
	DirSizeEntry *e;
	char *sz;
	char *tip;

	(void) widget;
	(void) y;
	(void) keyboard_mode;

	count = MIN (pd->entries->len, (guint) VBAR_MAX_BARS);
	bar_idx = x / (VBAR_W + VBAR_GAP);

	if (bar_idx < 0 || bar_idx >= (int) count)
		return FALSE;

	e = &g_array_index (pd->entries, DirSizeEntry, bar_idx);
	sz = g_format_size (e->size);
	tip = g_strdup_printf ("%s\n%s",
	                       e->full_path != NULL ? e->full_path : e->name,
	                       sz);

	gtk_tooltip_set_text (tooltip, tip);

	g_free (tip);
	g_free (sz);
	return TRUE;
}

static gboolean
list_path_button_press_cb (GtkWidget *widget,
                           GdkEventButton *event,
                           gpointer user_data)
{
	const char *full_path;
	GFile *location;
	GtkWidget *toplevel;

	(void) user_data;

	if (event->button != 1 ||
	    !(event->type == GDK_BUTTON_PRESS || event->type == GDK_2BUTTON_PRESS))
		return FALSE;

	full_path = g_object_get_data (G_OBJECT (widget), "full-path");
	if (full_path == NULL)
		return FALSE;

	location = g_file_new_for_path (full_path);
	toplevel = gtk_widget_get_toplevel (widget);
	if (NEMO_IS_WINDOW (toplevel)) {
		NemoWindowSlot *slot =
			nemo_window_get_active_slot (NEMO_WINDOW (toplevel));
		if (slot != NULL)
			nemo_window_slot_open_location (slot, location, 0);
	}
	g_object_unref (location);
	return TRUE;
}

/* ── Show pointer cursor on chart to hint clickability ─────────── */

static void
pareto_realize_cb (GtkWidget *widget, gpointer data)
{
	GdkCursor *hand;
	(void) data;

	hand = gdk_cursor_new_from_name (gtk_widget_get_display (widget),
	                                 "pointer");
	if (hand != NULL) {
		gdk_window_set_cursor (gtk_widget_get_window (widget), hand);
		g_object_unref (hand);
	}
}

/* ── Helper: create one vertical bar chart widget ──────────────── */

static GtkWidget *
create_pareto_chart (GArray *entries, int colour_idx)
{
	GtkWidget *draw_area;
	ParetoDrawData *pd;
	guint count = MIN (entries->len, (guint) VBAR_MAX_BARS);
	int chart_w = count * (VBAR_W + VBAR_GAP);

	draw_area = gtk_drawing_area_new ();
	gtk_widget_set_size_request (draw_area, chart_w, VBAR_TOTAL_H);
	gtk_widget_set_halign (draw_area, GTK_ALIGN_START);
	gtk_widget_set_margin_start (draw_area, 0);
	gtk_widget_set_margin_end (draw_area, 0);
	gtk_widget_set_margin_bottom (draw_area, 8);
	gtk_widget_set_hexpand (draw_area, TRUE);
	gtk_widget_set_has_tooltip (draw_area, TRUE);

	/* Enable button-press events for double-click navigation */
	gtk_widget_add_events (draw_area, GDK_BUTTON_PRESS_MASK);

	pd = g_new0 (ParetoDrawData, 1);
	pd->colour_idx = colour_idx;
	pd->entries    = g_array_ref (entries);

	g_object_set_data_full (G_OBJECT (draw_area), "pareto-data",
	                        pd, pareto_draw_data_free);
	g_signal_connect (draw_area, "draw",
	                  G_CALLBACK (pareto_draw_cb), pd);
	g_signal_connect (draw_area, "button-press-event",
	                  G_CALLBACK (pareto_button_press_cb), pd);
	g_signal_connect (draw_area, "query-tooltip",
	                  G_CALLBACK (pareto_query_tooltip_cb), pd);
	g_signal_connect (draw_area, "realize",
	                  G_CALLBACK (pareto_realize_cb), NULL);

	return draw_area;
}

/* ── Idle callback: add one volume's charts to the UI ──────────── */

static gboolean
pareto_idle_cb (gpointer data)
{
	ScanResult *sr = data;
	NemoOverview *self;
	GtkWidget *volume_box;
	GtkWidget *heading, *chart;
	char *heading_text;
	gboolean have_deep;
	GList *children, *c;

	if (g_cancellable_is_cancelled (sr->cancel)) {
		g_object_unref (sr->self);
		scan_result_free (sr);
		return G_SOURCE_REMOVE;
	}

	self = sr->self;
	have_deep = (sr->deep != NULL && sr->deep->len > 0);

	if (!have_deep) {
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
		gtk_box_pack_start (GTK_BOX (self->pareto_box),
		                    section_lbl, FALSE, FALSE, 0);
		gtk_widget_show (section_lbl);

		gtk_widget_show (self->pareto_sep);
		gtk_widget_show (self->pareto_box);
	}

	/* Remove old section for this mount before adding refreshed one */
	children = gtk_container_get_children (GTK_CONTAINER (self->pareto_box));
	for (c = children; c != NULL; c = c->next) {
		GtkWidget *child = GTK_WIDGET (c->data);
		const char *mp = g_object_get_data (G_OBJECT (child), "pareto-mount-path");
		if (mp != NULL && g_strcmp0 (mp, sr->mount_path) == 0)
			gtk_widget_destroy (child);
	}
	g_list_free (children);

	volume_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_margin_bottom (volume_box, 8);
	g_object_set_data_full (G_OBJECT (volume_box),
	                        "pareto-mount-path",
	                        g_strdup (sr->mount_path),
	                        g_free);

	/* ── Volume heading ── */
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
	gtk_widget_set_margin_bottom (heading, 2);
	gtk_box_pack_start (GTK_BOX (volume_box), heading, FALSE, FALSE, 0);
	gtk_widget_show (heading);

	/* One deep chart per drive */
	{
		GtkWidget *sub = gtk_label_new (_("Top offenders (full depth)"));
		gtk_widget_set_opacity (sub, 0.6);
		gtk_widget_set_halign (sub, GTK_ALIGN_START);
		gtk_widget_set_margin_start (sub, 8);
		gtk_widget_set_margin_top (sub, 4);
		gtk_box_pack_start (GTK_BOX (volume_box), sub, FALSE, FALSE, 0);
		gtk_widget_show (sub);

		chart = create_pareto_chart (sr->deep, sr->colour_idx);
		gtk_box_pack_start (GTK_BOX (volume_box), chart, FALSE, FALSE, 0);
		gtk_widget_show (chart);
	}

	/* Ranked list, du-like, with clickable full relative paths */
	{
		GtkWidget *list_grid = gtk_grid_new ();
		guint i;
		guint count = MIN (sr->deep->len, (guint) TOP_OFFENDERS_MAX);

		gtk_widget_set_margin_start (list_grid, 8);
		gtk_widget_set_margin_end (list_grid, 8);
		gtk_widget_set_margin_bottom (list_grid, 16);
		gtk_grid_set_row_spacing (GTK_GRID (list_grid), 2);
		gtk_grid_set_column_spacing (GTK_GRID (list_grid), 10);

		for (i = 0; i < count; i++) {
			DirSizeEntry *e = &g_array_index (sr->deep, DirSizeEntry, i);
			GtkWidget *size_lbl;
			GtkWidget *row_click;
			GtkWidget *path_lbl;
			char *sz = g_format_size (e->size);

			size_lbl = gtk_label_new (sz);
			gtk_widget_set_halign (size_lbl, GTK_ALIGN_START);
			gtk_widget_set_opacity (size_lbl, 0.8);
			gtk_grid_attach (GTK_GRID (list_grid), size_lbl, 0, (gint) i, 1, 1);
			gtk_widget_show (size_lbl);

			row_click = gtk_event_box_new ();
			gtk_widget_set_halign (row_click, GTK_ALIGN_FILL);
			gtk_widget_set_hexpand (row_click, TRUE);
			gtk_event_box_set_visible_window (GTK_EVENT_BOX (row_click), FALSE);
			gtk_widget_add_events (row_click, GDK_BUTTON_PRESS_MASK);
			g_object_set_data_full (G_OBJECT (row_click),
			                        "full-path",
			                        g_strdup (e->full_path != NULL ? e->full_path : ""),
			                        g_free);
			g_signal_connect (row_click, "button-press-event",
			                  G_CALLBACK (list_path_button_press_cb), NULL);
			g_signal_connect (row_click, "realize",
			                  G_CALLBACK (pareto_realize_cb), NULL);

			path_lbl = gtk_label_new (e->name);
			gtk_label_set_xalign (GTK_LABEL (path_lbl), 0.0);
			gtk_label_set_ellipsize (GTK_LABEL (path_lbl), PANGO_ELLIPSIZE_MIDDLE);
			gtk_widget_set_halign (path_lbl, GTK_ALIGN_START);
			gtk_widget_set_hexpand (path_lbl, TRUE);
			gtk_widget_set_tooltip_text (path_lbl,
			                             e->full_path != NULL ? e->full_path : e->name);
			gtk_container_add (GTK_CONTAINER (row_click), path_lbl);
			gtk_widget_show (path_lbl);
			gtk_widget_show (row_click);

			gtk_grid_attach (GTK_GRID (list_grid), row_click, 1, (gint) i, 1, 1);
			g_free (sz);
		}

		gtk_box_pack_start (GTK_BOX (volume_box), list_grid, FALSE, FALSE, 0);
		gtk_widget_show (list_grid);
	}

	gtk_box_pack_start (GTK_BOX (self->pareto_box), volume_box, FALSE, FALSE, 0);
	gtk_widget_show (volume_box);

	g_object_unref (sr->self);
	scan_result_free (sr);
	return G_SOURCE_REMOVE;
}

/* ── Background scan thread (full depth top offenders) ─────────── */

static gpointer
scan_thread_func (gpointer data)
{
	ScanThreadData *td = data;
	guint i;

	for (i = 0; i < td->n_volumes; i++) {
		struct stat mount_st;
		GArray *deep;
		ScanResult *sr;

		if (g_cancellable_is_cancelled (td->cancel))
			break;

		if (stat (td->mount_paths[i], &mount_st) != 0)
			continue;

		deep = g_array_new (FALSE, TRUE, sizeof (DirSizeEntry));
		g_array_set_clear_func (deep, (GDestroyNotify) dir_size_entry_clear);

		scan_path_collect_top (td->mount_paths[i],
		                      td->mount_paths[i],
		                      mount_st.st_dev,
		                      td->cancel,
		                      deep,
		                      FALSE);

		if (g_cancellable_is_cancelled (td->cancel)) {
			g_array_unref (deep);
			break;
		}

		scan_cache_store (td->volume_names[i],
		                  td->mount_paths[i],
		                  td->colour_idxs[i],
		                  deep);

		sr = g_new0 (ScanResult, 1);
		sr->self        = g_object_ref (td->self);
		sr->cancel      = g_object_ref (td->cancel);
		sr->volume_name = g_strdup (td->volume_names[i]);
		sr->mount_path  = g_strdup (td->mount_paths[i]);
		sr->colour_idx  = td->colour_idxs[i];
		sr->deep        = deep;

		g_idle_add (pareto_idle_cb, sr);
	}

	g_object_unref (td->self);
	scan_thread_data_free (td);
	return NULL;
}

static CacheScanJob *
build_cache_scan_job (void)
{
	CacheScanJob *job;
	GVolumeMonitor *monitor;
	GList *mounts, *l;
	guint n = 0;
	int colour = 0;
	gboolean have_root = FALSE;

	job = g_new0 (CacheScanJob, 1);

	/* Root filesystem first */
	{
		struct stat st;
		if (stat ("/", &st) == 0) {
			job->n_volumes = 1;
			job->mount_paths = g_new0 (char *, 1);
			job->volume_names = g_new0 (char *, 1);
			job->colour_idxs = g_new0 (int, 1);
			job->mount_paths[0] = g_strdup ("/");
			job->volume_names[0] = g_strdup (_("File System"));
			job->colour_idxs[0] = colour++;
			have_root = TRUE;
			n = 1;
		}
	}

	monitor = g_volume_monitor_get ();
	mounts = g_volume_monitor_get_mounts (monitor);

	for (l = mounts; l != NULL; l = l->next) {
		GMount *mount = G_MOUNT (l->data);
		GFile *root;
		char *mpath;

		if (g_mount_is_shadowed (mount))
			continue;

		if (have_root) {
			GFile *mroot = g_mount_get_root (mount);
			char *p = g_file_get_path (mroot);
			g_object_unref (mroot);
			if (p != NULL && g_strcmp0 (p, "/") == 0) {
				g_free (p);
				continue;
			}
			g_free (p);
		}

		root = g_mount_get_root (mount);
		mpath = g_file_get_path (root);
		if (mpath == NULL) {
			g_object_unref (root);
			continue;
		}

		job->n_volumes = n + 1;
		job->mount_paths = g_renew (char *, job->mount_paths, job->n_volumes);
		job->volume_names = g_renew (char *, job->volume_names, job->n_volumes);
		job->colour_idxs = g_renew (int, job->colour_idxs, job->n_volumes);
		job->mount_paths[n] = mpath;
		job->volume_names[n] = g_mount_get_name (mount);
		job->colour_idxs[n] = colour++;
		n++;

		g_object_unref (root);
	}

	g_list_free_full (mounts, g_object_unref);
	g_object_unref (monitor);
	return job;
}

static gpointer
cache_scan_thread_func (gpointer data)
{
	CacheScanJob *job = data;
	guint i;

	for (i = 0; i < job->n_volumes; i++) {
		struct stat mount_st;
		GArray *deep;

		if (g_scan_cache_cancel != NULL &&
		    g_cancellable_is_cancelled (g_scan_cache_cancel))
			break;

		if (stat (job->mount_paths[i], &mount_st) != 0)
			continue;

		deep = g_array_new (FALSE, TRUE, sizeof (DirSizeEntry));
		g_array_set_clear_func (deep, (GDestroyNotify) dir_size_entry_clear);

		scan_path_collect_top (job->mount_paths[i],
		                      job->mount_paths[i],
		                      mount_st.st_dev,
		                      g_scan_cache_cancel,
		                      deep,
		                      FALSE);

		if (g_scan_cache_cancel != NULL &&
		    g_cancellable_is_cancelled (g_scan_cache_cancel)) {
			g_array_unref (deep);
			break;
		}

		scan_cache_store (job->volume_names[i],
		                  job->mount_paths[i],
		                  job->colour_idxs[i],
		                  deep);
		g_array_unref (deep);
	}

	g_mutex_lock (&g_scan_cache_lock);
	g_scan_cache_running = FALSE;
	g_mutex_unlock (&g_scan_cache_lock);

	cache_scan_job_free (job);
	return NULL;
}

static void
start_cache_scan_if_needed (void)
{
	CacheScanJob *job;

	scan_cache_init_once ();

	g_mutex_lock (&g_scan_cache_lock);
	if (g_scan_cache_running) {
		g_mutex_unlock (&g_scan_cache_lock);
		return;
	}
	if (g_scan_cache_cancel != NULL)
		g_object_unref (g_scan_cache_cancel);
	g_scan_cache_cancel = g_cancellable_new ();
	g_scan_cache_running = TRUE;
	g_mutex_unlock (&g_scan_cache_lock);

	job = build_cache_scan_job ();
	if (job->n_volumes == 0) {
		cache_scan_job_free (job);
		g_mutex_lock (&g_scan_cache_lock);
		g_scan_cache_running = FALSE;
		g_mutex_unlock (&g_scan_cache_lock);
		return;
	}

	g_thread_unref (g_thread_new ("overview-cache-scan", cache_scan_thread_func, job));
}

static gboolean
cache_rescan_timer_cb (gpointer data)
{
	(void) data;
	start_cache_scan_if_needed ();
	return G_SOURCE_CONTINUE;
}

/* ── Kick off the background scan ──────────────────────────────── */

static void
start_background_scan (NemoOverview *self)
{
	ScanThreadData *td;
	guint i;

	if (self->volumes->len == 0)
		return;

	if (self->scan_cancel != NULL) {
		g_cancellable_cancel (self->scan_cancel);
		g_object_unref (self->scan_cancel);
	}
	self->scan_cancel = g_cancellable_new ();

	gtk_widget_hide (self->pareto_sep);
	gtk_container_foreach (GTK_CONTAINER (self->pareto_box),
	                       (GtkCallback) gtk_widget_destroy, NULL);
	gtk_widget_hide (self->pareto_box);

	/* Instant render from cache (if available) */
	for (i = 0; i < self->volumes->len; i++) {
		VolumeInfo *v = &g_array_index (self->volumes, VolumeInfo, i);
		ScanResult *sr = g_new0 (ScanResult, 1);
		sr->self = g_object_ref (self);
		sr->cancel = g_object_ref (self->scan_cancel);
		sr->volume_name = g_strdup (v->name);
		sr->mount_path = g_strdup (v->mount_path);
		sr->colour_idx = v->colour_idx;
		if (scan_cache_lookup_copy (v->mount_path, sr))
			g_idle_add (pareto_idle_cb, sr);
		else
			scan_result_free (sr);
	}

	/* Always keep background rescans running for freshness */
	start_cache_scan_if_needed ();

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

	mount_path = (const char *) g_object_get_data (G_OBJECT (card),
	                                               "mount-path");
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
		VolumeInfo *v  = &g_array_index (self->volumes, VolumeInfo, i);
		GtkWidget *card = create_volume_card (v);
		gtk_flow_box_insert (GTK_FLOW_BOX (self->flow_box), card, -1);
	}
}

/* ── Public API ────────────────────────────────────────────────── */

void
nemo_overview_start_lazy_cache (void)
{
	scan_cache_init_once ();
	start_cache_scan_if_needed ();

	if (g_scan_cache_timer_id == 0)
		g_scan_cache_timer_id = g_timeout_add_seconds (CACHE_RESCAN_SECS,
		                                              cache_rescan_timer_cb,
		                                              NULL);
}

void
nemo_overview_refresh (NemoOverview *self)
{
	nemo_overview_start_lazy_cache ();
	gather_volumes (self);
	rebuild_ui (self);
	start_background_scan (self);
}

/* ── GObject boilerplate ───────────────────────────────────────── */

static void
nemo_overview_dispose (GObject *obj)
{
	NemoOverview *self = NEMO_OVERVIEW (obj);

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

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self),
	                                GTK_POLICY_NEVER,
	                                GTK_POLICY_AUTOMATIC);

	self->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	/* ── Row 1: donut cards ── */
	self->flow_box = gtk_flow_box_new ();
	gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (self->flow_box), TRUE);
	gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (self->flow_box),
	                                 GTK_SELECTION_SINGLE);
	gtk_flow_box_set_activate_on_single_click (
		GTK_FLOW_BOX (self->flow_box), FALSE);
	g_signal_connect (self->flow_box, "child-activated",
	                  G_CALLBACK (card_activated_cb), NULL);
	gtk_flow_box_set_max_children_per_line (
		GTK_FLOW_BOX (self->flow_box), 6);
	gtk_flow_box_set_min_children_per_line (
		GTK_FLOW_BOX (self->flow_box), 1);
	gtk_flow_box_set_column_spacing (
		GTK_FLOW_BOX (self->flow_box), 0);
	gtk_flow_box_set_row_spacing (
		GTK_FLOW_BOX (self->flow_box), 0);
	gtk_widget_set_valign (self->flow_box, GTK_ALIGN_START);
	gtk_widget_set_halign (self->flow_box, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_top (self->flow_box, 24);
	gtk_widget_set_margin_bottom (self->flow_box, 12);
	gtk_widget_set_margin_start (self->flow_box, 24);
	gtk_widget_set_margin_end (self->flow_box, 24);
	gtk_box_pack_start (GTK_BOX (self->main_box),
	                    self->flow_box, FALSE, FALSE, 0);

	/* ── Separator ── */
	self->pareto_sep = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_margin_start (self->pareto_sep, 32);
	gtk_widget_set_margin_end (self->pareto_sep, 32);
	gtk_widget_set_margin_top (self->pareto_sep, 4);
	gtk_widget_set_margin_bottom (self->pareto_sep, 4);
	gtk_widget_set_no_show_all (self->pareto_sep, TRUE);
	gtk_box_pack_start (GTK_BOX (self->main_box),
	                    self->pareto_sep, FALSE, FALSE, 0);

	/* ── Row 2: Pareto charts ── */
	self->pareto_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_margin_start (self->pareto_box, 24);
	gtk_widget_set_margin_end (self->pareto_box, 24);
	gtk_widget_set_margin_bottom (self->pareto_box, 24);
	gtk_widget_set_no_show_all (self->pareto_box, TRUE);
	gtk_box_pack_start (GTK_BOX (self->main_box),
	                    self->pareto_box, FALSE, FALSE, 0);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport),
	                              GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (viewport), self->main_box);
	gtk_container_add (GTK_CONTAINER (self), viewport);

	self->volumes = g_array_new (FALSE, TRUE, sizeof (VolumeInfo));
	g_array_set_clear_func (self->volumes,
	                        (GDestroyNotify) volume_info_clear);

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

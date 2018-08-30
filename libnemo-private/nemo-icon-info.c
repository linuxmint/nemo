/* -*- Mode: C; indent-tabs-mode: f; c-basic-offset: 4; tab-width: 4 -*- */
/* nemo-icon-info.c
 * Copyright (C) 2007  Red Hat, Inc.,  Alexander Larsson <alexl@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#include <config.h>
#include <string.h>
#include "nemo-icon-info.h"
#include "nemo-icon-names.h"
#include "nemo-default-file-icon.h"
#include <gtk/gtk.h>
#include <gio/gio.h>

static void schedule_reap_cache (void);

static void
pixbuf_toggle_notify (gpointer      info,
              GObject      *object,
              gboolean      is_last_ref)
{
    NemoIconInfo  *icon = info;

    if (is_last_ref) {
        icon->sole_owner = TRUE;
        g_object_remove_toggle_ref (object,
                        pixbuf_toggle_notify,
                        info);
        icon->last_use_time = g_get_monotonic_time ();
        schedule_reap_cache ();
    }
}

static void
nemo_icon_info_free (NemoIconInfo *icon)
{
    g_return_if_fail (icon != NULL);

    if (!icon->sole_owner && icon->pixbuf) {
        g_object_remove_toggle_ref (G_OBJECT (icon->pixbuf),
                                    pixbuf_toggle_notify,
                                    icon);
    }

    if (icon->pixbuf) {
        g_object_unref (icon->pixbuf);
    }

    g_free (icon->icon_name);

    g_slice_free (NemoIconInfo, icon);
}

NemoIconInfo *
nemo_icon_info_ref (NemoIconInfo *icon)
{
    g_return_val_if_fail (icon != NULL, NULL);

    icon->ref_count++;

    return icon;
}

void
nemo_icon_info_unref (NemoIconInfo *icon)
{
    g_return_if_fail (icon != NULL);
    g_return_if_fail (icon->ref_count > 0);

    icon->ref_count--;

    if (icon->ref_count == 0) {
        nemo_icon_info_free (icon);
    }
}

void
nemo_icon_info_clear (NemoIconInfo **info)
{
    gpointer _info;

    _info = *info;

    if (_info) {
        *info = NULL;
        nemo_icon_info_unref (_info);
    }
}

static NemoIconInfo *
nemo_icon_info_create (void)
{
    NemoIconInfo *icon;

    icon = g_slice_new0 (NemoIconInfo);

	icon->last_use_time = g_get_monotonic_time ();
	icon->sole_owner = TRUE;
    icon->ref_count = 1;
    
    return icon;
}

gboolean
nemo_icon_info_is_fallback (NemoIconInfo  *icon)
{
  return icon->pixbuf == NULL;
}

NemoIconInfo *
nemo_icon_info_new_for_pixbuf (GdkPixbuf *pixbuf,
                                gint      scale)
{
	NemoIconInfo *icon;

	icon = nemo_icon_info_create ();

	if (pixbuf) {
		icon->pixbuf = g_object_ref (pixbuf);
	}

    icon->orig_scale = scale;

	return icon;
}

static NemoIconInfo *
nemo_icon_info_new_for_icon_info (GtkIconInfo *icon_info,
                                  gint         scale)
{
	NemoIconInfo *icon;
	const char *filename;
	char *basename, *p;

	icon = nemo_icon_info_create ();

	icon->pixbuf = gtk_icon_info_load_icon (icon_info, NULL);

	filename = gtk_icon_info_get_filename (icon_info);
	if (filename != NULL) {
		basename = g_path_get_basename (filename);
		p = strrchr (basename, '.');
		if (p) {
			*p = 0;
		}
		icon->icon_name = basename;
	}

    icon->orig_scale = scale;

	return icon;
}


typedef struct  {
	GIcon *icon;
	int size;
} IconKey;

static GHashTable *loadable_icon_cache = NULL;
static GHashTable *themed_icon_cache = NULL;
static guint reap_cache_timeout = 0;

#define MICROSEC_PER_SEC ((guint64)1000000L)

static guint time_now;

static gboolean
reap_old_icon (gpointer  key,
	       gpointer  value,
	       gpointer  user_info)
{
	NemoIconInfo *icon = value;
	gboolean *reapable_icons_left = user_info;

	if (icon->sole_owner) {
		if (time_now - icon->last_use_time > (gint64)(30 * MICROSEC_PER_SEC)) {
			/* This went unused 30 secs ago. reap */
			return TRUE;
		} else {
			/* We can reap this soon */
			*reapable_icons_left = TRUE;
		}
	}

	return FALSE;
}

static gboolean
reap_cache (gpointer data)
{
	gboolean reapable_icons_left;

	reapable_icons_left = TRUE;

	time_now = g_get_monotonic_time ();

	if (loadable_icon_cache) {
		g_hash_table_foreach_remove (loadable_icon_cache,
					     reap_old_icon,
					     &reapable_icons_left);
	}

	if (themed_icon_cache) {
		g_hash_table_foreach_remove (themed_icon_cache,
					     reap_old_icon,
					     &reapable_icons_left);
	}

	if (reapable_icons_left) {
		return TRUE;
	} else {
		reap_cache_timeout = 0;
		return FALSE;
	}
}

static void
schedule_reap_cache (void)
{
	if (reap_cache_timeout == 0) {
		reap_cache_timeout = g_timeout_add_seconds_full (0, 5,
								 reap_cache,
								 NULL, NULL);
	}
}

void
nemo_icon_info_clear_caches (void)
{
	if (loadable_icon_cache) {
		g_hash_table_remove_all (loadable_icon_cache);
	}

	if (themed_icon_cache) {
		g_hash_table_remove_all (themed_icon_cache);
	}
}

static guint
icon_key_hash (IconKey *key)
{
	return g_icon_hash (key->icon) ^ key->size;
}

static gboolean
icon_key_equal (const IconKey *a,
                const IconKey *b)
{
	return a->size == b->size &&
		g_icon_equal (a->icon, b->icon);
}

static IconKey *
icon_key_new (GIcon *icon, int size)
{
	IconKey *key;

	key = g_slice_new (IconKey);
	key->icon = g_object_ref (icon);
	key->size = size;

	return key;
}

static void
icon_key_free (IconKey *key)
{
	g_object_unref (key->icon);
	g_slice_free (IconKey, key);
}

NemoIconInfo *
nemo_icon_info_lookup (GIcon *icon,
               int size,
               int scale)
{
    GtkIconTheme *icon_theme;
    GtkIconInfo *gtkicon_info;

    NemoIconInfo *icon_info;

    icon_theme = gtk_icon_theme_get_default ();

    if (G_IS_LOADABLE_ICON (icon)) {
        GdkPixbuf *pixbuf;

        IconKey lookup_key;
        IconKey *key;
        GInputStream *stream;

        if (loadable_icon_cache == NULL) {
            loadable_icon_cache = g_hash_table_new_full ((GHashFunc) icon_key_hash,
                                                         (GEqualFunc) icon_key_equal,
                                                         (GDestroyNotify) icon_key_free,
                                                         (GDestroyNotify) nemo_icon_info_free);
        }

        lookup_key.icon = icon;
        lookup_key.size = size;

        icon_info = g_hash_table_lookup (loadable_icon_cache, &lookup_key);
        if (icon_info) {
            return nemo_icon_info_ref (icon_info);
        }

        pixbuf = NULL;
        stream = g_loadable_icon_load (G_LOADABLE_ICON (icon),
                           size * scale,
                           NULL, NULL, NULL);

        if (stream) {
            pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream,
                                      size * scale, size * scale,
                                      TRUE,
                                      NULL, NULL);
            g_input_stream_close (stream, NULL, NULL);
            g_object_unref (stream);
        }

        if (!pixbuf) {
            gtkicon_info = gtk_icon_theme_lookup_icon_for_scale (icon_theme,
                                                                 "text-x-generic",
                                                                 size,
                                                                 scale,
                                                                 GTK_ICON_LOOKUP_FORCE_SIZE);

            pixbuf = gtk_icon_info_load_icon (gtkicon_info, NULL);
        }

        icon_info = nemo_icon_info_new_for_pixbuf (pixbuf, scale);

        key = icon_key_new (icon, size);
        g_hash_table_insert (loadable_icon_cache, key, icon_info);

        g_clear_object (&pixbuf);

        return nemo_icon_info_ref (icon_info);
    } else  {
        IconKey lookup_key;
        IconKey *key;

        if (themed_icon_cache == NULL) {
            themed_icon_cache = g_hash_table_new_full ((GHashFunc) icon_key_hash,
                                                       (GEqualFunc) icon_key_equal,
                                                       (GDestroyNotify) icon_key_free,
                                                       (GDestroyNotify) nemo_icon_info_free);
        }

        lookup_key.icon = icon;
        lookup_key.size = size;

        icon_info = g_hash_table_lookup (themed_icon_cache, &lookup_key);
        if (icon_info) {
            return nemo_icon_info_ref (icon_info);
        }

        gtkicon_info = NULL;

        gtkicon_info = gtk_icon_theme_lookup_by_gicon_for_scale (icon_theme,
                                                                 icon,
                                                                 size,
                                                                 scale,
                                                                 GTK_ICON_LOOKUP_FORCE_SIZE);

        if (!gtkicon_info) {
            gtkicon_info = gtk_icon_theme_lookup_icon_for_scale (icon_theme,
                                                                 "text-x-generic",
                                                                 size,
                                                                 scale,
                                                                 GTK_ICON_LOOKUP_FORCE_SIZE);
        }

        icon_info = nemo_icon_info_new_for_icon_info (gtkicon_info, scale);
        g_object_unref (gtkicon_info);

        key = icon_key_new (icon, size);
        g_hash_table_insert (themed_icon_cache, key, icon_info);

        return nemo_icon_info_ref (icon_info);
    }
}

NemoIconInfo *
nemo_icon_info_lookup_from_name (const char *name,
                                 int size,
                                 int scale)
{
	GIcon *icon;
	NemoIconInfo *info;

	icon = g_themed_icon_new (name);
	info = nemo_icon_info_lookup (icon, size, scale);
	g_object_unref (icon);
	return info;
}

NemoIconInfo *
nemo_icon_info_lookup_from_path (const char *path,
                                 int size,
                                 int scale)
{
	GFile *icon_file;
	GIcon *icon;
	NemoIconInfo *info;

	icon_file = g_file_new_for_path (path);
	icon = g_file_icon_new (icon_file);
	info = nemo_icon_info_lookup (icon, size, scale);
	g_object_unref (icon);
	g_object_unref (icon_file);
	return info;
}

GdkPixbuf *
nemo_icon_info_get_pixbuf_nodefault (NemoIconInfo  *icon)
{
	GdkPixbuf *res;

	if (icon->pixbuf == NULL) {
		res = NULL;
	} else {
		res = g_object_ref (icon->pixbuf);

		if (icon->sole_owner) {
			icon->sole_owner = FALSE;
			g_object_add_toggle_ref (G_OBJECT (res),
						 pixbuf_toggle_notify,
						 icon);
		}
	}

	return res;
}


GdkPixbuf *
nemo_icon_info_get_pixbuf (NemoIconInfo *icon)
{
	GdkPixbuf *res;

	res = nemo_icon_info_get_pixbuf_nodefault (icon);
	if (res == NULL) {
		res = gdk_pixbuf_new_from_data (nemo_default_file_icon,
						GDK_COLORSPACE_RGB,
						TRUE,
						8,
						nemo_default_file_icon_width,
						nemo_default_file_icon_height,
						nemo_default_file_icon_width * 4, /* stride */
						NULL, /* don't destroy info */
						NULL);
	}

	return res;
}

GdkPixbuf *
nemo_icon_info_get_pixbuf_nodefault_at_size (NemoIconInfo  *icon,
						 gsize              forced_size)
{
	GdkPixbuf *pixbuf, *scaled_pixbuf;
	guint w, h, s;
	double scale;

	pixbuf = nemo_icon_info_get_pixbuf_nodefault (icon);

	if (pixbuf == NULL)
	  return NULL;

	w = gdk_pixbuf_get_width (pixbuf) / icon->orig_scale;
	h = gdk_pixbuf_get_height (pixbuf) / icon->orig_scale;
	s = MAX (w, h);
	if (s == forced_size) {
		return pixbuf;
	}

	scale = (double)forced_size / s;
	scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
						 MAX (w * scale, 1), MAX (h * scale, 1),
						 GDK_INTERP_BILINEAR);
	g_object_unref (pixbuf);
	return scaled_pixbuf;
}


GdkPixbuf *
nemo_icon_info_get_pixbuf_at_size (NemoIconInfo  *icon,
				       gsize              forced_size)
{
	GdkPixbuf *pixbuf, *scaled_pixbuf;
	guint w, h, s;
	double scale;

	pixbuf = nemo_icon_info_get_pixbuf (icon);

	w = gdk_pixbuf_get_width (pixbuf) / icon->orig_scale;
	h = gdk_pixbuf_get_height (pixbuf) / icon->orig_scale;
	s = MAX (w, h);
	if (s == forced_size) {
		return pixbuf;
	}

	scale = (double)forced_size / s;
	scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
						 MAX (w * scale, 1), MAX (h * scale, 1),
						 GDK_INTERP_BILINEAR);
	g_object_unref (pixbuf);
	return scaled_pixbuf;
}

GdkPixbuf *
nemo_icon_info_get_desktop_pixbuf_at_size (NemoIconInfo  *icon,
                                           gsize          max_height,
                                           gsize          max_width)
{
    GdkPixbuf *pixbuf, *scaled_pixbuf;
    guint w, h;
    double scale;

    pixbuf = nemo_icon_info_get_pixbuf (icon);

    w = gdk_pixbuf_get_width (pixbuf) / icon->orig_scale;
    h = gdk_pixbuf_get_height (pixbuf) / icon->orig_scale;

    if (w == max_width || h == max_height) {
        return pixbuf;
    }

    scale = (gdouble) max_height / h;

    if (w * scale > max_width) {
        scale = (gdouble) max_width / w;
    }

    scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
                         MAX (w * scale, 1), MAX (h * scale, 1),
                         GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);
    return scaled_pixbuf;
}

const char *
nemo_icon_info_get_used_name (NemoIconInfo  *icon)
{
	return icon->icon_name;
}

/* Return nominal icon size for given zoom level.
 * @zoom_level: zoom level for which to find matching icon size.
 *
 * Return value: icon size between NEMO_ICON_SIZE_SMALLEST and
 * NEMO_ICON_SIZE_LARGEST, inclusive.
 */
guint
nemo_get_icon_size_for_zoom_level (NemoZoomLevel zoom_level)
{
	switch (zoom_level) {
	case NEMO_ZOOM_LEVEL_SMALLEST:
		return NEMO_ICON_SIZE_SMALLEST;
	case NEMO_ZOOM_LEVEL_SMALLER:
		return NEMO_ICON_SIZE_SMALLER;
	case NEMO_ZOOM_LEVEL_SMALL:
		return NEMO_ICON_SIZE_SMALL;
	case NEMO_ZOOM_LEVEL_STANDARD:
		return NEMO_ICON_SIZE_STANDARD;
	case NEMO_ZOOM_LEVEL_LARGE:
		return NEMO_ICON_SIZE_LARGE;
	case NEMO_ZOOM_LEVEL_LARGER:
		return NEMO_ICON_SIZE_LARGER;
	case NEMO_ZOOM_LEVEL_LARGEST:
		return NEMO_ICON_SIZE_LARGEST;
    case NEMO_ZOOM_LEVEL_NULL:
    default:
        g_return_val_if_reached (NEMO_ICON_SIZE_STANDARD);
	}
}

guint
nemo_get_icon_text_width_for_zoom_level (NemoZoomLevel  zoom_level)
{
    switch (zoom_level) {
    case NEMO_ZOOM_LEVEL_SMALLEST:
        return NEMO_ICON_TEXT_WIDTH_SMALLEST;
    case NEMO_ZOOM_LEVEL_SMALLER:
        return NEMO_ICON_TEXT_WIDTH_SMALLER;
    case NEMO_ZOOM_LEVEL_SMALL:
        return NEMO_ICON_TEXT_WIDTH_SMALL;
    case NEMO_ZOOM_LEVEL_STANDARD:
        return NEMO_ICON_TEXT_WIDTH_STANDARD;
    case NEMO_ZOOM_LEVEL_LARGE:
        return NEMO_ICON_TEXT_WIDTH_LARGE;
    case NEMO_ZOOM_LEVEL_LARGER:
        return NEMO_ICON_TEXT_WIDTH_LARGER;
    case NEMO_ZOOM_LEVEL_LARGEST:
        return NEMO_ICON_TEXT_WIDTH_LARGEST;
    case NEMO_ZOOM_LEVEL_NULL:
    default:
        g_return_val_if_reached (NEMO_ICON_SIZE_STANDARD);
    }
}


guint
nemo_get_desktop_icon_size_for_zoom_level (NemoZoomLevel zoom_level)
{
    switch (zoom_level) {
        case NEMO_ZOOM_LEVEL_SMALL:
            return NEMO_DESKTOP_ICON_SIZE_SMALL;
        case NEMO_ZOOM_LEVEL_STANDARD:
            return NEMO_DESKTOP_ICON_SIZE_STANDARD;
        case NEMO_ZOOM_LEVEL_LARGE:
            return NEMO_DESKTOP_ICON_SIZE_LARGE;
        case NEMO_ZOOM_LEVEL_SMALLEST:
        case NEMO_ZOOM_LEVEL_SMALLER:
        case NEMO_ZOOM_LEVEL_LARGER:
        case NEMO_ZOOM_LEVEL_LARGEST:
        case NEMO_ZOOM_LEVEL_NULL:
        default:
            g_return_val_if_reached (NEMO_ICON_SIZE_STANDARD);
    }
}

guint
nemo_get_list_icon_size_for_zoom_level (NemoZoomLevel zoom_level)
{
    switch (zoom_level) {
    case NEMO_ZOOM_LEVEL_SMALLEST:
        return NEMO_LIST_ICON_SIZE_SMALLEST;
    case NEMO_ZOOM_LEVEL_SMALLER:
        return NEMO_LIST_ICON_SIZE_SMALLEST;
    case NEMO_ZOOM_LEVEL_SMALL:
        return NEMO_LIST_ICON_SIZE_SMALLER;
    case NEMO_ZOOM_LEVEL_STANDARD:
        return NEMO_LIST_ICON_SIZE_SMALL;
    case NEMO_ZOOM_LEVEL_LARGE:
        return NEMO_LIST_ICON_SIZE_STANDARD;
    case NEMO_ZOOM_LEVEL_LARGER:
        return NEMO_LIST_ICON_SIZE_LARGE;
    case NEMO_ZOOM_LEVEL_LARGEST:
        return NEMO_LIST_ICON_SIZE_LARGER;
    case NEMO_ZOOM_LEVEL_NULL:
    default:
        g_return_val_if_reached (NEMO_ICON_SIZE_STANDARD);
    }
}

gint
nemo_get_icon_size_for_stock_size (GtkIconSize size)
{
  gint w, h;

  if (gtk_icon_size_lookup (size, &w, &h)) {
    return MAX (w, h);
  }
  return NEMO_ICON_SIZE_STANDARD;
}


guint
nemo_icon_get_emblem_size_for_icon_size (guint size)
{
	if (size >= 96)
		return 48;
	if (size >= 64)
		return 32;
	if (size >= 48)
		return 24;
	if (size >= 24)
		return 16;
	if (size >= 16)
		return 12;

	return 0; /* no emblems for smaller sizes */
}

GIcon *
nemo_user_special_directory_get_gicon (GUserDirectory directory)
{

	#define ICON_CASE(x) \
		case G_USER_DIRECTORY_ ## x:\
			return g_themed_icon_new (NEMO_ICON_FOLDER_ ## x);

	switch (directory) {

		ICON_CASE (DESKTOP);
		ICON_CASE (DOCUMENTS);
		ICON_CASE (DOWNLOAD);
		ICON_CASE (MUSIC);
		ICON_CASE (PICTURES);
		ICON_CASE (PUBLIC_SHARE);
		ICON_CASE (TEMPLATES);
		ICON_CASE (VIDEOS);

    case G_USER_N_DIRECTORIES:
	default:
		return g_themed_icon_new ("folder");
	}

	#undef ICON_CASE
}

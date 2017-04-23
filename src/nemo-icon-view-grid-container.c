/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-icon-container.h - the container widget for file manager icons

   Copyright (C) 2002 Sun Microsystems, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Author: Michael Meeks <michael@ximian.com>
*/
#include <config.h>

#include "nemo-icon-view-grid-container.h"

#include <string.h>
#include <errno.h>
#include <math.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <eel/eel-glib-extensions.h>
#include "nemo-icon-private.h"

#define DEBUG_FLAG NEMO_DEBUG_DESKTOP
#include <libnemo-private/nemo-debug.h>

#include <libnemo-private/nemo-desktop-utils.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-file-attributes.h>
#include <libnemo-private/nemo-metadata.h>
#include <libnemo-private/nemo-thumbnails.h>
#include <libnemo-private/nemo-desktop-icon-file.h>

#define ICON_TEXT_ATTRIBUTES_NUM_ITEMS		3
#define ICON_TEXT_ATTRIBUTES_DEFAULT_TOKENS	"size,date_modified,type"

static void update_layout_constants (NemoIconContainer *container);

G_DEFINE_TYPE (NemoIconViewGridContainer, nemo_icon_view_grid_container, NEMO_TYPE_ICON_CONTAINER);

static GQuark attribute_none_q;

static NemoIconView *
get_icon_view (NemoIconContainer *container)
{
	/* Type unsafe comparison for performance */
	return ((NemoIconViewGridContainer *)container)->view;
}


static NemoIconInfo *
nemo_icon_view_grid_container_get_icon_images (NemoIconContainer *container,
					      NemoIconData      *data,
					      int                    size,
					      char                 **embedded_text,
					      gboolean               for_drag_accept,
					      gboolean               need_large_embeddded_text,
					      gboolean              *embedded_text_needs_loading,
					      gboolean              *has_window_open)
{
	NemoIconView *icon_view;
	char **emblems_to_ignore;
	NemoFile *file;
	NemoFileIconFlags flags;
	NemoIconInfo *icon_info;
	GdkPixbuf *pixbuf;
	GIcon *emblemed_icon;
	GEmblem *emblem;
	GList *emblem_icons, *l;
    gint scale;

	file = (NemoFile *) data;

	g_assert (NEMO_IS_FILE (file));
	icon_view = get_icon_view (container);
	g_return_val_if_fail (icon_view != NULL, NULL);
	
	*has_window_open = nemo_file_has_open_window (file);

	flags = NEMO_FILE_ICON_FLAGS_USE_MOUNT_ICON_AS_EMBLEM |
			NEMO_FILE_ICON_FLAGS_USE_THUMBNAILS;

	if (for_drag_accept) {
		flags |= NEMO_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT;
	}

	emblems_to_ignore = nemo_view_get_emblem_names_to_exclude 
		(NEMO_VIEW (icon_view));
	emblem_icons = nemo_file_get_emblem_icons (file,
						       emblems_to_ignore);
	g_strfreev (emblems_to_ignore);

    scale = gtk_widget_get_scale_factor (GTK_WIDGET (icon_view));
	icon_info = nemo_file_get_icon (file, size, scale, flags);

	/* apply emblems */
	if (emblem_icons != NULL) {
        gint w, h, s;
        gboolean bad_ratio;

        l = emblem_icons;

		pixbuf = nemo_icon_info_get_pixbuf (icon_info);

        w = gdk_pixbuf_get_width (pixbuf);
        h = gdk_pixbuf_get_height (pixbuf);

        s = MAX (w, h);
        if (s < size)
            size = s;

        bad_ratio = nemo_icon_get_emblem_size_for_icon_size (size) * scale > w ||
                    nemo_icon_get_emblem_size_for_icon_size (size) * scale > h;

        if (bad_ratio)
            goto skip_emblem; /* Would prefer to not use goto, but
                               * I don't want to do these checks on
                               * non-emblemed icons (the majority)
                               * as it would be too costly */

        emblem = g_emblem_new (l->data);

		emblemed_icon = g_emblemed_icon_new (G_ICON (pixbuf), emblem);
		g_object_unref (emblem);

		for (l = l->next; l != NULL; l = l->next) {
			emblem = g_emblem_new (l->data);
			g_emblemed_icon_add_emblem (G_EMBLEMED_ICON (emblemed_icon),
						    emblem);
			g_object_unref (emblem);
		}

		g_clear_object (&icon_info);
		icon_info = nemo_icon_info_lookup (emblemed_icon, size, scale);
        g_object_unref (emblemed_icon);

skip_emblem:
		g_object_unref (pixbuf);

	}

	if (emblem_icons != NULL) {
		g_list_free_full (emblem_icons, g_object_unref);
	}

	return icon_info;
}

static char *
nemo_icon_view_grid_container_get_icon_description (NemoIconContainer *container,
						   NemoIconData      *data)
{
	NemoFile *file;
	char *mime_type;
	const char *description;

	file = NEMO_FILE (data);
	g_assert (NEMO_IS_FILE (file));

	if (NEMO_IS_DESKTOP_ICON_FILE (file)) {
		return NULL;
	}

	mime_type = nemo_file_get_mime_type (file);
	description = g_content_type_get_description (mime_type);
	g_free (mime_type);
	return g_strdup (description);
}

static void
nemo_icon_view_grid_container_start_monitor_top_left (NemoIconContainer *container,
						     NemoIconData      *data,
						     gconstpointer          client,
						     gboolean               large_text)
{
	NemoFile *file;
	NemoFileAttributes attributes;
		
	file = (NemoFile *) data;

	g_assert (NEMO_IS_FILE (file));

	attributes = NEMO_FILE_ATTRIBUTE_TOP_LEFT_TEXT;
	if (large_text) {
		attributes |= NEMO_FILE_ATTRIBUTE_LARGE_TOP_LEFT_TEXT;
	}
	nemo_file_monitor_add (file, client, attributes);
}

static void
nemo_icon_view_grid_container_stop_monitor_top_left (NemoIconContainer *container,
						    NemoIconData      *data,
						    gconstpointer          client)
{
	NemoFile *file;

	file = (NemoFile *) data;

	g_assert (NEMO_IS_FILE (file));

	nemo_file_monitor_remove (file, client);
}

static void
nemo_icon_view_grid_container_prioritize_thumbnailing (NemoIconContainer *container,
						      NemoIconData      *data)
{
	NemoFile *file;
	char *uri;

	file = (NemoFile *) data;

	g_assert (NEMO_IS_FILE (file));

	if (nemo_file_is_thumbnailing (file)) {
		uri = nemo_file_get_uri (file);
		nemo_thumbnail_prioritize (uri);
		g_free (uri);
	}
}

static void
update_auto_strv_as_quarks (GSettings   *settings,
			    const gchar *key,
			    gpointer     user_data)
{
	GQuark **storage = user_data;
	int i = 0;
	char **value;

	value = g_settings_get_strv (settings, key);

	g_free (*storage);
	*storage = g_new (GQuark, g_strv_length (value) + 1);

	for (i = 0; value[i] != NULL; ++i) {
		(*storage)[i] = g_quark_from_string (value[i]);
	}
	(*storage)[i] = 0;

	g_strfreev (value);
}

/*
 * Get the preference for which caption text should appear
 * beneath icons.
 */
static GQuark *
nemo_icon_view_grid_container_get_icon_text_attributes_from_preferences (void)
{
	static GQuark *attributes = NULL;

	if (attributes == NULL) {
		update_auto_strv_as_quarks (nemo_icon_view_preferences, 
					    NEMO_PREFERENCES_ICON_VIEW_CAPTIONS,
					    &attributes);
		g_signal_connect (nemo_icon_view_preferences, 
				  "changed::" NEMO_PREFERENCES_ICON_VIEW_CAPTIONS,
				  G_CALLBACK (update_auto_strv_as_quarks),
				  &attributes);
	}

	/* We don't need to sanity check the attributes list even though it came
	 * from preferences.
	 *
	 * There are 2 ways that the values in the list could be bad.
	 *
	 * 1) The user picks "bad" values.  "bad" values are those that result in
	 *    there being duplicate attributes in the list.
	 *
	 * 2) Value stored in GConf are tampered with.  Its possible physically do
	 *    this by pulling the rug underneath GConf and manually editing its
	 *    config files.  Its also possible to use a third party GConf key
	 *    editor and store garbage for the keys in question.
	 *
	 * Thankfully, the Nemo preferences machinery deals with both of
	 * these cases.
	 *
	 * In the first case, the preferences dialog widgetry prevents
	 * duplicate attributes by making "bad" choices insensitive.
	 *
	 * In the second case, the preferences getter (and also the auto storage) for
	 * string_array values are always valid members of the enumeration associated
	 * with the preference.
	 *
	 * So, no more error checking on attributes is needed here and we can return
	 * a the auto stored value.
	 */
	return attributes;
}

static int
quarkv_length (GQuark *attributes)
{
	int i;
	i = 0;
	while (attributes[i] != 0) {
		i++;
	}
	return i;
}

/**
 * nemo_icon_view_get_icon_text_attribute_names:
 *
 * Get a list representing which text attributes should be displayed
 * beneath an icon. The result is dependent on zoom level and possibly
 * user configuration. Don't free the result.
 * @view: NemoIconView to query.
 * 
 **/
static GQuark *
nemo_icon_view_grid_container_get_icon_text_attribute_names (NemoIconContainer *container,
							    int *len)
{
	GQuark *attributes;
	int piece_count;

	const int pieces_by_level[] = {
		0,	/* NEMO_ZOOM_LEVEL_SMALLEST */
		0,	/* NEMO_ZOOM_LEVEL_SMALLER */
		0,	/* NEMO_ZOOM_LEVEL_SMALL */
		1,	/* NEMO_ZOOM_LEVEL_STANDARD */
		2,	/* NEMO_ZOOM_LEVEL_LARGE */
		2,	/* NEMO_ZOOM_LEVEL_LARGER */
		3	/* NEMO_ZOOM_LEVEL_LARGEST */
	};

	piece_count = pieces_by_level[nemo_icon_container_get_zoom_level (container)];

	attributes = nemo_icon_view_grid_container_get_icon_text_attributes_from_preferences ();

	*len = MIN (piece_count, quarkv_length (attributes));

	return attributes;
}

/* This callback returns the text, both the editable part, and the
 * part below that is not editable.
 */
static void
nemo_icon_view_grid_container_get_icon_text (NemoIconContainer *container,
					    NemoIconData      *data,
					    char                 **editable_text,
					    char                 **additional_text,
					    gboolean               include_invisible)
{
	GQuark *attributes;
	char *text_array[4];
	int i, j, num_attributes;
	NemoIconView *icon_view;
	NemoFile *file;
	gboolean use_additional;

	file = NEMO_FILE (data);

	g_assert (NEMO_IS_FILE (file));
	g_assert (editable_text != NULL);
	icon_view = get_icon_view (container);
	g_return_if_fail (icon_view != NULL);

	use_additional = (additional_text != NULL);

	/* In the smallest zoom mode, no text is drawn. */
	if (nemo_icon_container_get_zoom_level (container) == NEMO_ZOOM_LEVEL_SMALLEST &&
            !include_invisible) {
		*editable_text = NULL;
	} else {
		/* Strip the suffix for nemo object xml files. */
		*editable_text = nemo_file_get_display_name (file);
	}

	if (!use_additional) {
		return;
	}

	if (nemo_icon_view_is_compact (icon_view)) {
		*additional_text = NULL;
		return;
	}

	if (NEMO_IS_DESKTOP_ICON_FILE (file) ||
	    nemo_file_is_nemo_link (file)) {
		/* Don't show the normal extra information for desktop icons,
		 * or desktop files, it doesn't make sense. */
 		*additional_text = NULL;
		return;
	}

	/* Find out what attributes go below each icon. */
	attributes = nemo_icon_view_grid_container_get_icon_text_attribute_names (container,
									   &num_attributes);

	/* Get the attributes. */
	j = 0;
	for (i = 0; i < num_attributes; ++i) {
		if (attributes[i] == attribute_none_q) {
			continue;
		}

		text_array[j++] =
			nemo_file_get_string_attribute_with_default_q (file, attributes[i]);
	}
	text_array[j] = NULL;

	/* Return them. */
	if (j == 0) {
		*additional_text = NULL;
	} else if (j == 1) {
		/* Only one item, avoid the strdup + free */
		*additional_text = text_array[0];
	} else {
		*additional_text = g_strjoinv ("\n", text_array);
		
		for (i = 0; i < j; i++) {
			g_free (text_array[i]);
		}
	}
}

/* Sort as follows:
 *   0) computer link
 *   1) home link
 *   2) network link
 *   3) mount links
 *   4) other
 *   5) trash link
 */
typedef enum {
	SORT_COMPUTER_LINK,
	SORT_HOME_LINK,
	SORT_NETWORK_LINK,
	SORT_MOUNT_LINK,
	SORT_OTHER,
	SORT_TRASH_LINK
} SortCategory;

static SortCategory
get_sort_category (NemoFile *file)
{
	NemoDesktopLink *link;
	SortCategory category;

	category = SORT_OTHER;
	
	if (NEMO_IS_DESKTOP_ICON_FILE (file)) {
		link = nemo_desktop_icon_file_get_link (NEMO_DESKTOP_ICON_FILE (file));
		if (link != NULL) {
			switch (nemo_desktop_link_get_link_type (link)) {
			case NEMO_DESKTOP_LINK_COMPUTER:
				category = SORT_COMPUTER_LINK;
				break;
			case NEMO_DESKTOP_LINK_HOME:
				category = SORT_HOME_LINK;
				break;
			case NEMO_DESKTOP_LINK_MOUNT:
				category = SORT_MOUNT_LINK;
				break;
			case NEMO_DESKTOP_LINK_TRASH:
				category = SORT_TRASH_LINK;
				break;
			case NEMO_DESKTOP_LINK_NETWORK:
				category = SORT_NETWORK_LINK;
				break;
			default:
				category = SORT_OTHER;
				break;
			}
			g_object_unref (link);
		}
	} 
	
	return category;
}

static int
nemo_icon_view_grid_container_compare_icons (NemoIconContainer *container,
                                             NemoIconData      *data_a,
                                             NemoIconData      *data_b)
{
    NemoFile *file_a;
    NemoFile *file_b;
    NemoIconView *icon_view;
    SortCategory category_a, category_b;

    file_a = (NemoFile *) data_a;
    file_b = (NemoFile *) data_b;

    icon_view = NEMO_ICON_VIEW (NEMO_ICON_VIEW_GRID_CONTAINER (container)->view);
    g_return_val_if_fail (icon_view != NULL, 0);

    category_a = get_sort_category (file_a);
    category_b = get_sort_category (file_b);

    if (category_a == category_b) {
        return nemo_icon_view_compare_files (icon_view,
                                             file_a,
                                             file_b);
    }

    if (category_a < category_b) {
        return -1;
    } else {
        return +1;
    }
}

static void
nemo_icon_view_grid_container_freeze_updates (NemoIconContainer *container)
{
	NemoIconView *icon_view;
	icon_view = get_icon_view (container);
	g_return_if_fail (icon_view != NULL);
	nemo_view_freeze_updates (NEMO_VIEW (icon_view));
}

static void
nemo_icon_view_grid_container_unfreeze_updates (NemoIconContainer *container)
{
	NemoIconView *icon_view;
	icon_view = get_icon_view (container);
	g_return_if_fail (icon_view != NULL);
	nemo_view_unfreeze_updates (NEMO_VIEW (icon_view));
}

inline static void
nemo_icon_view_grid_container_icon_get_bounding_box (NemoIcon *icon,
               int *x1_return, int *y1_return,
               int *x2_return, int *y2_return,
               NemoIconCanvasItemBoundsUsage usage)
{
    double x1, y1, x2, y2;

    if (usage == BOUNDS_USAGE_FOR_DISPLAY) {
        eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (icon->item),
                        &x1, &y1, &x2, &y2);
    } else if (usage == BOUNDS_USAGE_FOR_LAYOUT) {
        nemo_icon_canvas_item_get_bounds_for_layout (icon->item,
                                 &x1, &y1, &x2, &y2);
    } else if (usage == BOUNDS_USAGE_FOR_ENTIRE_ITEM) {
        nemo_icon_canvas_item_get_bounds_for_entire_item (icon->item,
                                      &x1, &y1, &x2, &y2);
    } else {
        g_assert_not_reached ();
    }

    if (x1_return != NULL) {
        *x1_return = x1;
    }

    if (y1_return != NULL) {
        *y1_return = y1;
    }

    if (x2_return != NULL) {
        *x2_return = x2;
    }

    if (y2_return != NULL) {
        *y2_return = y2;
    }
}

static gboolean
get_stored_icon_position (NemoIconContainer *container,
                          NemoIconData      *data,
                          NemoIconPosition  *position)
{
    NemoFile *file;
    GdkPoint *point;

    g_assert (NEMO_IS_ICON_CONTAINER (container));
    g_assert (NEMO_IS_FILE (data));
    g_assert (position != NULL);

    file = NEMO_FILE (data);

    if (nemo_file_get_is_desktop_orphan (file)) {
        return FALSE;
    }

    point = nemo_file_get_position (file);
    position->x = point->x;
    position->y = point->y;
    position->scale = 1.0;

    return position->x > ICON_UNPOSITIONED_VALUE;
}

static void
store_new_icon_position (NemoIconContainer *container,
                         NemoIcon          *icon,
                         NemoIconPosition   position)
{
    gboolean dummy;
    time_t now;

    now = time (NULL);

    g_signal_emit_by_name (container, "icon_position_changed",
                           icon->data, &position);
    g_signal_emit_by_name (container, "store_layout_timestamp",
                           icon->data, &now, &dummy);
}

static void
snap_position (NemoIconContainer         *container,
               NemoCenteredPlacementGrid *grid,
               NemoIcon                  *icon,
               gint                      *x,
               gint                      *y)
{
    gint x_nominal, y_nominal, x_new, y_new;

    if (nemo_icon_container_is_layout_rtl (container)) {
        *x = nemo_icon_container_get_mirror_x_position (container, icon, *x);
    }

    nemo_centered_placement_grid_icon_position_to_nominal (grid,
                                                           *x, *y,
                                                           &x_nominal, &y_nominal);

    nemo_centered_placement_grid_find_empty_position (grid, icon,
                                                      *x, *y,
                                                      &x_new, &y_new);

    nemo_centered_placement_grid_nominal_to_icon_position (grid,
                                                           x_new, y_new,
                                                           x, y);

    if (nemo_icon_container_is_layout_rtl (container)) {
        *x = nemo_icon_container_get_mirror_x_position (container, icon, *x);
    }
}

static void
lay_down_icons_vertical_desktop (NemoIconContainer *container, GList *icons)
{
    GList *p, *placed_icons, *unplaced_icons;
    int total, new_length, placed;
    NemoIcon *icon;
    int x, y;
    gint current_monitor;

    current_monitor = nemo_desktop_utils_get_monitor_for_widget (GTK_WIDGET (container));

    /* Determine which icons have and have not been placed */
    placed_icons = NULL;
    unplaced_icons = NULL;

    total = g_list_length (container->details->icons);
    new_length = g_list_length (icons);

    placed = total - new_length;

    if (placed > 0) {
        NemoCenteredPlacementGrid *grid;
        /* Add only placed icons in list */
        for (p = container->details->icons; p != NULL; p = p->next) {
            icon = p->data;
            if (nemo_icon_container_icon_is_positioned (icon)) {
                nemo_icon_container_icon_set_position(container, icon, icon->saved_ltr_x, icon->y);
                placed_icons = g_list_prepend (placed_icons, icon);
            } else {
                icon->x = 0;
                icon->y = 0;
                unplaced_icons = g_list_prepend (unplaced_icons, icon);
            }
        }
        placed_icons = g_list_reverse (placed_icons);
        unplaced_icons = g_list_reverse (unplaced_icons);

        grid = nemo_centered_placement_grid_new (container, container->details->horizontal);

        if (grid) {
            nemo_centered_placement_grid_pre_populate (grid, placed_icons);

            /* Place unplaced icons in the best locations */
            for (p = unplaced_icons; p != NULL; p = p->next) {
                icon = p->data;

                snap_position (container, grid, icon, &x, &y);
                nemo_icon_container_icon_set_position (container, icon, x, y);
                icon->saved_ltr_x = x;
                nemo_centered_placement_grid_mark_icon (grid, icon);
            }

            nemo_centered_placement_grid_free (grid);
        }

        g_list_free (placed_icons);
        g_list_free (unplaced_icons);
    } else {
        NemoCenteredPlacementGrid *grid;
        /* There are no placed icons, or we have auto layout enabled */

        grid = nemo_centered_placement_grid_new (container, container->details->horizontal);

        while (icons != NULL) {
            NemoIconPosition position;

            icon = icons->data;

            nemo_centered_placement_grid_get_next_free_position (grid, &x, &y);

            nemo_icon_container_icon_set_position (container,
                                                   icon,
                                                   x, y);

            icon->saved_ltr_x = icon->x;

            icons = icons->next;

            position.x = icon->x;
            position.y = icon->y;
            position.scale = 1.0;
            position.monitor = current_monitor;
            store_new_icon_position (container, icon, position);
        }
    }

    if (!container->details->stored_auto_layout) {
        nemo_icon_container_freeze_icon_positions (container);
    }
}

static void
nemo_icon_view_grid_container_lay_down_icons (NemoIconContainer *container, GList *icons, double start_y)
{

    lay_down_icons_vertical_desktop (container, icons);
}

static int
compare_icons_by_position_vertical (gconstpointer a, gconstpointer b)
{
    NemoIcon *icon_a, *icon_b;
    int x1, y1, x2, y2;
    int center_a;
    int center_b;

    icon_a = (NemoIcon*)a;
    icon_b = (NemoIcon*)b;

    nemo_icon_view_grid_container_icon_get_bounding_box (icon_a, &x1, &y1, &x2, &y2,
                   BOUNDS_USAGE_FOR_DISPLAY);
    center_a = x1 + (x2 - x1) / 2;
    nemo_icon_view_grid_container_icon_get_bounding_box (icon_b, &x1, &y1, &x2, &y2,
                   BOUNDS_USAGE_FOR_DISPLAY);
    center_b = x1 + (x2 - x1) / 2;

    return center_a == center_b ?
        icon_a->y - icon_b->y :
        center_a - center_b;
}

static int
compare_icons_by_position_horizontal (gconstpointer a, gconstpointer b)
{
    NemoIcon *icon_a, *icon_b;
    int x1, y1, x2, y2;
    int center_a;
    int center_b;

    icon_a = (NemoIcon*)a;
    icon_b = (NemoIcon*)b;

    nemo_icon_view_grid_container_icon_get_bounding_box (icon_a, &x1, &y1, &x2, &y2,
                   BOUNDS_USAGE_FOR_DISPLAY);
    center_a = y1 + (y2 - y1) / 2;
    nemo_icon_view_grid_container_icon_get_bounding_box (icon_b, &x1, &y1, &x2, &y2,
                   BOUNDS_USAGE_FOR_DISPLAY);
    center_b = y1 + (y2 - y1) / 2;

    return center_a == center_b ?
        icon_a->x - icon_b->x :
        center_a - center_b;
}

/* x, y are the top-left coordinates of the icon. */
static void
nemo_icon_view_grid_container_icon_set_position (NemoIconContainer *container,
                                            NemoIcon          *icon,
                                            double             x,
                                            double             y)
{
    double pixels_per_unit; 
    int container_left, container_top, container_right, container_bottom;
    int x1, x2, y1, y2;
    int container_x, container_y, container_width, container_height;
    EelDRect icon_bounds;
    GtkAllocation alloc;
    int item_width, item_height;
    int height_above, width_left;
    int min_x, max_x, min_y, max_y;

    if (icon->x == x && icon->y == y) {
        return;
    }

    if (icon == nemo_icon_container_get_icon_being_renamed (container)) {
        nemo_icon_container_end_renaming_mode (container, TRUE);
    }

    gtk_widget_get_allocation (GTK_WIDGET (container), &alloc);
    container_x = alloc.x;
    container_y = alloc.y;
    container_width = alloc.width - container->details->left_margin - container->details->right_margin;
    container_height = alloc.height - container->details->top_margin - container->details->bottom_margin;
    pixels_per_unit = EEL_CANVAS (container)->pixels_per_unit;
    /* Clip the position of the icon within our desktop bounds */
    container_left = container_x / pixels_per_unit;
    container_top =  container_y / pixels_per_unit;
    container_right = container_left + container_width / pixels_per_unit;
    container_bottom = container_top + container_height / pixels_per_unit;

    nemo_icon_container_icon_get_bounding_box (container, icon, &x1, &y1, &x2, &y2,
                   BOUNDS_USAGE_FOR_ENTIRE_ITEM);
    item_width = x2 - x1;
    item_height = y2 - y1;

    icon_bounds = nemo_icon_canvas_item_get_icon_rectangle (icon->item);

    /* determine icon rectangle relative to item rectangle */
    height_above = icon_bounds.y0 - y1;
    width_left = icon_bounds.x0 - x1;

    min_x = container_left + width_left;
    max_x = container_right - item_width + width_left;
    x = CLAMP (x, min_x, max_x);

    min_y = container_top + height_above;
    max_y = container_bottom - item_height + height_above;
    y = CLAMP (y, min_y, max_y);

    if (icon->x == ICON_UNPOSITIONED_VALUE) {
        icon->x = 0;
    }
    if (icon->y == ICON_UNPOSITIONED_VALUE) {
        icon->y = 0;
    }
    
    eel_canvas_item_move (EEL_CANVAS_ITEM (icon->item),
                x - icon->x,
                y - icon->y);

    icon->x = x;
    icon->y = y;
}

static void
nemo_icon_view_grid_container_move_icon (NemoIconContainer *container,
                   NemoIcon *icon,
                   int x, int y,
                   double scale,
                   gboolean raise,
                   gboolean snap,
                   gboolean update_position)
{
    NemoIconContainerDetails *details;
    gboolean emit_signal;
    NemoIconPosition position;

    details = container->details;

    emit_signal = FALSE;
    
    if (icon == nemo_icon_container_get_icon_being_renamed (container)) {
        nemo_icon_container_end_renaming_mode (container, TRUE);
    }

    if (scale != icon->scale) {
        icon->scale = scale;

        nemo_icon_container_update_icon (container, icon);

        if (update_position) {
            nemo_icon_container_redo_layout (container); 
            emit_signal = TRUE;
        }
    }

    if (!details->auto_layout) {
        if (x != icon->x || y != icon->y) {
            nemo_icon_container_icon_set_position (container, icon, x, y);
            emit_signal = update_position;
        }

        NEMO_ICON_VIEW_GRID_CONTAINER (container)->manual_sort_dirty = TRUE;
        nemo_icon_view_set_sort_reversed (get_icon_view (container), FALSE, TRUE);

        icon->saved_ltr_x = nemo_icon_container_is_layout_rtl (container) ? nemo_icon_container_get_mirror_x_position (container, icon, icon->x) : icon->x;
    }

    if (emit_signal) {
        position.x = icon->saved_ltr_x;
        position.y = icon->y;
        position.scale = scale;
        position.monitor = nemo_desktop_utils_get_monitor_for_widget (GTK_WIDGET (container));
        g_signal_emit_by_name (container, "icon_position_changed", icon->data, &position);
    }
    
    if (raise) {
        nemo_icon_container_icon_raise (container, icon);
    }

    if (!container->details->auto_layout) {
        container->details->icons = g_list_sort (container->details->icons,
                                                 container->details->horizontal ? compare_icons_by_position_horizontal :
                                                                                  compare_icons_by_position_vertical);
    }
}

static void
nemo_icon_view_grid_container_align_icons (NemoIconContainer *container)
{
    NemoCenteredPlacementGrid *grid;
    GList *unplaced_icons;
    GList *l;

    unplaced_icons = g_list_copy (container->details->icons);

    unplaced_icons = g_list_sort (unplaced_icons,
                                  container->details->horizontal ? compare_icons_by_position_horizontal :
                                                                   compare_icons_by_position_vertical);

    if (nemo_icon_container_is_layout_rtl (container)) {
        unplaced_icons = g_list_reverse (unplaced_icons);
    }

    grid = nemo_centered_placement_grid_new (container, container->details->horizontal);

    for (l = unplaced_icons; l != NULL; l = l->next) {
        NemoIcon *icon;
        int x, y;

        icon = l->data;
        x = icon->saved_ltr_x;
        y = icon->y;

        snap_position (container, grid, icon, &x, &y);

        nemo_centered_placement_grid_mark_icon (grid, icon);
        nemo_icon_container_icon_set_position (container, icon, x, y);

        icon->saved_ltr_x = icon->x;
    }

    nemo_centered_placement_grid_free (grid);

    g_list_free (unplaced_icons);

    if (nemo_icon_container_is_layout_rtl (container)) {
        nemo_icon_container_set_rtl_positions (container);
    }
}

static void
nemo_icon_view_grid_container_reload_icon_positions (NemoIconContainer *container)
{
    GList *p, *no_position_icons;
    NemoIcon *icon;
    gboolean have_stored_position;
    NemoIconPosition position;
    EelDRect bounds;
    double bottom;
    EelCanvasItem *item;

    g_assert (!container->details->auto_layout);

    nemo_icon_container_resort (container);

    no_position_icons = NULL;

    /* Place all the icons with positions. */
    bottom = 0;
    for (p = container->details->icons; p != NULL; p = p->next) {
        icon = p->data;

        have_stored_position = get_stored_icon_position (container,
                                                         icon->data,
                                                         &position);

        if (have_stored_position) {
            nemo_icon_container_icon_set_position (container, icon, position.x, position.y);
            item = EEL_CANVAS_ITEM (icon->item);
            nemo_icon_canvas_item_get_bounds_for_layout (icon->item,
                                     &bounds.x0,
                                     &bounds.y0,
                                     &bounds.x1,
                                     &bounds.y1);
            eel_canvas_item_i2w (item->parent,
                         &bounds.x0,
                         &bounds.y0);
            eel_canvas_item_i2w (item->parent,
                         &bounds.x1,
                         &bounds.y1);
            if (bounds.y1 > bottom) {
                bottom = bounds.y1;
            }
        } else {
            no_position_icons = g_list_prepend (no_position_icons, icon);
        }
    }
    no_position_icons = g_list_reverse (no_position_icons);

    /* Place all the other icons. */
    NEMO_ICON_CONTAINER_GET_CLASS (container)->lay_down_icons (container, no_position_icons, bottom + GET_VIEW_CONSTANT (container, icon_pad_bottom));
    g_list_free (no_position_icons);
}

static void
nemo_icon_view_grid_container_finish_adding_new_icons (NemoIconContainer *container)
{
    GList *p, *new_icons, *no_position_icons;
    NemoIcon *icon;

    update_layout_constants (container);

    new_icons = container->details->new_icons;

    container->details->new_icons = NULL;

    /* Position most icons (not unpositioned manual-layout icons). */
    new_icons = g_list_reverse (new_icons);
    no_position_icons = NULL;

    for (p = new_icons; p != NULL; p = p->next) {
        gboolean have_stored_position;
        NemoIconPosition position;

        icon = p->data;

        nemo_icon_container_update_icon (container, icon);

        /* Get the stored position. */
        have_stored_position = FALSE;
        position.scale = 1.0;
        position.monitor = nemo_desktop_utils_get_monitor_for_widget (GTK_WIDGET (container));

        have_stored_position = get_stored_icon_position (container,
                                                         icon->data,
                                                         &position);

        icon->scale = position.scale;

        if (!container->details->auto_layout) {
            if (have_stored_position) {
                nemo_icon_container_icon_set_position (container, icon, position.x, position.y);
                icon->saved_ltr_x = icon->x;
            } else {
                no_position_icons = g_list_prepend (no_position_icons, icon);
            }
        }

        nemo_icon_container_finish_adding_icon (container, icon);
    }

    g_list_free (new_icons);

    /* Position the unpositioned manual layout icons. */
    if (no_position_icons != NULL) {
        g_assert (!container->details->auto_layout);

        nemo_icon_container_sort_icons (container, &no_position_icons);

        NEMO_ICON_CONTAINER_GET_CLASS (container)->lay_down_icons (container, no_position_icons, GET_VIEW_CONSTANT (container, container_pad_top));

        g_list_free (no_position_icons);
    }

    if (container->details->store_layout_timestamps_when_finishing_new_icons) {
        nemo_icon_container_store_layout_timestamps_now (container);
        container->details->store_layout_timestamps_when_finishing_new_icons = FALSE;
    }
}

static gboolean
should_place_before (gint          x,
                     gint          y,
                     GdkRectangle *rect,
                     gboolean      horizontal)
{
    gint half_x, half_y;

    half_x = rect->x + (rect->width / 2);
    half_y = rect->y + (rect->height / 2);

    if (horizontal) {
        return x < half_x;
    } else {
        return y < half_y;
    }

    return FALSE;
}

static void
draw_insert_stroke (cairo_t *cr,
                    gint     draw_x,
                    gint     draw_y,
                    gint     draw_distance_x,
                    gint     draw_distance_y)
{
    GdkRGBA shadow_rgba = { 0, 0, 0, 1.0 };
    GdkRGBA fore_rgba = { 1.0, 1.0, 1.0, 1.0};

    cairo_set_antialias (cr, CAIRO_ANTIALIAS_BEST);

    cairo_save (cr);

    cairo_set_line_width (cr, 1.0);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

    gdk_cairo_set_source_rgba (cr, &shadow_rgba);

    cairo_move_to (cr, draw_x, draw_y);
    cairo_line_to (cr, draw_x + draw_distance_x, draw_y + draw_distance_y);
    cairo_stroke (cr);

    cairo_restore (cr);

    draw_x--;
    draw_y--;

    cairo_save (cr);

    cairo_set_line_width (cr, 1.0);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

    gdk_cairo_set_source_rgba (cr, &fore_rgba);

    cairo_move_to (cr, draw_x, draw_y);
    cairo_line_to (cr, draw_x + draw_distance_x, draw_y + draw_distance_y);
    cairo_stroke (cr);

    cairo_restore (cr);
}

static void
nemo_icon_view_grid_container_draw_background (EelCanvas *canvas,
                                               cairo_t   *cr)
{
    NemoIconContainer *container;
    NemoIcon *target_icon;
    gboolean before;

    container = NEMO_ICON_CONTAINER (canvas);

    if (DEBUGGING) {
        NemoCenteredPlacementGrid *grid;
        GdkRectangle grid_rect;
        gint next_x, next_y;
        gint count;
        GdkRGBA grid_color = { 1.0, 1.0, 1.0, 0.4};

        grid = nemo_centered_placement_grid_new (container, container->details->horizontal);

        count = 0;

        cairo_save (cr);
        gdk_cairo_set_source_rgba (cr, &grid_color);
        cairo_set_line_width (cr, 1.0);
        while (count < grid->num_rows * grid->num_columns) {
            nemo_centered_placement_grid_get_next_free_position (grid, &next_x, &next_y);
            nemo_centered_placement_grid_get_current_position_rect (grid, next_x, next_y, &grid_rect, NULL);

            grid_rect.x += container->details->left_margin;
            grid_rect.y += container->details->top_margin;

            gdk_cairo_rectangle (cr, &grid_rect);

            count++;
        }

        cairo_stroke (cr);
        cairo_restore (cr);
    }

    if (container->details->dnd_grid == NULL) {
        return;
    }

    target_icon = container->details->drop_target;

    if (target_icon == NULL) {
        NemoCenteredPlacementGrid *grid;
        GdkRectangle grid_rect;
        gint grid_cs_x, grid_cs_y;
        gint draw_x, draw_y, draw_distance_x, draw_distance_y;
        gboolean is_free;

        grid = container->details->dnd_grid;

        /* Canvas draw vfunc encompasses the margins.  The placement grid coord system
         * does not - grid x and y are relative to the inside corner of the top and left canvas
         * margins.  So we have to shift and then unshift by the margins here.  The grid functions
         * will handle any grid padding calculations (grid->borders). */

        grid_cs_x = container->details->current_dnd_x - container->details->left_margin;
        grid_cs_y = container->details->current_dnd_y - container->details->top_margin;

        nemo_centered_placement_grid_get_current_position_rect (grid,
                                                                grid_cs_x,
                                                                grid_cs_y,
                                                                &grid_rect,
                                                                &is_free);

        grid_rect.x += container->details->left_margin;
        grid_rect.y += container->details->top_margin;

        before = should_place_before (container->details->current_dnd_x,
                                      container->details->current_dnd_y,
                                      &grid_rect,
                                      container->details->horizontal);

        if (container->details->horizontal) {
            if (is_free || before) {
                draw_x = grid_rect.x;
                draw_y = grid_rect.y + (grid_rect.height * .2);
                draw_distance_x = 0;
                draw_distance_y = grid_rect.height * .6;

                draw_insert_stroke (cr, draw_x, draw_y, draw_distance_x, draw_distance_y);
            }

            if (is_free || !before) {
                draw_x = grid_rect.x + grid_rect.width;
                draw_y = grid_rect.y + (grid_rect.height * .2);
                draw_distance_x = 0;
                draw_distance_y = grid_rect.height * .6;

                draw_insert_stroke (cr, draw_x, draw_y, draw_distance_x, draw_distance_y);
            }
        } else {
            if (is_free || before) {
                draw_x = grid_rect.x + (grid_rect.width * .2);
                draw_y = grid_rect.y;
                draw_distance_x = grid_rect.width * .6;
                draw_distance_y = 0;

                draw_insert_stroke (cr, draw_x, draw_y, draw_distance_x, draw_distance_y);
            }

            if (is_free || !before) {
                draw_x = grid_rect.x + (grid_rect.width * .2);
                draw_y = grid_rect.y + grid_rect.height;
                draw_distance_x = grid_rect.width * .6;
                draw_distance_y = 0;

                draw_insert_stroke (cr, draw_x, draw_y, draw_distance_x, draw_distance_y);
            }
        }
    }
}

#define BASE_SNAP_SIZE_X 130.0
#define BASE_SNAP_SIZE_Y 100.0
#define BASE_MAX_TEXT_WIDTH 120.0

/* from nemo-icon-canvas-item.c */
#define LABEL_OFFSET 1

static gint
get_vertical_adjustment (NemoIconContainer *container,
                         gint               icon_size)
{
    PangoLayout *layout;
    PangoFontDescription *desc;
    gint ellipses_limit;
    gint height;

    ellipses_limit = g_settings_get_int (nemo_desktop_preferences, NEMO_PREFERENCES_DESKTOP_TEXT_ELLIPSIS_LIMIT);

    layout = gtk_widget_create_pango_layout (GTK_WIDGET (container), "Test");

    desc = pango_font_description_from_string (container->details->font);
    pango_font_description_set_size (desc,
                                     pango_font_description_get_size (desc) +
                                     container->details->font_size_table [container->details->zoom_level]);

    pango_layout_set_font_description (layout, desc);

    pango_font_description_free (desc);

    pango_layout_set_width (layout, floor (GET_VIEW_CONSTANT (container, max_text_width_standard)) * PANGO_SCALE);
    pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_END);

    pango_layout_get_pixel_size (layout,
                                 NULL,
                                 &height);

    height *= ellipses_limit;

    height += icon_size + GET_VIEW_CONSTANT (container, icon_pad_bottom) + LABEL_OFFSET;

    return height / 2;
}

static void
update_layout_constants (NemoIconContainer *container)
{
    gint icon_size;
    NemoViewLayoutConstants *constants;
    gdouble scale, h_adjust, v_adjust;

    icon_size = nemo_get_desktop_icon_size_for_zoom_level (container->details->zoom_level);

    scale = (double) icon_size / NEMO_DESKTOP_ICON_SIZE_STANDARD;

    h_adjust = g_settings_get_double (nemo_desktop_preferences,
                                      NEMO_PREFERENCES_DESKTOP_HORIZONTAL_GRID_ADJUST);

    v_adjust = g_settings_get_double (nemo_desktop_preferences,
                                      NEMO_PREFERENCES_DESKTOP_VERTICAL_GRID_ADJUST);

    constants = container->details->view_constants;

    constants->snap_size_x = BASE_SNAP_SIZE_X * scale * h_adjust;
    constants->snap_size_y = BASE_SNAP_SIZE_Y * scale * v_adjust;
    constants->max_text_width_standard = BASE_MAX_TEXT_WIDTH * scale * h_adjust;

    constants->icon_vertical_adjust = get_vertical_adjustment (container, icon_size);

    /* This isn't what this is intended for, but it's a simple way vs. overriding what
     * icon_get_size() uses to get the icon size in nemo-icon-container.c (it should use
     * nemo_get_desktop_icon_size_for_zoom_level) */
    nemo_icon_container_set_forced_icon_size (container, icon_size);
}

static void
nemo_icon_view_grid_container_set_zoom_level (NemoIconContainer *container, gint new_level)
{
    NemoIconContainerDetails *details;
    int pinned_level;

    details = container->details;

    nemo_icon_container_end_renaming_mode (container, TRUE);

    pinned_level = new_level;
    if (pinned_level < NEMO_ZOOM_LEVEL_SMALLEST) {
        pinned_level = NEMO_ZOOM_LEVEL_SMALLEST;
    } else if (pinned_level > NEMO_ZOOM_LEVEL_LARGEST) {
        pinned_level = NEMO_ZOOM_LEVEL_LARGEST;
    }

    if (pinned_level == details->zoom_level) {
        return;
    }

    details->zoom_level = pinned_level;

    eel_canvas_set_pixels_per_unit (EEL_CANVAS (container), 1.0);

    update_layout_constants (container);
}

NemoIconContainer *
nemo_icon_view_grid_container_construct (NemoIconViewGridContainer *icon_container, NemoIconView *view)
{
    AtkObject *atk_obj;
    NemoViewLayoutConstants *constants = NEMO_ICON_CONTAINER (icon_container)->details->view_constants;

    g_return_val_if_fail (NEMO_IS_ICON_VIEW (view), NULL);

    icon_container->view = view;
    atk_obj = gtk_widget_get_accessible (GTK_WIDGET (icon_container));
    atk_object_set_name (atk_obj, _("Icon View"));

    constants = NEMO_ICON_CONTAINER (icon_container)->details->view_constants;

    constants->icon_pad_left = 4;
    constants->icon_pad_right = 4;
    constants->icon_pad_top = 4;
    constants->icon_pad_bottom = 4;
    constants->container_pad_left = 4;
    constants->container_pad_right = 4;
    constants->container_pad_top = 4;
    constants->container_pad_bottom = 4;
    constants->standard_icon_grid_width = 155; // Not used
    constants->text_beside_icon_grid_width = 205; // Not used
    constants->desktop_pad_horizontal = 10; // Not used
    constants->desktop_pad_vertical = 10; // Not used
    constants->snap_size_x = BASE_SNAP_SIZE_X;
    constants->snap_size_y = BASE_SNAP_SIZE_Y;
    constants->max_text_width_standard = BASE_MAX_TEXT_WIDTH;
    constants->max_text_width_tighter = 80; // Not used
    constants->max_text_width_beside = 90; // Not used
    constants->max_text_width_beside_top_to_bottom = 150; // Not used
    constants->icon_vertical_adjust = 20;

    return NEMO_ICON_CONTAINER (icon_container);
}

static void
nemo_icon_view_grid_container_class_init (NemoIconViewGridContainerClass *klass)
{
	NemoIconContainerClass *ic_class;

	ic_class = &klass->parent_class;

	attribute_none_q = g_quark_from_static_string ("none");
	
	ic_class->get_icon_text = nemo_icon_view_grid_container_get_icon_text;
	ic_class->get_icon_images = nemo_icon_view_grid_container_get_icon_images;
	ic_class->get_icon_description = nemo_icon_view_grid_container_get_icon_description;
	ic_class->start_monitor_top_left = nemo_icon_view_grid_container_start_monitor_top_left;
	ic_class->stop_monitor_top_left = nemo_icon_view_grid_container_stop_monitor_top_left;
	ic_class->prioritize_thumbnailing = nemo_icon_view_grid_container_prioritize_thumbnailing;

	ic_class->compare_icons = nemo_icon_view_grid_container_compare_icons;
	ic_class->freeze_updates = nemo_icon_view_grid_container_freeze_updates;
	ic_class->unfreeze_updates = nemo_icon_view_grid_container_unfreeze_updates;
    ic_class->lay_down_icons = nemo_icon_view_grid_container_lay_down_icons;
    ic_class->icon_set_position = nemo_icon_view_grid_container_icon_set_position;
    ic_class->move_icon = nemo_icon_view_grid_container_move_icon;
    ic_class->align_icons = nemo_icon_view_grid_container_align_icons;
    ic_class->reload_icon_positions = nemo_icon_view_grid_container_reload_icon_positions;
    ic_class->finish_adding_new_icons = nemo_icon_view_grid_container_finish_adding_new_icons;
    ic_class->icon_get_bounding_box = nemo_icon_view_grid_container_icon_get_bounding_box;
    ic_class->set_zoom_level = nemo_icon_view_grid_container_set_zoom_level;

    EEL_CANVAS_CLASS (klass)->draw_background = nemo_icon_view_grid_container_draw_background;
}

static void
nemo_icon_view_grid_container_init (NemoIconViewGridContainer *icon_container)
{
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (icon_container)),
				     GTK_STYLE_CLASS_VIEW);

    NEMO_ICON_CONTAINER (icon_container)->details->font_size_table[NEMO_ZOOM_LEVEL_SMALL] = -1 * PANGO_SCALE;
    NEMO_ICON_CONTAINER (icon_container)->details->font_size_table[NEMO_ZOOM_LEVEL_LARGE] =  1 * PANGO_SCALE;
}

NemoIconContainer *
nemo_icon_view_grid_container_new (NemoIconView *view)
{
	return nemo_icon_view_grid_container_construct
		(g_object_new (NEMO_TYPE_ICON_VIEW_GRID_CONTAINER, NULL),
		 view);
}

void
nemo_icon_view_grid_container_set_sort_desktop (NemoIconViewGridContainer *container,
					       gboolean         desktop)
{
	container->sort_for_desktop = desktop;
}

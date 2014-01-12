/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-icon-canvas-container.c - the container widget for file manager icons

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

#include "nemo-canvas-view-container.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <eel/eel-glib-extensions.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-file-attributes.h>
#include <libnemo-private/nemo-thumbnails.h>
#include <libnemo-private/nemo-desktop-icon-file.h>

#define ICON_TEXT_ATTRIBUTES_NUM_ITEMS		3
#define ICON_TEXT_ATTRIBUTES_DEFAULT_TOKENS	"size,date_modified,type"

G_DEFINE_TYPE (NemoCanvasViewContainer, nemo_canvas_view_container, NEMO_TYPE_CANVAS_CONTAINER);

static GQuark attribute_none_q;

static NemoCanvasView *
get_canvas_view (NemoCanvasContainer *container)
{
	/* Type unsafe comparison for performance */
	return ((NemoCanvasViewContainer *)container)->view;
}

static NemoIconInfo *
nemo_canvas_view_container_get_icon_images (NemoCanvasContainer *container,
					      NemoCanvasIconData      *data,
					      int                    size,
					      char                 **embedded_text,
					      gboolean               for_drag_accept,
					      gboolean               need_large_embeddded_text,
					      gboolean              *embedded_text_needs_loading,
					      gboolean              *has_window_open)
{
	NemoCanvasView *canvas_view;
	NemoFile *file;
	gboolean use_embedding;
	NemoFileIconFlags flags;
	NemoIconInfo *icon_info;
	GdkPixbuf *pixbuf;
	GIcon *emblemed_icon;
	GEmblem *emblem;
	GList *emblem_icons, *l;

	file = (NemoFile *) data;

	g_assert (NEMO_IS_FILE (file));
	canvas_view = get_canvas_view (container);
	g_return_val_if_fail (canvas_view != NULL, NULL);

	use_embedding = FALSE;
	if (embedded_text) {
		*embedded_text = nemo_file_peek_top_left_text (file, need_large_embeddded_text, embedded_text_needs_loading);
		use_embedding = *embedded_text != NULL;
	}
	
	*has_window_open = nemo_file_has_open_window (file);

	flags = NEMO_FILE_ICON_FLAGS_USE_MOUNT_ICON_AS_EMBLEM;
	if (!nemo_canvas_view_is_compact (canvas_view) ||
	    nemo_canvas_container_get_zoom_level (container) > NEMO_ZOOM_LEVEL_STANDARD) {
		flags |= NEMO_FILE_ICON_FLAGS_USE_THUMBNAILS;
		if (nemo_canvas_view_is_compact (canvas_view)) {
			flags |= NEMO_FILE_ICON_FLAGS_FORCE_THUMBNAIL_SIZE;
		}
	}

	if (use_embedding) {
		flags |= NEMO_FILE_ICON_FLAGS_EMBEDDING_TEXT;
	}
	if (for_drag_accept) {
		flags |= NEMO_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT;
	}

	icon_info = nemo_file_get_icon (file, size, flags);
	emblem_icons = nemo_file_get_emblem_icons (file);

	/* apply emblems */
	if (emblem_icons != NULL) {
		l = emblem_icons;


		pixbuf = nemo_icon_info_get_pixbuf (icon_info);

        gint w, h, s;
        gboolean bad_ratio;

        w = gdk_pixbuf_get_width (pixbuf);
        h = gdk_pixbuf_get_height (pixbuf);

        s = MAX (w, h);
        if (s < size)
            size = s;

        bad_ratio = nemo_icon_get_emblem_size_for_icon_size (size) > w ||
                    nemo_icon_get_emblem_size_for_icon_size (size) > h;

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
		icon_info = nemo_icon_info_lookup (emblemed_icon, size);
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
nemo_canvas_view_container_get_icon_description (NemoCanvasContainer *container,
						   NemoCanvasIconData      *data)
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
nemo_canvas_view_container_start_monitor_top_left (NemoCanvasContainer *container,
						     NemoCanvasIconData      *data,
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
nemo_canvas_view_container_stop_monitor_top_left (NemoCanvasContainer *container,
						    NemoCanvasIconData      *data,
						    gconstpointer          client)
{
	NemoFile *file;

	file = (NemoFile *) data;

	g_assert (NEMO_IS_FILE (file));

	nemo_file_monitor_remove (file, client);
}

static void
nemo_canvas_view_container_prioritize_thumbnailing (NemoCanvasContainer *container,
						      NemoCanvasIconData      *data)
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
nemo_canvas_view_container_get_icon_text_attributes_from_preferences (void)
{
	static GQuark *attributes = NULL;

	if (attributes == NULL) {
		update_auto_strv_as_quarks (nemo_canvas_view_preferences,
					    NEMO_PREFERENCES_CANVAS_VIEW_CAPTIONS,
					    &attributes);
		g_signal_connect (nemo_canvas_view_preferences,
				  "changed::" NEMO_PREFERENCES_CANVAS_VIEW_CAPTIONS,
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
 * nemo_canvas_view_get_icon_text_attribute_names:
 *
 * Get a list representing which text attributes should be displayed
 * beneath an icon. The result is dependent on zoom level and possibly
 * user configuration. Don't free the result.
 * @view: NemoCanvasView to query.
 * 
 **/
static GQuark *
nemo_canvas_view_container_get_icon_text_attribute_names (NemoCanvasContainer *container,
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

	piece_count = pieces_by_level[nemo_canvas_container_get_zoom_level (container)];

	attributes = nemo_canvas_view_container_get_icon_text_attributes_from_preferences ();

	*len = MIN (piece_count, quarkv_length (attributes));

	return attributes;
}

/* This callback returns the text, both the editable part, and the
 * part below that is not editable.
 */
static void
nemo_canvas_view_container_get_icon_text (NemoCanvasContainer *container,
					    NemoCanvasIconData      *data,
					    char                 **editable_text,
					    char                 **additional_text,
					    gboolean               include_invisible)
{
	GQuark *attributes;
	char *text_array[4];
	int i, j, num_attributes;
	NemoCanvasView *canvas_view;
	NemoFile *file;
	gboolean use_additional;

	file = NEMO_FILE (data);

	g_assert (NEMO_IS_FILE (file));
	g_assert (editable_text != NULL);
	canvas_view = get_canvas_view (container);
	g_return_if_fail (canvas_view != NULL);

	use_additional = (additional_text != NULL);

	/* In the smallest zoom mode, no text is drawn. */
	if (nemo_canvas_container_get_zoom_level (container) == NEMO_ZOOM_LEVEL_SMALLEST &&
            !include_invisible) {
		*editable_text = NULL;
	} else {
		/* Strip the suffix for nemo object xml files. */
		*editable_text = nemo_file_get_display_name (file);
	}

	if (!use_additional) {
		return;
	}

	if (nemo_canvas_view_is_compact (canvas_view)) {
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
	attributes = nemo_canvas_view_container_get_icon_text_attribute_names (container,
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
fm_desktop_canvas_container_icons_compare (NemoCanvasContainer *container,
					 NemoCanvasIconData      *data_a,
					 NemoCanvasIconData      *data_b)
{
	NemoFile *file_a;
	NemoFile *file_b;
	NemoView *directory_view;
	SortCategory category_a, category_b;

	file_a = (NemoFile *) data_a;
	file_b = (NemoFile *) data_b;

	directory_view = NEMO_VIEW (NEMO_CANVAS_VIEW_CONTAINER (container)->view);
	g_return_val_if_fail (directory_view != NULL, 0);
	
	category_a = get_sort_category (file_a);
	category_b = get_sort_category (file_b);

	if (category_a == category_b) {
		return nemo_file_compare_for_sort 
			(file_a, file_b, NEMO_FILE_SORT_BY_DISPLAY_NAME, 
			 nemo_view_should_sort_directories_first (directory_view),
			 FALSE);
	}

	if (category_a < category_b) {
		return -1;
	} else {
		return +1;
	}
}

static int
nemo_canvas_view_container_compare_icons (NemoCanvasContainer *container,
					    NemoCanvasIconData      *icon_a,
					    NemoCanvasIconData      *icon_b)
{
	NemoCanvasView *canvas_view;

	canvas_view = get_canvas_view (container);
	g_return_val_if_fail (canvas_view != NULL, 0);

	if (NEMO_CANVAS_VIEW_CONTAINER (container)->sort_for_desktop) {
		return fm_desktop_canvas_container_icons_compare
			(container, icon_a, icon_b);
	}

	/* Type unsafe comparisons for performance */
	return nemo_canvas_view_compare_files (canvas_view,
					   (NemoFile *)icon_a,
					   (NemoFile *)icon_b);
}

static int
nemo_canvas_view_container_compare_icons_by_name (NemoCanvasContainer *container,
						    NemoCanvasIconData      *icon_a,
						    NemoCanvasIconData      *icon_b)
{
	return nemo_file_compare_for_sort
		(NEMO_FILE (icon_a),
		 NEMO_FILE (icon_b),
		 NEMO_FILE_SORT_BY_DISPLAY_NAME,
		 FALSE, FALSE);
}

static void
nemo_canvas_view_container_freeze_updates (NemoCanvasContainer *container)
{
	NemoCanvasView *canvas_view;
	canvas_view = get_canvas_view (container);
	g_return_if_fail (canvas_view != NULL);
	nemo_view_freeze_updates (NEMO_VIEW (canvas_view));
}

static void
nemo_canvas_view_container_unfreeze_updates (NemoCanvasContainer *container)
{
	NemoCanvasView *canvas_view;
	canvas_view = get_canvas_view (container);
	g_return_if_fail (canvas_view != NULL);
	nemo_view_unfreeze_updates (NEMO_VIEW (canvas_view));
}

static void
nemo_canvas_view_container_class_init (NemoCanvasViewContainerClass *klass)
{
	NemoCanvasContainerClass *ic_class;

	ic_class = &klass->parent_class;

	attribute_none_q = g_quark_from_static_string ("none");
	
	ic_class->get_icon_text = nemo_canvas_view_container_get_icon_text;
	ic_class->get_icon_images = nemo_canvas_view_container_get_icon_images;
	ic_class->get_icon_description = nemo_canvas_view_container_get_icon_description;
	ic_class->start_monitor_top_left = nemo_canvas_view_container_start_monitor_top_left;
	ic_class->stop_monitor_top_left = nemo_canvas_view_container_stop_monitor_top_left;
	ic_class->prioritize_thumbnailing = nemo_canvas_view_container_prioritize_thumbnailing;

	ic_class->compare_icons = nemo_canvas_view_container_compare_icons;
	ic_class->compare_icons_by_name = nemo_canvas_view_container_compare_icons_by_name;
	ic_class->freeze_updates = nemo_canvas_view_container_freeze_updates;
	ic_class->unfreeze_updates = nemo_canvas_view_container_unfreeze_updates;
}

static void
nemo_canvas_view_container_init (NemoCanvasViewContainer *canvas_container)
{
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (canvas_container)),
				     GTK_STYLE_CLASS_VIEW);

}

NemoCanvasContainer *
nemo_canvas_view_container_construct (NemoCanvasViewContainer *canvas_container, NemoCanvasView *view)
{
	AtkObject *atk_obj;

	g_return_val_if_fail (NEMO_IS_CANVAS_VIEW (view), NULL);

	canvas_container->view = view;
	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (canvas_container));
	atk_object_set_name (atk_obj, _("Canvas View"));

	return NEMO_CANVAS_CONTAINER (canvas_container);
}

NemoCanvasContainer *
nemo_canvas_view_container_new (NemoCanvasView *view)
{
	return nemo_canvas_view_container_construct
		(g_object_new (NEMO_TYPE_CANVAS_VIEW_CONTAINER, NULL),
		 view);
}

void
nemo_canvas_view_container_set_sort_desktop (NemoCanvasViewContainer *container,
					       gboolean         desktop)
{
	container->sort_for_desktop = desktop;
}

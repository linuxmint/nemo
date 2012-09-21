/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-convert-metadata.c - Convert old metadata format to gvfs metadata.
 *
 * Copyright (C) 2009 Alexander Larsson
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
 *
 * Authors:
 *   Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include <glib.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <string.h>
#include <libxml/tree.h>

#include <libnemo-private/nemo-metadata.h>

static gboolean quiet = FALSE;

static xmlNodePtr
xml_get_children (xmlNodePtr parent)
{
	if (parent == NULL) {
		return NULL;
	}
	return parent->children;
}

static xmlNodePtr
xml_get_root_children (xmlDocPtr document)
{
	return xml_get_children (xmlDocGetRootElement (document));
}


static char *
get_uri_from_nemo_metafile_name (const char *filename)
{
	GString *s;
	char c;
	char *base_name, *p;
	int len;

	base_name = g_path_get_basename (filename);
	len = strlen (base_name);
	if (len <=  4 ||
	    strcmp (base_name + len - 4, ".xml") != 0) {
		g_free (base_name);
		return NULL;
	}
	base_name[len-4] = 0;

	s = g_string_new (NULL);

	p = base_name;
	while (*p) {
		c = *p++;
		if (c == '%') {
			c = g_ascii_xdigit_value (p[0]) << 4 |
			    g_ascii_xdigit_value (p[1]);
			p += 2;
		}
		g_string_append_c (s, c);
	}
	g_free (base_name);

	return g_string_free (s, FALSE);
}

static struct {
	const char *old_key;
	const char *new_key;
} metadata_keys[] = {
	{"default_component", "metadata::" NEMO_METADATA_KEY_DEFAULT_VIEW},
	{"background_color", "metadata::" NEMO_METADATA_KEY_LOCATION_BACKGROUND_COLOR},
	{"background_tile_image", "metadata::" NEMO_METADATA_KEY_LOCATION_BACKGROUND_IMAGE},
	{"icon_view_zoom_level", "metadata::" NEMO_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL},
	{"icon_view_auto_layout", "metadata::" NEMO_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT},
	{"icon_view_sort_by", "metadata::" NEMO_METADATA_KEY_ICON_VIEW_SORT_BY},
	{"icon_view_sort_reversed", "metadata::" NEMO_METADATA_KEY_ICON_VIEW_SORT_REVERSED},
	{"icon_view_keep_aligned", "metadata::" NEMO_METADATA_KEY_ICON_VIEW_KEEP_ALIGNED},
	{"icon_view_layout_timestamp", "metadata::" NEMO_METADATA_KEY_ICON_VIEW_LAYOUT_TIMESTAMP},
	{"list_view_zoom_level", "metadata::" NEMO_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL},
	{"list_view_sort_column", "metadata::" NEMO_METADATA_KEY_LIST_VIEW_SORT_COLUMN},
	{"list_view_sort_reversed", "metadata::" NEMO_METADATA_KEY_LIST_VIEW_SORT_REVERSED},
	{"list_view_visible_columns", "metadata::" NEMO_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS},
	{"list_view_column_order", "metadata::" NEMO_METADATA_KEY_LIST_VIEW_COLUMN_ORDER},
	{"compact_view_zoom_level", "metadata::" NEMO_METADATA_KEY_COMPACT_VIEW_ZOOM_LEVEL},
	{"window_geometry", "metadata::" NEMO_METADATA_KEY_WINDOW_GEOMETRY},
	{"window_scroll_position", "metadata::" NEMO_METADATA_KEY_WINDOW_SCROLL_POSITION},
	{"window_show_hidden_files", "metadata::" NEMO_METADATA_KEY_WINDOW_SHOW_HIDDEN_FILES},
	{"window_maximized", "metadata::" NEMO_METADATA_KEY_WINDOW_MAXIMIZED},
	{"window_sticky", "metadata::" NEMO_METADATA_KEY_WINDOW_STICKY},
	{"window_keep_above", "metadata::" NEMO_METADATA_KEY_WINDOW_KEEP_ABOVE},
	{"sidebar_background_color", "metadata::" NEMO_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR},
	{"sidebar_background_tile_image", "metadata::" NEMO_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE},
	{"sidebar_buttons", "metadata::" NEMO_METADATA_KEY_SIDEBAR_BUTTONS},
	{"annotation", "metadata::" NEMO_METADATA_KEY_ANNOTATION},
	{"icon_position", "metadata::" NEMO_METADATA_KEY_ICON_POSITION},
	{"icon_position_timestamp", "metadata::" NEMO_METADATA_KEY_ICON_POSITION_TIMESTAMP},
	{"icon_scale", "metadata::" NEMO_METADATA_KEY_ICON_SCALE},
	{"custom_icon", "metadata::" NEMO_METADATA_KEY_CUSTOM_ICON},
	{"screen", "metadata::" NEMO_METADATA_KEY_SCREEN},
	{"keyword", "metadata::" NEMO_METADATA_KEY_EMBLEMS},
};

static const char *
convert_key_name (const char *old_key)
{
	int i;

	for (i = 0; i < G_N_ELEMENTS (metadata_keys); i++) {
		if (strcmp (metadata_keys[i].old_key, old_key) == 0) {
			return metadata_keys[i].new_key;
		}
	}

	return NULL;
}

static void
parse_xml_node (GFile *file,
		xmlNodePtr filenode)
{
	xmlNodePtr node;
	xmlAttrPtr attr;
	xmlChar *property;
	const char *new_key;
	GHashTable *list_keys;
	GList *keys, *l;
	GHashTableIter iter;
	GFileInfo *info;
	int i;
	char **strv;
	GError *error;

	info = g_file_info_new ();

	for (attr = filenode->properties; attr != NULL; attr = attr->next) {
		if (strcmp ((char *)attr->name, "name") == 0 ||
		    strcmp ((char *)attr->name, "timestamp") == 0) {
			continue;
		}

		new_key = convert_key_name (attr->name);
		if (new_key) {
			property = xmlGetProp (filenode, attr->name);
			if (property) {
				g_file_info_set_attribute_string (info,
								  new_key,
								  property);
				xmlFree (property);
			}
		}
	}

	list_keys = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);
	for (node = filenode->children; node != NULL; node = node->next) {
		for (attr = node->properties; attr != NULL; attr = attr->next) {
			new_key = convert_key_name (node->name);
			if (new_key) {
				property = xmlGetProp (node, attr->name);
				if (property) {
					keys = g_hash_table_lookup (list_keys, new_key);
					keys = g_list_append (keys, property);
					g_hash_table_replace (list_keys, (char *)new_key, keys);
				}
			}
		}
	}

	g_hash_table_iter_init (&iter, list_keys);
	while (g_hash_table_iter_next (&iter, (void **)&new_key, (void **)&keys)) {
		strv = g_new0 (char *, g_list_length (keys) + 1);

		for (l = keys, i = 0; l != NULL; l = l->next, i++) {
			strv[i] = l->data;
		}
		g_file_info_set_attribute_stringv (info,
						   new_key,
						   strv);
		g_free (strv);
		g_list_foreach (keys, (GFunc)xmlFree, NULL);
		g_list_free (keys);
	}
	g_hash_table_destroy (list_keys);

	if (info) {
		error = NULL;
		if (!g_file_set_attributes_from_info (file,
						      info,
						      0, NULL, &error)) {
			char *uri;

			uri = g_file_get_uri (file);
			if (!quiet) {
				g_print ("error setting info for %s: %s\n", uri, error->message);
			}
			g_free (uri);
			g_error_free (error);
		}
		g_object_unref (info);
	}
}

static void
convert_xml_file (xmlDocPtr xml,
		  GFile *dir)
{
	xmlNodePtr node;
	xmlChar *name;
	char *unescaped_name;
	GFile *file;

	for (node = xml_get_root_children (xml);
	     node != NULL; node = node->next) {
		if (strcmp ((char *)node->name, "file") == 0) {
			name = xmlGetProp (node, (xmlChar *)"name");
			unescaped_name = g_uri_unescape_string ((char *)name, "/");
			xmlFree (name);

			if (unescaped_name == NULL) {
				continue;
			}

			if (strcmp (unescaped_name, ".") == 0) {
				file = g_object_ref (dir);
			} else  {
			    file = g_file_get_child (dir, unescaped_name);
			}

			parse_xml_node (file, node);
			g_object_unref (file);
			g_free (unescaped_name);
		}
	}
}

static void
convert_nemo_file (char *file)
{
	GFile *dir;
	char *uri;
	gchar *contents;
	gsize length;
	xmlDocPtr xml;

	if (!g_file_get_contents (file, &contents, &length, NULL)) {
		if (!quiet) {
			g_print ("failed to load %s\n", file);
		}
		return;
	}

	uri = get_uri_from_nemo_metafile_name (file);
	if (uri == NULL) {
		g_free (contents);
		return;
	}

	dir = g_file_new_for_uri (uri);
	g_free (uri);

	xml = xmlParseMemory (contents, length);
	g_free (contents);
	if (xml == NULL) {
		return;
	}

	convert_xml_file (xml, dir);
	xmlFreeDoc (xml);
}

static GOptionEntry entries[] =
{
	{ "quiet", 'q', 0, G_OPTION_ARG_NONE, &quiet,
	  "Don't show errors", NULL },
	{ NULL }
};

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	int i;

	g_type_init ();

	context = g_option_context_new ("<nemo metadata files> - convert nemo metadata");
	g_option_context_add_main_entries (context, entries, NULL);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr ("option parsing failed: %s\n", error->message);
		return 1;
	}

	if (argc < 2) {
		GDir *dir;
		char *metafile_dir;
		char *file;
		const char *entry;

		/* Convert all metafiles */

		metafile_dir = g_build_filename (g_get_home_dir (),
						 ".nemo/metafiles", NULL);

		dir = g_dir_open (metafile_dir, 0, NULL);
		if (dir) {
			while ((entry = g_dir_read_name (dir)) != NULL) {
				file = g_build_filename (metafile_dir, entry, NULL);
				if (g_str_has_suffix (file, ".xml"))
					convert_nemo_file (file);
				g_free (file);
			}
			g_dir_close (dir);
		}
		g_free (metafile_dir);
	} else {
		for (i = 1; i < argc; i++) {
			convert_nemo_file (argv[i]);
		}
	}

	return 0;
}

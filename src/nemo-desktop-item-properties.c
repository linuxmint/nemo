/*
 *  fm-ditem-page.c: Desktop item editing support
 * 
 *  Copyright (C) 2004 James Willcox
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 *  Authors: James Willcox <jwillcox@gnome.org>
 *
 */

#include <config.h>

#include "nemo-desktop-item-properties.h"

#include <string.h>

#include <eel/eel-glib-extensions.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libnemo-extension/nemo-extension-types.h>
#include <libnemo-extension/nemo-file-info.h>
#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-file-attributes.h>

#define MAIN_GROUP "Desktop Entry"

typedef struct ItemEntry {
	const char *field;
	const char *description;
	char *current_value;
	gboolean localized;
	gboolean filename;
} ItemEntry;

enum {
	TARGET_URI_LIST
};

static const GtkTargetEntry target_table[] = {
        { "text/uri-list",  0, TARGET_URI_LIST }
};

static gboolean
_g_key_file_load_from_gfile (GKeyFile *key_file,
			     GFile *file,
			     GKeyFileFlags flags,
			     GError **error)
{
	char *data;
	gsize len;
	gboolean res;
	
	if (!g_file_load_contents (file, NULL, &data, &len, NULL, error)) {
		return FALSE;
	}

	res = g_key_file_load_from_data (key_file, data, len, flags, error);
	
	g_free (data);
	
	return res;
}

static gboolean
_g_key_file_save_to_uri (GKeyFile *key_file,
			 const char *uri,
			 GError  **error)
{
	GFile *file;
	char *data;
	gsize len;

	data = g_key_file_to_data (key_file, &len, error);
	if (data == NULL) {
		return FALSE;
	}
	file = g_file_new_for_uri (uri);
	if (!g_file_replace_contents (file,
				      data, len,
				      NULL, FALSE,
				      G_FILE_CREATE_NONE,
				      NULL, NULL, error)) {
		g_object_unref (file);
		g_free (data);
		return FALSE;
	}
	g_object_unref (file);
	g_free (data);
	return TRUE;
}

static GKeyFile *
_g_key_file_new_from_file (GFile *file,
			   GKeyFileFlags flags,
			   GError **error)
{
	GKeyFile *key_file;
	
	key_file = g_key_file_new ();
	if (!_g_key_file_load_from_gfile (key_file, file, flags, error)) {
		g_key_file_free (key_file);
		key_file = NULL;
	}
	return key_file;
}

static GKeyFile *
_g_key_file_new_from_uri (const char *uri,
			  GKeyFileFlags flags,
			  GError **error)
{
	GKeyFile *key_file;
	GFile *file;
	
	file = g_file_new_for_uri (uri);
	key_file = _g_key_file_new_from_file (file, flags, error);
	g_object_unref (file);
	return key_file;
}

static ItemEntry *
item_entry_new (const char *field,
		const char *description,
		gboolean localized,
		gboolean filename)
{
	ItemEntry *entry;

	entry = g_new0 (ItemEntry, 1);
	entry->field = field;
	entry->description = description;
	entry->localized = localized;
	entry->filename = filename;
	
	return entry;
}

static void
item_entry_free (ItemEntry *entry)
{
	g_free (entry->current_value);
	g_free (entry);
}

static void
nemo_desktop_item_properties_url_drag_data_received (GtkWidget *widget, GdkDragContext *context,
                                                         int x, int y,
                                                         GtkSelectionData *selection_data,
                                                         guint info, guint time,
                                                         GtkEntry *entry)
{
	char **uris;
	gboolean exactly_one;
	char *path;
	
	uris = g_strsplit (gtk_selection_data_get_data (selection_data), "\r\n", 0);
        exactly_one = uris[0] != NULL && (uris[1] == NULL || uris[1][0] == '\0');

	if (!exactly_one) {
		g_strfreev (uris);
		return;
	}

	path = g_filename_from_uri (uris[0], NULL, NULL);
	if (path != NULL) {
		gtk_entry_set_text (entry, path);
		g_free (path);
	} else {
		gtk_entry_set_text (entry, uris[0]);
	}
	
	g_strfreev (uris);
}

static void
nemo_desktop_item_properties_exec_drag_data_received (GtkWidget *widget, GdkDragContext *context,
                                                          int x, int y,
                                                          GtkSelectionData *selection_data,
                                                          guint info, guint time,
                                                          GtkEntry *entry)
{
	char **uris;
	gboolean exactly_one;
	NemoFile *file;
	GKeyFile *key_file;
	char *uri, *type, *exec;
	
	uris = g_strsplit (gtk_selection_data_get_data (selection_data), "\r\n", 0);
        exactly_one = uris[0] != NULL && (uris[1] == NULL || uris[1][0] == '\0');

	if (!exactly_one) {
		g_strfreev (uris);
		return;
	}

	file = nemo_file_get_by_uri (uris[0]);

	g_return_if_fail (file != NULL);
	
	uri = nemo_file_get_uri (file);
	if (nemo_file_is_mime_type (file, "application/x-desktop")) {
		key_file = _g_key_file_new_from_uri (uri, G_KEY_FILE_NONE, NULL);
		if (key_file != NULL) {
			type = g_key_file_get_string (key_file, MAIN_GROUP, "Type", NULL);
			if (type != NULL && strcmp (type, "Application") == 0) {
				exec = g_key_file_get_string (key_file, MAIN_GROUP, "Exec", NULL);
				if (exec != NULL) {
					g_free (uri);
					uri = exec;
				}
			}
			g_free (type);
			g_key_file_free (key_file);
		}
	} 
	gtk_entry_set_text (entry,
			    uri?uri:"");
	gtk_widget_grab_focus (GTK_WIDGET (entry));
	
	g_free (uri);
	
	nemo_file_unref (file);
	
	g_strfreev (uris);
}

static void
save_entry (GtkEntry *entry, GKeyFile *key_file, const char *uri)
{
	GError *error;
	ItemEntry *item_entry;
	const char *val;
	gchar **languages;

	item_entry = g_object_get_data (G_OBJECT (entry), "item_entry");
	val = gtk_entry_get_text (entry);

	if (strcmp (val, item_entry->current_value) == 0) {
		return; /* No actual change, don't update file */
	}

	g_free (item_entry->current_value);
	item_entry->current_value = g_strdup (val);
	
	if (item_entry->localized) {
		languages = (gchar **) g_get_language_names ();
		g_key_file_set_locale_string (key_file, MAIN_GROUP, item_entry->field, languages[0], val);
	} else {
		g_key_file_set_string (key_file, MAIN_GROUP, item_entry->field, val);
	}

	error = NULL;

	if (!_g_key_file_save_to_uri (key_file, uri, &error)) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}
}

static void
entry_activate_cb (GtkWidget *entry,
		   GtkWidget *container)
{
	const char *uri;
	GKeyFile *key_file;
	
	uri = g_object_get_data (G_OBJECT (container), "uri");
	key_file = g_object_get_data (G_OBJECT (container), "keyfile");
	save_entry (GTK_ENTRY (entry), key_file, uri);
}

static gboolean
entry_focus_out_cb (GtkWidget *entry,
		    GdkEventFocus *event,
		    GtkWidget *container)
{
	const char *uri;
	GKeyFile *key_file;
	
	uri = g_object_get_data (G_OBJECT (container), "uri");
	key_file = g_object_get_data (G_OBJECT (container), "keyfile");
	save_entry (GTK_ENTRY (entry), key_file, uri);
	return FALSE;
}

static GtkWidget *
build_grid (GtkWidget *container,
            GKeyFile *key_file,
            GtkSizeGroup *label_size_group,
            GList *entries)
{
	GtkWidget *grid;
	GtkWidget *label;
	GtkWidget *entry;
	GList *l;
	char *val;

        grid = gtk_grid_new ();
        gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	
	for (l = entries; l; l = l->next) {
		ItemEntry *item_entry = (ItemEntry *)l->data;
		char *label_text;

		label_text = g_strdup_printf ("%s:", item_entry->description);
		label = gtk_label_new (label_text);
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		g_free (label_text);
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_size_group_add_widget (label_size_group, label);

		entry = gtk_entry_new ();
                gtk_widget_set_hexpand (entry, TRUE);

		if (item_entry->localized) {
			val = g_key_file_get_locale_string (key_file,
							    MAIN_GROUP,
							    item_entry->field,
							    NULL, NULL);
		} else {
			val = g_key_file_get_string (key_file,
						     MAIN_GROUP,
						     item_entry->field,
						     NULL);
		}
		
		item_entry->current_value = g_strdup (val?val:"");
		gtk_entry_set_text (GTK_ENTRY (entry), item_entry->current_value);
		g_free (val);

                gtk_container_add (GTK_CONTAINER (grid), label);
                gtk_grid_attach_next_to (GTK_GRID (grid), entry, label,
                                         GTK_POS_RIGHT, 1, 1);

		g_signal_connect (entry, "activate",
				  G_CALLBACK (entry_activate_cb),
				  container);
		g_signal_connect (entry, "focus_out_event",
				  G_CALLBACK (entry_focus_out_cb),
				  container);
		
		g_object_set_data_full (G_OBJECT (entry), "item_entry", item_entry,
					(GDestroyNotify)item_entry_free);

		if (item_entry->filename) {
			gtk_drag_dest_set (GTK_WIDGET (entry),
					   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
					   target_table, G_N_ELEMENTS (target_table),
					   GDK_ACTION_COPY | GDK_ACTION_MOVE);
			
			g_signal_connect (entry, "drag_data_received",
					  G_CALLBACK (nemo_desktop_item_properties_url_drag_data_received),
					  entry);
		} else if (strcmp (item_entry->field, "Exec") == 0) {
			gtk_drag_dest_set (GTK_WIDGET (entry),
					   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
					   target_table, G_N_ELEMENTS (target_table),
					   GDK_ACTION_COPY | GDK_ACTION_MOVE);
			
			g_signal_connect (entry, "drag_data_received",
					  G_CALLBACK (nemo_desktop_item_properties_exec_drag_data_received),
					  entry);
		}
	}

	/* append dummy row */
	label = gtk_label_new ("");
        gtk_container_add (GTK_CONTAINER (grid), label);
	gtk_size_group_add_widget (label_size_group, label);

	gtk_widget_show_all (grid);
	return grid;
}

static void
create_page (GKeyFile *key_file, GtkWidget *box)
{
	GtkWidget *grid;
	GList *entries;
	GtkSizeGroup *label_size_group;
	char *type;
	
	entries = NULL;

	type = g_key_file_get_string (key_file, MAIN_GROUP, "Type", NULL);
	
	if (g_strcmp0 (type, "Link") == 0) {
		entries = g_list_prepend (entries,
					  item_entry_new ("Comment",
							  _("Comment"), TRUE, FALSE));
		entries = g_list_prepend (entries,
					  item_entry_new ("URL",
							  _("URL"), FALSE, TRUE));
		entries = g_list_prepend (entries,
					  item_entry_new ("GenericName",
							  _("Description"), TRUE, FALSE));
	} else if (g_strcmp0 (type, "Application") == 0) {
		entries = g_list_prepend (entries,
					  item_entry_new ("Comment",
							  _("Comment"), TRUE, FALSE));
		entries = g_list_prepend (entries,
					  item_entry_new ("Exec", 
							  _("Command"), FALSE, FALSE));
		entries = g_list_prepend (entries,
					  item_entry_new ("GenericName",
							  _("Description"), TRUE, FALSE));
	} else {
		/* we only handle launchers and links */

		/* ensure that we build an empty table with a dummy row at the end */
		goto build_table;
	}
	g_free (type);

build_table:
	label_size_group = g_object_get_data (G_OBJECT (box), "label-size-group");

	grid = build_grid (box, key_file, label_size_group, entries);
	g_list_free (entries);
	
	gtk_box_pack_start (GTK_BOX (box), grid, FALSE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (box));
}


static void
ditem_read_cb (GObject *source_object,
	       GAsyncResult *res,
	       gpointer user_data)
{
	GKeyFile *key_file;
	GtkWidget *box;
	gsize file_size;
	char *file_contents;

	box = GTK_WIDGET (user_data);
	
	if (g_file_load_contents_finish (G_FILE (source_object),
					 res,
					 &file_contents, &file_size,
					 NULL, NULL)) {
		key_file = g_key_file_new ();
		g_object_set_data_full (G_OBJECT (box), "keyfile", key_file, (GDestroyNotify)g_key_file_free);
		if (g_key_file_load_from_data (key_file, file_contents, file_size, 0, NULL)) {
			create_page (key_file, box);
		}
		g_free (file_contents);
		
	}
	g_object_unref (box);
}

static void
nemo_desktop_item_properties_create_begin (const char *uri,
                                               GtkWidget *box)
{
	GFile *location;

	location = g_file_new_for_uri (uri);
	g_object_set_data_full (G_OBJECT (box), "uri", g_strdup (uri), g_free);
	g_file_load_contents_async (location, NULL, ditem_read_cb, g_object_ref (box));
	g_object_unref (location);
}

GtkWidget *
nemo_desktop_item_properties_make_box (GtkSizeGroup *label_size_group,
                                           GList *files)
{
	NemoFileInfo *info;
	char *uri;
	GtkWidget *box;

	g_assert (nemo_desktop_item_properties_should_show (files));

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	g_object_set_data_full (G_OBJECT (box), "label-size-group",
				label_size_group, (GDestroyNotify) g_object_unref);

	info = NEMO_FILE_INFO (files->data);

	uri = nemo_file_info_get_uri (info);
	nemo_desktop_item_properties_create_begin (uri, box);
	g_free (uri);

	return box;
}

gboolean
nemo_desktop_item_properties_should_show (GList *files)
{
	NemoFileInfo *info;

	if (!files || files->next) {
		return FALSE;
	}

	info = NEMO_FILE_INFO (files->data);

	if (!nemo_file_info_is_mime_type (info, "application/x-desktop")) {
		return FALSE;
	}

	return TRUE;
}


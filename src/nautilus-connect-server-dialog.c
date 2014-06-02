/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Nautilus
 *
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2010 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "nautilus-connect-server-dialog.h"

#include <string.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-gtk-extensions.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "nautilus-window.h"

struct _NautilusConnectServerDialogDetails {
	GtkTreeView *view;
	GtkListStore *store;
	GtkWidget *primary_grid;

	GtkWidget *uri_entry;
	GtkWidget *error_label;

	GtkWidget *menu;
	GtkWidget *remove_menu_item;
	GtkWidget *clear_menu_item;

	char **supported;
};

enum {
	COLUMN_NAME,
	COLUMN_URI,
	NUM_COLUMNS
};


G_DEFINE_TYPE (NautilusConnectServerDialog, nautilus_connect_server_dialog,
	       GTK_TYPE_DIALOG)

GFile *
nautilus_connect_server_dialog_get_location (NautilusConnectServerDialog *dialog)
{
	const char *uri;
	GFile *file = NULL;

	uri = gtk_entry_get_text (GTK_ENTRY (dialog->details->uri_entry));
	if (uri != NULL && uri[0] != '\0') {
		file = g_file_new_for_commandline_arg (uri);
	}

	return file;
}

static gboolean
is_scheme_supported (NautilusConnectServerDialog *dialog,
		     const char                  *scheme)
{
	int i;

	if (dialog->details->supported == NULL) {
		return FALSE;
	}

	for (i = 0; dialog->details->supported[i] != NULL; i++) {
		if (strcmp (scheme, dialog->details->supported[i]) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
validate_uri (NautilusConnectServerDialog *dialog,
	      const char                  *uri,
	      GError                     **error)
{
	gboolean valid = FALSE;
	char *scheme;

	scheme = g_uri_parse_scheme (uri);
	if (scheme != NULL) {
		valid = is_scheme_supported (dialog, scheme);
		if (! valid) {
			g_set_error_literal (error,
					     G_IO_ERROR,
					     G_IO_ERROR_NOT_SUPPORTED,
					     _("This file server type is not recognized."));
		}
		g_free (scheme);
	} else {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_ARGUMENT,
				     _("This doesn't look like an address."));

	}

	return valid;
}

static void
on_uri_entry_clear (GtkEntry                    *entry,
		    NautilusConnectServerDialog *dialog)
{
	gtk_entry_set_text (entry, "");
}

static const char *
get_default_schema (NautilusConnectServerDialog *dialog)
{
	if (dialog->details->supported == NULL) {
		return NULL;
	}

	return dialog->details->supported[0];
}

static int
get_index (const char **names,
	   const char  *needle)
{
	int i;
	int index = G_MAXINT;
	for (i = 0; names[i] != NULL; i++) {
		if (strcmp (needle, names[i]) == 0) {
			index = i;
			break;
		}
	}

	return index;
}

static int
sort_supported (gconstpointer a,
		gconstpointer b,
		gpointer user_data)
{
	const char **preferred = user_data;
	const char *s_a = *((char **)a);
	const char *s_b = *((char **)b);
	int i_a;
	int i_b;

	i_a = get_index (preferred, s_a);
	i_b = get_index (preferred, s_b);

	if (i_b == i_a) {
		return 0;
	} else if (i_a > i_b) {
		return 1;
	} else {
		return -1;
	}
}

static void
populate_supported_list (NautilusConnectServerDialog *dialog)
{
	const char * const *supported;
	GPtrArray *good;
	int i;
	int j;
	const char * unsupported[] = { "file", "afc", "obex", "http", "trash", "burn", "computer", "archive", "recent", "localtest", NULL };
	const char * preferred[] = { "smb", "afp", "sftp", "ssh", "davs", "dav", "ftp", NULL };

	supported = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());
	if (supported == NULL) {
		return;
	}

	good = g_ptr_array_new ();
	for (i = 0; supported[i] != NULL; i++) {
		gboolean support = TRUE;
		for (j = 0; unsupported[j] != NULL; j++) {
			if (strcmp (supported[i], unsupported[j]) == 0) {
				support = FALSE;
				break;
			}
		}
		if (support) {
			g_ptr_array_add (good, g_strdup (supported[i]));
		}
	}
	g_ptr_array_sort_with_data (good, sort_supported, preferred);

	g_ptr_array_add (good, NULL);

	dialog->details->supported = (char **)g_ptr_array_free (good, FALSE);
}

static void
reset_example_label (NautilusConnectServerDialog *dialog)
{
	char *text;
	char *uri;
	const char *schema;

	schema = get_default_schema (dialog);
	if (schema != NULL) {
		uri = g_strdup_printf ("%s://foo.example.org", schema);
		/* Translators: %s is a URI of the form "smb://foo.example.com" */
		text = g_strdup_printf (_("For example, %s"), uri);
		g_free (uri);
	} else {
		text = g_strdup ("");
	}
	gtk_label_set_text (GTK_LABEL (dialog->details->error_label), text);
	g_free (text);
}

static void
check_uri_entry (NautilusConnectServerDialog *dialog)
{
	guint length;
	gboolean button_active = FALSE;
	gboolean icon_active = FALSE;
	const char *text = NULL;

	length = gtk_entry_get_text_length (GTK_ENTRY (dialog->details->uri_entry));
	if (length > 0) {
		GError *error = NULL;

		text = gtk_entry_get_text (GTK_ENTRY (dialog->details->uri_entry));
		button_active = validate_uri (dialog, text, &error);
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED)) {
			gtk_label_set_text (GTK_LABEL (dialog->details->error_label), error->message);
		} else {
			reset_example_label (dialog);
		}
		g_clear_error (&error);
		icon_active = TRUE;
	}

	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_OK, button_active);

	g_object_set (dialog->details->uri_entry,
		      "secondary-icon-name", icon_active ? ("edit-clear-symbolic") : NULL,
		      "secondary-icon-activatable", icon_active,
		      "secondary-icon-sensitive", icon_active,
		      NULL);
}

static void
on_uri_entry_changed (GtkEditable                 *editable,
		      NautilusConnectServerDialog *dialog)
{
	GtkTreeSelection *selection;

	/* if the uri is changed at all it isn't the selected on anymore */
	selection = gtk_tree_view_get_selection (dialog->details->view);
	gtk_tree_selection_unselect_all (selection);

	check_uri_entry (dialog);
}

static char *
get_selected (NautilusConnectServerDialog *dialog,
	      GtkTreeIter                 *iter_out)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	char *uri = NULL;

	selection = gtk_tree_view_get_selection (dialog->details->view);
	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (dialog->details->store),
				    &iter,
				    COLUMN_URI, &uri,
				    -1);
		if (iter_out) {
			*iter_out = iter;
		}
	}

	return uri;
}

static void
on_selection_changed (GtkTreeSelection            *selection,
		      NautilusConnectServerDialog *dialog)
{
	char *uri;

	uri = get_selected (dialog, NULL);
	if (uri != NULL) {
		g_signal_handlers_block_by_func (dialog->details->uri_entry, on_uri_entry_changed, dialog);
		gtk_entry_set_text (GTK_ENTRY (dialog->details->uri_entry), uri);
		g_signal_handlers_unblock_by_func (dialog->details->uri_entry, on_uri_entry_changed, dialog);
		check_uri_entry (dialog);
		g_free (uri);
		gtk_widget_set_sensitive (dialog->details->remove_menu_item, TRUE);
	} else {
		gtk_widget_set_sensitive (dialog->details->remove_menu_item, FALSE);
	}
}

static GBookmarkFile *
server_list_load (NautilusConnectServerDialog *dialog)
{
	GBookmarkFile *bookmarks;
	GError *error = NULL;
	char *datadir;
	char *filename;

	bookmarks = g_bookmark_file_new ();
	datadir = g_build_filename (g_get_user_config_dir (), "nautilus", NULL);
	filename = g_build_filename (datadir, "servers", NULL);
	g_mkdir_with_parents (datadir, 0700);
	g_free (datadir);
	g_bookmark_file_load_from_file (bookmarks,
					filename,
					&error);
	g_free (filename);
	if (error != NULL) {
		if (! g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
			/* only warn if the file exists */
			g_warning ("Unable to open server bookmarks: %s", error->message);
		}
		g_error_free (error);
		g_bookmark_file_free (bookmarks);
		bookmarks = NULL;
	}

	return bookmarks;
}

static void
server_list_save (NautilusConnectServerDialog *dialog,
		  GBookmarkFile               *bookmarks)
{
	char *filename;

	filename = g_build_filename (g_get_user_config_dir (), "nautilus", "servers", NULL);
	g_bookmark_file_to_file (bookmarks, filename, NULL);
}

static void
populate_server_list (NautilusConnectServerDialog *dialog)
{
	GBookmarkFile *bookmarks;
	GtkTreeIter iter;
	char **uris;
	int i;

	bookmarks = server_list_load (dialog);
	if (bookmarks == NULL) {
		return;
	}
	uris = g_bookmark_file_get_uris (bookmarks, NULL);
	if (uris != NULL) {
		for (i = 0; uris[i] != NULL; i++) {
			char *name;

			name = g_bookmark_file_get_title (bookmarks, uris[i], NULL);
			gtk_list_store_append (dialog->details->store, &iter);
			gtk_list_store_set (dialog->details->store, &iter,
					    COLUMN_URI, uris[i],
					    COLUMN_NAME, name,
					    -1);
			g_free (name);
		}
		g_strfreev (uris);
		gtk_widget_set_sensitive (dialog->details->clear_menu_item, TRUE);
	} else {
		gtk_widget_set_sensitive (dialog->details->clear_menu_item, FALSE);
	}

	g_bookmark_file_free (bookmarks);
}

static void
server_list_remove (NautilusConnectServerDialog *dialog,
		    const char                  *uri)
{
	GBookmarkFile *bookmarks;

	bookmarks = server_list_load (dialog);
	if (bookmarks == NULL) {
		return;
	}

	g_bookmark_file_remove_item (bookmarks, uri, NULL);
	server_list_save (dialog, bookmarks);
	g_bookmark_file_free (bookmarks);
}

static void
server_list_remove_all (NautilusConnectServerDialog *dialog)
{
	GBookmarkFile *bookmarks;

	bookmarks = g_bookmark_file_new ();
	if (bookmarks == NULL) {
		return;
	}
	server_list_save (dialog, bookmarks);
	g_bookmark_file_free (bookmarks);
}

static void
boldify_label (GtkLabel *label)
{
	PangoAttrList *attrs;
	attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
	gtk_label_set_attributes (label, attrs);
	pango_attr_list_unref (attrs);
}

static void
on_clear_item_activated (GtkMenuItem                 *item,
			 NautilusConnectServerDialog *dialog)
{
	server_list_remove_all (dialog);
	gtk_list_store_clear (dialog->details->store);
	gtk_widget_set_sensitive (dialog->details->clear_menu_item, FALSE);
}

static void
on_remove_item_activated (GtkMenuItem                 *item,
			  NautilusConnectServerDialog *dialog)
{
	char *uri;
	GtkTreeIter iter;

	uri = get_selected (dialog, &iter);
	if (uri != NULL) {
		server_list_remove (dialog, uri);
		gtk_list_store_remove (dialog->details->store, &iter);
		g_free (uri);
	}
}

static void
on_row_activated (GtkTreeView                 *tree_view,
		  GtkTreePath                 *path,
		  GtkTreeViewColumn           *column,
		  NautilusConnectServerDialog *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
}

static void
on_popup_menu_detach (GtkWidget *attach_widget,
		      GtkMenu   *menu)
{
	NautilusConnectServerDialog *dialog = NAUTILUS_CONNECT_SERVER_DIALOG (attach_widget);

	dialog->details->menu = NULL;
	dialog->details->remove_menu_item = NULL;
	dialog->details->clear_menu_item = NULL;
}

static void
create_popup_menu (NautilusConnectServerDialog *dialog)
{
	GtkWidget *menu;
	GtkWidget *item;

	if (dialog->details->menu != NULL) {
		return;
	}

	menu = gtk_menu_new ();
	dialog->details->menu = menu;
	gtk_menu_attach_to_widget (GTK_MENU (menu),
			           GTK_WIDGET (dialog),
			           on_popup_menu_detach);

	item = gtk_menu_item_new_with_mnemonic (_("_Remove"));
	dialog->details->remove_menu_item = item;
	g_signal_connect (item, "activate",
			  G_CALLBACK (on_remove_item_activated), dialog);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);

	eel_gtk_menu_append_separator (GTK_MENU (menu));

	item = gtk_menu_item_new_with_mnemonic (_("_Clear All"));
	dialog->details->clear_menu_item = item;
	g_signal_connect (item, "activate",
			  G_CALLBACK (on_clear_item_activated), dialog);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	gtk_widget_show (item);
}

static void
history_popup_menu (NautilusConnectServerDialog *dialog)
{
	create_popup_menu (dialog);
	eel_pop_up_context_menu (GTK_MENU (dialog->details->menu), NULL);
}

static gboolean
on_popup_menu (GtkWidget                   *widget,
	       NautilusConnectServerDialog *dialog)
{
	history_popup_menu (dialog);
	return TRUE;
}

static void
nautilus_connect_server_dialog_init (NautilusConnectServerDialog *dialog)
{
	GtkWidget *label;
	GtkWidget *sw;
	GtkWidget *view;
	GtkWidget *box;
	GtkWidget *content_area;
	GtkWidget *grid;
	int row;
	GtkCellRenderer *cell;
	GtkTreeSelection *selection;
	GtkListStore *store;

	dialog->details = G_TYPE_INSTANCE_GET_PRIVATE (dialog, NAUTILUS_TYPE_CONNECT_SERVER_DIALOG,
						       NautilusConnectServerDialogDetails);

	populate_supported_list (dialog);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	/* set dialog properties */
	gtk_window_set_title (GTK_WINDOW (dialog), _("Connect to Server"));
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);

	grid = gtk_grid_new ();
	gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_widget_set_margin_bottom (grid, 12);
	gtk_container_set_border_width (GTK_CONTAINER (grid), 5);
	gtk_container_add (GTK_CONTAINER (content_area), grid);
	gtk_widget_show (grid);

	dialog->details->primary_grid = grid;

	row = 0;

	label = gtk_label_new_with_mnemonic (_("_Server Address"));
	boldify_label (GTK_LABEL (label));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_grid_attach (GTK_GRID (grid), label, 0, row++, 1, 1);
	gtk_widget_show (label);

	dialog->details->uri_entry = gtk_entry_new ();

	gtk_widget_set_hexpand (dialog->details->uri_entry, TRUE);
	gtk_entry_set_activates_default (GTK_ENTRY (dialog->details->uri_entry), TRUE);
	gtk_grid_attach (GTK_GRID (grid), dialog->details->uri_entry, 0, row++, 1, 1);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->details->uri_entry);
	gtk_widget_show (dialog->details->uri_entry);

	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_grid_attach (GTK_GRID (grid), label, 0, row++, 1, 1);
	gtk_widget_show (label);
	dialog->details->error_label = label;
	reset_example_label (dialog);
	gtk_widget_set_margin_bottom (label, 12);
	gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");

	label = gtk_label_new_with_mnemonic (_("_Recent Servers"));
	boldify_label (GTK_LABEL (label));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_grid_attach (GTK_GRID (grid), label, 0, row++, 1, 1);
	gtk_widget_show (label);

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand (box, TRUE);
	gtk_widget_set_vexpand (box, TRUE);
	gtk_grid_attach (GTK_GRID (grid), box, 0, row++, 1, 1);
	gtk_widget_show (box);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (sw, 400, 150);
	gtk_widget_show (sw);
	gtk_widget_set_hexpand (sw, TRUE);
	gtk_widget_set_vexpand (sw, TRUE);
	gtk_container_add (GTK_CONTAINER (box), sw);

	view = gtk_tree_view_new ();
	gtk_widget_show (view);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);
	gtk_container_add (GTK_CONTAINER (sw), view);

	g_signal_connect (view, "row-activated",
			  G_CALLBACK (on_row_activated),
			  dialog);
	g_signal_connect (view, "popup-menu",
			  G_CALLBACK (on_popup_menu),
			  dialog);

	store = gtk_list_store_new (NUM_COLUMNS,
				    G_TYPE_STRING,
				    G_TYPE_STRING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (view),
				 GTK_TREE_MODEL (store));
	g_object_unref (store);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (on_selection_changed),
			  dialog);

	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
						     -1,
						     NULL,
						     cell,
						     "text", COLUMN_NAME,
						     NULL);
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
						     -1,
						     NULL,
						     cell,
						     "text", COLUMN_URI,
						     NULL);
	dialog->details->view = GTK_TREE_VIEW (view);
	dialog->details->store = store;

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_Cancel"),
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("C_onnect"),
			       GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 GTK_RESPONSE_OK);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
					   GTK_RESPONSE_OK,
					   FALSE);

	g_signal_connect (dialog->details->uri_entry, "changed",
			  G_CALLBACK (on_uri_entry_changed),
			  dialog);
	g_signal_connect (dialog->details->uri_entry, "icon-release",
			  G_CALLBACK (on_uri_entry_clear),
			  dialog);

	create_popup_menu (dialog);
	populate_server_list (dialog);
}

static void
nautilus_connect_server_dialog_finalize (GObject *object)
{
	NautilusConnectServerDialog *dialog;

	dialog = NAUTILUS_CONNECT_SERVER_DIALOG (object);

	g_strfreev (dialog->details->supported);

	G_OBJECT_CLASS (nautilus_connect_server_dialog_parent_class)->finalize (object);
}

static void
nautilus_connect_server_dialog_class_init (NautilusConnectServerDialogClass *class)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (class);
	oclass->finalize = nautilus_connect_server_dialog_finalize;

	g_type_class_add_private (class, sizeof (NautilusConnectServerDialogDetails));
}

GtkWidget *
nautilus_connect_server_dialog_new (NautilusWindow *window)
{
	GtkWidget *dialog;

	dialog = gtk_widget_new (NAUTILUS_TYPE_CONNECT_SERVER_DIALOG,
				 "use-header-bar", TRUE,
				 NULL);

	if (window) {
		gtk_window_set_screen (GTK_WINDOW (dialog),
				       gtk_window_get_screen (GTK_WINDOW (window)));
	}

	return dialog;
}

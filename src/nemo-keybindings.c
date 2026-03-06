/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-keybindings.c - Configurable keyboard shortcuts for Nemo.
 *
 * Copyright (C) 2026 Nemo contributors
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 */

#include <config.h>
#include "nemo-keybindings.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

GSettings *nemo_keybinding_settings = NULL;

/*
 * Master table of all configurable keybindings.
 *
 * settings_key: GSettings key in org.nemo.keybindings
 * accel_path: GtkAccelMap path (<Actions>/<group>/<action>)
 * description: shown in the preferences UI
 * category: grouping for the preferences UI
 * default_accel: default GTK accelerator string
 */
const NemoKeybindingEntry nemo_keybinding_entries[] = {
	/* Navigation — accel-map entries */
	{ "go-back",               "<Actions>/ShellActions/Back",               N_("Go Back"),                    N_("Navigation"), "<Alt>Left",              NULL, NULL },
	{ "go-forward",            "<Actions>/ShellActions/Forward",            N_("Go Forward"),                 N_("Navigation"), "<Alt>Right",             NULL, NULL },
	{ "go-up",                 "<Actions>/ShellActions/Up",                 N_("Go to Parent Folder"),        N_("Navigation"), "<Alt>Up",                NULL, NULL },
	{ "go-home",               "<Actions>/ShellActions/Home",               N_("Go Home"),                    N_("Navigation"), "<Alt>Home",              NULL, NULL },
	{ "edit-location",         "<Actions>/ShellActions/Edit Location",      N_("Toggle Location Entry"),      N_("Navigation"), "<Control>l",             NULL, NULL },
	{ "reload",                "<Actions>/ShellActions/Reload",             N_("Reload"),                     N_("Navigation"), "<Control>r",             NULL, NULL },
	{ "search",                "<Actions>/ShellActions/Search",             N_("Search"),                     N_("Navigation"), "<Control>f",             NULL, NULL },

	/* Navigation — binding-set entries (unified from nemo-window.c) */
	{ "reload-alt",            NULL,                                        N_("Reload (alternate)"),         N_("Navigation"), "F5",                     "NemoWindow", "reload" },
	{ "go-up-alt",             NULL,                                        N_("Go Up (alternate)"),          N_("Navigation"), "BackSpace",              "NemoWindow", "go-up" },

	/* Window */
	{ "new-window",            "<Actions>/ShellActions/New Window",         N_("New Window"),                 N_("Window"), "<Control>n",                 NULL, NULL },
	{ "close-window",          "<Actions>/ShellActions/Close",              N_("Close Window/Tab"),           N_("Window"), "<Control>w",                 NULL, NULL },
	{ "close-all-windows",     "<Actions>/ShellActions/Close All Windows",  N_("Close All Windows"),          N_("Window"), "<Control>q",                NULL, NULL },
	{ "show-hidden-files",     "<Actions>/ShellActions/Show Hidden Files",  N_("Show Hidden Files"),          N_("Window"), "<Control>h",                NULL, NULL },
	{ "show-sidebar",          "<Actions>/ShellActions/Show Hide Sidebar",  N_("Toggle Sidebar"),             N_("Window"), "F9",                        NULL, NULL },
	{ "split-view",            "<Actions>/ShellActions/Show Hide Extra Pane", N_("Toggle Split View"),        N_("Window"), "F3",                        NULL, NULL },
	{ "switch-pane",           "<Actions>/ShellActions/SplitViewNextPane",  N_("Switch to Other Pane"),       N_("Window"), "F6",                        NULL, NULL },
	{ "same-location-pane",    "<Actions>/ShellActions/SplitViewSameLocation", N_("Same Location as Other Pane"), N_("Window"), "<Alt>s",                NULL, NULL },

	/* Tabs */
	{ "new-tab",               "<Actions>/ShellActions/New Tab",            N_("New Tab"),                    N_("Tabs"), "<Control>t",                   NULL, NULL },
	{ "previous-tab",          "<Actions>/ShellActions/TabsPrevious",       N_("Previous Tab"),               N_("Tabs"), "<Control>Page_Up",             NULL, NULL },
	{ "next-tab",              "<Actions>/ShellActions/TabsNext",           N_("Next Tab"),                   N_("Tabs"), "<Control>Page_Down",            NULL, NULL },
	{ "move-tab-left",         "<Actions>/ShellActions/TabsMoveLeft",       N_("Move Tab Left"),              N_("Tabs"), "<Shift><Control>Page_Up",      NULL, NULL },
	{ "move-tab-right",        "<Actions>/ShellActions/TabsMoveRight",      N_("Move Tab Right"),             N_("Tabs"), "<Shift><Control>Page_Down",    NULL, NULL },

	/* View */
	{ "zoom-in",               "<Actions>/ShellActions/Zoom In",            N_("Zoom In"),                    N_("View"), "<Control>plus",                NULL, NULL },
	{ "zoom-out",              "<Actions>/ShellActions/Zoom Out",           N_("Zoom Out"),                   N_("View"), "<Control>minus",               NULL, NULL },
	{ "zoom-normal",           "<Actions>/ShellActions/Zoom Normal",        N_("Normal Size"),                N_("View"), "<Control>0",                   NULL, NULL },
	{ "icon-view",             "<Actions>/ShellActions/IconView",           N_("Icon View"),                  N_("View"), "<Control>1",                   NULL, NULL },
	{ "list-view",             "<Actions>/ShellActions/ListView",           N_("List View"),                  N_("View"), "<Control>2",                   NULL, NULL },
	{ "compact-view",          "<Actions>/ShellActions/CompactView",        N_("Compact View"),               N_("View"), "<Control>3",                   NULL, NULL },

	/* Bookmarks */
	{ "add-bookmark",          "<Actions>/ShellActions/Add Bookmark",       N_("Add Bookmark"),               N_("Bookmarks"), "<Control>d",              NULL, NULL },
	{ "edit-bookmarks",        "<Actions>/ShellActions/Edit Bookmarks",     N_("Edit Bookmarks"),             N_("Bookmarks"), "<Control>b",              NULL, NULL },
	{ "bookmark-picker",       "<Actions>/ShellActions/Bookmark Picker",    N_("Bookmark/Disk Picker"),       N_("Bookmarks"), "<Alt>F1",                 NULL, NULL },
	{ "bookmark-picker-other", "<Actions>/ShellActions/Bookmark Picker Other Pane", N_("Bookmark/Disk Picker (Other Pane)"), N_("Bookmarks"), "<Alt>F2",     NULL, NULL },

	/* File Operations — accel-map entries */
	{ "open",                  "<Actions>/DirViewActions/Open",             N_("Open"),                       N_("File Operations"), "<Control>o",        NULL, NULL },
	{ "open-alternate",        "<Actions>/DirViewActions/OpenAlternate",    N_("Open in New Window"),         N_("File Operations"), "<Control><Shift>o", NULL, NULL },
	{ "open-in-new-tab",       "<Actions>/DirViewActions/OpenInNewTab",     N_("Open in New Tab"),            N_("File Operations"), "<Control><Shift>t", NULL, NULL },
	{ "open-in-terminal",      "<Actions>/DirViewActions/OpenInTerminal",   N_("Open in Terminal"),           N_("File Operations"), "<Shift>F4",         NULL, NULL },
	{ "open-containing-folder","<Actions>/DirViewActions/OpenContainingFolder", N_("Open Containing Folder"), N_("File Operations"), "<Control><Alt>o",   NULL, NULL },
	{ "properties",            "<Actions>/DirViewActions/Properties",       N_("Properties"),                 N_("File Operations"), "<Alt>Return",       NULL, NULL },
	{ "new-folder",            "<Actions>/DirViewActions/New Folder",       N_("New Folder"),                 N_("File Operations"), "<Control><Shift>n", NULL, NULL },
	{ "rename",                "<Actions>/DirViewActions/Rename",           N_("Rename"),                     N_("File Operations"), "F2",                NULL, NULL },
	{ "create-link",           "<Actions>/DirViewActions/Create Link",      N_("Make Symbolic Link"),         N_("File Operations"), "<Control>m",        NULL, NULL },
	{ "pin-file",              "<Actions>/DirViewActions/Pin File",         N_("Pin/Unpin File"),             N_("File Operations"), "<Control><Shift>d", NULL, NULL },
	{ "copy-to-other-pane",    "<Actions>/DirViewActions/Copy to next pane", N_("Copy to Other Pane"),         N_("File Operations"), "",                  NULL, NULL },
	{ "move-to-other-pane",    "<Actions>/DirViewActions/Move to next pane", N_("Move to Other Pane"),         N_("File Operations"), "",                  NULL, NULL },

	/* File Operations — binding-set entries (unified from nemo-view.c) */
	{ "trash",                 NULL,                                        N_("Move to Trash"),              N_("File Operations"), "Delete",             "NemoView", "trash" },
	{ "delete-permanently",    NULL,                                        N_("Delete Permanently"),         N_("File Operations"), "<Shift>Delete",      "NemoView", "delete" },

	/* Clipboard */
	{ "cut",                   "<Actions>/DirViewActions/Cut",              N_("Cut"),                        N_("Clipboard"), "<Control>x",               NULL, NULL },
	{ "copy",                  "<Actions>/DirViewActions/Copy",             N_("Copy"),                       N_("Clipboard"), "<Control>c",               NULL, NULL },
	{ "paste",                 "<Actions>/DirViewActions/Paste",            N_("Paste"),                      N_("Clipboard"), "<Control>v",               NULL, NULL },

	/* Selection */
	{ "select-all",            "<Actions>/DirViewActions/Select All",       N_("Select All"),                 N_("Selection"), "<Control>a",               NULL, NULL },
	{ "select-pattern",        "<Actions>/DirViewActions/Select Pattern",   N_("Select Items Matching..."),   N_("Selection"), "<Control>s",               NULL, NULL },
	{ "invert-selection",      "<Actions>/DirViewActions/Invert Selection", N_("Invert Selection"),           N_("Selection"), "<Control><Shift>i",        NULL, NULL },

	/* Undo/Redo */
	{ "undo",                  "<Actions>/DirViewActions/Undo",             N_("Undo"),                       N_("Edit"), "<Control>z",                    NULL, NULL },
	{ "redo",                  "<Actions>/DirViewActions/Redo",             N_("Redo"),                       N_("Edit"), "<Control>y",                    NULL, NULL },

	/* Help */
	{ "show-help",             "<Actions>/ShellActions/NemoHelp",           N_("Help"),                       N_("Help"), "F1",                            NULL, NULL },
	{ "show-shortcuts",        "<Actions>/ShellActions/NemoShortcuts",      N_("Keyboard Shortcuts"),         N_("Help"), "<Control>F1",                   NULL, NULL },
};

const gint nemo_keybinding_entries_count = G_N_ELEMENTS (nemo_keybinding_entries);

/*
 * Apply a single keybinding from GSettings.
 *
 * Handles both mechanisms:
 *   - accel-map entries (accel_path != NULL): modify GtkAccelMap
 *   - binding-set entries (binding_set_name != NULL): modify GtkBindingSet
 */
static void
apply_keybinding (const NemoKeybindingEntry *entry)
{
	g_autofree gchar *accel_string = NULL;
	guint key = 0;
	GdkModifierType mods = 0;
	guint default_key = 0;
	GdkModifierType default_mods = 0;

	accel_string = g_settings_get_string (nemo_keybinding_settings,
	                                      entry->settings_key);

	/* Parse the default so we can remove the old binding-set entry */
	gtk_accelerator_parse (entry->default_accel, &default_key, &default_mods);

	if (entry->binding_set_name != NULL) {
		/* --- Binding-set entry --- */
		GtkBindingSet *binding_set;

		binding_set = gtk_binding_set_find (entry->binding_set_name);
		if (binding_set == NULL) {
			return;
		}

		/* Always remove the default binding first */
		gtk_binding_entry_remove (binding_set, default_key, default_mods);

		if (accel_string == NULL || accel_string[0] == '\0') {
			/* Disabled — just leave it removed */
			return;
		}

		gtk_accelerator_parse (accel_string, &key, &mods);

		if (key == 0 && mods == 0) {
			g_warning ("Failed to parse accelerator '%s' for %s",
			           accel_string, entry->settings_key);
			return;
		}

		/* Remove the new key too in case it was already bound */
		gtk_binding_entry_remove (binding_set, key, mods);

		gtk_binding_entry_add_signal (binding_set, key, mods,
		                              entry->signal_name, 0);
		return;
	}

	/* --- Accel-map entry --- */

	if (accel_string == NULL || accel_string[0] == '\0') {
		/* Disabled binding: remove any existing accelerator */
		gtk_accel_map_change_entry (entry->accel_path, 0, 0, TRUE);
		return;
	}

	/* If the value matches the default, don't touch the accel map —
	 * let the hardcoded GtkActionEntry handle it normally. This avoids
	 * corrupting the accel map by pre-registering paths before action
	 * groups exist. */
	if (g_strcmp0 (accel_string, entry->default_accel) == 0) {
		return;
	}

	gtk_accelerator_parse (accel_string, &key, &mods);

	if (key == 0 && mods == 0) {
		g_warning ("Failed to parse accelerator '%s' for %s",
		           accel_string, entry->settings_key);
		return;
	}

	gtk_accel_map_change_entry (entry->accel_path, key, mods, TRUE);
}

/*
 * Apply all keybindings from GSettings.
 */
void
nemo_keybindings_apply_all (void)
{
	gint i;

	if (nemo_keybinding_settings == NULL) {
		return;
	}

	for (i = 0; i < nemo_keybinding_entries_count; i++) {
		apply_keybinding (&nemo_keybinding_entries[i]);
	}
}

/*
 * Set a specific keybinding in GSettings and apply it immediately.
 */
void
nemo_keybindings_set_for_action (const gchar *settings_key,
                                 const gchar *accel_string)
{
	g_settings_set_string (nemo_keybinding_settings,
	                       settings_key,
	                       accel_string ? accel_string : "");
}

/*
 * Handle GSettings "changed" signal for keybindings.
 */
static void
on_keybinding_changed (GSettings   *settings,
                       const gchar *key,
                       gpointer     user_data)
{
	gint i;

	for (i = 0; i < nemo_keybinding_entries_count; i++) {
		if (g_strcmp0 (nemo_keybinding_entries[i].settings_key, key) == 0) {
			apply_keybinding (&nemo_keybinding_entries[i]);
			break;
		}
	}
}

/*
 * Initialize the keybinding settings and apply all overrides.
 */
void
nemo_keybindings_init (void)
{
	if (nemo_keybinding_settings != NULL) {
		return;
	}

	nemo_keybinding_settings = g_settings_new ("org.nemo.keybindings");

	g_signal_connect (nemo_keybinding_settings, "changed",
	                  G_CALLBACK (on_keybinding_changed), NULL);

	nemo_keybindings_apply_all ();
}

/* --- Preferences UI --- */

enum {
	COL_CATEGORY,
	COL_DESCRIPTION,
	COL_ACCEL_KEY,
	COL_ACCEL_MODS,
	COL_SETTINGS_KEY,
	COL_ENTRY_INDEX,
	NUM_COLS
};

static void
on_accel_edited (GtkCellRendererAccel *renderer,
                 gchar               *path_string,
                 guint                accel_key,
                 GdkModifierType      accel_mods,
                 guint                hardware_keycode,
                 gpointer             user_data)
{
	GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (user_data);
	GtkTreeModel *child_model = gtk_tree_model_filter_get_model (filter);
	GtkTreeIter filter_iter, child_iter;
	gchar *settings_key;
	gchar *accel_string;

	if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (filter),
	                                          &filter_iter, path_string)) {
		return;
	}

	gtk_tree_model_filter_convert_iter_to_child_iter (filter, &child_iter,
	                                                  &filter_iter);

	gtk_tree_model_get (child_model, &child_iter,
	                    COL_SETTINGS_KEY, &settings_key, -1);

	accel_string = gtk_accelerator_name (accel_key, accel_mods);

	g_settings_set_string (nemo_keybinding_settings, settings_key, accel_string);

	gtk_tree_store_set (GTK_TREE_STORE (child_model), &child_iter,
	                    COL_ACCEL_KEY, accel_key,
	                    COL_ACCEL_MODS, accel_mods,
	                    -1);

	g_free (accel_string);
	g_free (settings_key);
}

static void
on_accel_cleared (GtkCellRendererAccel *renderer,
                  gchar               *path_string,
                  gpointer             user_data)
{
	GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (user_data);
	GtkTreeModel *child_model = gtk_tree_model_filter_get_model (filter);
	GtkTreeIter filter_iter, child_iter;
	gchar *settings_key;

	if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (filter),
	                                          &filter_iter, path_string)) {
		return;
	}

	gtk_tree_model_filter_convert_iter_to_child_iter (filter, &child_iter,
	                                                  &filter_iter);

	gtk_tree_model_get (child_model, &child_iter,
	                    COL_SETTINGS_KEY, &settings_key, -1);

	g_settings_set_string (nemo_keybinding_settings, settings_key, "");

	gtk_tree_store_set (GTK_TREE_STORE (child_model), &child_iter,
	                    COL_ACCEL_KEY, 0,
	                    COL_ACCEL_MODS, (GdkModifierType) 0,
	                    -1);

	g_free (settings_key);
}

static void
on_reset_all_clicked (GtkButton *button,
                      gpointer   user_data)
{
	GtkTreeStore *store = GTK_TREE_STORE (user_data);
	GtkTreeIter iter, child;
	gint i;

	/* Reset all settings to defaults */
	for (i = 0; i < nemo_keybinding_entries_count; i++) {
		g_settings_reset (nemo_keybinding_settings,
		                  nemo_keybinding_entries[i].settings_key);
	}

	/* Refresh the tree store */
	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter)) {
		return;
	}

	do {
		if (gtk_tree_model_iter_children (GTK_TREE_MODEL (store), &child, &iter)) {
			do {
				gint idx;
				guint key;
				GdkModifierType mods;

				gtk_tree_model_get (GTK_TREE_MODEL (store), &child,
				                    COL_ENTRY_INDEX, &idx, -1);

				if (idx >= 0 && idx < nemo_keybinding_entries_count) {
					gtk_accelerator_parse (nemo_keybinding_entries[idx].default_accel,
					                       &key, &mods);
					gtk_tree_store_set (store, &child,
					                    COL_ACCEL_KEY, key,
					                    COL_ACCEL_MODS, mods,
					                    -1);
				}
			} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &child));
		}
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
}

/*
 * Case-insensitive substring match (simple fuzzy).
 */
static gboolean
string_contains_ci (const gchar *haystack, const gchar *needle)
{
	g_autofree gchar *h = g_utf8_strdown (haystack, -1);
	g_autofree gchar *n = g_utf8_strdown (needle, -1);

	return strstr (h, n) != NULL;
}

/*
 * Filter visible function: show a row if the search text matches
 * either the action description or the accelerator label.
 * Category (parent) rows are visible if any child matches.
 */
static gboolean
filter_visible_func (GtkTreeModel *model,
                     GtkTreeIter  *iter,
                     gpointer      user_data)
{
	GtkSearchEntry *search_entry = GTK_SEARCH_ENTRY (user_data);
	const gchar *search_text;
	gchar *description = NULL;
	guint key = 0;
	GdkModifierType mods = 0;
	gint idx;
	gboolean visible = TRUE;

	search_text = gtk_entry_get_text (GTK_ENTRY (search_entry));

	/* If search is empty, show everything */
	if (search_text == NULL || search_text[0] == '\0') {
		return TRUE;
	}

	gtk_tree_model_get (model, iter,
	                    COL_DESCRIPTION, &description,
	                    COL_ACCEL_KEY, &key,
	                    COL_ACCEL_MODS, &mods,
	                    COL_ENTRY_INDEX, &idx,
	                    -1);

	if (idx == -1) {
		/* Category row: visible if any child matches */
		GtkTreeIter child;
		visible = FALSE;

		if (gtk_tree_model_iter_children (model, &child, iter)) {
			do {
				if (filter_visible_func (model, &child, user_data)) {
					visible = TRUE;
					break;
				}
			} while (gtk_tree_model_iter_next (model, &child));
		}
	} else {
		visible = FALSE;

		/* Match against description */
		if (description && string_contains_ci (description, search_text)) {
			visible = TRUE;
		}

		/* Match against accelerator label */
		if (!visible && key != 0) {
			gchar *accel_label = gtk_accelerator_get_label (key, mods);
			if (accel_label && string_contains_ci (accel_label, search_text)) {
				visible = TRUE;
			}
			g_free (accel_label);
		}

		/* Also match against the raw accelerator name (e.g. "<Control>w") */
		if (!visible && key != 0) {
			gchar *accel_name = gtk_accelerator_name (key, mods);
			if (accel_name && string_contains_ci (accel_name, search_text)) {
				visible = TRUE;
			}
			g_free (accel_name);
		}
	}

	g_free (description);

	return visible;
}

static void
on_search_changed (GtkSearchEntry *entry,
                   gpointer        user_data)
{
	GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER (user_data);
	GtkTreeView *tree_view;

	gtk_tree_model_filter_refilter (filter);

	/* Re-expand all after filtering */
	tree_view = g_object_get_data (G_OBJECT (filter), "tree-view");
	if (tree_view) {
		gtk_tree_view_expand_all (tree_view);
	}
}

static void
accel_cell_data_func (GtkTreeViewColumn *column,
                     GtkCellRenderer   *renderer,
                     GtkTreeModel      *model,
                     GtkTreeIter       *iter,
                     gpointer           user_data)
{
	gint idx;

	gtk_tree_model_get (model, iter, COL_ENTRY_INDEX, &idx, -1);

	g_object_set (renderer, "visible", idx >= 0, NULL);
}

/*
 * Create the keybinding editor widget for the preferences dialog.
 */
GtkWidget *
nemo_keybindings_create_editor (void)
{
	GtkWidget *vbox;
	GtkWidget *search_entry;
	GtkWidget *scrolled;
	GtkWidget *tree_view;
	GtkWidget *button_box;
	GtkWidget *reset_button;
	GtkTreeStore *store;
	GtkTreeModel *filter_model;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeIter entry_iter;
	GHashTable *category_iters;
	gint i;

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);

	/* Search entry */
	search_entry = gtk_search_entry_new ();
	gtk_entry_set_placeholder_text (GTK_ENTRY (search_entry),
	                                _("Filter shortcuts…"));
	gtk_box_pack_start (GTK_BOX (vbox), search_entry, FALSE, FALSE, 0);

	/* Tree store: category, description, accel_key, accel_mods, settings_key, entry_index */
	store = gtk_tree_store_new (NUM_COLS,
	                            G_TYPE_STRING,    /* category */
	                            G_TYPE_STRING,    /* description */
	                            G_TYPE_UINT,      /* accel key */
	                            G_TYPE_UINT,      /* accel mods */
	                            G_TYPE_STRING,    /* settings key */
	                            G_TYPE_INT);      /* entry index */

	/* Build the tree grouped by category */
	category_iters = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

	for (i = 0; i < nemo_keybinding_entries_count; i++) {
		const NemoKeybindingEntry *entry = &nemo_keybinding_entries[i];
		GtkTreeIter *cat_iter;
		guint key = 0;
		GdkModifierType mods = 0;
		g_autofree gchar *accel_string = NULL;

		/* Get or create category row */
		cat_iter = g_hash_table_lookup (category_iters, entry->category);
		if (cat_iter == NULL) {
			cat_iter = g_new0 (GtkTreeIter, 1);
			gtk_tree_store_append (store, cat_iter, NULL);
			gtk_tree_store_set (store, cat_iter,
			                    COL_CATEGORY, _(entry->category),
			                    COL_DESCRIPTION, _(entry->category),
			                    COL_ACCEL_KEY, 0,
			                    COL_ACCEL_MODS, 0,
			                    COL_SETTINGS_KEY, "",
			                    COL_ENTRY_INDEX, -1,
			                    -1);
			g_hash_table_insert (category_iters,
			                     (gpointer) entry->category,
			                     cat_iter);
		}

		/* Read current accel from GSettings */
		accel_string = g_settings_get_string (nemo_keybinding_settings,
		                                      entry->settings_key);
		if (accel_string && accel_string[0] != '\0') {
			gtk_accelerator_parse (accel_string, &key, &mods);
		}

		gtk_tree_store_append (store, &entry_iter, cat_iter);
		gtk_tree_store_set (store, &entry_iter,
		                    COL_CATEGORY, _(entry->category),
		                    COL_DESCRIPTION, _(entry->description),
		                    COL_ACCEL_KEY, key,
		                    COL_ACCEL_MODS, mods,
		                    COL_SETTINGS_KEY, entry->settings_key,
		                    COL_ENTRY_INDEX, i,
		                    -1);
	}

	g_hash_table_destroy (category_iters);

	/* Wrap store in a filter model */
	filter_model = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
	gtk_tree_model_filter_set_visible_func (
		GTK_TREE_MODEL_FILTER (filter_model),
		filter_visible_func, search_entry, NULL);

	/* Tree view uses the filter model */
	tree_view = gtk_tree_view_new_with_model (filter_model);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), TRUE);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (tree_view), FALSE);

	/* Connect search to filter */
	g_object_set_data (G_OBJECT (filter_model), "tree-view", tree_view);
	g_signal_connect (search_entry, "search-changed",
	                  G_CALLBACK (on_search_changed), filter_model);

	/* Action column */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (
		_("Action"), renderer,
		"text", COL_DESCRIPTION,
		NULL);
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

	/* Shortcut column with editable accelerator cells */
	renderer = gtk_cell_renderer_accel_new ();
	g_object_set (renderer,
	              "editable", TRUE,
	              "accel-mode", GTK_CELL_RENDERER_ACCEL_MODE_GTK,
	              NULL);

	g_signal_connect (renderer, "accel-edited",
	                  G_CALLBACK (on_accel_edited), filter_model);
	g_signal_connect (renderer, "accel-cleared",
	                  G_CALLBACK (on_accel_cleared), filter_model);

	column = gtk_tree_view_column_new_with_attributes (
		_("Shortcut"), renderer,
		"accel-key", COL_ACCEL_KEY,
		"accel-mods", COL_ACCEL_MODS,
		NULL);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
	                                         accel_cell_data_func,
	                                         NULL, NULL);
	gtk_tree_view_column_set_min_width (column, 200);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

	/* Expand all categories */
	gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

	/* Scrolled window */
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
	                                GTK_POLICY_NEVER,
	                                GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
	                                     GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (scrolled), tree_view);
	gtk_box_pack_start (GTK_BOX (vbox), scrolled, TRUE, TRUE, 0);

	/* Reset button */
	button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	reset_button = gtk_button_new_with_label (_("Reset All to Defaults"));
	g_signal_connect (reset_button, "clicked",
	                  G_CALLBACK (on_reset_all_clicked), store);
	gtk_box_pack_end (GTK_BOX (button_box), reset_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), button_box, FALSE, FALSE, 0);

	gtk_widget_show_all (vbox);

	g_object_unref (filter_model);
	g_object_unref (store);

	return vbox;
}

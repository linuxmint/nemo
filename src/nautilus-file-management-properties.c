/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-management-properties.c - Functions to create and show the nautilus preference dialog.

   Copyright (C) 2002 Jan Arne Petersen

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
   see <http://www.gnu.org/licenses/>.

   Authors: Jan Arne Petersen <jpetersen@uni-bonn.de>
*/

#include <config.h>

#include "nautilus-file-management-properties.h"

#include <string.h>
#include <time.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <glib/gi18n.h>

#include <eel/eel-glib-extensions.h>

#include <libnautilus-private/nautilus-column-chooser.h>
#include <libnautilus-private/nautilus-column-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-module.h>

/* string enum preferences */
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_DEFAULT_VIEW_WIDGET "default_view_combobox"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_ICON_VIEW_ZOOM_WIDGET "icon_view_zoom_combobox"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_LIST_VIEW_ZOOM_WIDGET "list_view_zoom_combobox"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SORT_ORDER_WIDGET "sort_order_combobox"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_FILES_WIDGET "preview_image_combobox"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_FOLDER_WIDGET "preview_folder_combobox"

/* bool preferences */
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_FOLDERS_FIRST_WIDGET "sort_folders_first_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_LIST_VIEW_USE_TREE_WIDGET "use_tree_view_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TRASH_CONFIRM_WIDGET "trash_confirm_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TRASH_DELETE_WIDGET "trash_delete_checkbutton"
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SHOW_HIDDEN_WIDGET "hidden_files_checkbutton"

/* int enums */
#define NAUTILUS_FILE_MANAGEMENT_PROPERTIES_THUMBNAIL_LIMIT_WIDGET "preview_image_size_combobox"

static const char * const default_view_values[] = {
	"icon-view",
	"list-view",
	NULL
};

static const char * const zoom_values[] = {
	"smallest",
	"smaller",
	"small",
	"standard",
	"large",
	"larger",
	"largest",
	NULL
};

static const char * const sort_order_values[] = {
	"name",
	"size",
	"type",
	"mtime",
	"atime",
	"trash-time",
	NULL
};

static const char * const preview_values[] = {
	"always",
	"local-only",
	"never",
	NULL
};

static const char * const click_behavior_components[] = {
	"single_click_radiobutton",
	"double_click_radiobutton",
	NULL
};

static const char * const click_behavior_values[] = {
	"single",
	"double",
	NULL
};

static const char * const executable_text_components[] = {
	"scripts_execute_radiobutton",
	"scripts_view_radiobutton",
	"scripts_confirm_radiobutton",
	NULL
};

static const char * const executable_text_values[] = {
	"launch",
	"display",
	"ask",
	NULL
};

static const guint64 thumbnail_limit_values[] = {
	102400,
	512000,
	1048576,
	3145728,
	5242880,
	10485760,
	104857600,
	1073741824,
	2147483648U,
	4294967295U
};

static const char * const icon_captions_components[] = {
	"captions_0_combobox",
	"captions_1_combobox",
	"captions_2_combobox",
	NULL
};

static GtkWidget *preferences_dialog = NULL;

static void
nautilus_file_management_properties_size_group_create (GtkBuilder *builder,
						       char *prefix,
						       int items)
{
	GtkSizeGroup *size_group;
	int i;
	char *item_name;
	GtkWidget *widget;

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	for (i = 0; i < items; i++) {	
		item_name = g_strdup_printf ("%s_%d", prefix, i);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, item_name));
		gtk_size_group_add_widget (size_group, widget);
		g_free (item_name);
	}
	g_object_unref (G_OBJECT (size_group));
}

static void
columns_changed_callback (NautilusColumnChooser *chooser,
			  gpointer callback_data)
{
	char **visible_columns;
	char **column_order;

	nautilus_column_chooser_get_settings (NAUTILUS_COLUMN_CHOOSER (chooser),
					      &visible_columns,
					      &column_order);

	g_settings_set_strv (nautilus_list_view_preferences,
			     NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS,
			     (const char * const *)visible_columns);
	g_settings_set_strv (nautilus_list_view_preferences,
			     NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER,
			     (const char * const *)column_order);

	g_strfreev (visible_columns);
	g_strfreev (column_order);
}

static void
free_column_names_array (GPtrArray *column_names)
{
	g_ptr_array_foreach (column_names, (GFunc) g_free, NULL);
	g_ptr_array_free (column_names, TRUE);
}

static void
create_icon_caption_combo_box_items (GtkComboBoxText *combo_box,
			             GList *columns)
{
	GList *l;
	GPtrArray *column_names;

	column_names = g_ptr_array_new ();

	/* Translators: this is referred to captions under icons. */
	gtk_combo_box_text_append_text (combo_box, _("None"));
	g_ptr_array_add (column_names, g_strdup ("none"));

	for (l = columns; l != NULL; l = l->next) {
		NautilusColumn *column;
		char *name;
		char *label;

		column = NAUTILUS_COLUMN (l->data);

		g_object_get (G_OBJECT (column), 
			      "name", &name, "label", &label, 
			      NULL);

		/* Don't show name here, it doesn't make sense */
		if (!strcmp (name, "name")) {
			g_free (name);
			g_free (label);
			continue;
		}

		gtk_combo_box_text_append_text (combo_box, label);
		g_ptr_array_add (column_names, name);

		g_free (label);
	}
	g_object_set_data_full (G_OBJECT (combo_box), "column_names",
			        column_names,
			        (GDestroyNotify) free_column_names_array);
}

static void
icon_captions_changed_callback (GtkComboBox *combo_box,
				gpointer user_data)
{
	GPtrArray *captions;
	GtkBuilder *builder;
	int i;

	builder = GTK_BUILDER (user_data);

	captions = g_ptr_array_new ();

	for (i = 0; icon_captions_components[i] != NULL; i++) {
		GtkWidget *combo_box;
		int active;
		GPtrArray *column_names;
		char *name;

		combo_box = GTK_WIDGET (gtk_builder_get_object
					(builder, icon_captions_components[i]));
		active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));

		column_names = g_object_get_data (G_OBJECT (combo_box),
						  "column_names");

		name = g_ptr_array_index (column_names, active);
		g_ptr_array_add (captions, name);
	}
	g_ptr_array_add (captions, NULL);

	g_settings_set_strv (nautilus_icon_view_preferences,
			     NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
			     (const char **)captions->pdata);
	g_ptr_array_free (captions, TRUE);
}

static void
update_caption_combo_box (GtkBuilder *builder,
			  const char *combo_box_name,
			  const char *name)
{
	GtkWidget *combo_box;
	int i;
	GPtrArray *column_names;

	combo_box = GTK_WIDGET (gtk_builder_get_object (builder, combo_box_name));

	g_signal_handlers_block_by_func
		(combo_box,
		 G_CALLBACK (icon_captions_changed_callback),
		 builder);

	column_names = g_object_get_data (G_OBJECT (combo_box), 
					  "column_names");

	for (i = 0; i < column_names->len; ++i) {
		if (!strcmp (name, g_ptr_array_index (column_names, i))) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), i);
			break;
		}
	}

	g_signal_handlers_unblock_by_func
		(combo_box,
		 G_CALLBACK (icon_captions_changed_callback),
		 builder);
}

static void
update_icon_captions_from_settings (GtkBuilder *builder)
{
	char **captions;
	int i, j;

	captions = g_settings_get_strv (nautilus_icon_view_preferences, NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS);
	if (captions == NULL)
		return;

	for (i = 0, j = 0; 
	     icon_captions_components[i] != NULL;
	     i++) {
		char *data;

		if (captions[j]) {
			data = captions[j];
			++j;
		} else {
			data = "none";
		}

		update_caption_combo_box (builder, 
					  icon_captions_components[i],
					  data);
	}

	g_strfreev (captions);
}

static void
nautilus_file_management_properties_dialog_setup_icon_caption_page (GtkBuilder *builder)
{
	GList *columns;
	int i;
	gboolean writable;

	writable = g_settings_is_writable (nautilus_icon_view_preferences,
					   NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS);

	columns = nautilus_get_common_columns ();

	for (i = 0; icon_captions_components[i] != NULL; i++) {
		GtkWidget *combo_box;

		combo_box = GTK_WIDGET (gtk_builder_get_object (builder,
								icon_captions_components[i]));

		create_icon_caption_combo_box_items (GTK_COMBO_BOX_TEXT (combo_box), columns);
		gtk_widget_set_sensitive (combo_box, writable);

		g_signal_connect_data (combo_box, "changed",
				       G_CALLBACK (icon_captions_changed_callback),
				       g_object_ref (builder),
				       (GClosureNotify)g_object_unref,
				       0);
	}

	nautilus_column_list_free (columns);

	update_icon_captions_from_settings (builder);
}

static void
set_columns_from_settings (NautilusColumnChooser *chooser)
{
	char **visible_columns;
	char **column_order;

	visible_columns = g_settings_get_strv (nautilus_list_view_preferences,
					       NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
	column_order = g_settings_get_strv (nautilus_list_view_preferences,
					    NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);

	nautilus_column_chooser_set_settings (NAUTILUS_COLUMN_CHOOSER (chooser),
					      visible_columns,
					      column_order);

	g_strfreev (visible_columns);
	g_strfreev (column_order);
}

static void
use_default_callback (NautilusColumnChooser *chooser,
		      gpointer user_data)
{
	g_settings_reset (nautilus_list_view_preferences,
			  NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
	g_settings_reset (nautilus_list_view_preferences,
			  NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);
	set_columns_from_settings (chooser);
}

static void
nautilus_file_management_properties_dialog_setup_list_column_page (GtkBuilder *builder)
{
	GtkWidget *chooser;
	GtkWidget *box;

	chooser = nautilus_column_chooser_new (NULL);
	g_signal_connect (chooser, "changed",
			  G_CALLBACK (columns_changed_callback), chooser);
	g_signal_connect (chooser, "use-default",
			  G_CALLBACK (use_default_callback), chooser);

	set_columns_from_settings (NAUTILUS_COLUMN_CHOOSER (chooser));

	gtk_widget_show (chooser);
	box = GTK_WIDGET (gtk_builder_get_object (builder, "list_columns_vbox"));

	gtk_box_pack_start (GTK_BOX (box), chooser, TRUE, TRUE, 0);
}

static void
bind_builder_bool (GtkBuilder *builder,
		   GSettings *settings,
		   const char *widget_name,
		   const char *prefs)
{
	g_settings_bind (settings, prefs,
			 gtk_builder_get_object (builder, widget_name),
			 "active", G_SETTINGS_BIND_DEFAULT);
}

static gboolean
enum_get_mapping (GValue             *value,
		  GVariant           *variant,
		  gpointer            user_data)
{
	const char **enum_values = user_data;
	const char *str;
	int i;

	str = g_variant_get_string (variant, NULL);
	for (i = 0; enum_values[i] != NULL; i++) {
		if (strcmp (enum_values[i], str) == 0) {
			g_value_set_int (value, i);
			return TRUE;
		}
	}

	return FALSE;
}

static GVariant *
enum_set_mapping (const GValue       *value,
		  const GVariantType *expected_type,
		  gpointer            user_data)
{
	const char **enum_values = user_data;

	return g_variant_new_string (enum_values[g_value_get_int (value)]);
}

static void
bind_builder_enum (GtkBuilder *builder,
		   GSettings *settings,
		   const char *widget_name,
		   const char *prefs,
		   const char **enum_values)
{
	g_settings_bind_with_mapping (settings, prefs,
				      gtk_builder_get_object (builder, widget_name),
				      "active", G_SETTINGS_BIND_DEFAULT,
				      enum_get_mapping,
				      enum_set_mapping,
				      enum_values, NULL);
}

typedef struct {
	const guint64 *values;
	int n_values;
} UIntEnumBinding;

static gboolean
uint_enum_get_mapping (GValue             *value,
		       GVariant           *variant,
		       gpointer            user_data)
{
	UIntEnumBinding *binding = user_data;
	guint64 v;
	int i;

	v = g_variant_get_uint64 (variant);
	for (i = 0; i < binding->n_values; i++) {
		if (binding->values[i] >= v) {
			g_value_set_int (value, i);
			return TRUE;
		}
	}

	return FALSE;
}

static GVariant *
uint_enum_set_mapping (const GValue       *value,
		       const GVariantType *expected_type,
		       gpointer            user_data)
{
	UIntEnumBinding *binding = user_data;

	return g_variant_new_uint64 (binding->values[g_value_get_int (value)]);
}

static void
bind_builder_uint_enum (GtkBuilder *builder,
			GSettings *settings,
			const char *widget_name,
			const char *prefs,
			const guint64 *values,
			int n_values)
{
	UIntEnumBinding *binding;

	binding = g_new (UIntEnumBinding, 1);
	binding->values = values;
	binding->n_values = n_values;

	g_settings_bind_with_mapping (settings, prefs,
				      gtk_builder_get_object (builder, widget_name),
				      "active", G_SETTINGS_BIND_DEFAULT,
				      uint_enum_get_mapping,
				      uint_enum_set_mapping,
				      binding, g_free);
}

static GVariant *
radio_mapping_set (const GValue *gvalue,
		   const GVariantType *expected_type,
		   gpointer user_data)
{
	const gchar *widget_value = user_data;
	GVariant *retval = NULL;

	if (g_value_get_boolean (gvalue)) {
		retval = g_variant_new_string (widget_value);
	}

	return retval;
}

static gboolean
radio_mapping_get (GValue *gvalue,
		   GVariant *variant,
		   gpointer user_data)
{
	const gchar *widget_value = user_data;
	const gchar *value;

	value = g_variant_get_string (variant, NULL);

	if (g_strcmp0 (value, widget_value) == 0) {
		g_value_set_boolean (gvalue, TRUE);
	} else {
		g_value_set_boolean (gvalue, FALSE);
	}

	return TRUE;
}

static void
bind_builder_radio (GtkBuilder *builder,
		    GSettings *settings,
		    const char **widget_names,
		    const char *prefs,
		    const char **values)
{
	GtkWidget *button;
	int i;

	for (i = 0; widget_names[i] != NULL; i++) {
		button = GTK_WIDGET (gtk_builder_get_object (builder, widget_names[i]));

		g_settings_bind_with_mapping (settings, prefs,
					      button, "active",
					      G_SETTINGS_BIND_DEFAULT,
					      radio_mapping_get, radio_mapping_set,
					      (gpointer) values[i], NULL);
	}
}

static void
set_gtk_filechooser_sort_first (GObject *object,
				GParamSpec *pspec)
{
	g_settings_set_boolean (gtk_filechooser_preferences,
				NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST,
				gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object)));
}

static  void
nautilus_file_management_properties_dialog_setup (GtkBuilder *builder, GtkWindow *window)
{
	GtkWidget *dialog;

	/* setup UI */
	nautilus_file_management_properties_size_group_create (builder,
							       "views_label",
							       4);
	nautilus_file_management_properties_size_group_create (builder,
							       "captions_label",
							       3);
	nautilus_file_management_properties_size_group_create (builder,
							       "preview_label",
							       3);

	/* setup preferences */
	bind_builder_bool (builder, nautilus_preferences,
			   NAUTILUS_FILE_MANAGEMENT_PROPERTIES_FOLDERS_FIRST_WIDGET,
			   NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST);
	g_signal_connect (gtk_builder_get_object (builder, NAUTILUS_FILE_MANAGEMENT_PROPERTIES_FOLDERS_FIRST_WIDGET),
                          "notify::active",
                          G_CALLBACK (set_gtk_filechooser_sort_first), NULL);

	bind_builder_bool (builder, nautilus_preferences,
			   NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TRASH_CONFIRM_WIDGET,
			   NAUTILUS_PREFERENCES_CONFIRM_TRASH);
	bind_builder_bool (builder, nautilus_preferences,
			   NAUTILUS_FILE_MANAGEMENT_PROPERTIES_TRASH_DELETE_WIDGET,
			   NAUTILUS_PREFERENCES_ENABLE_DELETE);
	bind_builder_bool (builder, gtk_filechooser_preferences,
			   NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SHOW_HIDDEN_WIDGET,
			   NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);
	bind_builder_bool (builder, nautilus_list_view_preferences,
			   NAUTILUS_FILE_MANAGEMENT_PROPERTIES_LIST_VIEW_USE_TREE_WIDGET,
			   NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE);

	bind_builder_enum (builder, nautilus_preferences,
			   NAUTILUS_FILE_MANAGEMENT_PROPERTIES_DEFAULT_VIEW_WIDGET,
			   NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER,
			   (const char **) default_view_values);
	bind_builder_enum (builder, nautilus_icon_view_preferences,
			   NAUTILUS_FILE_MANAGEMENT_PROPERTIES_ICON_VIEW_ZOOM_WIDGET,
			   NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
			   (const char **) zoom_values);
	bind_builder_enum (builder, nautilus_list_view_preferences,
			   NAUTILUS_FILE_MANAGEMENT_PROPERTIES_LIST_VIEW_ZOOM_WIDGET,
			   NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
			   (const char **) zoom_values);
	bind_builder_enum (builder, nautilus_preferences,
			   NAUTILUS_FILE_MANAGEMENT_PROPERTIES_SORT_ORDER_WIDGET,
			   NAUTILUS_PREFERENCES_DEFAULT_SORT_ORDER,
			   (const char **) sort_order_values);
	bind_builder_enum (builder, nautilus_preferences,
			   NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_FILES_WIDGET,
			   NAUTILUS_PREFERENCES_SHOW_FILE_THUMBNAILS,
			   (const char **) preview_values);
	bind_builder_enum (builder, nautilus_preferences,
			   NAUTILUS_FILE_MANAGEMENT_PROPERTIES_PREVIEW_FOLDER_WIDGET,
			   NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
			   (const char **) preview_values);

	bind_builder_radio (builder, nautilus_preferences,
			    (const char **) click_behavior_components,
			    NAUTILUS_PREFERENCES_CLICK_POLICY,
			    (const char **) click_behavior_values);
	bind_builder_radio (builder, nautilus_preferences,
			    (const char **) executable_text_components,
			    NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION,
			    (const char **) executable_text_values);

	bind_builder_uint_enum (builder, nautilus_preferences,
				NAUTILUS_FILE_MANAGEMENT_PROPERTIES_THUMBNAIL_LIMIT_WIDGET,
				NAUTILUS_PREFERENCES_FILE_THUMBNAIL_LIMIT,
				thumbnail_limit_values,
				G_N_ELEMENTS (thumbnail_limit_values));

	nautilus_file_management_properties_dialog_setup_icon_caption_page (builder);
	nautilus_file_management_properties_dialog_setup_list_column_page (builder);

	/* UI callbacks */
	dialog = GTK_WIDGET (gtk_builder_get_object (builder, "file_management_dialog"));
	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);

	gtk_window_set_icon_name (GTK_WINDOW (dialog), "system-file-manager");

	if (window) {
		gtk_window_set_screen (GTK_WINDOW (dialog), gtk_window_get_screen(window));
	}

	preferences_dialog = dialog;
	g_object_add_weak_pointer (G_OBJECT (dialog), (gpointer *) &preferences_dialog);
	gtk_widget_show (dialog);
}

void
nautilus_file_management_properties_dialog_show (GtkWindow *window)
{
	GtkBuilder *builder;

	if (preferences_dialog != NULL) {
		gtk_window_present (GTK_WINDOW (preferences_dialog));
		return;
	}

	builder = gtk_builder_new ();

	gtk_builder_add_from_resource (builder,
				       "/org/gnome/nautilus/nautilus-file-management-properties.ui",
				       NULL);

	nautilus_file_management_properties_dialog_setup (builder, window);

	g_object_unref (builder);
}

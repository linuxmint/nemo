/* -*- Mode: C; indent-tabs-mode: f; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * Copyright (C) 2005 Red Hat, Inc.
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
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#include <config.h>
#include "nemo-query-editor.h"
#include "nemo-file-utilities.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <eel/eel-glib-extensions.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

typedef struct
{
    GtkWidget *infobar;
	GtkWidget *entry;
    GtkWidget *menu;

	gboolean change_frozen;
	guint typing_timeout_id;
	gboolean is_visible;
	GtkWidget *vbox;

    gchar **faves;

	char *current_uri;
    char *base_uri;

	char *last_set_query_text;
} NemoQueryEditorPrivate;

struct _NemoQueryEditor
{
    GtkBox parent_object;

    NemoQueryEditorPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (NemoQueryEditor, nemo_query_editor, GTK_TYPE_BOX)

enum {
	CHANGED,
	CANCEL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void entry_activate_cb (GtkWidget *entry, NemoQueryEditor *editor);
static void entry_changed_cb  (GtkWidget *entry, NemoQueryEditor *editor);
static void nemo_query_editor_changed_force (NemoQueryEditor *editor,
						 gboolean             force);
static void nemo_query_editor_changed (NemoQueryEditor *editor);

static void on_saved_searches_setting_changed (GSettings *settings,
                                               gchar     *key,
                                               gpointer   user_data);

static gchar *
get_sanitized_query_string (NemoQueryEditor *editor)
{
    const gchar *entry_text;
    gchar *ret;

    entry_text = gtk_entry_get_text (GTK_ENTRY (editor->priv->entry));

    ret = g_strdup (entry_text);
    ret = g_strstrip (ret);

    return ret;
}

static void
nemo_query_editor_dispose (GObject *object)
{
	NemoQueryEditor *editor;

	editor = NEMO_QUERY_EDITOR (object);

    g_clear_pointer (&editor->priv->base_uri, g_free);
    g_clear_pointer (&editor->priv->last_set_query_text, g_free);

	if (editor->priv->typing_timeout_id > 0) {
		g_source_remove (editor->priv->typing_timeout_id);
		editor->priv->typing_timeout_id = 0;
	}

    g_clear_object (&editor->priv->menu);
    g_clear_pointer (&editor->priv->faves, g_strfreev);

    g_signal_handlers_disconnect_by_func (nemo_preferences,
                                          on_saved_searches_setting_changed,
                                          editor);

	G_OBJECT_CLASS (nemo_query_editor_parent_class)->dispose (object);
}

static void
nemo_query_editor_grab_focus (GtkWidget *widget)
{
	NemoQueryEditor *editor = NEMO_QUERY_EDITOR (widget);

	if (gtk_widget_get_visible (widget)) {
		gtk_entry_grab_focus_without_selecting (GTK_ENTRY (editor->priv->entry));
	}
}

static void
nemo_query_editor_class_init (NemoQueryEditorClass *class)
{
    GObjectClass *gobject_class;
    GtkWidgetClass *widget_class;
    GtkBindingSet *binding_set;

    gobject_class = G_OBJECT_CLASS (class);
    gobject_class->dispose = nemo_query_editor_dispose;

    widget_class = GTK_WIDGET_CLASS (class);
    widget_class->grab_focus = nemo_query_editor_grab_focus;

    signals[CHANGED] = g_signal_new ("changed",
                                     G_TYPE_FROM_CLASS (class),
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL, NULL,
                                     G_TYPE_NONE, 2, NEMO_TYPE_QUERY, G_TYPE_BOOLEAN);

    signals[CANCEL] = g_signal_new ("cancel",
                                    G_TYPE_FROM_CLASS (class),
                                    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                    0,
                                    NULL, NULL, NULL,
                                    G_TYPE_NONE, 0);

    binding_set = gtk_binding_set_by_class (class);
    gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "cancel", 0);
}

GFile *
nemo_query_editor_get_location (NemoQueryEditor *editor)
{
	GFile *file = NULL;
	if (editor->priv->current_uri != NULL)
		file = g_file_new_for_uri (editor->priv->current_uri);
	return file;
}

static void
entry_activate_cb (GtkWidget *entry, NemoQueryEditor *editor)
{
    g_autofree gchar *text = NULL;

	if (editor->priv->typing_timeout_id > 0) {
		g_source_remove (editor->priv->typing_timeout_id);
		editor->priv->typing_timeout_id = 0;
	}

    text = get_sanitized_query_string (editor);

    if (strlen (text) > 2) {
        nemo_query_editor_changed_force (editor, TRUE);
    }
}

static gboolean
typing_timeout_cb (gpointer user_data)
{
	NemoQueryEditor *editor;

	editor = NEMO_QUERY_EDITOR (user_data);
	editor->priv->typing_timeout_id = 0;

	nemo_query_editor_changed (editor);

	return FALSE;
}

#define TYPING_TIMEOUT 250

static gchar *
construct_favorite_entry (const gchar *uri,
                          const gchar *key)
{
    return g_strdup_printf ("%s::%s", uri, key);
}

static gboolean
parse_favorite_entry (const gchar  *favorite_entry,
                      gchar       **uri,
                      gchar       **key)
{
    gchar **split;

    split = g_strsplit (favorite_entry, "::", 2);

    if (split == NULL || g_strv_length (split) < 2) {
        *key = NULL;
        *uri = NULL;
        return FALSE;
    }

    *uri = g_strdup (split[0]);
    *key = g_strdup (split[1]);

    g_strfreev (split);

    return TRUE;
}

static gboolean
is_search_criteria_in_faves (NemoQueryEditor *editor,
                             const gchar     *key)
{
    gint i, length;
    gboolean ret;

    length = g_strv_length (editor->priv->faves);

    if (length == 0) {
        return FALSE;
    }

    ret = FALSE;

    for (i = 0; i < length; i++) {
        gchar *favorite_uri, *favorite_key;

        if (parse_favorite_entry (editor->priv->faves[i],
                                  &favorite_uri,
                                  &favorite_key)) {
            if (g_strcmp0 (editor->priv->current_uri, favorite_uri) == 0) {
                if (g_strcmp0 (key, favorite_key) == 0) {
                    ret = TRUE;
                }
            }

            g_clear_pointer (&favorite_uri, g_free);
            g_clear_pointer (&favorite_key, g_free);
        }

        if (ret) {
            break;
        }
    }

    return ret;
}

static void
add_key_to_faves (NemoQueryEditor *editor,
                  const gchar     *entry)
{
    gint i;
    GPtrArray *array;

    array = g_ptr_array_new ();

    g_ptr_array_add (array, g_strdup (entry));

    if (editor->priv->faves != NULL) {
        for (i = 0; i < g_strv_length (editor->priv->faves); i++) {
            g_ptr_array_add (array, g_strdup (editor->priv->faves[i]));
        }
    }

    g_ptr_array_add (array, NULL);

    g_signal_handlers_block_by_func (nemo_preferences,
                                     on_saved_searches_setting_changed,
                                     editor);

    g_settings_set_strv (nemo_preferences,
                         NEMO_PREFERENCES_SAVED_SEARCHES,
                         (const gchar * const *) array->pdata);

    g_signal_handlers_unblock_by_func (nemo_preferences,
                                       on_saved_searches_setting_changed,
                                       editor);

    g_clear_pointer (&editor->priv->faves, g_strfreev);
    editor->priv->faves = (gchar **) g_ptr_array_free (array, FALSE);
}

static void
remove_key_from_faves (NemoQueryEditor *editor,
                       const gchar     *entry)
{
    gint i;
    gchar *key, *uri;
    GPtrArray *array;

    if (!parse_favorite_entry (entry, &uri, &key)) {
        return;
    }

    array = g_ptr_array_new ();

    if (editor->priv->faves != NULL) {
        for (i = 0; i < g_strv_length (editor->priv->faves); i++) {
            gchar *favorite_key, *favorite_uri;

            if (parse_favorite_entry (editor->priv->faves[i],
                                      &favorite_uri,
                                      &favorite_key)) {
                if (g_strcmp0 (key, favorite_key) != 0 ||
                    g_strcmp0 (uri, favorite_uri) != 0) {
                    g_ptr_array_add (array, g_strdup (editor->priv->faves[i]));
                }

                g_free (favorite_key);
                g_free (favorite_uri);
            }
        }
    }

    g_ptr_array_add (array, NULL);

    g_signal_handlers_block_by_func (nemo_preferences,
                                     on_saved_searches_setting_changed,
                                     editor);

    g_settings_set_strv (nemo_preferences,
                         NEMO_PREFERENCES_SAVED_SEARCHES,
                         (const gchar * const *) array->pdata);

    g_signal_handlers_unblock_by_func (nemo_preferences,
                                       on_saved_searches_setting_changed,
                                       editor);

    g_free (key);
    g_free (uri);

    g_clear_pointer (&editor->priv->faves, g_strfreev);
    editor->priv->faves = (gchar **) g_ptr_array_free (array, FALSE);
}

static void
update_fav_icon (NemoQueryEditor *editor)
{
    g_autofree gchar *current_key = NULL;

    current_key = get_sanitized_query_string (editor);

    if (is_search_criteria_in_faves (editor, current_key)) {
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (editor->priv->entry),
                                           GTK_ENTRY_ICON_SECONDARY,
                                           "starred-symbolic");
        return;
    }

    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (editor->priv->entry),
                                       GTK_ENTRY_ICON_SECONDARY,
                                       "non-starred-symbolic");
}

static void
entry_changed_cb (GtkWidget *entry, NemoQueryEditor *editor)
{
    g_autofree gchar *text = NULL;

	if (editor->priv->change_frozen) {
		return;
	}

    if (editor->priv->typing_timeout_id > 0) {
        g_source_remove (editor->priv->typing_timeout_id);
        editor->priv->typing_timeout_id = 0;
    }

    update_fav_icon (editor);

    text = get_sanitized_query_string (editor);

    if (strlen (text) > 2) {
        editor->priv->typing_timeout_id = g_timeout_add (TYPING_TIMEOUT,
                                                         typing_timeout_cb,
                                                         editor);
    }
}

static void
get_markup_for_fave (NemoQueryEditor *editor,
                     const gchar     *favorite,
                     gchar          **loc_markup,
                     gchar          **key_markup)
{
    GFile *location;
    gchar *favorite_key, *favorite_location;
    gchar *location_string, *mnemonic_key;

    if (!parse_favorite_entry (favorite, &favorite_location, &favorite_key)) {
        *loc_markup = NULL;
        *key_markup = NULL;
        return;
    }

    location = g_file_new_for_uri (favorite_location);
    location_string = nemo_compute_search_title_for_location (location);

    mnemonic_key = g_strdup_printf ("_%s", favorite_key);

    *loc_markup = g_strdup_printf (_("in <tt><b>%s</b></tt>"), location_string);
    *key_markup = g_strdup_printf (_("Search for <b>%s</b>"), mnemonic_key);

    g_free (favorite_location);
    g_free (favorite_key);
    g_free (location_string);
    g_free (mnemonic_key);
    g_object_unref (location);
}

static void
on_menu_item_activated (GtkMenuItem *item,
                        gpointer     user_data)
{
    NemoQueryEditor *editor;
    NemoQuery *query;
    const gchar *fave_entry;
    gchar *favorite_key, *favorite_location;

    editor = NEMO_QUERY_EDITOR (user_data);

    fave_entry = g_object_get_data (G_OBJECT (item),
                                   "fave-entry");

    if (parse_favorite_entry (fave_entry, &favorite_location, &favorite_key)) {
        query = nemo_query_new ();

        nemo_query_set_location (query, favorite_location);
        nemo_query_set_text (query, favorite_key);

        nemo_query_editor_set_query (editor, query);
        nemo_query_editor_changed (editor);
        update_fav_icon (editor);

        g_free (favorite_location);
        g_free (favorite_key);
        g_object_unref (query);
    }
}

static gboolean
on_menu_item_key_press (GtkWidget    *widget,
                        GdkEvent     *event,
                        gpointer user_data)
{
    if (event->key.state == 0 && event->key.keyval == GDK_KEY_Delete) {
        NemoQueryEditor *editor;
        GtkWidget *item;
        const gchar *fave_entry;

        editor = NEMO_QUERY_EDITOR (user_data);

        item = gtk_menu_shell_get_selected_item (GTK_MENU_SHELL (widget));

        if (item == NULL) {
            return GDK_EVENT_PROPAGATE;
        }

        fave_entry = g_object_get_data (G_OBJECT (item),
                                        "fave-entry");

        remove_key_from_faves (editor, fave_entry);

        gtk_widget_set_sensitive (item, FALSE);
        update_fav_icon (editor);

        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

#if !GTK_CHECK_VERSION (3, 22, 0)
static void
menu_position_function (GtkMenu  *menu,
                        gint     *x,
                        gint     *y,
                        gboolean *push_in,
                        gpointer  user_data)
{
    NemoQueryEditor *editor;
    GtkWidget *parent;
    GtkAllocation menu_allocation;
    gint window_x, window_y, translated_x, translated_y;

    g_return_if_fail (NEMO_IS_QUERY_EDITOR (user_data));

    editor = NEMO_QUERY_EDITOR (user_data);

    parent = gtk_widget_get_toplevel (GTK_WIDGET (editor));

    gtk_widget_translate_coordinates (editor->priv->entry,
                                      parent,
                                      0, 0,
                                      &translated_x,
                                      &translated_y);

    gdk_window_get_position (gtk_widget_get_window (parent), &window_x, &window_y);

    gtk_widget_get_allocation (GTK_WIDGET (menu), &menu_allocation);

    *x = translated_x + window_x;
    *y = translated_y + window_y - menu_allocation.height;
    *push_in = TRUE;
}
#endif

static void
popup_favorites (NemoQueryEditor *editor,
                 GdkEvent        *event,
                 gboolean         use_pointer_location)
{
    GtkWidget *menu, *item, *item_child;
    GtkSizeGroup *group;
    gchar **faves;
    gint i;

    if (g_strv_length (editor->priv->faves) == 0) {
        return;
    }

    g_clear_object (&editor->priv->menu);
    editor->priv->menu = menu = g_object_ref_sink (gtk_menu_new ());

    group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

    faves = editor->priv->faves;

    for (i = 0; i < g_strv_length (faves); i++) {
        GtkWidget *label;
        gchar *loc_markup, *key_markup;

        get_markup_for_fave (editor,
                             faves[i],
                             &loc_markup,
                             &key_markup);

        if (loc_markup == NULL || key_markup == NULL) {
            continue;
        }

        item = gtk_menu_item_new();

        item_child = gtk_bin_get_child (GTK_BIN (item));

        if (item_child != NULL) {
            gtk_widget_destroy (item_child);
        }

        item_child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_container_add (GTK_CONTAINER (item), item_child);

        label = gtk_label_new (NULL);

        gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), key_markup);
        gtk_label_set_xalign (GTK_LABEL (label), 0.0);
        gtk_box_pack_start (GTK_BOX (item_child), label, FALSE, FALSE, 0);
        gtk_size_group_add_widget (group, label);

        label = gtk_label_new (NULL);

        gtk_label_set_markup (GTK_LABEL (label), loc_markup);
        gtk_label_set_xalign (GTK_LABEL (label), 0.0);
        gtk_box_pack_start (GTK_BOX (item_child), label, FALSE, FALSE, 0);

        g_object_set_data_full (G_OBJECT (item),
                                "fave-entry",
                                g_strdup (faves[i]),
                                (GDestroyNotify) g_free);

        g_free (loc_markup);
        g_free (key_markup);

        gtk_widget_show_all (GTK_WIDGET (item));

        gtk_menu_attach (GTK_MENU (menu), item, 0, 1, i, i + 1);

        g_signal_connect (item,
                          "activate",
                          G_CALLBACK (on_menu_item_activated),
                          editor);
    }

    g_object_unref (group);

    g_signal_connect (menu,
                      "key-press-event",
                      G_CALLBACK (on_menu_item_key_press),
                      editor);

#if GTK_CHECK_VERSION (3, 22, 0)
    if (use_pointer_location) {
        gtk_menu_popup_at_pointer (GTK_MENU (menu), event);
    } else {
        gtk_menu_popup_at_widget (GTK_MENU (menu),
                                  editor->priv->entry,
                                  GDK_GRAVITY_NORTH_WEST,
                                  GDK_GRAVITY_SOUTH_WEST,
                                  event);
    }
#else
    if (use_pointer_location) {
        gtk_menu_popup (GTK_MENU (menu),
                        NULL, NULL, NULL, NULL,
                        3,
                        gtk_get_current_event_time ());
    } else {
        gtk_menu_popup (GTK_MENU (menu),
                        NULL, NULL,
                        (GtkMenuPositionFunc) menu_position_function,
                        editor,
                        0,
                        gtk_get_current_event_time ());
    }
#endif
}

static gboolean
on_key_press_event (GtkWidget    *widget,
                    GdkEvent     *event,
                    gpointer user_data)
{
    if ((event->key.state & gtk_accelerator_get_default_mod_mask ()) == 0 && event->key.keyval == GDK_KEY_Up) {
        popup_favorites (NEMO_QUERY_EDITOR (user_data), event, FALSE);
        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static void
fave_icon_clicked_cb (GtkWidget             *widget,
                      GtkEntryIconPosition   position,
                      GdkEvent              *event,
                      gpointer               user_data)
{
    NemoQueryEditor *editor;
    g_autofree gchar *current_key = NULL;

    if (position == GTK_ENTRY_ICON_PRIMARY) {
        return;
    }

    editor = NEMO_QUERY_EDITOR (user_data);

    current_key = get_sanitized_query_string (editor);

    if ((event->button.state & gtk_accelerator_get_default_mod_mask ()) == 0 && event->button.button == 1) {
        gchar *entry;

        if (strlen (current_key) < 3) {
            return;
        }

        entry = construct_favorite_entry (editor->priv->current_uri, current_key);

        if (is_search_criteria_in_faves (editor, current_key)) {
            remove_key_from_faves (editor, entry);
        } else {
            add_key_to_faves (editor, entry);
        }

        g_free (entry);

        update_fav_icon (editor);
    } else {
        popup_favorites (editor, event, TRUE);
    }
}

static void
on_saved_searches_setting_changed (GSettings *settings,
                                   gchar     *key,
                                   gpointer   user_data)
{
    NemoQueryEditor *editor;

    g_return_if_fail (NEMO_IS_QUERY_EDITOR (user_data));

    editor = NEMO_QUERY_EDITOR (user_data);

    g_clear_pointer (&editor->priv->faves, g_strfreev);
    editor->priv->faves = g_settings_get_strv (settings, key);
}

static void
nemo_query_editor_init (NemoQueryEditor *editor)
{
    NemoQueryEditorPrivate *priv;
    GtkWidget *separator;

    editor->priv = G_TYPE_INSTANCE_GET_PRIVATE (editor,
                                                NEMO_TYPE_QUERY_EDITOR,
                                                NemoQueryEditorPrivate);
    priv = editor->priv;

    priv->base_uri = NULL;
    priv->menu = NULL;

    priv->faves = g_settings_get_strv (nemo_preferences,
                                       NEMO_PREFERENCES_SAVED_SEARCHES);

    gtk_orientable_set_orientation (GTK_ORIENTABLE (editor), GTK_ORIENTATION_VERTICAL);

    priv->infobar = gtk_info_bar_new ();
    gtk_box_pack_start (GTK_BOX (editor), priv->infobar, TRUE, TRUE, 0);
    gtk_widget_set_no_show_all (priv->infobar, TRUE);
    gtk_info_bar_set_message_type (GTK_INFO_BAR (priv->infobar), GTK_MESSAGE_OTHER);

    priv->entry = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (gtk_info_bar_get_content_area (GTK_INFO_BAR (priv->infobar))),
                       priv->entry,
                       TRUE, TRUE, 0);
    gtk_widget_show (priv->entry);

    gtk_entry_set_placeholder_text (GTK_ENTRY (priv->entry), _("Type to search or arrow-up to select a favorite"));

    g_signal_connect (priv->entry,
                      "activate",
                      G_CALLBACK (entry_activate_cb),
                      editor);

    g_signal_connect (priv->entry,
                      "changed",
                      G_CALLBACK (entry_changed_cb),
                      editor);

    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->entry),
                                       GTK_ENTRY_ICON_PRIMARY,
                                       "edit-find-symbolic");

    gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->entry),
                                       GTK_ENTRY_ICON_SECONDARY,
                                       "non-starred-symbolic");

    gtk_entry_set_icon_tooltip_text (GTK_ENTRY (priv->entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     _("Click to save or forget a favorite search. "
                                       "Right-click to display favorites."));

    g_signal_connect (priv->entry,
                      "icon-press",
                      G_CALLBACK (fave_icon_clicked_cb),
                      editor);

    separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start (GTK_BOX (editor), separator, TRUE, TRUE, 0);

    g_object_bind_property (priv->infobar, "visible",
                            separator, "visible",
                            G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    g_signal_connect (nemo_preferences,
                      "changed::" NEMO_PREFERENCES_SAVED_SEARCHES,
                      G_CALLBACK (on_saved_searches_setting_changed),
                      editor);

    gtk_widget_show (GTK_WIDGET (editor));
}

static void
nemo_query_editor_changed_force (NemoQueryEditor *editor, gboolean force_reload)
{
	NemoQuery *query;

	if (editor->priv->change_frozen) {
		return;
	}

	query = nemo_query_editor_get_query (editor);
	g_signal_emit (editor, signals[CHANGED], 0,
		       query, force_reload);
	g_clear_object (&query);
}

static void
nemo_query_editor_changed (NemoQueryEditor *editor)
{
	nemo_query_editor_changed_force (editor, TRUE);
}

static void
add_location_to_query (NemoQueryEditor *editor,
		       NemoQuery       *query)
{
	char *uri;

    uri = g_strdup (editor->priv->current_uri);

	nemo_query_set_location (query, uri);
	g_free (uri);
}


NemoQuery *
nemo_query_editor_get_query (NemoQueryEditor *editor)
{
    NemoQuery *query;
    g_autofree gchar *query_text = NULL;

	if (editor == NULL || editor->priv == NULL || editor->priv->entry == NULL) {
		return NULL;
	}

	query_text = get_sanitized_query_string (editor);

    if (g_strcmp0 (query_text, "") == 0) {
        return NULL;
    }

	query = nemo_query_new ();
	nemo_query_set_text (query, query_text);

	add_location_to_query (editor, query);

	return query;
}

GtkWidget *
nemo_query_editor_new (void)
{
    return g_object_new (NEMO_TYPE_QUERY_EDITOR, NULL);
}

void
nemo_query_editor_set_location (NemoQueryEditor *editor,
				    GFile               *location)
{
	g_free (editor->priv->current_uri);
	editor->priv->current_uri = g_file_get_uri (location);
}

void
nemo_query_editor_set_query (NemoQueryEditor	*editor,
				 NemoQuery		*query)
{
	char *text = NULL;

	if (query != NULL) {
		text = nemo_query_get_text (query);
	}

	if (!text) {
		text = g_strdup ("");
	}

	editor->priv->change_frozen = TRUE;
	gtk_entry_set_text (GTK_ENTRY (editor->priv->entry), text);
    gtk_widget_grab_focus (editor->priv->entry);

	g_free (editor->priv->current_uri);
	editor->priv->current_uri = NULL;

	if (query != NULL) {
		editor->priv->current_uri = nemo_query_get_location (query);
	}

	g_free (editor->priv->last_set_query_text);
	editor->priv->last_set_query_text = text;

	editor->priv->change_frozen = FALSE;
}

void
nemo_query_editor_set_active (NemoQueryEditor *editor,
                              gchar           *base_uri,
                              gboolean         active)
{
    g_return_if_fail (NEMO_IS_QUERY_EDITOR (editor));

    if (active) {
        gtk_widget_show (editor->priv->infobar);
        gtk_widget_queue_resize (GTK_WIDGET (editor->priv->infobar));

        g_clear_pointer (&editor->priv->base_uri, g_free);
        editor->priv->base_uri = base_uri;

        g_signal_connect (editor->priv->entry,
                          "key-press-event",
                          G_CALLBACK (on_key_press_event),
                          editor);

        update_fav_icon (editor);
    } else {
        g_signal_handlers_disconnect_by_func (editor->priv->entry,
                                              on_key_press_event,
                                              editor);

        gtk_widget_hide (editor->priv->infobar);
    }
}

gboolean
nemo_query_editor_get_active (NemoQueryEditor *editor)
{
    g_return_val_if_fail (NEMO_IS_QUERY_EDITOR (editor), FALSE);

    return gtk_widget_get_visible (editor->priv->infobar);
}

const gchar *
nemo_query_editor_get_base_uri (NemoQueryEditor *editor)
{
    g_return_val_if_fail (NEMO_IS_QUERY_EDITOR (editor), NULL);

    return editor->priv->base_uri;
}

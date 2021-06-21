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
    GtkBuilder *builder;
    GtkWidget *infobar;
    GtkWidget *file_entry;
    GtkWidget *file_entry_combo;
    GtkWidget *content_entry;
    GtkWidget *content_entry_combo;
    GtkWidget *content_case_toggle;
    GtkWidget *file_case_toggle;
    GtkWidget *regex_toggle;
    GtkWidget *file_recurse_toggle;
    GtkWidget *content_main_box;

    GtkWidget *content_view;
    GtkWidget *last_focus_widget;
    GList *focus_chain;

	gboolean change_frozen;
	guint typing_timeout_id;
	gboolean is_visible;
	GtkWidget *vbox;

    gboolean history_enabled;
    gboolean focus_frozen;

    gchar *current_uri;
    gchar *base_uri;

    gchar *last_set_query_file_pattern;
    gchar *last_set_query_content_pattern;
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

static void nemo_query_editor_changed_force (NemoQueryEditor *editor,
						 gboolean             force);
static void nemo_query_editor_changed (NemoQueryEditor *editor);

static gchar *
get_sanitized_file_search_string (NemoQueryEditor *editor)
{
    const gchar *entry_text;
    gchar *ret;

    entry_text = gtk_entry_get_text (GTK_ENTRY (editor->priv->file_entry));

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
    g_clear_pointer (&editor->priv->current_uri, g_free);
    g_clear_pointer (&editor->priv->last_set_query_file_pattern, g_free);
    g_clear_pointer (&editor->priv->last_set_query_content_pattern, g_free);

	if (editor->priv->typing_timeout_id > 0) {
		g_source_remove (editor->priv->typing_timeout_id);
		editor->priv->typing_timeout_id = 0;
	}

    g_clear_object (&editor->priv->builder);

    if (editor->priv->focus_chain != NULL) {
        editor->priv->focus_chain->prev->next = NULL;
        editor->priv->focus_chain->prev = NULL;
        g_list_free (editor->priv->focus_chain);
        editor->priv->focus_chain = NULL;
    }

	G_OBJECT_CLASS (nemo_query_editor_parent_class)->dispose (object);
}

static void
nemo_query_editor_grab_focus (GtkWidget *widget)
{
	NemoQueryEditor *editor = NEMO_QUERY_EDITOR (widget);

	if (gtk_widget_get_visible (widget)) {
        if (editor->priv->last_focus_widget != NULL) {
            gtk_entry_grab_focus_without_selecting (GTK_ENTRY (editor->priv->last_focus_widget));
        } else {
            gtk_entry_grab_focus_without_selecting (GTK_ENTRY (editor->priv->file_entry));
        }
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

static void
entry_activate_cb (GtkWidget *entry, NemoQueryEditor *editor)
{
	if (editor->priv->typing_timeout_id > 0) {
		g_source_remove (editor->priv->typing_timeout_id);
		editor->priv->typing_timeout_id = 0;
	}

    nemo_query_editor_changed_force (editor, TRUE);
}

static void
content_case_button_toggled_cb (GtkWidget *toggle, NemoQueryEditor *editor)
{
    g_settings_set_boolean (nemo_search_preferences,
                            NEMO_PREFERENCES_SEARCH_CONTENT_CASE,
                            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->priv->content_case_toggle)));
}

static void
file_case_button_toggled_cb (GtkWidget *toggle, NemoQueryEditor *editor)
{
    g_settings_set_boolean (nemo_search_preferences,
                            NEMO_PREFERENCES_SEARCH_FILE_CASE,
                            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->priv->file_case_toggle)));
}

static void
regex_button_toggled_cb (GtkWidget *toggle, NemoQueryEditor *editor)
{
    g_settings_set_boolean (nemo_search_preferences,
                            NEMO_PREFERENCES_SEARCH_CONTENT_REGEX,
                            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->priv->regex_toggle)));
}

static void
file_recurse_button_toggled_cb (GtkWidget *toggle, NemoQueryEditor *editor)
{
    g_settings_set_boolean (nemo_search_preferences,
                            NEMO_PREFERENCES_SEARCH_FILES_RECURSIVELY,
                            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->priv->file_recurse_toggle)));
}

static gboolean
on_key_press_event (GtkWidget    *widget,
                    GdkEvent     *event,
                    gpointer user_data)
{
    NemoQueryEditor *editor = NEMO_QUERY_EDITOR (user_data);

    if ((event->key.state & gtk_accelerator_get_default_mod_mask ()) == 0) {
        // if (event->key.keyval == GDK_KEY_Up) {
            // popup_favorites (NEMO_QUERY_EDITOR (user_data), event, FALSE);
        // } else
        if (event->key.keyval == GDK_KEY_Tab) {
            GList *focus_iter = editor->priv->focus_chain;

            while (focus_iter->data != widget) {
                focus_iter = focus_iter->next;
            }

            gtk_widget_grab_focus (GTK_WIDGET (focus_iter->next->data));

            return GDK_EVENT_STOP;
        }
        if (event->key.keyval == GDK_KEY_Escape) {

            g_signal_emit (editor, signals[CANCEL], 0);

            return GDK_EVENT_STOP;
        }
    }

    return GDK_EVENT_PROPAGATE;
}

static void
search_icon_clicked_cb (GtkWidget             *widget,
                        GtkEntryIconPosition   position,
                        GdkEvent              *event,
                        gpointer               user_data)
{
    NemoQueryEditor *editor;

    if (position == GTK_ENTRY_ICON_PRIMARY) {
        return;
    }

    editor = NEMO_QUERY_EDITOR (user_data);

    if ((event->button.state & gtk_accelerator_get_default_mod_mask ()) == 0 && event->button.button == 1) {
        nemo_query_editor_changed (editor);
    }
}

// static void
// file_combo_iter_changed (GtkWidget  *widget,
//                          GParamSpec *spec,
//                          gpointer    data)
// {
//     NemoQueryEditor *editor = NEMO_QUERY_EDITOR (data);

//     if (editor->priv->focus_frozen) {
//         return;
//     }

//     gtk_widget_grab_focus (editor->priv->file_entry);

//     editor->priv->last_focus_widget = editor->priv->file_entry;
//     gtk_editable_set_position (GTK_EDITABLE (editor->priv->file_entry), -1);
// }

// static void
// content_combo_iter_changed (GtkWidget  *widget,
//                             GParamSpec *spec,
//                             gpointer    data)
// {
//     NemoQueryEditor *editor = NEMO_QUERY_EDITOR (data);

//     if (editor->priv->focus_frozen) {
//         return;
//     }

//     editor->priv->last_focus_widget = editor->priv->content_entry;
//     gtk_editable_set_position (GTK_EDITABLE (editor->priv->content_entry), -1);
// }

static void
entry_focus_changed (GtkWidget  *widget,
                     GParamSpec *spec,
                     gpointer    data)
{
    NemoQueryEditor *editor = NEMO_QUERY_EDITOR (data);

    if (gtk_widget_is_focus (widget)) {
        editor->priv->last_focus_widget = widget;
    }
}

// static void
// setup_entry_history (NemoQueryEditor *editor,
//                      GtkComboBoxText *combo,
//                      const gchar     *settings_key)
// {
//     gchar **history_entries;
//     gchar *active_text;
//     gint i, n_entries;

//     gtk_combo_box_text_remove_all (combo);

//     history_entries = g_settings_get_strv (nemo_search_preferences, settings_key);
//     n_entries = g_strv_length (history_entries);

//     for (i = 0; i < n_entries; i++) {
//         const gchar *entry = history_entries[i];

//         gtk_combo_box_text_prepend_text (combo, entry);
//     }

//     g_strfreev (history_entries);

//     editor->priv->focus_frozen = TRUE;

//     active_text = gtk_combo_box_text_get_active_text (combo);

//     if (active_text != NULL && active_text[0] != '\0') {
//         gtk_combo_box_set_active (GTK_COMBO_BOX (combo), n_entries - 1);
//     }

//     g_free (active_text);
 
//     editor->priv->focus_frozen = FALSE;
// }

// static void
// update_history_from_entry (NemoQueryEditor  *editor,
//                            const gchar      *key,
//                            gchar           **history,
//                            GtkWidget        *widget)
// {
//     GPtrArray *array;
//     gchar *current_search;
//     gint i;
    
//     current_search = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));
//     g_strstrip (current_search); // sanitize;

//     if (g_strcmp0 (current_search, "") == 0) {
//         return;
//     }

//     array = g_ptr_array_new_full (0, g_free);

//     g_ptr_array_add (array, (gpointer) g_strdup (current_search));

//     if (history != NULL) {
//         for (i = 0; i < g_strv_length ((gchar **) history); i++) {
//             if (g_strcmp0 (history[i], current_search) == 0) {
//                 continue;
//             }

//             g_ptr_array_add (array, (gpointer) g_strdup (history[i]));

//             if (array->len == g_settings_get_int (nemo_search_preferences,
//                                                   NEMO_PREFERENCES_SEARCH_CONTENT_HISTORY_LENGTH)) {
//                 break;
//             }
//         }
//     }

//     g_ptr_array_add (array, NULL);
//     g_settings_set_strv (nemo_search_preferences, key, (const gchar * const *) array->pdata);
//     g_ptr_array_free (array, TRUE);
//     g_free (current_search);
// }

// static void
// update_histories (NemoQueryEditor *editor)
// {
//     NemoQueryEditorPrivate *priv;
//     gchar **file_history, **content_history;

//     priv = editor->priv;

//     file_history = g_settings_get_strv (nemo_search_preferences, NEMO_PREFERENCES_SEARCH_FILE_HISTORY);
//     content_history = g_settings_get_strv (nemo_search_preferences, NEMO_PREFERENCES_SEARCH_CONTENT_HISTORY);

//     update_history_from_entry (editor,
//                                NEMO_PREFERENCES_SEARCH_FILE_HISTORY,
//                                file_history,
//                                editor->priv->file_entry);

//     update_history_from_entry (editor,
//                                NEMO_PREFERENCES_SEARCH_CONTENT_HISTORY,
//                                content_history,
//                                editor->priv->content_entry);

//     g_strfreev (file_history);
//     g_strfreev (content_history);

//     setup_entry_history (editor,
//                          GTK_COMBO_BOX_TEXT (priv->file_entry_combo),
//                          NEMO_PREFERENCES_SEARCH_FILE_HISTORY);

//     setup_entry_history (editor,
//                          GTK_COMBO_BOX_TEXT (priv->content_entry_combo),
//                          NEMO_PREFERENCES_SEARCH_CONTENT_HISTORY);
// }

// static void
// apply_privacy_pref (NemoQueryEditor *editor)
// {
//     NemoQueryEditorPrivate *priv = editor->priv;

//     priv->history_enabled = g_settings_get_boolean (cinnamon_privacy_preferences,
//                                                             NEMO_PREFERENCES_RECENT_ENABLED);

//     if (!priv->history_enabled) {
//         g_settings_reset (nemo_search_preferences, NEMO_PREFERENCES_SEARCH_FILE_HISTORY);
//         g_settings_reset (nemo_search_preferences, NEMO_PREFERENCES_SEARCH_CONTENT_HISTORY);
//     }

//     setup_entry_history (editor,
//                          GTK_COMBO_BOX_TEXT (priv->file_entry_combo),
//                          NEMO_PREFERENCES_SEARCH_FILE_HISTORY);
//     setup_entry_history (editor,
//                          GTK_COMBO_BOX_TEXT (priv->content_entry_combo),
//                          NEMO_PREFERENCES_SEARCH_CONTENT_HISTORY);
// }

static void
nemo_query_editor_init (NemoQueryEditor *editor)
{
    NemoQueryEditorPrivate *priv;
    GtkBuilder *builder;
    GtkWidget *separator;

    editor->priv = G_TYPE_INSTANCE_GET_PRIVATE (editor,
                                                NEMO_TYPE_QUERY_EDITOR,
                                                NemoQueryEditorPrivate);
    priv = editor->priv;
    priv->base_uri = NULL;

    builder = gtk_builder_new ();
    gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
    if (!gtk_builder_add_from_resource (builder, "/org/nemo/nemo-search-bar.glade", NULL)) {
        g_object_unref (builder);
        return;
    }
    priv->builder = builder;

    gtk_orientable_set_orientation (GTK_ORIENTABLE (editor), GTK_ORIENTATION_VERTICAL);

    priv->infobar = GTK_WIDGET (gtk_builder_get_object (builder, "search_bar"));
    gtk_box_pack_start (GTK_BOX (editor), priv->infobar, TRUE, TRUE, 0);

    priv->content_main_box = GTK_WIDGET (gtk_builder_get_object (builder, "content_main_box"));

    // TODO: Need to make a custom combo so entries can be deleted using 'Del' key
    // priv->file_entry_combo = GTK_WIDGET (gtk_builder_get_object (builder, "file_entry_combo"));

    // g_signal_connect (priv->file_entry_combo,
    //                   "notify::active-id",
    //                   G_CALLBACK (file_combo_iter_changed),
    //                   editor);

    // priv->content_entry_combo = GTK_WIDGET (gtk_builder_get_object (builder, "content_entry_combo"));

    // g_signal_connect (priv->content_entry_combo,
    //                   "notify::active-id",
    //                   G_CALLBACK (content_combo_iter_changed),
    //                   editor);

    priv->file_entry = GTK_WIDGET (gtk_builder_get_object (builder, "file_search_entry"));
    g_signal_connect (priv->file_entry,
                      "activate",
                      G_CALLBACK (entry_activate_cb),
                      editor);
    g_signal_connect (priv->file_entry,
                      "icon-press",
                      G_CALLBACK (search_icon_clicked_cb),
                      editor);
    g_signal_connect (priv->file_entry,
                      "notify::is-focus",
                      G_CALLBACK (entry_focus_changed),
                      editor);

    priv->content_entry = GTK_WIDGET (gtk_builder_get_object (builder, "content_search_entry"));
    g_signal_connect (priv->content_entry,
                      "activate",
                      G_CALLBACK (entry_activate_cb),
                      editor);
    g_signal_connect (priv->content_entry,
                      "icon-press",
                      G_CALLBACK (search_icon_clicked_cb),
                      editor);
    g_signal_connect (priv->content_entry,
                      "notify::is-focus",
                      G_CALLBACK (entry_focus_changed),
                      editor);

    priv->content_case_toggle = GTK_WIDGET (gtk_builder_get_object (builder, "content_search_case_toggle"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->content_case_toggle),
                                  g_settings_get_boolean (nemo_search_preferences,
                                                          NEMO_PREFERENCES_SEARCH_CONTENT_CASE));

    g_signal_connect (priv->content_case_toggle,
                      "toggled",
                      G_CALLBACK (content_case_button_toggled_cb),
                      editor);

    priv->file_case_toggle = GTK_WIDGET (gtk_builder_get_object (builder, "file_search_case_toggle"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->file_case_toggle),
                                  g_settings_get_boolean (nemo_search_preferences,
                                                          NEMO_PREFERENCES_SEARCH_FILE_CASE));

    g_signal_connect (priv->file_case_toggle,
                      "toggled",
                      G_CALLBACK (file_case_button_toggled_cb),
                      editor);

    priv->regex_toggle = GTK_WIDGET (gtk_builder_get_object (builder, "content_search_regex_toggle"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->regex_toggle),
                                  g_settings_get_boolean (nemo_search_preferences,
                                                          NEMO_PREFERENCES_SEARCH_CONTENT_REGEX));

    g_signal_connect (priv->regex_toggle,
                      "toggled",
                      G_CALLBACK (regex_button_toggled_cb),
                      editor);

    priv->file_recurse_toggle = GTK_WIDGET (gtk_builder_get_object (builder, "file_search_recurse_toggle"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->file_recurse_toggle),
                                  g_settings_get_boolean (nemo_search_preferences,
                                                          NEMO_PREFERENCES_SEARCH_FILES_RECURSIVELY));

    g_signal_connect (priv->file_recurse_toggle,
                      "toggled",
                      G_CALLBACK (file_recurse_button_toggled_cb),
                      editor);

    gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "button_box")));

    separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start (GTK_BOX (editor), separator, FALSE, FALSE, 0);
    g_object_bind_property (priv->infobar, "visible",
                            separator, "visible",
                            G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

    priv->focus_chain = g_list_append (NULL, priv->file_entry);
    priv->focus_chain = g_list_append (priv->focus_chain, priv->content_entry);
    priv->focus_chain->prev = g_list_last (priv->focus_chain);
    priv->focus_chain->prev->next = priv->focus_chain;

    // g_signal_connect_swapped (cinnamon_privacy_preferences,
    //                           "changed::" NEMO_PREFERENCES_RECENT_ENABLED,
    //                           G_CALLBACK (apply_privacy_pref),
    //                           editor);

    // apply_privacy_pref (editor);

#ifdef ENABLE_TRACKER // No options currently supported with tracker.
    gtk_widget_hide (priv->content_main_box);
    gtk_widget_hide (priv->file_recurse_toggle);
    gtk_widget_hide (priv->file_case_toggle);
#endif

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

    // if (editor->priv->history_enabled) {
    //     update_histories (editor);
    // }
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
    gchar *file_search_text = NULL;
    const gchar *content_search_text = NULL;

	if (editor == NULL || editor->priv == NULL || editor->priv->file_entry == NULL) {
		return NULL;
	}

	file_search_text = get_sanitized_file_search_string (editor);
    content_search_text = gtk_entry_get_text (GTK_ENTRY (editor->priv->content_entry));

    if (g_strcmp0 (file_search_text, "") == 0) {
        g_free (file_search_text);
        file_search_text = g_strdup ("*");
    }

	query = nemo_query_new ();
    nemo_query_set_file_pattern (query, file_search_text);
	nemo_query_set_content_pattern (query, content_search_text);
    nemo_query_set_content_case_sensitive (query, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->priv->content_case_toggle)));
    nemo_query_set_file_case_sensitive (query, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->priv->file_case_toggle)));
    nemo_query_set_use_regex (query, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->priv->regex_toggle)));
    nemo_query_set_recurse (query, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->priv->file_recurse_toggle)));

	add_location_to_query (editor, query);

    g_free (file_search_text);

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
    gchar *file_pattern = NULL;
    gchar *content_pattern = NULL;

    if (query != NULL) {
        file_pattern = nemo_query_get_file_pattern (query);
        content_pattern = nemo_query_get_file_pattern (query);
    }

    if (!file_pattern) {
        file_pattern = g_strdup ("");
    }

    if (!content_pattern) {
        content_pattern = g_strdup ("");
    }

	editor->priv->change_frozen = TRUE;
    gtk_entry_set_text (GTK_ENTRY (editor->priv->file_entry), file_pattern);
	gtk_entry_set_text (GTK_ENTRY (editor->priv->content_entry), content_pattern);
    gtk_widget_grab_focus (editor->priv->file_entry);

	g_free (editor->priv->current_uri);
	editor->priv->current_uri = NULL;

	if (query != NULL) {
		editor->priv->current_uri = nemo_query_get_location (query);
	}

    g_free (editor->priv->last_set_query_file_pattern);
	g_free (editor->priv->last_set_query_content_pattern);
    editor->priv->last_set_query_file_pattern = file_pattern;
	editor->priv->last_set_query_content_pattern = content_pattern;

	editor->priv->change_frozen = FALSE;
}

void
nemo_query_editor_set_active (NemoQueryEditor *editor,
                              gchar           *base_uri,
                              gboolean         active)
{
    g_return_if_fail (NEMO_IS_QUERY_EDITOR (editor));

    if (active) {
        GFile *base;

        gtk_widget_show (editor->priv->infobar);
        gtk_widget_queue_resize (GTK_WIDGET (editor->priv->infobar));

        const gchar *content_forbidden_dirs[] = {
            "file:///dev",
            "file:///proc",
            "file:///run",
            "file:///sys",
            NULL
        };

        g_clear_pointer (&editor->priv->base_uri, g_free);
        editor->priv->base_uri = base_uri;

        base = g_file_new_for_uri (base_uri);

        if (g_file_is_native (base) && !g_strv_contains (content_forbidden_dirs, base_uri)) {
            gtk_widget_set_sensitive (editor->priv->content_main_box, TRUE);
            gtk_entry_set_placeholder_text (GTK_ENTRY (editor->priv->content_entry), "");
        } else {
            gtk_widget_set_sensitive (editor->priv->content_main_box, FALSE);
            gtk_entry_set_placeholder_text (GTK_ENTRY (editor->priv->content_entry),
                                            _("Not supported in this location"));
        }

        g_object_unref (base);

        g_signal_connect (editor->priv->file_entry,
                          "key-press-event",
                          G_CALLBACK (on_key_press_event),
                          editor);
        g_signal_connect (editor->priv->content_entry,
                          "key-press-event",
                          G_CALLBACK (on_key_press_event),
                          editor);
        editor->priv->last_focus_widget = editor->priv->file_entry;
    } else {
        g_signal_handlers_disconnect_by_func (editor->priv->file_entry,
                                              on_key_press_event,
                                              editor);
        g_signal_handlers_disconnect_by_func (editor->priv->content_entry,
                                              on_key_press_event,
                                              editor);

        gtk_widget_hide (editor->priv->infobar);

        editor->priv->last_focus_widget = NULL;
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

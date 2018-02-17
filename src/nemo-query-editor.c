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
 * General Public License for more priv.
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

typedef struct
{
    GtkWidget *infobar;
	GtkWidget *entry;

	gboolean change_frozen;
	guint typing_timeout_id;
	gboolean is_visible;
	GtkWidget *vbox;

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

static void
nemo_query_editor_dispose (GObject *object)
{
	NemoQueryEditor *editor;

	editor = NEMO_QUERY_EDITOR (object);

    g_clear_pointer (&editor->priv->base_uri, g_free);

	if (editor->priv->typing_timeout_id > 0) {
		g_source_remove (editor->priv->typing_timeout_id);
		editor->priv->typing_timeout_id = 0;
	}
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
	if (editor->priv->typing_timeout_id > 0) {
		g_source_remove (editor->priv->typing_timeout_id);
		editor->priv->typing_timeout_id = 0;
	}

	nemo_query_editor_changed_force (editor, TRUE);
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

static void
entry_changed_cb (GtkWidget *entry, NemoQueryEditor *editor)
{
	if (editor->priv->change_frozen) {
		return;
	}

	if (editor->priv->typing_timeout_id > 0) {
		g_source_remove (editor->priv->typing_timeout_id);
	}

	editor->priv->typing_timeout_id =
		g_timeout_add (TYPING_TIMEOUT,
			       typing_timeout_cb,
			       editor);
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

    gtk_orientable_set_orientation (GTK_ORIENTABLE (editor), GTK_ORIENTATION_VERTICAL);

    priv->infobar = gtk_info_bar_new ();
    gtk_box_pack_start (GTK_BOX (editor), priv->infobar, TRUE, TRUE, 0);
    gtk_widget_set_no_show_all (priv->infobar, TRUE);
    gtk_info_bar_set_message_type (GTK_INFO_BAR (priv->infobar), GTK_MESSAGE_OTHER);

    priv->entry = gtk_search_entry_new ();
    gtk_box_pack_start (GTK_BOX (gtk_info_bar_get_content_area (GTK_INFO_BAR (priv->infobar))),
                       priv->entry,
                       TRUE, TRUE, 0);
    gtk_widget_show (priv->entry);

    g_signal_connect (priv->entry,
                      "activate",
                      G_CALLBACK (entry_activate_cb),
                      editor);

    g_signal_connect (priv->entry,
                      "changed",
                      G_CALLBACK (entry_changed_cb),
                      editor);

    separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start (GTK_BOX (editor), separator, TRUE, TRUE, 0);

    g_object_bind_property (priv->infobar, "visible",
                            separator, "visible",
                            G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

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
	const char *query_text;
    gchar *cmp;
	NemoQuery *query;

	if (editor == NULL || editor->priv == NULL || editor->priv->entry == NULL) {
		return NULL;
	}

	query_text = gtk_entry_get_text (GTK_ENTRY (editor->priv->entry));

    cmp = g_strdup (query_text);
    g_strstrip (cmp);

    if (g_strcmp0 (cmp, "") == 0) {
        g_free (cmp);
        return NULL;
    }

    g_free (cmp);

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
    } else {
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
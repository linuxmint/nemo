/* nemo-filter-bar.c
 *
 * Copyright (C) 2026 Linux Mint
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "nemo-filter-bar.h"
#include <glib/gi18n.h>
#include <string.h>

enum {
    CANCEL,
    COMMIT,
    PREEDIT_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* The entry never takes keyboard focus: the file view keeps it and feeds
 * its key events through our input method context, which is what lets
 * IM hotkeys, compose sequences and preedit work while type-to-filter
 * is active. The entry shows the committed text followed by the preedit. */
struct _NemoFilterBar {
    GtkBox parent_instance;

    GtkWidget *infobar;
    GtkWidget *entry;
    GtkWidget *clear_button;

    GtkIMContext *im_context;
    gchar *text;
    gchar *preedit;
    guint discard_commits_id;
};

/* Long enough for an input method to answer a reset, short enough that
 * nobody has started composing again. */
#define DISCARD_COMMITS_TIMEOUT_MS 200

G_DEFINE_TYPE (NemoFilterBar, nemo_filter_bar, GTK_TYPE_BOX)

static void
clear_button_clicked_cb (GtkButton *button, NemoFilterBar *bar)
{
    g_signal_emit (bar, signals[CANCEL], 0);
}

static void
update_entry (NemoFilterBar *bar)
{
    const gchar *text = bar->text != NULL ? bar->text : "";
    const gchar *preedit = bar->preedit != NULL ? bar->preedit : "";
    PangoAttrList *attrs = NULL;
    gchar *display;

    display = g_strconcat (text, preedit, NULL);
    gtk_entry_set_text (GTK_ENTRY (bar->entry), display);

    if (preedit[0] != '\0') {
        PangoAttribute *underline = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);

        underline->start_index = strlen (text);
        underline->end_index = strlen (display);
        attrs = pango_attr_list_new ();
        pango_attr_list_insert (attrs, underline);
    }

    gtk_entry_set_attributes (GTK_ENTRY (bar->entry), attrs);
    g_clear_pointer (&attrs, pango_attr_list_unref);
    g_free (display);

    nemo_filter_bar_update_cursor_location (bar);
}

static gboolean
is_navigation_key (guint keyval)
{
    switch (keyval) {
        case GDK_KEY_Up:
        case GDK_KEY_Down:
        case GDK_KEY_Left:
        case GDK_KEY_Right:
        case GDK_KEY_Home:
        case GDK_KEY_End:
        case GDK_KEY_Page_Up:
        case GDK_KEY_Page_Down:
        case GDK_KEY_Begin:
        case GDK_KEY_KP_Up:
        case GDK_KEY_KP_Down:
        case GDK_KEY_KP_Left:
        case GDK_KEY_KP_Right:
        case GDK_KEY_KP_Home:
        case GDK_KEY_KP_End:
        case GDK_KEY_KP_Page_Up:
        case GDK_KEY_KP_Page_Down:
        case GDK_KEY_KP_Begin:
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
        case GDK_KEY_Tab:
        case GDK_KEY_ISO_Left_Tab:
        case GDK_KEY_Escape:
        case GDK_KEY_BackSpace:
        case GDK_KEY_Delete:
        case GDK_KEY_KP_Delete:
        case GDK_KEY_Insert:
        case GDK_KEY_KP_Insert:
            return TRUE;
        default:
            return FALSE;
    }
}

static gboolean
discard_commits_timeout_cb (gpointer user_data)
{
    NemoFilterBar *bar = NEMO_FILTER_BAR (user_data);

    bar->discard_commits_id = 0;
    return G_SOURCE_REMOVE;
}

static void
im_commit_cb (GtkIMContext *im_context, const gchar *text, NemoFilterBar *bar)
{
    if (bar->discard_commits_id == 0 && text != NULL && text[0] != '\0') {
        g_signal_emit (bar, signals[COMMIT], 0, text);
    }
}

static void
im_preedit_changed_cb (GtkIMContext *im_context, NemoFilterBar *bar)
{
    g_free (bar->preedit);
    gtk_im_context_get_preedit_string (im_context, &bar->preedit, NULL, NULL);

    update_entry (bar);
    g_signal_emit (bar, signals[PREEDIT_CHANGED], 0);
}

static void
nemo_filter_bar_finalize (GObject *object)
{
    NemoFilterBar *bar = NEMO_FILTER_BAR (object);

    g_clear_handle_id (&bar->discard_commits_id, g_source_remove);
    g_clear_object (&bar->im_context);
    g_free (bar->text);
    g_free (bar->preedit);

    G_OBJECT_CLASS (nemo_filter_bar_parent_class)->finalize (object);
}

static void
nemo_filter_bar_init (NemoFilterBar *bar)
{
    GtkWidget *content_area;
    GtkWidget *action_area;
    GtkWidget *separator;

    gtk_orientable_set_orientation (GTK_ORIENTABLE (bar), GTK_ORIENTATION_VERTICAL);

    bar->infobar = gtk_info_bar_new ();
    gtk_info_bar_set_message_type (GTK_INFO_BAR (bar->infobar), GTK_MESSAGE_OTHER);
    gtk_box_pack_start (GTK_BOX (bar), bar->infobar, FALSE, FALSE, 0);
    gtk_widget_show (bar->infobar);

    content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (bar->infobar));
    action_area = gtk_info_bar_get_action_area (GTK_INFO_BAR (bar->infobar));

    bar->entry = gtk_entry_new ();
    gtk_widget_set_hexpand (bar->entry, TRUE);
    gtk_widget_set_can_focus (bar->entry, FALSE);
    gtk_box_pack_start (GTK_BOX (content_area), bar->entry, TRUE, TRUE, 0);
    gtk_widget_show (bar->entry);

    bar->clear_button = gtk_button_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief (GTK_BUTTON (bar->clear_button), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus (bar->clear_button, FALSE);
    gtk_container_add (GTK_CONTAINER (action_area), bar->clear_button);
    gtk_widget_show (bar->clear_button);

    separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start (GTK_BOX (bar), separator, FALSE, FALSE, 0);
    gtk_widget_show (separator);

    g_signal_connect (bar->clear_button, "clicked",
                      G_CALLBACK (clear_button_clicked_cb), bar);

    bar->im_context = gtk_im_multicontext_new ();
    g_signal_connect (bar->im_context, "commit",
                      G_CALLBACK (im_commit_cb), bar);
    g_signal_connect (bar->im_context, "preedit-changed",
                      G_CALLBACK (im_preedit_changed_cb), bar);
}

static void
nemo_filter_bar_class_init (NemoFilterBarClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nemo_filter_bar_finalize;

    signals[CANCEL] =
        g_signal_new ("cancel",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[COMMIT] =
        g_signal_new ("commit",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__STRING,
                      G_TYPE_NONE, 1, G_TYPE_STRING);

    signals[PREEDIT_CHANGED] =
        g_signal_new ("preedit-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}

GtkWidget *
nemo_filter_bar_new (void)
{
    return g_object_new (NEMO_TYPE_FILTER_BAR, NULL);
}

const char *
nemo_filter_bar_get_text (NemoFilterBar *bar)
{
    g_return_val_if_fail (NEMO_IS_FILTER_BAR (bar), NULL);
    return bar->text != NULL ? bar->text : "";
}

/**
 * nemo_filter_bar_set_text:
 * @bar: a #NemoFilterBar
 * @text: the committed filter text, or %NULL
 *
 * Any input method preedit in progress is shown after @text.
 */
void
nemo_filter_bar_set_text (NemoFilterBar *bar, const char *text)
{
    g_return_if_fail (NEMO_IS_FILTER_BAR (bar));

    g_free (bar->text);
    bar->text = g_strdup (text);
    update_entry (bar);
}

gboolean
nemo_filter_bar_has_preedit (NemoFilterBar *bar)
{
    g_return_val_if_fail (NEMO_IS_FILTER_BAR (bar), FALSE);
    return bar->preedit != NULL && bar->preedit[0] != '\0';
}

/**
 * nemo_filter_bar_filter_key_event:
 * @bar: a #NemoFilterBar
 * @event: a key press or release
 *
 * Offers @event to the input method. Text it produces is reported through
 * the #NemoFilterBar::commit signal.
 *
 * Returns: %TRUE if the input method consumed the event
 */
gboolean
nemo_filter_bar_filter_key_event (NemoFilterBar *bar, GdkEventKey *event)
{
    g_return_val_if_fail (NEMO_IS_FILTER_BAR (bar), FALSE);

    if (event->type == GDK_KEY_PRESS) {
        g_clear_handle_id (&bar->discard_commits_id, g_source_remove);
    }

    /* Async input methods hold each key for a round trip, so navigation
     * keys skip it unless a preedit needs them. */
    if (!nemo_filter_bar_has_preedit (bar) && is_navigation_key (event->keyval)) {
        return FALSE;
    }

    return gtk_im_context_filter_keypress (bar->im_context, event);
}

/**
 * nemo_filter_bar_set_client_widget:
 * @bar: a #NemoFilterBar
 * @widget: the realized widget feeding us key events, or %NULL
 *
 * The bar itself isn't realized until it's first revealed, so the input
 * method's client window follows @widget's toplevel instead.
 */
void
nemo_filter_bar_set_client_widget (NemoFilterBar *bar, GtkWidget *widget)
{
    GdkWindow *window = NULL;

    g_return_if_fail (NEMO_IS_FILTER_BAR (bar));

    if (widget != NULL) {
        window = gtk_widget_get_window (gtk_widget_get_toplevel (widget));
    }

    gtk_im_context_set_client_window (bar->im_context, window);
}

/**
 * nemo_filter_bar_reset_im:
 * @bar: a #NemoFilterBar
 *
 * Discards any pending preedit or compose sequence. Anything the input
 * method commits from it, whether during the reset, on an immediate
 * focus-out, or asynchronously, is dropped until the next key press we
 * see or a short timeout. Wayland input methods consume keys in the
 * compositor, so we may never see one.
 */
void
nemo_filter_bar_reset_im (NemoFilterBar *bar)
{
    g_return_if_fail (NEMO_IS_FILTER_BAR (bar));

    g_clear_handle_id (&bar->discard_commits_id, g_source_remove);
    bar->discard_commits_id = g_timeout_add (DISCARD_COMMITS_TIMEOUT_MS,
                                             discard_commits_timeout_cb, bar);
    gtk_im_context_reset (bar->im_context);

    /* Async input methods may not clear their preedit until later. */
    if (nemo_filter_bar_has_preedit (bar)) {
        g_clear_pointer (&bar->preedit, g_free);
        update_entry (bar);
        g_signal_emit (bar, signals[PREEDIT_CHANGED], 0);
    }
}

void
nemo_filter_bar_set_im_focus (NemoFilterBar *bar, gboolean focused)
{
    g_return_if_fail (NEMO_IS_FILTER_BAR (bar));

    if (focused) {
        gtk_im_context_focus_in (bar->im_context);
        nemo_filter_bar_update_cursor_location (bar);
    } else {
        gtk_im_context_focus_out (bar->im_context);
    }
}

void
nemo_filter_bar_update_cursor_location (NemoFilterBar *bar)
{
    GtkEntry *entry;
    GtkWidget *toplevel;
    PangoRectangle pos;
    GdkRectangle rect = { 0, 0, 0, 0 };
    gint layout_x, layout_y;

    g_return_if_fail (NEMO_IS_FILTER_BAR (bar));

    entry = GTK_ENTRY (bar->entry);
    toplevel = gtk_widget_get_toplevel (GTK_WIDGET (bar));

    pango_layout_get_cursor_pos (gtk_entry_get_layout (entry),
                                 strlen (gtk_entry_get_text (entry)),
                                 &pos, NULL);
    gtk_entry_get_layout_offsets (entry, &layout_x, &layout_y);

    /* Fails until the bar is first revealed; the revealer's size-allocate
     * gets us here again once it is. */
    if (!gtk_widget_is_toplevel (toplevel) ||
        !gtk_widget_translate_coordinates (bar->entry, toplevel,
                                           layout_x + PANGO_PIXELS (pos.x),
                                           layout_y + PANGO_PIXELS (pos.y),
                                           &rect.x, &rect.y)) {
        return;
    }

    rect.height = PANGO_PIXELS (pos.height);
    gtk_im_context_set_cursor_location (bar->im_context, &rect);
}

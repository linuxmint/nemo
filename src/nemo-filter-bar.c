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

enum {
    CANCEL,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _NemoFilterBar {
    GtkBox parent_instance;

    GtkWidget *entry;
    GtkWidget *clear_button;
};

G_DEFINE_TYPE (NemoFilterBar, nemo_filter_bar, GTK_TYPE_BOX)

static void
clear_button_clicked_cb (GtkButton *button, NemoFilterBar *bar)
{
    g_signal_emit (bar, signals[CANCEL], 0);
}

static void
nemo_filter_bar_init (NemoFilterBar *bar)
{
    GtkStyleContext *context;

    gtk_orientable_set_orientation (GTK_ORIENTABLE (bar), GTK_ORIENTATION_HORIZONTAL);
    gtk_box_set_spacing (GTK_BOX (bar), 6);
    gtk_widget_set_margin_start (GTK_WIDGET (bar), 6);
    gtk_widget_set_margin_end (GTK_WIDGET (bar), 6);
    gtk_widget_set_margin_top (GTK_WIDGET (bar), 4);
    gtk_widget_set_margin_bottom (GTK_WIDGET (bar), 4);

    context = gtk_widget_get_style_context (GTK_WIDGET (bar));
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_INFO);

    bar->entry = gtk_entry_new ();
    gtk_widget_set_hexpand (bar->entry, TRUE);
    gtk_widget_set_can_focus (bar->entry, FALSE);
    gtk_box_pack_start (GTK_BOX (bar), bar->entry, TRUE, TRUE, 0);
    gtk_widget_show (bar->entry);

    bar->clear_button = gtk_button_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief (GTK_BUTTON (bar->clear_button), GTK_RELIEF_NONE);
    gtk_widget_set_can_focus (bar->clear_button, FALSE);
    gtk_box_pack_end (GTK_BOX (bar), bar->clear_button, FALSE, FALSE, 0);
    gtk_widget_show (bar->clear_button);

    g_signal_connect (bar->clear_button, "clicked",
                      G_CALLBACK (clear_button_clicked_cb), bar);
}

static void
nemo_filter_bar_class_init (NemoFilterBarClass *klass)
{

    signals[CANCEL] =
        g_signal_new ("cancel",
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
    return gtk_entry_get_text (GTK_ENTRY (bar->entry));
}

void
nemo_filter_bar_set_text (NemoFilterBar *bar, const char *text)
{
    g_return_if_fail (NEMO_IS_FILTER_BAR (bar));
    gtk_entry_set_text (GTK_ENTRY (bar->entry), text ? text : "");
}

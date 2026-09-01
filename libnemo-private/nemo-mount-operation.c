/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
 * Nemo
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
 *
 */

#include <config.h>

#include "nemo-mount-operation.h"

#include <eel/eel-gtk-extensions.h>
#include <glib/gi18n.h>

#define MOUNT_OPERATION_HANDLER_NAME "org.gtk.MountOperationHandler"
#define MOUNT_OPERATION_HANDLER_PATH "/org/gtk/MountOperationHandler"
#define MOUNT_OPERATION_HANDLER_INTERFACE "org.Gtk.MountOperationHandler"

/* GtkMountOperation delegates to MOUNT_OPERATION_HANDLER_NAME when the session
 * provides it. Cinnamon does, but Nemo can run under compositors that don't.
 * Without the handler, GtkMountOperation crashes when asked to list the
 * processes blocking an unmount in a Wayland session - the lookup it uses
 * is X11-only and unguarded.
 *
 * The lookup only happens for processes it is asked to list, so we hand its
 * dialog an empty process list and fold the process names into the message
 * instead. Everything else stays with Gtk.
 *
 * Nothing is broken in X11 sessions, so none of this is set up for them.
 */

static GDBusProxy *
get_mount_operation_handler_proxy (void)
{
    static GDBusProxy *proxy = NULL;
    static gsize once_init = 0;

    if (g_once_init_enter (&once_init)) {
        proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                               G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
                                               G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                               G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                               NULL,
                                               MOUNT_OPERATION_HANDLER_NAME,
                                               MOUNT_OPERATION_HANDLER_PATH,
                                               MOUNT_OPERATION_HANDLER_INTERFACE,
                                               NULL, NULL);

        g_once_init_leave (&once_init, 1);
    }

    return proxy;
}

static gboolean
mount_operation_handler_available (void)
{
    GDBusProxy *proxy;
    gchar *name_owner;

    proxy = get_mount_operation_handler_proxy ();

    if (proxy == NULL) {
        return FALSE;
    }

    name_owner = g_dbus_proxy_get_name_owner (proxy);

    if (name_owner == NULL) {
        return FALSE;
    }

    g_free (name_owner);

    return TRUE;
}

static gchar *
get_process_name (GPid pid)
{
    gchar *path;
    gchar *contents;
    gchar *name = NULL;

    /* /proc/pid/cmdline entries are nul-separated, the first is the executable */
    path = g_strdup_printf ("/proc/%d/cmdline", (gint) pid);
    contents = NULL;

    if (g_file_get_contents (path, &contents, NULL, NULL) && *contents != '\0') {
        name = g_path_get_basename (contents);
    }

    g_free (contents);
    g_free (path);

    if (name == NULL) {
        path = g_strdup_printf ("/proc/%d/comm", (gint) pid);
        contents = NULL;

        if (g_file_get_contents (path, &contents, NULL, NULL)) {
            name = g_strchomp (contents);
        }

        g_free (path);
    }

    if (name == NULL) {
        name = g_strdup_printf ("%s (PID %d)", _("Unknown"), (gint) pid);
    }

    return name;
}

static gchar *
append_process_names (const gchar *message,
                      GArray      *processes)
{
    GHashTable *seen;
    GString *str;
    guint i;

    if (processes == NULL || processes->len == 0) {
        return g_strdup (message);
    }

    /* Gtk renders this message as markup */
    str = g_string_new (message);
    g_string_append_c (str, '\n');

    seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    for (i = 0; i < processes->len; i++) {
        gchar *name = get_process_name (g_array_index (processes, GPid, i));

        if (g_hash_table_contains (seen, name)) {
            g_free (name);
            continue;
        }

        gchar *escaped = g_markup_escape_text (name, -1);
        g_string_append_printf (str, "\n%s", escaped);
        g_free (escaped);

        g_hash_table_add (seen, name);
    }

    g_hash_table_destroy (seen);

    return g_string_free (str, FALSE);
}

static GtkWidget *
find_descendant (GtkWidget *widget,
                 GType      type)
{
    GList *children, *l;
    GtkWidget *found = NULL;

    if (G_TYPE_CHECK_INSTANCE_TYPE (widget, type)) {
        return widget;
    }

    if (!GTK_IS_CONTAINER (widget)) {
        return NULL;
    }

    children = gtk_container_get_children (GTK_CONTAINER (widget));

    for (l = children; l != NULL && found == NULL; l = l->next) {
        found = find_descendant (l->data, type);
    }

    g_list_free (children);

    return found;
}

// Hide the empty process list - this is broken in Wayland (and the reason
// our mount operation wrapper exists in the first place). This is hacky
// but Gtk3 is stable!
static void
hide_empty_process_list (GList *toplevels_before)
{
    GList *toplevels, *l;

    toplevels = gtk_window_list_toplevels ();

    for (l = toplevels; l != NULL; l = l->next) {
        GtkWidget *list;

        if (g_list_find (toplevels_before, l->data) != NULL) {
            continue;
        }

        list = find_descendant (l->data, GTK_TYPE_SCROLLED_WINDOW);

        if (list != NULL) {
            gtk_widget_set_no_show_all (list, TRUE);
            gtk_widget_hide (list);
        }
    }

    g_list_free (toplevels);
}

static void
show_processes_cb (GMountOperation *operation,
                   const gchar     *message,
                   GArray          *processes,
                   GStrv            choices,
                   gpointer         user_data)
{
    GList *toplevels_before;
    GArray *empty;
    gchar *detailed_message;

    detailed_message = append_process_names (message, processes);
    empty = g_array_new (FALSE, FALSE, sizeof (GPid));
    toplevels_before = gtk_window_list_toplevels ();

    g_signal_stop_emission_by_name (operation, "show-processes");
    G_MOUNT_OPERATION_GET_CLASS (operation)->show_processes (operation,
                                                             detailed_message,
                                                             empty,
                                                             (const gchar **) choices);

    hide_empty_process_list (toplevels_before);

    g_list_free (toplevels_before);
    g_array_unref (empty);
    g_free (detailed_message);
}

GMountOperation *
nemo_mount_operation_new (GtkWindow *parent_window)
{
    GMountOperation *operation;

    operation = gtk_mount_operation_new (parent_window);

    if (eel_check_is_wayland () && !mount_operation_handler_available ()) {
        g_signal_connect (operation, "show-processes",
                          G_CALLBACK (show_processes_cb), NULL);
    }

    return operation;
}

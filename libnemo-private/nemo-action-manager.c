/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

*/

#include "nemo-action-manager.h"
#include "nemo-directory.h"
#include "nemo-action.h"
#include <libnemo-private/nemo-global-preferences.h>
#define DEBUG_FLAG NEMO_DEBUG_ACTIONS
#include <libnemo-private/nemo-debug.h>
#include "nemo-file-utilities.h"


G_DEFINE_TYPE (NemoActionManager, nemo_action_manager, G_TYPE_OBJECT);

static void     refresh_actions                 (NemoActionManager *action_manager, NemoDirectory *directory);
static void     add_action_to_action_list       (NemoActionManager *action_manager, NemoFile *file);

static gpointer parent_class;

enum 
{
  PROP_0,
  PROP_CONDITIONS
};

enum {
    CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
actions_changed (NemoDirectory *directory,
                          GList         *files,
                          gpointer       callback_data)
{
    NemoActionManager *action_manager;

    action_manager = NEMO_ACTION_MANAGER (callback_data);

    if (directory != NULL) {
        gchar *uri = nemo_directory_get_uri (directory);
        DEBUG ("Action directory '%s' changed, refreshing its actions.", uri);
        g_free (uri);
    }

    refresh_actions (action_manager, directory);
}

static void
plugin_prefs_changed (GSettings *settings, gchar *key, gpointer user_data)
{
    g_return_if_fail (NEMO_IS_ACTION_MANAGER (user_data));

    DEBUG ("Enabled actions changed, refreshing all.");
    refresh_actions (NEMO_ACTION_MANAGER (user_data), NULL);
}

static void
add_directory_to_directory_list (NemoActionManager *action_manager,
                                 NemoDirectory     *directory,
                                 GList            **directory_list,
                                 GCallback          changed_callback)
{
    NemoFileAttributes attributes;

    if (g_list_find (*directory_list, directory) == NULL) {
        nemo_directory_ref (directory);

        attributes = NEMO_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT;

        // We get an immediate callback from this, which will trigger our first actions_changed
        // callback. At nemo startup this will typically be before the directory has discovered its
        // children, but we can't ignore it, as the directory may also already be loaded and have a valid
        // list of children (and that's the only callback we'll receive).
        nemo_directory_file_monitor_add (directory, directory_list,
                             FALSE, attributes,
                             (NemoDirectoryCallback)changed_callback, action_manager);

        g_signal_connect_object (directory, "files_added",
                     G_CALLBACK (changed_callback), action_manager, 0);
        g_signal_connect_object (directory, "files_changed",
                     G_CALLBACK (changed_callback), action_manager, 0);

        *directory_list = g_list_append (*directory_list, directory);
    }
}

static void
remove_directory_from_directory_list (NemoActionManager *action_manager,
                                          NemoDirectory *directory,
                                                 GList **directory_list,
                                               GCallback changed_callback)
{
    *directory_list = g_list_remove (*directory_list, directory);

    g_signal_handlers_disconnect_by_func (directory,
                          G_CALLBACK (changed_callback),
                          action_manager);

    nemo_directory_file_monitor_remove (directory, directory_list);

    nemo_directory_unref (directory);
}

static void
add_directory_to_actions_directory_list (NemoActionManager *action_manager,
                                             NemoDirectory *directory)
{
    add_directory_to_directory_list (action_manager, directory,
                                     &action_manager->actions_directory_list,
                                     G_CALLBACK (actions_changed));
}

static void
remove_directory_from_actions_directory_list (NemoActionManager *action_manager,
                                                  NemoDirectory *directory)
{
    remove_directory_from_directory_list (action_manager, directory,
                                          &action_manager->actions_directory_list,
                                          G_CALLBACK (actions_changed));
}

static gint
sort_file_list_cb (gconstpointer a, gconstpointer b)
{
    NemoFile *file_a;
    NemoFile *file_b;
    const char *s0;
    const char *s1;
    gint r;

    file_a = (NemoFile *) a;
    file_b = (NemoFile *) b;
    s0 = nemo_file_peek_name (file_a);
    s1 = nemo_file_peek_name (file_b);
    //
    // Order alphabetically
    // Compare ASCII values of file names char by char
    //    < 0:  a < b
    //   == 0:  a == b
    //    > 0:  a > b
    //
    do {
        char c0 = *s0++;
        char c1 = *s1++;
        if (c0 < c1) {
            r = -1;
            break;
        }
        if (c0 > c1) {
            r = 1;
            break;
        }
        if (c0 == 0) {  // Special case: Both strings are identical and we have hit the \0 char? => Return equal
            r = 0;
            break;
        }
    } while (TRUE);

    return r;
}

static void
process_directory_actions (NemoActionManager *action_manager,
                           NemoDirectory     *directory)
{
    GList *file_list, *node;

    gchar *uri = nemo_directory_get_uri (directory);
    DEBUG ("Processing directory: %s", uri);
    g_free (uri);

    file_list = nemo_directory_get_file_list (directory);
    file_list = g_list_sort (file_list, sort_file_list_cb);
    for (node = file_list; node != NULL; node = node->next) {
        NemoFile *file = node->data;
        if (!g_str_has_suffix (nemo_file_peek_name (file), ".nemo_action") ||
            !nemo_global_preferences_should_load_plugin (nemo_file_peek_name (file), NEMO_PLUGIN_PREFERENCES_DISABLED_ACTIONS))
            continue;

        DEBUG ("Found: %s", nemo_file_peek_name (file));
        add_action_to_action_list (action_manager, file);
    }
    nemo_file_list_free (file_list);
}

static void
set_up_actions_directories (NemoActionManager *action_manager)
{
    NemoDirectory *dir;
    gchar *path, *uri;
    gchar **data_dirs;
    guint i;

    DEBUG ("Setting up action directories");

    data_dirs = (gchar **) g_get_system_data_dirs ();

    for (i = 0; i < g_strv_length (data_dirs); i++) {
        path = nemo_action_manager_get_system_directory_path (data_dirs[i]);
        uri = g_filename_to_uri (path, NULL, NULL);

        dir = nemo_directory_get_by_uri (uri);

        if (dir != NULL) {
            DEBUG ("Adding system location '%s'", uri);
            add_directory_to_actions_directory_list (action_manager, dir);
            nemo_directory_unref (dir);
        }

        g_clear_pointer (&path, g_free);
        g_clear_pointer (&uri, g_free);
    }

    path = nemo_action_manager_get_user_directory_path ();
    uri = g_filename_to_uri (path, NULL, NULL);

    if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
        g_mkdir_with_parents (path, DEFAULT_NEMO_DIRECTORY_MODE);
    }

    dir = nemo_directory_get_by_uri (uri);

    if (dir != NULL) {
        DEBUG ("Adding user location '%s'", uri);
        add_directory_to_actions_directory_list (action_manager, dir);
        nemo_directory_unref (dir);
    }

    g_clear_pointer (&path, g_free);
    g_clear_pointer (&uri, g_free);
}

static char *
escape_action_name (const char *action_name,
            const char *prefix)
{
    GString *s;

    if (action_name == NULL) {
        return NULL;
    }
    
    s = g_string_new (prefix);

    while (*action_name != 0) {
        switch (*action_name) {
        case '\\':
            g_string_append (s, "\\\\");
            break;
        case '/':
            g_string_append (s, "\\s");
            break;
        case '&':
            g_string_append (s, "\\a");
            break;
        case '"':
            g_string_append (s, "\\q");
            break;
        default:
            g_string_append_c (s, *action_name);
        }

        action_name ++;
    }
    return g_string_free (s, FALSE);
}

static void
on_action_condition_changed (NemoActionManager *action_manager)
{
    DEBUG ("Action manager (%p) received action condition changed.  Sending our own changed.",
           action_manager);

    g_signal_emit (action_manager, signals[CHANGED], 0);
}

static void
add_action_to_action_list (NemoActionManager *action_manager, NemoFile *file)
{
    gchar *uri;
    gchar *action_name;
    NemoAction *action;

    uri = nemo_file_get_uri (file);

    action_name = escape_action_name (uri, "action_");
    gchar *path = g_filename_from_uri (uri, NULL, NULL);

    action = nemo_action_new (action_name, path);

    g_free (path);
    g_free (uri);
    g_free (action_name);

    if (action == NULL) {
        return;
    }

    g_signal_connect_swapped (action,
                              "condition-changed",
                              G_CALLBACK (on_action_condition_changed),
                              action_manager);

    action_manager->actions = g_list_append (action_manager->actions, action);
}

static void
void_actions_for_directory (NemoActionManager *action_manager, NemoDirectory *directory)
{
    GFile *dir = nemo_directory_get_location (directory);
    const gchar *dir_path = g_file_peek_path (dir);
    GList *new_list = NULL;
    GList *l, *tmp;

    DEBUG ("Removing existing actions in %s:", dir_path);

    for (l = action_manager->actions; l != NULL; l = l->next) {
        NemoAction *action = NEMO_ACTION (l->data);

        if (g_strcmp0 (dir_path, action->parent_dir) != 0) {
            new_list = g_list_prepend (new_list, g_object_ref (action));
        } else {
            DEBUG ("Found %s", action->key_file_path);
        }
    }

    g_object_unref (dir);

    tmp = action_manager->actions;
    action_manager->actions = new_list;
    g_list_free_full (tmp, g_object_unref);
}

static void
refresh_actions (NemoActionManager *action_manager, NemoDirectory *directory)
{
    action_manager->action_list_dirty = TRUE;

    if (directory != NULL) {
        void_actions_for_directory (action_manager, directory);
        process_directory_actions (action_manager, directory);
    } else {
        g_list_free_full (action_manager->actions, g_object_unref);
        action_manager->actions = NULL;

        GList *l;
        for (l = action_manager->actions_directory_list; l != NULL; l = l->next) {
            NemoDirectory *dir = NEMO_DIRECTORY (l->data);
            process_directory_actions (action_manager, dir);
        }
    }

    action_manager->action_list_dirty = FALSE;

    g_signal_emit (action_manager, signals[CHANGED], 0);
}

static void
nemo_action_manager_init (NemoActionManager *action_manager)
{
    action_manager->actions = NULL;
    action_manager->actions_directory_list = NULL;
}

void
nemo_action_manager_constructed (GObject *object)
{
    G_OBJECT_CLASS (parent_class)->constructed (object);

    NemoActionManager *action_manager = NEMO_ACTION_MANAGER (object);

    action_manager->action_list_dirty = TRUE;
    set_up_actions_directories (action_manager);

    g_signal_connect (nemo_plugin_preferences,
                      "changed::" NEMO_PLUGIN_PREFERENCES_DISABLED_ACTIONS,
                      G_CALLBACK (plugin_prefs_changed), action_manager);
}

NemoActionManager *
nemo_action_manager_new (void)
{
    return g_object_new (NEMO_TYPE_ACTION_MANAGER, NULL);
}

static void
nemo_action_manager_dispose (GObject *object)
{
    NemoActionManager *action_manager = NEMO_ACTION_MANAGER (object);

    if (action_manager->actions_directory_list != NULL) {
        GList *node, *copy;
        copy = nemo_directory_list_copy (action_manager->actions_directory_list);

        for (node = copy; node != NULL; node = node->next) {
            remove_directory_from_actions_directory_list (action_manager, node->data);
        }
        g_list_free (action_manager->actions_directory_list);
        action_manager->actions_directory_list = NULL;
        nemo_directory_list_free (copy);
    }

    g_signal_handlers_disconnect_by_func (nemo_plugin_preferences, G_CALLBACK (plugin_prefs_changed), action_manager);

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
nemo_action_manager_finalize (GObject *object)
{
    NemoActionManager *action_manager = NEMO_ACTION_MANAGER (object);

    g_list_free_full (action_manager->actions, g_object_unref);

    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nemo_action_manager_class_init (NemoActionManagerClass *klass)
{
    GObjectClass         *object_class = G_OBJECT_CLASS(klass);
    parent_class           = g_type_class_peek_parent (klass);
    object_class->finalize = nemo_action_manager_finalize;
    object_class->dispose = nemo_action_manager_dispose;
    object_class->constructed = nemo_action_manager_constructed;

    signals[CHANGED] =
        g_signal_new ("changed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NemoActionManagerClass, changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}

GList *
nemo_action_manager_list_actions (NemoActionManager *action_manager)
{
    return action_manager->action_list_dirty ? NULL : action_manager->actions;
}

gchar *
nemo_action_manager_get_system_directory_path (const gchar *data_dir)
{
    g_autofree gchar *nemo_path = NULL;
    g_autofree gchar *target = NULL;

    nemo_path = g_build_filename (data_dir, "nemo", NULL);

    // For symbolic links, we try to figure out its actual path
    // to prevent possible duplicate right-click menu items.
    target = g_file_read_link (nemo_path, NULL);

    if (target) {
        return g_build_filename (target, "actions", NULL);
    } else {
        return g_build_filename (nemo_path, "actions", NULL);
    }
}

gchar *
nemo_action_manager_get_user_directory_path (void)
{
    return g_build_filename (g_get_user_data_dir (), "nemo", "actions", NULL);
}

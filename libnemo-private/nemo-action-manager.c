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

static void     set_up_actions                 (NemoActionManager *action_manager);

static void     nemo_action_manager_constructed (GObject *object);

static void     nemo_action_manager_dispose (GObject *gobject);

static void     nemo_action_manager_finalize (GObject *gobject);

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
actions_added_or_changed (NemoDirectory *directory,
                          GList         *files,
                          gpointer       callback_data)
{
    NemoActionManager *action_manager;

    action_manager = NEMO_ACTION_MANAGER (callback_data);

    action_manager->action_list_dirty = TRUE;

    set_up_actions (action_manager);
}

static void
plugin_prefs_changed (GSettings *settings, gchar *key, gpointer user_data)
{
    actions_added_or_changed (NULL, NULL, user_data);
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

        attributes =
            NEMO_FILE_ATTRIBUTES_FOR_ICON |
            NEMO_FILE_ATTRIBUTE_INFO |
            NEMO_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT;

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
                                     G_CALLBACK (actions_added_or_changed));
}

static void
remove_directory_from_actions_directory_list (NemoActionManager *action_manager,
                                                  NemoDirectory *directory)
{
    remove_directory_from_directory_list (action_manager, directory,
                                          &action_manager->actions_directory_list,
                                          G_CALLBACK (actions_added_or_changed));
}

static void
set_up_actions_directories (NemoActionManager *action_manager)
{
    NemoDirectory *dir;
    gchar *path, *uri;
    gchar **data_dirs;
    guint i;

    data_dirs = (gchar **) g_get_system_data_dirs ();

    for (i = 0; i < g_strv_length (data_dirs); i++) {
        path = nemo_action_manager_get_system_directory_path (data_dirs[i]);
        uri = g_filename_to_uri (path, NULL, NULL);

        dir = nemo_directory_get_by_uri (uri);

        add_directory_to_actions_directory_list (action_manager, dir);

        nemo_directory_unref (dir);
        g_clear_pointer (&path, g_free);
        g_clear_pointer (&uri, g_free);
    }

    path = nemo_action_manager_get_user_directory_path ();
    uri = g_filename_to_uri (path, NULL, NULL);

    if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
        g_mkdir_with_parents (path, DEFAULT_NEMO_DIRECTORY_MODE);
    }

    dir = nemo_directory_get_by_uri (uri);

    add_directory_to_actions_directory_list (action_manager, dir);

    nemo_directory_unref (dir);
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
void_action_list (NemoActionManager *action_manager)
{
    GList *tmp = action_manager->actions;

    action_manager->actions = NULL;

    g_list_free_full (tmp, g_object_unref);
}

static gint
_cbSortFileList(gconstpointer a, gconstpointer b)
{
  gint r;
  NemoFile* pFileA;
  NemoFile* pFileB;
  const char* s0;
  const char* s1;

  pFileA = (NemoFile*)a;
  pFileB = (NemoFile*)b;
  s0 = nemo_file_peek_name(pFileA);
  s1 = nemo_file_peek_name(pFileB);
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
  } while (1);
  return r;
}

static void
set_up_actions (NemoActionManager *action_manager)
{
    GList *dir, *file_list, *node;
    NemoFile *file;
    NemoDirectory *directory;

    if (g_list_length (action_manager->actions) > 0)
        void_action_list (action_manager);

    for (dir = action_manager->actions_directory_list; dir != NULL; dir = dir->next) {
        directory = dir->data;
        file_list = nemo_directory_get_file_list (directory);
        file_list = g_list_sort(file_list, _cbSortFileList);
        for (node = file_list; node != NULL; node = node->next) {
            file = node->data;
            if (!g_str_has_suffix (nemo_file_peek_name (file), ".nemo_action") ||
                !nemo_global_preferences_should_load_plugin (nemo_file_peek_name (file), NEMO_PLUGIN_PREFERENCES_DISABLED_ACTIONS))
                continue;
            add_action_to_action_list (action_manager, file);
        }
        nemo_file_list_free (file_list);
    }

    action_manager->action_list_dirty = FALSE;

    g_signal_emit (action_manager, signals[CHANGED], 0);
}

static void
nemo_action_manager_init (NemoActionManager *action_manager)
{
    action_manager->actions = NULL;
    action_manager->actions_directory_list = NULL;
    action_manager->action_list_dirty = TRUE;
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

void
nemo_action_manager_constructed (GObject *object)
{
    G_OBJECT_CLASS (parent_class)->constructed (object);

    NemoActionManager *action_manager = NEMO_ACTION_MANAGER (object);

    set_up_actions_directories (action_manager);
    set_up_actions (action_manager);

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

GList *
nemo_action_manager_list_actions (NemoActionManager *action_manager)
{
    return action_manager->action_list_dirty ? NULL : action_manager->actions;
}

gchar *
nemo_action_manager_get_system_directory_path (const gchar *data_dir)
{
    const gchar *nemo_path, *target;

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

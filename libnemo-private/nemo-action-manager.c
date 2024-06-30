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
#include "nemo-file.h"
#include "nemo-directory.h"
#include "nemo-file-utilities.h"

#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-ui-utilities.h>
#define DEBUG_FLAG NEMO_DEBUG_ACTIONS
#include <libnemo-private/nemo-debug.h>

typedef struct {
    JsonParser *json_parser;
    GFileMonitor *config_dir_monitor;

    GHashTable *actions_by_uuid;
    GList *actions;

    GList *actions_directory_list;
    gboolean action_list_dirty;
} NemoActionManagerPrivate;

struct _NemoActionManager 
{
    GObject parent_instance;
};

G_DEFINE_TYPE_WITH_PRIVATE (NemoActionManager, nemo_action_manager, G_TYPE_OBJECT)

static void     refresh_actions                 (NemoActionManager *action_manager, NemoDirectory *directory);
static void     add_action_to_action_list       (NemoActionManager *action_manager, NemoFile *file);

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

    NemoActionManager *action_manager = NEMO_ACTION_MANAGER (user_data);

    DEBUG ("Enabled actions changed, refreshing all.");

    refresh_actions (action_manager, NULL);
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
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);
    add_directory_to_directory_list (action_manager, directory,
                                     &priv->actions_directory_list,
                                     G_CALLBACK (actions_changed));
}

static void
remove_directory_from_actions_directory_list (NemoActionManager *action_manager,
                                                  NemoDirectory *directory)
{
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);
    remove_directory_from_directory_list (action_manager, directory,
                                          &priv->actions_directory_list,
                                          G_CALLBACK (actions_changed));
}

static gint
sort_file_list_cb (gconstpointer a, gconstpointer b)
{
    NemoFile *file_a;
    NemoFile *file_b;
    const char *s0;
    const char *s1;

    file_a = (NemoFile *) a;
    file_b = (NemoFile *) b;
    s0 = nemo_file_peek_name (file_a);
    s1 = nemo_file_peek_name (file_b);

    return g_strcmp0 (s0, s1);
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
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);

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

    priv->actions = g_list_append (priv->actions, action);
    g_hash_table_insert (priv->actions_by_uuid, action->uuid, action);
}

static void
void_actions_for_directory (NemoActionManager *action_manager, NemoDirectory *directory)
{
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);

    GFile *dir = nemo_directory_get_location (directory);
    const gchar *dir_path = g_file_peek_path (dir);
    GList *new_list = NULL;
    GList *l, *tmp;

    DEBUG ("Removing existing actions in %s:", dir_path);

    for (l = priv->actions; l != NULL; l = l->next) {
        NemoAction *action = NEMO_ACTION (l->data);

        if (g_strcmp0 (dir_path, action->parent_dir) != 0) {
            new_list = g_list_prepend (new_list, g_object_ref (action));
        } else {
            DEBUG ("Found %s", action->key_file_path);
            g_hash_table_remove (priv->actions_by_uuid, action->uuid);
        }
    }

    new_list = g_list_reverse (new_list);

    g_object_unref (dir);

    tmp = priv->actions;
    priv->actions = new_list;
    g_list_free_full (tmp, g_object_unref);
}

#define LAYOUT_FILENAME "actions-tree.json"

static void
reload_actions_layout (NemoActionManager *action_manager)
{
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);

    GError *error = NULL;
    g_autofree gchar *path = NULL;

    DEBUG ("Attempting to load action layout.");

    g_clear_object (&priv->json_parser);

    JsonParser *parser = json_parser_new ();
    path = g_build_filename (g_get_user_config_dir (), "nemo", LAYOUT_FILENAME, NULL);

    if (!json_parser_load_from_file (parser, path, &error)) {
        if (error != NULL) {
            DEBUG ("JsonParser couldn't load file: %s\n", error->message);
            if (error->code != G_FILE_ERROR_NOENT) {
                g_critical ("Error loading action layout file: %s\n", error->message);
            }
        }

        g_clear_error (&error);
        g_clear_object (&parser);
        return;
    }

    priv->json_parser = parser;
    DEBUG ("Loaded action layout file: %s\n", path);
}

static void
nemo_config_dir_changed (GFileMonitor      *monitor,
                         GFile             *file,
                         GFile             *other_file,
                         GFileMonitorEvent  event_type,
                         gpointer           user_data)
{
    NemoActionManager *action_manager = NEMO_ACTION_MANAGER (user_data);

    g_autofree gchar *basename = g_file_get_basename (file);

    if (g_strcmp0 (basename, LAYOUT_FILENAME) != 0) {
        return;
    }

    if (event_type != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT) {
        return;
    }

    refresh_actions (action_manager, NULL);
}

static void
monitor_nemo_config_dir (NemoActionManager *action_manager)
{
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);
    GFile *file;
    GFileMonitor *monitor;

    g_autofree gchar *path = g_build_filename (g_get_user_config_dir (), "nemo", NULL);

    file = g_file_new_for_path (path);
    monitor = g_file_monitor_directory (file, G_FILE_MONITOR_WATCH_MOVES | G_FILE_MONITOR_SEND_MOVED, NULL, NULL);
    g_object_unref (file);
    g_signal_connect (monitor, "changed", G_CALLBACK (nemo_config_dir_changed), action_manager);
    priv->config_dir_monitor = monitor;
}

static void
refresh_actions (NemoActionManager *action_manager, NemoDirectory *directory)
{
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);

    priv->action_list_dirty = TRUE;

    reload_actions_layout (action_manager);

    if (directory != NULL) {
        void_actions_for_directory (action_manager, directory);
        process_directory_actions (action_manager, directory);
    } else {
        g_list_free_full (priv->actions, g_object_unref);
        priv->actions = NULL;

        g_hash_table_destroy (priv->actions_by_uuid);
        priv->actions_by_uuid = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

        GList *l;
        for (l = priv->actions_directory_list; l != NULL; l = l->next) {
            NemoDirectory *dir = NEMO_DIRECTORY (l->data);
            process_directory_actions (action_manager, dir);
        }
    }

    priv->action_list_dirty = FALSE;
    g_signal_emit (action_manager, signals[CHANGED], 0);
}

static void
nemo_action_manager_init (NemoActionManager *action_manager)
{
}

void
nemo_action_manager_constructed (GObject *object)
{
    G_OBJECT_CLASS (nemo_action_manager_parent_class)->constructed (object);

    NemoActionManager *action_manager = NEMO_ACTION_MANAGER (object);
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);

    priv->action_list_dirty = TRUE;
    priv->actions_by_uuid = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

    set_up_actions_directories (action_manager);
    reload_actions_layout (action_manager);
    monitor_nemo_config_dir (action_manager);

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
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);

    if (priv->actions_directory_list != NULL) {
        GList *node, *copy;
        copy = nemo_directory_list_copy (priv->actions_directory_list);

        for (node = copy; node != NULL; node = node->next) {
            remove_directory_from_actions_directory_list (action_manager, node->data);
        }
        g_list_free (priv->actions_directory_list);
        priv->actions_directory_list = NULL;
        nemo_directory_list_free (copy);
    }

    g_clear_object (&priv->config_dir_monitor);
    g_clear_object (&priv->json_parser);
    g_signal_handlers_disconnect_by_func (nemo_plugin_preferences, G_CALLBACK (plugin_prefs_changed), action_manager);

    G_OBJECT_CLASS (nemo_action_manager_parent_class)->dispose (object);
}

static void
nemo_action_manager_finalize (GObject *object)
{
    NemoActionManager *action_manager = NEMO_ACTION_MANAGER (object);
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);

    g_hash_table_destroy (priv->actions_by_uuid);
    g_list_free_full (priv->actions, g_object_unref);

    G_OBJECT_CLASS (nemo_action_manager_parent_class)->finalize (object);
}

static void
nemo_action_manager_class_init (NemoActionManagerClass *klass)
{
    GObjectClass         *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = nemo_action_manager_finalize;
    object_class->dispose = nemo_action_manager_dispose;
    object_class->constructed = nemo_action_manager_constructed;

    signals[CHANGED] = g_signal_new ("changed",
                                     NEMO_TYPE_ACTION_MANAGER,
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL, NULL,
                                     G_TYPE_NONE, 0);
}

GList *
nemo_action_manager_list_actions (NemoActionManager *action_manager)
{
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);

    return priv->action_list_dirty ? NULL : priv->actions;
}

NemoAction *
nemo_action_manager_get_action (NemoActionManager *action_manager,
                                const gchar       *uuid)
{
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);
    NemoAction *action;

    if (priv->action_list_dirty || priv->actions == NULL) {
        return NULL;
    }

    action = g_hash_table_lookup (priv->actions_by_uuid, uuid);
    if (action) {
        return g_object_ref (action);
    }
    else {
        return NULL;
    }
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

typedef struct
{
    NemoActionManager                *action_manager;
    JsonReader                       *reader;
    GHashTable                       *used_uuids;

    GError                           *error;

    NemoActionManagerIterFunc         func;
    gpointer                          user_data;
} ActionsIterData;

static gboolean
parse_level (ActionsIterData *idata,
             const gchar     *path);

static void
item_error (JsonReader  *reader,
            GError     **error,
            const gchar *uuid,
            const gchar *custom_message)
{
    if (*error != NULL) {
        return;
    }

    const GError *internal_error = json_reader_get_error (reader);
    gchar *error_message;

    if (internal_error != NULL) {
        error_message = g_strdup_printf ("(%s) - %s: %s", uuid, custom_message, internal_error->message);
    } else {
        error_message = g_strdup_printf ("(%s) - %s", uuid, custom_message);
    }

    *error = g_error_new_literal (json_parser_error_quark (),
                                  JSON_PARSER_ERROR_INVALID_DATA,
                                  error_message);
    g_free (error_message);
}

static gboolean
parse_item (ActionsIterData *idata,
            const gchar               *path)
{
    const gchar *type = NULL;
    const gchar *uuid = NULL;

    JsonReader *reader = idata->reader;

    if (!json_reader_read_member (reader, "uuid") || json_reader_get_null_value (reader)) {
        item_error (reader, &idata->error, path, "Action layout member is missing mandatory value");
        return FALSE;
    }
    uuid = json_reader_get_string_value (reader);
    json_reader_end_member (reader);

    DEBUG ("Parsing action layout entry for '%s'.", uuid);

    if (!json_reader_read_member  (reader, "type") || json_reader_get_null_value (reader)) {
        item_error (reader, &idata->error, uuid, "Action layout member is missing mandatory value");
        return FALSE;
    }
    type = json_reader_get_string_value (reader);
    json_reader_end_member (reader);

    if (g_strcmp0 (type, "separator") == 0) {
        DEBUG ("Adding separator to UI.");

        idata->func (idata->action_manager,
                     NULL,
                     GTK_UI_MANAGER_SEPARATOR,
                     path,
                     idata->user_data);
        return TRUE;
    }

    const gchar *user_label = NULL;
    const gchar *user_icon = NULL;

    // user-label and user-icon are optional, no error if they're missing or null.
    if (json_reader_read_member (reader, "user-label") && !json_reader_get_null_value (reader)) {
        user_label = json_reader_get_string_value (reader);
    }
    json_reader_end_member (reader);

    if (json_reader_read_member (reader, "user-icon") && !json_reader_get_null_value (reader)) {
        user_icon = json_reader_get_string_value (reader);
    }
    json_reader_end_member (reader);

    if (g_strcmp0 (type, "action") == 0) {
        NemoAction *action;
        g_autofree gchar *lookup_uuid = nemo_make_action_uuid_for_path (uuid);

        action = nemo_action_manager_get_action (idata->action_manager, lookup_uuid);

        if (action == NULL) {
            // Don't fail a bad action, we'll show a message and keep going.
            DEBUG ("Missing action '%s' ignored in action layout", lookup_uuid);
            return TRUE;
        }

        if (user_label != NULL) {
            nemo_action_override_label (action, user_label);
        }

        if (user_icon != NULL) {
            nemo_action_override_icon (action, user_icon);
        }

        DEBUG ("Adding action '%s' to UI.", action->uuid);

        idata->func (idata->action_manager,
                     GTK_ACTION (action),
                     GTK_UI_MANAGER_MENUITEM,
                     path,
                     idata->user_data);
        g_object_unref (action);

        g_hash_table_add (idata->used_uuids, action->uuid);

        return TRUE;
    }
    else
    if (g_strcmp0 (type, "submenu") == 0) {
        GString *uuid_str;
        g_autofree gchar *safe_uuid = NULL;
        g_autofree gchar *next_path = NULL;

        // A submenu's UUID is just its label, which can have /'s.  This messes with the action 'paths' being
        // constructed when adding to the UI.
        uuid_str = g_string_new (uuid);
        g_string_replace (uuid_str, "/", "::", 0);
        safe_uuid = g_string_free (uuid_str, FALSE);

        GtkAction *submenu = gtk_action_new (safe_uuid, user_label ? user_label : "<submenus need a label>", NULL, NULL);

        if (user_icon != NULL) {
            gtk_action_set_icon_name (submenu, user_icon);
        }

        // A submenu gets added to the same level (path) as its sibling menuitems
        DEBUG ("Adding submenu '%s' to UI.", uuid);

        idata->func (idata->action_manager,
                     submenu,
                     GTK_UI_MANAGER_MENU,
                     path,
                     idata->user_data);

        if (!json_reader_read_member (reader, "children") || json_reader_get_null_value (reader)) {
            item_error (reader, &idata->error, uuid, "Layout submenu is missing mandatory 'children' field");
            return FALSE;
        }

        // But its children will be added to the next level path (which is the old path + the menu uuid)
        // Recursion happens, but 'next_path' becomes 'path' on the next level, and is never modified or
        // freed...
        if (path != NULL) {
            next_path = g_strdup_printf ("%s/%s", path, safe_uuid);
        }
        else
        {
            next_path = g_strdup (safe_uuid);
        }

        if (parse_level (idata, next_path)) {
            json_reader_end_member (reader);
            return TRUE;
        }
        // ...So next_path will be freed once return to this block after parse_level.
    }

    return FALSE;
}

static gboolean
parse_level (ActionsIterData *idata,
             const gchar     *path)
{
    JsonReader *reader = idata->reader;

    if (json_reader_is_array (reader)) {
        guint len = json_reader_count_elements (reader);
        DEBUG ("Processing %d children of '%s'.", len, path == NULL ? "root" : path);

        gint i;
        for (i = 0; i < len; i++) {
            if (!json_reader_read_element (reader, i)) {
                idata->error = g_error_copy (json_reader_get_error (reader));
                return FALSE;
            }

            if (!parse_item (idata, path)) {
                return FALSE;
            }

            json_reader_end_element (reader);
        }

        return TRUE;
    } else {
        idata->error = g_error_new (json_parser_error_quark (),
                                     JSON_PARSER_ERROR_INVALID_DATA,
                                     "Current layout depth lacks an array of action, submenu and separator definitions");
    }

    return FALSE;
}

static gboolean
iter_actions (NemoActionManager  *action_manager,
              ActionsIterData    *idata)
{
    JsonReader *reader = idata->reader;

    idata->error = NULL;

    if (json_reader_read_member (reader, "toplevel")) {
        if (parse_level (idata, NULL)) {
            json_reader_end_member (reader);
        }
    }

    if (idata->error == NULL && json_reader_get_error (reader)) {
        idata->error = g_error_copy (json_reader_get_error (reader));
    }

    if (idata->error != NULL) {
        g_critical ("Structured actions couldn't be set up: %p %s", idata->error, idata->error->message);
        return FALSE;
    }

    return TRUE;
}

static JsonReader *
get_layout_reader (NemoActionManager *action_manager)
{
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);

    if (priv->action_list_dirty || priv->actions == NULL || priv->json_parser == NULL) {
        return NULL;
    }

    JsonReader *reader;
    reader = json_reader_new (json_parser_get_root (priv->json_parser));

    return reader;
}

void
nemo_action_manager_iterate_actions (NemoActionManager                *action_manager,
                                     NemoActionManagerIterFunc         func,
                                     gpointer                          user_data)
{
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);
    JsonReader *reader = get_layout_reader (action_manager);
    gboolean ret = FALSE;

    GHashTable *used_uuids = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

    if (reader != NULL) {
        ActionsIterData idata;

        idata.action_manager = action_manager;
        idata.reader = reader;
        idata.func = func;
        idata.user_data = user_data;
        idata.used_uuids = used_uuids;

        ret = iter_actions (action_manager, &idata);

        g_object_unref (idata.reader);
    }

    if (!ret) {
        g_hash_table_remove_all (used_uuids);
    }

    if (g_hash_table_size (used_uuids) < g_hash_table_size (priv->actions_by_uuid)) {
        NemoAction *action;
        GList *node;

        for (node = priv->actions; node != NULL; node = node->next) {
            action = node->data;

            if (g_hash_table_contains (used_uuids, action->uuid)) {
                continue;
            }

            func (action_manager,
                  GTK_ACTION (action),
                  GTK_UI_MANAGER_MENUITEM,
                  NULL,
                  user_data);
        }
    }

    g_hash_table_destroy (used_uuids);
}

void
nemo_action_manager_add_action_ui (NemoActionManager   *manager,
                                   GtkUIManager        *ui_manager,
                                   GtkAction           *action,
                                   const gchar         *action_path,
                                   GtkActionGroup      *action_group,
                                   guint                merge_id,
                                   const gchar        **placeholder_paths,
                                   GtkUIManagerItemType type,
                                   GCallback            activate_callback,
                                   gpointer             user_data)
{
    if (type != GTK_UI_MANAGER_SEPARATOR) {
        if (type == GTK_UI_MANAGER_MENUITEM) {
            g_signal_handlers_disconnect_by_func (action,
                                                  activate_callback,
                                                  user_data);

            g_signal_connect (action, "activate",
                              G_CALLBACK (activate_callback),
                              user_data);
        }

        gtk_action_group_add_action (action_group, action);
        gtk_action_set_visible (action, FALSE);
    }

    gint i = 0;
    while (placeholder_paths[i] != NULL) {
        g_autofree gchar *full_path = NULL;
        const gchar *name;

        if (action_path != NULL) {
            full_path = g_strdup_printf ("%s/%s", placeholder_paths[i], action_path);
        }
        else {
            full_path = g_strdup (placeholder_paths[i]);
        }

        if (type == GTK_UI_MANAGER_SEPARATOR) {
            name = NULL;
        }
        else {
            name = gtk_action_get_name (action);
        }

        gtk_ui_manager_add_ui (ui_manager,
                               merge_id,
                               full_path,
                               name,
                               name,
                               type,
                               FALSE);
        i++;
    }
}

void
nemo_action_manager_update_action_states (NemoActionManager *action_manager,
                                          GtkActionGroup    *action_group,
                                          GList             *selection,
                                          NemoFile          *parent,
                                          gboolean           for_places,
                                          GtkWindow         *window)
{
    GList *l, *actions;

    actions = gtk_action_group_list_actions (action_group);

    for (l = actions; l != NULL; l = l->next) {
        if (!NEMO_IS_ACTION (l->data)) {
            DEBUG ("Skipping submenu '%s' (visibility managed by GtkUIManager)", gtk_action_get_name (GTK_ACTION (l->data)));
            gtk_action_set_visible (GTK_ACTION (l->data), TRUE);
            continue;
        }

        nemo_action_update_display_state (NEMO_ACTION (l->data), selection, parent, for_places, window);
    }

    g_list_free (actions);
}

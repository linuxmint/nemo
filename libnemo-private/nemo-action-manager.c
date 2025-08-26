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
#include "nemo-file-utilities.h"

#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-ui-utilities.h>
#define DEBUG_FLAG NEMO_DEBUG_ACTIONS
#include <libnemo-private/nemo-debug.h>

typedef struct {
    JsonParser *json_parser;
    GList *dir_monitors;

    GHashTable *actions_by_uuid;
    GList *actions;

    GList *actions_directory_list;
    gboolean action_list_dirty;
    gint layout_timestamp;
} NemoActionManagerPrivate;

struct _NemoActionManager 
{
    GObject parent_instance;
};

G_DEFINE_TYPE_WITH_PRIVATE (NemoActionManager, nemo_action_manager, G_TYPE_OBJECT)

static void     refresh_actions                 (NemoActionManager *action_manager, const gchar *directory);
static void     add_action_to_action_list       (NemoActionManager *action_manager, const gchar *path);

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
actions_changed (GFileMonitor      *monitor,
                 GFile             *file,
                 GFile             *other_file,
                 GFileMonitorEvent  event_type,
                 gpointer           user_data)
{
    NemoActionManager *action_manager;

    action_manager = NEMO_ACTION_MANAGER (user_data);

    g_autofree gchar *dir = g_path_get_dirname (g_file_peek_path (file));
    DEBUG ("Action directory '%s' changed, refreshing its actions.", dir);

    refresh_actions (action_manager, dir);
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
add_directory_to_actions_directory_list (NemoActionManager *action_manager,
                                         const gchar       *path)
{
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);

    if (g_list_find_custom (priv->actions_directory_list, path, (GCompareFunc) g_strcmp0) == NULL) {
        GFile *file;
        GFileMonitor *monitor;

        file = g_file_new_for_path(path);
        monitor = g_file_monitor_directory (file, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL);
        g_object_unref (file);
        g_signal_connect (monitor, "changed", G_CALLBACK (actions_changed), action_manager);

        priv->dir_monitors = g_list_prepend (priv->dir_monitors, monitor);
        priv->actions_directory_list = g_list_prepend (priv->actions_directory_list, g_strdup (path));
    }
}

static void
process_directory_actions (NemoActionManager *action_manager,
                           const gchar       *directory)
{
    GDir *dir;
    gchar **disabled_actions;

    DEBUG ("Processing directory: %s", directory);

    disabled_actions = g_settings_get_strv (nemo_plugin_preferences, NEMO_PLUGIN_PREFERENCES_DISABLED_ACTIONS);

    dir = g_dir_open (directory, 0, NULL);

    if (dir) {
        const char *name;

        while ((name = g_dir_read_name (dir))) {
            if (g_str_has_suffix (name, ".nemo_action")) {
                if (g_strv_contains ((const gchar* const*) disabled_actions, name)) {
                    DEBUG ("Skipping disabled action: '%s'", name);
                    continue;
                }

                gchar *filename;
                filename = g_build_filename (directory, name, NULL);

                DEBUG ("Found: %s", filename);
                add_action_to_action_list (action_manager, filename);
                g_free (filename);
            }
        }

        g_dir_close (dir);
    }

    g_strfreev (disabled_actions);
}

static void
set_up_actions_directories (NemoActionManager *action_manager)
{
    gchar *path;
    gchar **data_dirs;
    guint i;

    DEBUG ("Setting up action directories");

    data_dirs = (gchar **) g_get_system_data_dirs ();

    for (i = 0; i < g_strv_length (data_dirs); i++) {
        path = nemo_action_manager_get_system_directory_path (data_dirs[i]);

        if (path != NULL) {
            DEBUG ("Adding system location '%s'", path);
            add_directory_to_actions_directory_list (action_manager, path);
        }

        g_clear_pointer (&path, g_free);
    }

    path = nemo_action_manager_get_user_directory_path ();

    if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
        g_mkdir_with_parents (path, DEFAULT_NEMO_DIRECTORY_MODE);
    }

    if (path != NULL) {
        DEBUG ("Adding user location '%s'", path);
        add_directory_to_actions_directory_list (action_manager, path);
    }

    g_free (path);
}

static char *
create_action_name (NemoActionManager *manager,
                    const char        *uri)
{
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (manager);
    GString *s;
    g_autofree gchar *prefix = NULL;

    if (uri == NULL) {
        return NULL;
    }

    prefix = g_strdup_printf ("action_%d_", priv->layout_timestamp);
    s = g_string_new (prefix);

    while (*uri != 0) {
        switch (*uri) {
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
            g_string_append_c (s, *uri);
        }

        uri++;
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
add_action_to_action_list (NemoActionManager *action_manager,
                           const gchar       *path)
{
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);

    gchar *uri;
    gchar *action_name;
    NemoAction *action;

    uri = g_filename_to_uri (path, NULL, NULL);

    action_name = create_action_name (action_manager, uri);
    action = nemo_action_new (action_name, path);

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
void_actions_for_directory (NemoActionManager *action_manager,
                            const gchar       *directory)
{
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);

    GList *new_list = NULL;
    GList *l, *tmp;

    DEBUG ("Removing existing actions in %s:", directory);

    for (l = priv->actions; l != NULL; l = l->next) {
        NemoAction *action = NEMO_ACTION (l->data);

        if (g_strcmp0 (directory, action->parent_dir) != 0) {
            new_list = g_list_prepend (new_list, g_object_ref (action));
        } else {
            DEBUG ("Found %s", action->key_file_path);
            g_hash_table_remove (priv->actions_by_uuid, action->uuid);
        }
    }

    new_list = g_list_reverse (new_list);

    tmp = priv->actions;
    priv->actions = new_list;
    g_list_free_full (tmp, g_object_unref);
}

#define LAYOUT_FILENAME "actions-tree.json"

static void
reload_actions_layout (NemoActionManager *action_manager)
{
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);
    GFile *file;
    GFileInfo *info;
    guint64 timestamp;

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

        priv->layout_timestamp = 0;
        g_clear_error (&error);
        g_clear_object (&parser);
        return;
    }

    file = g_file_new_for_path (path);
    info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED, G_FILE_QUERY_INFO_NONE, NULL, NULL);
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED)) {
        timestamp = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
    } else {
        timestamp = (guint64) (ABS (g_get_monotonic_time ()));
    }
    priv->layout_timestamp = (gint) (timestamp % 10000);
    g_object_unref (info);
    g_object_unref (file);

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
    priv->dir_monitors = g_list_prepend (priv->dir_monitors, monitor);
}

static gint
sort_actions (gconstpointer a, gconstpointer b)
{
    NemoAction *action_a = (NemoAction *) a;
    NemoAction *action_b = (NemoAction *) b;

    return g_strcmp0 (action_a->uuid, action_b->uuid);
}

static void
refresh_actions (NemoActionManager *action_manager,
                 const gchar       *directory)
{
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);

    priv->action_list_dirty = TRUE;

    reload_actions_layout (action_manager);

    if (directory != NULL) {
        void_actions_for_directory (action_manager, directory);
        process_directory_actions (action_manager, directory);
    } else {
        GList *l;

        for (l = priv->actions_directory_list; l != NULL; l = l->next) {
            void_actions_for_directory (action_manager, (gchar *) l->data);
            process_directory_actions (action_manager, (gchar *) l->data);
        }
    }

    priv->actions = g_list_sort (priv->actions, sort_actions);

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
    monitor_nemo_config_dir (action_manager);
    reload_actions_layout (action_manager);
    refresh_actions (action_manager, NULL);

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
        g_list_free_full (priv->actions_directory_list, g_free);
        priv->actions_directory_list = NULL;
    }

    if (priv->dir_monitors != NULL) {
        g_list_free_full (priv->dir_monitors, g_object_unref);
        priv->dir_monitors = NULL;
    }

    g_clear_object (&priv->json_parser);
    g_signal_handlers_disconnect_by_func (nemo_plugin_preferences, G_CALLBACK (plugin_prefs_changed), action_manager);

    G_OBJECT_CLASS (nemo_action_manager_parent_class)->dispose (object);
}

static void
nemo_action_manager_finalize (GObject *object)
{
    NemoActionManager *action_manager = NEMO_ACTION_MANAGER (object);
    NemoActionManagerPrivate *priv = nemo_action_manager_get_instance_private (action_manager);

    if (priv->actions_by_uuid != NULL) {
        g_hash_table_destroy (priv->actions_by_uuid);
    }

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
                     NULL,
                     idata->user_data);
        return TRUE;
    }

    const gchar *user_label = NULL;
    const gchar *user_icon = NULL;
    const gchar *accelerator = NULL;

    // user-label and user-icon are optional, no error if they're missing or null.
    if (json_reader_read_member (reader, "user-label") && !json_reader_get_null_value (reader)) {
        user_label = json_reader_get_string_value (reader);
    }
    json_reader_end_member (reader);

    if (json_reader_read_member (reader, "user-icon") && !json_reader_get_null_value (reader)) {
        user_icon = json_reader_get_string_value (reader);
    }
    json_reader_end_member (reader);

    if (json_reader_read_member (reader, "accelerator") && !json_reader_get_null_value (reader)) {
        accelerator = json_reader_get_string_value (reader);
        if (accelerator[0] == '\0') {
            accelerator = NULL;
        }
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

        if (accelerator != NULL) {
            action->has_accel = TRUE;
        }

        DEBUG ("Adding action '%s' to UI.", action->uuid);

        idata->func (idata->action_manager,
                     GTK_ACTION (action),
                     GTK_UI_MANAGER_MENUITEM,
                     path,
                     accelerator,
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
                     NULL,
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

    if (priv->actions == NULL) {
        return;
    }

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
                                   const gchar         *accelerator,
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

        gtk_action_group_add_action_with_accel (action_group, action, accelerator ? accelerator : "");
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
                                          gboolean           for_accelerators,
                                          GtkWindow         *window)
{
    GList *l, *actions;

    DEBUG ("Updating action states: for accelerated actions only: %s", for_accelerators ? "yes" : "no");

    actions = gtk_action_group_list_actions (action_group);

    for (l = actions; l != NULL; l = l->next) {
        if (!NEMO_IS_ACTION (l->data)) {
            DEBUG ("Skipping submenu '%s' (visibility managed by GtkUIManager)", gtk_action_get_name (GTK_ACTION (l->data)));
            gtk_action_set_visible (GTK_ACTION (l->data), TRUE);
            continue;
        }


        if (for_accelerators && !NEMO_ACTION (l->data)->has_accel) {
            DEBUG ("Skipping '%s' (no accelerator assigned to this action)", NEMO_ACTION (l->data)->uuid);
            continue;
        }

        nemo_action_update_display_state (NEMO_ACTION (l->data), selection, parent, for_places, window);
    }

    g_list_free (actions);
}

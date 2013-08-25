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

#include "nemo-action.h"
#include <eel/eel-string.h>
#include <glib/gi18n.h>
#include "nemo-file-utilities.h"

#define DEBUG_FLAG NEMO_DEBUG_ACTIONS
#include <libnemo-private/nemo-debug.h>

G_DEFINE_TYPE (NemoAction, nemo_action,
	       GTK_TYPE_ACTION);

static void     nemo_action_init       (NemoAction      *action);

static void     nemo_action_class_init (NemoActionClass *klass);

static void     nemo_action_get_property  (GObject                    *object,
                                           guint                       param_id,
                                           GValue                     *value,
                                           GParamSpec                 *pspec);

static void     nemo_action_set_property  (GObject                    *object,
                                           guint                       param_id,
                                           const GValue               *value,
                                           GParamSpec                 *pspec);

static void     nemo_action_constructed (GObject *object);

static void     nemo_action_finalize (GObject *gobject);

static gpointer parent_class;

#define ACTION_FILE_GROUP "Nemo Action"

#define KEY_ACTIVE "Active"
#define KEY_NAME "Name"
#define KEY_COMMENT "Comment"
#define KEY_EXEC "Exec"
#define KEY_ICON_NAME "Icon-Name"
#define KEY_STOCK_ID "Stock-Id"
#define KEY_SELECTION "Selection"
#define KEY_EXTENSIONS "Extensions"
#define KEY_MIME_TYPES "Mimetypes"
#define KEY_SEPARATOR "Separator"
#define KEY_QUOTE_TYPE "Quote"
#define KEY_DEPENDENCIES "Dependencies"
#define KEY_CONDITIONS "Conditions"

enum 
{
  PROP_0,
  PROP_KEY_FILE_PATH,
  PROP_SELECTION_TYPE,
  PROP_EXTENSIONS,
  PROP_MIMES,
  PROP_EXEC,
  PROP_PARENT_DIR,
  PROP_USE_PARENT_DIR,
  PROP_ORIG_LABEL,
  PROP_ORIG_TT,
  PROP_SEPARATOR,
  PROP_QUOTE_TYPE,
  PROP_CONDITIONS
};

typedef struct {
    NemoAction *action;
    gchar *name;
    guint watch_id;
    gboolean exists;
} DBusCondition;

static void
dbus_condition_free (gpointer data)
{
    DBusCondition *cond = (DBusCondition *) data;
    g_free (cond->name);
    g_bus_unwatch_name (cond->watch_id);
}

static void
nemo_action_init (NemoAction *action)
{
    action->key_file_path = NULL;
    action->selection_type = SELECTION_SINGLE;
    action->extensions = NULL;
    action->mimetypes = NULL;
    action->exec = NULL;
    action->parent_dir = NULL;
    action->use_parent_dir = FALSE;
    action->orig_label = NULL;
    action->orig_tt = NULL;
    action->quote_type = QUOTE_TYPE_NONE;
    action->separator = NULL;
    action->conditions = NULL;
    action->dbus = NULL;
    action->dbus_satisfied = TRUE;
    action->escape_underscores = FALSE;
    action->log_output = g_getenv ("NEMO_ACTION_VERBOSE") != NULL;
}

static void
nemo_action_class_init (NemoActionClass *klass)
{
    GObjectClass         *object_class = G_OBJECT_CLASS(klass);
    parent_class           = g_type_class_peek_parent (klass);
    object_class->finalize = nemo_action_finalize;
    object_class->set_property = nemo_action_set_property;
    object_class->get_property = nemo_action_get_property;
    object_class->constructed = nemo_action_constructed;

    g_object_class_install_property (object_class,
                                     PROP_KEY_FILE_PATH,
                                     g_param_spec_string ("key-file-path",
                                                          "Key File Path",
                                                          "The key file path associated with this action",
                                                          NULL,
                                                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)
                                     );

    g_object_class_install_property (object_class,
                                     PROP_SELECTION_TYPE,
                                     g_param_spec_int ("selection-type",
                                                       "Selection Type",
                                                       "The action selection type",
                                                       0,
                                                       SELECTION_NONE,
                                                       SELECTION_SINGLE,
                                                       G_PARAM_READWRITE)
                                     );

    g_object_class_install_property (object_class,
                                     PROP_EXTENSIONS,
                                     g_param_spec_pointer ("extensions",
                                                           "Extensions",
                                                           "String array of file extensions",
                                                           G_PARAM_READWRITE)
                                     );

    g_object_class_install_property (object_class,
                                     PROP_MIMES,
                                     g_param_spec_pointer ("mimetypes",
                                                           "Mimetypes",
                                                           "String array of file mimetypes",
                                                           G_PARAM_READWRITE)
                                     );

    g_object_class_install_property (object_class,
                                     PROP_EXEC,
                                     g_param_spec_string ("exec",
                                                          "Executable String",
                                                          "The command line to run",
                                                          NULL,
                                                          G_PARAM_READWRITE)
                                     );

    g_object_class_install_property (object_class,
                                     PROP_PARENT_DIR,
                                     g_param_spec_string ("parent-dir",
                                                          "Parent directory",
                                                          "The directory the action file resides in",
                                                          NULL,
                                                          G_PARAM_READWRITE)
                                     );
    g_object_class_install_property (object_class,
                                     PROP_USE_PARENT_DIR,
                                     g_param_spec_boolean ("use-parent-dir",
                                                           "Use Parent Directory",
                                                           "Execute using the full action path",
                                                           FALSE,
                                                           G_PARAM_READWRITE)
                                     );
    g_object_class_install_property (object_class,
                                     PROP_ORIG_LABEL,
                                     g_param_spec_string ("orig-label",
                                                          "Original label string",
                                                          "The starting label - with token",
                                                          NULL,
                                                          G_PARAM_READWRITE)
                                     );
    g_object_class_install_property (object_class,
                                     PROP_ORIG_TT,
                                     g_param_spec_string ("orig-tooltip",
                                                          "Original tooltip string",
                                                          "The starting tooltip - with token",
                                                          NULL,
                                                          G_PARAM_READWRITE)
                                     );

    g_object_class_install_property (object_class,
                                     PROP_SEPARATOR,
                                     g_param_spec_string ("separator",
                                                          "Separator to insert between files in the exec line",
                                                          "Separator to use between files, like comma, space, etc",
                                                          NULL,
                                                          G_PARAM_READWRITE)
                                     );

    g_object_class_install_property (object_class,
                                     PROP_QUOTE_TYPE,
                                     g_param_spec_int ("quote-type",
                                                       "Type of quotes to use to enclose individual file names",
                                                       "Type of quotes to use to enclose individual file names - none, single or double",
                                                       QUOTE_TYPE_SINGLE,
                                                       QUOTE_TYPE_NONE,
                                                       QUOTE_TYPE_SINGLE,
                                                       G_PARAM_READWRITE)
                                     );

    g_object_class_install_property (object_class,
                                     PROP_CONDITIONS,
                                     g_param_spec_pointer ("conditions",
                                                           "Special show conditions",
                                                           "Special conditions, like a bool gsettings key, or 'desktop'",
                                                           G_PARAM_READWRITE)
                                     );
}

static void
recalc_dbus_conditions (NemoAction *action)
{
    GList *l;
    DBusCondition *c;
    gboolean cumul_found = TRUE;

    for (l = action->dbus; l != NULL; l = l->next) {
        c = l->data;
        if (!c->exists) {
            cumul_found = FALSE;
            break;
        }
    }

    action->dbus_satisfied = cumul_found;
}

static void
on_dbus_appeared (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
    DBusCondition *cond = user_data;

    cond->exists = TRUE;
    recalc_dbus_conditions (cond->action);
}

static void
on_dbus_disappeared (GDBusConnection *connection,
                     const gchar     *name,
                     gpointer         user_data)
{
    DBusCondition *cond = user_data;

    cond->exists = FALSE;
    recalc_dbus_conditions (cond->action);
}

static void
setup_dbus_condition (NemoAction *action, const gchar *condition)
{
    gchar **split = g_strsplit (condition, " ", 2);

    if (g_strv_length (split) != 2) {
        g_strfreev (split);
        return;
    }

    if (g_strcmp0 (split[0], "dbus") != 0) {
        g_strfreev (split);
        return;
    }

    DBusCondition *cond = g_new0 (DBusCondition, 1);

    cond->name = g_strdup (split[0]);
    cond->exists = FALSE;
    cond->action = action;
    action->dbus = g_list_append (action->dbus, cond);
    cond->watch_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                       split[1],
                                       0,
                                       on_dbus_appeared,
                                       on_dbus_disappeared,
                                       cond,
                                       NULL);
}

static void
strip_custom_modifier (const gchar *raw, gboolean *custom, gchar **out)
{
    if (g_str_has_prefix (raw, "<") && g_str_has_suffix (raw, ">")) {
        gchar **split = g_strsplit_set (raw, "<>", 3);
        *out = g_strdup (split[1]);
        *custom = TRUE;
        g_free (split);
    } else {
        *out = g_strdup (raw);
        *custom = FALSE;
    }
}

void
nemo_action_constructed (GObject *object)
{
    G_OBJECT_CLASS (parent_class)->constructed (object);

    NemoAction *action = NEMO_ACTION (object);

    GKeyFile *key_file = g_key_file_new();

    g_key_file_load_from_file (key_file, action->key_file_path, G_KEY_FILE_NONE, NULL);

    gchar *orig_label = g_key_file_get_locale_string (key_file,
                                                      ACTION_FILE_GROUP,
                                                      KEY_NAME,
                                                      NULL,
                                                      NULL);

    gchar *orig_tt = g_key_file_get_locale_string (key_file,
                                                   ACTION_FILE_GROUP,
                                                   KEY_COMMENT,
                                                   NULL,
                                                   NULL);

    gchar *icon_name = g_key_file_get_string (key_file,
                                              ACTION_FILE_GROUP,
                                              KEY_ICON_NAME,
                                              NULL);

    gchar *stock_id = g_key_file_get_string (key_file,
                                             ACTION_FILE_GROUP,
                                             KEY_STOCK_ID,
                                             NULL);


    gchar *exec_raw = g_key_file_get_string (key_file,
                                             ACTION_FILE_GROUP,
                                             KEY_EXEC,
                                             NULL);

    gchar *selection_string_raw = g_key_file_get_string (key_file,
                                                         ACTION_FILE_GROUP,
                                                         KEY_SELECTION,
                                                         NULL);

    gchar *selection_string = g_ascii_strdown (selection_string_raw, -1);

    g_free (selection_string_raw);

    gchar *separator = g_key_file_get_string (key_file,
                                              ACTION_FILE_GROUP,
                                              KEY_SEPARATOR,
                                              NULL);

    gchar *quote_type_string = g_key_file_get_string (key_file,
                                                      ACTION_FILE_GROUP,
                                                      KEY_QUOTE_TYPE,
                                                      NULL);

    QuoteType quote_type = QUOTE_TYPE_NONE;

    if (quote_type_string != NULL) {
        if (g_strcmp0 (quote_type_string, "single") == 0)
            quote_type = QUOTE_TYPE_SINGLE;
        else if (g_strcmp0 (quote_type_string, "double") == 0)
            quote_type = QUOTE_TYPE_DOUBLE;
        else if (g_strcmp0 (quote_type_string, "backtick") == 0)
            quote_type = QUOTE_TYPE_BACKTICK;
    }

    SelectionType type;

    if (g_strcmp0 (selection_string, SELECTION_SINGLE_KEY) == 0)
        type = SELECTION_SINGLE;
    else if (g_strcmp0 (selection_string, SELECTION_MULTIPLE_KEY) == 0)
        type = SELECTION_MULTIPLE;
    else if (g_strcmp0 (selection_string, SELECTION_ANY_KEY) == 0)
        type = SELECTION_ANY;
    else if (g_strcmp0 (selection_string, SELECTION_NONE_KEY) == 0)
        type = SELECTION_NONE;
    else if (g_strcmp0 (selection_string, SELECTION_NOT_NONE_KEY) == 0)
        type = SELECTION_NOT_NONE;
    else {
        gint val = (int) g_ascii_strtoll (selection_string, NULL, 10);
        type = val > 0 ? val : SELECTION_SINGLE;
    }

    g_free (selection_string);

    gsize count;

    gchar **ext = g_key_file_get_string_list (key_file,
                                              ACTION_FILE_GROUP,
                                              KEY_EXTENSIONS,
                                              &count,
                                              NULL);

    gsize mime_count;

    gchar **mimes = g_key_file_get_string_list (key_file,
                                                ACTION_FILE_GROUP,
                                                KEY_MIME_TYPES,
                                                &mime_count,
                                                NULL);

    gsize condition_count;

    gchar **conditions = g_key_file_get_string_list (key_file,
                                                     ACTION_FILE_GROUP,
                                                     KEY_CONDITIONS,
                                                     &condition_count,
                                                     NULL);

    if (conditions && condition_count > 0) {
        int j;
        gchar *condition;
        for (j = 0; j < condition_count; j++) {
            condition = conditions[j];
            if (g_str_has_prefix (condition, "dbus")) {
                setup_dbus_condition (action, condition);
            }
        }
    }

    gchar *exec = NULL;
    gboolean use_parent_dir = FALSE;

    strip_custom_modifier (exec_raw, &use_parent_dir, &exec);
    g_free (exec_raw);

    GFile *file = g_file_new_for_path (action->key_file_path);
    GFile *parent = g_file_get_parent (file);

    gchar *parent_dir = g_file_get_path (parent);

    g_object_unref (file);
    g_object_unref (parent);

    g_object_set  (action,
                   "label", orig_label,
                   "tooltip", orig_tt,
                   "icon-name", icon_name,
                   "stock-id", stock_id,
                   "exec", exec,
                   "selection-type", type,
                   "extensions", ext,
                   "mimetypes", mimes,
                   "parent-dir", parent_dir,
                   "use-parent-dir", use_parent_dir,
                   "orig-label", orig_label,
                   "orig-tooltip", orig_tt,
                   "quote-type", quote_type,
                   "separator", separator,
                   "conditions", conditions,
                    NULL);

    g_free (orig_label);
    g_free (orig_tt);
    g_free (icon_name);
    g_free (stock_id);
    g_free (exec);
    g_strfreev (ext);
    g_free (parent_dir);
    g_free (quote_type_string);
    g_free (separator);
    g_strfreev (conditions);
    g_key_file_free (key_file);
}

NemoAction *
nemo_action_new (const gchar *name, 
                 const gchar *path)
{
    GKeyFile *key_file = g_key_file_new();

    g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, NULL);

    if (!g_key_file_has_group (key_file, ACTION_FILE_GROUP))
        return NULL;

    if (g_key_file_has_key (key_file, ACTION_FILE_GROUP, KEY_ACTIVE, NULL)) {
        if (!g_key_file_get_boolean (key_file, ACTION_FILE_GROUP, KEY_ACTIVE, NULL))
            return NULL;
    }

    gchar *orig_label = g_key_file_get_locale_string (key_file,
                                                      ACTION_FILE_GROUP,
                                                      KEY_NAME,
                                                      NULL,
                                                      NULL);

    gchar *exec_raw = g_key_file_get_string (key_file,
                                             ACTION_FILE_GROUP,
                                             KEY_EXEC,
                                             NULL);

    gchar **ext = g_key_file_get_string_list (key_file,
                                              ACTION_FILE_GROUP,
                                              KEY_EXTENSIONS,
                                              NULL,
                                              NULL);

    gchar **mimes = g_key_file_get_string_list (key_file,
                                                ACTION_FILE_GROUP,
                                                KEY_MIME_TYPES,
                                                NULL,
                                                NULL);

    gchar **deps  = g_key_file_get_string_list (key_file,
                                                ACTION_FILE_GROUP,
                                                KEY_DEPENDENCIES,
                                                NULL,
                                                NULL);

    gchar *selection_string = g_key_file_get_string (key_file,
                                                     ACTION_FILE_GROUP,
                                                     KEY_SELECTION,
                                                     NULL);

    gboolean finish = TRUE;

    if (deps != NULL) {
        gint i = 0;
        for (i = 0; i < g_strv_length (deps); i++) {
            gchar *p = g_find_program_in_path (deps[i]);
            if (p == NULL) {
                finish = FALSE;
                DEBUG ("Missing action dependency: %s", deps[i]);
                g_free (p);
                break;
            }
            g_free (p);
        }
    }

    if (orig_label == NULL || exec_raw == NULL || (ext == NULL && mimes == NULL) || selection_string == NULL) {
        g_printerr ("An action definition requires, at minimum, "
                    "a Label field, an Exec field, a Selection field, and an either an Extensions or Mimetypes field.\n"
                    "Check the %s file for missing fields.\n", path);
        finish = FALSE;
    }

    g_free (orig_label);
    g_free (exec_raw);
    g_free (selection_string);
    g_strfreev (ext);
    g_strfreev (mimes);
    g_strfreev (deps);
    g_key_file_free (key_file);

    return finish ? g_object_new (NEMO_TYPE_ACTION,
                                  "name", name,
                                  "key-file-path", path,
                                  NULL): NULL;
}

static void
nemo_action_finalize (GObject *object)
{
    NemoAction *action = NEMO_ACTION (object);

    g_free (action->key_file_path);
    g_strfreev (action->extensions);
    g_strfreev (action->mimetypes);
    g_strfreev (action->conditions);
    g_free (action->exec);
    g_free (action->parent_dir);
    g_free (action->orig_label);
    g_free (action->orig_tt);

    if (action->dbus) {
        g_list_free_full (action->dbus, (GDestroyNotify) dbus_condition_free);
    }

    G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
nemo_action_set_property (GObject         *object,
                          guint            prop_id,
                          const GValue    *value,
                          GParamSpec      *pspec)
{
  NemoAction *action;
  
  action = NEMO_ACTION (object);

  switch (prop_id)
    {
    case PROP_KEY_FILE_PATH:
      nemo_action_set_key_file_path (action, g_value_get_string (value));
      break;
    case PROP_SELECTION_TYPE:
      action->selection_type = g_value_get_int (value);
      break;
    case PROP_EXTENSIONS:
      nemo_action_set_extensions (action, g_value_get_pointer (value));
      break;
    case PROP_MIMES:
      nemo_action_set_mimetypes (action, g_value_get_pointer (value));
      break;
    case PROP_EXEC:
      nemo_action_set_exec (action, g_value_get_string (value));
      break;
    case PROP_PARENT_DIR:
      nemo_action_set_parent_dir (action, g_value_get_string (value));
      break;
    case PROP_USE_PARENT_DIR:
      action->use_parent_dir = g_value_get_boolean (value);
      break;
    case PROP_ORIG_LABEL:
      nemo_action_set_orig_label (action, g_value_get_string (value));
      break;
    case PROP_ORIG_TT:
      nemo_action_set_orig_tt (action, g_value_get_string (value));
      break;
    case PROP_QUOTE_TYPE:
      action->quote_type = g_value_get_int (value);
      break;
    case PROP_SEPARATOR:
      nemo_action_set_separator (action, g_value_get_string (value));
      break;
    case PROP_CONDITIONS:
      nemo_action_set_conditions (action, g_value_get_pointer (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
nemo_action_get_property (GObject    *object,
             guint       prop_id,
             GValue     *value,
             GParamSpec *pspec)
{
  NemoAction *action;

  action = NEMO_ACTION (object);

  switch (prop_id)
    {
    case PROP_KEY_FILE_PATH:
      g_value_set_string (value, action->key_file_path);
      break;
    case PROP_SELECTION_TYPE:
      g_value_set_int (value, action->selection_type);
      break;
    case PROP_EXTENSIONS:
      g_value_set_pointer (value, action->extensions);
      break;
    case PROP_MIMES:
      g_value_set_pointer (value, action->mimetypes);
      break;
    case PROP_EXEC:
      g_value_set_string (value, action->exec);
      break;
    case PROP_PARENT_DIR:
      g_value_set_string (value, action->parent_dir);
      break;
    case PROP_USE_PARENT_DIR:
      g_value_set_boolean (value, action->use_parent_dir);
      break;
    case PROP_ORIG_LABEL:
      g_value_set_string (value, action->orig_label);
      break;
    case PROP_ORIG_TT:
      g_value_set_string (value, action->orig_tt);
      break;
    case PROP_QUOTE_TYPE:
      g_value_set_int (value, action->quote_type);
      break;
    case PROP_SEPARATOR:
      g_value_set_string (value, action->separator);
      break;
    case PROP_CONDITIONS:
      g_value_set_pointer (value, action->conditions);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gchar *
find_token_type (const gchar *str, TokenType *token_type)
{
    gchar *ptr = NULL;
    *token_type = TOKEN_NONE;

    ptr = g_strstr_len (str, -1, TOKEN_EXEC_FILE_LIST);
    if (ptr != NULL) {
        *token_type = TOKEN_PATH_LIST;
        return ptr;
    }
    ptr = g_strstr_len (str, -1, TOKEN_EXEC_URI_LIST);
    if (ptr != NULL) {
        *token_type = TOKEN_URI_LIST;
        return ptr;
    }
    ptr = g_strstr_len (str, -1, TOKEN_EXEC_PARENT);
    if (ptr != NULL) {
        *token_type = TOKEN_PARENT_PATH;
        return ptr;
    }
    ptr = g_strstr_len (str, -1, TOKEN_EXEC_FILE_NAME);
    if (ptr != NULL) {
        *token_type = TOKEN_FILE_DISPLAY_NAME;
        return ptr;
    }
    ptr = g_strstr_len (str, -1, TOKEN_EXEC_PARENT_NAME);
    if (ptr != NULL) {
        *token_type = TOKEN_PARENT_DISPLAY_NAME;
        return ptr;
    }
    ptr = g_strstr_len (str, -1, TOKEN_LABEL_FILE_NAME);
    if (ptr != NULL) {
        *token_type = TOKEN_FILE_DISPLAY_NAME;
        return ptr;
    }
    ptr = g_strstr_len (str, -1, TOKEN_EXEC_DEVICE);
    if (ptr != NULL) {
        *token_type = TOKEN_DEVICE;
        return ptr;
    }
    return NULL;
}

static GString *
_score_append (NemoAction *action, GString *str, const gchar *c)
{
    if (action->escape_underscores) {
        gchar *escaped = eel_str_double_underscores (c);
        str = g_string_append (str, escaped);
        g_free (escaped);
        return str;
    } else {
        return g_string_append (str, c);
    }
}

static GString *
insert_separator (NemoAction *action, GString *str)
{
    if (action->separator == NULL)
        str = g_string_append (str, " ");
    else
        str = _score_append (action, str, action->separator);

    return str;
}

static GString *
insert_quote (NemoAction *action, GString *str)
{
    switch (action->quote_type) {
        case QUOTE_TYPE_SINGLE:
            str = g_string_append (str, "'");
            break;
        case QUOTE_TYPE_DOUBLE:
            str = g_string_append (str, "\"");
            break;
        case QUOTE_TYPE_BACKTICK:
            str = g_string_append (str, "`");
            break;
        case QUOTE_TYPE_NONE:
            break;
    }

    return str;
}

static gchar *
get_device_path (NemoFile *file)
{
    GMount *mount = nemo_file_get_mount (file);
    GVolume *volume = g_mount_get_volume (mount);
    gchar *id = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);

    g_object_unref (mount);
    g_object_unref (volume);

    return id;
}

static gchar *
get_insertion_string (NemoAction *action, TokenType token_type, GList *selection, NemoFile *parent)
{
    GList *l;

    GString *str = g_string_new("");
    gboolean first = TRUE;

    switch (token_type) {
        case TOKEN_PATH_LIST:
            if (g_list_length (selection) > 0) {
                for (l = selection; l != NULL; l = l->next) {
                    if (!first)
                        str = insert_separator (action, str);
                    str = insert_quote (action, str);
                    gchar *path = nemo_file_get_path (NEMO_FILE (l->data));
                    if (path)
                        str = _score_append (action, str, path);
                    g_free (path);
                    str = insert_quote (action, str);
                    first = FALSE;
                }
            } else {
                goto default_parent_path;
            }
            break;
        case TOKEN_URI_LIST:
            if (g_list_length (selection) > 0) {
                for (l = selection; l != NULL; l = l->next) {
                    if (!first)
                        str = insert_separator (action, str);
                    str = insert_quote (action, str);
                    gchar *uri = nemo_file_get_uri (NEMO_FILE (l->data));
                    if (uri)
                        str = _score_append (action, str, uri);
                    g_free (uri);
                    str = insert_quote (action, str);
                    first = FALSE;
                }
            } else {
                goto default_parent_path;
            }
            break;
        case TOKEN_PARENT_PATH:
            ;
default_parent_path:
            ;
            gchar *path = nemo_file_get_path (parent);
            if (path == NULL) {
                gchar *name = nemo_file_get_display_name (parent);
                if (g_strcmp0 (name, "x-nemo-desktop") == 0)
                    path = nemo_get_desktop_directory ();
                else
                    path = g_strdup_printf ("");
                g_free (name);
            }
            str = insert_quote (action, str);
            str = _score_append (action, str, path);
            str = insert_quote (action, str);
            g_free (path);
            break;
        case TOKEN_FILE_DISPLAY_NAME:
            if (g_list_length (selection) > 0) {
                gchar *file_display_name = nemo_file_get_display_name (NEMO_FILE (selection->data));
                str = _score_append (action, str, file_display_name);
                g_free (file_display_name);
            } else {
                goto default_parent_display_name;
            }
            break;
        case TOKEN_PARENT_DISPLAY_NAME:
            ;
default_parent_display_name:
            ;
            gchar *parent_display_name;
            gchar *real_display_name = nemo_file_get_display_name (parent);
            if (g_strcmp0 (real_display_name, "x-nemo-desktop") == 0)
                parent_display_name = g_strdup_printf (_("Desktop"));
            else
                parent_display_name = nemo_file_get_display_name (parent);
            g_free (real_display_name);
            str = insert_quote (action, str);
            str = _score_append (action, str, parent_display_name);
            str = insert_quote (action, str);
            g_free (parent_display_name);
            break;
        case TOKEN_DEVICE:
            if (g_list_length (selection) > 0) {
                for (l = selection; l != NULL; l = l->next) {
                    if (!first)
                        str = insert_separator (action, str);
                    str = insert_quote (action, str);
                    gchar *dev = get_device_path (NEMO_FILE (l->data));
                    if (dev)
                        str = _score_append (action, str, dev);
                    g_free (dev);
                    str = insert_quote (action, str);
                    first = FALSE;
                }
            } else {
                goto default_parent_path;
            }
            break;
    }

    gchar *ret = str->str;

    g_string_free (str, FALSE);

    return ret;
}

static GString *
expand_action_string (NemoAction *action, GList *selection, NemoFile *parent, GString *str)
{
    gchar *ptr;
    TokenType token_type;

    ptr = find_token_type (str->str, &token_type);

    while (ptr != NULL) {
        gint shift = ptr - str->str;

        gchar *insertion = get_insertion_string (action, token_type, selection, parent);
        str = g_string_erase (str, shift, 2);
        str = g_string_insert (str, shift, insertion);

        token_type = TOKEN_NONE;
        g_free  (insertion);
        ptr = find_token_type (str->str, &token_type);
    }

    return str;
}

void
nemo_action_activate (NemoAction *action, GList *selection, NemoFile *parent)
{
    GString *exec = g_string_new (action->exec);

    action->escape_underscores = FALSE;

    exec = expand_action_string (action, selection, parent, exec);

    if (action->use_parent_dir) {
        exec = g_string_prepend (exec, G_DIR_SEPARATOR_S);
        exec = g_string_prepend (exec, action->parent_dir);
    }

    DEBUG ("Action Spawning: %s", exec->str);
    if (action->log_output)
        g_printerr ("Action Spawning: %s\n", exec->str);

    g_spawn_command_line_async (exec->str, NULL);

    g_string_free (exec, TRUE);
}

SelectionType
nemo_action_get_selection_type (NemoAction *action)
{
    return action->selection_type;
}

gchar **
nemo_action_get_extension_list (NemoAction *action)
{
    return action->extensions;
}

gchar **
nemo_action_get_mimetypes_list (NemoAction *action)
{
    return action->mimetypes;
}

void
nemo_action_set_key_file_path (NemoAction *action, const gchar *path)
{
    gchar *tmp;
    tmp = action->key_file_path;
    action->key_file_path = g_strdup (path);
    g_free (tmp);
}

void
nemo_action_set_exec (NemoAction *action, const gchar *exec)
{
    gchar *tmp;

    tmp = action->exec;
    action->exec = g_strdup (exec);
    g_free (tmp);
}

void
nemo_action_set_parent_dir (NemoAction *action, const gchar *parent_dir)
{
    gchar *tmp;

    tmp = action->parent_dir;
    action->parent_dir = g_strdup (parent_dir);
    g_free (tmp);
}

void
nemo_action_set_separator (NemoAction *action, const gchar *separator)
{
    gchar *tmp;

    tmp = action->separator;
    action->separator = g_strdup (separator);
    g_free (tmp);
}

void
nemo_action_set_conditions (NemoAction *action, gchar **conditions)
{
    gchar **tmp;

    tmp = action->conditions;
    action->conditions = g_strdupv (conditions);
    g_strfreev (tmp);
}

void
nemo_action_set_orig_label (NemoAction *action, const gchar *orig_label)
{
    gchar *tmp;

    tmp = action->orig_label;
    action->orig_label = g_strdup (orig_label);
    g_free (tmp);
}

const gchar *
nemo_action_get_orig_label (NemoAction *action)
{
    return action->orig_label;
}

void
nemo_action_set_orig_tt (NemoAction *action, const gchar *orig_tt)
{
    gchar *tmp;

    tmp = action->orig_tt;
    action->orig_tt = g_strdup (orig_tt);
    g_free (tmp);
}

const gchar *
nemo_action_get_orig_tt (NemoAction *action)
{
    return action->orig_tt;
}

gchar **
nemo_action_get_conditions (NemoAction *action)
{
    return action->conditions;
}

gchar *
nemo_action_get_label (NemoAction *action, GList *selection, NemoFile *parent)
{
    const gchar *orig_label = nemo_action_get_orig_label (action);

    if (orig_label == NULL)
        return;

    action->escape_underscores = TRUE;

    GString *str = g_string_new (orig_label);

    str = expand_action_string (action, selection, parent, str);

    DEBUG ("Action Label: %s", str->str);
    if (action->log_output)
        g_printerr ("Action Label: %s\n", str->str);

    gchar *ret = str->str;
    g_string_free (str, FALSE);
    return ret;
}

gchar *
nemo_action_get_tt (NemoAction *action, GList *selection, NemoFile *parent)
{
    const gchar *orig_tt = nemo_action_get_orig_tt (action);

    if (orig_tt == NULL)
        return;

    action->escape_underscores = FALSE;

    GString *str = g_string_new (orig_tt);

    str = expand_action_string (action, selection, parent, str);

    DEBUG ("Action Tooltip: %s", str->str);
    if (action->log_output)
        g_printerr ("Action Tooltip: %s\n", str->str);

    gchar *ret = str->str;
    g_string_free (str, FALSE);
    return ret;
}

void
nemo_action_set_extensions (NemoAction *action, gchar **extensions)
{
    gchar **tmp;

    tmp = action->extensions;
    action->extensions = g_strdupv (extensions);
    g_strfreev (tmp);
}

void
nemo_action_set_mimetypes (NemoAction *action, gchar **mimetypes)
{
    gchar **tmp;

    tmp = action->mimetypes;
    action->mimetypes = g_strdupv (mimetypes);
    g_strfreev (tmp);
}

gboolean
nemo_action_get_dbus_satisfied (NemoAction *action)
{
    return action->dbus_satisfied;
}


static gboolean
check_gsettings_condition (NemoAction *action, const gchar *condition)
{

    gchar **split = g_strsplit (condition, " ", 3);

    if (g_strv_length (split) != 3) {
        g_strfreev (split);
        return FALSE;
    }

    if (g_strcmp0 (split[0], "gsettings") != 0) {
        g_strfreev (split);
        return FALSE;
    }

    GSettingsSchemaSource *schema_source;

    schema_source = g_settings_schema_source_get_default();

    if (g_settings_schema_source_lookup (schema_source, split[1], TRUE)) {
        GSettings *s = g_settings_new (split[1]);
        gchar **keys = g_settings_list_keys (s);
        gboolean ret = FALSE;
        gint i;
        for (i = 0; i < g_strv_length (keys); i++) {
            if (g_strcmp0 (keys[i], split[2]) == 0) {
                GVariant *var = g_settings_get_value (s, split[2]);
                const GVariantType *type = g_variant_get_type (var);
                if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
                    ret = g_variant_get_boolean (var);
                g_variant_unref (var);
            }
        }
        g_strfreev (keys);
        g_object_unref (s);
        g_strfreev (split);
        return ret;
    } else {
        g_strfreev (split);
        return FALSE;
    }
}

static gboolean
get_is_dir_hack (NemoFile *file)
{
    gboolean ret = FALSE;

    GFile *f = nemo_file_get_location (file);
    GFileType type = g_file_query_file_type (f, 0, NULL);
    ret = type == G_FILE_TYPE_DIRECTORY;
    return ret;
}

gboolean
nemo_action_get_visibility (NemoAction *action, GList *selection, NemoFile *parent)
{

    gboolean selection_type_show = FALSE;
    gboolean extension_type_show = TRUE;
    gboolean condition_type_show = TRUE;

    recalc_dbus_conditions (action);

    if (!nemo_action_get_dbus_satisfied (action))
        goto out;

    gchar **conditions = nemo_action_get_conditions (action);

    guint condition_count = conditions != NULL ? g_strv_length (conditions) : 0;

    if (condition_count > 0) {
        int j;
        gchar *condition;
        for (j = 0; j < condition_count; j++) {
            condition = conditions[j];
            if (g_strcmp0 (condition, "desktop") == 0) {
                gchar *name = nemo_file_get_display_name (parent);
                if (g_strcmp0 (name, "x-nemo-desktop") != 0)
                    condition_type_show = FALSE;
                g_free (name);
                break;
            } else if (g_strcmp0 (condition, "removable") == 0) {
                gboolean is_removable = FALSE;
                if (g_list_length (selection) > 0) {
                    GMount *mount = nemo_file_get_mount (selection->data);
                    if (mount) {
                        GDrive *drive = g_mount_get_drive (mount);
                        if (drive) {
                            if (g_drive_is_media_removable (drive))
                                is_removable = TRUE;
                            g_object_unref (drive);
                        }
                    }
                }
                condition_type_show = is_removable;
            } else if (g_str_has_prefix (condition, "gsettings")) {
                condition_type_show = check_gsettings_condition (action, condition);
                if (!condition_type_show)
                    break;
            }
            if (!condition_type_show)
                break;
        }
    }

    if (!condition_type_show)
        goto out;

    SelectionType selection_type = nemo_action_get_selection_type (action);
    GList *iter;

    guint selected_count = g_list_length (selection);

    switch (selection_type) {
        case SELECTION_SINGLE:
            selection_type_show = selected_count == 1;
            break;
        case SELECTION_MULTIPLE:
            selection_type_show = selected_count > 1;
            break;
        case SELECTION_NOT_NONE:
            selection_type_show = selected_count > 0;
            break;
        case SELECTION_NONE:
            selection_type_show = selected_count == 0;
            break;
        case SELECTION_ANY:
            selection_type_show = TRUE;
            break;
        default:
            selection_type_show = selected_count == selection_type;
            break;
    }

    gchar **extensions = nemo_action_get_extension_list (action);
    gchar **mimetypes = nemo_action_get_mimetypes_list (action);

    guint ext_count = extensions != NULL ? g_strv_length (extensions) : 0;
    guint mime_count = mimetypes != NULL ? g_strv_length (mimetypes) : 0;

    if (ext_count == 1 && g_strcmp0 (extensions[0], "any") == 0)
        goto out;

    gboolean found_match = TRUE;

    for (iter = selection; iter != NULL && found_match; iter = iter->next) {
        found_match = FALSE;
        gchar *raw_fn = nemo_file_get_name (NEMO_FILE (iter->data));
        gchar *filename = g_ascii_strdown (raw_fn, -1);
        g_free (raw_fn);
        int i;
        gboolean is_dir = get_is_dir_hack (iter->data);
        if (ext_count > 0) {
            for (i = 0; i < ext_count; i++) {
                if (g_strcmp0 (extensions[i], "dir") == 0) {
                    if (is_dir) {
                        found_match = TRUE;
                        break;
                    }
                } else if (g_strcmp0 (extensions[i], "none") == 0) {
                    if (g_strrstr (filename, ".") == NULL) {
                        found_match = TRUE;
                        break;
                    }
                } else if (g_strcmp0 (extensions[i], "nodirs") == 0) {
                    if (!is_dir) {
                        found_match = TRUE;
                        break;
                    }
                } else {
                    if (g_str_has_suffix (filename, g_ascii_strdown (extensions[i], -1))) {
                        found_match = TRUE;
                        break;
                    }
                }
            }
            g_free (filename);
        }

        if (mime_count > 0) {
            for (i = 0; i < mime_count; i++) {
                if (nemo_file_is_mime_type (NEMO_FILE (iter->data), mimetypes[i])) {
                    found_match = TRUE;
                    break;
                }
            }
        }
    }

    extension_type_show = found_match;

out:

    return selection_type_show && extension_type_show && condition_type_show;
}
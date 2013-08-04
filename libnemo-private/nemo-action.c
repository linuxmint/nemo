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

enum 
{
  PROP_0,
  PROP_KEY_FILE_PATH,
  PROP_SELECTION_TYPE,
  PROP_EXTENSIONS,
  PROP_MIMES,
  PROP_EXT_LENGTH,
  PROP_MIME_LENGTH,
  PROP_EXEC,
  PROP_PARENT_DIR,
  PROP_USE_PARENT_DIR,
  PROP_ORIG_LABEL,
  PROP_ORIG_TT,
  PROP_SEPARATOR,
  PROP_QUOTE_TYPE
};

static void
nemo_action_init (NemoAction *action)
{
    action->key_file_path = NULL;
    action->selection_type = SELECTION_SINGLE;
    action->extensions = NULL;
    action->mimetypes = NULL;
    action->ext_length = 0;
    action->mime_length = 0;
    action->exec = NULL;
    action->parent_dir = NULL;
    action->use_parent_dir = FALSE;
    action->orig_label = NULL;
    action->orig_tt = NULL;
    action->quote_type = QUOTE_TYPE_NONE;
    action->separator = NULL;
    action->log_output = g_getenv ("NEMO_ACTION_VERBOSE") != NULL;
}

static void
nemo_action_class_init (NemoActionClass *klass)
{
	GtkActionClass *action_class = GTK_ACTION_CLASS (klass);
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
                                                       SELECTION_SINGLE,
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
                                     PROP_EXT_LENGTH,
                                     g_param_spec_int ("ext-length",
                                                       "Extensions Length",
                                                       "Number of extensions",
                                                       0,
                                                       999,
                                                       0,
                                                       G_PARAM_READWRITE)
                                     );

    g_object_class_install_property (object_class,
                                     PROP_MIME_LENGTH,
                                     g_param_spec_int ("mime-length",
                                                       "Mimetypes Length",
                                                       "Number of mimetypes",
                                                       0,
                                                       999,
                                                       0,
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

    gchar *selection_string = g_key_file_get_string (key_file,
                                                     ACTION_FILE_GROUP,
                                                     KEY_SELECTION,
                                                     NULL);

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
                   "ext-length", count,
                   "extensions", ext,
                   "mime-length", mime_count,
                   "mimetypes", mimes,
                   "parent-dir", parent_dir,
                   "use-parent-dir", use_parent_dir,
                   "orig-label", orig_label,
                   "orig-tooltip", orig_tt,
                   "quote-type", quote_type,
                   "separator", separator,
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
            gchar *path = g_find_program_in_path (deps[i]);
            if (path == NULL) {
                finish = FALSE;
                g_printerr ("Missing action dependency: %s", deps[i]);
                g_free (path);
                break;
            }
            g_free (path);
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
    g_free (action->exec);
    g_free (action->parent_dir);
    g_free (action->orig_label);
    g_free (action->orig_tt);

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
    case PROP_EXT_LENGTH:
      action->ext_length = g_value_get_int (value);
      break;
    case PROP_MIME_LENGTH:
      action->mime_length = g_value_get_int (value);
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
    case PROP_EXT_LENGTH:
      g_value_set_int (value, action->ext_length);
      break;
    case PROP_MIME_LENGTH:
      g_value_set_int (value, action->mime_length);
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
    return NULL;
}

static GString *
insert_separator (NemoAction *action, GString *str)
{
    if (action->separator == NULL)
        str = g_string_append (str, " ");
    else
        str = g_string_append (str, action->separator);

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
get_insertion_string (NemoAction *action, TokenType token_type, GList *selection, NemoFile *parent)
{
    GList *l;

    GString *str = g_string_new("");
    gboolean first = TRUE;

    switch (token_type) {
        case TOKEN_PATH_LIST:
            for (l = selection; l != NULL; l = l->next) {
                if (!first)
                    str = insert_separator (action, str);
                str = insert_quote (action, str);
                gchar *path = nemo_file_get_path (NEMO_FILE (l->data));
                str = g_string_append (str, path);
                g_free (path);
                str = insert_quote (action, str);
                first = FALSE;
            }
            break;
        case TOKEN_URI_LIST:
            for (l = selection; l != NULL; l = l->next) {
                if (!first)
                    str = insert_separator (action, str);
                str = insert_quote (action, str);
                gchar *uri = nemo_file_get_uri (NEMO_FILE (l->data));
                str = g_string_append (str, uri);
                g_free (uri);
                str = insert_quote (action, str);
                first = FALSE;
            }
            break;
        case TOKEN_PARENT_PATH:
            ;
            gchar *path = nemo_file_get_path (parent);
            str = insert_quote (action, str);
            str = g_string_append (str, path);
            str = insert_quote (action, str);
            g_free (path);
            break;
        case TOKEN_FILE_DISPLAY_NAME:
            if (selection != NULL) {
                gchar *file_display_name = nemo_file_get_display_name (NEMO_FILE (selection->data));
                str = g_string_append (str, file_display_name);
                g_free (file_display_name);
            }
            break;
        case TOKEN_PARENT_DISPLAY_NAME:
            ;
            gchar *parent_display_name = nemo_file_get_display_name (parent);
            str = insert_quote (action, str);
            str = g_string_append (str, parent_display_name);
            str = insert_quote (action, str);
            g_free (parent_display_name);
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

    exec = expand_action_string (action, selection, parent, exec);

    if (action->use_parent_dir) {
        exec = g_string_prepend (exec, G_DIR_SEPARATOR_S);
        exec = g_string_prepend (exec, action->parent_dir);
    }

    DEBUG ("Action Spawning: %s", exec->str);
    if (action->log_output)
        g_printerr ("Action Spawning: %s\n", exec->str);

    g_spawn_command_line_async (exec->str, NULL);

    nemo_file_list_free (selection);
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

guint
nemo_action_get_extension_count (NemoAction *action)
{
    return action->ext_length;
}

guint
nemo_action_get_mimetypes_count (NemoAction *action)
{
    return action->mime_length;
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

static gboolean
test_string_for_label_token (const gchar *string)
{
    return g_strstr_len (string, -1, TOKEN_LABEL_FILE_NAME) != NULL;
}

void
nemo_action_set_label (NemoAction *action, GList *selection, NemoFile *parent)
{
    const gchar *orig_label = nemo_action_get_orig_label (action);

    if (orig_label == NULL)
        return;

    GString *str = g_string_new (orig_label);

    str = expand_action_string (action, selection, parent, str);

    DEBUG ("Action Label: %s", str->str);
    if (action->log_output)
        g_printerr ("Action Label: %s\n", str->str);

    gtk_action_set_label (GTK_ACTION (action), str->str);
}

void
nemo_action_set_tt (NemoAction *action, GList *selection, NemoFile *parent)
{
    const gchar *orig_tt = nemo_action_get_orig_tt (action);

    if (orig_tt == NULL)
        return;

    GString *str = g_string_new (orig_tt);

    str = expand_action_string (action, selection, parent, str);

    DEBUG ("Action Tooltip: %s", str->str);
    if (action->log_output)
        g_printerr ("Action Tooltip: %s\n", str->str);


    gtk_action_set_tooltip (GTK_ACTION (action), str->str);
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

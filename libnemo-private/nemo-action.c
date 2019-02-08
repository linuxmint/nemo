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
#include <gdk/gdk.h>
#include "nemo-file-utilities.h"
#include "nemo-program-choosing.h"

#define DEBUG_FLAG NEMO_DEBUG_ACTIONS
#include <libnemo-private/nemo-debug.h>

#if (!GLIB_CHECK_VERSION(2,50,0))
#define g_drive_is_removable g_drive_is_media_removable
#endif

G_DEFINE_TYPE (NemoAction, nemo_action,
	       GTK_TYPE_ACTION);

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

static SelectionType nemo_action_get_selection_type   (NemoAction *action);
static void          nemo_action_set_extensions       (NemoAction *action, gchar **extensions);
static gchar       **nemo_action_get_extension_list   (NemoAction *action);
static void          nemo_action_set_mimetypes        (NemoAction *action, gchar **mimetypes);
static gchar       **nemo_action_get_mimetypes_list   (NemoAction *action);
static void          nemo_action_set_key_file_path    (NemoAction *action, const gchar *path);
static void          nemo_action_set_exec             (NemoAction *action, const gchar *exec);
static void          nemo_action_set_parent_dir       (NemoAction *action, const gchar *parent_dir);
static void          nemo_action_set_separator        (NemoAction *action, const gchar *separator);
static void          nemo_action_set_conditions       (NemoAction *action, gchar **conditions);
static gchar       **nemo_action_get_conditions       (NemoAction *action);
static void          nemo_action_set_orig_label       (NemoAction *action, const gchar *orig_label);
static void          nemo_action_set_orig_tt          (NemoAction *action, const gchar *orig_tt);

static gchar   *find_token_type (const gchar *str, TokenType *token_type);

static gpointer parent_class;

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
  PROP_ESCAPE_SPACE,
  PROP_RUN_IN_TERMINAL,
  PROP_CONDITIONS
};

enum {
    CONDITION_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

typedef struct {
    NemoAction *action;
    gchar *name;
    guint watch_id;
    gboolean exists;
} DBusCondition;

typedef struct {
    NemoAction *action;
    GSettings *settings;
    gchar *condition_string;
    gchar *key;
    guint handler_id;
} GSettingsCondition;

static void
dbus_condition_free (gpointer data)
{
    DBusCondition *cond = (DBusCondition *) data;
    g_free (cond->name);
    g_bus_unwatch_name (cond->watch_id);

    g_free (cond);
}

static void
gsettings_condition_free (gpointer data)
{
    GSettingsCondition *cond = (GSettingsCondition *) data;

    g_signal_handler_disconnect (cond->settings, cond->handler_id);

    g_object_unref (cond->settings);
    g_free (cond->key);
    g_free (cond->condition_string);

    g_free (cond);
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
    action->dbus_recalc_timeout_id = 0;
    action->gsettings = NULL;
    action->gsettings_satisfied = TRUE;
    action->gsettings_recalc_timeout_id = 0;
    action->escape_underscores = FALSE;
    action->escape_space = FALSE;
    action->show_in_blank_desktop = FALSE;
    action->run_in_terminal = FALSE;

    action->constructing = TRUE;
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
    g_object_class_install_property (object_class,
                                     PROP_ESCAPE_SPACE,
                                     g_param_spec_boolean ("escape-space",
                                                           "Escape spaces in file paths",
                                                           "Escape spaces in file paths",
                                                           FALSE,
                                                           G_PARAM_READWRITE)
                                     );
    g_object_class_install_property (object_class,
                                     PROP_RUN_IN_TERMINAL,
                                     g_param_spec_boolean ("run-in-terminal",
                                                           "Run command in a terminal",
                                                           "Run command in a terminal",
                                                           FALSE,
                                                           G_PARAM_READWRITE)
                                 );

    signals[CONDITION_CHANGED] = g_signal_new ("condition-changed",
                                               G_TYPE_FROM_CLASS (object_class),
                                               G_SIGNAL_RUN_LAST,
                                               0, NULL, NULL,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE, 0);
}

static gboolean
recalc_dbus_conditions (NemoAction *action)
{
    GList *l;
    DBusCondition *c;
    gboolean pass, old_satisfied;

    DEBUG ("Recalculating dbus conditions for %s", action->key_file_path);

    pass = TRUE;

    for (l = action->dbus; l != NULL; l = l->next) {
        c = (DBusCondition *) l->data;

        DEBUG ("Checking dbus name for an owner: '%s' - evaluated to %s",
               c->name,
               c->exists ? "TRUE" : "FALSE");

        if (!c->exists) {
            pass = FALSE;
            break;
        }
    }

    old_satisfied = action->dbus_satisfied;
    action->dbus_satisfied = pass;

    DEBUG ("DBus satisfied: %s",
           pass ? "TRUE" : "FALSE");

    if (pass != old_satisfied) {
        g_signal_emit (action, signals[CONDITION_CHANGED], 0);
    }

    action->dbus_recalc_timeout_id = 0;
    return FALSE;
}

static void
queue_recalc_dbus_conditions (NemoAction *action)
{
    if (action->constructing) {
        return;
    }

    if (action->dbus_recalc_timeout_id != 0) {
        g_source_remove (action->dbus_recalc_timeout_id);
        action->dbus_recalc_timeout_id = 0;
    }
    action->dbus_recalc_timeout_id = g_idle_add ((GSourceFunc) recalc_dbus_conditions,
                                                 action);
}

static void
on_dbus_appeared (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
    DBusCondition *cond = user_data;

    cond->exists = TRUE;
    queue_recalc_dbus_conditions (cond->action);
}

static void
on_dbus_disappeared (GDBusConnection *connection,
                     const gchar     *name,
                     gpointer         user_data)
{
    DBusCondition *cond = user_data;

    cond->exists = FALSE;
    queue_recalc_dbus_conditions (cond->action);
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

    cond->name = g_strdup (split[1]);
    cond->exists = FALSE;
    cond->action = action;
    action->dbus = g_list_append (action->dbus, cond);
    cond->watch_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                       cond->name,
                                       0,
                                       on_dbus_appeared,
                                       on_dbus_disappeared,
                                       cond,
                                       NULL);

    g_strfreev (split);
}

#define EQUALS "eq"
#define NOT_EQUALS "ne"
#define LESS_THAN "lt"
#define GREATER_THAN "gt"

enum
{
    GSETTINGS_SCHEMA_INDEX = 1,
    GSETTINGS_KEY_INDEX = 2,
    GSETTINGS_TYPE_INDEX = 3,
    GSETTINGS_OP_INDEX = 4,
    GSETTINGS_VAL_INDEX = 5,
};

static gboolean
operator_is_valid (const gchar *op_string)
{
    return (g_strcmp0 (op_string, EQUALS) == 0 ||
            g_strcmp0 (op_string, NOT_EQUALS) == 0 ||
            g_strcmp0 (op_string, LESS_THAN) == 0 ||
            g_strcmp0 (op_string, GREATER_THAN) == 0);
}

static gboolean
try_vector (const gchar *op, gint vector)
{
    if (g_strcmp0 (op, EQUALS) == 0) {
        return (vector == 0);
    } else if (g_strcmp0 (op, NOT_EQUALS) == 0) {
        return (vector != 0);
    } else if (g_strcmp0 (op, LESS_THAN) == 0) {
        return (vector < 0);
    } else if (g_strcmp0 (op, GREATER_THAN) == 0) {
        return (vector > 0);
    }
    return FALSE;
}

static gboolean
recalc_gsettings_conditions (NemoAction *action)
{
    GList *l;
    gboolean pass, old_satisfied;

    DEBUG ("Recalculating gsettings conditions for %s", action->key_file_path);

    pass = TRUE;

    for (l = action->gsettings; l != NULL; l = l->next) {
        GSettingsCondition *cond;
        const GVariantType *target_type, *setting_type;
        gchar **split;
        gint len;
        gboolean iter_pass = FALSE;

        cond = (GSettingsCondition *) l->data;

        split = g_strsplit (cond->condition_string, " ", 6);
        len = g_strv_length (split);

        if (len == 3) {
            GVariant *setting_var;

            target_type = G_VARIANT_TYPE_BOOLEAN;

            setting_var = g_settings_get_value (cond->settings, cond->key);
            setting_type = g_variant_get_type (setting_var);

            if (g_variant_type_equal (setting_type, target_type)) {
                iter_pass = g_variant_get_boolean (setting_var);
            }

            g_variant_unref (setting_var);
        } else {
            GVariant *setting_var;

            target_type = G_VARIANT_TYPE (split[GSETTINGS_TYPE_INDEX]);

            setting_var = g_settings_get_value (cond->settings, cond->key);
            setting_type = g_variant_get_type (setting_var);

            if (g_variant_type_equal (setting_type, target_type)) {
                GVariant *target_var;

                target_var = g_variant_parse (target_type,
                                              split[GSETTINGS_VAL_INDEX],
                                              NULL, NULL, NULL);

                if (target_var != NULL) {
                    gint vector = g_variant_compare (setting_var, target_var);

                    iter_pass = try_vector (split[GSETTINGS_OP_INDEX], vector);

                    g_variant_unref (target_var);
                } else {
                    g_warning ("Nemo Action: gsettings value could not be parsed into a valid GVariant");
                }
            }

            g_variant_unref (setting_var);
        }

        g_strfreev (split);

        DEBUG ("Checking gsettings condition: '%s' - evaluated to %s",
               cond->condition_string,
               iter_pass ? "TRUE" : "FALSE");

        /* This should just break here, except the catch with GSettings changed signal handler,
         * where the value must be retrieved once with a handler connected before the changed signal
         * will begin being emitted.  So, we should go thru all conditions so during setup none
         * of the signals are skipped. */
        if (!iter_pass) {
            pass = FALSE;
        }
    }

    DEBUG ("GSettings satisfied: %s",
           pass ? "TRUE" : "FALSE");

    old_satisfied = action->gsettings_satisfied;
    action->gsettings_satisfied = pass;

    if (pass != old_satisfied) {
        g_signal_emit (action, signals[CONDITION_CHANGED], 0);
    }

    action->gsettings_recalc_timeout_id = 0;
    return FALSE;
}

static void
queue_recalc_gsettings_conditions (NemoAction *action)
{
    if (action->constructing) {
        return;
    }

    if (action->gsettings_recalc_timeout_id != 0) {
        g_source_remove (action->gsettings_recalc_timeout_id);
        action->gsettings_recalc_timeout_id = 0;
    }

    action->gsettings_recalc_timeout_id = g_idle_add ((GSourceFunc) recalc_gsettings_conditions,
                                                      action);
}

static void
setup_gsettings_condition (NemoAction *action,
                           const gchar *condition)
{
    GSettingsSchemaSource *schema_source;
    GSettingsSchema *schema;
    gchar **split;
    gint len;

    split = g_strsplit (condition, " ", 6);
    len = g_strv_length (split);

    if (len != 6 &&
        len != 3) {
        g_strfreev (split);
        return;
    }

    if (g_strcmp0 (split[0], "gsettings") != 0) {
        g_strfreev (split);
        return;
    }

    if (len == 6 &&
        (!g_variant_type_string_is_valid (split[GSETTINGS_TYPE_INDEX]) ||
         !operator_is_valid (split[GSETTINGS_OP_INDEX]))) {
        g_warning ("Nemo Action: Either gsettings variant type (%s) or operator (%s) is invalid.",
                   split[GSETTINGS_TYPE_INDEX], split[GSETTINGS_OP_INDEX]);
        g_strfreev (split);
        return;
    }

    schema_source = g_settings_schema_source_get_default();
    schema = g_settings_schema_source_lookup (schema_source, split[GSETTINGS_SCHEMA_INDEX], TRUE);

    if (schema) {
        GSettings *settings;
        gchar **keys;
        gint i;

        settings = g_settings_new (split[GSETTINGS_SCHEMA_INDEX]);
        keys = g_settings_list_keys (settings);

        for (i = 0; i < g_strv_length (keys); i++) {
            if (g_strcmp0 (keys[i], split[GSETTINGS_KEY_INDEX]) == 0) {
                GSettingsCondition *cond;
                gchar *signal_string;

                cond = g_new0 (GSettingsCondition, 1);

                cond->action = action;
                cond->condition_string = g_strdup (condition);
                cond->settings = g_object_ref (settings);
                cond->key = g_strdup (keys[i]);

                signal_string = g_strdup_printf ("changed::%s", cond->key);

                cond->handler_id = g_signal_connect_swapped (settings,
                                                             signal_string,
                                                             G_CALLBACK (queue_recalc_gsettings_conditions),
                                                             action);

                action->gsettings = g_list_prepend (action->gsettings, cond);

                g_free (signal_string);

                break;
            }
        }

        g_object_unref (settings);
        g_strfreev (keys);
        g_settings_schema_unref (schema);
    }

    g_strfreev (split);
}

static void
strip_custom_modifier (const gchar *raw, gboolean *custom, gchar **out)
{
    if (g_str_has_prefix (raw, "<") && g_str_has_suffix (raw, ">")) {
        gchar **split = g_strsplit_set (raw, "<>", 3);
        *out = g_strdup (split[1]);
        *custom = TRUE;
        g_strfreev (split);
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

    gboolean escape_space;

    escape_space = g_key_file_get_boolean (key_file,
                                           ACTION_FILE_GROUP,
                                           KEY_WHITESPACE,
                                           NULL);

    gboolean run_in_terminal;

    run_in_terminal = g_key_file_get_boolean (key_file,
                                              ACTION_FILE_GROUP,
                                              KEY_TERMINAL,
                                              NULL);

    gboolean is_desktop = FALSE;

    if (conditions && condition_count > 0) {
        guint j;
        gchar *condition;
        for (j = 0; j < condition_count; j++) {
            condition = conditions[j];
            if (g_str_has_prefix (condition, "dbus")) {
                setup_dbus_condition (action, condition);
            }
            else
            if (g_str_has_prefix (condition, "gsettings")) {
                setup_gsettings_condition (action, condition);
            }
            else
            if (g_strcmp0 (condition, "desktop") == 0) {
                is_desktop = TRUE;
            }
            else
            if (g_strcmp0 (condition, "removable") == 0) {
                /* this is handled in nemo_action_get_visibility() */
            }
            else {
                g_warning ("Ignoring invalid condition: %s."
                           " See sample action at /usr/share/nemo/actions/sample.nemo_action", condition);
            }
        }
    }

    gchar *exec = NULL;
    gboolean use_parent_dir = FALSE;

    strip_custom_modifier (exec_raw, &use_parent_dir, &exec);
    g_free (exec_raw);

    TokenType token_type;

    action->show_in_blank_desktop = is_desktop &&
                                    type == SELECTION_NONE &&
                                    find_token_type (exec, &token_type) == NULL;

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
                   "escape-space", escape_space,
                   "run-in-terminal", run_in_terminal,
                    NULL);

    action->constructing = FALSE;

    DEBUG ("Initial action gsettings and dbus update (%s)", action->key_file_path);
    queue_recalc_dbus_conditions (action);
    queue_recalc_gsettings_conditions (action);
    DEBUG ("Initial action gsettings and dbus complete (%s)", action->key_file_path);

    g_free (orig_label);
    g_free (orig_tt);
    g_free (icon_name);
    g_free (stock_id);
    g_free (exec);
    g_free (parent_dir);
    g_free (quote_type_string);
    g_free (separator);
    g_strfreev (ext);
    g_strfreev (mimes);
    g_strfreev (conditions);
    g_key_file_free (key_file);
}

NemoAction *
nemo_action_new (const gchar *name, 
                 const gchar *path)
{
    GKeyFile *key_file = g_key_file_new();

    g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, NULL);

    if (!g_key_file_has_group (key_file, ACTION_FILE_GROUP)) {
        g_key_file_free (key_file);
        return NULL;
    }

    if (g_key_file_has_key (key_file, ACTION_FILE_GROUP, KEY_ACTIVE, NULL)) {
        if (!g_key_file_get_boolean (key_file, ACTION_FILE_GROUP, KEY_ACTIVE, NULL)) {
            g_key_file_free (key_file);
            return NULL;
        }
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
        guint i = 0;
        for (i = 0; i < g_strv_length (deps); i++) {
            if (g_path_is_absolute (deps[i])) {
                if (!g_file_test (deps[i], G_FILE_TEST_EXISTS)) {
                    finish = FALSE;
                    DEBUG ("Missing action dependency: %s", deps[i]);
                }
            } else {
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
    }

    if (orig_label == NULL || exec_raw == NULL || (ext == NULL && mimes == NULL) || selection_string == NULL) {
        g_warning ("An action definition requires, at minimum, "
                   "a Label field, an Exec field, a Selection field, and an either an Extensions or Mimetypes field.\n"
                   "Check the %s file for missing fields.", path);
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
    g_free (action->separator);

    if (action->dbus) {
        g_list_free_full (action->dbus, (GDestroyNotify) dbus_condition_free);
        action->dbus = NULL;
    }

    if (action->gsettings) {
        g_list_free_full (action->gsettings, (GDestroyNotify) gsettings_condition_free);
        action->gsettings = NULL;
    }

    if (action->dbus_recalc_timeout_id != 0) {
        g_source_remove (action->dbus_recalc_timeout_id);
        action->dbus_recalc_timeout_id = 0;
    }

    if (action->gsettings_recalc_timeout_id != 0) {
        g_source_remove (action->gsettings_recalc_timeout_id);
        action->gsettings_recalc_timeout_id = 0;
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
    case PROP_ESCAPE_SPACE:
      action->escape_space = g_value_get_boolean (value);
      break;
    case PROP_RUN_IN_TERMINAL:
      action->run_in_terminal = g_value_get_boolean (value);
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
    case PROP_ESCAPE_SPACE:
      g_value_set_boolean (value, action->escape_space);
      break;
    case PROP_RUN_IN_TERMINAL:
      g_value_set_boolean (value, action->run_in_terminal);
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

    ptr = g_strstr_len (str, -1, "%");

    if (ptr != NULL) {
        if (g_str_has_prefix (ptr, TOKEN_EXEC_FILE_LIST)) {
            *token_type = TOKEN_PATH_LIST;
            return ptr;
        }
        if (g_str_has_prefix (ptr, TOKEN_EXEC_URI_LIST)) {
            *token_type = TOKEN_URI_LIST;
            return ptr;
        }
        if (g_str_has_prefix (ptr, TOKEN_EXEC_PARENT)) {
            *token_type = TOKEN_PARENT_PATH;
            return ptr;
        }
        if (g_str_has_prefix (ptr, TOKEN_EXEC_FILE_NAME)) {
            *token_type = TOKEN_FILE_DISPLAY_NAME;
            return ptr;
        }
        if (g_str_has_prefix (ptr, TOKEN_EXEC_PARENT_NAME)) {
            *token_type = TOKEN_PARENT_DISPLAY_NAME;
            return ptr;
        }
        if (g_str_has_prefix (ptr, TOKEN_LABEL_FILE_NAME)) {
            *token_type = TOKEN_FILE_DISPLAY_NAME;
            return ptr;
        }
        if (g_str_has_prefix (ptr, TOKEN_EXEC_DEVICE)) {
            *token_type = TOKEN_DEVICE;
            return ptr;
        }
    }

    return NULL;
}

static gchar *
get_path (NemoAction *action, NemoFile *file)
{
    gchar *ret, *quote_escaped, *orig;

    orig = nemo_file_get_path (file);

    quote_escaped = eel_str_escape_quotes (orig);

    if (action->escape_space) {
        ret = eel_str_escape_spaces (quote_escaped);
    } else {
        ret = g_strdup (quote_escaped);
    }

    g_free (orig);
    g_free (quote_escaped);

    return ret;
}

static GString *
score_append (NemoAction *action, GString *str, const gchar *c)
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
        str = score_append (action, str, action->separator);

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
        default:
            break;
    }

    return str;
}

static gchar *
get_device_path (NemoAction *action, NemoFile *file)
{
    GMount *mount = nemo_file_get_mount (file);
    GVolume *volume = g_mount_get_volume (mount);
    gchar *ret = NULL;

    if (action->escape_space) {
        gchar *id = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
        ret = eel_str_escape_spaces (id);
        g_free (id);
    } else {
        ret = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
    }

    g_object_unref (mount);
    g_object_unref (volume);

    return ret;
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
                    gchar *path = get_path (action, NEMO_FILE (l->data));
                    if (path)
                        str = score_append (action, str, path);
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
                    str = score_append (action, str, uri);
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
            gchar *path = get_path (action, parent);
            if (path == NULL) {
                gchar *name = nemo_file_get_display_name (parent);
                if (g_strcmp0 (name, "x-nemo-desktop") == 0)
                    path = nemo_get_desktop_directory ();
                else
                    path = g_strdup ("");
                g_free (name);
            }
            str = insert_quote (action, str);
            str = score_append (action, str, path);
            str = insert_quote (action, str);
            g_free (path);
            break;
        case TOKEN_FILE_DISPLAY_NAME:
            if (g_list_length (selection) > 0) {
                gchar *file_display_name = nemo_file_get_display_name (NEMO_FILE (selection->data));
                str = score_append (action, str, file_display_name);
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
            str = score_append (action, str, parent_display_name);
            str = insert_quote (action, str);
            g_free (parent_display_name);
            break;
        case TOKEN_DEVICE:
            if (g_list_length (selection) > 0) {
                for (l = selection; l != NULL; l = l->next) {
                    if (!first)
                        str = insert_separator (action, str);
                    str = insert_quote (action, str);
                    gchar *dev = get_device_path (action, NEMO_FILE (l->data));
                    if (dev)
                        str = score_append (action, str, dev);
                    g_free (dev);
                    str = insert_quote (action, str);
                    first = FALSE;
                }
            } else {
                goto default_parent_path;
            }
            break;
        case TOKEN_NONE:
        default:
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

        /* The string may have expanded, and since we modify-in-place using GString, make sure
         * our continuation begins just beyond what we inserted, not the original match position.
         * Otherwise we may get confused by uri escape codes that happen to match *our* replacement
         * tokens (%U, %F, etc...).
         *
         * See: https://github.com/linuxmint/nemo/issues/1956
         */
        ptr = find_token_type (str->str + shift + strlen(insertion), &token_type);
        g_free  (insertion);
    }

    return str;
}

void
nemo_action_activate (NemoAction *action, GList *selection, NemoFile *parent)
{
    GError *error;
    GString *exec = g_string_new (action->exec);

    error = NULL;

    action->escape_underscores = FALSE;

    exec = expand_action_string (action, selection, parent, exec);

    if (action->use_parent_dir) {
        exec = g_string_prepend (exec, G_DIR_SEPARATOR_S);
        exec = g_string_prepend (exec, action->parent_dir);
    }

    DEBUG ("Action Spawning: %s", exec->str);

    if (action->run_in_terminal) {
        gint argcp;
        gchar **argvp;

        if (g_shell_parse_argv (exec->str, &argcp, &argvp, &error)) {
            nemo_launch_application_from_command_array (gdk_screen_get_default (),
                                                        argvp[0],
                                                        TRUE,
                                                        (const char * const *)(argvp + sizeof (gchar)));
            g_strfreev (argvp);
        } else {
            DEBUG ("Could not parse arguments terminal launch.  Possibly turn off Quotes and remove any from your Exec line: %s\n",
                   error->message);
            g_error_free (error);
        }
    } else {
        if (!g_spawn_command_line_async (exec->str, &error)) {
            DEBUG ("Error spawning action: %s\n",
                   error->message);
            g_error_free (error);
        }
    }

    g_string_free (exec, TRUE);
}

static SelectionType
nemo_action_get_selection_type (NemoAction *action)
{
    return action->selection_type;
}

static void
nemo_action_set_extensions (NemoAction *action, gchar **extensions)
{
    gchar **tmp;

    tmp = action->extensions;
    action->extensions = g_strdupv (extensions);
    g_strfreev (tmp);
}

static gchar **
nemo_action_get_extension_list (NemoAction *action)
{
    return action->extensions;
}

static void
nemo_action_set_mimetypes (NemoAction *action, gchar **mimetypes)
{
    gchar **tmp;

    tmp = action->mimetypes;
    action->mimetypes = g_strdupv (mimetypes);
    g_strfreev (tmp);
}

static gchar **
nemo_action_get_mimetypes_list (NemoAction *action)
{
    return action->mimetypes;
}

static void
nemo_action_set_key_file_path (NemoAction *action, const gchar *path)
{
    gchar *tmp;
    tmp = action->key_file_path;
    action->key_file_path = g_strdup (path);
    g_free (tmp);
}

static void
nemo_action_set_exec (NemoAction *action, const gchar *exec)
{
    gchar *tmp;

    tmp = action->exec;
    action->exec = g_strdup (exec);
    g_free (tmp);
}

static void
nemo_action_set_parent_dir (NemoAction *action, const gchar *parent_dir)
{
    gchar *tmp;

    tmp = action->parent_dir;
    action->parent_dir = g_strdup (parent_dir);
    g_free (tmp);
}

static void
nemo_action_set_separator (NemoAction *action, const gchar *separator)
{
    gchar *tmp;

    tmp = action->separator;
    action->separator = g_strdup (separator);
    g_free (tmp);
}

static void
nemo_action_set_conditions (NemoAction *action, gchar **conditions)
{
    gchar **tmp;

    tmp = action->conditions;
    action->conditions = g_strdupv (conditions);
    g_strfreev (tmp);
}

static gchar **
nemo_action_get_conditions (NemoAction *action)
{
    return action->conditions;
}

static void
nemo_action_set_orig_label (NemoAction *action, const gchar *orig_label)
{
    gchar *tmp;

    tmp = action->orig_label;
    action->orig_label = g_strdup (orig_label);
    g_free (tmp);
}

static void
nemo_action_set_orig_tt (NemoAction *action, const gchar *orig_tt)
{
    gchar *tmp;

    tmp = action->orig_tt;
    action->orig_tt = g_strdup (orig_tt);
    g_free (tmp);
}

const gchar *
nemo_action_get_orig_label (NemoAction *action)
{
    return action->orig_label;
}

const gchar *
nemo_action_get_orig_tt (NemoAction *action)
{
    return action->orig_tt;
}


gchar *
nemo_action_get_label (NemoAction *action, GList *selection, NemoFile *parent)
{
    const gchar *orig_label = nemo_action_get_orig_label (action);

    if (orig_label == NULL)
        return NULL;

    action->escape_underscores = TRUE;

    GString *str = g_string_new (orig_label);

    str = expand_action_string (action, selection, parent, str);

    DEBUG ("Action Label: %s", str->str);

    gchar *ret = str->str;
    g_string_free (str, FALSE);
    return ret;
}

gchar *
nemo_action_get_tt (NemoAction *action, GList *selection, NemoFile *parent)
{
    const gchar *orig_tt = nemo_action_get_orig_tt (action);

    if (orig_tt == NULL)
        return NULL;

    action->escape_underscores = FALSE;

    GString *str = g_string_new (orig_tt);

    str = expand_action_string (action, selection, parent, str);

    DEBUG ("Action Tooltip: %s", str->str);

    gchar *ret = str->str;
    g_string_free (str, FALSE);
    return ret;
}

static gboolean
get_dbus_satisfied (NemoAction *action)
{
    return action->dbus_satisfied;
}

static gboolean
get_gsettings_satisfied (NemoAction *action)
{
    return action->gsettings_satisfied;
}

static gboolean
check_exec_condition (NemoAction *action, const gchar *condition, GList *selection)
{
    int return_code;
    GPtrArray *array;
    gchar *exec, *pathed_exec;
    gchar **split;
    gchar **argv;
    gboolean use_parent_dir;

    split = g_strsplit (condition, " ", 2);

    if (g_strv_length (split) != 2) {
        g_strfreev (split);
        return FALSE;
    }

    if (g_strcmp0 (split[0], "exec") != 0) {
        g_strfreev (split);
        return FALSE;
    }

    array = g_ptr_array_new ();

    strip_custom_modifier (split[1], &use_parent_dir, &exec);

    if (use_parent_dir) {
        pathed_exec = g_build_path (G_DIR_SEPARATOR_S,
                                    action->parent_dir,
                                    exec,
                                    NULL);
    } else {
        pathed_exec = g_strdup (exec);
    }

    g_free (exec);

    g_ptr_array_add (array, pathed_exec);

    if (selection && g_list_length (selection) > 0) {
        GList *iter;

        for (iter = selection; iter != NULL; iter = iter->next) {
            NemoFile *file = NEMO_FILE (iter->data);

            g_ptr_array_add (array, nemo_file_get_path (file));
        }

    }

    g_strfreev (split);

    g_ptr_array_add (array, NULL);
    argv = (gchar **) g_ptr_array_free (array, FALSE);

    g_spawn_sync (NULL,
                  argv,
                  NULL,
                  G_SPAWN_SEARCH_PATH,
                  NULL, NULL, NULL, NULL,
                  &return_code,
                  NULL);

    DEBUG ("Action checking exec condition '%s' returned: %d", argv[0], return_code);
    if (action->log_output) {
        g_printerr ("Action checking exec condition '%s' returned: %d\n", argv[0], return_code);
    }

    g_strfreev (argv);

    return (return_code == 0);
}

static gboolean
get_is_dir (NemoFile *file)
{
    gboolean ret = FALSE;

    GFile *f = nemo_file_get_location (file);

    if (g_file_is_native (f)) {
        gchar *path;

        path = g_file_get_path (f);
        ret = g_file_test (path, G_FILE_TEST_IS_DIR);
        g_free (path);
    } else {
        ret = nemo_file_is_directory (file);
    }

    g_object_unref (f);

    return ret;
}

gboolean
nemo_action_get_visibility (NemoAction *action,
                            GList *selection,
                            NemoFile *parent,
                            gboolean for_places)
{
    // Check DBUS
    if (!get_dbus_satisfied (action))
        return FALSE;

    if (!get_gsettings_satisfied (action))
        return FALSE;

    // Check selection
    gboolean selection_type_show = FALSE;
    SelectionType selection_type = nemo_action_get_selection_type (action);

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

    if (!selection_type_show)
        return FALSE;

    // Check extensions and mimetypes
    gboolean extension_type_show = TRUE;
    gchar **extensions = nemo_action_get_extension_list (action);
    gchar **mimetypes = nemo_action_get_mimetypes_list (action);

    guint ext_count = extensions != NULL ? g_strv_length (extensions) : 0;
    guint mime_count = mimetypes != NULL ? g_strv_length (mimetypes) : 0;

    if (ext_count == 1 && g_strcmp0 (extensions[0], "any") == 0) {
        extension_type_show = TRUE;
    }
    else {
      gboolean found_match = TRUE;
      GList *iter;
      for (iter = selection; iter != NULL && found_match; iter = iter->next) {
          found_match = FALSE;
          gboolean is_dir;
          gchar *raw_fn = nemo_file_get_name (NEMO_FILE (iter->data));
          gchar *filename = g_ascii_strdown (raw_fn, -1);
          g_free (raw_fn);
          guint i;

          is_dir = get_is_dir (iter->data);

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
                      gchar *str = g_ascii_strdown (extensions[i], -1);
                      if (g_str_has_suffix (filename, str)) {
                          found_match = TRUE;
                      }

                      g_free (str);

                      if (found_match) {
                          break;
                      }
                  }
              }
          }

          g_free (filename);

          if (mime_count > 0) {
              for (i = 0; i < mime_count; i++) {
                  if (nemo_file_is_mime_type (NEMO_FILE (iter->data), mimetypes[i])) {
                      found_match = TRUE;
                      break;
                  }
              }
          }

          if (nemo_file_is_mime_type (NEMO_FILE (iter->data), "application/x-nemo-link")) {
              found_match = FALSE;
          }
      }
      extension_type_show = found_match;
    }
    if (!extension_type_show)
        return FALSE;

    // Check conditions
    gboolean condition_type_show = TRUE;
    gchar **conditions = nemo_action_get_conditions (action);
    guint condition_count = conditions != NULL ? g_strv_length (conditions) : 0;

    if (condition_count > 0) {
        guint j;
        gchar *condition;
        for (j = 0; j < condition_count; j++) {
            condition = conditions[j];
            if (g_strcmp0 (condition, "desktop") == 0) {
                gchar *name = nemo_file_get_display_name (parent);
                if (g_strcmp0 (name, "x-nemo-desktop") != 0)
                    condition_type_show = FALSE;
                g_free (name);
            } else if (g_strcmp0 (condition, "removable") == 0) {
                gboolean is_removable = FALSE;
                if (g_list_length (selection) > 0) {
                    NemoFile *file;
                    GMount *mount = NULL;

                    file = NEMO_FILE (selection->data);

                    mount = nemo_file_get_mount (file);

                    /* find_enclosing_mount can block, so only bother when activated
                     * from the places sidebar (which is strictly done on-demand),
                     * so we don't drag down any view loads. */
                    if (!mount && for_places) {
                        GFile *f;

                        f = nemo_file_get_location (file);

                        if (g_file_is_native (f)) {
                            mount = g_file_find_enclosing_mount (f, NULL, NULL);
                            nemo_file_set_mount (file, mount);
                        }

                        g_object_unref (f);
                    }

                    if (mount) {
                        GDrive *drive;

                        drive = g_mount_get_drive (mount);

                        if (drive) {
                            if (g_drive_is_removable (drive)) {
                                is_removable = TRUE;
                            }

                            g_object_unref (drive);
                        }
                    }
                }
                condition_type_show = is_removable;
            } else if (g_str_has_prefix (condition, "exec")) {
                condition_type_show = check_exec_condition (action, condition, selection);
            }

            if (!condition_type_show)
                break;
        }
    }

    if (!condition_type_show)
        return FALSE;

    return TRUE;
}

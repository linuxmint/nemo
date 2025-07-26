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
#include "nemo-action-symbols.h"
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include "nemo-file-utilities.h"
#include "nemo-program-choosing.h"
#include "nemo-ui-utilities.h"

#define DEBUG_FLAG NEMO_DEBUG_ACTIONS
#include <libnemo-private/nemo-debug.h>

#if (!GLIB_CHECK_VERSION(2,50,0))
#define g_drive_is_removable g_drive_is_media_removable
#endif

#ifndef GLIB_VERSION_2_70
#define g_pattern_spec_match g_pattern_match
#endif

typedef struct {
    SelectionType selection_type;
    gchar **extensions;
    gchar **mimetypes;
    gchar *exec;
    gchar **conditions;
    gchar *separator;
    QuoteType quote_type;
    gchar *orig_label;
    gchar *orig_tt;
    gboolean use_parent_dir;
    GList *dbus;
    guint dbus_recalc_timeout_id;
    GList *gsettings;
    guint gsettings_recalc_timeout_id;
    gboolean dbus_satisfied;
    gboolean gsettings_satisfied;
    gboolean escape_underscores;
    gboolean escape_space;
    gboolean show_in_blank_desktop;
    gboolean run_in_terminal;
    gchar *uri_scheme;

    GList *allowed_location_patterns;
    GList *forbidden_location_patterns;
    GList *allowed_location_filenames;
    GList *forbidden_location_filenames;

    GList *allowed_patterns;
    GList *forbidden_patterns;
    GList *allowed_filenames;
    GList *forbidden_filenames;

    gboolean constructing;
} NemoActionPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NemoAction, nemo_action, GTK_TYPE_ACTION)

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

static gchar   *find_token_type (const gchar *str, TokenType *token_type);

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
  PROP_CONDITIONS,
  PROP_URI_SCHEME,
  PROP_LAST
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

typedef struct {
    GPatternSpec *pattern;
    gboolean      absolute;
} MatchPattern;

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

static MatchPattern *
new_match_pattern (const gchar *pattern)
{
    MatchPattern *mp = g_new0 (MatchPattern, 1);

    mp->pattern = g_pattern_spec_new (pattern);

    // A pattern with a leading * can be matched against a full path, whether it has
    // any other path elements or not (*/foo/bar/* or *.foo)
    mp->absolute = g_str_has_prefix (pattern, "/") || g_str_has_prefix (pattern, "*");

    return mp;
}

static void
free_match_pattern (gpointer data)
{
    MatchPattern *mp = (MatchPattern *) data;

    g_pattern_spec_free (mp->pattern);
    g_free (mp);
}

static void
nemo_action_init (NemoAction *action)
{
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);

    action->key_file_path = NULL;
    action->parent_dir = NULL;
    action->uuid = NULL;

    priv->selection_type = SELECTION_SINGLE;
    priv->quote_type = QUOTE_TYPE_NONE;
    priv->dbus_satisfied = TRUE;
    priv->dbus_recalc_timeout_id = 0;
    priv->gsettings_satisfied = TRUE;
    priv->gsettings_recalc_timeout_id = 0;
    priv->constructing = TRUE;
}

static void
nemo_action_class_init (NemoActionClass *klass)
{
    GObjectClass         *object_class = G_OBJECT_CLASS(klass);
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
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);

    GList *l;
    DBusCondition *c;
    gboolean pass, old_satisfied;

    DEBUG ("Recalculating dbus conditions for %s", action->key_file_path);

    pass = TRUE;

    for (l = priv->dbus; l != NULL; l = l->next) {
        c = (DBusCondition *) l->data;

        DEBUG ("Checking dbus name for an owner: '%s' - evaluated to %s",
               c->name,
               c->exists ? "TRUE" : "FALSE");

        if (!c->exists) {
            pass = FALSE;
            break;
        }
    }

    old_satisfied = priv->dbus_satisfied;
    priv->dbus_satisfied = pass;

    DEBUG ("DBus satisfied: %s",
           pass ? "TRUE" : "FALSE");

    if (pass != old_satisfied) {
        g_signal_emit (action, signals[CONDITION_CHANGED], 0);
    }

    priv->dbus_recalc_timeout_id = 0;
    return FALSE;
}

static void
queue_recalc_dbus_conditions (NemoAction *action)
{
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);

    if (priv->constructing) {
        return;
    }

    if (priv->dbus_recalc_timeout_id != 0) {
        g_source_remove (priv->dbus_recalc_timeout_id);
        priv->dbus_recalc_timeout_id = 0;
    }
    priv->dbus_recalc_timeout_id = g_idle_add ((GSourceFunc) recalc_dbus_conditions,
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
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);

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
    priv->dbus = g_list_append (priv->dbus, cond);
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
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);

    GList *l;
    gboolean pass, old_satisfied;

    DEBUG ("Recalculating gsettings conditions for %s", action->key_file_path);

    pass = TRUE;

    for (l = priv->gsettings; l != NULL; l = l->next) {
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

    old_satisfied = priv->gsettings_satisfied;
    priv->gsettings_satisfied = pass;

    if (pass != old_satisfied) {
        g_signal_emit (action, signals[CONDITION_CHANGED], 0);
    }

    priv->gsettings_recalc_timeout_id = 0;
    return FALSE;
}

static void
queue_recalc_gsettings_conditions (NemoAction *action)
{
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);

    if (priv->constructing) {
        return;
    }

    if (priv->gsettings_recalc_timeout_id != 0) {
        g_source_remove (priv->gsettings_recalc_timeout_id);
        priv->gsettings_recalc_timeout_id = 0;
    }

    priv->gsettings_recalc_timeout_id = g_idle_add ((GSourceFunc) recalc_gsettings_conditions,
                                                      action);
}

static void
setup_gsettings_condition (NemoAction *action,
                           const gchar *condition)
{
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);

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

                priv->gsettings = g_list_prepend (priv->gsettings, cond);

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

static void
populate_patterns_and_filenames (NemoAction   *action,
                                 gchar       **array,
                                 GList       **allowed_patterns,
                                 GList       **forbidden_patterns,
                                 GList       **allowed_filenames,
                                 GList       **forbidden_filenames)
{
    *allowed_patterns = NULL;
    *forbidden_patterns = NULL;
    *allowed_filenames = NULL;
    *forbidden_filenames = NULL;

    if (array == NULL) {
        return;
    }

    gint i;

    for (i = 0; i < g_strv_length (array); i++) {
        GString *str = g_string_new (array[i]);

        g_string_replace (str, "~", g_get_home_dir (), 1);

        if (g_strstr_len (str->str, -1, "?") || g_strstr_len (str->str, -1, "*")) {
            if (g_str_has_prefix (str->str, "!")) {
                *forbidden_patterns = g_list_prepend (*forbidden_patterns, new_match_pattern (str->str + 1));
            } else {
                *allowed_patterns = g_list_prepend (*allowed_patterns, new_match_pattern (str->str));
            }
        }
        else
        {
            if (g_str_has_prefix (str->str, "!")) {
                *forbidden_filenames = g_list_prepend (*forbidden_filenames, g_strdup (str->str + 1));
            } else {
                *allowed_filenames = g_list_prepend (*allowed_filenames, g_strdup (str->str));
            }
        }

        g_string_free (str, TRUE);
    }

    *allowed_patterns = g_list_reverse (*allowed_patterns);
    *forbidden_patterns = g_list_reverse (*forbidden_patterns);
    *allowed_filenames = g_list_reverse (*allowed_filenames);
    *forbidden_filenames = g_list_reverse (*forbidden_filenames);
}

void
nemo_action_constructed (GObject *object)
{
    NemoAction *action = NEMO_ACTION (object);
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);
    GIcon *gicon;

    G_OBJECT_CLASS (nemo_action_parent_class)->constructed (object);

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

    gicon = NULL;

    if (icon_name != NULL) {
        gboolean prepend_action_dir = FALSE;
        gchar *real_icon_name;

        strip_custom_modifier (icon_name, &prepend_action_dir, &real_icon_name);

        if (prepend_action_dir) {
            gchar *action_dir = g_path_get_dirname (action->key_file_path);
            gchar *icon_path = g_build_filename (action_dir, real_icon_name, NULL);

            g_free (action_dir);
            g_free (real_icon_name);

            real_icon_name = icon_path;
        }

        if (g_path_is_absolute (real_icon_name)) {
            GFile *icon_file = g_file_new_for_path (real_icon_name);
            if (g_file_is_native (icon_file)) {
                gicon = g_file_icon_new (icon_file);
            }
            g_object_unref (icon_file);
        }

        if (gicon == NULL) {
            gicon = g_themed_icon_new (real_icon_name);
        }

        g_free (real_icon_name);
    }
    g_free (icon_name);

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

    gchar *uri_scheme = g_key_file_get_string (key_file,
                                               ACTION_FILE_GROUP,
                                               KEY_URI_SCHEME,
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

    gchar **locations = g_key_file_get_string_list (key_file,
                                                    ACTION_FILE_GROUP,
                                                    KEY_LOCATIONS,
                                                    NULL,
                                                    NULL);

    populate_patterns_and_filenames (action, locations,
                                     &priv->allowed_location_patterns,
                                     &priv->forbidden_location_patterns,
                                     &priv->allowed_location_filenames,
                                     &priv->forbidden_location_filenames);
    g_strfreev (locations);

    gchar **files = g_key_file_get_string_list (key_file,
                                                ACTION_FILE_GROUP,
                                                KEY_FILES,
                                                NULL,
                                                NULL);

    populate_patterns_and_filenames (action, files,
                                     &priv->allowed_patterns,
                                     &priv->forbidden_patterns,
                                     &priv->allowed_filenames,
                                     &priv->forbidden_filenames);
    g_strfreev (files);

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
            if (g_str_has_prefix (condition, "exec")) {
                /* handled in nemo_action_get_visibility */
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

    priv->show_in_blank_desktop = is_desktop &&
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
                   "gicon", gicon,
                   "stock-id", stock_id,
                    NULL);

    priv->orig_label = orig_label;
    priv->orig_tt = orig_tt;
    priv->exec = exec;
    priv->selection_type = type;
    priv->extensions = ext;
    priv->mimetypes = mimes;
    action->parent_dir = parent_dir;
    priv->use_parent_dir = use_parent_dir;
    priv->quote_type = quote_type;
    priv->separator = separator;
    priv->conditions = conditions;
    priv->escape_space = escape_space;
    priv->run_in_terminal = run_in_terminal;
    priv->uri_scheme = uri_scheme;

    priv->constructing = FALSE;

    DEBUG ("Initial action gsettings and dbus update (%s)", action->key_file_path);
    queue_recalc_dbus_conditions (action);
    queue_recalc_gsettings_conditions (action);
    DEBUG ("Initial action gsettings and dbus complete (%s)", action->key_file_path);

    g_clear_object (&gicon);
    g_free (stock_id);
    g_free (quote_type_string);
    g_key_file_free (key_file);
}

NemoAction *
nemo_action_new (const gchar *name, 
                 const gchar *path)
{
    GKeyFile *key_file = g_key_file_new();

    g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, NULL);

    if (!g_key_file_has_group (key_file, ACTION_FILE_GROUP)) {
        DEBUG ("Action file '%s' is missing [Nemo Action] group, skipping.", path);
        g_key_file_free (key_file);
        return NULL;
    }

    if (g_key_file_has_key (key_file, ACTION_FILE_GROUP, KEY_ACTIVE, NULL)) {
        if (!g_key_file_get_boolean (key_file, ACTION_FILE_GROUP, KEY_ACTIVE, NULL)) {
            DEBUG ("Action file '%s' is marked inactive, skipping.", path);
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
            gboolean reverse, found;

            reverse = g_str_has_prefix (deps[i], "!");
            found = FALSE;

            const gchar *prg_name = reverse ? deps[i] + 1 : deps[i];

            if (g_path_is_absolute (prg_name)) {
                if ((!nemo_path_is_network_safe (prg_name)) && g_file_test (prg_name, G_FILE_TEST_EXISTS)) {
                    found = TRUE;
                }
            } else {
                gchar *p = g_find_program_in_path (prg_name);
                if (p != NULL) {
                    found = TRUE;
                    g_free (p);
                }
            }

            if (reverse) {
                if (found) {
                    finish = FALSE;
                    g_autofree gchar *base = g_path_get_basename (path);
                    g_warning_once ("Action '%s' is missing reverse dependency: %s", base, deps[i]);
                    break;
                }
            } else {
                if (!found) {
                    finish = FALSE;
                    g_autofree gchar *base = g_path_get_basename (path);
                    g_warning_once ("Action '%s' is missing dependency: %s", base, deps[i]);
                    break;
                }
            }
        }
    }

    if (orig_label == NULL || exec_raw == NULL || (ext == NULL && mimes == NULL) || selection_string == NULL) {
        g_warning ("An action definition requires, at minimum, "
                   "Name, Exec and Selection fields, and either an Extensions or Mimetypes field.\n"
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
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);

    g_free (action->key_file_path);
    g_strfreev (priv->extensions);
    g_strfreev (priv->mimetypes);
    g_strfreev (priv->conditions);
    g_free (priv->exec);
    g_free (action->parent_dir);
    g_free (priv->orig_label);
    g_free (priv->orig_tt);
    g_free (priv->separator);
    g_free (priv->uri_scheme);
    g_free (action->uuid);

    g_list_free_full (priv->dbus, (GDestroyNotify) dbus_condition_free);
    g_list_free_full (priv->gsettings, (GDestroyNotify) gsettings_condition_free);

    g_list_free_full (priv->allowed_location_patterns, (GDestroyNotify) free_match_pattern);
    g_list_free_full (priv->forbidden_location_patterns, (GDestroyNotify) free_match_pattern);
    g_list_free_full (priv->allowed_patterns, (GDestroyNotify) free_match_pattern);
    g_list_free_full (priv->forbidden_patterns, (GDestroyNotify) free_match_pattern);
    g_list_free_full (priv->allowed_location_filenames, (GDestroyNotify) g_free);
    g_list_free_full (priv->forbidden_location_filenames, (GDestroyNotify) g_free);
    g_list_free_full (priv->allowed_filenames, (GDestroyNotify) g_free);
    g_list_free_full (priv->forbidden_filenames, (GDestroyNotify) g_free);

    g_clear_handle_id (&priv->dbus_recalc_timeout_id, g_source_remove);
    g_clear_handle_id (&priv->gsettings_recalc_timeout_id, g_source_remove);

    G_OBJECT_CLASS (nemo_action_parent_class)->finalize (object);
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
      action->key_file_path = g_strdup (g_value_get_string (value));
      action->uuid = nemo_make_action_uuid_for_path (action->key_file_path);
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
        if (g_str_has_prefix (ptr, TOKEN_EXEC_LOCATION_PATH)) {
            *token_type = TOKEN_PARENT_PATH;
            return ptr;
        }
        if (g_str_has_prefix (ptr, TOKEN_EXEC_LOCATION_URI)) {
            *token_type = TOKEN_PARENT_URI;
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
        if (g_str_has_prefix (ptr, TOKEN_EXEC_FILE_NO_EXT)) {
            *token_type = TOKEN_FILE_DISPLAY_NAME_NO_EXT;
            return ptr;
        }
        if (g_str_has_prefix (ptr, TOKEN_EXEC_LITERAL_PERCENT)) {
            *token_type = TOKEN_LITERAL_PERCENT;
            return ptr;
        }
        if (g_str_has_prefix (ptr, TOKEN_EXEC_XID)) {
            *token_type = TOKEN_XID;
            return ptr;
        }
    }

    return NULL;
}

static gchar *
get_path (NemoAction *action, NemoFile *file)
{
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);
    gchar *ret, *orig;

    orig = nemo_file_get_path (file);

    if (priv->quote_type == QUOTE_TYPE_DOUBLE) {
        ret = eel_str_escape_double_quoted_content (orig);
    } else if (priv->quote_type == QUOTE_TYPE_SINGLE) {
        // Replace literal ' with a close ', a \', and an open '
        ret = eel_str_replace_substring (orig, "'", "'\\''");
    } else {
        ret = eel_str_escape_shell_characters (orig);
    }

    g_free (orig);

    return ret;
}

static GString *
score_append (NemoAction *action, GString *str, const gchar *c)
{
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);

    if (priv->escape_underscores) {
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
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);

    if (priv->separator == NULL)
        str = g_string_append (str, " ");
    else
        str = score_append (action, str, priv->separator);

    return str;
}

static GString *
insert_quote (NemoAction *action, GString *str)
{
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);

    switch (priv->quote_type) {
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
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);
    GMount *mount = nemo_file_get_mount (file);

    g_return_val_if_fail (mount != NULL, NULL);

    GVolume *volume = g_mount_get_volume (mount);
    gchar *ret, *id;

    id = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);

    if (priv->quote_type == QUOTE_TYPE_DOUBLE) {
        ret = eel_str_escape_double_quoted_content (id);
    } else if (priv->quote_type == QUOTE_TYPE_SINGLE) {
        // Replace literal ' with a close ', a \', and an open '
        ret = eel_str_replace_substring (id, "'", "'\\''");
    } else {
        ret = eel_str_escape_shell_characters (id);
    }

    g_free (id);

    g_object_unref (mount);
    g_object_unref (volume);

    return ret;
}

static gchar *
get_insertion_string (NemoAction *action,
                      TokenType   token_type,
                      GList      *selection,
                      NemoFile   *parent,
                      GtkWindow  *window)
{
    GList *l;

    GString *str = g_string_new("");
    gboolean first = TRUE;

    switch (token_type) {
        case TOKEN_LITERAL_PERCENT:
            str = g_string_append(str, "%");
            break;
        case TOKEN_XID:
            g_string_append_printf (str, "%lu", eel_gtk_get_window_xid (window));
            break;
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
                goto default_parent_uri;
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
        case TOKEN_PARENT_URI:
            ;
default_parent_uri:
            ;
            gchar *uri;
            gchar *name = nemo_file_get_display_name (parent);
            if (g_strcmp0 (name, "x-nemo-desktop") == 0) {
                gchar *real_desktop_path = nemo_get_desktop_directory ();
                if (real_desktop_path) {
                    GFile *file;
                    file = g_file_new_for_path (real_desktop_path);
                    uri = g_file_get_uri (file);
                    g_object_unref (file);
                    g_free (real_desktop_path);
                } else {
                    uri = NULL;
                }
            } else {
                uri = nemo_file_get_uri (parent);
            }

            str = insert_quote (action, str);
            str = score_append (action, str, uri);
            str = insert_quote (action, str);
            g_free (name);
            g_free (uri);
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
        case TOKEN_FILE_DISPLAY_NAME_NO_EXT:
            if (g_list_length (selection) > 0) {
                gchar *file_display_name = nemo_file_get_display_name (NEMO_FILE (selection->data));
                str = score_append (action, str, eel_filename_strip_extension (file_display_name));
                g_free (file_display_name);
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
expand_action_string (NemoAction *action,
                      GList      *selection,
                      NemoFile   *parent,
                      GString    *str,
                      GtkWindow  *window)
{
    gchar *ptr;
    TokenType token_type;

    ptr = find_token_type (str->str, &token_type);

    while (ptr != NULL) {
        gint shift = ptr - str->str;

        gchar *insertion = get_insertion_string (action, token_type, selection, parent, window);
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
nemo_action_activate (NemoAction *action,
                      GList      *selection,
                      NemoFile   *parent,
                      GtkWindow  *window)
{
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);
    GError *error;
    GString *exec = g_string_new (priv->exec);

    error = NULL;

    priv->escape_underscores = FALSE;

    exec = expand_action_string (action, selection, parent, exec, window);

    if (priv->use_parent_dir) {
        exec = g_string_prepend (exec, G_DIR_SEPARATOR_S);
        exec = g_string_prepend (exec, action->parent_dir);
    }

    DEBUG ("Action Spawning: %s", exec->str);

    if (priv->run_in_terminal) {
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

const gchar *
nemo_action_get_orig_label (NemoAction *action)
{
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);
    return priv->orig_label;
}

void
nemo_action_override_label (NemoAction  *action,
                            const gchar *label)
{
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);
    g_free (priv->orig_label);
    priv->orig_label = g_strdup (label);
}

void
nemo_action_override_icon (NemoAction  *action,
                           const gchar *icon_name)
{
    GIcon *gicon = NULL;

    if (icon_name != NULL && *icon_name != '\0') {
        if (g_path_is_absolute (icon_name)) {
            GFile *icon_file = g_file_new_for_path (icon_name);
            if (g_file_is_native (icon_file)) {
                gicon = g_file_icon_new (icon_file);
            }
            g_object_unref (icon_file);
        }

        if (gicon == NULL) {
            gicon = g_themed_icon_new (icon_name);
        }

        gtk_action_set_gicon (GTK_ACTION (action), gicon);
        g_clear_object (&gicon);
    } else {
        gtk_action_set_gicon(GTK_ACTION (action), NULL);
    }
}

const gchar *
nemo_action_get_orig_tt (NemoAction *action)
{
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);
    return priv->orig_tt;
}

static gchar *
get_final_label (NemoAction *action,
                 GList      *selection,
                 NemoFile   *parent,
                 GtkWindow  *window)
{
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);
    const gchar *orig_label = nemo_action_get_orig_label (action);

    if (orig_label == NULL)
        return NULL;

    priv->escape_underscores = TRUE;

    GString *str = g_string_new (orig_label);

    str = expand_action_string (action, selection, parent, str, window);

    DEBUG ("Action Label: %s", str->str);

    gchar *ret = str->str;
    g_string_free (str, FALSE);
    return ret;
}

static gchar *
get_final_tt (NemoAction *action,
              GList      *selection,
              NemoFile   *parent,
              GtkWindow  *window)
{
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);
    const gchar *orig_tt = nemo_action_get_orig_tt (action);

    if (orig_tt == NULL)
        return NULL;

    priv->escape_underscores = FALSE;

    GString *str = g_string_new (orig_tt);

    str = expand_action_string (action, selection, parent, str, window);

    DEBUG ("Action Tooltip: %s", str->str);

    gchar *ret = str->str;
    g_string_free (str, FALSE);
    return ret;
}

static void
finalize_strings (NemoAction *action,
                  GList      *selection,
                  NemoFile   *parent,
                  GtkWindow  *window)
{
    gchar *label, *tt;

    label = get_final_label (action, selection, parent, window);
    tt = get_final_tt (action, selection, parent, window);

    gtk_action_set_label (GTK_ACTION (action), label);
    gtk_action_set_tooltip (GTK_ACTION (action), tt);

    g_free (label);
    g_free (tt);
}

static gboolean
check_exec_condition (NemoAction  *action,
                      const gchar *condition,
                      GList       *selection,
                      NemoFile    *parent,
                      GtkWindow   *window)
{
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);
    GString *exec;
    GError *error;
    gint return_code;
    gchar *exec_str;
    gchar **split;
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

    strip_custom_modifier (split[1], &use_parent_dir, &exec_str);

    g_strfreev (split);

    exec = g_string_new (exec_str);

    g_free (exec_str);

    error = NULL;

    priv->escape_underscores = FALSE;

    exec = expand_action_string (action, selection, parent, exec, window);

    if (use_parent_dir) {
        exec = g_string_prepend (exec, G_DIR_SEPARATOR_S);
        exec = g_string_prepend (exec, action->parent_dir);
    }

    DEBUG ("Checking exec condition: %s", exec->str);

    if (!g_spawn_command_line_sync (exec->str,
                                    NULL,
                                    NULL,
                                    &return_code,
                                    &error)) {
            DEBUG ("Error spawning exec condition: %s\n",
                   error->message);
            g_error_free (error);
        }

    DEBUG ("Action checking exec condition '%s' returned: %d", exec->str, return_code);

    g_string_free (exec, TRUE);

    return (return_code == 0);
}

static gboolean
get_is_dir (NemoFile *file)
{
    gboolean ret = FALSE;

    GFile *f = nemo_file_get_location (file);

    if (g_file_is_native (f) && (!nemo_location_is_network_safe (f))) {
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

static gboolean
check_is_allowed (NemoAction *action,
                  NemoFile   *parent,
                  GList      *selection,
                  GList      *allowed_patterns,
                  GList      *forbidden_patterns,
                  GList      *allowed_names,
                  GList      *forbidden_names)
{
    // If there are no pattern/name specs, always allow the location..
    if ((allowed_patterns == NULL && forbidden_patterns == NULL && allowed_names == NULL && forbidden_names == NULL)) {
        return TRUE;
    }

    GList *l, *files;

    if (parent != NULL) {
        DEBUG ("Checking pattern/name matching for location: %s", nemo_file_peek_name (parent));
        files = g_list_prepend (NULL, parent);
    }
    else {
        DEBUG ("Checking pattern/name matching for selected files");
        files = selection;
    }

    gboolean allowed = TRUE;

    for (l = files; l != NULL; l = l->next) {
        NemoFile *file = NEMO_FILE (l->data);

        g_autofree gchar *path = nemo_file_get_path (file);
        // If the first file (or the single parent) isn't native, no need to
        // check all selected items, just exit early.
        if (path == NULL) {
            goto check_allowed_done;
        }

        const gchar *name = nemo_file_peek_name (file);
        gboolean allowed_allowed = TRUE;
        gboolean forbidden_allowed = TRUE;
        GList *ll;

        if (allowed_patterns != NULL || allowed_names != NULL) {
            allowed_allowed = FALSE;

            for (ll = allowed_patterns; ll != NULL; ll = ll->next) {
                MatchPattern *mp = (MatchPattern *) ll->data;
                const gchar *test_str = mp->absolute ? path : name;

                if (g_pattern_spec_match (mp->pattern, strlen (test_str), test_str, NULL)) {
                    allowed_allowed = TRUE;
                    break;
                }
            }

            for (ll = allowed_names; ll != NULL; ll = ll->next) {
                gchar *aname = (gchar *) ll->data;

                if (name[0] == '/' && g_strcmp0 (path, aname) == 0) {
                        allowed_allowed = TRUE;
                        break;
                }
                else
                if (g_str_has_suffix (name, aname)) {
                    allowed_allowed = TRUE;
                    break;
                }
            }
        }

        if (forbidden_patterns != NULL || forbidden_names != NULL) {
            // (forbidden_allowed = TRUE;)

            for (ll = forbidden_patterns; ll != NULL; ll = ll->next) {
                MatchPattern *mp = (MatchPattern *) ll->data;
                const gchar *test_str = mp->absolute ? path : name;

                if (g_pattern_spec_match (mp->pattern, strlen (test_str), test_str, NULL)) {
                    forbidden_allowed = FALSE;
                    break;
                }
            }

            for (ll = forbidden_names; ll != NULL; ll = ll->next) {
                gchar *fname = (gchar *) ll->data;
                if (name[0] == '/' && g_strcmp0 (path, fname) == 0) {
                    forbidden_allowed = FALSE;
                    break;
                }
                else
                if (g_str_has_suffix (name, fname)) {
                    forbidden_allowed = FALSE;
                    break;
                }
            }
        }

        DEBUG ("Final result - allowed pass: %d, forbidden pass: %d", allowed_allowed, forbidden_allowed);
        if (!(allowed_allowed && forbidden_allowed)) {
            allowed = FALSE;
            break;
        }
    }

check_allowed_done:
    if (parent != NULL) {
        g_list_free (files);
    }

    return allowed;
}


static gboolean
get_visibility (NemoAction *action,
                GList      *selection,
                NemoFile   *parent,
                gboolean    for_places,
                GtkWindow  *window)
{
    NemoActionPrivate *priv = nemo_action_get_instance_private (action);
    // Check DBUS
    if (!priv->dbus_satisfied)
        return FALSE;

    if (!priv->gsettings_satisfied)
        return FALSE;

    if ((priv->uri_scheme != NULL) && !nemo_file_has_uri_scheme (parent, priv->uri_scheme)) {
        return FALSE;
    }

    if (!check_is_allowed (action, parent, NULL,
                           priv->allowed_location_patterns,
                           priv->forbidden_location_patterns,
                           priv->allowed_location_filenames,
                           priv->forbidden_location_filenames)) {
        return FALSE;
    }

    if (!check_is_allowed (action, NULL, selection,
                           priv->allowed_patterns,
                           priv->forbidden_patterns,
                           priv->allowed_filenames,
                           priv->forbidden_filenames)) {
        return FALSE;
    }

    // Check selection
    gboolean selection_type_show = FALSE;
    guint selected_count = g_list_length (selection);

    switch (priv->selection_type) {
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
            selection_type_show = selected_count == priv->selection_type;
            break;
    }

    if (!selection_type_show)
        return FALSE;

    // Check extensions and mimetypes
    gboolean extension_type_show = TRUE;
    gchar **extensions = priv->extensions;
    gchar **mimetypes = priv->mimetypes;

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
    gchar **conditions = priv->conditions;
    guint condition_count = conditions != NULL ? g_strv_length (conditions) : 0;

    if (condition_count > 0) {
        guint j;
        gchar *condition;
        for (j = 0; j < condition_count; j++) {
            condition = conditions[j];
            if (g_strcmp0 (condition, "desktop") == 0) {
                gchar *name = nemo_file_get_display_name (parent);
                if (g_strcmp0 (name, "x-nemo-desktop") != 0 && !(parent == NULL && priv->show_in_blank_desktop))
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
                            mount = nemo_get_mount_for_location_safe (f);
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
                condition_type_show = check_exec_condition (action,
                                                            condition,
                                                            selection,
                                                            parent,
                                                            window);
            }

            if (!condition_type_show)
                break;
        }
    }

    if (!condition_type_show) {
        return FALSE;
    }

    return TRUE;
}

void
nemo_action_update_display_state (NemoAction *action,
                                  GList      *selection,
                                  NemoFile   *parent,
                                  gboolean    for_places,
                                  GtkWindow  *window)
{
    if (get_visibility (action, selection, parent, for_places, window)) {
        DEBUG ("Action '%s' determined VISIBLE", action->uuid);

        finalize_strings (action, selection, parent, window);
        gtk_action_set_visible (GTK_ACTION (action), TRUE);

        return;
    }

    DEBUG ("Action '%s' determined HIDDEN", action->uuid);
    gtk_action_set_visible (GTK_ACTION (action), FALSE);
}

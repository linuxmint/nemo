/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-

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

#include "nemo-action-wizard.h"
#include "nemo-action-manager.h"
#include "nemo-action-symbols.h"
#include <glib/gi18n.h>
#include <libxapp/xapp-icon-chooser-button.h>
#include <eel/eel-string.h>

struct _NemoActionWizard
{
    GtkAssistant parent_instance;

    /* UI widgets from glade */
    GtkWidget *page_intro;
    GtkWidget *page_basic;
    GtkWidget *page_command;
    GtkWidget *page_selection;
    GtkWidget *page_advanced;
    GtkWidget *page_summary;

    /* Basic info widgets */
    GtkWidget *entry_name;
    GtkWidget *entry_comment;
    GtkWidget *icon_button_box;
    XAppIconChooserButton *icon_button;

    /* Command widgets */
    GtkWidget *entry_exec;
    GtkWidget *button_app_chooser;
    GtkWidget *combo_quote;
    GtkWidget *entry_separator;
    GtkWidget *check_terminal;

    /* Selection widgets */
    GtkWidget *combo_selection;
    GtkWidget *specific_count_box;
    GtkWidget *spin_selection_count;
    GtkWidget *check_extensions;
    GtkWidget *extensions_content_box;
    GtkWidget *combo_extensions_preset;
    GtkWidget *entry_extensions;
    GtkWidget *check_mimetypes;
    GtkWidget *mimetypes_content_box;
    GtkWidget *treeview_mimetypes;
    GtkListStore *mimetypes_store;
    GtkWidget *entry_mimetypes;

    /* Advanced widgets */
    GtkWidget *entry_dependencies;
    GtkWidget *entry_uri_scheme;
    GtkWidget *entry_conditions;
    GtkWidget *entry_files;
    GtkWidget *entry_locations;

    /* Summary widgets */
    GtkWidget *entry_filename;
    GtkWidget *label_save_location;
    GtkWidget *textview_preview;

    /* Token legend boxes */
    GtkWidget *token_legend_basic;
    GtkWidget *token_legend_command;

    /* Sample preview labels */
    GtkWidget *sample_label_basic;
    GtkWidget *sample_tooltip_basic;
    GtkWidget *sample_exec_command;

    /* State */
    gchar *editing_path;  /* NULL for new action, path for editing */
};

G_DEFINE_TYPE (NemoActionWizard, nemo_action_wizard, GTK_TYPE_ASSISTANT)

enum {
    MIME_COL_ENABLED,
    MIME_COL_MIMETYPE,
    MIME_COL_DESCRIPTION,
    MIME_N_COLS
};

static void
update_basic_page_complete (NemoActionWizard *self)
{
    const gchar *name = gtk_entry_get_text (GTK_ENTRY (self->entry_name));
    gboolean complete = (name != NULL && *name != '\0');
    gtk_assistant_set_page_complete (GTK_ASSISTANT (self), self->page_basic, complete);
}

static void
update_command_page_complete (NemoActionWizard *self)
{
    const gchar *exec = gtk_entry_get_text (GTK_ENTRY (self->entry_exec));
    gboolean complete = (exec != NULL && *exec != '\0');
    gtk_assistant_set_page_complete (GTK_ASSISTANT (self), self->page_command, complete);
}

static gboolean
is_valid_mimetype (const gchar *mimetype)
{
    if (mimetype == NULL || *mimetype == '\0') {
        return FALSE;
    }

    /* Must contain exactly one slash */
    const gchar *slash = strchr (mimetype, '/');
    if (slash == NULL) {
        return FALSE;
    }

    /* Check there's only one slash */
    if (strchr (slash + 1, '/') != NULL) {
        return FALSE;
    }

    /* Type (before slash) must be non-empty */
    if (slash == mimetype) {
        return FALSE;
    }

    /* Subtype (after slash) must be non-empty */
    if (*(slash + 1) == '\0') {
        return FALSE;
    }

    /* Check for invalid characters - allow alphanumeric, dash, plus, dot, asterisk */
    for (const gchar *p = mimetype; *p != '\0'; p++) {
        if (*p != '/' && *p != '-' && *p != '+' && *p != '.' && *p != '*' &&
            !g_ascii_isalnum (*p)) {
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
validate_mimetypes_string (const gchar *mimetypes_str, gchar **invalid_entry)
{
    if (invalid_entry != NULL) {
        *invalid_entry = NULL;
    }

    if (mimetypes_str == NULL || *mimetypes_str == '\0') {
        return FALSE;
    }

    gchar **parts = g_strsplit (mimetypes_str, ";", -1);
    gboolean valid = TRUE;
    gboolean has_any = FALSE;

    for (gint i = 0; parts[i] != NULL; i++) {
        gchar *trimmed = g_strstrip (g_strdup (parts[i]));

        /* Skip empty parts (e.g., trailing semicolon) */
        if (*trimmed == '\0') {
            g_free (trimmed);
            continue;
        }

        has_any = TRUE;

        if (!is_valid_mimetype (trimmed)) {
            valid = FALSE;
            if (invalid_entry != NULL) {
                *invalid_entry = g_strdup (trimmed);
            }
            g_free (trimmed);
            break;
        }

        g_free (trimmed);
    }

    g_strfreev (parts);

    return valid && has_any;
}

static gboolean
any_mimetype_selected (NemoActionWizard *self)
{
    GtkTreeIter iter;
    GtkTreeModel *model = GTK_TREE_MODEL (self->mimetypes_store);

    if (!gtk_tree_model_get_iter_first (model, &iter)) {
        return FALSE;
    }

    do {
        gboolean enabled;
        gtk_tree_model_get (model, &iter, MIME_COL_ENABLED, &enabled, -1);
        if (enabled) {
            return TRUE;
        }
    } while (gtk_tree_model_iter_next (model, &iter));

    return FALSE;
}

static void
update_selection_page_complete (NemoActionWizard *self)
{
    GtkTreeIter iter;
    gboolean extensions_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->check_extensions));
    gboolean mimetypes_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->check_mimetypes));
    gboolean extensions_valid = FALSE;
    gboolean mimetypes_valid = FALSE;
    gboolean complete = FALSE;

    /* At least one must be enabled */
    if (!extensions_enabled && !mimetypes_enabled) {
        gtk_assistant_set_page_complete (GTK_ASSISTANT (self), self->page_selection, FALSE);
        return;
    }

    /* Check extensions validity if enabled */
    if (extensions_enabled) {
        if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->combo_extensions_preset), &iter)) {
            GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->combo_extensions_preset));
            gchar *value;
            gtk_tree_model_get (model, &iter, 1, &value, -1);

            if (g_strcmp0 (value, "custom") == 0) {
                const gchar *ext = gtk_entry_get_text (GTK_ENTRY (self->entry_extensions));
                extensions_valid = (ext != NULL && *ext != '\0');
            } else {
                extensions_valid = TRUE;
            }
            g_free (value);
        }
    }

    /* Check mimetypes validity if enabled - either checklist or custom entry */
    if (mimetypes_enabled) {
        const gchar *custom_mimes = gtk_entry_get_text (GTK_ENTRY (self->entry_mimetypes));
        gboolean has_custom = (custom_mimes != NULL && *custom_mimes != '\0');
        gboolean custom_valid = TRUE;
        gboolean has_selected = any_mimetype_selected (self);

        /* Validate custom mimetypes if provided */
        if (has_custom) {
            gchar *invalid_mime = NULL;
            custom_valid = validate_mimetypes_string (custom_mimes, &invalid_mime);
            /* Add/remove error styling and tooltip */
            GtkStyleContext *context = gtk_widget_get_style_context (self->entry_mimetypes);
            if (custom_valid) {
                gtk_style_context_remove_class (context, "error");
                gtk_widget_set_tooltip_text (self->entry_mimetypes, NULL);
            } else {
                gtk_style_context_add_class (context, "error");
                gchar *tooltip = g_strdup_printf (_("'%s' is not a valid mimetype (expected format: type/subtype)"),
                                                  invalid_mime ? invalid_mime : custom_mimes);
                gtk_widget_set_tooltip_text (self->entry_mimetypes, tooltip);
                g_free (tooltip);
            }
            g_free (invalid_mime);
        } else {
            /* No custom input - remove any error styling and tooltip */
            gtk_style_context_remove_class (gtk_widget_get_style_context (self->entry_mimetypes), "error");
            gtk_widget_set_tooltip_text (self->entry_mimetypes, NULL);
        }

        mimetypes_valid = has_selected || (has_custom && custom_valid);
    }

    /* Complete if at least one enabled method is valid */
    if (extensions_enabled && mimetypes_enabled) {
        complete = extensions_valid && mimetypes_valid;
    } else if (extensions_enabled) {
        complete = extensions_valid;
    } else {
        complete = mimetypes_valid;
    }

    gtk_assistant_set_page_complete (GTK_ASSISTANT (self), self->page_selection, complete);
}

static gchar *
expand_sample_tokens (const gchar *input)
{
    if (input == NULL || *input == '\0') {
        return NULL;
    }

    gchar *result = g_strdup (input);
    gchar *tmp;

    struct {
        const gchar *token;
        const gchar *sample;
    } samples[] = {
        { "%F", "/home/user/Documents/example.txt" },
        { "%U", "file:///home/user/Documents/example.txt" },
        { "%N", "example.txt" },
        { "%f", "example.txt" },
        { "%p", "Documents" },
        { "%P", "/home/user/Documents" },
        { "%R", "file:///home/user/Documents" },
        { "%e", "example" },
        { "%D", "/dev/sda1" },
        { "%X", "12345678" },
        { "%%", "%" },
    };

    for (guint i = 0; i < G_N_ELEMENTS (samples); i++) {
        if (strstr (result, samples[i].token) != NULL) {
            tmp = result;
            result = eel_str_replace_substring (tmp, samples[i].token, samples[i].sample);
            g_free (tmp);
        }
    }

    return result;
}

static gchar *
expand_command_tokens (const gchar *input,
                       const gchar *quote_type,
                       const gchar *separator)
{
    if (input == NULL || *input == '\0') {
        return NULL;
    }

    gchar *result = g_strdup (input);
    gchar *tmp;
    const gchar *sep = (separator != NULL && *separator != '\0') ? separator : " ";

    gchar quote_char = '\0';
    if (g_strcmp0 (quote_type, "single") == 0) {
        quote_char = '\'';
    } else if (g_strcmp0 (quote_type, "double") == 0) {
        quote_char = '"';
    } else if (g_strcmp0 (quote_type, "backtick") == 0) {
        quote_char = '`';
    }

    struct {
        const gchar *token;
        const gchar *sample;
        gboolean use_quote;
        gboolean multi_file;
    } samples[] = {
        { "%F", "/home/user/Documents/file1.txt", TRUE, TRUE },
        { "%U", "file:///home/user/Documents/file1.txt", TRUE, TRUE },
        { "%D", "/dev/sda1", TRUE, TRUE },
        { "%P", "/home/user/Documents", TRUE, FALSE },
        { "%R", "file:///home/user/Documents", TRUE, FALSE },
        { "%p", "Documents", TRUE, FALSE },
        { "%N", "file1.txt", FALSE, FALSE },
        { "%f", "file1.txt", FALSE, FALSE },
        { "%e", "file1", FALSE, FALSE },
        { "%X", "12345678", FALSE, FALSE },
        { "%%", "%", FALSE, FALSE },
    };

    for (guint i = 0; i < G_N_ELEMENTS (samples); i++) {
        if (strstr (result, samples[i].token) != NULL) {
            GString *replacement = g_string_new ("");

            if (samples[i].use_quote && quote_char != '\0') {
                g_string_append_c (replacement, quote_char);
            }
            g_string_append (replacement, samples[i].sample);
            if (samples[i].use_quote && quote_char != '\0') {
                g_string_append_c (replacement, quote_char);
            }

            if (samples[i].multi_file) {
                g_string_append (replacement, sep);
                if (samples[i].use_quote && quote_char != '\0') {
                    g_string_append_c (replacement, quote_char);
                }
                if (g_strcmp0 (samples[i].token, "%F") == 0) {
                    g_string_append (replacement, "/home/user/Documents/file2.txt");
                } else if (g_strcmp0 (samples[i].token, "%U") == 0) {
                    g_string_append (replacement, "file:///home/user/Documents/file2.txt");
                } else if (g_strcmp0 (samples[i].token, "%D") == 0) {
                    g_string_append (replacement, "/dev/sdb1");
                }
                if (samples[i].use_quote && quote_char != '\0') {
                    g_string_append_c (replacement, quote_char);
                }
            }

            tmp = result;
            result = eel_str_replace_substring (tmp, samples[i].token, replacement->str);
            g_free (tmp);
            g_string_free (replacement, TRUE);
        }
    }

    return result;
}

static void
update_command_sample_preview (NemoActionWizard *self)
{
    const gchar *exec_text = gtk_entry_get_text (GTK_ENTRY (self->entry_exec));
    const gchar *separator = gtk_entry_get_text (GTK_ENTRY (self->entry_separator));

    gchar *quote_type = NULL;
    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->combo_quote), &iter)) {
        GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->combo_quote));
        gtk_tree_model_get (model, &iter, 1, &quote_type, -1);
    }

    if (exec_text != NULL && *exec_text != '\0') {
        g_autofree gchar *expanded = expand_command_tokens (exec_text, quote_type, separator);
        gtk_label_set_text (GTK_LABEL (self->sample_exec_command), expanded);
        gtk_style_context_remove_class (gtk_widget_get_style_context (self->sample_exec_command), "dim-label");
    } else {
        gtk_label_set_text (GTK_LABEL (self->sample_exec_command), _("(enter a command above)"));
        gtk_style_context_add_class (gtk_widget_get_style_context (self->sample_exec_command), "dim-label");
    }

    g_free (quote_type);
}

static void
update_basic_sample_preview (NemoActionWizard *self)
{
    const gchar *label_text = gtk_entry_get_text (GTK_ENTRY (self->entry_name));
    const gchar *tooltip_text = gtk_entry_get_text (GTK_ENTRY (self->entry_comment));

    if (label_text != NULL && *label_text != '\0') {
        g_autofree gchar *expanded = expand_sample_tokens (label_text);
        gtk_label_set_text (GTK_LABEL (self->sample_label_basic), expanded);
        gtk_style_context_remove_class (gtk_widget_get_style_context (self->sample_label_basic), "dim-label");
    } else {
        gtk_label_set_text (GTK_LABEL (self->sample_label_basic), _("(enter a label above)"));
        gtk_style_context_add_class (gtk_widget_get_style_context (self->sample_label_basic), "dim-label");
    }

    if (tooltip_text != NULL && *tooltip_text != '\0') {
        g_autofree gchar *expanded = expand_sample_tokens (tooltip_text);
        gtk_label_set_text (GTK_LABEL (self->sample_tooltip_basic), expanded);
        gtk_style_context_remove_class (gtk_widget_get_style_context (self->sample_tooltip_basic), "dim-label");
    } else {
        gtk_label_set_text (GTK_LABEL (self->sample_tooltip_basic), _("(enter a tooltip above)"));
        gtk_style_context_add_class (gtk_widget_get_style_context (self->sample_tooltip_basic), "dim-label");
    }
}

static void
on_entry_name_changed (GtkEntry *entry, NemoActionWizard *self)
{
    update_basic_page_complete (self);
    update_basic_sample_preview (self);

    /* Also update the suggested filename */
    const gchar *name = gtk_entry_get_text (entry);
    if (name != NULL && *name != '\0') {
        g_autofree gchar *filename = NULL;
        g_autofree gchar *sanitized = g_strdup (name);

        /* Sanitize the name for use as filename */
        for (gchar *p = sanitized; *p; p++) {
            if (*p == ' ' || *p == '/' || *p == '\\' || *p == ':' ||
                *p == '*' || *p == '?' || *p == '"' || *p == '<' ||
                *p == '>' || *p == '|' || *p == '%') {
                *p = '-';
            }
        }

        filename = g_strdup_printf ("%s.nemo_action", sanitized);
        gtk_entry_set_text (GTK_ENTRY (self->entry_filename), filename);
    }
}

static void
on_entry_comment_changed (GtkEntry *entry, NemoActionWizard *self)
{
    update_basic_sample_preview (self);
}

static void
on_entry_exec_changed (GtkEntry *entry, NemoActionWizard *self)
{
    update_command_page_complete (self);
    update_command_sample_preview (self);
}

static void
on_combo_quote_changed (GtkComboBox *combo, NemoActionWizard *self)
{
    update_command_sample_preview (self);
}

static void
on_entry_separator_changed (GtkEntry *entry, NemoActionWizard *self)
{
    update_command_sample_preview (self);
}

static void
on_app_chooser_clicked (GtkButton *button, NemoActionWizard *self)
{
    GtkWidget *dialog;

    dialog = gtk_app_chooser_dialog_new_for_content_type (GTK_WINDOW (self),
                                                          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                          "application/octet-stream");

    gtk_app_chooser_dialog_set_heading (GTK_APP_CHOOSER_DIALOG (dialog),
                                        _("Select an application"));

    /* Show all apps, not just recommended ones */
    GtkWidget *widget = gtk_app_chooser_dialog_get_widget (GTK_APP_CHOOSER_DIALOG (dialog));
    gtk_app_chooser_widget_set_show_all (GTK_APP_CHOOSER_WIDGET (widget), TRUE);
    gtk_app_chooser_widget_set_show_recommended (GTK_APP_CHOOSER_WIDGET (widget), TRUE);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
        GAppInfo *app_info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (dialog));

        if (app_info != NULL) {
            const gchar *commandline = g_app_info_get_commandline (app_info);

            if (commandline != NULL) {
                /* Remove any %f, %F, %u, %U tokens from the desktop file command
                 * as we'll let the user add their own nemo tokens */
                g_autofree gchar *cmd = g_strdup (commandline);
                gchar *p;

                /* Remove common desktop file tokens */
                const gchar *desktop_tokens[] = { "%f", "%F", "%u", "%U", "%i", "%c", "%k", NULL };
                for (gint i = 0; desktop_tokens[i] != NULL; i++) {
                    while ((p = strstr (cmd, desktop_tokens[i])) != NULL) {
                        memmove (p, p + 2, strlen (p + 2) + 1);
                    }
                }

                /* Trim trailing whitespace */
                g_strchomp (cmd);

                gtk_entry_set_text (GTK_ENTRY (self->entry_exec), cmd);
            }

            g_object_unref (app_info);
        }
    }

    gtk_widget_destroy (dialog);
}

static void
on_mimetype_toggled (GtkCellRendererToggle *cell,
                     gchar                 *path_str,
                     NemoActionWizard      *self)
{
    GtkTreeIter iter;
    GtkTreePath *path = gtk_tree_path_new_from_string (path_str);

    if (gtk_tree_model_get_iter (GTK_TREE_MODEL (self->mimetypes_store), &iter, path)) {
        gboolean enabled;
        gtk_tree_model_get (GTK_TREE_MODEL (self->mimetypes_store), &iter,
                            MIME_COL_ENABLED, &enabled, -1);
        gtk_list_store_set (self->mimetypes_store, &iter,
                            MIME_COL_ENABLED, !enabled, -1);
        update_selection_page_complete (self);
    }

    gtk_tree_path_free (path);
}

static void
on_check_extensions_toggled (GtkToggleButton *button, NemoActionWizard *self)
{
    gboolean active = gtk_toggle_button_get_active (button);
    gtk_widget_set_sensitive (self->extensions_content_box, active);
    update_selection_page_complete (self);
}

static void
on_check_mimetypes_toggled (GtkToggleButton *button, NemoActionWizard *self)
{
    gboolean active = gtk_toggle_button_get_active (button);
    gtk_widget_set_sensitive (self->mimetypes_content_box, active);
    update_selection_page_complete (self);
}

static void
on_extensions_preset_changed (GtkComboBox *combo, NemoActionWizard *self)
{
    GtkTreeIter iter;

    if (gtk_combo_box_get_active_iter (combo, &iter)) {
        GtkTreeModel *model = gtk_combo_box_get_model (combo);
        gchar *value;
        gtk_tree_model_get (model, &iter, 1, &value, -1);

        gboolean show_custom = (g_strcmp0 (value, "custom") == 0);
        gtk_widget_set_visible (self->entry_extensions, show_custom);

        g_free (value);
    }

    update_selection_page_complete (self);
}

static void
on_extensions_entry_changed (GtkEntry *entry, NemoActionWizard *self)
{
    update_selection_page_complete (self);
}

static void
on_mimetypes_entry_changed (GtkEntry *entry, NemoActionWizard *self)
{
    update_selection_page_complete (self);
}

static void
on_selection_type_changed (GtkComboBox *combo, NemoActionWizard *self)
{
    GtkTreeIter iter;

    if (gtk_combo_box_get_active_iter (combo, &iter)) {
        GtkTreeModel *model = gtk_combo_box_get_model (combo);
        gchar *value;
        gtk_tree_model_get (model, &iter, 1, &value, -1);

        gboolean show_count = (g_strcmp0 (value, "count") == 0);
        gtk_widget_set_visible (self->specific_count_box, show_count);

        g_free (value);
    }
}

static gchar *
generate_action_content (NemoActionWizard *self)
{
    GString *content = g_string_new ("[Nemo Action]\n\n");

    /* Name (required) */
    const gchar *name = gtk_entry_get_text (GTK_ENTRY (self->entry_name));
    g_string_append_printf (content, "Name=%s\n", name);

    /* Comment (optional) */
    const gchar *comment = gtk_entry_get_text (GTK_ENTRY (self->entry_comment));
    if (comment != NULL && *comment != '\0') {
        g_string_append_printf (content, "Comment=%s\n", comment);
    }

    /* Exec (required) */
    const gchar *exec = gtk_entry_get_text (GTK_ENTRY (self->entry_exec));
    g_string_append_printf (content, "Exec=%s\n", exec);

    /* Icon-Name (optional) */
    const gchar *icon_name = xapp_icon_chooser_button_get_icon (self->icon_button);
    if (icon_name != NULL && *icon_name != '\0') {
        g_string_append_printf (content, "Icon-Name=%s\n", icon_name);
    }

    /* Selection (required) */
    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->combo_selection), &iter)) {
        GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->combo_selection));
        gchar *value;
        gtk_tree_model_get (model, &iter, 1, &value, -1);

        if (g_strcmp0 (value, "count") == 0) {
            gint count = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->spin_selection_count));
            g_string_append_printf (content, "Selection=%d\n", count);
        } else {
            g_string_append_printf (content, "Selection=%s\n", value);
        }
        g_free (value);
    }

    /* Extensions (only if enabled) */
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->check_extensions))) {
        if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->combo_extensions_preset), &iter)) {
            GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->combo_extensions_preset));
            gchar *value;
            gtk_tree_model_get (model, &iter, 1, &value, -1);

            if (g_strcmp0 (value, "custom") == 0) {
                const gchar *ext = gtk_entry_get_text (GTK_ENTRY (self->entry_extensions));
                if (ext != NULL && *ext != '\0') {
                    g_string_append_printf (content, "Extensions=%s;\n", ext);
                }
            } else if (*value != '\0') {
                g_string_append_printf (content, "Extensions=%s;\n", value);
            }
            g_free (value);
        }
    }

    /* Mimetypes (only if enabled) */
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->check_mimetypes))) {
        GString *mimes_str = g_string_new ("");
        GtkTreeModel *model = GTK_TREE_MODEL (self->mimetypes_store);

        /* Collect selected mimetypes from checklist */
        if (gtk_tree_model_get_iter_first (model, &iter)) {
            do {
                gboolean enabled;
                gchar *mimetype;
                gtk_tree_model_get (model, &iter,
                                    MIME_COL_ENABLED, &enabled,
                                    MIME_COL_MIMETYPE, &mimetype, -1);
                if (enabled) {
                    if (mimes_str->len > 0) {
                        g_string_append_c (mimes_str, ';');
                    }
                    g_string_append (mimes_str, mimetype);
                }
                g_free (mimetype);
            } while (gtk_tree_model_iter_next (model, &iter));
        }

        /* Append custom mimetypes */
        const gchar *custom_mimes = gtk_entry_get_text (GTK_ENTRY (self->entry_mimetypes));
        if (custom_mimes != NULL && *custom_mimes != '\0') {
            if (mimes_str->len > 0) {
                g_string_append_c (mimes_str, ';');
            }
            g_string_append (mimes_str, custom_mimes);
        }

        if (mimes_str->len > 0) {
            g_string_append_printf (content, "Mimetypes=%s;\n", mimes_str->str);
        }
        g_string_free (mimes_str, TRUE);
    }

    /* Quote (optional) */
    if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (self->combo_quote), &iter)) {
        GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (self->combo_quote));
        gchar *value;
        gtk_tree_model_get (model, &iter, 1, &value, -1);
        if (value != NULL && *value != '\0') {
            g_string_append_printf (content, "Quote=%s\n", value);
        }
        g_free (value);
    }

    /* Separator (optional) */
    const gchar *separator = gtk_entry_get_text (GTK_ENTRY (self->entry_separator));
    if (separator != NULL && *separator != '\0') {
        g_string_append_printf (content, "Separator=%s\n", separator);
    }

    /* Terminal (optional) */
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->check_terminal))) {
        g_string_append (content, "Terminal=true\n");
    }

    /* Advanced options */
    const gchar *deps = gtk_entry_get_text (GTK_ENTRY (self->entry_dependencies));
    if (deps != NULL && *deps != '\0') {
        g_string_append_printf (content, "Dependencies=%s;\n", deps);
    }

    const gchar *uri = gtk_entry_get_text (GTK_ENTRY (self->entry_uri_scheme));
    if (uri != NULL && *uri != '\0') {
        g_string_append_printf (content, "UriScheme=%s\n", uri);
    }

    const gchar *conditions = gtk_entry_get_text (GTK_ENTRY (self->entry_conditions));
    if (conditions != NULL && *conditions != '\0') {
        g_string_append_printf (content, "Conditions=%s;\n", conditions);
    }

    const gchar *files = gtk_entry_get_text (GTK_ENTRY (self->entry_files));
    if (files != NULL && *files != '\0') {
        g_string_append_printf (content, "Files=%s;\n", files);
    }

    const gchar *locations = gtk_entry_get_text (GTK_ENTRY (self->entry_locations));
    if (locations != NULL && *locations != '\0') {
        g_string_append_printf (content, "Locations=%s;\n", locations);
    }

    return g_string_free (content, FALSE);
}

static void
update_preview (NemoActionWizard *self)
{
    g_autofree gchar *content = generate_action_content (self);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->textview_preview));
    gtk_text_buffer_set_text (buffer, content, -1);
}

static void
on_prepare (GtkAssistant *assistant, GtkWidget *page, NemoActionWizard *self)
{
    if (!GTK_IS_ASSISTANT (assistant)) {
        return;
    }

    if (page == self->page_summary) {
        update_preview (self);
    }
}

static gboolean
save_action_file (NemoActionWizard *self, GError **error)
{
    const gchar *filename = gtk_entry_get_text (GTK_ENTRY (self->entry_filename));

    if (filename == NULL || *filename == '\0') {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_FILENAME,
                     _("Filename cannot be empty"));
        return FALSE;
    }

    /* Ensure filename has correct extension */
    g_autofree gchar *final_filename = NULL;
    if (!g_str_has_suffix (filename, ".nemo_action")) {
        final_filename = g_strdup_printf ("%s.nemo_action", filename);
    } else {
        final_filename = g_strdup (filename);
    }

    /* Get the user actions directory */
    g_autofree gchar *user_dir = nemo_action_manager_get_user_directory_path ();

    /* Ensure directory exists */
    if (g_mkdir_with_parents (user_dir, 0755) != 0) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     _("Failed to create actions directory: %s"), user_dir);
        return FALSE;
    }

    g_autofree gchar *path = g_build_filename (user_dir, final_filename, NULL);

    /* Generate content and write */
    g_autofree gchar *content = generate_action_content (self);

    return g_file_set_contents (path, content, -1, error);
}

static void
on_apply (GtkAssistant *assistant, NemoActionWizard *self)
{
    GError *error = NULL;

    if (!save_action_file (self, &error)) {
        GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW (self),
                                                    GTK_DIALOG_MODAL,
                                                    GTK_MESSAGE_ERROR,
                                                    GTK_BUTTONS_OK,
                                                    _("Failed to save action"));
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  "%s", error->message);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
        g_error_free (error);
    }

    /* Don't destroy here - let the close signal handle it */
}

static void
on_cancel (GtkAssistant *assistant, NemoActionWizard *self)
{
    gtk_widget_destroy (GTK_WIDGET (self));
}

static void
on_close (GtkAssistant *assistant, NemoActionWizard *self)
{
    gtk_widget_destroy (GTK_WIDGET (self));
}

typedef struct {
    const gchar *token;
    const gchar *description;
} TokenInfo;

typedef struct {
    const gchar *mimetype;
    const gchar *description;
} MimetypeInfo;

static const MimetypeInfo common_mimetypes[] = {
    { "inode/directory", N_("Directories") },
    { "text/*", N_("All text files") },
    { "text/plain", N_("Plain text files") },
    { "image/*", N_("All images") },
    { "audio/*", N_("All audio files") },
    { "video/*", N_("All video files") },
    { "application/pdf", N_("PDF documents") },
    { "application/zip", N_("ZIP archives") },
    { "application/x-compressed-tar", N_("Compressed tar archives") },
    { "application/x-executable", N_("Executable files") },
    { "application/x-shellscript", N_("Shell scripts") },
    { "application/json", N_("JSON files") },
    { "application/xml", N_("XML files") },
    { "text/html", N_("HTML files") },
    { "text/x-python", N_("Python scripts") },
};

static const TokenInfo token_info[] = {
    { "%F", N_("Full path(s) of selected file(s)") },
    { "%U", N_("URI(s) of selected file(s)") },
    { "%f", N_("Display name of first selected file") },
    { "%N", N_("Display name (same as %f)") },
    { "%p", N_("Display name of parent directory") },
    { "%P", N_("Full path of parent directory") },
    { "%R", N_("URI of parent directory") },
    { "%e", N_("Filename without extension") },
    { "%D", N_("Device path (mounted volumes)") },
    { "%X", N_("Window XID") },
    { "%%", N_("Literal percent sign") },
};

static GtkWidget *
create_token_legend (void)
{
    GtkWidget *frame;
    GtkWidget *grid;
    GtkWidget *label;
    guint i;
    guint num_tokens = G_N_ELEMENTS (token_info);
    guint rows = (num_tokens + 1) / 2;

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
    gtk_widget_set_hexpand (frame, TRUE);

    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), _("<b>Available Tokens</b>"));
    gtk_frame_set_label_widget (GTK_FRAME (frame), label);

    grid = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (grid), 2);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 24);
    gtk_widget_set_hexpand (grid, TRUE);
    g_object_set (grid, "margin-start", 12, "margin-top", 6, NULL);
    gtk_container_add (GTK_CONTAINER (frame), grid);

    for (i = 0; i < num_tokens; i++) {
        guint col = (i / rows) * 2;
        guint row = i % rows;
        PangoAttrList *attrs;

        label = gtk_label_new (token_info[i].token);
        gtk_label_set_xalign (GTK_LABEL (label), 0);
        attrs = pango_attr_list_new ();
        pango_attr_list_insert (attrs, pango_attr_family_new ("monospace"));
        gtk_label_set_attributes (GTK_LABEL (label), attrs);
        pango_attr_list_unref (attrs);
        gtk_grid_attach (GTK_GRID (grid), label, col, row, 1, 1);

        label = gtk_label_new (_(token_info[i].description));
        gtk_label_set_xalign (GTK_LABEL (label), 0);
        gtk_widget_set_hexpand (label, TRUE);
        gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
        gtk_grid_attach (GTK_GRID (grid), label, col + 1, row, 1, 1);
    }

    gtk_widget_show_all (frame);
    return frame;
}

static void
nemo_action_wizard_finalize (GObject *object)
{
    NemoActionWizard *self = NEMO_ACTION_WIZARD (object);

    g_free (self->editing_path);

    G_OBJECT_CLASS (nemo_action_wizard_parent_class)->finalize (object);
}

static void
nemo_action_wizard_class_init (NemoActionWizardClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nemo_action_wizard_finalize;
}

static void
nemo_action_wizard_init (NemoActionWizard *self)
{
    GtkBuilder *builder;

    self->editing_path = NULL;

    builder = gtk_builder_new_from_resource ("/org/nemo/nemo-action-wizard.glade");

    /* Get page widgets */
    self->page_intro = GTK_WIDGET (gtk_builder_get_object (builder, "page_intro"));
    self->page_basic = GTK_WIDGET (gtk_builder_get_object (builder, "page_basic"));
    self->page_command = GTK_WIDGET (gtk_builder_get_object (builder, "page_command"));
    self->page_selection = GTK_WIDGET (gtk_builder_get_object (builder, "page_selection"));
    self->page_advanced = GTK_WIDGET (gtk_builder_get_object (builder, "page_advanced"));
    self->page_summary = GTK_WIDGET (gtk_builder_get_object (builder, "page_summary"));

    /* Get entry widgets */
    self->entry_name = GTK_WIDGET (gtk_builder_get_object (builder, "entry_name"));
    self->entry_comment = GTK_WIDGET (gtk_builder_get_object (builder, "entry_comment"));
    self->icon_button_box = GTK_WIDGET (gtk_builder_get_object (builder, "icon_button_box"));

    /* Create and pack the XAppIconChooserButton */
    self->icon_button = XAPP_ICON_CHOOSER_BUTTON (xapp_icon_chooser_button_new ());
    xapp_icon_chooser_button_set_icon_size (self->icon_button, GTK_ICON_SIZE_DND);
    gtk_box_pack_start (GTK_BOX (self->icon_button_box), GTK_WIDGET (self->icon_button), FALSE, FALSE, 0);
    gtk_widget_show (GTK_WIDGET (self->icon_button));

    self->entry_exec = GTK_WIDGET (gtk_builder_get_object (builder, "entry_exec"));
    self->button_app_chooser = GTK_WIDGET (gtk_builder_get_object (builder, "button_app_chooser"));
    self->combo_quote = GTK_WIDGET (gtk_builder_get_object (builder, "combo_quote"));
    self->entry_separator = GTK_WIDGET (gtk_builder_get_object (builder, "entry_separator"));
    self->check_terminal = GTK_WIDGET (gtk_builder_get_object (builder, "check_terminal"));

    self->combo_selection = GTK_WIDGET (gtk_builder_get_object (builder, "combo_selection"));
    self->specific_count_box = GTK_WIDGET (gtk_builder_get_object (builder, "specific_count_box"));
    self->spin_selection_count = GTK_WIDGET (gtk_builder_get_object (builder, "spin_selection_count"));
    self->check_extensions = GTK_WIDGET (gtk_builder_get_object (builder, "check_extensions"));
    self->extensions_content_box = GTK_WIDGET (gtk_builder_get_object (builder, "extensions_content_box"));
    self->combo_extensions_preset = GTK_WIDGET (gtk_builder_get_object (builder, "combo_extensions_preset"));
    self->entry_extensions = GTK_WIDGET (gtk_builder_get_object (builder, "entry_extensions"));
    self->check_mimetypes = GTK_WIDGET (gtk_builder_get_object (builder, "check_mimetypes"));
    self->mimetypes_content_box = GTK_WIDGET (gtk_builder_get_object (builder, "mimetypes_content_box"));
    self->treeview_mimetypes = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_mimetypes"));
    self->entry_mimetypes = GTK_WIDGET (gtk_builder_get_object (builder, "entry_mimetypes"));

    /* Set up mimetypes treeview */
    self->mimetypes_store = gtk_list_store_new (MIME_N_COLS,
                                                 G_TYPE_BOOLEAN,
                                                 G_TYPE_STRING,
                                                 G_TYPE_STRING);

    for (guint i = 0; i < G_N_ELEMENTS (common_mimetypes); i++) {
        GtkTreeIter iter;
        gtk_list_store_append (self->mimetypes_store, &iter);
        gtk_list_store_set (self->mimetypes_store, &iter,
                            MIME_COL_ENABLED, FALSE,
                            MIME_COL_MIMETYPE, common_mimetypes[i].mimetype,
                            MIME_COL_DESCRIPTION, _(common_mimetypes[i].description),
                            -1);
    }

    gtk_tree_view_set_model (GTK_TREE_VIEW (self->treeview_mimetypes),
                             GTK_TREE_MODEL (self->mimetypes_store));

    GtkCellRenderer *toggle_renderer = gtk_cell_renderer_toggle_new ();
    g_signal_connect (toggle_renderer, "toggled", G_CALLBACK (on_mimetype_toggled), self);
    GtkTreeViewColumn *toggle_col = gtk_tree_view_column_new_with_attributes (
        "", toggle_renderer, "active", MIME_COL_ENABLED, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (self->treeview_mimetypes), toggle_col);

    GtkCellRenderer *text_renderer = gtk_cell_renderer_text_new ();
    GtkTreeViewColumn *desc_col = gtk_tree_view_column_new_with_attributes (
        "", text_renderer, "text", MIME_COL_DESCRIPTION, NULL);
    gtk_tree_view_column_set_expand (desc_col, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (self->treeview_mimetypes), desc_col);

    GtkCellRenderer *mime_renderer = gtk_cell_renderer_text_new ();
    g_object_set (mime_renderer, "style", PANGO_STYLE_ITALIC, "foreground", "gray", NULL);
    GtkTreeViewColumn *mime_col = gtk_tree_view_column_new_with_attributes (
        "", mime_renderer, "text", MIME_COL_MIMETYPE, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (self->treeview_mimetypes), mime_col);

    self->entry_dependencies = GTK_WIDGET (gtk_builder_get_object (builder, "entry_dependencies"));
    self->entry_uri_scheme = GTK_WIDGET (gtk_builder_get_object (builder, "entry_uri_scheme"));
    self->entry_conditions = GTK_WIDGET (gtk_builder_get_object (builder, "entry_conditions"));
    self->entry_files = GTK_WIDGET (gtk_builder_get_object (builder, "entry_files"));
    self->entry_locations = GTK_WIDGET (gtk_builder_get_object (builder, "entry_locations"));

    self->entry_filename = GTK_WIDGET (gtk_builder_get_object (builder, "entry_filename"));
    self->label_save_location = GTK_WIDGET (gtk_builder_get_object (builder, "label_save_location"));
    self->textview_preview = GTK_WIDGET (gtk_builder_get_object (builder, "textview_preview"));

    self->token_legend_basic = GTK_WIDGET (gtk_builder_get_object (builder, "token_legend_basic"));
    self->token_legend_command = GTK_WIDGET (gtk_builder_get_object (builder, "token_legend_command"));

    self->sample_label_basic = GTK_WIDGET (gtk_builder_get_object (builder, "sample_label_basic"));
    self->sample_tooltip_basic = GTK_WIDGET (gtk_builder_get_object (builder, "sample_tooltip_basic"));
    self->sample_exec_command = GTK_WIDGET (gtk_builder_get_object (builder, "sample_exec_command"));

    /* Create and pack token legends */
    gtk_box_pack_start (GTK_BOX (self->token_legend_basic), create_token_legend (), TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (self->token_legend_command), create_token_legend (), TRUE, TRUE, 0);

    /* Add pages to assistant */
    gtk_assistant_append_page (GTK_ASSISTANT (self), self->page_intro);
    gtk_assistant_set_page_type (GTK_ASSISTANT (self), self->page_intro, GTK_ASSISTANT_PAGE_INTRO);
    gtk_assistant_set_page_title (GTK_ASSISTANT (self), self->page_intro, _("Introduction"));
    gtk_assistant_set_page_complete (GTK_ASSISTANT (self), self->page_intro, TRUE);

    gtk_assistant_append_page (GTK_ASSISTANT (self), self->page_basic);
    gtk_assistant_set_page_type (GTK_ASSISTANT (self), self->page_basic, GTK_ASSISTANT_PAGE_CONTENT);
    gtk_assistant_set_page_title (GTK_ASSISTANT (self), self->page_basic, _("Basic Information"));

    gtk_assistant_append_page (GTK_ASSISTANT (self), self->page_command);
    gtk_assistant_set_page_type (GTK_ASSISTANT (self), self->page_command, GTK_ASSISTANT_PAGE_CONTENT);
    gtk_assistant_set_page_title (GTK_ASSISTANT (self), self->page_command, _("Command"));

    gtk_assistant_append_page (GTK_ASSISTANT (self), self->page_selection);
    gtk_assistant_set_page_type (GTK_ASSISTANT (self), self->page_selection, GTK_ASSISTANT_PAGE_CONTENT);
    gtk_assistant_set_page_title (GTK_ASSISTANT (self), self->page_selection, _("Selection & File Matching"));

    gtk_assistant_append_page (GTK_ASSISTANT (self), self->page_advanced);
    gtk_assistant_set_page_type (GTK_ASSISTANT (self), self->page_advanced, GTK_ASSISTANT_PAGE_CONTENT);
    gtk_assistant_set_page_title (GTK_ASSISTANT (self), self->page_advanced, _("Advanced Options"));
    gtk_assistant_set_page_complete (GTK_ASSISTANT (self), self->page_advanced, TRUE);

    gtk_assistant_append_page (GTK_ASSISTANT (self), self->page_summary);
    gtk_assistant_set_page_type (GTK_ASSISTANT (self), self->page_summary, GTK_ASSISTANT_PAGE_CONFIRM);
    gtk_assistant_set_page_title (GTK_ASSISTANT (self), self->page_summary, _("Summary & Save"));
    gtk_assistant_set_page_complete (GTK_ASSISTANT (self), self->page_summary, TRUE);

    /* Connect signals for validation */
    g_signal_connect (self->entry_name, "changed", G_CALLBACK (on_entry_name_changed), self);
    g_signal_connect (self->entry_comment, "changed", G_CALLBACK (on_entry_comment_changed), self);
    g_signal_connect (self->entry_exec, "changed", G_CALLBACK (on_entry_exec_changed), self);
    g_signal_connect (self->button_app_chooser, "clicked", G_CALLBACK (on_app_chooser_clicked), self);
    g_signal_connect (self->combo_quote, "changed", G_CALLBACK (on_combo_quote_changed), self);
    g_signal_connect (self->entry_separator, "changed", G_CALLBACK (on_entry_separator_changed), self);
    g_signal_connect (self->combo_selection, "changed", G_CALLBACK (on_selection_type_changed), self);
    g_signal_connect (self->check_extensions, "toggled", G_CALLBACK (on_check_extensions_toggled), self);
    g_signal_connect (self->check_mimetypes, "toggled", G_CALLBACK (on_check_mimetypes_toggled), self);
    g_signal_connect (self->combo_extensions_preset, "changed", G_CALLBACK (on_extensions_preset_changed), self);
    g_signal_connect (self->entry_extensions, "changed", G_CALLBACK (on_extensions_entry_changed), self);
    g_signal_connect (self->entry_mimetypes, "changed", G_CALLBACK (on_mimetypes_entry_changed), self);

    /* Connect assistant signals */
    g_signal_connect (self, "prepare", G_CALLBACK (on_prepare), self);
    g_signal_connect (self, "apply", G_CALLBACK (on_apply), self);
    g_signal_connect (self, "cancel", G_CALLBACK (on_cancel), self);
    g_signal_connect (self, "close", G_CALLBACK (on_close), self);

    /* Set initial validation state */
    update_basic_page_complete (self);
    update_command_page_complete (self);
    update_selection_page_complete (self);

    /* Set save location label */
    g_autofree gchar *user_dir = nemo_action_manager_get_user_directory_path ();
    gtk_label_set_text (GTK_LABEL (self->label_save_location), user_dir);

    g_object_unref (builder);
}

GtkWidget *
nemo_action_wizard_new (GtkWindow *parent)
{
    GtkWidget *wizard = g_object_new (NEMO_TYPE_ACTION_WIZARD,
                                      "use-header-bar", TRUE,
                                      NULL);

    if (parent != NULL) {
        gtk_window_set_transient_for (GTK_WINDOW (wizard), parent);
        gtk_window_set_modal (GTK_WINDOW (wizard), TRUE);
    }

    gtk_window_set_title (GTK_WINDOW (wizard), _("Create New Action"));
    gtk_window_set_default_size (GTK_WINDOW (wizard), 650, 500);

    return wizard;
}

GtkWidget *
nemo_action_wizard_new_for_file (GtkWindow   *parent,
                                 const gchar *action_path)
{
    GtkWidget *wizard = nemo_action_wizard_new (parent);
    NemoActionWizard *self = NEMO_ACTION_WIZARD (wizard);

    self->editing_path = g_strdup (action_path);
    gtk_window_set_title (GTK_WINDOW (wizard), _("Edit Action"));

    /* TODO: Load existing action file and populate fields */

    return wizard;
}

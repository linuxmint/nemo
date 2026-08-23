/* nemo-action-layout-editor.c */

#include <config.h>
#include "nemo-action-layout-editor.h"
#include <json-glib/json-glib.h>
#include <libxapp/xapp-icon-chooser-dialog.h>
#include <libxapp/xapp-visibility-group.h>
#include <glib/gi18n.h>
#include <string.h>
#include <pango/pango.h>

#define DEBUG_EDITOR 0

#if DEBUG_EDITOR
#define DEBUG(format, ...) g_print("ActionLayoutEditor: " format "\n", ##__VA_ARGS__)
#else
#define DEBUG(format, ...) G_STMT_START { } G_STMT_END
#endif

#define JSON_FILE "nemo/actions-tree.json"
#define USER_ACTIONS_DIR "nemo/actions"

// From nemo-action-symbols.h
#define ACTION_FILE_GROUP "Nemo Action"
#define KEY_NAME "Name"
#define KEY_ICON_NAME "Icon-Name"
#define NEMO_PLUGIN_PREFERENCES_DISABLED_ACTIONS "disabled-actions"

enum {
    COL_HASH,
    COL_UUID,
    COL_TYPE,
    COL_ROW_DATA,
    N_COLUMNS
};

typedef enum {
    ROW_TYPE_ACTION,
    ROW_TYPE_SUBMENU,
    ROW_TYPE_SEPARATOR
} RowType;

typedef struct {
    gchar *label;
    guint key;
    GdkModifierType mods;
} BuiltinShortcut;

typedef struct {
    gchar *uuid;
    RowType type;
    gchar *user_label;
    gchar *user_icon;
    gchar *accelerator;
    gchar *filename; // only used for NemoActions
    GKeyFile *keyfile;  // same
    gboolean enabled;
} RowData;

static const gchar *
row_type_to_string (RowType type)
{
    switch (type) {
        case ROW_TYPE_ACTION:
            return "action";
        case ROW_TYPE_SUBMENU:
            return "submenu";
        case ROW_TYPE_SEPARATOR:
            return "separator";
        default:
            return "action";
    }
}

static RowType
row_type_from_string (const gchar *str)
{
    if (g_strcmp0 (str, "submenu") == 0)
        return ROW_TYPE_SUBMENU;
    else if (g_strcmp0 (str, "separator") == 0)
        return ROW_TYPE_SEPARATOR;
    else
        return ROW_TYPE_ACTION;
}

static void row_data_free (RowData *data);

static RowData *
row_data_copy (RowData *data)
{
    RowData *copy;

    if (data == NULL)
        return NULL;

    copy = g_new0 (RowData, 1);
    copy->uuid = g_strdup (data->uuid);
    copy->type = data->type;
    copy->user_label = g_strdup (data->user_label);
    copy->user_icon = g_strdup (data->user_icon);
    copy->accelerator = g_strdup (data->accelerator);
    copy->filename = g_strdup (data->filename);
    copy->enabled = data->enabled;

    if (data->keyfile)
        copy->keyfile = g_key_file_ref (data->keyfile);

    return copy;
}

static GType
row_data_get_type (void)
{
    static GType type = 0;

    if (G_UNLIKELY (type == 0)) {
        type = g_boxed_type_register_static ("NemoActionLayoutEditorRowData",
                                             (GBoxedCopyFunc) row_data_copy,
                                             (GBoxedFreeFunc) row_data_free);
    }

    return type;
}

#define ROW_DATA_TYPE (row_data_get_type ())

typedef struct {
    /* UI widgets */
    GtkWidget *treeview;
    GtkTreeStore *model;
    GtkWidget *save_button;
    GtkWidget *discard_button;
    GtkWidget *default_layout_button;
    GtkWidget *name_entry;
    GtkWidget *new_row_button;
    GtkWidget *row_controls_box;
    GtkWidget *remove_submenu_button;
    GtkWidget *icon_selector_menu_button;
    GtkWidget *icon_selector_image;
    GtkWidget *original_icon_menu_image;
    GtkWidget *original_icon_menu_item;
    GtkWidget *up_button;
    GtkWidget *down_button;
    GtkWidget *scrolled_window;

    XAppVisibilityGroup *selected_item_widgets_group;

    /* Settings and monitors */
    GSettings *nemo_plugin_settings;
    GList *dir_monitors;
    gulong settings_handler_id;

    GList *builtin_shortcuts;

    /* State */
    gboolean needs_saved;
    gboolean updating_model;
    gboolean updating_row_edit_fields;
    gboolean editing_accel;

    /* DND */
    guint dnd_autoscroll_timeout_id;
} NemoActionLayoutEditorPrivate;

struct _NemoActionLayoutEditor
{
    GtkBox parent_instance;
    NemoActionLayoutEditorPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (NemoActionLayoutEditor, nemo_action_layout_editor, GTK_TYPE_BOX)

static void reload_model (NemoActionLayoutEditor *self, gboolean flat);
static void save_model (NemoActionLayoutEditor *self);
static void set_needs_saved (NemoActionLayoutEditor *self, gboolean needs_saved);
static void update_row_controls (NemoActionLayoutEditor *self);
static void update_arrow_button_states (NemoActionLayoutEditor *self);
static void selected_row_changed (NemoActionLayoutEditor *self, gboolean needs_saved);
static gboolean lookup_iter_by_hash (GtkTreeModel *model, const gchar *hash, GtkTreeIter *result);
static void remove_row_by_hash (NemoActionLayoutEditor *self, const gchar *hash);
static void select_row_by_hash (NemoActionLayoutEditor *self, const gchar *hash);
static void move_tree_recursive (NemoActionLayoutEditor *self, GtkTreeIter *source_iter, GtkTreeIter *new_parent);

static RowData *
row_data_new (void)
{
    RowData *data = g_new0 (RowData, 1);
    data->enabled = TRUE;
    return data;
}

static void
row_data_free (RowData *data)
{
    if (data == NULL)
        return;

    g_free (data->uuid);
    g_free (data->user_label);
    g_free (data->user_icon);
    g_free (data->accelerator);
    g_free (data->filename);

    if (data->keyfile)
        g_key_file_unref (data->keyfile);

    g_free (data);
}

static void
tree_store_append_row_data (GtkTreeStore *model,
                            GtkTreeIter  *iter,
                            GtkTreeIter  *parent,
                            RowData      *data)
{
    g_autofree gchar *hash = g_uuid_string_random ();

    gtk_tree_store_append (model, iter, parent);
    gtk_tree_store_set (model, iter,
                       COL_HASH, hash,
                       COL_UUID, data->uuid,
                       COL_TYPE, data->type,
                       COL_ROW_DATA, data,
                       -1);
    row_data_free (data);
}

static void
tree_store_update_row_data (GtkTreeStore *model,
                            GtkTreeIter  *iter,
                            RowData      *data)
{
    gtk_tree_store_set (model, iter,
                        COL_ROW_DATA, data,
                        -1);
    row_data_free (data);
}

static BuiltinShortcut *
builtin_shortcut_new (const gchar *label,
                      const gchar *accelerator)
{
    BuiltinShortcut *shortcut = g_new0 (BuiltinShortcut, 1);
    shortcut->label = g_strdup (label);

    gtk_accelerator_parse (accelerator, &shortcut->key, &shortcut->mods);
    return shortcut;
}

static void
builtin_shortcut_free (BuiltinShortcut *shortcut)
{
    if (shortcut == NULL)
        return;

    g_free (shortcut->label);
    g_free (shortcut);
}

static gchar *
row_data_get_icon_string (RowData *data, gboolean original)
{
    if (!original && data->user_icon != NULL)
        return g_strdup (data->user_icon);

    if (data->keyfile != NULL) {
        return g_key_file_get_string (data->keyfile,
                                      ACTION_FILE_GROUP,
                                      KEY_ICON_NAME,
                                      NULL);
    }

    return NULL;
}

static gchar *
row_data_get_label (RowData *data)
{
    if (data->type == ROW_TYPE_SEPARATOR)
        return g_strdup ("──────────────────────────────");

    if (data->user_label != NULL)
        return g_strdup (data->user_label);

    if (data->keyfile != NULL) {
        gchar *label = g_key_file_get_locale_string (data->keyfile,
                                                      ACTION_FILE_GROUP,
                                                      KEY_NAME,
                                                      NULL,
                                                      NULL);
        if (label) {
            gchar *result = g_strdup (label);
            g_free (label);
            /* Remove underscores */
            gchar *p = result;
            while (*p) {
                if (*p == '_')
                    *p = ' ';
                p++;
            }
            return result;
        }
    }

    return g_strdup (_("Unknown"));
}

static gboolean
get_selected_row (NemoActionLayoutEditor *self, GtkTreePath **path_out, GtkTreeIter *iter_out)
{
    GtkTreeSelection *selection;
    GList *paths;
    gboolean result = FALSE;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->treeview));
    paths = gtk_tree_selection_get_selected_rows (selection, NULL);

    if (paths) {
        GtkTreePath *path = paths->data;
        if (gtk_tree_model_get_iter (GTK_TREE_MODEL (self->priv->model), iter_out, path)) {
            if (path_out)
                *path_out = gtk_tree_path_copy (path);
            result = TRUE;
        }
        g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);
    }

    return result;
}

static RowData *
get_selected_row_data (NemoActionLayoutEditor *self, GtkTreeIter *iter_out)
{
    GtkTreeIter iter;
    RowData *data = NULL;

    if (get_selected_row (self, NULL, &iter)) {
        gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &iter,
                           COL_ROW_DATA, &data,
                           -1);
    }

    if (iter_out)
        *iter_out = iter;

    return data;
}

static void
save_disabled_list (NemoActionLayoutEditor *self)
{
    GStrvBuilder *builder = g_strv_builder_new ();
    GtkTreeIter iter;
    gboolean valid;
    gchar **disabled_actions;

    valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->priv->model), &iter);

    while (valid) {
        RowType row_type;
        RowData *data;

        gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &iter,
                           COL_TYPE, &row_type,
                           COL_ROW_DATA, &data,
                           -1);

        if (row_type == ROW_TYPE_ACTION) {
            if (!data->enabled && data->filename) {
                gchar *basename = g_path_get_basename (data->filename);
                g_strv_builder_add (builder, basename);
                g_free (basename);
            }
        }

        if (data != NULL) {
            row_data_free (data);
        }

        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->priv->model), &iter);
    }

    disabled_actions = g_strv_builder_end (builder);

    g_signal_handler_block (self->priv->nemo_plugin_settings, self->priv->settings_handler_id);
    g_settings_set_strv (self->priv->nemo_plugin_settings,
                        NEMO_PLUGIN_PREFERENCES_DISABLED_ACTIONS,
                        (const gchar * const *) disabled_actions);
    g_signal_handler_unblock (self->priv->nemo_plugin_settings, self->priv->settings_handler_id);

    g_strfreev (disabled_actions);
    g_strv_builder_unref (builder);
}

static gboolean
update_row_disabled_state (GtkTreeModel *model,
                           GtkTreePath *path,
                           GtkTreeIter *iter,
                           gpointer user_data)
{
    gchar **disabled_actions = (gchar **) user_data;
    RowType row_type;
    RowData *data;
    gboolean old_enabled, new_enabled;

    gtk_tree_model_get (model, iter,
                       COL_TYPE, &row_type,
                       -1);

    if (row_type != ROW_TYPE_ACTION || !data)
        return FALSE;

    gtk_tree_model_get (model, iter,
                       COL_ROW_DATA, &data,
                       -1);

    if (data == NULL) {
        return FALSE;
    }

    old_enabled = data->enabled;

    new_enabled = TRUE;
    if (disabled_actions) {
        for (gint i = 0; disabled_actions[i] != NULL; i++) {
            if (g_strcmp0 (data->uuid, disabled_actions[i]) == 0) {
                new_enabled = FALSE;
                break;
            }
        }
    }

    if (old_enabled != new_enabled) {
        data->enabled = new_enabled;
        tree_store_update_row_data (GTK_TREE_STORE (model), iter, data);
        gtk_tree_model_row_changed (model, path, iter);
    } else {
        row_data_free (data);

    }

    return FALSE;
}

static void
on_disabled_settings_list_changed (GSettings *settings,
                                   const gchar *key,
                                   NemoActionLayoutEditor *self)
{
    gchar **disabled_actions;

    if (g_strcmp0 (key, NEMO_PLUGIN_PREFERENCES_DISABLED_ACTIONS) != 0)
        return;

    disabled_actions = g_settings_get_strv (self->priv->nemo_plugin_settings,
                                           NEMO_PLUGIN_PREFERENCES_DISABLED_ACTIONS);

    gtk_tree_model_foreach (GTK_TREE_MODEL (self->priv->model),
                           update_row_disabled_state,
                           disabled_actions);

    g_strfreev (disabled_actions);
}

static void
on_save_clicked (GtkButton *button, NemoActionLayoutEditor *self)
{
    save_model (self);
    save_disabled_list (self);
    set_needs_saved (self, FALSE);
}

static void
on_discard_changes_clicked (GtkButton *button, NemoActionLayoutEditor *self)
{
    reload_model (self, FALSE);
    set_needs_saved (self, FALSE);
}

static void
on_default_layout_clicked (GtkButton *button, NemoActionLayoutEditor *self)
{
    reload_model (self, TRUE);
    set_needs_saved (self, TRUE);
}

static void
insert_new_row_at_selection (NemoActionLayoutEditor *self,
                             RowType type,
                             const gchar *default_label,
                             gboolean grab_name_focus)
{
    GtkTreeIter iter, new_iter, parent_iter;
    GtkTreePath *path;
    RowType row_type;
    RowData *data;

    if (!get_selected_row (self, &path, &iter))
        return;

    gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &iter,
                       COL_TYPE, &row_type,
                       -1);

    if (row_type == ROW_TYPE_ACTION) {
        if (gtk_tree_model_iter_parent (GTK_TREE_MODEL (self->priv->model), &parent_iter, &iter)) {
            gtk_tree_store_insert_after (self->priv->model, &new_iter, &parent_iter, &iter);
        } else {
            gtk_tree_store_insert_after (self->priv->model, &new_iter, NULL, &iter);
        }
    } else {
        gtk_tree_store_insert_after (self->priv->model, &new_iter, &iter, NULL);
    }

    data = row_data_new ();
    data->uuid = g_strdup (default_label);
    data->type = type;
    if (type == ROW_TYPE_SUBMENU) {
        data->user_label = g_strdup (default_label);
    }

    g_autofree gchar *hash = g_uuid_string_random ();
    gtk_tree_store_set (self->priv->model, &new_iter,
                       COL_HASH, hash,
                       COL_UUID, data->uuid,
                       COL_TYPE, type,
                       COL_ROW_DATA, data,
                       -1);

    row_data_free (data);
    gtk_tree_selection_select_iter (gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->treeview)),
                                    &new_iter);

    if (grab_name_focus) {
        gtk_widget_grab_focus (self->priv->name_entry);
    }

    gtk_tree_path_free (path);
    selected_row_changed (self, TRUE);
}

static void
on_new_submenu_clicked (GtkMenuItem *item, NemoActionLayoutEditor *self)
{
    insert_new_row_at_selection (self, ROW_TYPE_SUBMENU, _("New submenu"), TRUE);
}

static void
on_new_separator_clicked (GtkMenuItem *item, NemoActionLayoutEditor *self)
{
    insert_new_row_at_selection (self, ROW_TYPE_SEPARATOR, "separator", FALSE);
}

static void
on_clear_icon_clicked (GtkMenuItem *item, NemoActionLayoutEditor *self)
{
    GtkTreeIter iter;

    RowData *data = get_selected_row_data (self, &iter);
    if (data) {
        g_free (data->user_icon);
        data->user_icon = g_strdup ("");
        tree_store_update_row_data (self->priv->model, &iter, data);
        selected_row_changed (self, TRUE);
    }
}

static void
on_original_icon_clicked (GtkMenuItem *item, NemoActionLayoutEditor *self)
{
    GtkTreeIter iter;

    RowData *data = get_selected_row_data (self, &iter);
    if (data) {
        g_free (data->user_icon);
        data->user_icon = NULL;
        tree_store_update_row_data (self->priv->model, &iter, data);
        selected_row_changed (self, TRUE);
    }
}

static void
on_choose_icon_clicked (GtkMenuItem *item, NemoActionLayoutEditor *self)
{
    XAppIconChooserDialog *chooser;
    GtkTreeIter iter;
    RowData *data;
    gchar *icon_string;
    gint response;

    data = get_selected_row_data (self, &iter);
    if (!data)
        return;

    chooser = xapp_icon_chooser_dialog_new ();

    icon_string = row_data_get_icon_string (data, FALSE);
    if (icon_string != NULL && icon_string[0] != '\0') {
        response = xapp_icon_chooser_dialog_run_with_icon (chooser, icon_string);
    } else {
        response = xapp_icon_chooser_dialog_run (chooser);
    }
    g_free (icon_string);

    if (response == GTK_RESPONSE_OK) {
        g_free (data->user_icon);
        data->user_icon = xapp_icon_chooser_dialog_get_icon_string (chooser);
        tree_store_update_row_data (self->priv->model, &iter, data);
        selected_row_changed (self, TRUE);
    }

    gtk_widget_hide (GTK_WIDGET (chooser));
    gtk_widget_destroy (GTK_WIDGET (chooser));
}

static void
on_remove_submenu_clicked (GtkButton *button, NemoActionLayoutEditor *self)
{
    GtkTreeIter iter;
    RowType row_type;

    if (!get_selected_row (self, NULL, &iter))
        return;

    gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &iter,
                       COL_TYPE, &row_type,
                       -1);

    if (row_type == ROW_TYPE_ACTION) {
        return;
    }

    /* For submenus, move children to parent level before removing */
    if (row_type == ROW_TYPE_SUBMENU) {
        GtkTreeIter parent_iter, child_iter;
        gboolean has_parent = gtk_tree_model_iter_parent (GTK_TREE_MODEL (self->priv->model),
                                                          &parent_iter, &iter);

        /* Move all children up one level */
        while (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (self->priv->model), &iter) > 0) {
            GtkTreeIter new_iter;
            RowData *child_data;
            gchar *child_uuid, *child_type, *child_hash;

            gtk_tree_model_iter_children (GTK_TREE_MODEL (self->priv->model), &child_iter, &iter);

            gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &child_iter,
                               COL_HASH, &child_hash,
                               COL_UUID, &child_uuid,
                               COL_TYPE, &child_type,
                               COL_ROW_DATA, &child_data,
                               -1);

            if (has_parent) {
                gtk_tree_store_insert_before (self->priv->model, &new_iter, &parent_iter, &iter);
            } else {
                gtk_tree_store_insert_before (self->priv->model, &new_iter, NULL, &iter);
            }

            gtk_tree_store_set (self->priv->model, &new_iter,
                               COL_HASH, child_hash,
                               COL_UUID, child_uuid,
                               COL_TYPE, child_type,
                               COL_ROW_DATA, child_data,
                               -1);

            row_data_free (child_data);

            gtk_tree_store_remove (self->priv->model, &child_iter);

            g_free (child_hash);
            g_free (child_uuid);
        }
    }

    gtk_tree_store_remove (self->priv->model, &iter);

    selected_row_changed (self, TRUE);
}

// DND

static void
dnd_autoscroll_cancel (NemoActionLayoutEditor *self)
{
    if (self->priv->dnd_autoscroll_timeout_id > 0) {
        g_source_remove (self->priv->dnd_autoscroll_timeout_id);
        self->priv->dnd_autoscroll_timeout_id = 0;
    }
}

static gboolean
dnd_autoscroll_timeout (gpointer user_data)
{
    NemoActionLayoutEditor *self = NEMO_ACTION_LAYOUT_EDITOR (user_data);
    self->priv->dnd_autoscroll_timeout_id = 0;
    return G_SOURCE_REMOVE;
}

static void
dnd_autoscroll_start (NemoActionLayoutEditor *self)
{
    if (self->priv->dnd_autoscroll_timeout_id > 0)
        g_source_remove (self->priv->dnd_autoscroll_timeout_id);
    self->priv->dnd_autoscroll_timeout_id = g_timeout_add (50, dnd_autoscroll_timeout, self);
}

static void
gather_row_surfaces (NemoActionLayoutEditor *self,
                     GtkTreeIter *iter,
                     GList **surfaces,
                     gint *width,
                     gint *height)
{
    GtkTreeIter child_iter;
    RowType row_type;
    gint child_count = 0;

    if (!gtk_tree_model_iter_children (GTK_TREE_MODEL (self->priv->model), &child_iter, iter)) {
        return;
    }

    do {
        GtkTreePath *child_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->priv->model), &child_iter);

        cairo_surface_t *surface = gtk_tree_view_create_row_drag_icon (GTK_TREE_VIEW (self->priv->treeview),
                                                                        child_path);

        cairo_t *cr_measure = cairo_create (surface);
        double x1, y1, x2, y2;
        cairo_clip_extents (cr_measure, &x1, &y1, &x2, &y2);
        cairo_destroy (cr_measure);

        gint surf_width = (gint)(x2 - x1);
        gint surf_height = (gint)(y2 - y1);

        *surfaces = g_list_prepend (*surfaces, surface);
        child_count++;

        *width = MAX (*width, surf_width);
        *height += surf_height - 1;

        gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &child_iter,
                           COL_TYPE, &row_type, -1);

        if (row_type == ROW_TYPE_SUBMENU) {
            gather_row_surfaces (self, &child_iter, surfaces, width, height);
        }

        gtk_tree_path_free (child_path);
    } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->priv->model), &child_iter));
}

static void
on_drag_begin (GtkWidget *widget, GdkDragContext *context, NemoActionLayoutEditor *self)
{
    GtkTreePath *path;
    GtkTreeIter iter;
    cairo_surface_t *source_surface, *final_surface;
    GList *surfaces = NULL;
    gint width, height;
    gint scale;
    cairo_t *cr;
    gdouble y;
    gboolean first;

    if (!get_selected_row (self, &path, &iter))
        return;

    source_surface = gtk_tree_view_create_row_drag_icon (GTK_TREE_VIEW (self->priv->treeview), path);
    cairo_t *cr_measure = cairo_create (source_surface);
    double x1, y1, x2, y2;
    cairo_clip_extents (cr_measure, &x1, &y1, &x2, &y2);
    cairo_destroy (cr_measure);

    width = (gint)(x2 - x1);
    height = (gint)(y2 - y1) - 1;

    surfaces = g_list_prepend (surfaces, source_surface);
    gather_row_surfaces (self, &iter, &surfaces, &width, &height);
    surfaces = g_list_reverse (surfaces);

    scale = gtk_widget_get_scale_factor (widget);
    final_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width + 4, height + 4);
    cairo_surface_set_device_scale (final_surface, scale, scale);

    cr = cairo_create (final_surface);

    y = 2.0;
    first = TRUE;

    for (GList *l = surfaces; l != NULL; l = l->next) {
        cairo_surface_t *s = (cairo_surface_t *) l->data;

        cairo_t *cr_measure = cairo_create (s);
        double sx1, sy1, sx2, sy2;
        cairo_clip_extents (cr_measure, &sx1, &sy1, &sx2, &sy2);
        cairo_destroy (cr_measure);
        gint surf_height = (gint)(sy2 - sy1);

        cairo_save (cr);
        cairo_set_source_surface (cr, s, 2.0, y);
        cairo_paint (cr);
        cairo_restore (cr);

        if (!first) {
            cairo_save (cr);
            cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
            cairo_rectangle (cr, 1.0 * scale, (y - 2.0) * scale,
                           (width / (gdouble)scale) - 2.0, 4.0);
            cairo_fill (cr);
            cairo_restore (cr);
        }
        first = FALSE;

        y += surf_height / (gdouble)scale - 1.0;
        cairo_surface_destroy (s);
    }

    cairo_show_page (cr);
    cairo_destroy (cr);

    gtk_drag_set_icon_surface (context, final_surface);
    cairo_surface_destroy (final_surface);

    g_list_free (surfaces);
    gtk_tree_path_free (path);
}

static void
on_drag_end (GtkWidget *widget, GdkDragContext *context, NemoActionLayoutEditor *self)
{
    dnd_autoscroll_cancel (self);
}

static gboolean
on_drag_motion (GtkWidget *widget,
                GdkDragContext *context,
                gint x,
                gint y,
                guint time,
                NemoActionLayoutEditor *self)
{
    GtkTreePath *path = NULL;
    GtkTreeViewDropPosition pos;
    GtkTreeIter iter, source_iter;
    GtkTreePath *source_path;
    RowType target_type;

    if (!gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget), x, y, &path, &pos)) {
        gdk_drag_status (context, 0, time);
        return FALSE;
    }

    if (!get_selected_row (self, &source_path, &source_iter)) {
        gtk_tree_path_free (path);
        return FALSE;
    }

    // Don't allow dropping on itself
    if (gtk_tree_path_compare (source_path, path) == 0) {
        gtk_tree_path_free (path);
        gtk_tree_path_free (source_path);
        gdk_drag_status (context, 0, time);
        return FALSE;
    }

    // Don't allow dropping into own hierarchy
    if (gtk_tree_path_is_ancestor (source_path, path) ||
        gtk_tree_path_is_descendant (source_path, path)) {
        gtk_tree_path_free (path);
        gtk_tree_path_free (source_path);
        gdk_drag_status (context, 0, time);
        return FALSE;
    }

    gtk_tree_model_get_iter (GTK_TREE_MODEL (self->priv->model), &iter, path);
    gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &iter,
                       COL_TYPE, &target_type, -1);

    // Don't allow INTO on actions/separators - only on submenus
    if (target_type == ROW_TYPE_ACTION ||
        target_type == ROW_TYPE_SEPARATOR) {
        if (pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE ||
            pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER) {
            gtk_tree_path_free (path);
            gtk_tree_path_free (source_path);
            gdk_drag_status (context, 0, time);
            return FALSE;
        }
    }

    gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (widget), path, pos);
    gdk_drag_status (context, GDK_ACTION_MOVE, time);

    dnd_autoscroll_start (self);

    gtk_tree_path_free (path);
    gtk_tree_path_free (source_path);
    return TRUE;
}

static void
on_drag_data_get (GtkWidget *widget,
                  GdkDragContext *context,
                  GtkSelectionData *selection_data,
                  guint info,
                  guint time,
                  NemoActionLayoutEditor *self)
{
    GtkTreeIter iter;
    gchar *hash, *uuid;
    RowType type;
    gchar *data_str;

    if (!get_selected_row (self, NULL, &iter))
        return;

    gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &iter,
                       COL_HASH, &hash,
                       COL_UUID, &uuid,
                       COL_TYPE, &type,
                       -1);

    data_str = g_strdup_printf ("%s::%s::%s", hash, uuid, row_type_to_string (type));

    gtk_selection_data_set_text (selection_data, data_str, -1);

    g_free (data_str);
    g_free (hash);
    g_free (uuid);
}

static void
on_drag_data_received (GtkWidget *widget,
                       GdkDragContext *context,
                       gint x,
                       gint y,
                       GtkSelectionData *selection_data,
                       guint info,
                       guint time,
                       NemoActionLayoutEditor *self)
{
    GtkTreePath *path = NULL;
    GtkTreeViewDropPosition pos;
    GtkTreeIter target_iter, source_iter, new_iter, parent_iter;
    const guchar *data;
    gchar **parts;
    const gchar *source_hash, *source_uuid;
    RowType source_type, target_type;
    RowData *row_data;
    gchar *new_hash;
    gboolean has_parent;

    if (!gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget), x, y, &path, &pos)) {
        gtk_drag_finish (context, FALSE, FALSE, time);
        return;
    }

    data = gtk_selection_data_get_text (selection_data);
    if (!data) {
        gtk_tree_path_free (path);
        gtk_drag_finish (context, FALSE, FALSE, time);
        return;
    }

    parts = g_strsplit ((const gchar *) data, "::", 3);
    source_hash = parts[0];
    source_uuid = parts[1];
    source_type = row_type_from_string (parts[2]);
    g_free ((gchar *) data);

    if (!lookup_iter_by_hash (GTK_TREE_MODEL (self->priv->model), source_hash, &source_iter)) {
        g_warning ("Source row not found");
        g_strfreev (parts);
        gtk_tree_path_free (path);
        gtk_drag_finish (context, FALSE, FALSE, time);
        return;
    }

    GtkTreePath *source_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->priv->model), &source_iter);
    if (gtk_tree_path_compare (source_path, path) == 0 ||
        gtk_tree_path_is_ancestor (source_path, path) ||
        gtk_tree_path_is_descendant (source_path, path)) {
        gtk_tree_path_free (source_path);
        g_strfreev (parts);
        gtk_tree_path_free (path);
        gtk_drag_finish (context, FALSE, FALSE, time);
        return;
    }
    gtk_tree_path_free (source_path);

    gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &source_iter,
                       COL_ROW_DATA, &row_data, -1);

    gtk_tree_model_get_iter (GTK_TREE_MODEL (self->priv->model), &target_iter, path);
    gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &target_iter,
                       COL_TYPE, &target_type, -1);

    has_parent = gtk_tree_model_iter_parent (GTK_TREE_MODEL (self->priv->model), &parent_iter, &target_iter);
    new_hash = g_uuid_string_random ();

    if (target_type == ROW_TYPE_SUBMENU &&
        (pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE ||
         pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER ||
         pos == GTK_TREE_VIEW_DROP_AFTER)) {
        gtk_tree_store_insert (self->priv->model, &new_iter, &target_iter, 0);
    } else {
        if (pos == GTK_TREE_VIEW_DROP_BEFORE) {
            gtk_tree_store_insert_before (self->priv->model, &new_iter,
                                         has_parent ? &parent_iter : NULL,
                                         &target_iter);
        } else {
            gtk_tree_store_insert_after (self->priv->model, &new_iter,
                                        has_parent ? &parent_iter : NULL,
                                        &target_iter);
        }
    }

    gtk_tree_store_set (self->priv->model, &new_iter,
                       COL_HASH, new_hash,
                       COL_UUID, source_uuid,
                       COL_TYPE, source_type,
                       COL_ROW_DATA, row_data,
                       -1);

    row_data_free (row_data);

    if (source_type == ROW_TYPE_SUBMENU) {
        move_tree_recursive (self, &source_iter, &new_iter);
    }

    remove_row_by_hash (self, source_hash);
    select_row_by_hash (self, new_hash);

    g_free (new_hash);
    g_strfreev (parts);
    gtk_tree_path_free (path);

    gtk_drag_finish (context, TRUE, TRUE, time);
    set_needs_saved (self, TRUE);
}

static void
on_name_entry_changed (GtkEntry *entry, NemoActionLayoutEditor *self)
{
    GtkTreeIter iter;
    RowData *data;
    const gchar *text;

    if (self->priv->updating_row_edit_fields)
        return;

    data = get_selected_row_data (self, &iter);
    if (data == NULL)
        return;

    text = gtk_entry_get_text (entry);
    g_free (data->user_label);
    data->user_label = g_strdup (text);

    /* For submenus, uuid matches the label */
    if (data->type == ROW_TYPE_SUBMENU) {
        g_free (data->uuid);
        data->uuid = g_strdup (text);
        gtk_tree_store_set (self->priv->model, &iter,
                            COL_UUID, text,
                            -1);
    }

    tree_store_update_row_data (self->priv->model, &iter, data);
    selected_row_changed (self, TRUE);
}

static void
on_name_entry_icon_press (GtkEntry *entry,
                          GtkEntryIconPosition icon_pos,
                          GdkEvent *event,
                          NemoActionLayoutEditor *self)
{
    GtkTreeIter iter;
    RowData *data;

    if (icon_pos != GTK_ENTRY_ICON_SECONDARY)
        return;

    data = get_selected_row_data (self, &iter);
    if (data == NULL)
        return;

    g_clear_pointer (&data->user_label, g_free);

    tree_store_update_row_data (self->priv->model, &iter, data);
    selected_row_changed (self, TRUE);
}

// Shortcuts

static void
accel_editing_done (GtkCellEditable *editable,
                    gpointer user_data)
{
    NemoActionLayoutEditor *self = NEMO_ACTION_LAYOUT_EDITOR (user_data);
    self->priv->editing_accel = FALSE;
    g_signal_handlers_disconnect_by_data (editable, user_data);
}

static void
on_accel_edit_started (GtkCellRenderer *renderer,
                       GtkCellEditable *editable,
                       char            *path,
                       gpointer         user_data)
{
    NemoActionLayoutEditor *self = NEMO_ACTION_LAYOUT_EDITOR (user_data);
    self->priv->editing_accel = TRUE;
    g_signal_connect (editable, "editing-done", G_CALLBACK (accel_editing_done), self);
}

static gboolean
find_conflict_recursively (NemoActionLayoutEditor *self,
                           guint                   key,
                           GdkModifierType         mods,
                           const gchar            *selected_hash,
                           GtkTreeIter            *parent_iter)
{
    GtkTreeIter child_iter;
    gboolean valid;

    if (parent_iter) {
        valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (self->priv->model),
                                             &child_iter, parent_iter);
    } else {
        valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->priv->model),
                                               &child_iter);
    }

    // Return TRUE if a conflict was found and the user wanted to preserve the
    // existing keybinding. FALSE if they wish to overwrite (or no conflict was
    // found).

    while (valid) {
        RowData *data;
        RowType row_type;
        g_autofree gchar *hash = NULL;

        gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &child_iter,
                            COL_HASH, &hash,
                           -1);

        if (g_strcmp0 (hash, selected_hash) == 0) {
            valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->priv->model), &child_iter);
            continue;
        }

        gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &child_iter,
                            COL_ROW_DATA, &data,
                            COL_TYPE, &row_type,
                           -1);

        if (data && data->accelerator && data->accelerator[0] != '\0') {
            guint row_key = 0;
            GdkModifierType row_mods = 0;
            gtk_accelerator_parse (data->accelerator, &row_key, &row_mods);

            if (row_key == key && row_mods == mods) {
                gchar *label = row_data_get_label (data);
                // FIXME: bad string for translation
                gchar *message = g_strdup_printf (
                    _("This key combination is already in use by another action:\n\n<b>%s</b>\n\nDo you want to replace it?"),
                    label);

                GtkWidget *dialog = gtk_message_dialog_new (
                    GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
                    GTK_DIALOG_MODAL,
                    GTK_MESSAGE_WARNING,
                    GTK_BUTTONS_YES_NO,
                    NULL);
                gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), message);

                gint response = gtk_dialog_run (GTK_DIALOG (dialog));
                gtk_widget_destroy (dialog);

                g_free (label);
                g_free (message);

                if (response == GTK_RESPONSE_YES) {
                    g_clear_pointer (&data->accelerator, g_free);
                    tree_store_update_row_data (self->priv->model, &child_iter, data);
                    return FALSE;
                } else {
                    row_data_free (data);
                    return TRUE;
                }
            }
        }

        if (data != NULL) {
            row_data_free (data);
        }

        if (row_type == ROW_TYPE_SUBMENU) {
            if (find_conflict_recursively (self, key, mods, selected_hash, &child_iter)) {
                return TRUE;
            }
        }

        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->priv->model), &child_iter);
    }

    return FALSE;
}

static gboolean
overwrite_any_existing (NemoActionLayoutEditor *self,
                        guint                   key,
                        GdkModifierType         mods,
                        const gchar            *selected_hash)
{
    GList *l;

    // Check against built-in Nemo shortcuts, fail if there's conflict.
    for (l = self->priv->builtin_shortcuts; l != NULL; l = l->next) {
        BuiltinShortcut *shortcut = l->data;
        if (shortcut->key == key && shortcut->mods == mods) {
            GtkWidget *parent_window = gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_WINDOW);
            gchar *message = g_markup_printf_escaped (
                _("This key combination is already in use by Nemo (<b>%s</b>). It cannot be changed."),
                shortcut->label);

            GtkWidget *dialog = gtk_message_dialog_new (
                GTK_WINDOW (parent_window),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                NULL);
            gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), message);

            gtk_dialog_run (GTK_DIALOG (dialog));
            gtk_widget_destroy (dialog);

            g_free (message);
            return FALSE;
        }
    }

    // Then check for conflicts with other actions
    return !find_conflict_recursively (self, key, mods, selected_hash, NULL);
}

static void
on_accel_edited (GtkCellRendererAccel *cell,
                const gchar *path_string,
                guint key,
                GdkModifierType mods,
                guint hardware_keycode,
                gpointer user_data)
{
    NemoActionLayoutEditor *self = NEMO_ACTION_LAYOUT_EDITOR (user_data);
    GtkTreeIter iter;
    RowData *data;
    g_autofree gchar *new_accel = NULL;
    g_autofree gchar *selected_hash = NULL;

    data = get_selected_row_data (self, &iter);
    if (data == NULL)
        return;

    gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &iter, COL_HASH, &selected_hash, -1);

    if (!overwrite_any_existing (self, key, mods, selected_hash)) {
        row_data_free (data);
        return;
    }

    new_accel = gtk_accelerator_name (key, mods);
    if (g_strcmp0 (data->accelerator, new_accel) == 0) {
        row_data_free (data);
        return;
    }

    g_free (data->accelerator);
    data->accelerator = g_strdup (new_accel);

    tree_store_update_row_data (self->priv->model, &iter, data);
    selected_row_changed (self, TRUE);
}

static void
on_accel_cleared (GtkCellRendererAccel *cell,
                 const gchar *path_string,
                 gpointer user_data)
{
    NemoActionLayoutEditor *self = NEMO_ACTION_LAYOUT_EDITOR (user_data);
    GtkTreeIter iter;
    RowData *data;

    data = get_selected_row_data (self, &iter);
    if (data == NULL)
        return;

    g_free (data->accelerator);
    data->accelerator = NULL;

    tree_store_update_row_data (self->priv->model, &iter, data);
    selected_row_changed (self, TRUE);
}

static GtkTreeIter
get_last_at_level (GtkTreeModel *model, GtkTreeIter *iter)
{
    if (gtk_tree_model_iter_has_child (model, iter)) {
        GtkTreeIter child, last;
        gtk_tree_model_iter_children (model, &child, iter);

        while (TRUE) {
            last = child;
            if (!gtk_tree_model_iter_next (model, &child))
                break;
        }

        return get_last_at_level (model, &last);
    }
    return *iter;
}

static gboolean
same_iter (GtkTreeModel *model, GtkTreeIter *iter1, GtkTreeIter *iter2)
{
    if (iter1 == NULL && iter2 == NULL)
        return TRUE;
    if (iter1 == NULL || iter2 == NULL)
        return FALSE;

    GtkTreePath *path1 = gtk_tree_model_get_path (model, iter1);
    GtkTreePath *path2 = gtk_tree_model_get_path (model, iter2);
    gint result = gtk_tree_path_compare (path1, path2);
    gtk_tree_path_free (path1);
    gtk_tree_path_free (path2);

    return result == 0;
}

static gboolean
path_is_valid (GtkTreeModel *model, GtkTreePath *path)
{
    GtkTreeIter iter;
    return gtk_tree_model_get_iter (model, &iter, path);
}

static gboolean
next_path_validated (GtkTreeModel *model, GtkTreePath *path)
{
    gtk_tree_path_next (path);
    return path_is_valid (model, path);
}

typedef struct {
    const gchar *hash;
    GtkTreeIter *result;
    gboolean found;
} LookupData;

static gboolean
lookup_iter_foreach (GtkTreeModel *model,
                     GtkTreePath  *path,
                     GtkTreeIter  *iter,
                     gpointer      user_data)
{
    LookupData *data = user_data;
    g_autofree gchar *current_hash = NULL;

    gtk_tree_model_get (model, iter, COL_HASH, &current_hash, -1);

    if (g_strcmp0 (current_hash, data->hash) == 0) {
        *data->result = *iter;
        data->found = TRUE;
        return TRUE;
    }

    return FALSE;
}

static gboolean
lookup_iter_by_hash (GtkTreeModel *model, const gchar *hash, GtkTreeIter *result)
{
    LookupData data = { hash, result, FALSE };
    gtk_tree_model_foreach (model, lookup_iter_foreach, &data);
    return data.found;
}

static void
remove_row_by_hash (NemoActionLayoutEditor *self, const gchar *hash)
{
    GtkTreeIter iter;
    if (lookup_iter_by_hash (GTK_TREE_MODEL (self->priv->model), hash, &iter)) {
        gtk_tree_store_remove (self->priv->model, &iter);
    }
}

static void
select_row_by_hash (NemoActionLayoutEditor *self, const gchar *hash)
{
    GtkTreeIter iter;
    if (lookup_iter_by_hash (GTK_TREE_MODEL (self->priv->model), hash, &iter)) {
        GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->treeview));
        gtk_tree_selection_select_iter (selection, &iter);
    }
}

static void
move_tree_recursive (NemoActionLayoutEditor *self, GtkTreeIter *source_iter, GtkTreeIter *new_parent)
{
    GtkTreeIter child;
    if (!gtk_tree_model_iter_children (GTK_TREE_MODEL (self->priv->model), &child, source_iter))
        return;

    do {
        gchar *hash, *uuid;
        RowType type;
        RowData *data;

        gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &child,
                           COL_HASH, &hash,
                           COL_UUID, &uuid,
                           COL_TYPE, &type,
                           COL_ROW_DATA, &data,
                           -1);

        GtkTreeIter new_child;
        gchar *new_hash = g_uuid_string_random ();
        gtk_tree_store_insert (self->priv->model, &new_child, new_parent, -1);
        gtk_tree_store_set (self->priv->model, &new_child,
                           COL_HASH, new_hash,
                           COL_UUID, uuid,
                           COL_TYPE, type,
                           COL_ROW_DATA, data,
                           -1);

        row_data_free (data);

        if (type == ROW_TYPE_SUBMENU) {
            move_tree_recursive (self, &child, &new_child);
        }

        g_free (new_hash);
        g_free (hash);
        g_free (uuid);
    } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->priv->model), &child));
}

static void
on_up_button_clicked (GtkButton *button, NemoActionLayoutEditor *self)
{
    GtkTreePath *path;
    GtkTreeIter iter;

    if (!get_selected_row (self, &path, &iter))
        return;

    GtkTreeIter parent;
    gboolean has_parent = gtk_tree_model_iter_parent (GTK_TREE_MODEL (self->priv->model), &parent, &iter);
    GtkTreeIter *parent_ptr = has_parent ? &parent : NULL;

    gchar *source_hash, *source_uuid;
    RowType source_type;
    RowData *source_data;
    gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &iter,
                       COL_HASH, &source_hash,
                       COL_UUID, &source_uuid,
                       COL_TYPE, &source_type,
                       COL_ROW_DATA, &source_data,
                       -1);

    gchar *target_hash = g_uuid_string_random ();
    GtkTreeIter inserted_iter;
    GtkTreeIter *target_parent = NULL;
    gboolean inserted = FALSE;
    gboolean source_was_expanded = FALSE;

    GtkTreePath *target_path = gtk_tree_path_copy (path);

    if (gtk_tree_path_prev (target_path)) {
        /* Move before previous sibling or into it */
        GtkTreeIter target_iter;
        gtk_tree_model_get_iter (GTK_TREE_MODEL (self->priv->model), &target_iter, target_path);
        target_iter = get_last_at_level (GTK_TREE_MODEL (self->priv->model), &target_iter);

        RowType target_iter_type;
        gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &target_iter,
                           COL_TYPE, &target_iter_type, -1);

        if (target_iter_type == ROW_TYPE_SUBMENU) {
            gtk_tree_store_prepend (self->priv->model, &inserted_iter, &target_iter);
            target_parent = gtk_tree_iter_copy (&target_iter);
        } else {
            GtkTreeIter target_iter_parent;
            gboolean has_target_parent = gtk_tree_model_iter_parent (GTK_TREE_MODEL (self->priv->model),
                                                                     &target_iter_parent, &target_iter);

            /* Check if both items are at the same level */
            gboolean same_level = FALSE;
            if (has_target_parent && has_parent) {
                same_level = same_iter (GTK_TREE_MODEL (self->priv->model), parent_ptr, &target_iter_parent);
            } else if (!has_target_parent && !has_parent) {
                /* Both at root level */
                same_level = TRUE;
            }

            if (same_level) {
                gtk_tree_store_insert_before (self->priv->model, &inserted_iter,
                                             has_target_parent ? &target_iter_parent : NULL,
                                             &target_iter);
                if (has_target_parent)
                    target_parent = gtk_tree_iter_copy (&target_iter_parent);
            } else {
                gtk_tree_store_insert_after (self->priv->model, &inserted_iter,
                                            has_target_parent ? &target_iter_parent : NULL,
                                            &target_iter);
                if (has_target_parent)
                    target_parent = gtk_tree_iter_copy (&target_iter_parent);
            }
        }
        inserted = TRUE;
    } else if (has_parent) {
        /* We're the first child of our parent - insert before parent at grandparent level */
        GtkTreeIter grandparent;
        gboolean has_grandparent = gtk_tree_model_iter_parent (GTK_TREE_MODEL (self->priv->model),
                                                                &grandparent, &parent);
        gtk_tree_store_insert_before (self->priv->model, &inserted_iter,
                                      has_grandparent ? &grandparent : NULL,
                                      &parent);
        if (has_grandparent)
            target_parent = gtk_tree_iter_copy (&grandparent);
        inserted = TRUE;
    } else {
        /* At root level and already first - nowhere to go */
    }

    if (inserted) {
        self->priv->updating_model = TRUE;

        gtk_tree_store_set (self->priv->model, &inserted_iter,
                           COL_HASH, target_hash,
                           COL_UUID, source_uuid,
                           COL_TYPE, source_type,
                           COL_ROW_DATA, source_data,
                           -1);

        row_data_free (source_data);

        if (source_type == ROW_TYPE_SUBMENU) {
            GtkTreePath *source_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->priv->model), &iter);
            source_was_expanded = gtk_tree_view_row_expanded (GTK_TREE_VIEW (self->priv->treeview), source_path);
            gtk_tree_path_free (source_path);

            move_tree_recursive (self, &iter, &inserted_iter);
        }

        if (target_parent) {
            GtkTreePath *parent_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->priv->model), target_parent);
            gtk_tree_view_expand_row (GTK_TREE_VIEW (self->priv->treeview), parent_path, TRUE);
            gtk_tree_path_free (parent_path);
            gtk_tree_iter_free (target_parent);
        } else if (source_was_expanded) {
            GtkTreePath *inserted_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->priv->model), &inserted_iter);
            gtk_tree_view_expand_row (GTK_TREE_VIEW (self->priv->treeview), inserted_path, TRUE);
            gtk_tree_path_free (inserted_path);
        }

        remove_row_by_hash (self, source_hash);
        self->priv->updating_model = FALSE;

        select_row_by_hash (self, target_hash);

        GtkTreePath *scroll_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->priv->model), &inserted_iter);
        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (self->priv->treeview), scroll_path, NULL, FALSE, 0, 0);
        gtk_tree_path_free (scroll_path);

        set_needs_saved (self, TRUE);
    }

    g_free (target_hash);
    g_free (source_hash);
    g_free (source_uuid);
    gtk_tree_path_free (target_path);
    gtk_tree_path_free (path);
}

static void
on_down_button_clicked (GtkButton *button, NemoActionLayoutEditor *self)
{
    GtkTreePath *path;
    GtkTreeIter iter;

    if (!get_selected_row (self, &path, &iter))
        return;

    gchar *source_hash, *source_uuid;
    RowType source_type;
    RowData *source_data;
    gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &iter,
                       COL_HASH, &source_hash,
                       COL_UUID, &source_uuid,
                       COL_TYPE, &source_type,
                       COL_ROW_DATA, &source_data,
                       -1);

    gchar *target_hash = g_uuid_string_random ();
    GtkTreeIter inserted_iter;
    GtkTreeIter *target_parent = NULL;
    gboolean inserted = FALSE;
    gboolean source_was_expanded = FALSE;

    GtkTreePath *target_path = gtk_tree_path_copy (path);

    if (next_path_validated (GTK_TREE_MODEL (self->priv->model), target_path)) {
        /* Maybe move into submenu or after next sibling */
        GtkTreeIter maybe_submenu;
        gtk_tree_model_get_iter (GTK_TREE_MODEL (self->priv->model), &maybe_submenu, target_path);

        RowType maybe_submenu_type;
        gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &maybe_submenu,
                           COL_TYPE, &maybe_submenu_type, -1);

        if (maybe_submenu_type == ROW_TYPE_SUBMENU) {
            gtk_tree_store_prepend (self->priv->model, &inserted_iter, &maybe_submenu);
            target_parent = gtk_tree_iter_copy (&maybe_submenu);
        } else {
            GtkTreeIter target_iter_parent;
            gboolean has_parent = gtk_tree_model_iter_parent (GTK_TREE_MODEL (self->priv->model),
                                                              &target_iter_parent, &maybe_submenu);
            gtk_tree_store_insert_after (self->priv->model, &inserted_iter,
                                        has_parent ? &target_iter_parent : NULL,
                                        &maybe_submenu);
            if (has_parent)
                target_parent = gtk_tree_iter_copy (&target_iter_parent);
        }
        inserted = TRUE;
    } else {
        /* No next sibling - try to move up and continue after parent */
        gtk_tree_path_free (target_path);
        target_path = gtk_tree_path_copy (path);

        /* Keep trying to go up until we find a valid next position */
        while (gtk_tree_path_get_depth (target_path) > 1) {
            if (!gtk_tree_path_up (target_path))
                break;

            if (!path_is_valid (GTK_TREE_MODEL (self->priv->model), target_path))
                break;

            /* Now try to go to the next sibling of this parent */
            GtkTreePath *test_path = gtk_tree_path_copy (target_path);
            if (next_path_validated (GTK_TREE_MODEL (self->priv->model), test_path)) {
                /* Found a next position - use it */
                GtkTreeIter target_iter;
                gtk_tree_model_get_iter (GTK_TREE_MODEL (self->priv->model), &target_iter, test_path);

                GtkTreeIter target_iter_parent;
                gboolean has_parent = gtk_tree_model_iter_parent (GTK_TREE_MODEL (self->priv->model),
                                                                  &target_iter_parent, &target_iter);

                /* Check if it's a submenu */
                RowType test_type;
                gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &target_iter,
                                   COL_TYPE, &test_type, -1);

                if (test_type == ROW_TYPE_SUBMENU) {
                    gtk_tree_store_prepend (self->priv->model, &inserted_iter, &target_iter);
                    target_parent = gtk_tree_iter_copy (&target_iter);
                } else {
                    gtk_tree_store_insert_after (self->priv->model, &inserted_iter,
                                                has_parent ? &target_iter_parent : NULL,
                                                &target_iter);
                    if (has_parent)
                        target_parent = gtk_tree_iter_copy (&target_iter_parent);
                }
                gtk_tree_path_free (test_path);
                inserted = TRUE;
                break;
            } else {
                /* No next sibling at this level, try going up another level */
                gtk_tree_path_free (test_path);
                continue;
            }
        }
    }

    if (inserted) {
        self->priv->updating_model = TRUE;

        gtk_tree_store_set (self->priv->model, &inserted_iter,
                           COL_HASH, target_hash,
                           COL_UUID, source_uuid,
                           COL_TYPE, source_type,
                           COL_ROW_DATA, source_data,
                           -1);
        row_data_free (source_data);

        if (source_type == ROW_TYPE_SUBMENU) {
            GtkTreePath *source_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->priv->model), &iter);
            source_was_expanded = gtk_tree_view_row_expanded (GTK_TREE_VIEW (self->priv->treeview), source_path);
            gtk_tree_path_free (source_path);

            move_tree_recursive (self, &iter, &inserted_iter);
        }

        if (target_parent) {
            GtkTreePath *parent_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->priv->model), target_parent);
            gtk_tree_view_expand_row (GTK_TREE_VIEW (self->priv->treeview), parent_path, TRUE);
            gtk_tree_path_free (parent_path);
            gtk_tree_iter_free (target_parent);
        } else if (source_was_expanded) {
            GtkTreePath *inserted_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->priv->model), &inserted_iter);
            gtk_tree_view_expand_row (GTK_TREE_VIEW (self->priv->treeview), inserted_path, TRUE);
            gtk_tree_path_free (inserted_path);
        }

        remove_row_by_hash (self, source_hash);
        self->priv->updating_model = FALSE;

        select_row_by_hash (self, target_hash);

        GtkTreePath *scroll_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->priv->model), &inserted_iter);
        gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (self->priv->treeview), scroll_path, NULL, FALSE, 0, 0);
        gtk_tree_path_free (scroll_path);

        set_needs_saved (self, TRUE);
    }

    g_free (target_hash);
    g_free (source_hash);
    g_free (source_uuid);
    gtk_tree_path_free (target_path);
    gtk_tree_path_free (path);
}

static void
on_row_activated (GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column,
                 NemoActionLayoutEditor *self)
{
    GtkTreeIter iter;

    RowData *data = get_selected_row_data (self, &iter);

    if (data && data->type == ROW_TYPE_ACTION) {
        data->enabled = !data->enabled;
        tree_store_update_row_data (self->priv->model, &iter, data);
        selected_row_changed (self, FALSE);
        save_disabled_list (self);
    }
}

static void
on_action_row_toggled (GtkCellRendererToggle *cell, gchar *path_str, NemoActionLayoutEditor *self)
{
    GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
    GtkTreeIter iter;
    RowData *data;

    if (gtk_tree_model_get_iter (GTK_TREE_MODEL (self->priv->model), &iter, path)) {
        gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &iter,
                           COL_ROW_DATA, &data,
                           -1);

        if (data) {
            data->enabled = !data->enabled;
            tree_store_update_row_data (self->priv->model, &iter, data);
            selected_row_changed (self, FALSE);
            save_disabled_list (self);
        }
    }

    gtk_tree_path_free (path);
}

// CellRenderer functions

static void
toggle_render_func (GtkTreeViewColumn *column, GtkCellRenderer *cell,
                   GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    RowType row_type;
    RowData *data;

    gtk_tree_model_get (model, iter,
                       COL_TYPE, &row_type,
                       COL_ROW_DATA, &data,
                       -1);

    if (row_type == ROW_TYPE_SUBMENU ||
        row_type == ROW_TYPE_SEPARATOR) {
        g_object_set (cell, "visible", FALSE, NULL);
    } else {
        g_object_set (cell,
                     "visible", TRUE,
                     "active", data ? data->enabled : FALSE,
                     NULL);
    }

    if (data != NULL) {
        row_data_free (data);
    }

}

static void
menu_icon_render_func (GtkTreeViewColumn *column, GtkCellRenderer *cell,
                      GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    RowData *data;
    gchar *icon_string;

    gtk_tree_model_get (model, iter, COL_ROW_DATA, &data, -1);

    if (data) {
        icon_string = row_data_get_icon_string (data, FALSE);
        if (icon_string) {
            g_object_set (cell, "icon-name", icon_string, NULL);
            g_free (icon_string);
        } else {
            g_object_set (cell, "icon-name", NULL, NULL);
        }

        row_data_free (data);
    }
}

static void
menu_label_render_func (GtkTreeViewColumn *column, GtkCellRenderer *cell,
                       GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    RowType row_type;
    RowData *data;
    gchar *label;

    gtk_tree_model_get (model, iter,
                       COL_TYPE, &row_type,
                       COL_ROW_DATA, &data,
                       -1);

    if (data) {
        label = row_data_get_label (data);

        if (row_type == ROW_TYPE_SUBMENU) {
            gchar *markup = g_strdup_printf ("<b>%s</b>", label);
            g_object_set (cell,
                         "markup", markup,
                         "weight", PANGO_WEIGHT_BOLD,
                         "style", PANGO_STYLE_NORMAL,
                         NULL);
            g_free (markup);
        } else {
            g_object_set (cell,
                         "text", label,
                         "weight", data->enabled ? PANGO_WEIGHT_NORMAL : PANGO_WEIGHT_ULTRALIGHT,
                         "style", data->enabled ? PANGO_STYLE_NORMAL : PANGO_STYLE_ITALIC,
                         NULL);
        }

        g_free (label);
        row_data_free (data);
    }

}

static void
accel_render_func (GtkTreeViewColumn *column, GtkCellRenderer *cell,
                  GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
    NemoActionLayoutEditor *self = NEMO_ACTION_LAYOUT_EDITOR (user_data);
    RowType row_type;
    RowData *data;
    guint key = 0;
    GdkModifierType mods = 0;

    gtk_tree_model_get (model, iter,
                       COL_TYPE, &row_type,
                       COL_ROW_DATA, &data,
                       -1);

    if (row_type == ROW_TYPE_SUBMENU ||
        row_type == ROW_TYPE_SEPARATOR) {
        g_object_set (cell, "visible", FALSE, NULL);

        if (data != NULL) {
            row_data_free (data);
        }
        return;
    }

    if (data && data->accelerator) {
        gtk_accelerator_parse (data->accelerator, &key, &mods);
    }

    g_object_set (cell,
                 "visible", TRUE,
                 "accel-key", key,
                 "accel-mods", mods,
                 NULL);

    /* Set text only for empty accelerators */
    if (!data || !data->accelerator || data->accelerator[0] == '\0') {
        GtkTreeIter selected_iter;
        gboolean is_selected = FALSE;

        if (get_selected_row (self, NULL, &selected_iter)) {
            GtkTreePath *current_path = gtk_tree_model_get_path (model, iter);
            GtkTreePath *selected_path = gtk_tree_model_get_path (model, &selected_iter);

            if (current_path && selected_path) {
                is_selected = (gtk_tree_path_compare (current_path, selected_path) == 0);
            }

            if (current_path)
                gtk_tree_path_free (current_path);
            if (selected_path)
                gtk_tree_path_free (selected_path);
        }

        if (is_selected) {
            if (!self->priv->editing_accel) {
                g_object_set (cell, "text", _("Click to add a shortcut"), NULL);
            } else {
                g_object_set (cell, "text", NULL, NULL);
            }
        } else {
            g_object_set (cell, "text", " ", NULL);
        }
    }

    if (data != NULL) {
        row_data_free (data);
    }
}

static void
extract_shortcuts_recursive (NemoActionLayoutEditor *self,
                              GtkWidget              *widget)
{
    if (GTK_IS_SHORTCUTS_SHORTCUT (widget)) {
        g_autofree gchar *title = NULL;
        g_autofree gchar *accelerator = NULL;

        /* Get the title and accelerator properties */
        g_object_get (widget,
                     "title", &title,
                     "accelerator", &accelerator,
                     NULL);

        self->priv->builtin_shortcuts = g_list_prepend (self->priv->builtin_shortcuts,
                                                        builtin_shortcut_new (title, accelerator));
    }

    if (GTK_IS_CONTAINER (widget)) {
        GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
        GList *l;

        for (l = children; l != NULL; l = l->next) {
            extract_shortcuts_recursive (self, GTK_WIDGET (l->data));
        }
        g_list_free (children);
    }
}

static void
load_builtin_shortcuts (NemoActionLayoutEditor *self)
{
    GtkBuilder *builder;
    GError *error = NULL;
    GObject *shortcuts_window;

    builder = gtk_builder_new ();
    if (!gtk_builder_add_from_resource (builder, "/org/nemo/nemo-shortcuts.ui", &error)) {
        g_warning ("Could not load nemo-shortcuts.ui from resource file - "
                  "we won't be able to detect built-in shortcut collisions: %s",
                  error->message);
        g_error_free (error);
        g_object_unref (builder);
        return;
    }

    shortcuts_window = gtk_builder_get_object (builder, "keyboard_shortcuts");
    if (!shortcuts_window) {
        g_warning ("Could not find keyboard_shortcuts object in UI file");
        g_object_unref (builder);
        return;
    }

    extract_shortcuts_recursive (self, GTK_WIDGET (shortcuts_window));

    g_object_unref (builder);
}

static JsonArray *
serialize_model_recursive (NemoActionLayoutEditor *self, GtkTreeIter *parent)
{
    JsonArray *array = json_array_new ();
    GtkTreeIter iter;
    gboolean valid;

    if (parent) {
        valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (self->priv->model), &iter, parent);
    } else {
        valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->priv->model), &iter);
    }

    while (valid) {
        RowType row_type;
        RowData *data;
        JsonObject *obj;

        gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &iter,
                           COL_TYPE, &row_type,
                           COL_ROW_DATA, &data,
                           -1);

        if (data) {
            obj = json_object_new ();
            json_object_set_string_member (obj, "uuid", data->uuid);
            json_object_set_string_member (obj, "type", row_type_to_string (data->type));

            if (data->user_label)
                json_object_set_string_member (obj, "user-label", data->user_label);
            else
                json_object_set_null_member (obj, "user-label");

            if (data->user_icon)
                json_object_set_string_member (obj, "user-icon", data->user_icon);
            else
                json_object_set_null_member (obj, "user-icon");

            if (data->accelerator)
                json_object_set_string_member (obj, "accelerator", data->accelerator);
            else
                json_object_set_null_member (obj, "accelerator");

            if (row_type == ROW_TYPE_SUBMENU) {
                JsonArray *children = serialize_model_recursive (self, &iter);
                json_object_set_array_member (obj, "children", children);
            }

            json_array_add_object_element (array, obj);
            row_data_free (data);
        }

        valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->priv->model), &iter);
    }

    return array;
}

static void
save_model (NemoActionLayoutEditor *self)
{
    gchar *config_dir = g_build_filename (g_get_user_config_dir (), "nemo", NULL);
    gchar *json_file = g_build_filename (g_get_user_config_dir (), JSON_FILE, NULL);
    JsonGenerator *generator;
    JsonNode *root;
    JsonObject *obj;
    JsonArray *toplevel;

    g_mkdir_with_parents (config_dir, 0755);

    toplevel = serialize_model_recursive (self, NULL);

    obj = json_object_new ();
    json_object_set_array_member (obj, "toplevel", toplevel);

    root = json_node_new (JSON_NODE_OBJECT);
    json_node_set_object (root, obj);

    generator = json_generator_new ();
    json_generator_set_root (generator, root);
    json_generator_set_pretty (generator, TRUE);

    json_generator_to_file (generator, json_file, NULL);

    g_object_unref (generator);
    json_node_free (root);
    json_object_unref (obj);
    g_free (json_file);
    g_free (config_dir);
}

static GHashTable *
load_installed_actions (NemoActionLayoutEditor *self)
{
    GHashTable *actions;
    const gchar * const *data_dirs;
    gint i;

    actions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                     (GDestroyNotify) row_data_free);

    data_dirs = g_get_system_data_dirs ();

    for (i = 0; data_dirs[i] != NULL; i++) {
        gchar *actions_dir = g_build_filename (data_dirs[i], USER_ACTIONS_DIR, NULL);
        GDir *dir = g_dir_open (actions_dir, 0, NULL);

        if (dir) {
            const gchar *name;
            while ((name = g_dir_read_name (dir)) != NULL) {
                if (g_str_has_suffix (name, ".nemo_action")) {
                    gchar *fullpath = g_build_filename (actions_dir, name, NULL);
                    GKeyFile *keyfile = g_key_file_new ();

                    if (g_key_file_load_from_file (keyfile, fullpath, G_KEY_FILE_NONE, NULL)) {
                        RowData *data = row_data_new ();
                        data->uuid = g_strdup (name);
                        data->type = ROW_TYPE_ACTION;
                        data->filename = fullpath;
                        data->keyfile = keyfile;

                        g_hash_table_replace (actions, g_strdup (name), data);
                    } else {
                        g_key_file_free (keyfile);
                        g_free (fullpath);
                    }
                }
            }
            g_dir_close (dir);
        }
        g_free (actions_dir);
    }

    gchar *user_actions_dir = g_build_filename (g_get_user_data_dir (), USER_ACTIONS_DIR, NULL);
    GDir *dir = g_dir_open (user_actions_dir, 0, NULL);

    if (dir) {
        const gchar *name;
        while ((name = g_dir_read_name (dir)) != NULL) {
            if (g_str_has_suffix (name, ".nemo_action")) {
                gchar *fullpath = g_build_filename (user_actions_dir, name, NULL);
                GKeyFile *keyfile = g_key_file_new ();

                if (g_key_file_load_from_file (keyfile, fullpath, G_KEY_FILE_NONE, NULL)) {
                    RowData *data = row_data_new ();
                    data->uuid = g_strdup (name);
                    data->type = ROW_TYPE_ACTION;
                    data->filename = fullpath;
                    data->keyfile = keyfile;

                    /* User actions override system actions */
                    g_hash_table_replace (actions, g_strdup (name), data);
                } else {
                    g_key_file_free (keyfile);
                    g_free (fullpath);
                }
            }
        }
        g_dir_close (dir);
    }
    g_free (user_actions_dir);

    return actions;
}

static gboolean
is_action_disabled_by_filename (const gchar *filename, gchar **disabled_actions)
{
    if (!disabled_actions || !filename)
        return FALSE;

    gchar *basename = g_path_get_basename (filename);
    gboolean is_disabled = FALSE;

    for (guint i = 0; disabled_actions[i] != NULL; i++) {
        if (g_strcmp0 (disabled_actions[i], basename) == 0) {
            is_disabled = TRUE;
            break;
        }
    }

    g_free (basename);
    return is_disabled;
}

static void
fill_model_recursive (NemoActionLayoutEditor *self, JsonArray *array,
                     GtkTreeIter *parent, GHashTable *installed_actions,
                     gchar **disabled_actions)
{
    guint i, len;

    if (!array)
        return;

    len = json_array_get_length (array);

    for (i = 0; i < len; i++) {
        JsonObject *item = json_array_get_object_element (array, i);
        const gchar *uuid = json_object_get_string_member (item, "uuid");
        const gchar *type_str = json_object_get_string_member (item, "type");
        RowType type = row_type_from_string (type_str);
        GtkTreeIter iter;
        RowData *data;

        if (type == ROW_TYPE_ACTION) {
            gchar *key = NULL;
            if (!g_hash_table_steal_extended (installed_actions, uuid, (gpointer *) &key, (gpointer *) &data))
                continue;
            g_free (key);
            data->enabled = !is_action_disabled_by_filename (data->filename, disabled_actions);
        } else if (type == ROW_TYPE_SEPARATOR) {
            data = row_data_new ();
            data->uuid = g_strdup ("separator");
            data->type = ROW_TYPE_SEPARATOR;

        } else if (type == ROW_TYPE_SUBMENU) {
            data = row_data_new ();
            data->uuid = g_strdup (uuid);
            data->type = ROW_TYPE_SUBMENU;

        } else {
            continue;
        }

        // optional fields
        if (json_object_has_member (item, "user-label") &&
            !json_object_get_null_member (item, "user-label")) {
            data->user_label = g_strdup (json_object_get_string_member (item, "user-label"));
        }

        if (json_object_has_member (item, "user-icon") &&
            !json_object_get_null_member (item, "user-icon")) {
            data->user_icon = g_strdup (json_object_get_string_member (item, "user-icon"));
        }

        if (json_object_has_member (item, "accelerator") &&
            !json_object_get_null_member (item, "accelerator")) {
            data->accelerator = g_strdup (json_object_get_string_member (item, "accelerator"));
        }

        tree_store_append_row_data (self->priv->model, &iter, parent, data);

        if (type == ROW_TYPE_SUBMENU &&
            json_object_has_member (item, "children")) {
            JsonArray *children = json_object_get_array_member (item, "children");
            fill_model_recursive (self, children, &iter, installed_actions, disabled_actions);
        }
    }
}

static void
reload_model (NemoActionLayoutEditor *self, gboolean flat)
{
    GHashTable *installed_actions;
    JsonParser *parser = NULL;
    JsonObject *root_obj = NULL;
    JsonArray *toplevel = NULL;
    gchar **disabled_actions;

    self->priv->updating_model = TRUE;
    gtk_tree_store_clear (self->priv->model);

    installed_actions = load_installed_actions (self);

    disabled_actions = g_settings_get_strv (self->priv->nemo_plugin_settings,
                                           NEMO_PLUGIN_PREFERENCES_DISABLED_ACTIONS);

    if (!flat) {
        gchar *json_file = g_build_filename (g_get_user_config_dir (), JSON_FILE, NULL);

        if (g_file_test (json_file, G_FILE_TEST_EXISTS)) {
            parser = json_parser_new ();

            if (json_parser_load_from_file (parser, json_file, NULL)) {
                JsonNode *root = json_parser_get_root (parser);
                if (root && JSON_NODE_HOLDS_OBJECT (root)) {
                    root_obj = json_node_get_object (root);
                    if (json_object_has_member (root_obj, "toplevel")) {
                        toplevel = json_object_get_array_member (root_obj, "toplevel");
                    }
                }
            }
        }

        g_free (json_file);
    }

    if (toplevel) {
        fill_model_recursive (self, toplevel, NULL, installed_actions, disabled_actions);
    }

    // Add remaining untracked actions to the end

    GHashTableIter hash_iter;
    gpointer key, value;
    g_hash_table_iter_init (&hash_iter, installed_actions);

    while (g_hash_table_iter_next (&hash_iter, &key, &value)) {
        RowData *data = (RowData *) value;
        GtkTreeIter iter;

        data->enabled = !is_action_disabled_by_filename (data->filename, disabled_actions);

        g_hash_table_iter_steal (&hash_iter);
        g_free (key);

        tree_store_append_row_data (self->priv->model, &iter, NULL, data);
    }

    g_strfreev (disabled_actions);

    if (parser)
        g_object_unref (parser);

    g_hash_table_destroy (installed_actions);

    gtk_tree_view_expand_all (GTK_TREE_VIEW (self->priv->treeview));

    GtkTreePath *first_path = gtk_tree_path_new_first ();
    gtk_tree_selection_select_path (gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->treeview)),
                                    first_path);
    gtk_tree_path_free (first_path);

    update_row_controls (self);
    update_arrow_button_states (self);

    self->priv->updating_model = FALSE;
}

static void
set_needs_saved (NemoActionLayoutEditor *self, gboolean needs_saved)
{
    self->priv->needs_saved = needs_saved;

    gtk_widget_set_sensitive (self->priv->save_button, needs_saved);
    gtk_widget_set_sensitive (self->priv->discard_button, needs_saved);
}

static void
update_row_controls (NemoActionLayoutEditor *self)
{
    RowData *data;
    RowType row_type;
    GtkTreeIter iter;

    self->priv->updating_row_edit_fields = TRUE;

    data = get_selected_row_data (self, &iter);

    if (!data) {
        gtk_widget_set_sensitive (self->priv->row_controls_box, FALSE);
        gtk_entry_set_text (GTK_ENTRY (self->priv->name_entry), "");
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (self->priv->name_entry),
                                          GTK_ENTRY_ICON_SECONDARY, NULL);
        gtk_image_clear (GTK_IMAGE (self->priv->icon_selector_image));
        gtk_image_clear (GTK_IMAGE (self->priv->original_icon_menu_image));
        gtk_widget_hide (self->priv->original_icon_menu_item);
        self->priv->updating_row_edit_fields = FALSE;
        return;
    }

    gtk_widget_set_sensitive (self->priv->row_controls_box, TRUE);

    gchar *label;

    gtk_tree_model_get (GTK_TREE_MODEL (self->priv->model), &iter,
                       COL_TYPE, &row_type,
                       -1);

    label = row_data_get_label (data);
    gtk_entry_set_text (GTK_ENTRY (self->priv->name_entry), label);
    g_free (label);

    gchar *icon_string = row_data_get_icon_string (data, FALSE);
    if (icon_string) {
        gtk_image_set_from_icon_name (GTK_IMAGE (self->priv->icon_selector_image),
                                     icon_string, GTK_ICON_SIZE_BUTTON);
        g_free (icon_string);
    } else {
        gtk_image_clear (GTK_IMAGE (self->priv->icon_selector_image));
    }

    // Original icon in menu - only show for actions
    gboolean is_action = (row_type == ROW_TYPE_ACTION);
    if (is_action) {
        gchar *original_icon_string = row_data_get_icon_string (data, TRUE);
        if (original_icon_string) {
            gtk_image_set_from_icon_name (GTK_IMAGE (self->priv->original_icon_menu_image),
                                         original_icon_string, GTK_ICON_SIZE_MENU);
            gtk_widget_set_sensitive (self->priv->original_icon_menu_item, TRUE);
            g_free (original_icon_string);
        } else {
            gtk_image_clear (GTK_IMAGE (self->priv->original_icon_menu_image));
            gtk_widget_set_sensitive (self->priv->original_icon_menu_item, FALSE);
        }
        gtk_widget_show (self->priv->original_icon_menu_item);
    } else {
        gtk_widget_hide (self->priv->original_icon_menu_item);
    }

    gboolean is_separator = (row_type == ROW_TYPE_SEPARATOR);
    xapp_visibility_group_set_sensitive (self->priv->selected_item_widgets_group,
                                        data->enabled && !is_separator);

    gtk_widget_set_sensitive (self->priv->remove_submenu_button,
                             row_type != ROW_TYPE_ACTION);

    // Show clear custom label icon if applicable
    if (row_type == ROW_TYPE_ACTION && data->user_label) {
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (self->priv->name_entry),
                                          GTK_ENTRY_ICON_SECONDARY,
                                          "edit-delete-symbolic");
    } else {
        gtk_entry_set_icon_from_icon_name (GTK_ENTRY (self->priv->name_entry),
                                          GTK_ENTRY_ICON_SECONDARY, NULL);
    }

    row_data_free (data);
    self->priv->updating_row_edit_fields = FALSE;
}

static void
update_arrow_button_states (NemoActionLayoutEditor *self)
{
    GtkTreeIter iter, first_iter;
    GtkTreePath *path;
    gboolean can_up = TRUE, can_down = TRUE;

    if (!get_selected_row (self, &path, &iter)) {
        gtk_widget_set_sensitive (self->priv->up_button, FALSE);
        gtk_widget_set_sensitive (self->priv->down_button, FALSE);
        return;
    }

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->priv->model), &first_iter)) {
        GtkTreePath *first_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->priv->model), &first_iter);
        if (gtk_tree_path_compare (path, first_path) == 0) {
            can_up = FALSE;
        }
        gtk_tree_path_free (first_path);
    }

    if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->priv->model), &first_iter)) {
        GtkTreeIter last_top = first_iter;
        GtkTreeIter temp = first_iter;
        while (gtk_tree_model_iter_next (GTK_TREE_MODEL (self->priv->model), &temp)) {
            last_top = temp;
        }

        GtkTreeIter absolute_last = get_last_at_level (GTK_TREE_MODEL (self->priv->model), &last_top);
        GtkTreePath *last_path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->priv->model), &absolute_last);

        if (last_path) {
            if (gtk_tree_path_compare (path, last_path) == 0) {
                can_down = FALSE;
            }
            gtk_tree_path_free (last_path);
        }
    }

    gtk_widget_set_sensitive (self->priv->up_button, can_up);
    gtk_widget_set_sensitive (self->priv->down_button, can_down);

    gtk_tree_path_free (path);
}

static void
selected_row_changed (NemoActionLayoutEditor *self, gboolean needs_saved)
{
    GtkTreePath *path;
    GtkTreeIter iter;

    if (self->priv->updating_model)
        return;

    if (get_selected_row (self, &path, &iter)) {
        gtk_tree_model_row_changed (GTK_TREE_MODEL (self->priv->model), path, &iter);
        gtk_tree_path_free (path);
    }

    update_row_controls (self);
    update_arrow_button_states (self);

    if (needs_saved)
        set_needs_saved (self, TRUE);
}

static void
on_selection_changed (GtkTreeSelection *selection, NemoActionLayoutEditor *self)
{
    if (self->priv->updating_model)
        return;

    update_row_controls (self);
    update_arrow_button_states (self);

    gtk_widget_queue_draw (self->priv->treeview);
}

static void
nemo_action_layout_editor_dispose (GObject *object)
{
    NemoActionLayoutEditor *self = NEMO_ACTION_LAYOUT_EDITOR (object);

    g_clear_object (&self->priv->model);
    g_clear_object (&self->priv->nemo_plugin_settings);
    g_clear_pointer (&self->priv->selected_item_widgets_group, xapp_visibility_group_free);

    if (self->priv->dir_monitors) {
        g_list_free_full (self->priv->dir_monitors, g_object_unref);
        self->priv->dir_monitors = NULL;
    }

    if (self->priv->builtin_shortcuts) {
        g_list_free_full (self->priv->builtin_shortcuts, (GDestroyNotify) builtin_shortcut_free);
        self->priv->builtin_shortcuts = NULL;
    }

    G_OBJECT_CLASS (nemo_action_layout_editor_parent_class)->dispose (object);
}

static void
nemo_action_layout_editor_finalize (GObject *object)
{
    G_OBJECT_CLASS (nemo_action_layout_editor_parent_class)->finalize (object);
}

static void
nemo_action_layout_editor_class_init (NemoActionLayoutEditorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = nemo_action_layout_editor_dispose;
    object_class->finalize = nemo_action_layout_editor_finalize;
}

static void
nemo_action_layout_editor_init (NemoActionLayoutEditor *self)
{
    GtkBuilder *builder;
    GtkWidget *main_box, *menu, *item;
    GtkTreeViewColumn *column;
    GtkCellRenderer *cell;
    GtkTreeSelection *selection;
    PangoLayout *layout;
    gint layout_w, layout_h;

    self->priv = nemo_action_layout_editor_get_instance_private (self);

    self->priv->needs_saved = FALSE;
    self->priv->updating_model = FALSE;
    self->priv->updating_row_edit_fields = FALSE;
    self->priv->dnd_autoscroll_timeout_id = 0;

    self->priv->nemo_plugin_settings = g_settings_new ("org.nemo.plugins");
    self->priv->settings_handler_id = g_signal_connect (self->priv->nemo_plugin_settings,
                                                        "changed",
                                                        G_CALLBACK (on_disabled_settings_list_changed),
                                                        self);

    self->priv->builtin_shortcuts = NULL;
    load_builtin_shortcuts (self);

    builder = gtk_builder_new_from_resource ("/org/nemo/action-layout-editor/nemo-action-layout-editor.glade");
    main_box = GTK_WIDGET (gtk_builder_get_object (builder, "layout_editor_box"));
    gtk_box_pack_start (GTK_BOX (self), main_box, TRUE, TRUE, 0);

    self->priv->scrolled_window = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_holder"));
    self->priv->save_button = GTK_WIDGET (gtk_builder_get_object (builder, "save_button"));
    self->priv->discard_button = GTK_WIDGET (gtk_builder_get_object (builder, "discard_changes_button"));
    self->priv->default_layout_button = GTK_WIDGET (gtk_builder_get_object (builder, "default_layout_button"));
    self->priv->row_controls_box = GTK_WIDGET (gtk_builder_get_object (builder, "row_controls_box"));
    self->priv->new_row_button = GTK_WIDGET (gtk_builder_get_object (builder, "new_row_button"));
    self->priv->remove_submenu_button = GTK_WIDGET (gtk_builder_get_object (builder, "remove_submenu_button"));
    self->priv->up_button = GTK_WIDGET (gtk_builder_get_object (builder, "up_button"));
    self->priv->down_button = GTK_WIDGET (gtk_builder_get_object (builder, "down_button"));
    self->priv->icon_selector_menu_button = GTK_WIDGET (gtk_builder_get_object (builder, "icon_selector_menu_button"));
    self->priv->icon_selector_image = GTK_WIDGET (gtk_builder_get_object (builder, "icon_selector_image"));
    self->priv->name_entry = GTK_WIDGET (gtk_builder_get_object (builder, "name_entry"));

    // The store owns all of this data. Whenever we get a RowData from the store,
    // it's a copy and must be freed. Whenever we add a RowData to the store, it copies
    // it, we must free the one we made (just like strings).
    self->priv->model = gtk_tree_store_new (N_COLUMNS,
                                            G_TYPE_STRING,    // hash
                                            G_TYPE_STRING,    // uuid
                                            G_TYPE_INT,       // RowType
                                            ROW_DATA_TYPE);   // RowData

    self->priv->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (self->priv->model));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (self->priv->treeview), FALSE);
    gtk_tree_view_set_enable_tree_lines (GTK_TREE_VIEW (self->priv->treeview), TRUE);
    gtk_container_add (GTK_CONTAINER (self->priv->scrolled_window), self->priv->treeview);

    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_expand (column, TRUE);
    gtk_tree_view_column_set_spacing (column, 2);
    gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->treeview), column);

    cell = gtk_cell_renderer_toggle_new ();
    g_object_set (cell, "activatable", TRUE, NULL);
    g_signal_connect (cell, "toggled", G_CALLBACK (on_action_row_toggled), self);
    gtk_tree_view_column_pack_start (column, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, cell, toggle_render_func, self, NULL);

    cell = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, cell, menu_icon_render_func, self, NULL);

    cell = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, cell, menu_label_render_func, self, NULL);

    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_expand (column, TRUE);
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->treeview), column);

    cell = gtk_cell_renderer_accel_new ();
    g_object_set (cell,
                 "editable", TRUE,
                 "xalign", 0.0,
                 NULL);
    gtk_tree_view_column_pack_end (column, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func (column, cell, accel_render_func, self, NULL);
    
    layout = gtk_widget_create_pango_layout (GTK_WIDGET (self->priv->treeview), _("Click to add a shortcut"));
    pango_layout_get_pixel_size (layout, &layout_w, &layout_h);
    gtk_tree_view_column_set_min_width (column, layout_w);
    g_object_unref (layout);

    g_signal_connect (cell, "editing-started", G_CALLBACK (on_accel_edit_started), self);
    g_signal_connect (cell, "accel-edited", G_CALLBACK (on_accel_edited), self);
    g_signal_connect (cell, "accel-cleared", G_CALLBACK (on_accel_cleared), self);
    self->priv->editing_accel = FALSE;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->treeview));
    g_signal_connect (selection, "changed", G_CALLBACK (on_selection_changed), self);
    g_signal_connect (self->priv->treeview, "row-activated", G_CALLBACK (on_row_activated), self);

    gtk_drag_source_set (self->priv->treeview,
                        GDK_BUTTON1_MASK,
                        NULL, 0,
                        GDK_ACTION_MOVE);
    gtk_drag_dest_set (self->priv->treeview,
                      GTK_DEST_DEFAULT_ALL,
                      NULL, 0,
                      GDK_ACTION_MOVE);

    gtk_drag_source_add_text_targets (GTK_WIDGET (self->priv->treeview));
    gtk_drag_dest_add_text_targets (GTK_WIDGET (self->priv->treeview));
    g_signal_connect (self->priv->treeview, "drag-begin", G_CALLBACK (on_drag_begin), self);
    g_signal_connect (self->priv->treeview, "drag-end", G_CALLBACK (on_drag_end), self);
    g_signal_connect (self->priv->treeview, "drag-motion", G_CALLBACK (on_drag_motion), self);
    g_signal_connect (self->priv->treeview, "drag-data-get", G_CALLBACK (on_drag_data_get), self);
    g_signal_connect (self->priv->treeview, "drag-data-received", G_CALLBACK (on_drag_data_received), self);

    menu = gtk_menu_new ();
    item = gtk_menu_item_new_with_label (_("New submenu"));
    g_signal_connect (item, "activate", G_CALLBACK (on_new_submenu_clicked), self);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    item = gtk_menu_item_new_with_label (_("New separator"));
    g_signal_connect (item, "activate", G_CALLBACK (on_new_separator_clicked), self);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    gtk_widget_show_all (menu);
    gtk_menu_button_set_popup (GTK_MENU_BUTTON (self->priv->new_row_button), menu);

    menu = gtk_menu_new ();

    GtkWidget *image = gtk_image_new_from_icon_name ("xsi-checkbox-symbolic", GTK_ICON_SIZE_MENU);
    item = gtk_image_menu_item_new_with_label (_("No icon"));
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    g_signal_connect (item, "activate", G_CALLBACK (on_clear_icon_clicked), self);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    self->priv->original_icon_menu_image = gtk_image_new ();
    self->priv->original_icon_menu_item = gtk_image_menu_item_new_with_label (_("Use the original icon (if there is one)"));
    gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (self->priv->original_icon_menu_item),
                                    self->priv->original_icon_menu_image);
    g_signal_connect (self->priv->original_icon_menu_item, "activate", G_CALLBACK (on_original_icon_clicked), self);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), self->priv->original_icon_menu_item);

    item = gtk_menu_item_new_with_label (_("Choose..."));
    g_signal_connect (item, "activate", G_CALLBACK (on_choose_icon_clicked), self);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

    gtk_widget_show_all (menu);
    gtk_menu_button_set_popup (GTK_MENU_BUTTON (self->priv->icon_selector_menu_button), menu);

    g_signal_connect (self->priv->save_button, "clicked", G_CALLBACK (on_save_clicked), self);
    g_signal_connect (self->priv->discard_button, "clicked", G_CALLBACK (on_discard_changes_clicked), self);
    g_signal_connect (self->priv->default_layout_button, "clicked", G_CALLBACK (on_default_layout_clicked), self);
    g_signal_connect (self->priv->remove_submenu_button, "clicked", G_CALLBACK (on_remove_submenu_clicked), self);
    g_signal_connect (self->priv->up_button, "clicked", G_CALLBACK (on_up_button_clicked), self);
    g_signal_connect (self->priv->down_button, "clicked", G_CALLBACK (on_down_button_clicked), self);
    g_signal_connect (self->priv->name_entry, "changed", G_CALLBACK (on_name_entry_changed), self);
    g_signal_connect (self->priv->name_entry, "icon-press", G_CALLBACK (on_name_entry_icon_press), self);

    self->priv->selected_item_widgets_group = xapp_visibility_group_new (TRUE, TRUE, NULL);
    xapp_visibility_group_add_widget (self->priv->selected_item_widgets_group,
                                     self->priv->icon_selector_menu_button);
    xapp_visibility_group_add_widget (self->priv->selected_item_widgets_group,
                                     self->priv->name_entry);

    g_object_unref (builder);

    gtk_widget_show_all (GTK_WIDGET (self));

    reload_model (self, FALSE);
    set_needs_saved (self, FALSE);
}

GtkWidget *
nemo_action_layout_editor_new (void)
{
    return g_object_new (NEMO_TYPE_ACTION_LAYOUT_EDITOR, NULL);
}

/* nemo-template-config-widget.h */

/*  A widget that displays a list of templates. The user can add, remove and rename them.
 */

#include <config.h>
#include "nemo-template-config-widget.h"
#include "nemo-application.h"
#include "nemo-view.h"
#include "nemo-file.h"
#include "nemo-global-preferences.h"
#include <libnemo-private/nemo-file-utilities.h>

#include <glib.h>

G_DEFINE_TYPE (NemoTemplateConfigWidget, nemo_template_config_widget, NEMO_TYPE_CONFIG_BASE_WIDGET);

typedef struct {
    NemoTemplateConfigWidget *widget;

    gchar *name;
    gchar *content_desc;
    gchar *path;
    GIcon *gicon;
} TemplateInfo;

#define TEMPLATE_ATTRIBUTES \
    G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
    G_FILE_ATTRIBUTE_STANDARD_ICON

enum {
    TARGET_URI_LIST,
    TARGET_TEXT_PLAIN
};

static GtkTargetEntry drop_targets[] = {
    {"text/uri-list", 0, TARGET_URI_LIST},
    {"text/plain", 0, TARGET_TEXT_PLAIN}
};

static void refresh_widget (NemoTemplateConfigWidget *widget);
static void start_renaming_selected_row (NemoTemplateConfigWidget *widget, GtkWidget *row);
static void template_info_free (TemplateInfo *info);
static void
template_info_free (TemplateInfo *info)
{
    g_free (info->name);
    g_free (info->content_desc);
    g_free (info->path);
    g_object_unref (info->gicon);
    g_free (info);
}

static void
cancel_file_monitors (NemoTemplateConfigWidget *widget)
{
    GList *l;

    for (l = widget->dir_monitors; l != NULL; l = l->next) {
        g_file_monitor_cancel (l->data);
        g_object_unref (l->data);
    }

    g_list_free (widget->dir_monitors);
    widget->dir_monitors = NULL;
}

static void
on_dir_changed (GFileMonitor     *monitor,
                GFile            *file,
                GFile            *other_file,
                GFileMonitorEvent event_type,
                gpointer          user_data)
{
    NemoTemplateConfigWidget *widget = NEMO_TEMPLATE_CONFIG_WIDGET (user_data);

    refresh_widget (widget);
}

static void
monitor_path (NemoTemplateConfigWidget *widget, const gchar *path)
{
    GFile *dir = g_file_new_for_path (path);

    GFileMonitor *mon = g_file_monitor_directory (dir, G_FILE_MONITOR_SEND_MOVED, NULL, NULL);

    g_object_unref (dir);

    if (mon != NULL) {
        g_signal_connect (mon, "changed", G_CALLBACK (on_dir_changed), widget);
        widget->dir_monitors = g_list_append (widget->dir_monitors, mon);
    }
}

static void
on_row_selected (GtkWidget *box,
                  GtkWidget *row,
                  gpointer   user_data)
{
    NemoTemplateConfigWidget *widget = NEMO_TEMPLATE_CONFIG_WIDGET (user_data);

    gtk_widget_set_sensitive (widget->remove_button, row != NULL);
    gtk_widget_set_sensitive (widget->rename_button, row != NULL);
    gtk_widget_set_sensitive (widget->edit_button, row != NULL);
}

static void
on_row_activated (GtkWidget *box,
                  GtkWidget *row,
                  gpointer   user_data)
{
    g_return_if_fail (GTK_IS_WIDGET (row));

    NemoTemplateConfigWidget *widget = NEMO_TEMPLATE_CONFIG_WIDGET (user_data);

    if (row == NULL) {
        return;
    }

    start_renaming_selected_row (widget, row);
}

static gchar **
get_filename_tuple (const gchar *path)
{
    g_autoptr(GStrvBuilder) builder = g_strv_builder_new ();
    g_autofree gchar *basename = g_path_get_basename (path);
    gchar *ptr;

    ptr = g_strrstr (basename, ".");

    if (ptr != NULL) {
        gchar *stem = g_strndup (basename, ptr - basename);
        g_strv_builder_add (builder, stem);
        g_free (stem);
        g_strv_builder_add (builder, ptr + 1);
    } else {
        g_strv_builder_add (builder, basename);
    }

    return g_strv_builder_end (builder);
}

static TemplateInfo *
new_entry (const gchar *filename)
{
    GFileInfo *file_info;
    GFile *file;
    GError *error = NULL;
    gchar **basename_tuple;

    TemplateInfo *info = g_new0 (TemplateInfo, 1);

    file = g_file_new_for_path (filename);
    file_info = g_file_query_info (file, TEMPLATE_ATTRIBUTES , 0, NULL, &error);

    if (error != NULL) {
        g_warning ("Failed to query content type for %s: %s", filename, error->message);
        g_clear_error (&error);

        info->content_desc = g_strdup (_("Unknown template"));
    } else {
        const gchar *ctype = g_file_info_get_content_type (file_info);

        info->content_desc = g_content_type_get_description (ctype);
        info->gicon = g_object_ref (g_file_info_get_icon (file_info));
    }

    basename_tuple = get_filename_tuple (filename);
    info->name = g_strdup (basename_tuple[0]);
    g_strfreev (basename_tuple);

    info->path = g_strdup (filename);

    g_clear_object (&file_info);
    g_object_unref (file);

    return info;
}

static void
populate_from_directory (NemoTemplateConfigWidget *widget, const gchar *path)
{
    GDir *dir;

    monitor_path (widget, path);
    dir = g_dir_open (path, 0, NULL);

    if (dir) {
        const char *name;

        while ((name = g_dir_read_name (dir))) {
            char *filename;

            filename = g_build_filename (path, name, NULL);

            if (g_file_test (filename, G_FILE_TEST_IS_DIR)) {
                populate_from_directory (widget, filename);
                g_free (filename);
                continue;
            }

            widget->templates = g_list_append (widget->templates, new_entry (filename));
            g_free (filename);
        }

        g_dir_close (dir);
    }
}

static void
refresh_widget (NemoTemplateConfigWidget *widget)
{
    if (widget->templates != NULL) {
        g_list_free_full (widget->templates, (GDestroyNotify) template_info_free);
        widget->templates = NULL;
    }

    nemo_config_base_widget_clear_list (NEMO_CONFIG_BASE_WIDGET (widget));
    cancel_file_monitors (widget);

    gchar *path = NULL;

    path = nemo_get_templates_directory ();
    populate_from_directory (widget, path);
    g_clear_pointer (&path, g_free);

    if (widget->templates == NULL) {
        GtkWidget *empty_label = gtk_label_new (NULL);
        gchar *markup = NULL;

        markup = g_strdup_printf ("<i>%s</i>", _("No templates found. Click New or drag a file here to create one."));

        gtk_label_set_markup (GTK_LABEL (empty_label), markup);
        g_free (markup);

        GtkWidget *empty_row = gtk_list_box_row_new ();
        gtk_container_add (GTK_CONTAINER (empty_row), empty_label);

        gtk_widget_show_all (empty_row);
        gtk_container_add (GTK_CONTAINER (NEMO_CONFIG_BASE_WIDGET (widget)->listbox), empty_row);
        gtk_widget_set_sensitive (empty_row, FALSE);
    } else {
        GList *l;
        GtkSizeGroup *group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

        for (l = widget->templates; l != NULL; l=l->next) {
            GtkWidget *w;
            GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
            gchar *markup;

            TemplateInfo *info = l->data;

            w = gtk_image_new_from_gicon (info->gicon, GTK_ICON_SIZE_BUTTON);
            gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

            w = gtk_label_new (info->content_desc);
            markup = g_strdup_printf ("<b>%s</b>", info->name);
            gtk_label_set_markup (GTK_LABEL (w), markup);
            g_free (markup);
            gtk_label_set_xalign (GTK_LABEL (w), 0.0);
            gtk_size_group_add_widget (group, w);
            gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 2);

            w = gtk_label_new (info->content_desc);
            gtk_label_set_xalign (GTK_LABEL (w), 0.0);
            gtk_box_pack_start (GTK_BOX (box), w, TRUE, TRUE, 0);

            GtkWidget *row = gtk_list_box_row_new ();
            gtk_container_add (GTK_CONTAINER (row), box);

            g_object_set_data (G_OBJECT (row), "template-info", info);

            gtk_widget_show_all (row);
            gtk_container_add (GTK_CONTAINER (NEMO_CONFIG_BASE_WIDGET (widget)->listbox), row);
        }

        gtk_widget_set_sensitive (GTK_WIDGET (NEMO_CONFIG_BASE_WIDGET (widget)->listbox), TRUE);
    }

    nemo_config_base_widget_set_default_buttons_sensitive (NEMO_CONFIG_BASE_WIDGET (widget), widget->templates != NULL);
}

static void
on_remove_row_clicked (GtkWidget *button, gpointer user_data)
{
    NemoTemplateConfigWidget *widget = NEMO_TEMPLATE_CONFIG_WIDGET (user_data);
    GtkListBoxRow *row = gtk_list_box_get_selected_row (GTK_LIST_BOX (NEMO_CONFIG_BASE_WIDGET (widget)->listbox));

    if (row == NULL) {
        return;
    }

    TemplateInfo *info = g_object_get_data (G_OBJECT (row), "template-info");
    GFile *file = g_file_new_for_path (info->path);
    g_file_delete_async (file, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
    g_object_unref (file);
}

static void
on_rename_entry_changed (GtkEditable *editable, gpointer user_data)
{
    GtkDialog *dialog = GTK_DIALOG (user_data);
    GtkEntry *entry = GTK_ENTRY (editable);
    gchar *text = g_strdup (gtk_entry_get_text (entry));

    g_strstrip (text); 
    gtk_widget_set_sensitive (gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_ACCEPT), *text != '\0');
    g_free (text);
}

static void
start_renaming_selected_row (NemoTemplateConfigWidget *widget, GtkWidget *row)
{
    TemplateInfo *info = g_object_get_data (G_OBJECT (row), "template-info");

    gint response = 0;
    GtkWidget *rename_dialog;
    rename_dialog = gtk_dialog_new_with_buttons ("Rename template",
                                          GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (widget))),
                                          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                          _("_Cancel"),
                                          GTK_RESPONSE_REJECT,
                                          _("_Rename"),
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (rename_dialog), GTK_RESPONSE_ACCEPT);

    GtkWidget *content = gtk_dialog_get_content_area (GTK_DIALOG (rename_dialog));
    GtkWidget *entry = gtk_entry_new ();
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    gtk_box_pack_start (GTK_BOX (content), entry, FALSE, FALSE, 0);
    gtk_entry_set_text (GTK_ENTRY (entry), info->name);
    g_signal_connect (GTK_EDITABLE (entry), "changed", G_CALLBACK (on_rename_entry_changed), rename_dialog);

    gtk_widget_show_all (rename_dialog);
    response = gtk_dialog_run (GTK_DIALOG (rename_dialog));
    if (response == GTK_RESPONSE_ACCEPT) {
        GFile *file = g_file_new_for_path (info->path);
        gchar **basename_tuple = get_filename_tuple (info->path);
        gchar *new_name = NULL;

        if (g_strv_length (basename_tuple) > 1) {
            new_name = g_strdup_printf ("%s.%s", gtk_entry_get_text (GTK_ENTRY (entry)), basename_tuple[1]);
        } else {
            new_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
        }

        g_strfreev (basename_tuple);

        g_file_set_display_name_async (file, new_name, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
        g_object_unref (file);
        g_free (new_name);
    }

    gtk_widget_destroy (rename_dialog);
}

static void
import_template (const gchar *filename)
{
    GFile *source, *dest;
    gchar *basename, *template_dir, *dest_path;

    if (!g_path_is_absolute (filename) && !g_uri_is_valid (filename, G_URI_FLAGS_NONE, NULL)) {
        g_warning ("Invalid file path or uri: %s", filename);
        return;
    }

    if (g_path_is_absolute (filename)) {
        source = g_file_new_for_path (filename);
    }
    else
    if (g_uri_is_valid (filename, G_URI_FLAGS_NONE, NULL)) {
        source = g_file_new_for_uri (filename);
    }
    else {
        g_warning ("Invalid file path or uri: %s", filename);
        return;
    }

    basename = g_file_get_basename (source);
    template_dir = nemo_get_templates_directory ();
    dest_path = g_build_filename (template_dir, basename, NULL);
    dest = g_file_new_for_path (dest_path);

    g_file_copy_async (source, dest, G_FILE_COPY_OVERWRITE, G_PRIORITY_DEFAULT, NULL, NULL, NULL, NULL, NULL);

    g_free (basename);
    g_free (template_dir);
    g_free (dest_path);
    g_object_unref (source);
    g_object_unref (dest);
}

static gboolean
file_has_valid_app_association (const GtkFileFilterInfo *filter_info, gpointer data)
{
    g_autofree gchar *content_type = g_content_type_from_mime_type (filter_info->mime_type);
    g_autoptr(GAppInfo) app_info = g_app_info_get_default_for_type (content_type, FALSE);

    return app_info != NULL;
}

static void
on_new_template_clicked (GtkWidget *button, gpointer user_data)
{
    NemoTemplateConfigWidget *widget = NEMO_TEMPLATE_CONFIG_WIDGET (user_data);
    GtkWidget *dialog;
    gint response = 0;

    dialog = gtk_file_chooser_dialog_new (_("Select a document to use as a template"),
                                          GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (widget))),
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                          _("Cancel"),
                                          GTK_RESPONSE_CANCEL,
                                          _("Select"),
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name (filter, "Files associated with applications");
    gtk_file_filter_add_custom (filter, GTK_FILE_FILTER_MIME_TYPE,
                                file_has_valid_app_association, NULL, NULL);

    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
    response = gtk_dialog_run (GTK_DIALOG (dialog));

    if (response == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *file_chooser = GTK_FILE_CHOOSER (dialog);
        gchar *selected_filename = gtk_file_chooser_get_filename (file_chooser);

        if (selected_filename != NULL) {
            import_template (selected_filename);
        }

        g_free (selected_filename);
    }

    gtk_widget_destroy (dialog);
}

static void
on_rename_row_clicked (GtkWidget *button, gpointer user_data)
{
    NemoTemplateConfigWidget *widget = NEMO_TEMPLATE_CONFIG_WIDGET (user_data);
    GtkWidget *row = GTK_WIDGET (gtk_list_box_get_selected_row (GTK_LIST_BOX (NEMO_CONFIG_BASE_WIDGET (widget)->listbox)));

    if (row == NULL) {
        return;
    }

    start_renaming_selected_row (widget, row);
}

static void
on_edit_template_clicked (GtkWidget *button, gpointer user_data)
{
    NemoTemplateConfigWidget *widget = NEMO_TEMPLATE_CONFIG_WIDGET (user_data);
    GtkWidget *row = GTK_WIDGET (gtk_list_box_get_selected_row (GTK_LIST_BOX (NEMO_CONFIG_BASE_WIDGET (widget)->listbox)));

    if (row == NULL) {
        return;
    }

    TemplateInfo *info = g_object_get_data (G_OBJECT (row), "template-info");
    GFile *file = g_file_new_for_path (info->path);
    gchar *uri = g_file_get_uri (file);
    g_object_unref (file);

    GAppLaunchContext *context = G_APP_LAUNCH_CONTEXT (gdk_display_get_app_launch_context (gdk_display_get_default ()));

    g_app_info_launch_default_for_uri_async (uri, context, NULL, NULL, NULL);
    g_object_unref (context);

    g_free (uri);
}

static void
on_open_folder_clicked (GtkWidget *button, NemoTemplateConfigWidget *widget)
{
    gchar *path = NULL;
    path = nemo_get_templates_directory ();
    GFile *location = g_file_new_for_path (path);

    nemo_application_open_location (nemo_application_get_singleton (),
                                    location,
                                    NULL,
                                    "nemo",
                                    FALSE);

    g_free (path);
    g_object_unref (location);
}

static void
on_drag_data_received(GtkWidget *widget,
                      GdkDragContext *context,
                      gint x, gint y,
                      GtkSelectionData *data,
                      guint target_type,
                      guint time,
                      gpointer user_data)
{
    gboolean success = FALSE;

    if (data != NULL && gtk_selection_data_get_length(data) >= 0) {
        const guchar *raw_data = gtk_selection_data_get_data(data);

        if (target_type == TARGET_URI_LIST) {
            gchar **uris = g_uri_list_extract_uris((const gchar*)raw_data);

            if (uris != NULL && uris[0] != NULL) {
                for (int i = 0; uris[i] != NULL; i++) {
                    import_template (uris[i]);
                }

                g_strfreev(uris);
                success = TRUE;
            }
        } else if (target_type == TARGET_TEXT_PLAIN) {
            import_template ((const gchar *) raw_data);
            success = TRUE;
        }
    }

    gtk_drag_finish(context, success, FALSE, time);
}

static gboolean
on_drag_motion (GtkWidget *widget,
                GdkDragContext *context,
                gint x, gint y,
                guint time,
                gpointer user_data)
{
    GdkAtom target = gtk_drag_dest_find_target(widget, context, NULL);

    if (target == GDK_NONE) {
        gdk_drag_status(context, 0, time);
        return FALSE;
    } else {
        gdk_drag_status(context, GDK_ACTION_COPY, time);
        return TRUE;
    }
}

static void
nemo_template_config_widget_finalize (GObject *object)
{
    NemoTemplateConfigWidget *widget = NEMO_TEMPLATE_CONFIG_WIDGET (object);

    if (widget->templates != NULL) {
        g_list_free_full (widget->templates, (GDestroyNotify) template_info_free);
        widget->templates = NULL;
    }

    cancel_file_monitors (widget);

    G_OBJECT_CLASS (nemo_template_config_widget_parent_class)->finalize (object);
}

static void
nemo_template_config_widget_class_init (NemoTemplateConfigWidgetClass *klass)
{
    GObjectClass *oclass;
    oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = nemo_template_config_widget_finalize;
}

static void
nemo_template_config_widget_init (NemoTemplateConfigWidget *self)
{
    self->templates = NULL;

    GtkWidget *label = nemo_config_base_widget_get_label (NEMO_CONFIG_BASE_WIDGET (self));
    gtk_list_box_set_selection_mode (GTK_LIST_BOX (NEMO_CONFIG_BASE_WIDGET (self)->listbox), GTK_SELECTION_SINGLE);
    gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (NEMO_CONFIG_BASE_WIDGET (self)->listbox), FALSE);

    gchar *title = g_strdup (_("New Document templates"));
    gchar *markup = g_strdup_printf ("<b>%s</b>", title);

    gtk_label_set_markup (GTK_LABEL (label), markup);

    g_free (title);
    g_free (markup);

    GtkWidget *widget = gtk_button_new_from_icon_name ("folder-symbolic", GTK_ICON_SIZE_BUTTON);

    GtkWidget *bb = NEMO_CONFIG_BASE_WIDGET (self)->rbuttonbox;
    gtk_box_pack_end (GTK_BOX (bb),
                      widget,
                      FALSE, FALSE, 0);
    gtk_widget_show (widget);

    g_signal_connect (widget, "clicked", G_CALLBACK (on_open_folder_clicked), self);

    bb = NEMO_CONFIG_BASE_WIDGET (self)->lbuttonbox;

    widget = gtk_button_new_with_label (_("New"));
    gtk_widget_set_tooltip_text (widget, _("Create a new template"));

    gtk_box_pack_start (GTK_BOX (bb),
                      widget,
                      FALSE, FALSE, 0);
    gtk_widget_show (widget);
    g_signal_connect (widget, "clicked", G_CALLBACK (on_new_template_clicked), self);

    widget = gtk_button_new_with_label (_("Remove"));
    gtk_widget_set_tooltip_text (widget, _("Delete the selected template"));

    gtk_box_pack_start (GTK_BOX (bb),
                      widget,
                      FALSE, FALSE, 0);
    gtk_widget_show (widget);
    g_signal_connect (widget, "clicked", G_CALLBACK (on_remove_row_clicked), self);
    self->remove_button = widget;

    widget = gtk_button_new_with_label (_("Rename"));
    gtk_widget_set_tooltip_text (widget, _("Rename the selected template"));

    gtk_box_pack_start (GTK_BOX (bb),
                      widget,
                      FALSE, FALSE, 0);
    gtk_widget_show (widget);
    g_signal_connect (widget, "clicked", G_CALLBACK (on_rename_row_clicked), self);
    self->rename_button = widget;

    widget = gtk_button_new_with_label (_("Edit"));
    gtk_widget_set_tooltip_text (widget, _("Modify the selected template's contents"));

    gtk_box_pack_start (GTK_BOX (bb),
                      widget,
                      FALSE, FALSE, 0);
    gtk_widget_show (widget);
    g_signal_connect (widget, "clicked", G_CALLBACK (on_edit_template_clicked), self);
    self->edit_button = widget;

    gtk_widget_hide (nemo_config_base_widget_get_enable_button (NEMO_CONFIG_BASE_WIDGET (self)));
    gtk_widget_hide (nemo_config_base_widget_get_disable_button (NEMO_CONFIG_BASE_WIDGET (self)));

    g_signal_connect (NEMO_CONFIG_BASE_WIDGET (self)->listbox, "row-selected", G_CALLBACK (on_row_selected), self);
    g_signal_connect (NEMO_CONFIG_BASE_WIDGET (self)->listbox, "row-activated", G_CALLBACK (on_row_activated), self);

    gtk_drag_dest_set(NEMO_CONFIG_BASE_WIDGET (self)->listbox,
                      GTK_DEST_DEFAULT_ALL,
                      drop_targets,
                      G_N_ELEMENTS(drop_targets),
                      GDK_ACTION_COPY);

    g_signal_connect(NEMO_CONFIG_BASE_WIDGET (self)->listbox, "drag-data-received",
                    G_CALLBACK(on_drag_data_received), label);
    g_signal_connect(NEMO_CONFIG_BASE_WIDGET (self)->listbox, "drag-motion",
                    G_CALLBACK(on_drag_motion), NULL);

    refresh_widget (self);
    on_row_selected (NEMO_CONFIG_BASE_WIDGET (self)->listbox, NULL, self);
}

GtkWidget *
nemo_template_config_widget_new (void)
{
  return g_object_new (NEMO_TYPE_TEMPLATE_CONFIG_WIDGET, NULL);
}

/* nemo-script-config-widget.h */

/*  A widget that displays a list of scripts to enable or disable.
 *  This is usually part of a NemoPluginManagerWidget
 */

#include <config.h>
#include "nemo-script-config-widget.h"
#include "nemo-application.h"
#include "nemo-view.h"
#include "nemo-file.h"
#include "nemo-global-preferences.h"
#include <libnemo-private/nemo-file-utilities.h>

#include <glib.h>

G_DEFINE_TYPE (NemoScriptConfigWidget, nemo_script_config_widget, NEMO_TYPE_CONFIG_BASE_WIDGET);


typedef struct {
    NemoScriptConfigWidget *widget;

    gchar *name;
} ScriptProxy;

static void
script_proxy_free (ScriptProxy *proxy)
{
    g_clear_pointer (&proxy->name, g_free);
}

static GtkWidget *
get_button_for_row (GtkWidget *row)
{
    GtkWidget *ret;

    GtkWidget *box = gtk_bin_get_child (GTK_BIN (row));
    GList *clist = gtk_container_get_children (GTK_CONTAINER (box));

    ret = clist->data;

    g_list_free (clist);

    return ret;
}

static void
on_row_activated (GtkWidget *box, GtkWidget *row, GtkWidget *widget)
{
    GtkWidget *button = get_button_for_row (row);

    gtk_button_clicked (GTK_BUTTON (button));
}

static void
on_check_toggled(GtkWidget *button, ScriptProxy *proxy)
{
    gboolean enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

    gchar **blacklist = g_settings_get_strv (nemo_plugin_preferences,
    		                                 NEMO_PLUGIN_PREFERENCES_DISABLED_SCRIPTS);

    GPtrArray *new_list = g_ptr_array_new ();

    guint i;

    if (enabled) {
        for (i = 0; i < g_strv_length (blacklist); i++) {
            if (g_strcmp0 (blacklist[i], proxy->name) == 0)
                continue;
            g_ptr_array_add (new_list, g_strdup (blacklist[i]));
        }
    } else {
        for (i = 0; i < g_strv_length (blacklist); i++) {
            g_ptr_array_add (new_list, g_strdup (blacklist[i]));
        }

        g_ptr_array_add (new_list, g_strdup (proxy->name));
    }

    g_ptr_array_add (new_list, NULL);

    gchar **new_list_ptr = (char **) g_ptr_array_free (new_list, FALSE);

    g_signal_handler_block (nemo_plugin_preferences, proxy->widget->bl_handler);
    g_settings_set_strv (nemo_plugin_preferences,
    		             NEMO_PLUGIN_PREFERENCES_DISABLED_SCRIPTS,
						 (const gchar * const *) new_list_ptr);
    g_signal_handler_unblock (nemo_plugin_preferences, proxy->widget->bl_handler);

    g_strfreev (blacklist);
    g_strfreev (new_list_ptr);
}

static void
populate_from_directory (NemoScriptConfigWidget *widget, const gchar *path)
{
    GDir *dir;

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

            ScriptProxy *p = g_slice_new0 (ScriptProxy);
            p->name = g_strdup (name);
            p->widget = widget;

            widget->scripts = g_list_append (widget->scripts, p);
            g_free (filename);
        }

        g_dir_close (dir);
    }
}

static void
refresh_widget (NemoScriptConfigWidget *widget)
{
    if (widget->scripts != NULL) {
        g_list_free_full (widget->scripts, (GDestroyNotify) script_proxy_free);
        widget->scripts = NULL;
    }

    nemo_config_base_widget_clear_list (NEMO_CONFIG_BASE_WIDGET (widget));

    gchar *path = NULL;

    path = nemo_get_scripts_directory_path ();
    populate_from_directory (widget, path);
    g_clear_pointer (&path, g_free);

    if (widget->scripts == NULL) {
        GtkWidget *empty_label = gtk_label_new (NULL);
        gchar *markup = NULL;

        markup = g_strdup_printf ("<i>%s</i>", _("No scripts found"));

        gtk_label_set_markup (GTK_LABEL (empty_label), markup);
        g_free (markup);

        GtkWidget *empty_row = gtk_list_box_row_new ();
        gtk_container_add (GTK_CONTAINER (empty_row), empty_label);

        gtk_widget_show_all (empty_row);
        gtk_container_add (GTK_CONTAINER (NEMO_CONFIG_BASE_WIDGET (widget)->listbox), empty_row);
        gtk_widget_set_sensitive (GTK_WIDGET (NEMO_CONFIG_BASE_WIDGET (widget)->listbox), FALSE);
    } else {
        GList *l;
        gchar **blacklist = g_settings_get_strv (nemo_plugin_preferences,
        		                                 NEMO_PLUGIN_PREFERENCES_DISABLED_SCRIPTS);

        for (l = widget->scripts; l != NULL; l=l->next) {
            ScriptProxy *proxy = l->data;

            gboolean active = TRUE;
            guint i = 0;

            for (i = 0; i < g_strv_length (blacklist); i++) {
                if (g_strcmp0 (blacklist[i], proxy->name) == 0) {
                    active = FALSE;
                    break;
                }
            }

            GtkWidget *w;
            GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

            GtkWidget *button = gtk_check_button_new ();
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), active);
            g_signal_connect (button, "toggled", G_CALLBACK (on_check_toggled), proxy);
            gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 2);

            w = gtk_label_new (proxy->name);
            gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 2);

            GtkWidget *row = gtk_list_box_row_new ();
            gtk_container_add (GTK_CONTAINER (row), box);

            gtk_widget_show_all (row);
            gtk_container_add (GTK_CONTAINER (NEMO_CONFIG_BASE_WIDGET (widget)->listbox), row);
        }

        gtk_widget_set_sensitive (GTK_WIDGET (NEMO_CONFIG_BASE_WIDGET (widget)->listbox), TRUE);

        g_strfreev (blacklist);
    }

    nemo_config_base_widget_set_default_buttons_sensitive (NEMO_CONFIG_BASE_WIDGET (widget), widget->scripts != NULL);
}

static void
on_settings_changed (GSettings *settings, gchar *key, gpointer user_data)
{
    NemoScriptConfigWidget *w = NEMO_SCRIPT_CONFIG_WIDGET (user_data);

    refresh_widget (w);
}

static void
on_enable_clicked (GtkWidget *button, NemoScriptConfigWidget *widget)
{
    g_settings_set_strv (nemo_plugin_preferences,
    		             NEMO_PLUGIN_PREFERENCES_DISABLED_SCRIPTS,
						 NULL);
}

static void
on_disable_clicked (GtkWidget *button, NemoScriptConfigWidget *widget)
{
    GPtrArray *new_list = g_ptr_array_new ();

    GList *l;

    for (l = widget->scripts; l != NULL; l = l->next)
        g_ptr_array_add (new_list, g_strdup (((ScriptProxy *) l->data)->name));

    g_ptr_array_add (new_list, NULL);

    gchar **new_list_ptr = (char **) g_ptr_array_free (new_list, FALSE);
    g_settings_set_strv (nemo_plugin_preferences,
    		             NEMO_PLUGIN_PREFERENCES_DISABLED_SCRIPTS,
    		             (const gchar * const *) new_list_ptr);

    g_strfreev (new_list_ptr);
}

static void
on_open_folder_clicked (GtkWidget *button, NemoScriptConfigWidget *widget)
{
    gchar *path = NULL;
    path = nemo_get_scripts_directory_path ();
    GFile *location = g_file_new_for_path (path);

    nemo_application_open_location (nemo_application_get_singleton (),
                                    location,
                                    NULL,
                                    "nemo");

    g_free (path);
    g_object_unref (location);
}

static void
on_dir_changed (GFileMonitor     *monitor,
                GFile            *file,
                GFile            *other_file,
                GFileMonitorEvent event_type,
                gpointer          user_data)
{
    NemoScriptConfigWidget *widget = NEMO_SCRIPT_CONFIG_WIDGET (user_data);

    refresh_widget (widget);
}

static void
try_monitor_path (NemoScriptConfigWidget *widget, const gchar *path)
{
    GFile *dir = g_file_new_for_path (path);

    GFileMonitor *mon = g_file_monitor_directory (dir, G_FILE_MONITOR_SEND_MOVED, NULL, NULL);

    g_object_unref (dir);

    if (mon != NULL) {
        g_signal_connect (mon, "changed", G_CALLBACK (on_dir_changed), widget);
        widget->dir_monitors = g_list_append (widget->dir_monitors, mon);
    }
}

static void setup_dir_monitors (NemoScriptConfigWidget *widget)
{
    widget->dir_monitors = NULL;

    gchar **data_dirs = (gchar **) g_get_system_data_dirs ();

    guint i;
    for (i = 0; i < g_strv_length (data_dirs); i++) {
        gchar *path = g_build_filename (data_dirs[i], "nemo", "actions", NULL);
        try_monitor_path (widget, path);
        g_free (path);
    }

    gchar *path = g_build_filename (g_get_user_data_dir (), "nemo", "actions", NULL);
    try_monitor_path (widget, path);
    g_free (path);
}

static void
nemo_script_config_widget_finalize (GObject *object)
{
    NemoScriptConfigWidget *widget = NEMO_SCRIPT_CONFIG_WIDGET (object);

    if (widget->scripts != NULL) {
        g_list_free_full (widget->scripts, (GDestroyNotify) script_proxy_free);
        widget->scripts = NULL;
    }

    GList *l;

    for (l = widget->dir_monitors; l != NULL; l = l->next) {
        g_file_monitor_cancel (l->data);
        g_object_unref (l->data);
    }

    g_list_free (widget->dir_monitors);

    g_signal_handler_disconnect (nemo_plugin_preferences, widget->bl_handler);

    G_OBJECT_CLASS (nemo_script_config_widget_parent_class)->finalize (object);
}

static void
nemo_script_config_widget_class_init (NemoScriptConfigWidgetClass *klass)
{
    GObjectClass *oclass;
    oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = nemo_script_config_widget_finalize;
}

static void
nemo_script_config_widget_init (NemoScriptConfigWidget *self)
{
    self->scripts = NULL;

    self->bl_handler = g_signal_connect (nemo_plugin_preferences,
    		                             "changed::" NEMO_PLUGIN_PREFERENCES_DISABLED_SCRIPTS,
                                         G_CALLBACK (on_settings_changed), self);

    GtkWidget *label = nemo_config_base_widget_get_label (NEMO_CONFIG_BASE_WIDGET (self));

    gchar *title = g_strdup (_("Scripts"));
    gchar *markup = g_strdup_printf ("<b>%s</b>", title);

    gtk_label_set_markup (GTK_LABEL (label), markup);

    g_free (title);
    g_free (markup);

    GtkWidget *widget = gtk_button_new_from_icon_name ("folder", GTK_ICON_SIZE_BUTTON);

    GtkWidget *bb = nemo_config_base_widget_get_buttonbox (NEMO_CONFIG_BASE_WIDGET (self));
    gtk_box_pack_end (GTK_BOX (bb),
                      widget,
                      FALSE, FALSE, 0);
    gtk_widget_show (widget);

    g_signal_connect (widget, "clicked", G_CALLBACK (on_open_folder_clicked), self);

    g_signal_connect (nemo_config_base_widget_get_enable_button (NEMO_CONFIG_BASE_WIDGET (self)), "clicked",
                                                                 G_CALLBACK (on_enable_clicked), self);

    g_signal_connect (nemo_config_base_widget_get_disable_button (NEMO_CONFIG_BASE_WIDGET (self)), "clicked",
                                                                  G_CALLBACK (on_disable_clicked), self);

    g_signal_connect (NEMO_CONFIG_BASE_WIDGET (self)->listbox, "row-activated", G_CALLBACK (on_row_activated), self);

    refresh_widget (self);

    setup_dir_monitors (self);
}

GtkWidget *
nemo_script_config_widget_new (void)
{
  return g_object_new (NEMO_TYPE_SCRIPT_CONFIG_WIDGET, NULL);
}

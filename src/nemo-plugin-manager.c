/* nemo-plugin-manager.c */

/*  A GtkWidget that can be inserted into a UI that provides a simple interface for
 *  managing the loading of extensions, actions and scripts
 */

#include <config.h>
#include "nemo-plugin-manager.h"
#include <glib.h>

G_DEFINE_TYPE (NemoPluginManager, nemo_plugin_manager, GTK_TYPE_BIN);

static GtkWindow *plugin_manager = NULL;

static void
nemo_plugin_manager_class_init (NemoPluginManagerClass *klass)
{
}

static void
nemo_plugin_manager_init (NemoPluginManager *self)
{
    GtkWidget *grid = gtk_grid_new ();
    self->grid = grid;

    gtk_widget_set_margin_left (grid, 10);
    gtk_widget_set_margin_right (grid, 10);
    gtk_widget_set_margin_top (grid, 10);
    gtk_widget_set_margin_bottom (grid, 10);
    gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
    gtk_grid_set_row_homogeneous (GTK_GRID (grid), TRUE);
    gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);

    self->action_widget = nemo_action_config_widget_new ();
    gtk_grid_attach (GTK_GRID (grid), self->action_widget, 0, 0, 1, 1);

    self->script_widget = nemo_script_config_widget_new ();
    gtk_grid_attach (GTK_GRID (grid), self->script_widget, 1, 0, 1, 1);

    self->extension_widget = nemo_extension_config_widget_new ();
    gtk_grid_attach (GTK_GRID (grid), self->extension_widget, 0, 1, 2, 1);

    gtk_container_add (GTK_CONTAINER (self), grid);

    gtk_widget_show_all (GTK_WIDGET (self));
}

NemoPluginManager *
nemo_plugin_manager_new (void)
{
  return g_object_new (NEMO_TYPE_PLUGIN_MANAGER, NULL);
}

/**
 * nemo_plugin_manager_get_grid:
 * @manager: a #NemoPluginManager
 *
 * Returns: (transfer none): the grid #GtkWidget containing the individual
 * listboxes.
 */

GtkWidget *
nemo_plugin_manager_get_grid (NemoPluginManager *manager)
{
    return manager->grid;
}

/**
 * nemo_plugin_manager_get_action_widget:
 * @manager: a #NemoPluginManager
 *
 * Returns: (transfer none): the action #GtkWidget for managing actions.
 */

GtkWidget *
nemo_plugin_manager_get_action_widget (NemoPluginManager *manager)
{
    return manager->action_widget;
}

/**
 * nemo_plugin_manager_get_script_widget:
 * @manager: a #NemoPluginManager
 *
 * Returns: (transfer none): the action #GtkWidget for managing scripts.
 */

GtkWidget *
nemo_plugin_manager_get_script_widget (NemoPluginManager *manager)
{
    return manager->script_widget;
}

/**
 * nemo_plugin_manager_get_extension_widget:
 * @manager: a #NemoPluginManager
 *
 * Returns: (transfer none): the action #GtkWidget for managing extensions.
 */

GtkWidget *
nemo_plugin_manager_get_extension_widget (NemoPluginManager *manager)
{
    return manager->extension_widget;
}

static void
clear_singleton (void)
{
    g_object_unref (plugin_manager);
    plugin_manager = NULL;
}

void
nemo_plugin_manager_show (NemoWindow *parent)
{
    if (plugin_manager) {
        NemoPluginManager *pm = NEMO_PLUGIN_MANAGER (gtk_bin_get_child (GTK_BIN (plugin_manager)));

        NEMO_ACTION_CONFIG_WIDGET (pm->action_widget)->view_window = parent;
        NEMO_SCRIPT_CONFIG_WIDGET (pm->script_widget)->view_window = parent;

        gtk_window_present (plugin_manager);
        return;
    }

    GtkWindow *window = NULL;
    window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));

    gtk_window_set_title (window, _("Plugins"));
    gtk_window_set_icon_name (window, "preferences-system");

    gtk_window_set_default_size (window, 640, 480);

    NemoPluginManager *pm = nemo_plugin_manager_new ();

    /* Can't use transient property and destroy_with_parent, we don't want
     * the plugins window to be forced to stay on top of the calling Nemo window,
     * but we do want it to go away if that window is closed. */
    g_signal_connect_object (parent, "destroy", G_CALLBACK (gtk_widget_destroy), window, G_CONNECT_SWAPPED);
    g_signal_connect_object (window, "destroy", G_CALLBACK (clear_singleton), NULL, G_CONNECT_SWAPPED);

    NEMO_ACTION_CONFIG_WIDGET (pm->action_widget)->view_window = parent;
    NEMO_SCRIPT_CONFIG_WIDGET (pm->script_widget)->view_window = parent;
    NEMO_EXTENSION_CONFIG_WIDGET (pm->extension_widget)->view_window = parent;

    plugin_manager = g_object_ref (window);

    gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (pm));

    gtk_widget_show_all (GTK_WIDGET (window));
}


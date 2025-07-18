/* nemo-plugin-manager.c */

/*  A GtkWidget that can be inserted into a UI that provides a simple interface for
 *  managing the loading of extensions, actions and scripts
 */

#include <config.h>
#include "nemo-plugin-manager.h"
#include "nemo-action-config-widget.h"
#include "nemo-extension-config-widget.h"
#include <glib.h>

struct _NemoPluginManager
{
    GtkBin parent_instance;
};

G_DEFINE_TYPE (NemoPluginManager, nemo_plugin_manager, GTK_TYPE_BIN);

static void
nemo_plugin_manager_class_init (NemoPluginManagerClass *klass)
{
}

static void
nemo_plugin_manager_init (NemoPluginManager *self)
{
    GtkWidget *widget, *grid;

    grid = gtk_grid_new ();

    gtk_widget_set_margin_left (grid, 10);
    gtk_widget_set_margin_right (grid, 10);
    gtk_widget_set_margin_top (grid, 10);
    gtk_widget_set_margin_bottom (grid, 10);
    gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
    gtk_grid_set_row_homogeneous (GTK_GRID (grid), TRUE);
    gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);

    widget = nemo_action_config_widget_new ();
    gtk_grid_attach (GTK_GRID (grid), widget, 0, 0, 2, 1);

    widget = nemo_extension_config_widget_new ();
    gtk_grid_attach (GTK_GRID (grid), widget, 0, 1, 2, 1);

    gtk_container_add (GTK_CONTAINER (self), grid);

    gtk_widget_show_all (GTK_WIDGET (self));
}

NemoPluginManager *
nemo_plugin_manager_new (void)
{
  return g_object_new (NEMO_TYPE_PLUGIN_MANAGER, NULL);
}


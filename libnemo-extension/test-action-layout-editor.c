#include <gtk/gtk.h>
#include "nemo-action-layout-editor.h"

int main (int argc, char *argv[])
{
    GtkWidget *window;
    GtkWidget *editor;

    gtk_init (&argc, &argv);

    /* Create main window */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "Nemo Action Layout Editor Test");
    gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
    g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

    /* Create editor widget */
    editor = nemo_action_layout_editor_new ();
    gtk_container_set_border_width (GTK_CONTAINER (window), 6);
    gtk_container_add (GTK_CONTAINER (window), editor);

    /* Show everything */
    gtk_widget_show_all (window);
    gtk_window_present (GTK_WINDOW (window));

    /* Run */
    gtk_main ();

    return 0;
}

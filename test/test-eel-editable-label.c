/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

#include <config.h>

#include <gtk/gtk.h>

#include <eel/eel-editable-label.h>


static void
quit (GtkWidget *widget, gpointer data)
{
	gtk_main_quit ();
}

int 
main (int argc, char* argv[])
{
	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *vbox;
	
	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (quit), NULL);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	gtk_container_add (GTK_CONTAINER (window), vbox);
	
	label = eel_editable_label_new ("Centered dsau dsfgsdfgoydsfiugy oiusdyfg iouysdf goiuys dfioguy siodufgy iusdyfgiu ydsf giusydf gouiysdfgoiuysdfg oiudyfsg Label");

	gtk_widget_set_size_request (label, 200, -1);
	eel_editable_label_set_line_wrap (EEL_EDITABLE_LABEL (label), TRUE);
	
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 4);

	label = eel_editable_label_new ("Left aligned label");

	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 4);

	label = eel_editable_label_new ("Right aligned label");

	gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
	
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 4);
	
	gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);
	
	gtk_widget_show_all (window);	
	
	gtk_main ();

	return 0;
}

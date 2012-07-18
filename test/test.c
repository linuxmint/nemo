#include "test.h"
#include <sys/types.h>
#include <unistd.h>

void
test_init (int *argc,
	   char ***argv)
{
	gtk_init (argc, argv);

	eel_make_warnings_and_criticals_stop_in_debugger ();
}

int
test_quit (int exit_code)
{
	if (gtk_main_level () > 0) {
		gtk_main_quit ();
	}

	return exit_code;
}

void
test_delete_event (GtkWidget *widget,
		   GdkEvent *event,
		   gpointer callback_data)
{
	test_quit (0);
}

GtkWidget *
test_window_new (const char *title, guint border_width)
{
	GtkWidget *window;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	
	if (title != NULL) {
		gtk_window_set_title (GTK_WINDOW (window), title);
	}

	g_signal_connect (window, "delete_event",
                          G_CALLBACK (test_delete_event), NULL);
	
	gtk_container_set_border_width (GTK_CONTAINER (window), border_width);
	
	return window;
}

GdkPixbuf *
test_pixbuf_new_named (const char *name, float scale)
{
	GdkPixbuf *pixbuf;
	char *path;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (scale >= 0.0, NULL);

	if (name[0] == '/') {
		path = g_strdup (name);
	} else {
		path = g_strdup_printf ("%s/%s", NEMO_DATADIR, name);
	}

	pixbuf = gdk_pixbuf_new_from_file (path, NULL);

	g_free (path);

	g_return_val_if_fail (pixbuf != NULL, NULL);
	
	if (scale != 1.0) {
		GdkPixbuf *scaled;
		float width = gdk_pixbuf_get_width (pixbuf) * scale;
		float height = gdk_pixbuf_get_width (pixbuf) * scale;

		scaled = gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);

		g_object_unref (pixbuf);

		g_return_val_if_fail (scaled != NULL, NULL);

		pixbuf = scaled;
	}

	return pixbuf;
}

GtkWidget *
test_label_new (const char *text,
		gboolean with_background,
		int num_sizes_larger)
{
	GtkWidget *label;

	if (text == NULL) {
		text = "Foo";
	}
	
	label = gtk_label_new (text);

	return label;
}

void 
test_window_set_title_with_pid (GtkWindow *window,
				const char *title)
{
	char *tmp;
	
	g_return_if_fail (GTK_IS_WINDOW (window));

	tmp = g_strdup_printf ("%lu: %s", (gulong) getpid (), title);
	gtk_window_set_title (GTK_WINDOW (window), tmp);
	g_free (tmp);
}


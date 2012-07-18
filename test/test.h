#ifndef TEST_H
#define TEST_H

#include <config.h>
#include <gtk/gtk.h>

#include <eel/eel-debug.h>
#include <eel/eel.h>
#include <libnemo-private/nemo-file-utilities.h>

void       test_init                            (int                         *argc,
						 char                      ***argv);
int        test_quit                            (int                          exit_code);
void       test_delete_event                    (GtkWidget                   *widget,
						 GdkEvent                    *event,
						 gpointer                     callback_data);
GtkWidget *test_window_new                      (const char                  *title,
						 guint                        border_width);
void       test_gtk_widget_set_background_image (GtkWidget                   *widget,
						 const char                  *image_name);
void       test_gtk_widget_set_background_color (GtkWidget                   *widget,
						 const char                  *color_spec);
GdkPixbuf *test_pixbuf_new_named                (const char                  *name,
						 float                        scale);
GtkWidget *test_label_new                       (const char                  *text,
						 gboolean                     with_background,
						 int                          num_sizes_larger);
void       test_pixbuf_draw_rectangle_tiled     (GdkPixbuf                   *pixbuf,
						 const char                  *tile_name,
						 int                          x0,
						 int                          y0,
						 int                          x1,
						 int                          y1,
						 int                          opacity);
void       test_window_set_title_with_pid       (GtkWindow                   *window,
						 const char                  *title);

#endif /* TEST_H */

/* nemo-simple-button.c */

#include <config.h>
#include "nemo-simple-button.h"
#include <glib.h>

struct _NemoSimpleButton
{
  GtkButton parent_object;
};

G_DEFINE_TYPE (NemoSimpleButton, nemo_simple_button, GTK_TYPE_BUTTON)

/**
 * SECTION:nemo-simple-button
 * @Title: NemoSimpleButton
 * @Short_description: A stripped down #GtkButton for embedding in menu items.
 *
 * This button class is intended to be used in a #NemoMenuItem to allow some
 * advanced functionality within a single item row.
 **/

static gboolean
nemo_simple_button_button_release (GtkWidget      *widget,
                                   GdkEventButton *event)
{
  g_signal_emit_by_name (GTK_BUTTON (widget), "released");

  return GDK_EVENT_PROPAGATE;
}

static void
nemo_simple_button_class_init (NemoSimpleButtonClass *klass)
{
  GtkWidgetClass *widget_class = (GtkWidgetClass*) klass;
  widget_class->button_release_event = nemo_simple_button_button_release;
}

static void
nemo_simple_button_init (NemoSimpleButton *self)
{
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_remove_class (context, GTK_STYLE_CLASS_BUTTON);
}

NemoSimpleButton *
nemo_simple_button_new (void)
{
  return g_object_new (NEMO_TYPE_SIMPLE_BUTTON, NULL);
}

NemoSimpleButton *
nemo_simple_button_new_from_icon_name (const gchar *icon_name, int icon_size)
{
  GtkWidget *w, *image;

  w = g_object_new (NEMO_TYPE_SIMPLE_BUTTON, NULL);

  image = gtk_image_new_from_icon_name (icon_name, icon_size);
  gtk_button_set_image (GTK_BUTTON (w), image);

  return NEMO_SIMPLE_BUTTON (w);
}

NemoSimpleButton *
nemo_simple_button_new_from_stock (const gchar *stock_id, int icon_size)
{
  GtkWidget *w, *image;

  w = g_object_new (NEMO_TYPE_SIMPLE_BUTTON, NULL);

  image = gtk_image_new_from_stock (stock_id, icon_size);
  gtk_button_set_image (GTK_BUTTON (w), image);

  return NEMO_SIMPLE_BUTTON (w);
}

NemoSimpleButton *
nemo_simple_button_new_from_file (const gchar *path, int icon_size)
{
  GtkWidget *w, *image;
  GdkPixbuf *pixbuf = NULL;
  cairo_surface_t *surface = NULL;
  gint width, height;
  gint scale = 1;

  gtk_icon_size_lookup (icon_size, &width, &height);

  w = g_object_new (NEMO_TYPE_SIMPLE_BUTTON, NULL);

  scale = gtk_widget_get_scale_factor (w);

  pixbuf = gdk_pixbuf_new_from_file_at_size (path, width * scale, height * scale, NULL);

  if (pixbuf) {
    surface = gdk_cairo_surface_create_from_pixbuf (pixbuf, scale, NULL);
    g_object_unref (pixbuf);
  }

  if (surface) {
    image = gtk_image_new ();
    g_object_set (image,
                  "surface", surface,
                  NULL);
    gtk_button_set_image (GTK_BUTTON (w), image);
  }

  return NEMO_SIMPLE_BUTTON (w);
}
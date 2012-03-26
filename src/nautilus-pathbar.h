/* nautilus-pathbar.h
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * 
 */

#ifndef NAUTILUS_PATHBAR_H
#define NAUTILUS_PATHBAR_H

#include <gtk/gtk.h>
#include <gio/gio.h>

typedef struct _NautilusPathBar      NautilusPathBar;
typedef struct _NautilusPathBarClass NautilusPathBarClass;


#define NAUTILUS_TYPE_PATH_BAR                 (nautilus_path_bar_get_type ())
#define NAUTILUS_PATH_BAR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_PATH_BAR, NautilusPathBar))
#define NAUTILUS_PATH_BAR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PATH_BAR, NautilusPathBarClass))
#define NAUTILUS_IS_PATH_BAR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_PATH_BAR))
#define NAUTILUS_IS_PATH_BAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PATH_BAR))
#define NAUTILUS_PATH_BAR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_PATH_BAR, NautilusPathBarClass))

struct _NautilusPathBar
{
	GtkContainer parent;

	GdkWindow *event_window;
 
	GFile *root_path;
	GFile *home_path;
	GFile *desktop_path;

	GFile *current_path;
	gpointer current_button_data;

	GList *button_list;
	GList *first_scrolled_button;
	GList *fake_root;
	GtkWidget *up_slider_button;
	GtkWidget *down_slider_button;
	guint settings_signal_id;
	gint icon_size;
	gint16 slider_width;
	gint16 spacing;
	gint16 button_offset;
	guint timer;
	guint slider_visible : 1;
	guint need_timer : 1;
	guint ignore_click : 1;

	unsigned int drag_slider_timeout;
	gboolean drag_slider_timeout_for_up_button;
};

struct _NautilusPathBarClass
{
	GtkContainerClass parent_class;

  	void (* path_clicked)   (NautilusPathBar  *path_bar,
				 GFile             *location);
  	void (* path_set)       (NautilusPathBar  *path_bar,
				 GFile             *location);
};

GType    nautilus_path_bar_get_type (void) G_GNUC_CONST;

gboolean nautilus_path_bar_set_path    (NautilusPathBar *path_bar, GFile *file);
GFile *  nautilus_path_bar_get_path_for_button (NautilusPathBar *path_bar,
						GtkWidget       *button);
void     nautilus_path_bar_clear_buttons (NautilusPathBar *path_bar);

#endif /* NAUTILUS_PATHBAR_H */

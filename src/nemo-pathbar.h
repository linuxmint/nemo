/* nemo-pathbar.h
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
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * 
 */

#ifndef NEMO_PATHBAR_H
#define NEMO_PATHBAR_H

#include <gtk/gtk.h>
#include <gio/gio.h>

typedef struct _NemoPathBar      NemoPathBar;
typedef struct _NemoPathBarClass NemoPathBarClass;
typedef struct _NemoPathBarDetails NemoPathBarDetails;

#define NEMO_TYPE_PATH_BAR                 (nemo_path_bar_get_type ())
#define NEMO_PATH_BAR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_PATH_BAR, NemoPathBar))
#define NEMO_PATH_BAR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_PATH_BAR, NemoPathBarClass))
#define NEMO_IS_PATH_BAR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_PATH_BAR))
#define NEMO_IS_PATH_BAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_PATH_BAR))
#define NEMO_PATH_BAR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_PATH_BAR, NemoPathBarClass))

struct _NemoPathBar
{
	GtkContainer parent;
	
	NemoPathBarDetails *priv;
};

struct _NemoPathBarClass
{
	GtkContainerClass parent_class;

  	void (* path_clicked)   (NemoPathBar  *path_bar,
				 GFile             *location);
  	void (* path_set)       (NemoPathBar  *path_bar,
				 GFile             *location);
};

GType    nemo_path_bar_get_type (void) G_GNUC_CONST;

gboolean nemo_path_bar_set_path    (NemoPathBar *path_bar, GFile *file);
GFile *  nemo_path_bar_get_path_for_button (NemoPathBar *path_bar,
						GtkWidget       *button);
void     nemo_path_bar_clear_buttons (NemoPathBar *path_bar);

#endif /* NEMO_PATHBAR_H */

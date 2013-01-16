/* nemo-statusbar.h
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

#ifndef NEMO_STATUSBAR_H
#define NEMO_STATUSBAR_H

#include <gtk/gtk.h>
#include <gio/gio.h>
#include "nemo-window.h"
#include "nemo-window-slot.h"
#include "nemo-view.h"

typedef struct _NemoStatusBar      NemoStatusBar;
typedef struct _NemoStatusBarClass NemoStatusBarClass;


#define NEMO_TYPE_STATUS_BAR                 (nemo_status_bar_get_type ())
#define NEMO_STATUS_BAR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_STATUS_BAR, NemoStatusBar))
#define NEMO_STATUS_BAR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_STATUS_BAR, NemoStatusBarClass))
#define NEMO_IS_STATUS_BAR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_STATUS_BAR))
#define NEMO_IS_STATUS_BAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_STATUS_BAR))
#define NEMO_STATUS_BAR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_STATUS_BAR, NemoStatusBarClass))

#define NEMO_STATUSBAR_ICON_SIZE_NAME "statusbar-icon"
#define NEMO_STATUSBAR_ICON_SIZE 11

struct _NemoStatusBar
{
    GtkBox parent;
    NemoWindow *window;
    GtkWidget *real_statusbar;

    GtkWidget *zoom_slider;

    GtkWidget *tree_button;
    GtkWidget *places_button;
    GtkWidget *show_button;
    GtkWidget *hide_button;
    GtkWidget *separator;
};

struct _NemoStatusBarClass
{
    GtkBoxClass parent_class;
};

GType    nemo_status_bar_get_type (void) G_GNUC_CONST;

GtkWidget *nemo_status_bar_new (NemoWindow *window);

GtkWidget *nemo_status_bar_get_real_statusbar (NemoStatusBar *bar);

void       nemo_status_bar_sync_button_states (NemoStatusBar *bar);

void       nemo_status_bar_sync_zoom_widgets (NemoStatusBar *bar);

#endif /* NEMO_STATUSBAR_H */

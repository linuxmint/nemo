/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __NEMO_CONTEXT_MENU_MENU_ITEM_H__
#define __NEMO_CONTEXT_MENU_MENU_ITEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NEMO_TYPE_CONTEXT_MENU_MENU_ITEM            (nemo_context_menu_menu_item_get_type ())
#define NEMO_CONTEXT_MENU_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_CONTEXT_MENU_MENU_ITEM, NemoContextMenuMenuItem))
#define NEMO_CONTEXT_MENU_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_CONTEXT_MENU_MENU_ITEM, NemoContextMenuMenuItemClass))
#define NEMO_IS_CONTEXT_MENU_MENU_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_CONTEXT_MENU_MENU_ITEM))
#define NEMO_IS_CONTEXT_MENU_MENU_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_CONTEXT_MENU_MENU_ITEM))
#define NEMO_CONTEXT_MENU_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_CONTEXT_MENU_MENU_ITEM, NemoContextMenuMenuItemClass))


typedef struct _NemoContextMenuMenuItem       NemoContextMenuMenuItem;
typedef struct _NemoContextMenuMenuItemClass  NemoContextMenuMenuItemClass;

struct _NemoContextMenuMenuItem
{
  GtkImageMenuItem menu_item;

  gchar *label;
  GtkWidget *stack;
  GtkWidget *label_widget;
  GtkWidget *toggle_label_widget;
  GtkWidget *toggle_widget;

  gulong settings_monitor_id;
  gboolean on_toggle;
};

/**
 * NemoContextMenuMenuItemClass:
 * @parent_class: The parent class.
 */
struct _NemoContextMenuMenuItemClass
{
  GtkImageMenuItemClass parent_class;
};

GType      nemo_context_menu_menu_item_get_type    (void) G_GNUC_CONST;
GtkWidget *nemo_context_menu_menu_item_new             (GtkWidget *widget);

G_END_DECLS

#endif /* __NEMO_CONTEXT_MENU_MENU_ITEM_H__ */

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

#ifndef __NEMO_WIDGET_MENU_ITEM_H__
#define __NEMO_WIDGET_MENU_ITEM_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NEMO_TYPE_WIDGET_MENU_ITEM            (nemo_widget_menu_item_get_type ())
#define NEMO_WIDGET_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_WIDGET_MENU_ITEM, NemoWidgetMenuItem))
#define NEMO_WIDGET_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_WIDGET_MENU_ITEM, NemoWidgetMenuItemClass))
#define NEMO_IS_WIDGET_MENU_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_WIDGET_MENU_ITEM))
#define NEMO_IS_WIDGET_MENU_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_WIDGET_MENU_ITEM))
#define NEMO_WIDGET_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_WIDGET_MENU_ITEM, NemoWidgetMenuItemClass))


typedef struct _NemoWidgetMenuItem       NemoWidgetMenuItem;
typedef struct _NemoWidgetMenuItemClass  NemoWidgetMenuItemClass;

struct _NemoWidgetMenuItem
{
  GtkMenuItem menu_item;

  GtkWidget *child_widget;
  gint action_slot;

  /* GtkActivatable */
  GtkAction *related_action;
  gboolean use_action_appearance;
};

/**
 * NemoWidgetMenuItemClass:
 * @parent_class: The parent class.
 */
struct _NemoWidgetMenuItemClass
{
  GtkMenuItemClass parent_class;
};

GType      nemo_widget_menu_item_get_type    (void) G_GNUC_CONST;
GtkWidget* nemo_widget_menu_item_new             (GtkWidget *widget);

void       nemo_widget_menu_item_set_child_widget (NemoWidgetMenuItem *widget_menu_item,
                                                   GtkWidget          *widget);

GtkWidget* nemo_widget_menu_item_get_child_widget (NemoWidgetMenuItem *widget_menu_item);

G_END_DECLS

#endif /* __NEMO_WIDGET_MENU_ITEM_H__ */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

*/

#include "nemo-separator-action.h"

G_DEFINE_TYPE (NemoSeparatorAction, nemo_separator_action,
	           GTK_TYPE_ACTION);

static GtkWidget *create_menu_item    (GtkAction *action);
static GtkWidget *create_tool_item    (GtkAction *action);

static gpointer parent_class;

static void
nemo_separator_action_init (NemoSeparatorAction *action)
{
}

static void
nemo_separator_action_class_init (NemoSeparatorActionClass *klass)
{
    GtkActionClass       *action_class = GTK_ACTION_CLASS (klass);
    parent_class           = g_type_class_peek_parent (klass);

    action_class->create_menu_item  = create_menu_item;
    action_class->create_tool_item  = create_tool_item;

    action_class->menu_item_type = GTK_TYPE_SEPARATOR_MENU_ITEM;
    action_class->toolbar_item_type = GTK_TYPE_SEPARATOR_TOOL_ITEM;
}

GtkAction *
nemo_separator_action_new (const gchar *name)
{
    return g_object_new (NEMO_TYPE_SEPARATOR_ACTION,
                         "name", name,
                         NULL);
}

static GtkWidget *
create_menu_item (GtkAction *action)
{
  GType menu_item_type;
  GtkWidget *ret;

  menu_item_type = GTK_ACTION_GET_CLASS (action)->menu_item_type;

  ret = g_object_new (menu_item_type,
                      NULL);

  gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (ret), FALSE);
  return ret;
}

static GtkWidget *
create_tool_item (GtkAction *action)
{
  g_warning ("NemoSeparatorAction: Toolbar items unsupported at this time.");
  return NULL;
}


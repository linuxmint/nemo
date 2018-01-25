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

#include "nemo-widget-action.h"
#include "nemo-widget-menu-item.h"
#include "nemo-context-menu-menu-item.h"

G_DEFINE_TYPE (NemoWidgetAction, nemo_widget_action,
	       GTK_TYPE_ACTION);

static void     nemo_widget_action_get_property  (GObject                    *object,
                                           guint                       param_id,
                                           GValue                     *value,
                                           GParamSpec                 *pspec);

static void     nemo_widget_action_set_property  (GObject                    *object,
                                           guint                       param_id,
                                           const GValue               *value,
                                           GParamSpec                 *pspec);

static void     nemo_widget_action_constructed (GObject *object);

static void     nemo_widget_action_finalize (GObject *gobject);
static void     nemo_widget_action_dispose (GObject *gobject);

static GtkWidget *create_menu_item    (GtkAction *action);

static GtkWidget *create_tool_item    (GtkAction *action);

static gpointer parent_class;

enum 
{
  PROP_0,
  PROP_WIDGET_A,
  PROP_WIDGET_B
};

static void
nemo_widget_action_init (NemoWidgetAction *action)
{
    action->widget_a = NULL;
    action->widget_b = NULL;
    action->a_used = FALSE;
    action->b_used = FALSE;
    action->is_menu_toggle = FALSE;
}

static void
nemo_widget_action_class_init (NemoWidgetActionClass *klass)
{
    GObjectClass         *object_class = G_OBJECT_CLASS(klass);
    GtkActionClass       *action_class = GTK_ACTION_CLASS (klass);
    parent_class           = g_type_class_peek_parent (klass);
    object_class->finalize = nemo_widget_action_finalize;
    object_class->dispose = nemo_widget_action_dispose;
    object_class->set_property = nemo_widget_action_set_property;
    object_class->get_property = nemo_widget_action_get_property;
    object_class->constructed = nemo_widget_action_constructed;
    action_class->create_menu_item  = create_menu_item;
    action_class->create_tool_item  = create_tool_item;

    action_class->menu_item_type = NEMO_TYPE_WIDGET_MENU_ITEM;
    action_class->toolbar_item_type = G_TYPE_NONE;

    g_object_class_install_property (object_class,
                                     PROP_WIDGET_A,
                                     g_param_spec_object ("widget-a",
                                                          "The widget A for this action",
                                                          "The widget to use for this action",
                                                          GTK_TYPE_WIDGET,
                                                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
                                     );

    g_object_class_install_property (object_class,
                                     PROP_WIDGET_B,
                                     g_param_spec_object ("widget-b",
                                                          "The widget B for this action",
                                                          "The widget to use for this action",
                                                          GTK_TYPE_WIDGET,
                                                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
                                     );
}

void
nemo_widget_action_constructed (GObject *object)
{
    G_OBJECT_CLASS (parent_class)->constructed (object);
}

GtkAction *
nemo_widget_action_new (const gchar *name, 
                          GtkWidget *widget_a,
                          GtkWidget *widget_b)
{
    return g_object_new (NEMO_TYPE_WIDGET_ACTION,
                         "name", name,
                         "widget-a", widget_a,
                         "widget-b", widget_b,
                         NULL);
}

GtkAction *
nemo_widget_action_new_for_menu_toggle (const gchar *name,
                                        const gchar *label,
                                        const gchar *tooltip)
{
    GtkAction *ret = g_object_new (NEMO_TYPE_WIDGET_ACTION,
                                   "name", name,
                                   "label", label,
                                   "tooltip", tooltip,
                                   NULL);

    NEMO_WIDGET_ACTION (ret)->is_menu_toggle = TRUE;

    return ret;
}

static void
nemo_widget_action_finalize (GObject *object)
{
    G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nemo_widget_action_dispose (GObject *object)
{
    NemoWidgetAction *action = NEMO_WIDGET_ACTION (object);

    if (action->widget_a) {
        g_object_unref (action->widget_a);

        nemo_widget_action_set_widget_a (action, NULL);
    }
    if (action->widget_b) {
        g_object_unref (action->widget_b);

        nemo_widget_action_set_widget_b (action, NULL);
    }

    G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
nemo_widget_action_set_property (GObject         *object,
                          guint            prop_id,
                          const GValue    *value,
                          GParamSpec      *pspec)
{
  NemoWidgetAction *action;
  
  action = NEMO_WIDGET_ACTION (object);

  switch (prop_id)
    {
    case PROP_WIDGET_A:
      nemo_widget_action_set_widget_a (action, g_value_get_object (value));
      break;
    case PROP_WIDGET_B:
      action->widget_b = g_value_get_object (value);
      g_object_ref_sink (action->widget_b);
      g_object_notify (G_OBJECT (action), "widget-b");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
nemo_widget_action_get_property (GObject    *object,
             guint       prop_id,
             GValue     *value,
             GParamSpec *pspec)
{
  NemoWidgetAction *action;

  action = NEMO_WIDGET_ACTION (object);

  switch (prop_id)
    {
    case PROP_WIDGET_A:
      g_value_set_object (value, action->widget_a);
      break;
    case PROP_WIDGET_B:
      g_value_set_object (value, action->widget_b);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GtkWidget *
create_menu_item (GtkAction *action)
{
  NemoWidgetAction *widget_action;
  GType menu_item_type;
  GtkWidget *w;
  GtkWidget *ret = NULL;
  gint slot = -1;

  widget_action = NEMO_WIDGET_ACTION (action);
  menu_item_type = GTK_ACTION_GET_CLASS (action)->menu_item_type;

  if (widget_action->is_menu_toggle) {
    ret = g_object_new (NEMO_TYPE_CONTEXT_MENU_MENU_ITEM, NULL);
  } else {
    if (!widget_action->a_used) {
      w = widget_action->widget_a;
      widget_action->a_used = TRUE;
      slot = ACTION_SLOT_A;
    } else if (!widget_action->b_used) {
      w = widget_action->widget_b;
      widget_action->b_used = TRUE;
      slot = ACTION_SLOT_B;
    }

    if (slot != -1)
      ret = g_object_new (menu_item_type,
                          "child-widget", w,
                          "action-slot", slot,
                          NULL);
  }

  if (ret)
    gtk_activatable_set_related_action (GTK_ACTIVATABLE (ret), action);

  return ret;
}

static GtkWidget *
create_tool_item (GtkAction *action)
{
  g_warning ("NemoWidgetAction: Toolbar items unsupported at this time.");
  return NULL;
}

void
nemo_widget_action_activate (NemoWidgetAction *action)
{

}

GtkWidget *
nemo_widget_action_get_widget_a (NemoWidgetAction *action)
{
  if (action->widget_a)
    return action->widget_a;
  else
    return NULL;
}

void
nemo_widget_action_set_widget_a (NemoWidgetAction *action, GtkWidget *widget)
{
    action->widget_a = widget;

    if (widget)
        g_object_ref_sink (action->widget_a);

    g_object_notify (G_OBJECT (action), "widget-a");
}

GtkWidget *
nemo_widget_action_get_widget_b (NemoWidgetAction *action)
{
  if (action->widget_b)
    return action->widget_b;
  else
    return NULL;
}

void
nemo_widget_action_set_widget_b (NemoWidgetAction *action, GtkWidget *widget)
{
    action->widget_b = widget;

    if (widget)
        g_object_ref_sink (action->widget_b);

    g_object_notify (G_OBJECT (action), "widget-b");
}

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

#include "nemo-widget-menu-item.h"
#include "nemo-widget-action.h"
#include <string.h>

enum {
  PROP_0,
  PROP_CHILD_WIDGET,
  PROP_ACTION_SLOT,
  PROP_ACTIVATABLE_RELATED_ACTION,
  PROP_ACTIVATABLE_USE_ACTION_APPEARANCE,
};

static GtkActivatableIface *parent_activatable_iface;

static void nemo_widget_menu_item_dispose              (GObject *object);

static void nemo_widget_menu_item_set_property         (GObject         *object,
                                                      guint            prop_id,
                                                      const GValue    *value,
                                                      GParamSpec      *pspec);
static void nemo_widget_menu_item_get_property         (GObject         *object,
                                                      guint            prop_id,
                                                      GValue          *value,
                                                      GParamSpec      *pspec);

static void nemo_widget_menu_item_activatable_interface_init (GtkActivatableIface  *iface);

static void nemo_widget_menu_item_update                     (GtkActivatable       *activatable,
                                                            GtkAction            *action,
                                                            const gchar          *property_name);
static void nemo_widget_menu_item_sync_action_properties     (GtkActivatable       *activatable,
                                                            GtkAction            *action);

static void nemo_widget_menu_item_set_related_action         (NemoWidgetMenuItem  *menu_item, 
                                                              GtkAction           *action);

static void nemo_widget_menu_item_set_use_action_appearance (NemoWidgetMenuItem *widget_menu_item,
                                                             gboolean use_appearance);

static gboolean activatable_update_child_widget (NemoWidgetMenuItem *widget_menu_item, GtkAction *action);

static void nemo_widget_menu_item_realize (GtkWidget *widget);
static void nemo_widget_menu_item_unrealize (GtkWidget *widget);
static void nemo_widget_menu_item_map (GtkWidget *widget);
static void nemo_widget_menu_item_unmap (GtkWidget *widget);
static void nemo_widget_menu_item_size_allocate (GtkWidget *widget,
                                            GtkAllocation *allocation);

static void nemo_widget_menu_item_activate (GtkMenuItem *menu_item);

G_DEFINE_TYPE_WITH_CODE (NemoWidgetMenuItem, nemo_widget_menu_item, GTK_TYPE_MENU_ITEM,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
                                                nemo_widget_menu_item_activatable_interface_init));

static void
nemo_widget_menu_item_class_init (NemoWidgetMenuItemClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass*) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass*) klass;
  GtkMenuItemClass *menu_item_class = (GtkMenuItemClass*) klass;

  gobject_class->dispose = nemo_widget_menu_item_dispose;
  gobject_class->set_property = nemo_widget_menu_item_set_property;
  gobject_class->get_property = nemo_widget_menu_item_get_property;

  menu_item_class->activate = nemo_widget_menu_item_activate;

  widget_class->realize = nemo_widget_menu_item_realize;
  widget_class->unrealize = nemo_widget_menu_item_unrealize;
  widget_class->map = nemo_widget_menu_item_map;
  widget_class->unmap = nemo_widget_menu_item_unmap;
  widget_class->size_allocate = nemo_widget_menu_item_size_allocate;

  /**
   * NemoWidgetMenuItem:widget:
   *
   * Child widget to use in place of a label.
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CHILD_WIDGET,
                                   g_param_spec_object ("child-widget",
                                                        "Child widget",
                                                        "Child widget to appear next to the menu text",
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READWRITE));


  /**
   * NemoWidgetMenuItem:action-slot:
   *
   * The action widget slot the child-widget came from.
   *
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ACTION_SLOT,
                                   g_param_spec_int ("action-slot",
                                                     "Action slot",
                                                     "The action's widget slot child-widget came from",
                                                     -1, 1, -1,
                                                     G_PARAM_READWRITE)
                                   );

  g_object_class_override_property (gobject_class, PROP_ACTIVATABLE_RELATED_ACTION, "related-action");
  g_object_class_override_property (gobject_class, PROP_ACTIVATABLE_USE_ACTION_APPEARANCE, "use-action-appearance");
}

static void 
nemo_widget_menu_item_init (NemoWidgetMenuItem *item)
{
    item->child_widget = NULL;
    item->action_slot = -1;
}

static void
nemo_widget_menu_item_dispose (GObject *object)
{
  NemoWidgetMenuItem *widget_menu_item = NEMO_WIDGET_MENU_ITEM (object);

  if (widget_menu_item->related_action)
    {
      gtk_action_disconnect_accelerator (widget_menu_item->related_action);
      nemo_widget_menu_item_set_related_action (widget_menu_item, NULL);
      widget_menu_item->related_action = NULL;
    }

  G_OBJECT_CLASS (nemo_widget_menu_item_parent_class)->dispose (object);
}

static void
nemo_widget_menu_item_set_property (GObject         *object,
                                  guint            prop_id,
                                  const GValue    *value,
                                  GParamSpec      *pspec)
{
  NemoWidgetMenuItem *widget_menu_item = NEMO_WIDGET_MENU_ITEM (object);

  switch (prop_id)
    {
    case PROP_CHILD_WIDGET:
      nemo_widget_menu_item_set_child_widget (widget_menu_item, (GtkWidget *) g_value_get_object (value));
      break;
    case PROP_ACTION_SLOT:
      widget_menu_item->action_slot = g_value_get_int (value);
      break;
    case PROP_ACTIVATABLE_RELATED_ACTION:
      nemo_widget_menu_item_set_related_action (widget_menu_item, g_value_get_object (value));
      break;
    case PROP_ACTIVATABLE_USE_ACTION_APPEARANCE:
      nemo_widget_menu_item_set_use_action_appearance (widget_menu_item, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
nemo_widget_menu_item_get_property (GObject         *object,
                                  guint            prop_id,
                                  GValue          *value,
                                  GParamSpec      *pspec)
{
  NemoWidgetMenuItem *widget_menu_item = NEMO_WIDGET_MENU_ITEM (object);

  switch (prop_id)
    {
    case PROP_CHILD_WIDGET:
      g_value_set_object (value, nemo_widget_menu_item_get_child_widget (widget_menu_item));
      break;
    case PROP_ACTION_SLOT:
      g_value_set_int (value, widget_menu_item->action_slot);
      break;
    case PROP_ACTIVATABLE_RELATED_ACTION:
      g_value_set_object (value, widget_menu_item->related_action);
      break;
    case PROP_ACTIVATABLE_USE_ACTION_APPEARANCE:
      g_value_set_boolean (value, widget_menu_item->use_action_appearance);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
nemo_widget_menu_item_activatable_interface_init (GtkActivatableIface  *iface)
{
  parent_activatable_iface = g_type_interface_peek_parent (iface);
  iface->update = nemo_widget_menu_item_update;
  iface->sync_action_properties = nemo_widget_menu_item_sync_action_properties;
}

static gboolean
activatable_update_child_widget (NemoWidgetMenuItem *widget_menu_item, GtkAction *action)
{
  GtkWidget   *widget, *action_widget;
  NemoWidgetAction *widget_action = NEMO_WIDGET_ACTION (action);

  widget = nemo_widget_menu_item_get_child_widget (widget_menu_item);

  if (widget_menu_item->action_slot == ACTION_SLOT_A) {
    action_widget = nemo_widget_action_get_widget_a (widget_action);
  } else if (widget_menu_item->action_slot == ACTION_SLOT_B) {
    action_widget = nemo_widget_action_get_widget_b (widget_action);
  } else
    return FALSE;

  if (widget && action_widget && (widget == action_widget)) {
    return FALSE;
  }

  if (action_widget) {
    nemo_widget_menu_item_set_child_widget (widget_menu_item, action_widget);
    return TRUE;
  }

  if (!action_widget) {
    nemo_widget_menu_item_set_child_widget (widget_menu_item, NULL);
    return TRUE;
  }

  return FALSE;
}


static void
nemo_widget_menu_item_update (GtkActivatable *activatable,
                            GtkAction      *action,
                            const gchar    *property_name)
{
  NemoWidgetMenuItem *widget_menu_item;
  widget_menu_item = NEMO_WIDGET_MENU_ITEM (activatable);

  if (!gtk_activatable_get_use_action_appearance (activatable))
    return;

  if (strcmp (property_name, "widget-a") == 0)
    activatable_update_child_widget (widget_menu_item, action);
  else if (strcmp (property_name, "widget-b") == 0)
    activatable_update_child_widget (widget_menu_item, action);
}

static void
nemo_widget_menu_item_sync_action_properties (GtkActivatable *activatable,
                                            GtkAction      *action)
{
  NemoWidgetMenuItem *widget_menu_item;
  GtkWidget *widget;

  widget_menu_item = NEMO_WIDGET_MENU_ITEM (activatable);

  // parent_activatable_iface->sync_action_properties (activatable, action);
  if (!action)
    return;

  if (!gtk_activatable_get_use_action_appearance (activatable))
    return;

  widget = nemo_widget_menu_item_get_child_widget (widget_menu_item);
  if (widget && !GTK_IS_WIDGET (widget))
    {
      nemo_widget_menu_item_set_child_widget (widget_menu_item, NULL);
      widget = NULL;
    }

  activatable_update_child_widget (widget_menu_item, action);

  gtk_widget_show (GTK_WIDGET (widget_menu_item));
}

static void
nemo_widget_menu_item_set_related_action (NemoWidgetMenuItem *widget_menu_item,
                                          GtkAction          *action)
{
    if (widget_menu_item->related_action == action)
      return;

    if (widget_menu_item->related_action)
      {
        gtk_action_disconnect_accelerator (widget_menu_item->related_action);
      }

    if (action)
      {
        const gchar *accel_path;

        accel_path = gtk_action_get_accel_path (action);
        if (accel_path)
          {
            gtk_action_connect_accelerator (action);
            gtk_menu_item_set_accel_path (GTK_MENU_ITEM (widget_menu_item), accel_path);
          }
      }
    gtk_activatable_do_set_related_action (GTK_ACTIVATABLE (widget_menu_item), action);

    nemo_widget_menu_item_sync_action_properties (GTK_ACTIVATABLE (widget_menu_item), action);

    widget_menu_item->related_action = action;
}

static void
nemo_widget_menu_item_set_use_action_appearance (NemoWidgetMenuItem *widget_menu_item,
                                                        gboolean     use_appearance)
{
    if (widget_menu_item->use_action_appearance != use_appearance)
      {
        widget_menu_item->use_action_appearance = use_appearance;
        gtk_activatable_sync_action_properties (GTK_ACTIVATABLE (widget_menu_item),
                                                widget_menu_item->related_action);
      }
}

static void
nemo_widget_menu_item_activate (GtkMenuItem *menu_item)
{
}

static void
nemo_widget_menu_item_realize (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (g_type_class_peek_parent (nemo_widget_menu_item_parent_class))->realize (widget);
}

static void
nemo_widget_menu_item_unrealize (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (g_type_class_peek_parent (nemo_widget_menu_item_parent_class))->unrealize (widget);
}

static void
nemo_widget_menu_item_map (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (g_type_class_peek_parent (nemo_widget_menu_item_parent_class))->map (widget);
}

static void
nemo_widget_menu_item_unmap (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (g_type_class_peek_parent (nemo_widget_menu_item_parent_class))->unmap (widget);
}


static void
get_arrow_size (GtkWidget *widget,
                GtkWidget *child,
                gint      *size,
                gint      *spacing)
{
  PangoContext     *context;
  PangoFontMetrics *metrics;
  gfloat            arrow_scaling;
  gint              arrow_spacing;

  g_assert (size);

  gtk_widget_style_get (widget,
                        "arrow-scaling", &arrow_scaling,
                        "arrow-spacing", &arrow_spacing,
                        NULL);

  if (spacing != NULL)
    *spacing = arrow_spacing;

  context = gtk_widget_get_pango_context (child);

  metrics = pango_context_get_metrics (context,
                                       pango_context_get_font_description (context),
                                       pango_context_get_language (context));

  *size = (PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) +
                         pango_font_metrics_get_descent (metrics)));

  pango_font_metrics_unref (metrics);

  *size = *size * arrow_scaling;
}

static gint
get_toggle_size (GtkMenuItem *menu_item)
{
  NemoWidgetMenuItem *widget_menu_item = NEMO_WIDGET_MENU_ITEM (menu_item);
  GtkPackDirection pack_dir;
  GtkWidget *parent;
  gint req = 0;
  GtkWidget *widget = GTK_WIDGET (menu_item);

  parent = gtk_widget_get_parent (widget);

  if (GTK_IS_MENU_BAR (parent))
    pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (parent));
  else
    pack_dir = GTK_PACK_DIRECTION_LTR;

  if (widget_menu_item->child_widget && gtk_widget_get_visible (widget_menu_item->child_widget))
    {
      gint icon_size;
      guint toggle_spacing;

      gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_size, NULL);

      gtk_widget_style_get (GTK_WIDGET (menu_item),
                            "toggle-spacing", &toggle_spacing,
                            NULL);

      if (pack_dir == GTK_PACK_DIRECTION_LTR || pack_dir == GTK_PACK_DIRECTION_RTL)
        {
          if (icon_size > 0)
            req = icon_size + toggle_spacing;
        }
      else
        {
          if (icon_size > 0)
            req = icon_size + toggle_spacing;
        }
    }

    return req;
}


static void
nemo_widget_menu_item_size_allocate (GtkWidget     *widget,
                                     GtkAllocation *allocation)
{
  GtkBin *bin;
  GtkAllocation child_allocation;
  GtkTextDirection direction;
  GtkPackDirection child_pack_dir;
  GtkWidget *child;
  GtkWidget *parent;

  g_return_if_fail (GTK_IS_MENU_ITEM (widget));
  g_return_if_fail (allocation != NULL);

  bin = GTK_BIN (widget);

  direction = gtk_widget_get_direction (widget);

  parent = gtk_widget_get_parent (widget);
  if (GTK_IS_MENU_BAR (parent))
    {
      child_pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (parent));
    }
  else
    {
      child_pack_dir = GTK_PACK_DIRECTION_LTR;
    }

  gtk_widget_set_allocation (widget, allocation);

  child = gtk_bin_get_child (bin);
  if (child)
    {
      GtkStyleContext *context;
      GtkStateFlags state;
      GtkBorder padding;
      guint border_width;
      gint toggle_size;

      context = gtk_widget_get_style_context (widget);
      state = gtk_widget_get_state_flags (widget);
      gtk_style_context_get_padding (context, state, &padding);

      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
      child_allocation.x = border_width + padding.left;
      child_allocation.y = border_width + padding.top;

      child_allocation.width = allocation->width - (border_width * 2) -
        padding.left - padding.right;
      child_allocation.height = allocation->height - (border_width * 2) -
        padding.top - padding.bottom;

      toggle_size = get_toggle_size (GTK_MENU_ITEM (widget));

      if (child_pack_dir == GTK_PACK_DIRECTION_LTR ||
          child_pack_dir == GTK_PACK_DIRECTION_RTL)
        {
          if ((direction == GTK_TEXT_DIR_LTR) == (child_pack_dir != GTK_PACK_DIRECTION_RTL))
            child_allocation.x += toggle_size;
          child_allocation.width -= toggle_size;
        }
      else
        {
          if ((direction == GTK_TEXT_DIR_LTR) == (child_pack_dir != GTK_PACK_DIRECTION_BTT))
            child_allocation.y += toggle_size;
          child_allocation.height -= toggle_size;
        }

      child_allocation.x += allocation->x;
      child_allocation.y += allocation->y;

      if ((gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget)) && !GTK_IS_MENU_BAR (parent)) ||
          gtk_menu_item_get_reserve_indicator (GTK_MENU_ITEM (widget)))
    {
      gint arrow_spacing, arrow_size;

      get_arrow_size (widget, child, &arrow_size, &arrow_spacing);

      if (direction == GTK_TEXT_DIR_RTL)
        child_allocation.x += arrow_size + arrow_spacing;
      child_allocation.width -= arrow_size + arrow_spacing;
    }

      if (child_allocation.width < 1)
        child_allocation.width = 1;

      gtk_widget_size_allocate (child, &child_allocation);
    }

  if (gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget)))
    gtk_menu_reposition (GTK_MENU (gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget))));
}


/**
 * nemo_widget_menu_item_new:
 * @widget: The custom widget to use
 *
 * Creates a new #NemoWidgetMenuItem.
 *
 * Returns: a new #NemoWidgetMenuItem.
 */
GtkWidget *
nemo_widget_menu_item_new (GtkWidget *widget)
{
  return g_object_new (NEMO_TYPE_WIDGET_MENU_ITEM,
                       "widget", widget,
                       NULL);
}

/**
 * nemo_widget_menu_item_set_child_widget:
 * @widget_menu_item: a #GtkNemoWidgetMenuItem.
 * @widget: (allow-none): a widget to set as the child widget for the menu item.
 *
 * Sets the child-widget of @widget_menu_item to the given widget.
 *
 */
void
nemo_widget_menu_item_set_child_widget (NemoWidgetMenuItem *widget_menu_item,
                               GtkWidget        *widget)
{
  g_return_if_fail (NEMO_IS_WIDGET_MENU_ITEM (widget_menu_item));

  GtkWidget *old_w;

  old_w = gtk_bin_get_child (GTK_BIN (widget_menu_item));

  if (widget == widget_menu_item->child_widget && widget == old_w)
    return;

  if (old_w)
    gtk_container_remove (GTK_CONTAINER (widget_menu_item),
                          old_w);

  widget_menu_item->child_widget = widget;

  if (widget == NULL)
    return;

  if (!gtk_widget_get_parent (widget))
    gtk_container_add (GTK_CONTAINER (widget_menu_item), widget);

  g_object_set (widget,
                "visible", TRUE,
                NULL);

  g_object_notify (G_OBJECT (widget_menu_item), "child-widget");
}

/**
 * nemo_widget_menu_item_get_child_widget:
 * @widget_menu_item: a #NemoWidgetMenuItem
 *
 * Gets the widget that is currently set as the child-widget of @widget_menu_item.
 * See nemo_widget_menu_item_set_child_widget().
 *
 * Returns: (transfer none): the widget set as child-widget of @widget_menu_item
 *
 * Deprecated: 3.10
 **/
GtkWidget*
nemo_widget_menu_item_get_child_widget (NemoWidgetMenuItem *widget_menu_item)
{
  g_return_val_if_fail (NEMO_IS_WIDGET_MENU_ITEM (widget_menu_item), NULL);

  return widget_menu_item->child_widget;
}


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
#include <config.h>

#include "nemo-global-preferences.h"
#include "nemo-context-menu-menu-item.h"
#include "nemo-widget-action.h"

#include <glib/gi18n.h>

static GtkActivatableIface *parent_activatable_iface;

static void nemo_context_menu_menu_item_dispose              (GObject *object);

static gboolean nemo_context_menu_menu_item_enter      (GtkWidget        *widget,
                                          GdkEventCrossing *event);
static gboolean nemo_context_menu_menu_item_leave      (GtkWidget        *widget,
                                          GdkEventCrossing *event);
static gboolean nemo_context_menu_menu_item_motion     (GtkWidget        *widget,
                                          GdkEventMotion   *event);

static gboolean nemo_context_menu_menu_item_button_press (GtkWidget      *widget,
                                                          GdkEventButton *event);

static gboolean nemo_context_menu_menu_item_button_release (GtkWidget      *widget,
                                                            GdkEventButton *event);

static void nemo_context_menu_menu_item_set_label (GtkMenuItem      *menu_item,
                                                   const gchar      *label);

static void nemo_context_menu_menu_item_activatable_interface_init (GtkActivatableIface  *iface);

G_DEFINE_TYPE_WITH_CODE (NemoContextMenuMenuItem, nemo_context_menu_menu_item, GTK_TYPE_IMAGE_MENU_ITEM,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIVATABLE,
                                                nemo_context_menu_menu_item_activatable_interface_init));

static void
set_action_image_temporary_visibility (NemoContextMenuMenuItem *item,
                                       gboolean                 visible)
{
    GtkWidget *image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (item));

    if (!image) {
        return;
    }

    if (!visible) {
        gtk_widget_set_visible (image,
                                FALSE);
    } else {
        gtk_widget_set_visible (image,
                                gtk_image_menu_item_get_always_show_image (GTK_IMAGE_MENU_ITEM (item)));
    }
}

static void
update_toggle_state (NemoContextMenuMenuItem *item,
                     gboolean                 from_event,
                     gboolean                 on_item)
{
    gboolean complex_mode = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_CONTEXT_MENUS_SHOW_ALL_ACTIONS);

    /* const */ gchar *tip_text = complex_mode ? _("Show fewer actions") :
                                                 _("Show more actions");

    gchar *markup = g_strdup_printf ("<small><i>%s</i></small>", tip_text);

    gtk_label_set_markup (GTK_LABEL (item->toggle_label_widget), markup);

    g_free (markup);

    if (item->on_toggle) {
        set_action_image_temporary_visibility (item, FALSE);
        gtk_stack_set_visible_child_name (GTK_STACK (item->stack), "toggle");
    } else {
        set_action_image_temporary_visibility (item, TRUE);
        gtk_stack_set_visible_child_name (GTK_STACK (item->stack), "action");
    }

    GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (item));

    if (on_item) {
        gtk_image_set_from_icon_name (GTK_IMAGE (item->toggle_widget),
                                      complex_mode ? "collapse-menu-hover-symbolic" : "expand-menu-hover-symbolic",
                                      GTK_ICON_SIZE_MENU);
    } else {
        gtk_image_set_from_icon_name (GTK_IMAGE (item->toggle_widget),
                                      complex_mode ? "collapse-menu-symbolic" : "expand-menu-symbolic",
                                      GTK_ICON_SIZE_MENU);
    }

    GtkStateFlags default_item_state = from_event ? GTK_STATE_FLAG_PRELIGHT : gtk_style_context_get_state (context);

    gtk_style_context_set_state (context, item->on_toggle ? GTK_STATE_FLAG_NORMAL : default_item_state);

    gtk_widget_queue_draw (GTK_WIDGET (item));
}

static void
update_toggle_appearance_from_event (GtkWidget *widget,
                                     gint       x,
                                     gint       y,
                                     gboolean   on_item)
{
    NemoContextMenuMenuItem *item = NEMO_CONTEXT_MENU_MENU_ITEM (widget);
    GtkAllocation alloc;

    gtk_widget_get_allocation (item->toggle_widget, &alloc);

    item->on_toggle = ((x >= alloc.x) &&
                       (x <= alloc.x + alloc.width) &&
                       (y >= alloc.y) &&
                       (y <= alloc.y + alloc.height));

    update_toggle_state (item, TRUE, on_item);
}

static void
nemo_context_menu_menu_item_class_init (NemoContextMenuMenuItemClass *klass)
{
    GObjectClass *gobject_class = (GObjectClass*) klass;
    GtkMenuItemClass *menu_item_class = (GtkMenuItemClass*) klass;
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->dispose = nemo_context_menu_menu_item_dispose;

    menu_item_class->set_label = nemo_context_menu_menu_item_set_label;

    widget_class->enter_notify_event = nemo_context_menu_menu_item_enter;
    widget_class->leave_notify_event = nemo_context_menu_menu_item_leave;
    widget_class->motion_notify_event = nemo_context_menu_menu_item_motion;
    widget_class->button_press_event = nemo_context_menu_menu_item_button_press;
    widget_class->button_release_event = nemo_context_menu_menu_item_button_release;
}

static void 
nemo_context_menu_menu_item_init (NemoContextMenuMenuItem *item)
{
    item->on_toggle = FALSE;
    GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

    GtkWidget *stack = gtk_stack_new ();

    item->stack = stack;

    GtkWidget *label = gtk_label_new (NULL);
    gtk_misc_set_alignment (GTK_MISC (label), 0.98, 0.5);
    gtk_stack_add_named (GTK_STACK (stack), label, "toggle");
    item->toggle_label_widget = label;

    label = gtk_label_new (NULL);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_stack_add_named (GTK_STACK (stack), label, "action");
    item->label_widget = label;

    gtk_box_pack_start (GTK_BOX (box), stack, TRUE, TRUE, 0);

    GtkWidget *toggle = gtk_image_new ();
    gtk_box_pack_end (GTK_BOX (box), toggle, FALSE, FALSE, 0);
    item->toggle_widget = toggle;

    gtk_widget_show_all (box);

    gtk_container_add (GTK_CONTAINER (item), box);

    update_toggle_state (item, FALSE, FALSE);

    item->settings_monitor_id = g_signal_connect_swapped (nemo_preferences, 
                                                          "changed::" NEMO_PREFERENCES_CONTEXT_MENUS_SHOW_ALL_ACTIONS,
                                                          G_CALLBACK (update_toggle_state),
                                                          item);
}

static void
nemo_context_menu_menu_item_dispose (GObject *object)
{
    NemoContextMenuMenuItem *item = NEMO_CONTEXT_MENU_MENU_ITEM (object);

    if (item->settings_monitor_id > 0) {
        g_signal_handler_disconnect (nemo_preferences, item->settings_monitor_id);
        item->settings_monitor_id = 0;
    }

    g_clear_pointer (&item->label, g_free);

    G_OBJECT_CLASS (nemo_context_menu_menu_item_parent_class)->dispose (object);
}

static gboolean
nemo_context_menu_menu_item_enter (GtkWidget        *widget,
                                   GdkEventCrossing *event)
{
    g_return_val_if_fail (event != NULL, FALSE);

    update_toggle_appearance_from_event (widget, event->x, event->y, TRUE);

    return gtk_widget_event (gtk_widget_get_parent (widget), (GdkEvent *) event);
}

static gboolean
nemo_context_menu_menu_item_leave (GtkWidget        *widget,
                                   GdkEventCrossing *event)
{
    g_return_val_if_fail (event != NULL, FALSE);

    update_toggle_appearance_from_event (widget, event->x, event->y, FALSE);

    return gtk_widget_event (gtk_widget_get_parent (widget), (GdkEvent *) event);
}

static gboolean
nemo_context_menu_menu_item_motion (GtkWidget        *widget,
                                    GdkEventMotion   *event)
{
    g_return_val_if_fail (event != NULL, FALSE);

    update_toggle_appearance_from_event (widget, event->x, event->y, TRUE);

    return gtk_widget_event (gtk_widget_get_parent (widget), (GdkEvent *) event);
}

static gboolean
nemo_context_menu_menu_item_button_press (GtkWidget      *widget,
                                          GdkEventButton *event)
{
    NemoContextMenuMenuItem *item = NEMO_CONTEXT_MENU_MENU_ITEM (widget);

    if (event->button != GDK_BUTTON_PRIMARY)
        return GDK_EVENT_PROPAGATE;

    update_toggle_appearance_from_event (widget, event->x, event->y, TRUE);

    if (item->on_toggle) {
        g_settings_set_boolean (nemo_preferences,
                                NEMO_PREFERENCES_CONTEXT_MENUS_SHOW_ALL_ACTIONS,
                                !g_settings_get_boolean (nemo_preferences,
                                                         NEMO_PREFERENCES_CONTEXT_MENUS_SHOW_ALL_ACTIONS));
        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean
nemo_context_menu_menu_item_button_release (GtkWidget      *widget,
                                            GdkEventButton *event)
{
    NemoContextMenuMenuItem *item = NEMO_CONTEXT_MENU_MENU_ITEM (widget);

    if (event->button != GDK_BUTTON_PRIMARY &&
        event->button != GDK_BUTTON_SECONDARY)
        return GDK_EVENT_PROPAGATE;

    update_toggle_appearance_from_event (widget, event->x, event->y, TRUE);

    if (item->on_toggle) {
        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static void
nemo_context_menu_menu_item_set_label (GtkMenuItem      *menu_item,
                                       const gchar      *label)
{
    NemoContextMenuMenuItem *item = NEMO_CONTEXT_MENU_MENU_ITEM (menu_item);

    if (g_strcmp0 (item->label, label) != 0)
    {
        g_free (item->label);
        item->label = g_strdup (label);

        gtk_label_set_text_with_mnemonic (GTK_LABEL (item->label_widget), label);

        g_object_notify (G_OBJECT (menu_item), "label");
    }
}

static gboolean
activatable_update_stock_id (GtkImageMenuItem *image_menu_item,
                             GtkAction        *action)
{
    GtkWidget   *image;
    const gchar *stock_id  = gtk_action_get_stock_id (action);

    image = gtk_image_menu_item_get_image (image_menu_item);

    if (GTK_IS_IMAGE (image) &&
        stock_id && gtk_icon_factory_lookup_default (stock_id)) {
        gtk_image_set_from_stock (GTK_IMAGE (image), stock_id, GTK_ICON_SIZE_MENU);
        return TRUE;
    }

    return FALSE;
}

static gboolean
activatable_update_gicon (GtkImageMenuItem *image_menu_item,
                          GtkAction        *action)
{
    GtkWidget   *image;
    GIcon       *icon = gtk_action_get_gicon (action);
    const gchar *stock_id;
    gboolean     ret = FALSE;

    stock_id = gtk_action_get_stock_id (action);

    image = gtk_image_menu_item_get_image (image_menu_item);

    if (icon && GTK_IS_IMAGE (image) &&
        !(stock_id && gtk_icon_factory_lookup_default (stock_id))) {
        gtk_image_set_from_gicon (GTK_IMAGE (image), icon, GTK_ICON_SIZE_MENU);
        ret = TRUE;
    }

    return ret;
}

static void
activatable_update_icon_name (GtkImageMenuItem *image_menu_item,
                              GtkAction        *action)
{
    GtkWidget   *image;
    const gchar *icon_name = gtk_action_get_icon_name (action);

    image = gtk_image_menu_item_get_image (image_menu_item);

    if (GTK_IS_IMAGE (image) &&
        (gtk_image_get_storage_type (GTK_IMAGE (image)) == GTK_IMAGE_EMPTY ||
         gtk_image_get_storage_type (GTK_IMAGE (image)) == GTK_IMAGE_ICON_NAME)) {
        gtk_image_set_from_icon_name (GTK_IMAGE (image), icon_name, GTK_ICON_SIZE_MENU);
    }
}

static void
activatable_update_label (GtkMenuItem *menu_item, GtkAction *action)
{
    const gchar *label;
    label = gtk_action_get_label (action);
    nemo_context_menu_menu_item_set_label (menu_item, label);
}

static void
nemo_context_menu_menu_item_update (GtkActivatable *activatable,
                                    GtkAction      *action,
                                    const gchar    *property_name)
{
    if (!gtk_activatable_get_use_action_appearance (activatable))
        return;
    if (strcmp (property_name, "label") == 0)
        activatable_update_label (GTK_MENU_ITEM (activatable), action);
    else if (strcmp (property_name, "stock-id") == 0)
        activatable_update_stock_id (GTK_IMAGE_MENU_ITEM (activatable), action);
    else if (strcmp (property_name, "gicon") == 0)
        activatable_update_gicon (GTK_IMAGE_MENU_ITEM (activatable), action);
    else if (strcmp (property_name, "icon-name") == 0)
        activatable_update_icon_name (GTK_IMAGE_MENU_ITEM (activatable), action);
}

static void
menu_item_sync_action_properties (NemoContextMenuMenuItem *menu_item,
                                  GtkAction               *action)
{
    GtkImageMenuItem *image_menu_item;
    GtkActivatable *activatable;
    GtkWidget *image;
    gboolean   use_appearance;

    image_menu_item = GTK_IMAGE_MENU_ITEM (menu_item);

    activatable = GTK_ACTIVATABLE (image_menu_item);

    if (!action)
        return;

    use_appearance = gtk_activatable_get_use_action_appearance (activatable);
    if (!use_appearance)
        return;

    image = gtk_image_menu_item_get_image (image_menu_item);
    if (image && !GTK_IS_IMAGE (image)) {
        gtk_image_menu_item_set_image (image_menu_item, NULL);
        image = NULL;
    }

    if (!image) {
        image = gtk_image_new ();
        gtk_widget_show (image);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (activatable),
                                       image);
    }

    if (!activatable_update_stock_id (image_menu_item, action) &&
        !activatable_update_gicon (image_menu_item, action))
        activatable_update_icon_name (image_menu_item, action);

    gtk_image_menu_item_set_always_show_image (image_menu_item,
                                               gtk_action_get_always_show_image (action));

    activatable_update_label (GTK_MENU_ITEM (menu_item), action);
}

static void
nemo_context_menu_menu_item_sync_action_properties (GtkActivatable *activatable,
                                                    GtkAction      *action)
{
    NemoContextMenuMenuItem *context_menu_menu_item;

    context_menu_menu_item = NEMO_CONTEXT_MENU_MENU_ITEM (activatable);

    if (!action)
        return;

    if (!gtk_activatable_get_use_action_appearance (activatable))
        return;

    menu_item_sync_action_properties (context_menu_menu_item, action);

    gtk_widget_show (GTK_WIDGET (context_menu_menu_item));
}

static void
nemo_context_menu_menu_item_activatable_interface_init (GtkActivatableIface  *iface)
{
    parent_activatable_iface = g_type_interface_peek_parent (iface);
    iface->update = nemo_context_menu_menu_item_update;
    iface->sync_action_properties = nemo_context_menu_menu_item_sync_action_properties;
}

/**
 * nemo_context_menu_menu_item_new:
 * @widget: The custom widget to use
 *
 * Creates a new #NemoContextMenuMenuItem.
 *
 * Returns: a new #NemoContextMenuMenuItem.
 */
GtkWidget *
nemo_context_menu_menu_item_new (GtkWidget *widget)
{
    return g_object_new (NEMO_TYPE_CONTEXT_MENU_MENU_ITEM,
                         NULL);
}

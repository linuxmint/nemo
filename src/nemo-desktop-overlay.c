#include <config.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>

#include <math.h>
#include <libxapp/xapp-gtk-window.h>

#include "nemo-desktop-overlay.h"
#include "nemo-desktop-manager.h"

#include <libnemo-extension/nemo-desktop-preferences.h>

typedef struct
{
    GtkWindow *window;
    NemoDesktopManager *manager;
    gboolean is_cinnamon;

    GtkStack *stack;
    GtkStack *view_substack;
    GtkWidget *icon_size_combo;
    GtkWidget *direction_combo;
    GtkWidget *sort_combo;

    GtkBuilder *builder;
    GtkWindow *nemo_window;
    GtkActionGroup *action_group;
    gint monitor;

    guint adjust_changed_id;
    guint configure_event_id;

    gint h_percent;
    gint v_percent;
} NemoDesktopOverlayPrivate;

struct _NemoDesktopOverlay
{
    GObject parent_object;

    NemoDesktopOverlayPrivate *priv;
};

enum
{
    ADJUSTS_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0, };

G_DEFINE_TYPE_WITH_PRIVATE (NemoDesktopOverlay, nemo_desktop_overlay, G_TYPE_OBJECT)

static void
show_view_page (NemoDesktopOverlay *overlay)
{
    NemoDesktopOverlayPrivate *priv = nemo_desktop_overlay_get_instance_private (overlay);
    GdkScreen *screen;
    GdkRectangle rect;
    gchar *plug_name;

    screen = gdk_screen_get_default ();

    plug_name = gdk_screen_get_monitor_plug_name (screen,
                                                  priv->monitor);

    gdk_screen_get_monitor_geometry (screen,
                                     priv->monitor,
                                     &rect);

    if (plug_name) {
        gchar *title;

        title = g_strdup_printf (_("Current Monitor Layout (%s - %dx%d)"),
                                 plug_name,
                                 rect.width,
                                 rect.height);

        gtk_window_set_title (priv->window,
                              title);

        g_free (title);
    } else {
        gchar *title;

        title = g_strdup_printf (_("Current Monitor Layout (%dx%d)"),
                                 rect.width,
                                 rect.height);

        gtk_window_set_title (priv->window,
                              title);

        g_free (title);
    }

    g_free (plug_name);

    gtk_stack_set_visible_child_name (priv->stack, "view");
}

static void
sync_controls (NemoDesktopOverlay *overlay,
               gboolean same_monitor)
{
    NemoDesktopOverlayPrivate *priv = nemo_desktop_overlay_get_instance_private (overlay);
    GtkRange *range;
    GtkActionGroup *action_group;
    GtkAction *action;
    gint h_adjust, v_adjust, active_id;
    gboolean fake_group;
    const gchar *combo_id;

    if (!nemo_desktop_manager_get_monitor_is_active (priv->manager, priv->monitor)) {
        gtk_stack_set_visible_child_name (priv->view_substack, "substack_disabled");

        priv->nemo_window = NULL;

        priv->action_group = gtk_action_group_new ("empty");
        fake_group = TRUE;
    } else {
        gtk_stack_set_visible_child_name (priv->view_substack, "substack_enabled");

        priv->nemo_window = nemo_desktop_manager_get_window_for_monitor (nemo_desktop_manager_get (),
                                                                         priv->monitor);

        nemo_desktop_manager_get_overlay_info (priv->manager,
                                               priv->monitor,
                                               &action_group,
                                               &h_adjust,
                                               &v_adjust);

        range = GTK_RANGE (gtk_builder_get_object (priv->builder, "horizontal_adjust_slider"));
        gtk_range_set_value (range, (double) h_adjust);

        range = GTK_RANGE (gtk_builder_get_object (priv->builder, "vertical_adjust_slider"));
        gtk_range_set_value (range, (double) v_adjust);

        /* Catch enabling of a particular monitor.  If we were a blank window and now
         * we're a real desktop window, it makes sense to present the view settings to the user */
        if (same_monitor && (action_group != priv->action_group)) {
            show_view_page (overlay);
        }

        priv->action_group = action_group;

        /* Sync combo boxes - we just need to access a single radio action for each group to get the active one.  From that
         * we can set the active-id of the combobox */

        /* Icon size */
        action = gtk_action_group_get_action (priv->action_group, "Desktop Normal");
        active_id = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));

        switch (active_id) {
            case NEMO_ZOOM_LEVEL_SMALLER:
                combo_id = "Desktop Smaller";
                break;
            case NEMO_ZOOM_LEVEL_SMALL:
                combo_id = "Desktop Small";
                break;
            case NEMO_ZOOM_LEVEL_LARGE:
                combo_id = "Desktop Large";
                break;
            case NEMO_ZOOM_LEVEL_LARGER:
                combo_id = "Desktop Larger";
                break;
            case NEMO_ZOOM_LEVEL_STANDARD:
            default:
                combo_id = "Desktop Normal";
                break;
        }

        gtk_combo_box_set_active_id (GTK_COMBO_BOX (priv->icon_size_combo), combo_id);

        /* Layout direction */
        action = gtk_action_group_get_action (priv->action_group, "Vertical Layout");
        active_id = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));

        switch (active_id) {
            case DESKTOP_ARRANGE_HORIZONTAL:
                combo_id = "Horizontal Layout";
                break;
            case DESKTOP_ARRANGE_VERTICAL:
            default:
                combo_id = "Vertical Layout";
                break;
        }

        gtk_combo_box_set_active_id (GTK_COMBO_BOX (priv->direction_combo), combo_id);

        /* Sort type */
        action = gtk_action_group_get_action (priv->action_group, "Vertical Layout");
        active_id = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));

        switch (active_id) {
            case NEMO_FILE_SORT_BY_SIZE:
                combo_id = "Desktop Sort by Size";
                break;
            case NEMO_FILE_SORT_BY_DETAILED_TYPE:
                combo_id = "Desktop Sort by Type";
                break;
            case NEMO_FILE_SORT_BY_MTIME:
                combo_id = "Desktop Sort by Date";
                break;
            case NEMO_FILE_SORT_BY_DISPLAY_NAME:
            default:
                combo_id = "Desktop Sort by Name";
                break;
        }

        gtk_combo_box_set_active_id (GTK_COMBO_BOX (priv->sort_combo), combo_id);

        fake_group = FALSE;
    }

    /* Actual handling is done in NemoDesktopIconGridView */

    /* Sync toggle switches */
    action = gtk_action_group_get_action (priv->action_group, "Desktop Autoarrange");
    gtk_activatable_set_related_action (GTK_ACTIVATABLE (gtk_builder_get_object (priv->builder, "auto_arrange_switch")),
                                        action);

    action = gtk_action_group_get_action (priv->action_group, "Desktop Reverse Sort");
    gtk_activatable_set_related_action (GTK_ACTIVATABLE (gtk_builder_get_object (priv->builder, "reverse_sort_switch")),
                                        action);

    if (fake_group) {
        g_clear_object (&priv->action_group);
    }

    show_view_page (overlay);
}

static void
signal_adjust_changed (NemoDesktopOverlay *overlay)
{
    NemoDesktopOverlayPrivate *priv = overlay->priv;
    double h_val, v_val;
    gint h_percent, v_percent;

    h_val = gtk_range_get_value (GTK_RANGE (gtk_builder_get_object (priv->builder,
                                                                    "horizontal_adjust_slider")));
    v_val = gtk_range_get_value (GTK_RANGE (gtk_builder_get_object (priv->builder,
                                                                    "vertical_adjust_slider")));

    h_percent = CLAMP (floor (h_val), 0, 200);
    v_percent = CLAMP (floor (v_val), 0, 200);

    if (h_percent != priv->h_percent || v_percent != priv->v_percent) {
        priv->h_percent = h_percent;
        priv->v_percent = v_percent;

        g_signal_emit (overlay,
                       signals[ADJUSTS_CHANGED], 0,
                       priv->nemo_window,
                       h_percent,
                       v_percent);
    }
}

static gboolean
signal_adjust_delay_timeout (gpointer user_data)
{
    NemoDesktopOverlay *overlay = NEMO_DESKTOP_OVERLAY (user_data);
    NemoDesktopOverlayPrivate *priv = nemo_desktop_overlay_get_instance_private (overlay);

    priv->adjust_changed_id = 0;

    signal_adjust_changed (overlay);

    return FALSE;
}

static void
queue_changed_signal (NemoDesktopOverlay *overlay)
{
    NemoDesktopOverlayPrivate *priv = nemo_desktop_overlay_get_instance_private (overlay);

    if (priv->adjust_changed_id > 0) {
        return;
    }

    priv->adjust_changed_id = g_timeout_add (50, (GSourceFunc) signal_adjust_delay_timeout, overlay);
}

static void
on_horizontal_adjust_changed (GtkRange *range,
                              gpointer  user_data)
{
    NemoDesktopOverlay *overlay = NEMO_DESKTOP_OVERLAY (user_data);

    queue_changed_signal (overlay);
}

static void
on_vertical_adjust_changed (GtkRange *range,
                            gpointer  user_data)
{
    NemoDesktopOverlay *overlay = NEMO_DESKTOP_OVERLAY (user_data);

    queue_changed_signal (overlay);
}

static void
on_combo_changed (GtkComboBox *widget,
                            gpointer     user_data)
{
    NemoDesktopOverlay *overlay = NEMO_DESKTOP_OVERLAY (user_data);
    NemoDesktopOverlayPrivate *priv = nemo_desktop_overlay_get_instance_private (overlay);
    const gchar *action_id;

    action_id = gtk_combo_box_get_active_id (widget);
    if (action_id) {
        GtkAction *action;

        action = gtk_action_group_get_action (priv->action_group, action_id);

        gtk_action_activate (action);
    }
}

static void
on_global_prefs_link_button_clicked (GtkWidget *button,
                                       gpointer   user_data)
{
    NemoDesktopOverlay *overlay = NEMO_DESKTOP_OVERLAY (user_data);

    show_view_page (overlay);
}


static void
on_view_prefs_button_clicked (GtkWidget *button,
                                       gpointer   user_data)
{
    NemoDesktopOverlay *overlay = NEMO_DESKTOP_OVERLAY (user_data);
    NemoDesktopOverlayPrivate *priv = nemo_desktop_overlay_get_instance_private (overlay);

    if (priv->is_cinnamon) {
        GError *error;

        error = NULL;

        if (!g_spawn_command_line_async ("cinnamon-settings desktop", &error)) {
            g_warning ("Could not spawn 'cinnamon-settings desktop': %s", error->message);

            g_error_free (error);
        } else {
            return;
        }
    }

    gtk_stack_set_visible_child_name (priv->stack, "global");
    gtk_window_set_title (priv->window, _("Desktop Settings"));
}

static gboolean
on_link_button_activate_link (GtkLinkButton *button,
                              gpointer       user_data)
{
    /* Stop the link activation */
    return GDK_EVENT_STOP;
}

static void
on_grid_reset_button_clicked (GtkWidget *button,
                              gpointer   user_data)
{
    NemoDesktopOverlay *overlay = NEMO_DESKTOP_OVERLAY (user_data);
    NemoDesktopOverlayPrivate *priv = nemo_desktop_overlay_get_instance_private (overlay);
    GtkRange *range;

    range = GTK_RANGE (gtk_builder_get_object (priv->builder, "horizontal_adjust_slider"));
    gtk_range_set_value (range, 100.0);

    range = GTK_RANGE (gtk_builder_get_object (priv->builder, "vertical_adjust_slider"));
    gtk_range_set_value (range, 100.0);
}

static gboolean
on_close_window (GtkWidget *overlay_window,
                 GdkEvent  *event,
                 gpointer   user_data)
{
    g_return_val_if_fail (NEMO_IS_DESKTOP_OVERLAY (user_data), GDK_EVENT_PROPAGATE);

    /* When the window is destroyed, kill the overlay instance also.
     * This will end up clearing the weak pointer made in nemo-desktop-icon-grid-view.c. */
    g_object_unref (G_OBJECT (user_data));

    return GDK_EVENT_PROPAGATE;
}

gboolean
on_window_configure_event (GtkWidget *widget,
                           GdkEvent  *event,
                           gpointer   user_data)
{
    NemoDesktopOverlay *overlay = NEMO_DESKTOP_OVERLAY (user_data);
    NemoDesktopOverlayPrivate *priv = nemo_desktop_overlay_get_instance_private (overlay);
    gint monitor;

    g_return_val_if_fail (gtk_widget_get_window (widget) != NULL, GDK_EVENT_PROPAGATE);

    monitor = gdk_screen_get_monitor_at_window (gdk_screen_get_default (),
                                                gtk_widget_get_window (widget));

    if (monitor != priv->monitor) {
        priv->monitor = monitor;

        sync_controls (overlay, FALSE); 
    }

    return GDK_EVENT_PROPAGATE;
}

static void
nemo_desktop_overlay_init (NemoDesktopOverlay *overlay)
{
    NemoDesktopOverlayPrivate *priv = nemo_desktop_overlay_get_instance_private (overlay);
    GtkWindow *window;
    GtkWidget *widget;
    GtkWidget *prefs_box;

    overlay->priv = priv;
    priv->manager = nemo_desktop_manager_get ();
    priv->is_cinnamon = nemo_desktop_manager_get_is_cinnamon (priv->manager);

    priv->builder = gtk_builder_new ();
    gtk_builder_set_translation_domain (priv->builder, GETTEXT_PACKAGE);
    gtk_builder_add_from_resource (priv->builder, "/org/nemo/nemo-desktop-overlay.glade", NULL);

    window = GTK_WINDOW (gtk_builder_get_object (priv->builder, "overlay_window"));
    priv->window = window;

    /* Can't set this in glade, glade uses the icon-name property.  This could be remedied
     * by watching property changes in XAppGtkWindow, but it's not that big a deal */
    xapp_gtk_window_set_icon_name (XAPP_GTK_WINDOW (window), "preferences-desktop");

    gtk_widget_add_events (GTK_WIDGET (window), GDK_STRUCTURE_MASK);

    prefs_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width (GTK_CONTAINER (prefs_box), 6);

    g_object_set (GTK_BOX (prefs_box),
                  "margin-start", 80,
                  "margin-end", 80,
                  "margin-top", 15,
                  "margin-bottom", 15,
                  NULL);

    if (!priv->is_cinnamon) {
        widget = GTK_WIDGET (nemo_desktop_preferences_new ());
        gtk_box_pack_start (GTK_BOX (prefs_box), widget, TRUE, TRUE, 0);

        widget = gtk_link_button_new_with_label ("http://null",
                                               _("Current Monitor Preferences"));

        g_signal_connect (widget,
                          "clicked",
                          G_CALLBACK (on_global_prefs_link_button_clicked),
                          overlay);

        g_signal_connect (widget,
                          "activate-link",
                          G_CALLBACK (on_link_button_activate_link),
                          overlay);

        gtk_widget_set_hexpand (widget, TRUE);
        gtk_box_pack_end (GTK_BOX (prefs_box), widget, TRUE, FALSE, 0);

        gtk_widget_show_all (prefs_box);
        gtk_widget_queue_allocate (prefs_box);
    }

    priv->stack = GTK_STACK (gtk_builder_get_object (priv->builder, "stack"));
    gtk_stack_add_titled (priv->stack,
                          prefs_box,
                          "global",
                          _("Show global desktop settings"));

    gtk_container_child_set (GTK_CONTAINER (priv->stack),
                             prefs_box,
                             "icon-name", "preferences-system-symbolic",
                             NULL);

    priv->view_substack = GTK_STACK (gtk_builder_get_object (priv->builder, "view_substack"));

    widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "view_prefs_link_button"));
    gtk_button_set_label (GTK_BUTTON (widget), _("Desktop Settings"));

    widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "disabled_view_link_button"));
    gtk_button_set_label (GTK_BUTTON (widget), _("Desktop Settings"));

    priv->icon_size_combo = GTK_WIDGET (gtk_builder_get_object (priv->builder, "icon_size_combo"));
    priv->direction_combo = GTK_WIDGET (gtk_builder_get_object (priv->builder, "direction_combo"));
    priv->sort_combo = GTK_WIDGET (gtk_builder_get_object (priv->builder, "sort_combo"));

    gtk_builder_add_callback_symbols (priv->builder,
      "on_vertical_adjust_changed", G_CALLBACK (on_vertical_adjust_changed),
      "on_horizontal_adjust_changed", G_CALLBACK (on_horizontal_adjust_changed),
      "on_disabled_view_link_button_clicked", G_CALLBACK (on_view_prefs_button_clicked),
      "on_disabled_view_link_button_activate_link", G_CALLBACK (on_link_button_activate_link),
      "on_view_prefs_link_button_clicked", G_CALLBACK (on_view_prefs_button_clicked),
      "on_view_prefs_link_button_activate_link", G_CALLBACK (on_link_button_activate_link),
      "on_grid_reset_button_clicked", G_CALLBACK (on_grid_reset_button_clicked),
      "on_icon_size_combo_changed", G_CALLBACK (on_combo_changed),
      "on_direction_combo_changed", G_CALLBACK (on_combo_changed),
      "on_sort_combo_changed", G_CALLBACK (on_combo_changed),
      "on_close_window", G_CALLBACK (on_close_window),
      NULL);

    gtk_builder_connect_signals (priv->builder, overlay);
}

static void
show_overlay (NemoDesktopOverlay *overlay,
              gint                monitor)
{
    NemoDesktopOverlayPrivate *priv = nemo_desktop_overlay_get_instance_private (overlay);
    gint root_x, root_y, parent_width, parent_height, default_width, default_height;
    gint old_monitor;

    old_monitor = priv->monitor;

    priv->monitor = monitor;
    priv->nemo_window = nemo_desktop_manager_get_window_for_monitor (priv->manager,
                                                                     monitor);

    if (old_monitor != monitor) {
        gtk_window_get_size (priv->window,
                             &default_width,
                             &default_height);

        gtk_window_get_position (GTK_WINDOW (priv->nemo_window),
                                 &root_x,
                                 &root_y);

        gtk_window_get_size (GTK_WINDOW (priv->nemo_window),
                             &parent_width,
                             &parent_height);

        gtk_window_move (priv->window,
                         (root_x + (parent_width / 2) - (default_width / 2)),
                         (root_y + (parent_height / 2) - (default_height / 2)));
    }

    sync_controls (overlay, old_monitor == monitor);

    if (priv->configure_event_id == 0) {
        priv->configure_event_id = g_signal_connect (GTK_WIDGET (priv->window),
                                                     "configure-event",
                                                     G_CALLBACK (on_window_configure_event),
                                                     overlay);
    }

    gtk_window_present_with_time (priv->window, gdk_event_get_time (gtk_get_current_event ()));
}

static void
nemo_desktop_overlay_dispose (GObject *object)
{
    NemoDesktopOverlay *overlay = NEMO_DESKTOP_OVERLAY (object);

    g_clear_object (&overlay->priv->builder);
    g_clear_pointer (&overlay->priv->window, gtk_widget_destroy);

    G_OBJECT_CLASS (nemo_desktop_overlay_parent_class)->dispose (object);
}

static void
nemo_desktop_overlay_class_init (NemoDesktopOverlayClass *klass)
{
    signals[ADJUSTS_CHANGED] =
        g_signal_new ("adjusts-changed",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL, NULL,
                      G_TYPE_NONE, 3,
                      NEMO_TYPE_WINDOW, G_TYPE_INT, G_TYPE_INT);

    G_OBJECT_CLASS (klass)->dispose = nemo_desktop_overlay_dispose;
}

NemoDesktopOverlay *
nemo_desktop_overlay_new (void)
{
    return g_object_new (NEMO_TYPE_DESKTOP_OVERLAY, NULL);
}

void
nemo_desktop_overlay_show (NemoDesktopOverlay *overlay,
                           gint                monitor)
{
    g_return_if_fail (NEMO_IS_DESKTOP_OVERLAY (overlay));
    show_overlay (overlay, monitor);
}

void
nemo_desktop_overlay_update_in_place (NemoDesktopOverlay *overlay)
{
    g_return_if_fail (NEMO_IS_DESKTOP_OVERLAY (overlay));
    show_overlay (overlay, overlay->priv->monitor);
}
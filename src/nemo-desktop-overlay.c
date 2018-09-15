#include <config.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>

#include <math.h>
#include "nemo-desktop-overlay.h"
#include "nemo-desktop-manager.h"

#include <libnemo-extension/nemo-desktop-preferences.h>

typedef struct
{
    GtkWindow *window;
    NemoDesktopManager *manager;

    GtkStack *stack;
    GtkStack *view_substack;
    GtkStackSwitcher *page_switcher;
    GtkWidget *header_bar;


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
sync_controls (NemoDesktopOverlay *overlay,
               gboolean same_monitor)
{
    NemoDesktopOverlayPrivate *priv = nemo_desktop_overlay_get_instance_private (overlay);
    GtkRange *range;
    GtkActionGroup *action_group;
    GtkAction *action;
    gint h_adjust, v_adjust;
    gboolean fake_group;

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
            gtk_stack_set_visible_child_name (priv->stack, "view");
        }

        priv->action_group = action_group;

        fake_group = FALSE;
    }

    /* Actual handling is done in NemoDesktopIconGridView */

    action = gtk_action_group_get_action (priv->action_group, "Desktop Large");

    gtk_activatable_set_related_action (GTK_ACTIVATABLE (gtk_builder_get_object (priv->builder, "large_icons_toggle")),
                                   action);
    action = gtk_action_group_get_action (priv->action_group, "Desktop Normal");
    gtk_activatable_set_related_action (GTK_ACTIVATABLE (gtk_builder_get_object (priv->builder, "medium_icons_toggle")),
                                   action);
    action = gtk_action_group_get_action (priv->action_group, "Desktop Small");

    gtk_activatable_set_related_action (GTK_ACTIVATABLE (gtk_builder_get_object (priv->builder, "small_icons_toggle")),
                                   action);
    
    action = gtk_action_group_get_action (priv->action_group, "Horizontal Layout");

    gtk_activatable_set_related_action (GTK_ACTIVATABLE (gtk_builder_get_object (priv->builder, "horizontal_layout_button")),
                                   action);
    action = gtk_action_group_get_action (priv->action_group, "Vertical Layout");

    gtk_activatable_set_related_action (GTK_ACTIVATABLE (gtk_builder_get_object (priv->builder, "vertical_layout_button")),
                                   action);
    action = gtk_action_group_get_action (priv->action_group, "Desktop Autoarrange");

    gtk_activatable_set_related_action (GTK_ACTIVATABLE (gtk_builder_get_object (priv->builder, "auto_arrange_button")),
                                   action);

    if (fake_group) {
        g_clear_object (&priv->action_group);
    }
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
on_disabled_view_prefs_button_clicked (GtkWidget *button,
                                       gpointer   user_data)
{
    NemoDesktopOverlay *overlay = NEMO_DESKTOP_OVERLAY (user_data);
    NemoDesktopOverlayPrivate *priv = nemo_desktop_overlay_get_instance_private (overlay);

    gtk_stack_set_visible_child_name (priv->stack, "global");
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

static void
on_stack_changed (GtkWidget  *stack,
                  GParamSpec *pspec,
                  gpointer    user_data)
{
    NemoDesktopOverlay *overlay = NEMO_DESKTOP_OVERLAY (user_data);
    NemoDesktopOverlayPrivate *priv = overlay->priv;
    const gchar *visible_child_name;

    visible_child_name = gtk_stack_get_visible_child_name (priv->stack);

    if (g_strcmp0 (visible_child_name, "view") == 0) {
        gtk_header_bar_set_subtitle (GTK_HEADER_BAR (priv->header_bar), _("Current Monitor Preferences"));
    } else {
        gtk_header_bar_set_subtitle (GTK_HEADER_BAR (priv->header_bar), _("Global Desktop Settings"));
    }
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
    GtkWidget *prefs_widget;
    GtkWidget *prefs_box;

    overlay->priv = priv;
    priv->manager = nemo_desktop_manager_get ();

    priv->builder = gtk_builder_new ();
    gtk_builder_set_translation_domain (priv->builder, GETTEXT_PACKAGE);
    gtk_builder_add_from_resource (priv->builder, "/org/nemo/nemo-desktop-overlay.glade", NULL);


    window = GTK_WINDOW (gtk_builder_get_object (priv->builder, "overlay_window"));
    priv->window = window;

    gtk_widget_add_events (GTK_WIDGET (window), GDK_STRUCTURE_MASK);

    prefs_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width (GTK_CONTAINER (prefs_box), 6);
    gtk_widget_show (prefs_box);

    prefs_widget = GTK_WIDGET (nemo_desktop_preferences_new ());
    gtk_box_pack_start (GTK_BOX (prefs_box), prefs_widget, TRUE, TRUE, 0);

    priv->stack = GTK_STACK (gtk_builder_get_object (priv->builder, "stack"));
    gtk_stack_add_titled (priv->stack,
                          prefs_box,
                          "global",
                          _("Show global desktop settings"));

    gtk_container_child_set (GTK_CONTAINER (priv->stack),
                             prefs_box,
                             "icon-name", "preferences-system-symbolic",
                             NULL);

    g_signal_connect (priv->stack,
                      "notify::visible-child-name",
                      G_CALLBACK (on_stack_changed),
                      overlay);

    priv->view_substack = GTK_STACK (gtk_builder_get_object (priv->builder, "view_substack"));

    priv->page_switcher = GTK_STACK_SWITCHER (gtk_builder_get_object (priv->builder, "page_switcher"));
    priv->header_bar = GTK_WIDGET (gtk_builder_get_object (priv->builder, "header_bar"));

    gtk_builder_add_callback_symbols (priv->builder,
      "on_vertical_adjust_changed", G_CALLBACK (on_vertical_adjust_changed),
      "on_horizontal_adjust_changed", G_CALLBACK (on_horizontal_adjust_changed),
      "on_disabled_view_prefs_button_clicked", G_CALLBACK (on_disabled_view_prefs_button_clicked),
      "on_grid_reset_button_clicked", G_CALLBACK (on_grid_reset_button_clicked),
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
                         (root_x + parent_width - default_width),
                         (root_y + parent_height - default_height));
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
/* nemo-desktop-manager.c */

#include "nemo-desktop-manager.h"
#include "nemo-blank-desktop-window.h"
#include "nemo-desktop-window.h"
#include "nemo-desktop-application.h"

#include <gdk/gdkx.h>

#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-desktop-utils.h>

static gboolean layout_changed (NemoDesktopManager *manager);

G_DEFINE_TYPE (NemoDesktopManager, nemo_desktop_manager, G_TYPE_OBJECT);

#define DESKTOPS_ON_PRIMARY "true::false"
#define DESKTOPS_ON_ALL "true::true"
#define DESKTOPS_ON_NON_PRIMARY "false::true"
#define DESKTOPS_ON_NONE "false::false"
#define DESKTOPS_DEFAULT DESKTOPS_ON_PRIMARY

typedef struct {
    GtkWidget *window;

    gint monitor_num;
    gboolean shows_desktop;
    gboolean is_primary;
} DesktopInfo;

static NemoDesktopManager *_manager = NULL;

static void
free_info (DesktopInfo *info)
{
    g_return_if_fail (info != NULL);

    g_clear_pointer (&info->window, gtk_widget_destroy);
    g_slice_free (DesktopInfo, info);
}

static void
close_all_windows (NemoDesktopManager *manager)
{
    g_list_foreach (manager->desktops, (GFunc) free_info, NULL);
    g_clear_pointer (&manager->desktops, g_list_free);
}

static void
queue_update_layout (NemoDesktopManager *manager)
{
    if (manager->update_layout_idle_id > 0) {
        g_source_remove (manager->update_layout_idle_id);
        manager->update_layout_idle_id = 0;
    }

    manager->update_layout_idle_id = g_idle_add ((GSourceFunc) layout_changed, manager);
}

static void
on_window_scale_changed (GtkWidget          *window,
                         GParamSpec         *pspec,
                         NemoDesktopManager *manager)
{
    manager->scale_factor_changed_id = 0;

    queue_update_layout (manager);
}

static void
create_new_desktop_window (NemoDesktopManager *manager,
                                         gint  monitor,
                                     gboolean  primary,
                                     gboolean  show_desktop)
{
    GtkWidget *window;

    DesktopInfo *info = g_slice_new0 (DesktopInfo);

    info->monitor_num = monitor;
    info->shows_desktop = show_desktop;
    info->is_primary = primary;

    if (show_desktop) {
        window = GTK_WIDGET (nemo_desktop_window_new (monitor));
    } else {
        window = GTK_WIDGET (nemo_blank_desktop_window_new (monitor));
    }

    info->window = window;

    /* We realize it immediately so that the NEMO_DESKTOP_WINDOW_ID
       property is set so gnome-settings-daemon doesn't try to set the
       background. And we do a gdk_flush() to be sure X gets it. */

    gtk_widget_realize (GTK_WIDGET (window));
    gdk_flush ();

    if (manager->scale_factor_changed_id == 0) {
        manager->scale_factor_changed_id = g_signal_connect (window,
                                                             "notify::scale-factor",
                                                             G_CALLBACK (on_window_scale_changed),
                                                             manager);
    }

    gtk_application_add_window (GTK_APPLICATION (nemo_application_get_singleton ()),
                                GTK_WINDOW (window));

    manager->desktops = g_list_append (manager->desktops, info);
}

static gboolean
layout_changed (NemoDesktopManager *manager)
{
    gint n_monitors = 0;
    gint x_primary = 0;
    gboolean show_desktop_on_primary = FALSE;
    gboolean show_desktop_on_remaining = FALSE;

    manager->update_layout_idle_id = 0;

    close_all_windows (manager);

    gchar *pref = g_settings_get_string (nemo_desktop_preferences, NEMO_PREFERENCES_DESKTOP_LAYOUT);

    if (g_strcmp0 (pref, "") == 0) {
        g_settings_set_string (nemo_desktop_preferences, NEMO_PREFERENCES_DESKTOP_LAYOUT, DESKTOPS_DEFAULT);
        g_free (pref);
        layout_changed (manager);
        return FALSE;
    }

    gchar **pref_split = g_strsplit (pref, "::", 2);

    if (g_strv_length (pref_split) != 2) {
        g_settings_set_string (nemo_desktop_preferences, NEMO_PREFERENCES_DESKTOP_LAYOUT, DESKTOPS_DEFAULT);
        g_free (pref);
        g_strfreev (pref_split);
        layout_changed (manager);
        return FALSE;;
    }

    n_monitors = gdk_screen_get_n_monitors (manager->screen);
    x_primary = gdk_screen_get_primary_monitor (manager->screen);

    show_desktop_on_primary = g_strcmp0 (pref_split[0], "true") == 0;
    show_desktop_on_remaining = g_strcmp0 (pref_split[1], "true") == 0;

    manager->desktop_on_primary_only = show_desktop_on_primary && !show_desktop_on_remaining;

    gint i = 0;
    gboolean primary_set = FALSE;

    for (i = 0; i < n_monitors; i++) {
        if (i == x_primary) {
            create_new_desktop_window (manager, i, show_desktop_on_primary, show_desktop_on_primary);
            primary_set = primary_set || show_desktop_on_primary;
        } else if (!nemo_desktop_utils_get_monitor_cloned (i, x_primary)) {
            gboolean set_layout_primary = !primary_set && !show_desktop_on_primary && show_desktop_on_remaining;
            create_new_desktop_window (manager, i, set_layout_primary, show_desktop_on_remaining);
            primary_set = primary_set || set_layout_primary;
        }
    }

    g_free (pref);
    g_strfreev (pref_split);

    return FALSE;
}

static void
nemo_desktop_manager_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
nemo_desktop_manager_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
nemo_desktop_manager_constructed (GObject *object)
{
    G_OBJECT_CLASS (nemo_desktop_manager_parent_class)->constructed (object);

    NemoDesktopManager *manager = NEMO_DESKTOP_MANAGER (object);

    manager->screen = gdk_screen_get_default ();
    manager->update_layout_idle_id = 0;

    manager->show_desktop_changed_id = g_signal_connect_swapped (nemo_desktop_preferences, 
                                                                 "changed::" NEMO_PREFERENCES_SHOW_DESKTOP,
                                                                 G_CALLBACK (queue_update_layout),
                                                                 manager);

    manager->desktop_layout_changed_id = g_signal_connect_swapped (nemo_desktop_preferences,
                                                                   "changed::" NEMO_PREFERENCES_DESKTOP_LAYOUT,
                                                                   G_CALLBACK (queue_update_layout),
                                                                   manager);

    manager->size_changed_id = g_signal_connect_swapped (manager->screen,
                                                         "size_changed",
                                                         G_CALLBACK (queue_update_layout),
                                                         manager);

    manager->use_grid_changed_id = g_signal_connect_swapped (nemo_desktop_preferences,
                                                             "changed::" NEMO_PREFERENCES_USE_DESKTOP_GRID,
                                                             G_CALLBACK (queue_update_layout),
                                                             manager);

    /* Monitor the preference to have the desktop */
    /* point to the Unix home folder */

    manager->home_dir_changed_id = g_signal_connect_swapped (nemo_preferences,
                                                             "changed::" NEMO_PREFERENCES_DESKTOP_IS_HOME_DIR,
                                                             G_CALLBACK (queue_update_layout),
                                                             manager);

    manager->orphaned_icon_handling_id = g_signal_connect_swapped (nemo_preferences,
                                                                   "changed::" NEMO_PREFERENCES_SHOW_ORPHANED_DESKTOP_ICONS,
                                                                   G_CALLBACK (queue_update_layout),
                                                                   manager);

    layout_changed (manager);
}

static void
nemo_desktop_manager_dispose (GObject *object)
{
    NemoDesktopManager *manager = NEMO_DESKTOP_MANAGER (object);

    close_all_windows (manager);

    g_signal_handler_disconnect (nemo_desktop_preferences, manager->show_desktop_changed_id);
    g_signal_handler_disconnect (nemo_desktop_preferences, manager->desktop_layout_changed_id);
    g_signal_handler_disconnect (nemo_desktop_preferences, manager->use_grid_changed_id);
    g_signal_handler_disconnect (manager->screen, manager->size_changed_id);
    g_signal_handler_disconnect (nemo_preferences, manager->home_dir_changed_id);
    g_signal_handler_disconnect (nemo_preferences, manager->orphaned_icon_handling_id);

    G_OBJECT_CLASS (nemo_desktop_manager_parent_class)->dispose (object);
}

static void
nemo_desktop_manager_finalize (GObject *object)
{
    g_object_unref (NEMO_DESKTOP_MANAGER (object)->action_manager);

    G_OBJECT_CLASS (nemo_desktop_manager_parent_class)->finalize (object);
}

static void
nemo_desktop_manager_class_init (NemoDesktopManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = nemo_desktop_manager_get_property;
  object_class->set_property = nemo_desktop_manager_set_property;
  object_class->finalize = nemo_desktop_manager_finalize;
  object_class->dispose = nemo_desktop_manager_dispose;
  object_class->constructed = nemo_desktop_manager_constructed;
}

static void
nemo_desktop_manager_init (NemoDesktopManager *self)
{
    self->size_changed_id = 0;
    self->desktop_layout_changed_id = 0;
    self->show_desktop_changed_id = 0;
    self->home_dir_changed_id = 0;
    self->cinnamon_panel_layout_changed_id = 0;
    self->orphaned_icon_handling_id = 0;
    self->scale_factor_changed_id = 0;

    self->desktops = NULL;

    self->desktop_on_primary_only = FALSE;

    self->action_manager = nemo_action_manager_new ();
}

NemoDesktopManager*
nemo_desktop_manager_get (void)
{
    if (_manager == NULL) {
        _manager = g_object_new (NEMO_TYPE_DESKTOP_MANAGER, NULL);
    }

    return _manager;
}

gboolean
nemo_desktop_manager_has_desktop_windows (NemoDesktopManager *manager)
{
    GList *iter;
    gboolean ret = FALSE;

    g_return_val_if_fail (manager != NULL, FALSE);

    for (iter = manager->desktops; iter != NULL; iter = iter->next) {
        DesktopInfo *info = iter->data;

        if (info->shows_desktop) {
            ret = TRUE;
            break;
        }
    }

    return ret;
}

gboolean
nemo_desktop_manager_get_monitor_is_active (NemoDesktopManager *manager,
                                                          gint  monitor)
{
    GList *iter;
    gboolean ret = FALSE;

    g_return_val_if_fail (manager != NULL, FALSE);

    for (iter = manager->desktops; iter != NULL; iter = iter->next) {
        DesktopInfo *info = iter->data;

        if (info->monitor_num == monitor) {
            ret = info->shows_desktop;
            break;
        }
    }

    return ret;
}

gboolean
nemo_desktop_manager_get_monitor_is_primary (NemoDesktopManager *manager,
                                                           gint  monitor)
{
    GList *iter;
    gboolean ret = FALSE;

    g_return_val_if_fail (manager != NULL, FALSE);

    for (iter = manager->desktops; iter != NULL; iter = iter->next) {
        DesktopInfo *info = iter->data;

        if (info->monitor_num == monitor) {
            ret = info->is_primary;
            break;
        }
    }

    return ret;
}

gboolean
nemo_desktop_manager_get_primary_only (NemoDesktopManager *manager)
{
    return manager->desktop_on_primary_only;
}

NemoActionManager *
nemo_desktop_manager_get_action_manager (void)
{
    g_return_val_if_fail (_manager != NULL, NULL);

    return _manager->action_manager;
}

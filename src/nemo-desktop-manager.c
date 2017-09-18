/* nemo-desktop-manager.c */

#include <config.h>

#define DEBUG_FLAG NEMO_DEBUG_DESKTOP
#include <libnemo-private/nemo-debug.h>

#include "nemo-desktop-manager.h"
#include "nemo-blank-desktop-window.h"
#include "nemo-desktop-window.h"
#include "nemo-application.h"
#include "nemo-cinnamon-dbus.h"

#include <gdk/gdkx.h>
#include <stdio.h>

#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-desktop-utils.h>

static gboolean layout_changed (NemoDesktopManager *manager);

#define DESKTOPS_ON_PRIMARY "true::false"
#define DESKTOPS_ON_ALL "true::true"
#define DESKTOPS_ON_NON_PRIMARY "false::true"
#define DESKTOPS_ON_NONE "false::false"
#define DESKTOPS_DEFAULT DESKTOPS_ON_PRIMARY
#define PRIMARY_MONITOR 0

typedef enum {
    RUN_STATE_INIT = 0,
    RUN_STATE_STARTUP,
    RUN_STATE_RUNNING,
    RUN_STATE_FALLBACK
} RunState;

typedef struct {
    NemoCinnamonDBus *proxy;
    NemoActionManager *action_manager;

    GdkScreen *fallback_screen;

    GList *desktops;

    RunState current_run_state;

    guint desktop_on_primary_only : 1;
    guint other_desktop : 1;
    guint proxy_owned : 1;
    guint startup_complete : 1;

    guint update_layout_idle_id;
    guint failsafe_timeout_id;

    gulong scale_factor_changed_id;
    gulong name_owner_changed_id;
    gulong proxy_signals_id;
    gulong fallback_size_changed_id;
} NemoDesktopManagerPrivate;

struct _NemoDesktopManager
{
    GtkWindow parent_object;

    NemoDesktopManagerPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (NemoDesktopManager, nemo_desktop_manager, G_TYPE_OBJECT);

#define FETCH_PRIV(m) NemoDesktopManagerPrivate *priv = NEMO_DESKTOP_MANAGER (m)->priv;

typedef struct {
    GtkWidget *window;

    gint monitor_num;
    gboolean shows_desktop;
    gboolean is_primary;
} DesktopInfo;

static const gchar *
run_state_str (RunState state)
{
    switch (state) {
        case RUN_STATE_INIT:
            return "RunState.INIT";
        case RUN_STATE_STARTUP:
            return "RunState.STARTUP";
        case RUN_STATE_RUNNING:
            return "RunState.RUNNING";
        case RUN_STATE_FALLBACK:
            return "RunState.FALLBACK";
        default:
            g_assert_not_reached ();
    }
}

static void
free_info (DesktopInfo *info)
{
    g_return_if_fail (info != NULL);

    g_clear_pointer (&info->window, gtk_widget_destroy);
    g_slice_free (DesktopInfo, info);
}

static RunState
get_run_state (NemoDesktopManager *manager)
{
    FETCH_PRIV (manager);
    gint ret;
    GError *error;

    if (priv->other_desktop) {
        ret = RUN_STATE_FALLBACK;
        goto out;
    }

    if (priv->proxy == NULL || !priv->proxy_owned) {
        if (priv->failsafe_timeout_id > 0) {
            ret = RUN_STATE_INIT;
        } else {
            ret = RUN_STATE_FALLBACK;
        }
        
        goto out;
    }

    error = NULL;

    if (!nemo_cinnamon_dbus_call_get_run_state_sync (priv->proxy,
                                                     &ret,
                                                     NULL,
                                                     &error)) {

        DEBUG ("Attempting proxy call 'GetRunState' failed, resorting to fallback mode: %s", error->message);
        g_error_free (error);

        ret = RUN_STATE_FALLBACK;
        goto out;
    }

out:
    DEBUG ("Run state is %s", run_state_str (ret));

    return (RunState) ret;
}

static gint
get_n_monitors (NemoDesktopManager *manager)
{
    FETCH_PRIV (manager);
    gsize n_monitors, i;
    const gint *indices G_GNUC_UNUSED;
    GVariant *monitors;
    GError *error;

    if (priv->current_run_state == RUN_STATE_FALLBACK) {
        DEBUG ("Currently in fallback mode, retrieving n_monitors via GdkScreen");

        n_monitors = nemo_desktop_utils_get_num_monitors ();

        goto out;
    }

    error = NULL;

    if (!nemo_cinnamon_dbus_call_get_monitors_sync (priv->proxy,
                                                    &monitors,
                                                    NULL,
                                                    &error)) {
        DEBUG ("Attempting proxy call 'GetMonitors' failed, retrieving n_monitors via GdkScreen: %s", error->message);
        g_error_free (error);

        n_monitors = nemo_desktop_utils_get_num_monitors ();

        goto out;
    }

    DEBUG ("Proxy call to 'GetMonitors' succeeded");

    indices = g_variant_get_fixed_array (monitors, &n_monitors, sizeof(gint));
    g_variant_unref (monitors);

out:
    if (DEBUGGING) {
        GString *string = g_string_new (NULL);

        for (i = 0; i < n_monitors; i++) {
            gchar *m = g_strdup_printf (" %lu", i);

            string = g_string_append (string, m);
            g_free (m);
        }

        DEBUG ("Found %lu monitor(s):%s", n_monitors, string->str);

        g_string_free (string, TRUE);
    }

    return n_monitors;
}

static void
get_window_rect_for_monitor (NemoDesktopManager *manager,
                             gint                monitor,
                             GdkRectangle       *rect)
{
    FETCH_PRIV (manager);
    GVariant *out_rect_var;
    GdkRectangle out_rect;
    gsize n_elem;
    GError *error;

    error = NULL;
    out_rect_var = NULL;

    if (priv->current_run_state == RUN_STATE_FALLBACK) {
        DEBUG ("Currently in fallback mode, retrieving n_monitors via GdkScreen");

        nemo_desktop_utils_get_monitor_geometry (monitor, &out_rect);

        goto out;
    }

    if (!nemo_cinnamon_dbus_call_get_monitor_work_rect_sync (priv->proxy,
                                                             monitor,
                                                             &out_rect_var,
                                                             NULL,
                                                             &error)) {
        DEBUG ("Attempting proxy call 'GetMonitorWorkRect' failed, retrieving n_monitors via GdkScreen: %s", error->message);

        nemo_desktop_utils_get_monitor_geometry (monitor, &out_rect);

        goto out;
    }

    out_rect = *( (GdkRectangle *) g_variant_get_fixed_array (out_rect_var, &n_elem, sizeof(gint)) );

out:

    rect->x = out_rect.x;
    rect->y = out_rect.y;
    rect->width = out_rect.width;
    rect->height = out_rect.height;

    if (out_rect_var != NULL) {
        g_variant_unref (out_rect_var);
    }
}

static void
close_all_windows (NemoDesktopManager *manager)
{
    FETCH_PRIV (manager);

    g_list_foreach (priv->desktops, (GFunc) free_info, NULL);
    g_clear_pointer (&priv->desktops, g_list_free);
}

static void
queue_update_layout (NemoDesktopManager *manager)
{
    FETCH_PRIV (manager);

    if (priv->update_layout_idle_id > 0) {
        g_source_remove (priv->update_layout_idle_id);
        priv->update_layout_idle_id = 0;
    }

    priv->update_layout_idle_id = g_idle_add ((GSourceFunc) layout_changed, manager);
}

static void
on_window_scale_changed (GtkWidget          *window,
                         GParamSpec         *pspec,
                         NemoDesktopManager *manager)
{
    FETCH_PRIV (manager);

    priv->scale_factor_changed_id = 0;

    queue_update_layout (manager);
}

static void
create_new_desktop_window (NemoDesktopManager *manager,
                                         gint  monitor,
                                     gboolean  primary,
                                     gboolean  show_desktop)
{
    FETCH_PRIV (manager);
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

    if (priv->scale_factor_changed_id == 0) {
        priv->scale_factor_changed_id = g_signal_connect (window,
                                                          "notify::scale-factor",
                                                          G_CALLBACK (on_window_scale_changed),
                                                          manager);
    }

    gtk_application_add_window (GTK_APPLICATION (nemo_application_get_singleton ()),
                                GTK_WINDOW (window));

    priv->desktops = g_list_append (priv->desktops, info);
}

static gboolean
layout_changed (NemoDesktopManager *manager)
{
    FETCH_PRIV (manager);
    gint n_monitors = 0;
    gint x_primary = 0;
    gboolean show_desktop_on_primary = FALSE;
    gboolean show_desktop_on_remaining = FALSE;

    priv->update_layout_idle_id = 0;

    close_all_windows (manager);

    gchar *pref = g_settings_get_string (nemo_desktop_preferences, NEMO_PREFERENCES_DESKTOP_LAYOUT);

    if (g_strcmp0 (pref, "") == 0) {
        g_settings_set_string (nemo_desktop_preferences, NEMO_PREFERENCES_DESKTOP_LAYOUT, DESKTOPS_DEFAULT);
        g_free (pref);
        layout_changed (manager);
        return G_SOURCE_REMOVE;
    }

    gchar **pref_split = g_strsplit (pref, "::", 2);

    if (g_strv_length (pref_split) != 2) {
        g_settings_set_string (nemo_desktop_preferences, NEMO_PREFERENCES_DESKTOP_LAYOUT, DESKTOPS_DEFAULT);
        g_free (pref);
        g_strfreev (pref_split);
        layout_changed (manager);
        return G_SOURCE_REMOVE;
    }

    n_monitors = get_n_monitors (manager);
    x_primary = 0; /* always */

    show_desktop_on_primary = g_strcmp0 (pref_split[0], "true") == 0;
    show_desktop_on_remaining = g_strcmp0 (pref_split[1], "true") == 0;

    priv->desktop_on_primary_only = show_desktop_on_primary && !show_desktop_on_remaining;

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

    return G_SOURCE_REMOVE;
}

static void
on_bus_name_owner_changed (NemoDesktopManager *manager)
{
    FETCH_PRIV (manager);
    gchar *name_owner;

    g_return_if_fail (priv->proxy != NULL);

    name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (priv->proxy));

    priv->proxy_owned = name_owner != NULL;

    if (priv->proxy_owned) {
        if (priv->failsafe_timeout_id > 0) {
            g_source_remove (priv->failsafe_timeout_id);
            priv->failsafe_timeout_id = 0;
        }
    }

    DEBUG ("New name owner: %s", name_owner ? name_owner : "unowned");

    g_free (name_owner);
}

static void
on_run_state_changed (NemoDesktopManager *manager)
{
    g_return_if_fail (NEMO_IS_DESKTOP_MANAGER (manager));

    FETCH_PRIV (manager);
    RunState new_state;

    DEBUG ("New run state...");

    /* If we're already running (showing icons,) there's no
     * change in behavior, we just keep showing. */
    if (priv->current_run_state == RUN_STATE_RUNNING) {
        return;
    }

    new_state = get_run_state (manager);

    /* If our state is INIT, we're waiting for the proxy to
     * get picked up (cinnamon starting) and still within our
     * failsafe timeout, so we just return */
    if (new_state == RUN_STATE_INIT) {
        priv->current_run_state = new_state;
        return;
    }

    /* If our state is STARTUP, RUNNING, or FAILSAFE, we can
     * cancel our failsafe timer.  We've either gotten a proxy
     * owner, given up waiting, or are now running */
    if (new_state > RUN_STATE_INIT) {
        if (priv->failsafe_timeout_id > 0) {
            g_source_remove (priv->failsafe_timeout_id);
            priv->failsafe_timeout_id = 0;
        }
    }

    /* RUNNING or FALLBACK is the final endpoint of the desktop startup
     * sequence.  Either way we trigger the desktop to start and release
     * our hold on the GApplication (the windows created in layout_changed
     * will keep the application alive from here on out.) */
    if (new_state == RUN_STATE_RUNNING || new_state == RUN_STATE_FALLBACK) {
        priv->current_run_state = new_state;
        layout_changed (manager);
        g_application_release (G_APPLICATION (nemo_application_get_singleton ()));
    }
}

static void
on_monitors_changed (NemoDesktopManager *manager)
{
    g_return_if_fail (NEMO_IS_DESKTOP_MANAGER (manager));

    FETCH_PRIV (manager);
    GList *l;

    DEBUG ("Monitors or workarea changed");

    if (get_run_state (manager) < RUN_STATE_RUNNING) {
        DEBUG ("...ignoring possibly bogus MonitorsChanged - we're not RUNNING or FALLBACK");
        return;
    }

    if (((guint) get_n_monitors (manager)) != g_list_length (priv->desktops)) {
        queue_update_layout (manager);
        return;
    }

    for (l = priv->desktops; l != NULL; l = l->next) {
        DesktopInfo *info = (DesktopInfo *) l->data;

        if (NEMO_IS_DESKTOP_WINDOW (info->window)) {
            nemo_desktop_window_update_geometry (NEMO_DESKTOP_WINDOW (info->window));
        }
        else
        if (NEMO_IS_BLANK_DESKTOP_WINDOW (info->window)) {
            nemo_blank_desktop_window_update_geometry (NEMO_BLANK_DESKTOP_WINDOW (info->window));
        }
    }
}

static void
on_proxy_signal (GDBusProxy *proxy,
                 gchar      *sender,
                 gchar      *signal_name,
                 GVariant   *params,
                 gpointer   *user_data)
{
    if (g_strcmp0 (signal_name, "RunStateChanged") == 0) {
        on_run_state_changed (NEMO_DESKTOP_MANAGER (user_data));
    } 
    else
    if (g_strcmp0 (signal_name, "MonitorsChanged") == 0) {
        on_monitors_changed (NEMO_DESKTOP_MANAGER (user_data));
    }
}

static gboolean
on_failsafe_timeout (NemoDesktopManager *manager)
{
    g_return_val_if_fail (NEMO_IS_DESKTOP_MANAGER (manager), FALSE);

    FETCH_PRIV (manager);

    /* Our failsafe timeout is up, we'll zero out out id and trigger
     * on_run_state_changed.  A combination of no proxy, no owner and
     * no timeout id will put us in FALLBACK mode */

    g_warning ("nemo-desktop: Desktop failsafe timeout reached, applying fallback behavior");

    priv->failsafe_timeout_id = 0;

    on_run_state_changed (manager);

    return G_SOURCE_REMOVE;
}

static void
connect_fallback_signals (NemoDesktopManager *manager)
{
    FETCH_PRIV (manager);

    priv->fallback_screen = gdk_screen_get_default ();

    priv->fallback_size_changed_id = g_signal_connect_swapped (priv->fallback_screen,
                                                               "size_changed",
                                                               G_CALLBACK (queue_update_layout),
                                                               manager);
}

static void
on_proxy_created (GObject      *source,
                  GAsyncResult *res,
                  gpointer      user_data)
{
    NemoDesktopManager *manager = NEMO_DESKTOP_MANAGER (user_data);
    FETCH_PRIV (manager);

    priv->proxy = nemo_cinnamon_dbus_proxy_new_for_bus_finish (res, NULL);

    if (priv->proxy == NULL) {
        g_warning ("Cinnamon proxy unsuccessful, applying default behavior");

        /* We should always end up with a proxy, as long as dbus itself is working.. */
        priv->other_desktop = TRUE;
        return;
    }

    DEBUG ("Cinnamon proxy established, getting owner and state");

    priv->name_owner_changed_id = g_signal_connect_swapped (priv->proxy,
                                                            "notify::g-name-owner",
                                                            G_CALLBACK (on_bus_name_owner_changed),
                                                            manager);

    priv->proxy_signals_id = g_signal_connect (priv->proxy,
                                               "g-signal",
                                               G_CALLBACK (on_proxy_signal),
                                               manager);

    on_bus_name_owner_changed (manager);

    if (!priv->proxy_owned) {
        priv->failsafe_timeout_id = g_timeout_add_seconds (5, (GSourceFunc) on_failsafe_timeout, manager);
    }

    on_run_state_changed (manager);
}


static void
nemo_desktop_manager_dispose (GObject *object)
{
    NemoDesktopManager *manager = NEMO_DESKTOP_MANAGER (object);
    FETCH_PRIV (manager);

    DEBUG ("Disposing NemoDesktopManager");

    close_all_windows (manager);

    g_signal_handlers_disconnect_by_func (nemo_desktop_preferences, queue_update_layout, manager);
    g_signal_handlers_disconnect_by_func (nemo_preferences, queue_update_layout, manager);

    if (priv->fallback_size_changed_id > 0) {
        g_signal_handler_disconnect (priv->fallback_screen, priv->fallback_size_changed_id);
        priv->fallback_size_changed_id = 0;
    }

    G_OBJECT_CLASS (nemo_desktop_manager_parent_class)->dispose (object);
}

static void
nemo_desktop_manager_finalize (GObject *object)
{
    FETCH_PRIV (object);

    g_object_unref (priv->action_manager);
    g_object_unref (priv->proxy);

    DEBUG ("Finalizing NemoDesktopManager");

    G_OBJECT_CLASS (nemo_desktop_manager_parent_class)->finalize (object);
}

static void
nemo_desktop_manager_class_init (NemoDesktopManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = nemo_desktop_manager_finalize;
  object_class->dispose = nemo_desktop_manager_dispose;
}

static void
nemo_desktop_manager_init (NemoDesktopManager *manager)
{
    NemoDesktopManagerPrivate *priv;

    manager->priv = G_TYPE_INSTANCE_GET_PRIVATE (manager, NEMO_TYPE_DESKTOP_MANAGER, NemoDesktopManagerPrivate);

    DEBUG ("Desktop Manager Initialization");

    priv = manager->priv;

    priv->scale_factor_changed_id = 0;
    priv->desktops = NULL;
    priv->desktop_on_primary_only = FALSE;

    priv->action_manager = nemo_action_manager_new ();

    priv->update_layout_idle_id = 0;

    g_signal_connect_swapped (nemo_desktop_preferences, 
                              "changed::" NEMO_PREFERENCES_SHOW_DESKTOP,
                              G_CALLBACK (queue_update_layout),
                              manager);

    g_signal_connect_swapped (nemo_desktop_preferences,
                              "changed::" NEMO_PREFERENCES_DESKTOP_LAYOUT,
                              G_CALLBACK (queue_update_layout),
                              manager);

    g_signal_connect_swapped (nemo_desktop_preferences,
                              "changed::" NEMO_PREFERENCES_USE_DESKTOP_GRID,
                              G_CALLBACK (queue_update_layout),
                              manager);

    /* Monitor the preference to have the desktop */
    /* point to the Unix home folder */

    g_signal_connect_swapped (nemo_preferences,
                              "changed::" NEMO_PREFERENCES_DESKTOP_IS_HOME_DIR,
                              G_CALLBACK (queue_update_layout),
                              manager);

    g_signal_connect_swapped (nemo_preferences,
                              "changed::" NEMO_PREFERENCES_SHOW_ORPHANED_DESKTOP_ICONS,
                              G_CALLBACK (queue_update_layout),
                              manager);

    /* If we're a cinnamon session, increase the use count temporarily for the application,
     * and establish a proxy for org.Cinnamon.  The hold prevents the GApplication from simply
     * exiting while waiting for the GAsyncReadyCallback.
     * 
     * If we're not running cinnamon,  */

    if (g_strcmp0 (g_getenv ("XDG_SESSION_DESKTOP"), "cinnamon") == 0) {
        DEBUG ("XDG_SESSION_DESKTOP is cinnamon, establishing proxy");

        g_application_hold (G_APPLICATION (nemo_application_get_singleton ()));

        nemo_cinnamon_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                              G_DBUS_PROXY_FLAGS_NONE,
                                              "org.Cinnamon",
                                              "/org/Cinnamon",
                                              NULL,
                                              (GAsyncReadyCallback) on_proxy_created,
                                              manager);
    } else {
        DEBUG ("XDG_SESSION_DESKTOP is not cinnamon, applying default behavior");

        priv->other_desktop = TRUE;
        connect_fallback_signals (manager);

        on_run_state_changed (manager);
    }
}

static NemoDesktopManager *_manager = NULL;

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
    FETCH_PRIV (manager);

    GList *iter;
    gboolean ret = FALSE;

    g_return_val_if_fail (manager != NULL, FALSE);

    for (iter = priv->desktops; iter != NULL; iter = iter->next) {
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
    FETCH_PRIV (manager);
    GList *iter;
    gboolean ret = FALSE;

    g_return_val_if_fail (manager != NULL, FALSE);

    for (iter = priv->desktops; iter != NULL; iter = iter->next) {
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
    FETCH_PRIV (manager);
    GList *iter;
    gboolean ret = FALSE;

    g_return_val_if_fail (manager != NULL, FALSE);

    for (iter = priv->desktops; iter != NULL; iter = iter->next) {
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
    FETCH_PRIV (manager);

    return priv->desktop_on_primary_only;
}

NemoActionManager *
nemo_desktop_manager_get_action_manager (void)
{
    g_return_val_if_fail (_manager != NULL, NULL);

    return _manager->priv->action_manager;
}

void
nemo_desktop_manager_get_window_rect_for_monitor (NemoDesktopManager *manager,
                                                  gint                monitor,
                                                  GdkRectangle       *rect)
{
    g_return_if_fail (manager != NULL);

    get_window_rect_for_monitor (manager, monitor, rect);
}

void
nemo_desktop_manager_get_margins (NemoDesktopManager *manager,
                                  gint                monitor,
                                  gint               *left,
                                  gint               *right,
                                  gint               *top,
                                  gint               *bottom)
{
    FETCH_PRIV (manager);
    GdkRectangle work_rect, geometry;

    /* We don't use margins if we have reliable work area
     * info (e.g. having an active Cinnamon session) */

    if (priv->proxy_owned && !priv->other_desktop) {
        *left = *right = *top = *bottom = 0;

        return;
    }

    /* _NET_WORKAREA only applies to the primary monitor - use it to adjust
       container margins on the primary icon container only.  For any others,
       add a sane amount of padding for any likely chrome. */

    if (monitor != nemo_desktop_utils_get_primary_monitor ()) {
        *left = *right = *top = *bottom = 25;

        return;
    }

    nemo_desktop_utils_get_monitor_geometry (monitor, &geometry);
    nemo_desktop_utils_get_monitor_work_rect (monitor, &work_rect);

    *left = work_rect.x - geometry.x;
    *right = (geometry.x + geometry.width) - (work_rect.x + work_rect.width);
    *top = work_rect.y - geometry.y;
    *bottom = (geometry.y + geometry.height) - (work_rect.y + work_rect.height);
}

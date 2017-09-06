/* nemo-desktop-manager.h */

#ifndef _NEMO_DESKTOP_MANAGER_H
#define _NEMO_DESKTOP_MANAGER_H

#include <glib-object.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "libnemo-private/nemo-action-manager.h"

G_BEGIN_DECLS

#define NEMO_TYPE_DESKTOP_MANAGER nemo_desktop_manager_get_type ()

G_DECLARE_FINAL_TYPE (NemoDesktopManager, nemo_desktop_manager, NEMO, DESKTOP_MANAGER, GObject)

NemoDesktopManager* nemo_desktop_manager_get (void);

gboolean nemo_desktop_manager_has_desktop_windows (NemoDesktopManager *manager);
gboolean nemo_desktop_manager_get_monitor_is_active (NemoDesktopManager *manager,
                                                                   gint  monitor);
gboolean nemo_desktop_manager_get_monitor_is_primary (NemoDesktopManager *manager,
                                                                   gint  monitor);

gboolean nemo_desktop_manager_get_primary_only (NemoDesktopManager *manager);
void     nemo_desktop_manager_get_window_rect_for_monitor (NemoDesktopManager *manager,
                                                           gint                monitor,
                                                           GdkRectangle       *rect);
gboolean nemo_desktop_manager_has_good_workarea_info (NemoDesktopManager *manager);

void     nemo_desktop_manager_get_margins             (NemoDesktopManager *manager,
                                                       gint                monitor,
                                                       gint               *left,
                                                       gint               *right,
                                                       gint               *top,
                                                       gint               *bottom);

G_END_DECLS

#endif /* _NEMO_DESKTOP_MANAGER_H */

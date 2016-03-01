/* nemo-desktop-manager.h */

#ifndef _NEMO_DESKTOP_MANAGER_H
#define _NEMO_DESKTOP_MANAGER_H

#include <glib-object.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "libnemo-private/nemo-action-manager.h"

G_BEGIN_DECLS

#define NEMO_TYPE_DESKTOP_MANAGER nemo_desktop_manager_get_type()

#define NEMO_DESKTOP_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  NEMO_TYPE_DESKTOP_MANAGER, NemoDesktopManager))

#define NEMO_DESKTOP_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  NEMO_TYPE_DESKTOP_MANAGER, NemoDesktopManagerClass))

#define NEMO_IS_DESKTOP_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  NEMO_TYPE_DESKTOP_MANAGER))

#define NEMO_IS_DESKTOP_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  NEMO_TYPE_DESKTOP_MANAGER))

#define NEMO_DESKTOP_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  NEMO_TYPE_DESKTOP_MANAGER, NemoDesktopManagerClass))

typedef struct {
  GObject parent;

  GdkScreen *screen;
  GdkWindow *root_window;

  gulong size_changed_id;
  gulong desktop_layout_changed_id;
  gulong show_desktop_changed_id;
  gulong home_dir_changed_id;
  gulong cinnamon_panel_layout_changed_id;
  gulong orphaned_icon_handling_id;

  NemoActionManager *action_manager;

  GList *desktops;

} NemoDesktopManager;

typedef struct {
  GObjectClass parent_class;
} NemoDesktopManagerClass;

GType nemo_desktop_manager_get_type (void);

NemoDesktopManager* nemo_desktop_manager_get (void);
gboolean nemo_desktop_manager_has_desktop_windows (NemoDesktopManager *manager);
gboolean nemo_desktop_manager_get_monitor_is_active (NemoDesktopManager *manager,
                                                                   gint  monitor);
gboolean nemo_desktop_manager_get_monitor_is_primary (NemoDesktopManager *manager,
                                                                   gint  monitor);

G_END_DECLS

#endif /* _NEMO_DESKTOP_MANAGER_H */

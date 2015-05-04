/* nemo-plugin-manager.h */

/*  A GtkWidget that can be inserted into a UI that provides a simple interface for
 *  managing the loading of extensions, actions and scripts
 */

#ifndef __NEMO_PLUGIN_MANAGER_H__
#define __NEMO_PLUGIN_MANAGER_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include "nemo-action-config-widget.h"
#include "nemo-script-config-widget.h"
#include "nemo-extension-config-widget.h"

G_BEGIN_DECLS

#define NEMO_TYPE_PLUGIN_MANAGER (nemo_plugin_manager_get_type())

#define NEMO_PLUGIN_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_PLUGIN_MANAGER, NemoPluginManager))
#define NEMO_PLUGIN_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_PLUGIN_MANAGER, NemoPluginManagerClass))
#define NEMO_IS_PLUGIN_MANAGER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_PLUGIN_MANAGER))
#define NEMO_IS_PLUGIN_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_PLUGIN_MANAGER))
#define NEMO_PLUGIN_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_PLUGIN_MANAGER, NemoPluginManagerClass))

typedef struct _NemoPluginManager NemoPluginManager;
typedef struct _NemoPluginManagerClass NemoPluginManagerClass;

struct _NemoPluginManager
{
  GtkBin parent;
  GtkWidget *grid;

  GtkWidget *action_widget;
  GtkWidget *script_widget;
  GtkWidget *extension_widget;
};

struct _NemoPluginManagerClass
{
  GtkBinClass parent_class;
};

GType nemo_plugin_manager_get_type (void);

NemoPluginManager       *nemo_plugin_manager_new                   (void);
void                     nemo_plugin_manager_show                  (void);
GtkWidget               *nemo_plugin_manager_get_grid              (NemoPluginManager *manager);
GtkWidget               *nemo_plugin_manager_get_action_widget     (NemoPluginManager *manager);
GtkWidget               *nemo_plugin_manager_get_script_widget     (NemoPluginManager *manager);
GtkWidget               *nemo_plugin_manager_get_extension_widget  (NemoPluginManager *manager);

G_END_DECLS

#endif /* __NEMO_PLUGIN_MANAGER_H__ */

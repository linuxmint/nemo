/* nemo-plugin-manager.h */

/*  A GtkWidget that can be inserted into a UI that provides a simple interface for
 *  managing the loading of extensions, actions and scripts
 */

#ifndef __NEMO_PLUGIN_MANAGER_H__
#define __NEMO_PLUGIN_MANAGER_H__

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NEMO_TYPE_PLUGIN_MANAGER (nemo_plugin_manager_get_type())

G_DECLARE_FINAL_TYPE (NemoPluginManager, nemo_plugin_manager, NEMO, PLUGIN_MANAGER, GtkBin)

NemoPluginManager       *nemo_plugin_manager_new                   (void);

G_END_DECLS

#endif /* __NEMO_PLUGIN_MANAGER_H__ */

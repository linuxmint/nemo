/* nemo-extension-config-widget.h */

/*  A widget that displays a list of extensions to enable or disable.
 *  This is usually part of a NemoPluginManagerWidget
 */

#ifndef __NEMO_EXTENSION_CONFIG_WIDGET_H__
#define __NEMO_EXTENSION_CONFIG_WIDGET_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "nemo-config-base-widget.h"

G_BEGIN_DECLS

#define NEMO_TYPE_EXTENSION_CONFIG_WIDGET (nemo_extension_config_widget_get_type())

#define NEMO_EXTENSION_CONFIG_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_EXTENSION_CONFIG_WIDGET, NemoExtensionConfigWidget))
#define NEMO_EXTENSION_CONFIG_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_EXTENSION_CONFIG_WIDGET, NemoExtensionConfigWidgetClass))
#define NEMO_IS_EXTENSION_CONFIG_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_EXTENSION_CONFIG_WIDGET))
#define NEMO_IS_EXTENSION_CONFIG_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_EXTENSION_CONFIG_WIDGET))
#define NEMO_EXTENSION_CONFIG_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_EXTENSION_CONFIG_WIDGET, NemoExtensionConfigWidgetClass))

typedef struct _NemoExtensionConfigWidget NemoExtensionConfigWidget;
typedef struct _NemoExtensionConfigWidgetClass NemoExtensionConfigWidgetClass;

struct _NemoExtensionConfigWidget
{
  NemoConfigBaseWidget parent;
  GtkWidget *restart_button;

  GList *current_extensions;
  GList *initial_extension_ids;

  gulong bl_handler;
};

struct _NemoExtensionConfigWidgetClass
{
  NemoConfigBaseWidgetClass parent_class;
};

GType nemo_extension_config_widget_get_type (void);

GtkWidget  *nemo_extension_config_widget_new                   (void);

G_END_DECLS

#endif /* __NEMO_EXTENSION_CONFIG_WIDGET_H__ */

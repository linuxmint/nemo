/* nemo-script-config-widget.h */

/*  A widget that displays a list of scripts to enable or disable.
 *  This is usually part of a NemoPluginManagerWidget
 */

#ifndef __NEMO_SCRIPT_CONFIG_WIDGET_H__
#define __NEMO_SCRIPT_CONFIG_WIDGET_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "nemo-config-base-widget.h"

G_BEGIN_DECLS

#define NEMO_TYPE_SCRIPT_CONFIG_WIDGET (nemo_script_config_widget_get_type())

#define NEMO_SCRIPT_CONFIG_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_SCRIPT_CONFIG_WIDGET, NemoScriptConfigWidget))
#define NEMO_SCRIPT_CONFIG_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_SCRIPT_CONFIG_WIDGET, NemoScriptConfigWidgetClass))
#define NEMO_IS_SCRIPT_CONFIG_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_SCRIPT_CONFIG_WIDGET))
#define NEMO_IS_SCRIPT_CONFIG_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_SCRIPT_CONFIG_WIDGET))
#define NEMO_SCRIPT_CONFIG_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_SCRIPT_CONFIG_WIDGET, NemoScriptConfigWidgetClass))

typedef struct _NemoScriptConfigWidget NemoScriptConfigWidget;
typedef struct _NemoScriptConfigWidgetClass NemoScriptConfigWidgetClass;

struct _NemoScriptConfigWidget
{
  NemoConfigBaseWidget parent;

  GList *scripts;

  GList *dir_monitors;
  gulong bl_handler;
};

struct _NemoScriptConfigWidgetClass
{
  NemoConfigBaseWidgetClass parent_class;
};

GType nemo_script_config_widget_get_type (void);

GtkWidget  *nemo_script_config_widget_new                   (void);

G_END_DECLS

#endif /* __NEMO_SCRIPT_CONFIG_WIDGET_H__ */

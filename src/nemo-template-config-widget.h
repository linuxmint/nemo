/* nemo-script-config-widget.h */

/*  A widget that displays a list of scripts to enable or disable.
 *  This is usually part of a NemoPluginManagerWidget
 */

#ifndef __NEMO_TEMPLATE_CONFIG_WIDGET_H__
#define __NEMO_TEMPLATE_CONFIG_WIDGET_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "nemo-config-base-widget.h"

G_BEGIN_DECLS

#define NEMO_TYPE_TEMPLATE_CONFIG_WIDGET (nemo_template_config_widget_get_type())

#define NEMO_TEMPLATE_CONFIG_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_TEMPLATE_CONFIG_WIDGET, NemoTemplateConfigWidget))
#define NEMO_TEMPLATE_CONFIG_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_TEMPLATE_CONFIG_WIDGET, NemoTemplateConfigWidgetClass))
#define NEMO_IS_TEMPLATE_CONFIG_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_TEMPLATE_CONFIG_WIDGET))
#define NEMO_IS_TEMPLATE_CONFIG_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_TEMPLATE_CONFIG_WIDGET))
#define NEMO_TEMPLATE_CONFIG_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_TEMPLATE_CONFIG_WIDGET, NemoTemplateConfigWidgetClass))

typedef struct _NemoTemplateConfigWidget NemoTemplateConfigWidget;
typedef struct _NemoTemplateConfigWidgetClass NemoTemplateConfigWidgetClass;

struct _NemoTemplateConfigWidget
{
  NemoConfigBaseWidget parent;

  GList *templates;

  GList *dir_monitors;
  GtkWidget *remove_button;
  GtkWidget *rename_button;
  GtkWidget *edit_button;
};

struct _NemoTemplateConfigWidgetClass
{
  NemoConfigBaseWidgetClass parent_class;
};

GType nemo_template_config_widget_get_type (void);

GtkWidget  *nemo_template_config_widget_new                   (void);

G_END_DECLS

#endif /* __NEMO_TEMPLATE_CONFIG_WIDGET_H__ */

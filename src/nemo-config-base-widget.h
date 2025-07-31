/* nemo-config-base-widget.h */

/*  A base widget class for extension/action/script config widgets.
 *  This is usually part of a NemoPluginManagerWidget
 */

#ifndef __NEMO_CONFIG_BASE_WIDGET_H__
#define __NEMO_CONFIG_BASE_WIDGET_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "nemo-window-private.h"

G_BEGIN_DECLS

#define NEMO_TYPE_CONFIG_BASE_WIDGET (nemo_config_base_widget_get_type())

#define NEMO_CONFIG_BASE_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_CONFIG_BASE_WIDGET, NemoConfigBaseWidget))
#define NEMO_CONFIG_BASE_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_CONFIG_BASE_WIDGET, NemoConfigBaseWidgetClass))
#define NEMO_IS_CONFIG_BASE_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_CONFIG_BASE_WIDGET))
#define NEMO_IS_CONFIG_BASE_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_CONFIG_BASE_WIDGET))
#define NEMO_CONFIG_BASE_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_CONFIG_BASE_WIDGET, NemoConfigBaseWidgetClass))

typedef struct _NemoConfigBaseWidget NemoConfigBaseWidget;
typedef struct _NemoConfigBaseWidgetClass NemoConfigBaseWidgetClass;

struct _NemoConfigBaseWidget
{
  GtkBin parent;

  GtkWidget *label;
  GtkWidget *listbox;
  GtkWidget *lbuttonbox;
  GtkWidget *rbuttonbox;
  GtkWidget *enable_button;
  GtkWidget *disable_button;
};

struct _NemoConfigBaseWidgetClass
{
  GtkBinClass parent_class;
};

GType nemo_config_base_widget_get_type (void);

GtkWidget *nemo_config_base_widget_get_label          (NemoConfigBaseWidget *widget);
GtkWidget *nemo_config_base_widget_get_listbox        (NemoConfigBaseWidget *widget);
GtkWidget *nemo_config_base_widget_get_enable_button  (NemoConfigBaseWidget *widget);
GtkWidget *nemo_config_base_widget_get_disable_button (NemoConfigBaseWidget *widget);

void       nemo_config_base_widget_set_default_buttons_sensitive (NemoConfigBaseWidget *widget, gboolean sensitive);

void       nemo_config_base_widget_clear_list         (NemoConfigBaseWidget *widget);
NemoWindow *nemo_config_base_widget_get_view_window   (NemoConfigBaseWidget *widget, NemoWindow *view_window);

G_END_DECLS

#endif /* __NEMO_CONFIG_BASE_WIDGET_H__ */

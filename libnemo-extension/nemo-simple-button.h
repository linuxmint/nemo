/* nemo-simple-button.h */

#ifndef __NEMO_SIMPLE_BUTTON_H__
#define __NEMO_SIMPLE_BUTTON_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include "nemo-extension-types.h"

G_BEGIN_DECLS

#define NEMO_TYPE_SIMPLE_BUTTON (nemo_simple_button_get_type())

#define NEMO_SIMPLE_BUTTON(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_SIMPLE_BUTTON, NemoSimpleButton))
#define NEMO_SIMPLE_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_SIMPLE_BUTTON, NemoSimpleButtonClass))
#define NEMO_IS_SIMPLE_BUTTON(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_SIMPLE_BUTTON))
#define NEMO_IS_SIMPLE_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_SIMPLE_BUTTON))
#define NEMO_SIMPLE_BUTTON_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_SIMPLE_BUTTON, NemoSimpleButtonClass))

typedef struct _NemoSimpleButton NemoSimpleButton;
typedef struct _NemoSimpleButtonClass NemoSimpleButtonClass;

struct _NemoSimpleButton
{
  GtkButton parent;
};

struct _NemoSimpleButtonClass
{
  GtkButtonClass parent_class;
};

GType nemo_simple_button_get_type (void);

NemoSimpleButton *nemo_simple_button_new (void);
NemoSimpleButton *nemo_simple_button_new_from_icon_name (const gchar *icon_name, int icon_size);
NemoSimpleButton *nemo_simple_button_new_from_stock (const gchar *stock_id, int icon_size);
NemoSimpleButton *nemo_simple_button_new_from_file (const gchar *path, int icon_size);

G_END_DECLS

#endif /* __NEMO_SIMPLE_BUTTON_H__ */

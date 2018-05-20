/* nemo-simple-button.h */

#ifndef __NEMO_SIMPLE_BUTTON_H__
#define __NEMO_SIMPLE_BUTTON_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include "nemo-extension-types.h"

G_BEGIN_DECLS

#define NEMO_TYPE_SIMPLE_BUTTON nemo_simple_button_get_type()

G_DECLARE_FINAL_TYPE (NemoSimpleButton, nemo_simple_button, NEMO, SIMPLE_BUTTON, GtkButton)

NemoSimpleButton *nemo_simple_button_new (void);
NemoSimpleButton *nemo_simple_button_new_from_icon_name (const gchar *icon_name, int icon_size);
NemoSimpleButton *nemo_simple_button_new_from_stock (const gchar *stock_id, int icon_size);
NemoSimpleButton *nemo_simple_button_new_from_file (const gchar *path, int icon_size);

G_END_DECLS

#endif /* __NEMO_SIMPLE_BUTTON_H__ */

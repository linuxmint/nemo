/*
 * nemo-file-chooser-dbus: Implementation for the org.Nemo FileChooser D-Bus interface
 */

#ifndef __NEMO_FILE_CHOOSER_DBUS_H__
#define __NEMO_FILE_CHOOSER_DBUS_H__

#include <glib-object.h>

#define NEMO_TYPE_FILE_CHOOSER_DBUS nemo_file_chooser_dbus_get_type()
#define NEMO_FILE_CHOOSER_DBUS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_FILE_CHOOSER_DBUS, NemoFileChooserDBus))
#define NEMO_FILE_CHOOSER_DBUS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_FILE_CHOOSER_DBUS, NemoFileChooserDBusClass))
#define NEMO_IS_FILE_CHOOSER_DBUS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_FILE_CHOOSER_DBUS))
#define NEMO_IS_FILE_CHOOSER_DBUS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_FILE_CHOOSER_DBUS))
#define NEMO_FILE_CHOOSER_DBUS_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_FILE_CHOOSER_DBUS, NemoFileChooserDBusClass))

typedef struct _NemoFileChooserDBus NemoFileChooserDBus;
typedef struct _NemoFileChooserDBusClass NemoFileChooserDBusClass;

GType nemo_file_chooser_dbus_get_type (void);
NemoFileChooserDBus * nemo_file_chooser_dbus_new (void);

#endif /* __NEMO_FILE_CHOOSER_DBUS_H__ */

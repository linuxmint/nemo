/*
 * nemo-file-chooser-dialog: Custom layout file picker dialog header
 */

#ifndef __NEMO_FILE_CHOOSER_DIALOG_H__
#define __NEMO_FILE_CHOOSER_DIALOG_H__

#include <gtk/gtk.h>

GtkWidget * nemo_file_chooser_dialog_new (const gchar *title,
                                          GtkFileChooserAction action,
                                          gboolean select_multiple,
                                          const gchar *initial_folder_uri,
                                          const gchar *suggested_name);

GSList * nemo_file_chooser_dialog_get_selected_uris (GtkDialog *dialog);
gchar *  nemo_file_chooser_dialog_get_selected_uri  (GtkDialog *dialog);

#endif /* __NEMO_FILE_CHOOSER_DIALOG_H__ */

/*
 * nemo-file-chooser-dbus: D-Bus service provider for org.Nemo.FileChooser
 */

#include <config.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "nemo-application.h"
#include "nemo-file-chooser-dbus.h"
#include "nemo-file-chooser-dialog.h"
#include "nemo-file-chooser-generated.h"

struct _NemoFileChooserDBus {
    GObject parent;
    guint owner_id;
    NemoFileChooser *skeleton;
};

struct _NemoFileChooserDBusClass {
    GObjectClass parent_class;
};

G_DEFINE_TYPE (NemoFileChooserDBus, nemo_file_chooser_dbus, G_TYPE_OBJECT);

typedef struct {
    GDBusMethodInvocation *invocation;
    GtkWidget *dialog;
    NemoFileChooser *skeleton;
} ResponseData;

static void
on_open_dialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
    ResponseData *data = user_data;
    GPtrArray *results = g_ptr_array_new_with_free_func (g_free);

    g_message ("Nemo Open Dialog Response ID: %d", response_id);

    if (response_id == GTK_RESPONSE_ACCEPT || response_id == GTK_RESPONSE_OK) {
        GSList *uris = nemo_file_chooser_dialog_get_selected_uris (dialog);
        GSList *l;
        for (l = uris; l != NULL; l = l->next) {
            g_message ("Nemo Dialog selected URI: %s", (const gchar *)l->data);
            g_ptr_array_add (results, g_strdup (l->data));
        }
        g_slist_free_full (uris, g_free);
    }

    g_ptr_array_add (results, NULL); // NULL terminator for const gchar *const * parameter

    nemo_file_chooser_complete_open_file (data->skeleton, data->invocation, (const gchar *const *)results->pdata);
    
    g_ptr_array_unref (results);
    gtk_widget_destroy (GTK_WIDGET (dialog));
    g_free (data);
}

static gboolean
handle_open_file_cb (NemoFileChooser *object,
                     GDBusMethodInvocation *invocation,
                     const gchar *title,
                     const gchar *const *filters,
                     gboolean multiselect,
                     gboolean directory,
                     const gchar *initial_folder,
                     gpointer user_data)
{
    GtkWidget *dialog;

    g_message ("handle_open_file_cb: title='%s', multiselect=%d, directory=%d, initial_folder='%s'",
               title ? title : "", multiselect, directory, initial_folder ? initial_folder : "");

    dialog = nemo_file_chooser_dialog_new (title,
                                          directory ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER : GTK_FILE_CHOOSER_ACTION_OPEN,
                                          multiselect,
                                          initial_folder,
                                          NULL);

    ResponseData *data = g_new0 (ResponseData, 1);
    data->invocation = invocation;
    data->dialog = dialog;
    data->skeleton = object;

    g_signal_connect (dialog, "response", G_CALLBACK (on_open_dialog_response), data);
    gtk_widget_show_all (dialog);

    return TRUE;
}

static void
on_save_dialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
    ResponseData *data = user_data;
    gchar *result = NULL;

    if (response_id == GTK_RESPONSE_ACCEPT || response_id == GTK_RESPONSE_OK) {
        result = nemo_file_chooser_dialog_get_selected_uri (dialog);
        if (result && *result) {
            g_autoptr(GFile) file = g_file_new_for_uri (result);
            if (g_file_query_exists (file, NULL)) {
                g_autofree gchar *basename = g_file_get_basename (file);
                GtkWidget *msg_dialog = gtk_message_dialog_new (GTK_WINDOW (dialog),
                                                                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                                GTK_MESSAGE_QUESTION,
                                                                GTK_BUTTONS_YES_NO,
                                                                _("A file named \"%s\" already exists. Do you want to replace it?"),
                                                                basename);
                gtk_window_set_title (GTK_WINDOW (msg_dialog), _("Replace File"));
                gint res = gtk_dialog_run (GTK_DIALOG (msg_dialog));
                gtk_widget_destroy (msg_dialog);
                
                if (res != GTK_RESPONSE_YES) {
                    g_free (result);
                    return;
                }
            }
        }
    }

    nemo_file_chooser_complete_save_file (data->skeleton, data->invocation, result ? result : "");
    
    g_free (result);
    gtk_widget_destroy (GTK_WIDGET (dialog));
    g_free (data);
}

static gboolean
handle_save_file_cb (NemoFileChooser *object,
                     GDBusMethodInvocation *invocation,
                     const gchar *title,
                     const gchar *initial_folder,
                     const gchar *suggested_name,
                     gpointer user_data)
{
    GtkWidget *dialog;

    dialog = nemo_file_chooser_dialog_new (title,
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          FALSE,
                                          initial_folder,
                                          suggested_name);

    ResponseData *data = g_new0 (ResponseData, 1);
    data->invocation = invocation;
    data->dialog = dialog;
    data->skeleton = object;

    g_signal_connect (dialog, "response", G_CALLBACK (on_save_dialog_response), data);
    gtk_widget_show_all (dialog);

    return TRUE;
}

static void
bus_acquired_cb (GDBusConnection *conn,
                 const gchar     *name,
                 gpointer         user_data)
{
    NemoFileChooserDBus *self = user_data;

    self->skeleton = nemo_file_chooser_skeleton_new ();

    g_signal_connect (self->skeleton, "handle-open-file",
                      G_CALLBACK (handle_open_file_cb), self);
    g_signal_connect (self->skeleton, "handle-save-file",
                      G_CALLBACK (handle_save_file_cb), self);

    g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->skeleton), conn, "/org/Nemo/FileChooser", NULL);
}

static void
name_acquired_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
}

static void
name_lost_cb (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
}

static void
nemo_file_chooser_dbus_dispose (GObject *object)
{
    NemoFileChooserDBus *self = (NemoFileChooserDBus *) object;

    if (self->owner_id != 0) {
        g_bus_unown_name (self->owner_id);
        self->owner_id = 0;
    }

    if (self->skeleton != NULL) {
        g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->skeleton));
        g_object_unref (self->skeleton);
        self->skeleton = NULL;
    }

    G_OBJECT_CLASS (nemo_file_chooser_dbus_parent_class)->dispose (object);
}

static void
nemo_file_chooser_dbus_class_init (NemoFileChooserDBusClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = nemo_file_chooser_dbus_dispose;
}

static void
nemo_file_chooser_dbus_init (NemoFileChooserDBus *self)
{
    self->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                    "org.Nemo.FileChooser",
                                    G_BUS_NAME_OWNER_FLAGS_NONE,
                                    bus_acquired_cb,
                                    name_acquired_cb,
                                    name_lost_cb,
                                    self,
                                    NULL);
}

NemoFileChooserDBus *
nemo_file_chooser_dbus_new (void)
{
    return g_object_new (nemo_file_chooser_dbus_get_type (), NULL);
}

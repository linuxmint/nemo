/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  nemo-mime-application-chooser.c: an mime-application chooser
 *
 *  Copyright (C) 2004 Novell, Inc.
 *  Copyright (C) 2007, 2010 Red Hat, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but APPLICATIONOUT ANY WARRANTY; applicationout even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along application the Gnome Library; see the file COPYING.LIB.  If not,
 *  write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 *  Boston, MA 02110-1335, USA.
 *
 *  Authors: Dave Camp <dave@novell.com>
 *           Alexander Larsson <alexl@redhat.com>
 *           Cosimo Cecchi <ccecchi@redhat.com>
 */

#include <config.h>
#include "nemo-mime-application-chooser.h"

#include "nemo-file.h"
#include "nemo-signaller.h"
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

struct _NemoMimeApplicationChooserDetails {
	GList *files;
	char *uri;

	char *content_type;

	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *set_as_default_button;
	GtkWidget *open_with_widget;
	GtkWidget *add_button;
    GtkWidget *custom_picker;
    GtkWidget *file_button;
    GAppInfo *custom_info;
    GtkWidget *custom_entry;

    GtkWidget *dialog_ok;
};

enum {
	PROP_CONTENT_TYPE = 1,
	PROP_URI,
	PROP_FILES,
    PROP_DIALOG_OK,
	NUM_PROPERTIES
};

#define DEFAULT_APPS "Default Applications"
#define ADDED_ASS "Added Associations"

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NemoMimeApplicationChooser, nemo_mime_application_chooser, GTK_TYPE_BOX);

static void
add_clicked_cb (GtkButton *button,
		gpointer user_data)
{
	NemoMimeApplicationChooser *chooser = user_data;
	GAppInfo *info;

    if (!chooser->details->custom_info) {
        info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (chooser->details->open_with_widget));
        g_app_info_set_as_last_used_for_type (info, chooser->details->content_type, NULL);
    } else {
        info = chooser->details->custom_info;
        g_app_info_set_as_last_used_for_type (info, chooser->details->content_type, NULL);
    }

	if (info == NULL)
		return;

    gtk_app_chooser_refresh (GTK_APP_CHOOSER (chooser->details->open_with_widget));
    gtk_entry_set_text (GTK_ENTRY (chooser->details->custom_entry), "");
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser->details->file_button), "");
    g_signal_emit_by_name (nemo_signaller_get_current (), "mime_data_changed");
}

static void
remove_clicked_cb (GtkMenuItem *item,
		   gpointer user_data)
{
	NemoMimeApplicationChooser *chooser = user_data;
	GError *error;
	GAppInfo *info;

	info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (chooser->details->open_with_widget));

	if (info) {
		error = NULL;
		if (!g_app_info_remove_supports_type (info,
						      chooser->details->content_type,
						      &error)) {
			eel_show_error_dialog (_("Could not forget association"),
					       error->message,
					       GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (chooser))));
			g_error_free (error);

		}

		gtk_app_chooser_refresh (GTK_APP_CHOOSER (chooser->details->open_with_widget));
		g_object_unref (info);
	}

	g_signal_emit_by_name (nemo_signaller_get_current (), "mime_data_changed");
}

static void
populate_popup_cb (GtkAppChooserWidget *widget,
		   GtkMenu *menu,
		   GAppInfo *app,
		   gpointer user_data)
{
	GtkWidget *item;
	NemoMimeApplicationChooser *chooser = user_data;

	if (g_app_info_can_remove_supports_type (app)) {
		item = gtk_menu_item_new_with_label (_("Forget association"));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		gtk_widget_show (item);

		g_signal_connect (item, "activate",
				  G_CALLBACK (remove_clicked_cb), chooser);
	}
}

static void
reset_clicked_cb (GtkButton *button,
                  gpointer   user_data)
{
	NemoMimeApplicationChooser *chooser;

	chooser = NEMO_MIME_APPLICATION_CHOOSER (user_data);

	g_app_info_reset_type_associations (chooser->details->content_type);
	gtk_app_chooser_refresh (GTK_APP_CHOOSER (chooser->details->open_with_widget));
    gtk_entry_set_text (GTK_ENTRY (chooser->details->custom_entry), "");
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser->details->file_button), "");
	g_signal_emit_by_name (nemo_signaller_get_current (), "mime_data_changed");
}

static void
set_as_default_clicked_cb (GtkButton *button,
			   gpointer user_data)
{
	NemoMimeApplicationChooser *chooser = user_data;
	GAppInfo *info;

    if (!chooser->details->custom_info) {
        info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (chooser->details->open_with_widget));
        g_app_info_set_as_default_for_type (info, chooser->details->content_type, NULL);
    } else {
        info = chooser->details->custom_info;
        g_app_info_set_as_default_for_type (info, chooser->details->content_type, NULL);
    }

    gtk_app_chooser_refresh (GTK_APP_CHOOSER (chooser->details->open_with_widget));
    gtk_entry_set_text (GTK_ENTRY (chooser->details->custom_entry), "");
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser->details->file_button), "");
    g_signal_emit_by_name (nemo_signaller_get_current (), "mime_data_changed");
}

static gint
app_compare (gconstpointer a,
	     gconstpointer b)
{
	return !g_app_info_equal (G_APP_INFO (a), G_APP_INFO (b));
}

static gboolean
app_info_can_add (GAppInfo *info,
		  const gchar *content_type)
{
	GList *recommended, *fallback;
	gboolean retval = FALSE;

	recommended = g_app_info_get_recommended_for_type (content_type);
	fallback = g_app_info_get_fallback_for_type (content_type);

	if (g_list_find_custom (recommended, info, app_compare)) {
		goto out;
	}

	if (g_list_find_custom (fallback, info, app_compare)) {
		goto out;
	}

	retval = TRUE;

 out:
	g_list_free_full (recommended, g_object_unref);
	g_list_free_full (fallback, g_object_unref);

	return retval;
}

static void
application_selected_cb (GtkAppChooserWidget *widget,
			 GAppInfo *info,
			 gpointer user_data)
{
	NemoMimeApplicationChooser *chooser = user_data;
	GAppInfo *default_app;

    gtk_entry_set_text (GTK_ENTRY (chooser->details->custom_entry), "");

	default_app = g_app_info_get_default_for_type (chooser->details->content_type, FALSE);
	gtk_widget_set_sensitive (chooser->details->set_as_default_button,
				  !g_app_info_equal (info, default_app));

	gtk_widget_set_sensitive (chooser->details->add_button,
				  app_info_can_add (info, chooser->details->content_type));

    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser->details->file_button), "");

    if (chooser->details->dialog_ok)
        gtk_widget_set_sensitive (chooser->details->dialog_ok, TRUE);

    g_object_unref (default_app);
}

static void
application_activated_cb (GtkAppChooserWidget *widget,
                                     GAppInfo *info,
                                     gpointer  user_data)
{
    NemoMimeApplicationChooser *chooser = user_data;

    if (chooser->details->dialog_ok) {
        gtk_window_activate_default (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (widget))));
    }
}

static gboolean
validate_entry (GtkEntry *entry, const gchar *str)
{
    gint argcp = 0;
    gchar **argvp = NULL;
    gboolean ret = FALSE;
    if (g_shell_parse_argv (str, &argcp, &argvp, NULL)) {
        if (argcp > 0) {
            gchar *path_exec = g_find_program_in_path (argvp[0]);
            if (path_exec) {
                ret = TRUE;
            } else {
                if (!g_file_test (str, G_FILE_TEST_IS_DIR) &&
                    g_file_test (str, G_FILE_TEST_IS_EXECUTABLE)) {
                    ret = TRUE;
                }
            }
            g_free (path_exec);
        }
        g_strfreev (argvp);
    }

    if (ret) {
        gtk_entry_set_icon_from_icon_name (entry,
                                           GTK_ENTRY_ICON_SECONDARY,
                                           "ok");
        gtk_entry_set_icon_tooltip_text (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         _("Valid executable."));
    } else {
        gtk_entry_set_icon_from_icon_name (entry,
                                           GTK_ENTRY_ICON_SECONDARY,
                                           "stop");
        gtk_entry_set_icon_tooltip_text (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         _("Not a valid executable.  Spaces in the file path must be escaped with backslash (\\)."));
    }

    return ret;
}

static gchar *
get_nice_name (const gchar *entry)
{
    gchar *exec_path;
    gchar *ret = NULL;
    gint argcp = 0;
    gchar **argvp = NULL;

    if (g_shell_parse_argv (entry, &argcp, &argvp, NULL)) {
        if (argcp > 0) {
            exec_path = argvp[0];
            GFile *file = g_file_new_for_path (exec_path);
            ret = g_file_get_basename (file);
            g_object_unref (file);
        }
        g_strfreev (argvp);
    }

    return ret;
}


static void
custom_entry_changed_cb (GtkEditable *entry, gpointer user_data)
{
    NemoMimeApplicationChooser *chooser = user_data;

    const gchar *entry_text = gtk_entry_get_text (GTK_ENTRY (entry));
    gboolean empty = g_strcmp0 (entry_text, "") == 0;

    if (!empty && validate_entry (GTK_ENTRY (entry), entry_text)) {
        GAppInfo *default_app;
        gchar *cl = g_strdup_printf ("%s", entry_text);

        GAppInfo *info = g_app_info_create_from_commandline (cl, get_nice_name (cl),
                                                             G_APP_INFO_CREATE_NONE, NULL);
        default_app = g_app_info_get_default_for_type (chooser->details->content_type, FALSE);
        gtk_widget_set_sensitive (chooser->details->set_as_default_button,
                      !g_app_info_equal (info, default_app));

        gtk_widget_set_sensitive (chooser->details->add_button,
                      app_info_can_add (info, chooser->details->content_type));

        g_object_unref (default_app);
        if (chooser->details->custom_info != NULL) {
            g_object_unref (chooser->details->custom_info);
            chooser->details->custom_info = NULL;
        }
        chooser->details->custom_info = info;

        if (chooser->details->dialog_ok)
            gtk_widget_set_sensitive (chooser->details->dialog_ok, TRUE);
    } else {
        if (chooser->details->custom_info != NULL) {
            g_object_unref (chooser->details->custom_info);
            chooser->details->custom_info = NULL;
        }

        if (empty) {
            gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                               GTK_ENTRY_ICON_SECONDARY,
                                               NULL);
            gtk_entry_set_icon_tooltip_text (GTK_ENTRY (entry),
                                             GTK_ENTRY_ICON_SECONDARY,
                                             NULL);
        }

        gtk_widget_set_sensitive (chooser->details->set_as_default_button, FALSE);
        gtk_widget_set_sensitive (chooser->details->add_button, FALSE);

        if (chooser->details->dialog_ok)
            gtk_widget_set_sensitive (chooser->details->dialog_ok, FALSE);
    }
}

static void
custom_app_set_cb (GtkFileChooserButton *button,
                   gpointer user_data)
{
    NemoMimeApplicationChooser *chooser = user_data;

    gchar *unescaped = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (button));
    gchar *escaped = eel_str_escape_spaces (unescaped);

    gtk_entry_set_text (GTK_ENTRY (chooser->details->custom_entry), escaped);
    gtk_widget_grab_focus (chooser->details->custom_entry);
    gtk_editable_set_position (GTK_EDITABLE (chooser->details->custom_entry), -1);

    g_free (unescaped);
    g_free (escaped);
}

static char *
get_extension (const char *basename)
{
	char *p;

	p = strrchr (basename, '.');

	if (p && *(p + 1) != '\0') {
		return g_strdup (p + 1);
	} else {
		return NULL;
	}
}

static gchar *
get_extension_from_file (NemoFile *nfile)
{
	char *name;
	char *extension;

	name = nemo_file_get_name (nfile);
	extension = get_extension (name);

	g_free (name);

	return extension;
}

static void
nemo_mime_application_chooser_apply_labels (NemoMimeApplicationChooser *chooser)
{
	gchar *label, *extension = NULL, *description = NULL;

	if (chooser->details->files != NULL) {
		/* here we assume all files are of the same content type */
		if (g_content_type_is_unknown (chooser->details->content_type)) {
			extension = get_extension_from_file (NEMO_FILE (chooser->details->files->data));

			/* the %s here is a file extension */
			description = g_strdup_printf (_("%s document"), extension);
		} else {
			description = g_content_type_get_description (chooser->details->content_type);
		}

		label = g_strdup_printf (_("Open all files of type \"%s\" with"),
					 description);
	} else {
		GFile *file;
		gchar *basename, *emname;

		file = g_file_new_for_uri (chooser->details->uri);
		basename = g_file_get_basename (file);

		if (g_content_type_is_unknown (chooser->details->content_type)) {
			extension = get_extension (basename);

			/* the %s here is a file extension */
			description = g_strdup_printf (_("%s document"), extension);
		} else {
			description = g_content_type_get_description (chooser->details->content_type);
		}

		/* first %s is filename, second %s is mime-type description */
		emname = g_strdup_printf ("<i>%s</i>", basename);
		label = g_strdup_printf (_("Select an application in the list to open %s and other files of type \"%s\""),
					 emname, description);

		g_free (emname);
		g_free (basename);
		g_object_unref (file);
	}

	gtk_label_set_markup (GTK_LABEL (chooser->details->label), label);

	g_free (label);
	g_free (extension);
	g_free (description);
}

static gboolean
exec_filter_func (const GtkFileFilterInfo *info, gpointer data)
{
    if (info->contains & GTK_FILE_FILTER_FILENAME) {
        if (g_file_test (info->filename, G_FILE_TEST_IS_EXECUTABLE))
            return TRUE;
    }

    return FALSE;
}

static void
nemo_mime_application_chooser_build_ui (NemoMimeApplicationChooser *chooser)
{
	GtkWidget *box, *button, *w;
	GAppInfo *info;

	gtk_container_set_border_width (GTK_CONTAINER (chooser), 8);
	gtk_box_set_spacing (GTK_BOX (chooser), 0);
	gtk_box_set_homogeneous (GTK_BOX (chooser), FALSE);

	chooser->details->label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (chooser->details->label), 0.0, 0.5);
	gtk_label_set_line_wrap (GTK_LABEL (chooser->details->label), TRUE);
	gtk_label_set_line_wrap_mode (GTK_LABEL (chooser->details->label),
				      PANGO_WRAP_WORD_CHAR);
	gtk_box_pack_start (GTK_BOX (chooser), chooser->details->label,
			    FALSE, FALSE, 0);

	gtk_widget_show (chooser->details->label);

	chooser->details->open_with_widget = gtk_app_chooser_widget_new (chooser->details->content_type);
	gtk_app_chooser_widget_set_show_default (GTK_APP_CHOOSER_WIDGET (chooser->details->open_with_widget),
						 TRUE);
	gtk_app_chooser_widget_set_show_fallback (GTK_APP_CHOOSER_WIDGET (chooser->details->open_with_widget),
						  TRUE);
	gtk_box_pack_start (GTK_BOX (chooser), chooser->details->open_with_widget,
			    TRUE, TRUE, 6);
	gtk_widget_show (chooser->details->open_with_widget);

	g_signal_connect (chooser->details->open_with_widget, "application-selected",
			  G_CALLBACK (application_selected_cb),
			  chooser);
	g_signal_connect (chooser->details->open_with_widget, "populate-popup",
			  G_CALLBACK (populate_popup_cb),
			  chooser);
    g_signal_connect (chooser->details->open_with_widget, "application-activated",
              G_CALLBACK (application_activated_cb),
              chooser);

    gtk_app_chooser_widget_set_show_other (GTK_APP_CHOOSER_WIDGET (chooser->details->open_with_widget),
                          TRUE);
    gtk_app_chooser_widget_set_show_recommended (GTK_APP_CHOOSER_WIDGET (chooser->details->open_with_widget),
                          TRUE);

    GtkWidget *custom_label = gtk_label_new (_("You can also type or select a custom executable file to use to open this file type.  "
                                               "You can use this command just once, or set it as default for all files of this type."));
    gtk_misc_set_alignment (GTK_MISC (custom_label), 0.0, 0.5);
    gtk_label_set_line_wrap (GTK_LABEL (custom_label), TRUE);
    gtk_label_set_line_wrap_mode (GTK_LABEL (custom_label),
                                  PANGO_WRAP_WORD_CHAR);
    gtk_box_pack_start (GTK_BOX (chooser), custom_label, FALSE, FALSE, 0);
    gtk_widget_show (GTK_WIDGET (custom_label));

    GtkWidget *custom_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (chooser), custom_box, TRUE, TRUE, 0);

    GtkWidget *entry = gtk_entry_new ();
    gtk_box_pack_start (GTK_BOX (custom_box), entry, TRUE, TRUE, 0);
    gtk_entry_set_placeholder_text (GTK_ENTRY (entry), _("Enter a custom command..."));
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

    g_signal_connect (entry, "changed",
                      G_CALLBACK (custom_entry_changed_cb),
                      chooser);

    chooser->details->custom_entry = entry;

    button = gtk_file_chooser_button_new (_("Custom application"), GTK_FILE_CHOOSER_ACTION_OPEN);

    /* FIXME: I shouldn't do this, but this is the simplest way for 2.6 at this point to remove the
       useless (and misleading) label and image in the file picker button - when a program is selected,
       it dumps the path into the custom entry box, we don't need to show anything in the button.

       These names are valid in GTK 3.10, .12, .14, .16 and master.

       This should be replaced by a simple icon-only button that creates a GtkFileChooserDialog when
       clicked.  A lot more changes involved there, postpone for 2.8
     */

    w = GTK_WIDGET (gtk_widget_get_template_child (button, GTK_TYPE_FILE_CHOOSER_BUTTON, "label"));
    gtk_widget_hide (w);
    w = GTK_WIDGET (gtk_widget_get_template_child (button, GTK_TYPE_FILE_CHOOSER_BUTTON, "image"));
    gtk_widget_hide (w);

    g_signal_connect (button, "file-set",
                      G_CALLBACK (custom_app_set_cb),
                      chooser);
    gtk_widget_show (button);
    gtk_box_pack_start (GTK_BOX (custom_box), button, FALSE, FALSE, 6);

    chooser->details->file_button = button;

    gtk_widget_show_all (custom_box);

    chooser->details->custom_picker = button;

	box = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_box_set_spacing (GTK_BOX (box), 6);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (box), GTK_BUTTONBOX_CENTER);
	gtk_box_pack_start (GTK_BOX (chooser), box, FALSE, FALSE, 6);
	gtk_widget_show (box);

    GtkFileFilter *filter = gtk_file_filter_new ();
    gtk_file_filter_add_custom (filter,
                                GTK_FILE_FILTER_FILENAME,
                                (GtkFileFilterFunc) exec_filter_func,
                                NULL,
                                NULL);
    gtk_file_filter_set_name (filter, _("Executables"));
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (button), filter);

 	button = gtk_button_new_with_label (_("Add to list"));
	g_signal_connect (button, "clicked",
			  G_CALLBACK (add_clicked_cb),
			  chooser);
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
	chooser->details->add_button = button;

	button = gtk_button_new_with_label (_("Set as default"));
	g_signal_connect (button, "clicked",
			  G_CALLBACK (set_as_default_clicked_cb),
			  chooser);
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

	chooser->details->set_as_default_button = button;

    button = gtk_button_new_with_label (_("Reset to system defaults"));
    g_signal_connect (button, "clicked",
              G_CALLBACK (reset_clicked_cb),
              chooser);
    gtk_widget_show (button);
    gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

	/* initialize sensitivity */
	info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (chooser->details->open_with_widget));
	if (info != NULL) {
		application_selected_cb (GTK_APP_CHOOSER_WIDGET (chooser->details->open_with_widget),
					 info, chooser);
		g_object_unref (info);
	}

    gtk_widget_grab_focus (chooser->details->custom_entry);
}

static void
nemo_mime_application_chooser_init (NemoMimeApplicationChooser *chooser)
{
	chooser->details = G_TYPE_INSTANCE_GET_PRIVATE (chooser, NEMO_TYPE_MIME_APPLICATION_CHOOSER,
							NemoMimeApplicationChooserDetails);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (chooser),
					GTK_ORIENTATION_VERTICAL);

    chooser->details->custom_info = NULL;
}

static void
nemo_mime_application_chooser_constructed (GObject *object)
{
	NemoMimeApplicationChooser *chooser = NEMO_MIME_APPLICATION_CHOOSER (object);

	if (G_OBJECT_CLASS (nemo_mime_application_chooser_parent_class)->constructed != NULL)
		G_OBJECT_CLASS (nemo_mime_application_chooser_parent_class)->constructed (object);

	nemo_mime_application_chooser_build_ui (chooser);
	nemo_mime_application_chooser_apply_labels (chooser);
}

static void
nemo_mime_application_chooser_finalize (GObject *object)
{
	NemoMimeApplicationChooser *chooser;

	chooser = NEMO_MIME_APPLICATION_CHOOSER (object);

	g_free (chooser->details->uri);
	g_free (chooser->details->content_type);
    if (chooser->details->custom_info != NULL)
        g_object_unref (chooser->details->custom_info);

	G_OBJECT_CLASS (nemo_mime_application_chooser_parent_class)->finalize (object);
}

static void
nemo_mime_application_chooser_get_property (GObject *object,
						guint property_id,
						GValue *value,
						GParamSpec *pspec)
{
	NemoMimeApplicationChooser *chooser = NEMO_MIME_APPLICATION_CHOOSER (object);

	switch (property_id) {
	case PROP_CONTENT_TYPE:
		g_value_set_string (value, chooser->details->content_type);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nemo_mime_application_chooser_set_property (GObject *object,
						guint property_id,
						const GValue *value,
						GParamSpec *pspec)
{
	NemoMimeApplicationChooser *chooser = NEMO_MIME_APPLICATION_CHOOSER (object);

	switch (property_id) {
	case PROP_CONTENT_TYPE:
		chooser->details->content_type = g_value_dup_string (value);
		break;
	case PROP_FILES:
		chooser->details->files = g_value_get_pointer (value);
		break;
	case PROP_URI:
		chooser->details->uri = g_value_dup_string (value);
		break;
    case PROP_DIALOG_OK:
        chooser->details->dialog_ok = g_value_get_object (value);
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nemo_mime_application_chooser_class_init (NemoMimeApplicationChooserClass *class)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->set_property = nemo_mime_application_chooser_set_property;
	gobject_class->get_property = nemo_mime_application_chooser_get_property;
	gobject_class->finalize = nemo_mime_application_chooser_finalize;
	gobject_class->constructed = nemo_mime_application_chooser_constructed;

	properties[PROP_CONTENT_TYPE] = g_param_spec_string ("content-type",
							     "Content type",
							     "Content type for this widget",
							     NULL,
							     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
							     G_PARAM_STATIC_STRINGS);
	properties[PROP_URI] = g_param_spec_string ("uri",
						    "URI",
						    "URI for this widget",
						    NULL,
						    G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
						    G_PARAM_STATIC_STRINGS);
	properties[PROP_FILES] = g_param_spec_pointer ("files",
						       "Files",
						       "Files for this widget",
						       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
						       G_PARAM_STATIC_STRINGS);

    properties[PROP_DIALOG_OK] = g_param_spec_object ("ok-button",
                               "Dialog OK button",
                               "Dialog OK button",
                               GTK_TYPE_WIDGET,
                               G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                               G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);

	g_type_class_add_private (class, sizeof (NemoMimeApplicationChooserDetails));
}

GtkWidget *
nemo_mime_application_chooser_new (const char *uri,
                                        GList *files,
                                   const char *mime_type,
                                    GtkWidget *ok_button)
{
	GtkWidget *chooser;

	chooser = g_object_new (NEMO_TYPE_MIME_APPLICATION_CHOOSER,
				"uri", uri,
				"files", files,
				"content-type", mime_type,
                "ok-button", ok_button,
				NULL);

	return chooser;
}

GAppInfo *
nemo_mime_application_chooser_get_info (NemoMimeApplicationChooser *chooser)
{
    if (chooser->details->custom_info == NULL)
        return gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (chooser->details->open_with_widget));
    else
        return chooser->details->custom_info;
}

const gchar *
nemo_mime_application_chooser_get_uri (NemoMimeApplicationChooser *chooser)
{
    return chooser->details->uri;
}

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-file-conflict-dialog: dialog that handles file conflicts
   during transfer operations.

   Copyright (C) 2008-2010 Cosimo Cecchi

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
   
   Authors: Cosimo Cecchi <cosimoc@gnome.org>
*/

#include <config.h>
#include "nemo-file-conflict-dialog.h"

#include <string.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <pango/pango.h>
#include <eel/eel-vfs-extensions.h>

#include "nemo-file.h"
#include "nemo-icon-info.h"

struct _NemoFileConflictDialogDetails
{
	/* conflicting objects */
	NemoFile *source;
	NemoFile *destination;
	NemoFile *dest_dir;

	gchar *conflict_name;
	NemoFileListHandle *handle;
	gulong src_handler_id;
	gulong dest_handler_id;

	/* UI objects */
	GtkWidget *titles_vbox;
	GtkWidget *first_hbox;
	GtkWidget *second_hbox;
	GtkWidget *expander;
	GtkWidget *entry;
	GtkWidget *checkbox;
	GtkWidget *rename_button;
	GtkWidget *replace_button;
	GtkWidget *dest_image;
	GtkWidget *src_image;
};

G_DEFINE_TYPE (NemoFileConflictDialog,
	       nemo_file_conflict_dialog,
	       GTK_TYPE_DIALOG);

#define NEMO_FILE_CONFLICT_DIALOG_GET_PRIVATE(object)		\
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), NEMO_TYPE_FILE_CONFLICT_DIALOG, \
				      NemoFileConflictDialogDetails))

static void
file_icons_changed (NemoFile *file,
		    NemoFileConflictDialog *fcd)
{
	GdkPixbuf *pixbuf;

	pixbuf = nemo_file_get_icon_pixbuf (fcd->details->destination,
						NEMO_ICON_SIZE_LARGE,
						TRUE,
						NEMO_FILE_ICON_FLAGS_USE_THUMBNAILS);

	gtk_image_set_from_pixbuf (GTK_IMAGE (fcd->details->dest_image), pixbuf);
	g_object_unref (pixbuf);

	pixbuf = nemo_file_get_icon_pixbuf (fcd->details->source,
						NEMO_ICON_SIZE_LARGE,
						TRUE,
						NEMO_FILE_ICON_FLAGS_USE_THUMBNAILS);

	gtk_image_set_from_pixbuf (GTK_IMAGE (fcd->details->src_image), pixbuf);
	g_object_unref (pixbuf);
}

static void
file_list_ready_cb (GList *files,
		    gpointer user_data)
{
	NemoFileConflictDialog *fcd = user_data;
	NemoFile *src, *dest, *dest_dir;
	time_t src_mtime, dest_mtime;
	gboolean source_is_dir,	dest_is_dir, should_show_type;
	NemoFileConflictDialogDetails *details;
	char *primary_text, *message, *secondary_text;
	const gchar *message_extra;
	char *dest_name, *dest_dir_name, *edit_name;
	char *label_text;
	char *size, *date, *type = NULL;
	GdkPixbuf *pixbuf;
	GtkWidget *label;
	GString *str;
	PangoAttrList *attr_list;

	details = fcd->details;

	details->handle = NULL;

	dest_dir = g_list_nth_data (files, 0);
	dest = g_list_nth_data (files, 1);
	src = g_list_nth_data (files, 2);

	src_mtime = nemo_file_get_mtime (src);
	dest_mtime = nemo_file_get_mtime (dest);

	dest_name = nemo_file_get_display_name (dest);
	dest_dir_name = nemo_file_get_display_name (dest_dir);

	source_is_dir = nemo_file_is_directory (src);
	dest_is_dir = nemo_file_is_directory (dest);

	type = nemo_file_get_mime_type (dest);
	should_show_type = !nemo_file_is_mime_type (src, type);

	g_free (type);
	type = NULL;

	/* Set up the right labels */
	if (dest_is_dir) {
		if (source_is_dir) {
			primary_text = g_strdup_printf
				(_("Merge folder \"%s\"?"),
				 dest_name);

			message_extra = 
				_("Merging will ask for confirmation before replacing any files in "
				  "the folder that conflict with the files being copied.");

			if (src_mtime > dest_mtime) {
				message = g_strdup_printf (
					_("An older folder with the same name already exists in \"%s\"."),
					dest_dir_name);
			} else if (src_mtime < dest_mtime) {
				message = g_strdup_printf (
					_("A newer folder with the same name already exists in \"%s\"."),
					dest_dir_name);
			} else {
				message = g_strdup_printf (
					_("Another folder with the same name already exists in \"%s\"."),
					dest_dir_name);
			}
		} else {
			message_extra =
				_("Replacing it will remove all files in the folder.");
			primary_text = g_strdup_printf
				(_("Replace folder \"%s\"?"), dest_name);
			message = g_strdup_printf
				(_("A folder with the same name already exists in \"%s\"."),
				 dest_dir_name);
		}
	} else {
		primary_text = g_strdup_printf
			(_("Replace file \"%s\"?"), dest_name);

		message_extra = _("Replacing it will overwrite its content.");

		if (src_mtime > dest_mtime) {
			message = g_strdup_printf (
				_("An older file with the same name already exists in \"%s\"."),
				dest_dir_name);
		} else if (src_mtime < dest_mtime) {
			message = g_strdup_printf (
				_("A newer file with the same name already exists in \"%s\"."),
				dest_dir_name);
		} else {
			message = g_strdup_printf (
				_("Another file with the same name already exists in \"%s\"."),
				dest_dir_name);
		}
	}

	secondary_text = g_strdup_printf ("%s\n%s", message, message_extra);
	g_free (message);

	label = gtk_label_new (primary_text);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD_CHAR);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (details->titles_vbox),
			    label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	attr_list = pango_attr_list_new ();
	pango_attr_list_insert (attr_list, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
	pango_attr_list_insert (attr_list, pango_attr_scale_new (PANGO_SCALE_LARGE));
	g_object_set (label,
		      "attributes", attr_list,
		      NULL);

	pango_attr_list_unref (attr_list);

	label = gtk_label_new (secondary_text);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (details->titles_vbox),
			    label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	g_free (primary_text);
	g_free (secondary_text);

	/* Set up file icons */
	pixbuf = nemo_file_get_icon_pixbuf (dest,
						NEMO_ICON_SIZE_LARGE,
						TRUE,
						NEMO_FILE_ICON_FLAGS_USE_THUMBNAILS);
	details->dest_image = gtk_image_new_from_pixbuf (pixbuf);
	gtk_box_pack_start (GTK_BOX (details->first_hbox),
			    details->dest_image, FALSE, FALSE, 0);
	gtk_widget_show (details->dest_image);
	g_object_unref (pixbuf);

	pixbuf = nemo_file_get_icon_pixbuf (src,
						NEMO_ICON_SIZE_LARGE,
						TRUE,
						NEMO_FILE_ICON_FLAGS_USE_THUMBNAILS);
	details->src_image = gtk_image_new_from_pixbuf (pixbuf);
	gtk_box_pack_start (GTK_BOX (details->second_hbox),
			    details->src_image, FALSE, FALSE, 0);
	gtk_widget_show (details->src_image);
	g_object_unref (pixbuf);

	/* Set up labels */
	label = gtk_label_new (NULL);
	date = nemo_file_get_string_attribute (dest,
						   "date_modified");
	size = nemo_file_get_string_attribute (dest, "size");

	if (should_show_type) {
		type = nemo_file_get_string_attribute (dest, "type");
	}

	str = g_string_new (NULL);
	g_string_append_printf (str, "<b>%s</b>\n", _("Original file"));
	g_string_append_printf (str, "<i>%s</i> %s\n", _("Size:"), size);

	if (should_show_type) {
		g_string_append_printf (str, "<i>%s</i> %s\n", _("Type:"), type);
	}

	g_string_append_printf (str, "<i>%s</i> %s", _("Last modified:"), date);

	label_text = str->str;
	gtk_label_set_markup (GTK_LABEL (label),
			      label_text);
	gtk_box_pack_start (GTK_BOX (details->first_hbox),
			    label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	g_free (size);
	g_free (type);
	g_free (date);
	g_string_erase (str, 0, -1);

	/* Second label */
	label = gtk_label_new (NULL);
	date = nemo_file_get_string_attribute (src,
						   "date_modified");
	size = nemo_file_get_string_attribute (src, "size");

	if (should_show_type) {
		type = nemo_file_get_string_attribute (src, "type");
	}

	g_string_append_printf (str, "<b>%s</b>\n", _("Replace with"));
	g_string_append_printf (str, "<i>%s</i> %s\n", _("Size:"), size);

	if (should_show_type) {
		g_string_append_printf (str, "<i>%s</i> %s\n", _("Type:"), type);
	}

	g_string_append_printf (str, "<i>%s</i> %s", _("Last modified:"), date);
	label_text = g_string_free (str, FALSE);

	gtk_label_set_markup (GTK_LABEL (label),
			      label_text);
	gtk_box_pack_start (GTK_BOX (details->second_hbox),
			    label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	g_free (size);
	g_free (date);
	g_free (type);
	g_free (label_text);

	/* Populate the entry */
	edit_name = nemo_file_get_edit_name (dest);
	details->conflict_name = edit_name;

	gtk_entry_set_text (GTK_ENTRY (details->entry), edit_name);

	if (source_is_dir && dest_is_dir) {
		gtk_button_set_label (GTK_BUTTON (details->replace_button),
				      _("Merge"));
	}

	nemo_file_monitor_add (src, fcd, NEMO_FILE_ATTRIBUTES_FOR_ICON);
	nemo_file_monitor_add (dest, fcd, NEMO_FILE_ATTRIBUTES_FOR_ICON);

	details->src_handler_id = g_signal_connect (src, "changed",
			  G_CALLBACK (file_icons_changed), fcd);
	details->dest_handler_id = g_signal_connect (dest, "changed",
			  G_CALLBACK (file_icons_changed), fcd);
}

static void
build_dialog_appearance (NemoFileConflictDialog *fcd)
{
	GList *files = NULL;
	NemoFileConflictDialogDetails *details = fcd->details;

	files = g_list_prepend (files, details->source);
	files = g_list_prepend (files, details->destination);
	files = g_list_prepend (files, details->dest_dir);

	nemo_file_list_call_when_ready (files,
					    NEMO_FILE_ATTRIBUTES_FOR_ICON,
					    &details->handle, file_list_ready_cb, fcd);
	g_list_free (files);
}

static void
set_source_and_destination (GtkWidget *w,
			    GFile *source,
			    GFile *destination,
			    GFile *dest_dir)
{
	NemoFileConflictDialog *dialog;
	NemoFileConflictDialogDetails *details;

	dialog = NEMO_FILE_CONFLICT_DIALOG (w);
	details = dialog->details;

	details->source = nemo_file_get (source);
	details->destination = nemo_file_get (destination);
	details->dest_dir = nemo_file_get (dest_dir);

	build_dialog_appearance (dialog);
}

static void
entry_text_changed_cb (GtkEditable *entry,
		       NemoFileConflictDialog *dialog)
{
	NemoFileConflictDialogDetails *details;

	details = dialog->details;

	/* The rename button is visible only if there's text
	 * in the entry.
	 */
	if  (g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (entry)), "") != 0 &&
	     g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (entry)), details->conflict_name) != 0) {
		gtk_widget_hide (details->replace_button);
		gtk_widget_show (details->rename_button);

		gtk_widget_set_sensitive (details->checkbox, FALSE);

		gtk_dialog_set_default_response (GTK_DIALOG (dialog),
						 CONFLICT_RESPONSE_RENAME);
	} else {
		gtk_widget_hide (details->rename_button);
		gtk_widget_show (details->replace_button);

		gtk_widget_set_sensitive (details->checkbox, TRUE);

		gtk_dialog_set_default_response (GTK_DIALOG (dialog),
						 CONFLICT_RESPONSE_REPLACE);
	}
}

static void
expander_activated_cb (GtkExpander *w,
		       NemoFileConflictDialog *dialog)
{
	NemoFileConflictDialogDetails *details;
	int start_pos, end_pos;

	details = dialog->details;

	if (!gtk_expander_get_expanded (w)) {
		if (g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (details->entry)),
			       details->conflict_name) == 0) {
			gtk_widget_grab_focus (details->entry);

			eel_filename_get_rename_region (details->conflict_name,
							&start_pos, &end_pos);
			gtk_editable_select_region (GTK_EDITABLE (details->entry),
						    start_pos, end_pos);
		}
	}
}

static void
checkbox_toggled_cb (GtkToggleButton *t,
		     NemoFileConflictDialog *dialog)
{
	NemoFileConflictDialogDetails *details;

	details = dialog->details;

	gtk_widget_set_sensitive (details->expander,
				  !gtk_toggle_button_get_active (t));
	gtk_widget_set_sensitive (details->rename_button,
				  !gtk_toggle_button_get_active (t));

	if  (!gtk_toggle_button_get_active (t) &&
	     g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (details->entry)),
			"") != 0 &&
	     g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (details->entry)),
			details->conflict_name) != 0) {
		gtk_widget_hide (details->replace_button);
		gtk_widget_show (details->rename_button);
	} else {
		gtk_widget_hide (details->rename_button);
		gtk_widget_show (details->replace_button);
	}
}

static void
reset_button_clicked_cb (GtkButton *w,
			 NemoFileConflictDialog *dialog)
{
	NemoFileConflictDialogDetails *details;
	int start_pos, end_pos;

	details = dialog->details;

	gtk_entry_set_text (GTK_ENTRY (details->entry),
			    details->conflict_name);
	gtk_widget_grab_focus (details->entry);
	eel_filename_get_rename_region (details->conflict_name,
					&start_pos, &end_pos);
	gtk_editable_select_region (GTK_EDITABLE (details->entry),
				    start_pos, end_pos);

}

static void
nemo_file_conflict_dialog_init (NemoFileConflictDialog *fcd)
{
	GtkWidget *hbox, *vbox, *vbox2, *alignment;
	GtkWidget *widget, *dialog_area;
	NemoFileConflictDialogDetails *details;
	GtkDialog *dialog;

	details = fcd->details = NEMO_FILE_CONFLICT_DIALOG_GET_PRIVATE (fcd);
	dialog = GTK_DIALOG (fcd);

	/* Setup the main hbox */
	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	dialog_area = gtk_dialog_get_content_area (dialog);
	gtk_box_pack_start (GTK_BOX (dialog_area), hbox, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);

	/* Setup the dialog image */
	widget = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING,
					   GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.5, 0.0);

	/* Setup the vbox containing the dialog body */
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	/* Setup the vbox for the dialog labels */
	widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	details->titles_vbox = widget;

	/* Setup the hboxes to pack file infos into */
	alignment = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
	g_object_set (alignment, "left-padding", 12, NULL);
	vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_add (GTK_CONTAINER (alignment), vbox2);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
	details->first_hbox = hbox;

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
	details->second_hbox = hbox;

	/* Setup the expander for the rename action */
	details->expander = gtk_expander_new_with_mnemonic (_("_Select a new name for the destination"));
	gtk_box_pack_start (GTK_BOX (vbox2), details->expander, FALSE, FALSE, 0);
	g_signal_connect (details->expander, "activate",
			  G_CALLBACK (expander_activated_cb), dialog);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_add (GTK_CONTAINER (details->expander), hbox);

	widget = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 6);
	details->entry = widget;
	g_signal_connect (widget, "changed",
			  G_CALLBACK (entry_text_changed_cb), dialog);

	widget = gtk_button_new_with_label (_("Reset"));
	gtk_button_set_image (GTK_BUTTON (widget),
			      gtk_image_new_from_stock (GTK_STOCK_UNDO,
							GTK_ICON_SIZE_MENU));
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 6);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (reset_button_clicked_cb), dialog);

	gtk_widget_show_all (alignment);


	/* Setup the checkbox to apply the action to all files */
	widget = gtk_check_button_new_with_mnemonic (_("Apply this action to all files"));
	gtk_box_pack_start (GTK_BOX (vbox),
			    widget, FALSE, FALSE, 0);
	details->checkbox = widget;
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (checkbox_toggled_cb), dialog);

	/* Add buttons */
	gtk_dialog_add_buttons (dialog,
				GTK_STOCK_CANCEL,
				GTK_RESPONSE_CANCEL,
				_("_Skip"),
				CONFLICT_RESPONSE_SKIP,
				NULL);
	details->rename_button =
		gtk_dialog_add_button (dialog,
				       _("Re_name"),
				       CONFLICT_RESPONSE_RENAME);
	gtk_widget_hide (details->rename_button);

	details->replace_button =
		gtk_dialog_add_button (dialog,
				       _("Replace"),
				       CONFLICT_RESPONSE_REPLACE);
	gtk_widget_grab_focus (details->replace_button);

	/* Setup HIG properties */
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (dialog)), 14);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

	gtk_widget_show_all (dialog_area);
}

static void
do_finalize (GObject *self)
{
	NemoFileConflictDialogDetails *details =
		NEMO_FILE_CONFLICT_DIALOG (self)->details;

	g_free (details->conflict_name);

	if (details->handle != NULL) {
		nemo_file_list_cancel_call_when_ready (details->handle);
	}

	if (details->src_handler_id) {
		g_signal_handler_disconnect (details->source, details->src_handler_id);
		nemo_file_monitor_remove (details->source, self);
	}

	if (details->dest_handler_id) {
		g_signal_handler_disconnect (details->destination, details->dest_handler_id);
		nemo_file_monitor_remove (details->destination, self);
	}

	nemo_file_unref (details->source);
	nemo_file_unref (details->destination);
	nemo_file_unref (details->dest_dir);

	G_OBJECT_CLASS (nemo_file_conflict_dialog_parent_class)->finalize (self);
}

static void
nemo_file_conflict_dialog_class_init (NemoFileConflictDialogClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = do_finalize;

	g_type_class_add_private (klass, sizeof (NemoFileConflictDialogDetails));
}

char *
nemo_file_conflict_dialog_get_new_name (NemoFileConflictDialog *dialog)
{
	return g_strdup (gtk_entry_get_text
			 (GTK_ENTRY (dialog->details->entry)));
}

gboolean
nemo_file_conflict_dialog_get_apply_to_all (NemoFileConflictDialog *dialog)
{
	return gtk_toggle_button_get_active 
		(GTK_TOGGLE_BUTTON (dialog->details->checkbox));
}

GtkWidget *
nemo_file_conflict_dialog_new (GtkWindow *parent,
				   GFile *source,
				   GFile *destination,
				   GFile *dest_dir)
{
	GtkWidget *dialog;
	
	dialog = GTK_WIDGET (g_object_new (NEMO_TYPE_FILE_CONFLICT_DIALOG,
					   "title", _("File conflict"),
					   NULL));
	set_source_and_destination (dialog,
				    source,
				    destination,
				    dest_dir);
	gtk_window_set_transient_for (GTK_WINDOW (dialog),
				      parent);
	return dialog;
}

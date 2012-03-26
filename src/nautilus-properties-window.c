/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-properties-window.c - window that lets user modify file properties

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Darin Adler <darin@bentspoon.com>
*/

#include <config.h>

#include "nautilus-properties-window.h"

#include "nautilus-desktop-item-properties.h"
#include "nautilus-error-reporting.h"
#include "nautilus-mime-actions.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <string.h>
#include <sys/stat.h>
#include <cairo.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-desktop-thumbnail.h>

#include <eel/eel-accessibility.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>

#include <libnautilus-extension/nautilus-property-page-provider.h>
#include <libnautilus-private/nautilus-entry.h>
#include <libnautilus-private/nautilus-file-attributes.h>
#include <libnautilus-private/nautilus-file-operations.h>
#include <libnautilus-private/nautilus-desktop-icon-file.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-link.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-mime-application-chooser.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-undo-signal-handlers.h>
#include <libnautilus-private/nautilus-undo.h>

#if HAVE_SYS_VFS_H
#include <sys/vfs.h>
#elif HAVE_SYS_MOUNT_H
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/mount.h>
#endif

#define USED_FILL_R  0.988235294
#define USED_FILL_G  0.91372549
#define USED_FILL_B  0.309803922

#define FREE_FILL_R  0.447058824
#define FREE_FILL_G  0.623529412
#define FREE_FILL_B  0.811764706


#define PREVIEW_IMAGE_WIDTH 96

#define ROW_PAD 6

static GHashTable *windows;
static GHashTable *pending_lists;

struct NautilusPropertiesWindowDetails {	
	GList *original_files;
	GList *target_files;
	
	GtkNotebook *notebook;
	
	GtkGrid *basic_grid;

	GtkWidget *icon_button;
	GtkWidget *icon_image;
	GtkWidget *icon_chooser;

	GtkLabel *name_label;
	GtkWidget *name_field;
	unsigned int name_row;
	char *pending_name;

	GtkLabel *directory_contents_title_field;
	GtkLabel *directory_contents_value_field;
	guint update_directory_contents_timeout_id;
	guint update_files_timeout_id;

	NautilusFile *group_change_file;
	char         *group_change_group;
	unsigned int  group_change_timeout;
	NautilusFile *owner_change_file;
	char         *owner_change_owner;
	unsigned int  owner_change_timeout;

	GList *permission_buttons;
	GList *permission_combos;
	GHashTable *initial_permissions;
	gboolean has_recursive_apply;

	GList *value_fields;

	GList *mime_list;

	gboolean deep_count_finished;

	guint total_count;
	goffset total_size;

	guint long_operation_underway;

 	GList *changed_files;
 	
 	guint64 volume_capacity;
 	guint64 volume_free;
	
	GdkRGBA used_color;
	GdkRGBA free_color;
	GdkRGBA used_stroke_color;
	GdkRGBA free_stroke_color;
};

typedef enum {
	PERMISSIONS_CHECKBOXES_READ,
	PERMISSIONS_CHECKBOXES_WRITE,
	PERMISSIONS_CHECKBOXES_EXECUTE
} CheckboxType;

enum {
	TITLE_COLUMN,
	VALUE_COLUMN,
	COLUMN_COUNT
};

typedef struct {
	GList *original_files;
	GList *target_files;
	GtkWidget *parent_widget;
	char *startup_id;
	char *pending_key;
	GHashTable *pending_files;
} StartupData;

/* drag and drop definitions */

enum {
	TARGET_URI_LIST,
	TARGET_GNOME_URI_LIST,
};

static const GtkTargetEntry target_table[] = {
	{ "text/uri-list",  0, TARGET_URI_LIST },
	{ "x-special/gnome-icon-list",  0, TARGET_GNOME_URI_LIST },
};

#define DIRECTORY_CONTENTS_UPDATE_INTERVAL	200 /* milliseconds */
#define FILES_UPDATE_INTERVAL			200 /* milliseconds */

/*
 * A timeout before changes through the user/group combo box will be applied.
 * When quickly changing owner/groups (i.e. by keyboard or scroll wheel),
 * this ensures that the GUI doesn't end up unresponsive.
 *
 * Both combos react on changes by scheduling a new change and unscheduling
 * or cancelling old pending changes.
 */
#define CHOWN_CHGRP_TIMEOUT			300 /* milliseconds */

static void directory_contents_value_field_update (NautilusPropertiesWindow *window);
static void file_changed_callback                 (NautilusFile       *file,
						   gpointer            user_data);
static void permission_button_update              (NautilusPropertiesWindow *window,
						   GtkToggleButton    *button);
static void permission_combo_update               (NautilusPropertiesWindow *window,
						   GtkComboBox        *combo);
static void value_field_update                    (NautilusPropertiesWindow *window,
						   GtkLabel           *field);
static void properties_window_update              (NautilusPropertiesWindow *window,
						   GList              *files);
static void is_directory_ready_callback           (NautilusFile       *file,
						   gpointer            data);
static void cancel_group_change_callback          (NautilusPropertiesWindow *window);
static void cancel_owner_change_callback          (NautilusPropertiesWindow *window);
static void parent_widget_destroyed_callback      (GtkWidget          *widget,
						   gpointer            callback_data);
static void select_image_button_callback          (GtkWidget          *widget,
						   NautilusPropertiesWindow *properties_window);
static void set_icon                              (const char         *icon_path,
						   NautilusPropertiesWindow *properties_window);
static void remove_pending                        (StartupData        *data,
						   gboolean            cancel_call_when_ready,
						   gboolean            cancel_timed_wait,
						   gboolean            cancel_destroy_handler);
static void append_extension_pages                (NautilusPropertiesWindow *window);

static gboolean name_field_focus_out              (NautilusEntry *name_field,
						   GdkEventFocus *event,
						   gpointer callback_data);
static void name_field_activate                   (NautilusEntry *name_field,
						   gpointer callback_data);
static GtkLabel *attach_ellipsizing_value_label   (GtkGrid *grid,
						   GtkWidget *sibling,
						   const char *initial_text);
						   
static GtkWidget* create_pie_widget 		  (NautilusPropertiesWindow *window);

G_DEFINE_TYPE (NautilusPropertiesWindow, nautilus_properties_window, GTK_TYPE_DIALOG);

static gboolean
is_multi_file_window (NautilusPropertiesWindow *window)
{
	GList *l;
	int count;
	
	count = 0;
	
	for (l = window->details->original_files; l != NULL; l = l->next) {
		if (!nautilus_file_is_gone (NAUTILUS_FILE (l->data))) {			
			count++;
			if (count > 1) {
				return TRUE;
			}	
		}
	}

	return FALSE;
}

static int
get_not_gone_original_file_count (NautilusPropertiesWindow *window)
{
	GList *l;
	int count;

	count = 0;

	for (l = window->details->original_files; l != NULL; l = l->next) {
		if (!nautilus_file_is_gone (NAUTILUS_FILE (l->data))) {
			count++;
		}
	}

	return count;
}

static NautilusFile *
get_original_file (NautilusPropertiesWindow *window) 
{
	g_return_val_if_fail (!is_multi_file_window (window), NULL);

	if (window->details->original_files == NULL) {
		return NULL;
	}

	return NAUTILUS_FILE (window->details->original_files->data);
}

static NautilusFile *
get_target_file_for_original_file (NautilusFile *file)
{
	NautilusFile *target_file;
	GFile *location;
	char *uri_to_display;
	NautilusDesktopLink *link;

	target_file = NULL;
	if (NAUTILUS_IS_DESKTOP_ICON_FILE (file)) {
		link = nautilus_desktop_icon_file_get_link (NAUTILUS_DESKTOP_ICON_FILE (file));

		if (link != NULL) {
			/* map to linked URI for these types of links */
			location = nautilus_desktop_link_get_activation_location (link);
			if (location) {
				target_file = nautilus_file_get (location);
				g_object_unref (location);
			}
			
			g_object_unref (link);
		}
        } else {
		uri_to_display = nautilus_file_get_activation_uri (file);
		if (uri_to_display != NULL) {
			target_file = nautilus_file_get_by_uri (uri_to_display);
			g_free (uri_to_display);
		}
	}
	
	if (target_file != NULL) {
		return target_file;
	}

	/* Ref passed-in file here since we've decided to use it. */
	nautilus_file_ref (file);
	return file;
}

static NautilusFile *
get_target_file (NautilusPropertiesWindow *window)
{
	return NAUTILUS_FILE (window->details->target_files->data);
}

static void
add_prompt (GtkWidget *vbox, const char *prompt_text, gboolean pack_at_start)
{
	GtkWidget *prompt;

	prompt = gtk_label_new (prompt_text);
   	gtk_label_set_justify (GTK_LABEL (prompt), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (prompt), TRUE);
	gtk_widget_show (prompt);
	if (pack_at_start) {
		gtk_box_pack_start (GTK_BOX (vbox), prompt, FALSE, FALSE, 0);
	} else {
		gtk_box_pack_end (GTK_BOX (vbox), prompt, FALSE, FALSE, 0);
	}
}

static void
add_prompt_and_separator (GtkWidget *vbox, const char *prompt_text)
{
	GtkWidget *separator_line;

	add_prompt (vbox, prompt_text, FALSE);

	separator_line = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  	gtk_widget_show (separator_line);
  	gtk_box_pack_end (GTK_BOX (vbox), separator_line, TRUE, TRUE, 2*ROW_PAD);
}

static void
get_image_for_properties_window (NautilusPropertiesWindow *window,
				 char **icon_name,
				 GdkPixbuf **icon_pixbuf)
{
	NautilusIconInfo *icon, *new_icon;
	GList *l;
	
	icon = NULL;
	for (l = window->details->original_files; l != NULL; l = l->next) {
		NautilusFile *file;
		
		file = NAUTILUS_FILE (l->data);
		
		if (!icon) {
			icon = nautilus_file_get_icon (file, NAUTILUS_ICON_SIZE_STANDARD, NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS | NAUTILUS_FILE_ICON_FLAGS_IGNORE_VISITING);
		} else {
			new_icon = nautilus_file_get_icon (file, NAUTILUS_ICON_SIZE_STANDARD, NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS | NAUTILUS_FILE_ICON_FLAGS_IGNORE_VISITING);
			if (!new_icon || new_icon != icon) {
				g_object_unref (icon);
				g_object_unref (new_icon);
				icon = NULL;
				break;
			}
			g_object_unref (new_icon);
		}
	}

	if (!icon) {
		icon = nautilus_icon_info_lookup_from_name ("text-x-generic", NAUTILUS_ICON_SIZE_STANDARD);
	}

	if (icon_name != NULL) {
		*icon_name = g_strdup (nautilus_icon_info_get_used_name (icon));
	}

	if (icon_pixbuf != NULL) {
		*icon_pixbuf = nautilus_icon_info_get_pixbuf_at_size (icon, NAUTILUS_ICON_SIZE_STANDARD);
	}

	g_object_unref (icon);
}


static void
update_properties_window_icon (GtkImage *image)
{
	NautilusPropertiesWindow *window;
	GdkPixbuf *pixbuf;
	char *name;

	window = g_object_get_data (G_OBJECT (image), "properties_window");
	
	get_image_for_properties_window (window, &name, &pixbuf);

	if (name != NULL) {
		gtk_window_set_icon_name (GTK_WINDOW (window), name);
	} else {
		gtk_window_set_icon (GTK_WINDOW (window), pixbuf);
	}

	gtk_image_set_from_pixbuf (image, pixbuf);

	g_free (name);
	g_object_unref (pixbuf);
}

/* utility to test if a uri refers to a local image */
static gboolean
uri_is_local_image (const char *uri)
{
	GdkPixbuf *pixbuf;
	char *image_path;
	
	image_path = g_filename_from_uri (uri, NULL, NULL);
	if (image_path == NULL) {
		return FALSE;
	}

	pixbuf = gdk_pixbuf_new_from_file (image_path, NULL);
	g_free (image_path);
	
	if (pixbuf == NULL) {
		return FALSE;
	}
	g_object_unref (pixbuf);
	return TRUE;
}


static void
reset_icon (NautilusPropertiesWindow *properties_window)
{
	GList *l;

	for (l = properties_window->details->original_files; l != NULL; l = l->next) {
		NautilusFile *file;
		
		file = NAUTILUS_FILE (l->data);
		
		nautilus_file_set_metadata (file,
					    NAUTILUS_METADATA_KEY_ICON_SCALE,
					    NULL, NULL);
		nautilus_file_set_metadata (file,
					    NAUTILUS_METADATA_KEY_CUSTOM_ICON,
					    NULL, NULL);
	}
}


static void  
nautilus_properties_window_drag_data_received (GtkWidget *widget, GdkDragContext *context,
					       int x, int y,
					       GtkSelectionData *selection_data,
					       guint info, guint time)
{
	char **uris;
	gboolean exactly_one;
	GtkImage *image;
 	GtkWindow *window; 

	image = GTK_IMAGE (widget);
 	window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (image)));

	uris = g_strsplit (gtk_selection_data_get_data (selection_data), "\r\n", 0);
	exactly_one = uris[0] != NULL && (uris[1] == NULL || uris[1][0] == '\0');


	if (!exactly_one) {
		eel_show_error_dialog
			(_("You cannot assign more than one custom icon at a time!"),
			 _("Please drag just one image to set a custom icon."), 
			 window);
	} else {		
		if (uri_is_local_image (uris[0])) {			
			set_icon (uris[0], NAUTILUS_PROPERTIES_WINDOW (window));
		} else {
			GFile *f;

			f = g_file_new_for_uri (uris[0]);
			if (!g_file_is_native (f)) {
				eel_show_error_dialog
					(_("The file that you dropped is not local."),
					 _("You can only use local images as custom icons."), 
					 window);
				
			} else {
				eel_show_error_dialog
					(_("The file that you dropped is not an image."),
					 _("You can only use local images as custom icons."),
					 window);
			}
			g_object_unref (f);
		}		
	}
	g_strfreev (uris);
}

static GtkWidget *
create_image_widget (NautilusPropertiesWindow *window,
		     gboolean is_customizable)
{
 	GtkWidget *button;
	GtkWidget *image;
	GdkPixbuf *pixbuf;
	
	get_image_for_properties_window (window, NULL, &pixbuf);

	image = gtk_image_new ();
	gtk_widget_show (image);

	button = NULL;
	if (is_customizable) {
		button = gtk_button_new ();
		gtk_container_add (GTK_CONTAINER (button), image);

		/* prepare the image to receive dropped objects to assign custom images */
		gtk_drag_dest_set (GTK_WIDGET (image),
				   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
				   target_table, G_N_ELEMENTS (target_table),
				   GDK_ACTION_COPY | GDK_ACTION_MOVE);

		g_signal_connect (image, "drag_data_received",
				  G_CALLBACK (nautilus_properties_window_drag_data_received), NULL);
		g_signal_connect (button, "clicked",
				  G_CALLBACK (select_image_button_callback), window);
	}

	gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);

	g_object_unref (pixbuf);

	g_object_set_data (G_OBJECT (image), "properties_window", window);

	window->details->icon_image = image;
	window->details->icon_button = button;

	return button != NULL ? button : image;
}

static void
set_name_field (NautilusPropertiesWindow *window,
		const gchar *original_name,
		const gchar *name)
{
	gboolean new_widget;
	gboolean use_label;

	/* There are four cases here:
	 * 1) Changing the text of a label
	 * 2) Changing the text of an entry
	 * 3) Creating label (potentially replacing entry)
	 * 4) Creating entry (potentially replacing label)
	 */
	use_label = is_multi_file_window (window) || !nautilus_file_can_rename (get_original_file (window));
	new_widget = !window->details->name_field || (use_label ? NAUTILUS_IS_ENTRY (window->details->name_field) : GTK_IS_LABEL (window->details->name_field));

	if (new_widget) {
		if (window->details->name_field) {
			gtk_widget_destroy (window->details->name_field);
		}

		if (use_label) {
			window->details->name_field = GTK_WIDGET 
				(attach_ellipsizing_value_label (window->details->basic_grid,
								 GTK_WIDGET (window->details->name_label),
								 name));
		} else {
			window->details->name_field = nautilus_entry_new ();
			gtk_entry_set_text (GTK_ENTRY (window->details->name_field), name);
			gtk_widget_show (window->details->name_field);

			gtk_grid_attach_next_to (window->details->basic_grid, window->details->name_field,
						 GTK_WIDGET (window->details->name_label),
						 GTK_POS_RIGHT, 1, 1);
			gtk_label_set_mnemonic_widget (GTK_LABEL (window->details->name_label), window->details->name_field);
			
			/* FIXME bugzilla.gnome.org 42151:
			 * With this (and one place elsewhere in this file, not sure which is the
			 * trouble-causer) code in place, bug 2151 happens (crash on quit). Since
			 * we've removed Undo from Nautilus for now, I'm just ifdeffing out this
			 * code rather than trying to fix 2151 now. Note that it might be possible
			 * to fix 2151 without making Undo actually work, it's just not worth the
			 * trouble.
			 */
#ifdef UNDO_ENABLED
			/* Set up name field for undo */
			nautilus_undo_set_up_nautilus_entry_for_undo ( NAUTILUS_ENTRY (window->details->name_field));
			nautilus_undo_editable_set_undo_key (GTK_EDITABLE (window->details->name_field), TRUE);
#endif

			g_signal_connect_object (window->details->name_field, "focus_out_event",
						 G_CALLBACK (name_field_focus_out), window, 0);
			g_signal_connect_object (window->details->name_field, "activate",
						 G_CALLBACK (name_field_activate), window, 0);
		}

		gtk_widget_show (window->details->name_field);
	}
	/* Only replace text if the file's name has changed. */ 
	else if (original_name == NULL || strcmp (original_name, name) != 0) {
		
		if (use_label) {
			gtk_label_set_text (GTK_LABEL (window->details->name_field), name);
		} else {
			/* Only reset the text if it's different from what is
			 * currently showing. This causes minimal ripples (e.g.
			 * selection change).
			 */
			gchar *displayed_name = gtk_editable_get_chars (GTK_EDITABLE (window->details->name_field), 0, -1);
			if (strcmp (displayed_name, name) != 0) {
				gtk_entry_set_text (GTK_ENTRY (window->details->name_field), name);
			}
			g_free (displayed_name);
		}
	}
}

static void
update_name_field (NautilusPropertiesWindow *window)
{
	NautilusFile *file;

	gtk_label_set_text_with_mnemonic (window->details->name_label,
					  ngettext ("_Name:", "_Names:",
						    get_not_gone_original_file_count (window)));

	if (is_multi_file_window (window)) {
		/* Multifile property dialog, show all names */
		GString *str;
		char *name;
		gboolean first;
		GList *l;
		
		str = g_string_new ("");

		first = TRUE;

		for (l = window->details->target_files; l != NULL; l = l->next) {
			file = NAUTILUS_FILE (l->data);

			if (!nautilus_file_is_gone (file)) {
				if (!first) {
					g_string_append (str, ", ");
				} 
				first = FALSE;
				
				name = nautilus_file_get_display_name (file);
				g_string_append (str, name);
				g_free (name);
			}
		}
		set_name_field (window, NULL, str->str);
		g_string_free (str, TRUE);
	} else {
		const char *original_name = NULL;
		char *current_name;

		file = get_original_file (window);

		if (file == NULL || nautilus_file_is_gone (file)) {
			current_name = g_strdup ("");
		} else {
			current_name = nautilus_file_get_display_name (file);
		}

		/* If the file name has changed since the original name was stored,
		 * update the text in the text field, possibly (deliberately) clobbering
		 * an edit in progress. If the name hasn't changed (but some other
		 * aspect of the file might have), then don't clobber changes.
		 */
		if (window->details->name_field) {
			original_name = (const char *) g_object_get_data (G_OBJECT (window->details->name_field), "original_name");
		}

		set_name_field (window, original_name, current_name);

		if (original_name == NULL || 
		    g_strcmp0 (original_name, current_name) != 0) {
			g_object_set_data_full (G_OBJECT (window->details->name_field),
						"original_name",
						current_name,
						g_free);
		} else {
			g_free (current_name);
		}
	}
}

static void
name_field_restore_original_name (NautilusEntry *name_field)
{
	const char *original_name;
	char *displayed_name;

	original_name = (const char *) g_object_get_data (G_OBJECT (name_field),
							  "original_name");

	if (!original_name) {
		return;
	}

	displayed_name = gtk_editable_get_chars (GTK_EDITABLE (name_field), 0, -1);

	if (strcmp (original_name, displayed_name) != 0) {
		gtk_entry_set_text (GTK_ENTRY (name_field), original_name);
	}
	nautilus_entry_select_all (name_field);

	g_free (displayed_name);
}

static void
rename_callback (NautilusFile *file, GFile *res_loc, GError *error, gpointer callback_data)
{
	NautilusPropertiesWindow *window;

	window = NAUTILUS_PROPERTIES_WINDOW (callback_data);

	/* Complain to user if rename failed. */
	if (error != NULL) {
		nautilus_report_error_renaming_file (file, 
						     window->details->pending_name, 
						     error,
						     GTK_WINDOW (window));
		if (window->details->name_field != NULL) {
			name_field_restore_original_name (NAUTILUS_ENTRY (window->details->name_field));
		}
	}

	g_object_unref (window);
}

static void
set_pending_name (NautilusPropertiesWindow *window, const char *name)
{
	g_free (window->details->pending_name);
	window->details->pending_name = g_strdup (name);
}

static void
name_field_done_editing (NautilusEntry *name_field, NautilusPropertiesWindow *window)
{
	NautilusFile *file;
	char *new_name;
	const char *original_name;
	
	g_return_if_fail (NAUTILUS_IS_ENTRY (name_field));

	/* Don't apply if the dialog has more than one file */
	if (is_multi_file_window (window)) {
		return;
	}	

	file = get_original_file (window);

	/* This gets called when the window is closed, which might be
	 * caused by the file having been deleted.
	 */
	if (file == NULL || nautilus_file_is_gone  (file)) {
		return;
	}

	new_name = gtk_editable_get_chars (GTK_EDITABLE (name_field), 0, -1);

	/* Special case: silently revert text if new text is empty. */
	if (strlen (new_name) == 0) {
		name_field_restore_original_name (NAUTILUS_ENTRY (name_field));
	} else {
		original_name = (const char *) g_object_get_data (G_OBJECT (window->details->name_field),
								  "original_name");
		/* Don't rename if not changed since we read the display name.
		   This is needed so that we don't save the display name to the
		   file when nothing is changed */
		if (strcmp (new_name, original_name) != 0) {		
			set_pending_name (window, new_name);
			g_object_ref (window);
			nautilus_file_rename (file, new_name,
					      rename_callback, window);
		}
	}

	g_free (new_name);
}

static gboolean
name_field_focus_out (NautilusEntry *name_field,
		      GdkEventFocus *event,
		      gpointer callback_data)
{
	g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (callback_data));

	if (gtk_widget_get_sensitive (GTK_WIDGET (name_field))) {
		name_field_done_editing (name_field, NAUTILUS_PROPERTIES_WINDOW (callback_data));
	}

	return FALSE;
}

static void
name_field_activate (NautilusEntry *name_field, gpointer callback_data)
{
	g_assert (NAUTILUS_IS_ENTRY (name_field));
	g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (callback_data));

	/* Accept changes. */
	name_field_done_editing (name_field, NAUTILUS_PROPERTIES_WINDOW (callback_data));

	nautilus_entry_select_all_at_idle (name_field);
}

static void
update_properties_window_title (NautilusPropertiesWindow *window)
{
	char *name, *title;
	NautilusFile *file;

	g_return_if_fail (GTK_IS_WINDOW (window));

	title = g_strdup_printf (_("Properties"));

	if (!is_multi_file_window (window)) {
		file = get_original_file (window);

		if (file != NULL) {
			g_free (title);
			name = nautilus_file_get_display_name (file);
			title = g_strdup_printf (_("%s Properties"), name);
			g_free (name);
		}
	}
	
  	gtk_window_set_title (GTK_WINDOW (window), title);

	g_free (title);
}

static void
clear_extension_pages (NautilusPropertiesWindow *window)
{
	int i;
	int num_pages;
	GtkWidget *page;

	num_pages = gtk_notebook_get_n_pages
				(GTK_NOTEBOOK (window->details->notebook));

	for (i = 0; i < num_pages; i++) {
		page = gtk_notebook_get_nth_page
				(GTK_NOTEBOOK (window->details->notebook), i);

		if (g_object_get_data (G_OBJECT (page), "is-extension-page")) {
			gtk_notebook_remove_page
				(GTK_NOTEBOOK (window->details->notebook), i);
			num_pages--;
			i--;
		}
	}
}

static void
refresh_extension_pages (NautilusPropertiesWindow *window)
{
	clear_extension_pages (window);
	append_extension_pages (window);	
}

static void
remove_from_dialog (NautilusPropertiesWindow *window,
		    NautilusFile *file)
{
	int index;
	GList *original_link;
	GList *target_link;
	NautilusFile *original_file;
	NautilusFile *target_file;

	index = g_list_index (window->details->target_files, file);
	if (index == -1) {
		index = g_list_index (window->details->original_files, file);
		g_return_if_fail (index != -1);
	}	

	original_link = g_list_nth (window->details->original_files, index);
	target_link = g_list_nth (window->details->target_files, index);

	g_return_if_fail (original_link && target_link);

	original_file = NAUTILUS_FILE (original_link->data);
	target_file = NAUTILUS_FILE (target_link->data);
	
	window->details->original_files = g_list_remove_link (window->details->original_files, original_link);
	g_list_free (original_link);

	window->details->target_files = g_list_remove_link (window->details->target_files, target_link);
	g_list_free (target_link);

	g_hash_table_remove (window->details->initial_permissions, target_file);

	g_signal_handlers_disconnect_by_func (original_file,
					      G_CALLBACK (file_changed_callback),
					      window);
	g_signal_handlers_disconnect_by_func (target_file,
					      G_CALLBACK (file_changed_callback),
					      window);

	nautilus_file_monitor_remove (original_file, &window->details->original_files);
	nautilus_file_monitor_remove (target_file, &window->details->target_files);

	nautilus_file_unref (original_file);
	nautilus_file_unref (target_file);
	
}

static gboolean
mime_list_equal (GList *a, GList *b)
{
	while (a && b) {
		if (strcmp (a->data, b->data)) {
			return FALSE;
		}	
		a = a->next;
		b = b->next;
	}

	return (a == b);
}

static GList *
get_mime_list (NautilusPropertiesWindow *window)
{
	GList *ret;
	GList *l;
	
	ret = NULL;
	for (l = window->details->target_files; l != NULL; l = l->next) {
		ret = g_list_append (ret, nautilus_file_get_mime_type (NAUTILUS_FILE (l->data)));
	}
	ret = g_list_reverse (ret);
	return ret;
}

static void
properties_window_update (NautilusPropertiesWindow *window, 
			  GList *files)
{
	GList *l;
	GList *mime_list;
	GList *tmp;
	NautilusFile *changed_file;
	gboolean dirty_original = FALSE;
	gboolean dirty_target = FALSE;

	if (files == NULL) {
		dirty_original = TRUE;
		dirty_target = TRUE;
	}

	for (tmp = files; tmp != NULL; tmp = tmp->next) {
		changed_file = NAUTILUS_FILE (tmp->data);

		if (changed_file && nautilus_file_is_gone (changed_file)) {
			/* Remove the file from the property dialog */
			remove_from_dialog (window, changed_file);
			changed_file = NULL;
			
			if (window->details->original_files == NULL) {
				return;
			}
		}		
		if (changed_file == NULL ||
		    g_list_find (window->details->original_files, changed_file)) {
			dirty_original = TRUE;
		}
		if (changed_file == NULL ||
		    g_list_find (window->details->target_files, changed_file)) {
			dirty_target = TRUE;
		}

	}

	if (dirty_original) {
		update_properties_window_title (window);
		update_properties_window_icon (GTK_IMAGE (window->details->icon_image));

		update_name_field (window);

		/* If any of the value fields start to depend on the original
		 * value, value_field_updates should be added here */
	}

	if (dirty_target) {
		for (l = window->details->permission_buttons; l != NULL; l = l->next) {
			permission_button_update (window, GTK_TOGGLE_BUTTON (l->data));
		}
		
		for (l = window->details->permission_combos; l != NULL; l = l->next) {
			permission_combo_update (window, GTK_COMBO_BOX (l->data));
		}
		
		for (l = window->details->value_fields; l != NULL; l = l->next) {
			value_field_update (window, GTK_LABEL (l->data));
		}
	}

	mime_list = get_mime_list (window);

	if (!window->details->mime_list) {
		window->details->mime_list = mime_list;
	} else {
		if (!mime_list_equal (window->details->mime_list, mime_list)) {
			refresh_extension_pages (window);			
		}

		g_list_free_full (window->details->mime_list, g_free);
		window->details->mime_list = mime_list;
	}
}

static gboolean
update_files_callback (gpointer data)
{
 	NautilusPropertiesWindow *window;
 
 	window = NAUTILUS_PROPERTIES_WINDOW (data);
 
	window->details->update_files_timeout_id = 0;

	properties_window_update (window, window->details->changed_files);
	
	if (window->details->original_files == NULL) {
		/* Close the window if no files are left */
		gtk_widget_destroy (GTK_WIDGET (window));
	} else {
		nautilus_file_list_free (window->details->changed_files);
		window->details->changed_files = NULL;
	}
	
 	return FALSE;
 }

static void
schedule_files_update (NautilusPropertiesWindow *window)
 {
 	g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (window));
 
	if (window->details->update_files_timeout_id == 0) {
		window->details->update_files_timeout_id
			= g_timeout_add (FILES_UPDATE_INTERVAL,
					 update_files_callback,
 					 window);
 	}
 }

static gboolean
file_list_attributes_identical (GList *file_list, const char *attribute_name)
{
	gboolean identical;
	char *first_attr;
	GList *l;
	
	first_attr = NULL;
	identical = TRUE;
	
	for (l = file_list; l != NULL; l = l->next) {
		NautilusFile *file;

		file = NAUTILUS_FILE (l->data);
	
		if (nautilus_file_is_gone (file)) {
			continue;
		}

		if (first_attr == NULL) {
			first_attr = nautilus_file_get_string_attribute_with_default (file, attribute_name);
		} else {
			char *attr;
			attr = nautilus_file_get_string_attribute_with_default (file, attribute_name);
			if (strcmp (attr, first_attr)) {
				identical = FALSE;
				g_free (attr);
				break;
			}
			g_free (attr);
		}
	}

	g_free (first_attr);
	return identical;
}

static char *
file_list_get_string_attribute (GList *file_list, 
				const char *attribute_name,
				const char *inconsistent_value)
{
	if (file_list_attributes_identical (file_list, attribute_name)) {
		GList *l;
		
		for (l = file_list; l != NULL; l = l->next) {
			NautilusFile *file;
			
			file = NAUTILUS_FILE (l->data);
			if (!nautilus_file_is_gone (file)) {
				return nautilus_file_get_string_attribute_with_default
					(file, 
					 attribute_name);
			}
		}
		return g_strdup (_("unknown"));
	} else {
		return g_strdup (inconsistent_value);
	}
}


static gboolean
file_list_all_directories (GList *file_list)
{
	GList *l;
	for (l = file_list; l != NULL; l = l->next) {
		if (!nautilus_file_is_directory (NAUTILUS_FILE (l->data))) {
			return FALSE;
		}
	}
	return TRUE;
}

static void
value_field_update_internal (GtkLabel *label, 
			     GList *file_list)
{
	const char *attribute_name;
	char *attribute_value;
	char *inconsistent_string;
	char *mime_type, *tmp;

	g_assert (GTK_IS_LABEL (label));

	attribute_name = g_object_get_data (G_OBJECT (label), "file_attribute");
	inconsistent_string = g_object_get_data (G_OBJECT (label), "inconsistent_string");
	attribute_value = file_list_get_string_attribute (file_list, 
							  attribute_name,
							  inconsistent_string);
	if (!strcmp (attribute_name, "type") && strcmp (attribute_value, inconsistent_string)) {
		mime_type = file_list_get_string_attribute (file_list,
							    "mime_type",
							    inconsistent_string);
		if (strcmp (mime_type, inconsistent_string)) {
			tmp = attribute_value;
			attribute_value = g_strdup_printf (C_("MIME type description (MIME type)", "%s (%s)"), attribute_value, mime_type);
			g_free (tmp);
		}
		g_free (mime_type);
	}

	gtk_label_set_text (label, attribute_value);
	g_free (attribute_value);
}

static void
value_field_update (NautilusPropertiesWindow *window, GtkLabel *label)
{
	gboolean use_original;

	use_original = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (label), "show_original"));

	value_field_update_internal (label, 
				     (use_original ?
				      window->details->original_files : 
				      window->details->target_files));
}

static GtkLabel *
attach_label (GtkGrid *grid,
	      GtkWidget *sibling,
	      const char *initial_text,
	      gboolean right_aligned,
	      gboolean bold,
	      gboolean ellipsize_text,
	      gboolean selectable,
	      gboolean mnemonic)
{
	GtkWidget *label_field;

	if (ellipsize_text) {
		label_field = gtk_label_new (initial_text);
                gtk_label_set_ellipsize (GTK_LABEL (label_field),
                                         right_aligned ? PANGO_ELLIPSIZE_START :
                                                         PANGO_ELLIPSIZE_END);
	} else if (mnemonic) {
		label_field = gtk_label_new_with_mnemonic (initial_text);
	} else {
		label_field = gtk_label_new (initial_text);
	}

	if (selectable) {
		gtk_label_set_selectable (GTK_LABEL (label_field), TRUE);
	}
	
	if (bold) {
		eel_gtk_label_make_bold (GTK_LABEL (label_field));
	}
	gtk_misc_set_alignment (GTK_MISC (label_field), right_aligned ? 1 : 0, 0.5);
	gtk_widget_show (label_field);

	if (ellipsize_text) {
		gtk_widget_set_hexpand (label_field, TRUE);
	}

	if (sibling != NULL) {
		gtk_grid_attach_next_to (grid, label_field, sibling,
					 GTK_POS_RIGHT, 1, 1);
	} else {
		gtk_container_add (GTK_CONTAINER (grid), label_field);
	}

	return GTK_LABEL (label_field);
}	      

static GtkLabel *
attach_value_label (GtkGrid *grid,
		    GtkWidget *sibling,
		    const char *initial_text)
{
	return attach_label (grid, sibling, initial_text, FALSE, FALSE, FALSE, TRUE, FALSE);
}

static GtkLabel *
attach_ellipsizing_value_label (GtkGrid *grid,
				GtkWidget *sibling,
				const char *initial_text)
{
	return attach_label (grid, sibling, initial_text, FALSE, FALSE, TRUE, TRUE, FALSE);
}

static GtkWidget*
attach_value_field_internal (NautilusPropertiesWindow *window,
			     GtkGrid *grid,
			     GtkWidget *sibling,
			     const char *file_attribute_name,
			     const char *inconsistent_string,
			     gboolean show_original,
			     gboolean ellipsize_text)
{
	GtkLabel *value_field;

	if (ellipsize_text) {
		value_field = attach_ellipsizing_value_label (grid, sibling, "");
	} else {
		value_field = attach_value_label (grid, sibling, "");
	}

  	/* Stash a copy of the file attribute name in this field for the callback's sake. */
	g_object_set_data_full (G_OBJECT (value_field), "file_attribute",
				g_strdup (file_attribute_name), g_free);

	g_object_set_data_full (G_OBJECT (value_field), "inconsistent_string",
				g_strdup (inconsistent_string), g_free);

	g_object_set_data (G_OBJECT (value_field), "show_original", GINT_TO_POINTER (show_original));

	window->details->value_fields = g_list_prepend (window->details->value_fields,
							value_field);
	return GTK_WIDGET(value_field);
}			     

static GtkWidget*
attach_value_field (NautilusPropertiesWindow *window,
		    GtkGrid *grid,
		    GtkWidget *sibling,
		    const char *file_attribute_name,
		    const char *inconsistent_string,
		    gboolean show_original)
{
	return attach_value_field_internal (window, 
					    grid, sibling,
					    file_attribute_name, 
					    inconsistent_string,
					    show_original,
					    FALSE);
}

static GtkWidget*
attach_ellipsizing_value_field (NautilusPropertiesWindow *window,
				GtkGrid *grid,
				GtkWidget *sibling,
		    		const char *file_attribute_name,
				const char *inconsistent_string,
				gboolean show_original)
{
	return attach_value_field_internal (window,
					    grid, sibling, 
					    file_attribute_name, 
					    inconsistent_string, 
					    show_original,
					    TRUE);
}

static void
group_change_callback (NautilusFile *file,
		       GFile *res_loc,
		       GError *error,
		       NautilusPropertiesWindow *window)
{
	char *group;

	g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (window));
	g_assert (window->details->group_change_file == file);

	group = window->details->group_change_group;
	g_assert (group != NULL);

	/* Report the error if it's an error. */
	eel_timed_wait_stop ((EelCancelCallback) cancel_group_change_callback, window);
	nautilus_report_error_setting_group (file, error, GTK_WINDOW (window));

	nautilus_file_unref (file);
	g_free (group);

	window->details->group_change_file = NULL;
	window->details->group_change_group = NULL;
	g_object_unref (G_OBJECT (window));
}

static void
cancel_group_change_callback (NautilusPropertiesWindow *window)
{
	NautilusFile *file;
	char *group;

	file = window->details->group_change_file;
	g_assert (NAUTILUS_IS_FILE (file));

	group = window->details->group_change_group;
	g_assert (group != NULL);

	nautilus_file_cancel (file, (NautilusFileOperationCallback) group_change_callback, window);

	g_free (group);
	nautilus_file_unref (file);

	window->details->group_change_file = NULL;
	window->details->group_change_group = NULL;
	g_object_unref (window);
}

static gboolean
schedule_group_change_timeout (NautilusPropertiesWindow *window)
{
	NautilusFile *file;
	char *group;

	g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (window));

	file = window->details->group_change_file;
	g_assert (NAUTILUS_IS_FILE (file));

	group = window->details->group_change_group;
	g_assert (group != NULL);

	eel_timed_wait_start
		((EelCancelCallback) cancel_group_change_callback,
		 window,
		 _("Cancel Group Change?"),
		 GTK_WINDOW (window));

	nautilus_file_set_group
		(file,  group,
		 (NautilusFileOperationCallback) group_change_callback, window);

	window->details->group_change_timeout = 0;
	return FALSE;
}

static void
schedule_group_change (NautilusPropertiesWindow *window,
		       NautilusFile       *file,
		       const char         *group)
{
	g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (window));
	g_assert (window->details->group_change_group == NULL);
	g_assert (window->details->group_change_file == NULL);
	g_assert (NAUTILUS_IS_FILE (file));

	window->details->group_change_file = nautilus_file_ref (file);
	window->details->group_change_group = g_strdup (group);
	g_object_ref (G_OBJECT (window));
	window->details->group_change_timeout =
		g_timeout_add (CHOWN_CHGRP_TIMEOUT,
			       (GSourceFunc) schedule_group_change_timeout,
			       window);
}

static void
unschedule_or_cancel_group_change (NautilusPropertiesWindow *window)
{
	NautilusFile *file;
	char *group;

	g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (window));

	file = window->details->group_change_file;
	group = window->details->group_change_group;

	g_assert ((file == NULL && group == NULL) ||
		  (file != NULL && group != NULL));

	if (file != NULL) {
		g_assert (NAUTILUS_IS_FILE (file));

		if (window->details->group_change_timeout == 0) {
			nautilus_file_cancel (file,
					      (NautilusFileOperationCallback) group_change_callback, window);
			eel_timed_wait_stop ((EelCancelCallback) cancel_group_change_callback, window);
		}

		nautilus_file_unref (file);
		g_free (group);

		window->details->group_change_file = NULL;
		window->details->group_change_group = NULL;
		g_object_unref (G_OBJECT (window));
	}

	if (window->details->group_change_timeout > 0) {
		g_assert (file != NULL);
		g_source_remove (window->details->group_change_timeout);
		window->details->group_change_timeout = 0;
	}
}

static void
changed_group_callback (GtkComboBox *combo_box, NautilusFile *file)
{
	NautilusPropertiesWindow *window;
	char *group;
	char *cur_group;

	g_assert (GTK_IS_COMBO_BOX (combo_box));
	g_assert (NAUTILUS_IS_FILE (file));

	group = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo_box));
	cur_group = nautilus_file_get_group_name (file);

	if (group != NULL && strcmp (group, cur_group) != 0) {
		/* Try to change file group. If this fails, complain to user. */
		window = NAUTILUS_PROPERTIES_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (combo_box), GTK_TYPE_WINDOW));

		unschedule_or_cancel_group_change (window);
		schedule_group_change (window, file, group);
	}
	g_free (group);
	g_free (cur_group);
}

/* checks whether the given column at the first level
 * of model has the specified entries in the given order. */
static gboolean
tree_model_entries_equal (GtkTreeModel *model,
			  unsigned int  column,
			  GList        *entries)
{
	GtkTreeIter iter;
	gboolean empty_model;

	g_assert (GTK_IS_TREE_MODEL (model));
	g_assert (gtk_tree_model_get_column_type (model, column) == G_TYPE_STRING);

	empty_model = !gtk_tree_model_get_iter_first (model, &iter);

	if (!empty_model && entries != NULL) {
		GList *l;

		l = entries;

		do {
			char *val;

			gtk_tree_model_get (model, &iter,
					    column, &val,
					    -1);
			if ((val == NULL && l->data != NULL) ||
			    (val != NULL && l->data == NULL) ||
			    (val != NULL && strcmp (val, l->data))) {
				g_free (val);
				return FALSE;
			}

			g_free (val);
			l = l->next;
		} while (gtk_tree_model_iter_next (model, &iter));

		return l == NULL;
	} else {
		return (empty_model && entries == NULL) ||
		       (!empty_model && entries != NULL);
	}
}

static char *
combo_box_get_active_entry (GtkComboBox *combo_box,
			    unsigned int column)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *val;

	g_assert (GTK_IS_COMBO_BOX (combo_box));

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo_box), &iter)) {
		model = gtk_combo_box_get_model (combo_box);
		g_assert (GTK_IS_TREE_MODEL (model));

		gtk_tree_model_get (model, &iter,
				    column, &val,
				    -1);
		return val;
	}

	return NULL;
}

/* returns the index of the given entry in the the given column
 * at the first level of model. Returns -1 if entry can't be found
 * or entry is NULL.
 * */
static int
tree_model_get_entry_index (GtkTreeModel *model,
			    unsigned int  column,
			    const char   *entry)
{
	GtkTreeIter iter;
	int index;
	gboolean empty_model;

	g_assert (GTK_IS_TREE_MODEL (model));
	g_assert (gtk_tree_model_get_column_type (model, column) == G_TYPE_STRING);

	empty_model = !gtk_tree_model_get_iter_first (model, &iter);
	if (!empty_model && entry != NULL) {
		index = 0;

		do {
			char *val;

			gtk_tree_model_get (model, &iter,
					    column, &val,
					    -1);
			if (val != NULL && !strcmp (val, entry)) {
				g_free (val);
				return index;
			}

			g_free (val);
			index++;
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	return -1;
}


static void
synch_groups_combo_box (GtkComboBox *combo_box, NautilusFile *file)
{
	GList *groups;
	GList *node;
	GtkTreeModel *model;
	GtkListStore *store;
	const char *group_name;
	char *current_group_name;
	int group_index;
	int current_group_index;

	g_assert (GTK_IS_COMBO_BOX (combo_box));
	g_assert (NAUTILUS_IS_FILE (file));

	if (nautilus_file_is_gone (file)) {
		return;
	}

	groups = nautilus_file_get_settable_group_names (file);

	model = gtk_combo_box_get_model (combo_box);
	store = GTK_LIST_STORE (model);
	g_assert (GTK_IS_LIST_STORE (model));

	if (!tree_model_entries_equal (model, 0, groups)) {
		/* Clear the contents of ComboBox in a wacky way because there
		 * is no function to clear all items and also no function to obtain
		 * the number of items in a combobox.
		 */
		gtk_list_store_clear (store);

		for (node = groups, group_index = 0; node != NULL; node = node->next, ++group_index) {
			group_name = (const char *)node->data;
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box), group_name);
		}
	}

	current_group_name = nautilus_file_get_group_name (file);
	current_group_index = tree_model_get_entry_index (model, 0, current_group_name);

	/* If current group wasn't in list, we prepend it (with a separator). 
	 * This can happen if the current group is an id with no matching
	 * group in the groups file.
	 */
	if (current_group_index < 0 && current_group_name != NULL) {
		if (groups != NULL) {
			/* add separator */
			gtk_combo_box_text_prepend_text (GTK_COMBO_BOX_TEXT (combo_box), "-");
		}

		gtk_combo_box_text_prepend_text (GTK_COMBO_BOX_TEXT (combo_box), current_group_name);
		current_group_index = 0;
	}
	gtk_combo_box_set_active (combo_box, current_group_index);

	g_free (current_group_name);
	g_list_free_full (groups, g_free);
}

static gboolean
combo_box_row_separator_func (GtkTreeModel *model,
			      GtkTreeIter  *iter,
			      gpointer      data)
{
  	gchar *text;
	gboolean ret;

  	gtk_tree_model_get (model, iter, 0, &text, -1);

	if (text == NULL) {
		return FALSE;
	}

  	if (strcmp (text, "-") == 0) {
    		ret = TRUE;
	} else {
		ret = FALSE;
	}
	
  	g_free (text);
  	return ret;
}

static GtkComboBox *
attach_combo_box (GtkGrid *grid,
		  GtkWidget *sibling,
		  gboolean two_columns)
{
	GtkWidget *combo_box;
	GtkWidget *aligner;

	if (!two_columns) {
		combo_box = gtk_combo_box_text_new ();
	} else {
		GtkTreeModel *model;
		GtkCellRenderer *renderer;

		model = GTK_TREE_MODEL (gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING));
		combo_box = gtk_combo_box_new_with_model (model);
		g_object_unref (G_OBJECT (model));

		renderer = gtk_cell_renderer_text_new ();
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
		gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo_box), renderer,
					       "text", 0);
		
	}
	gtk_widget_show (combo_box);

  	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combo_box),
					      combo_box_row_separator_func,
					      NULL,
					      NULL);

	/* Put combo box in alignment to make it left-justified
	 * but minimally sized.
	 */
	aligner = gtk_alignment_new (0, 0.5, 0, 0);
	gtk_widget_show (aligner);

	gtk_container_add (GTK_CONTAINER (aligner), combo_box);
	gtk_grid_attach_next_to (grid, aligner, sibling,
				 GTK_POS_RIGHT, 1, 1);

	return GTK_COMBO_BOX (combo_box);
}		    	

static GtkComboBox*
attach_group_combo_box (GtkGrid *grid,
			GtkWidget *sibling,
		        NautilusFile *file)
{
	GtkComboBox *combo_box;

	combo_box = attach_combo_box (grid, sibling, FALSE);

	synch_groups_combo_box (combo_box, file);

	/* Connect to signal to update menu when file changes. */
	g_signal_connect_object (file, "changed",
				 G_CALLBACK (synch_groups_combo_box),
				 combo_box, G_CONNECT_SWAPPED);
	g_signal_connect_data (combo_box, "changed",
			       G_CALLBACK (changed_group_callback),
			       nautilus_file_ref (file),
			       (GClosureNotify)nautilus_file_unref, 0);

	return combo_box;
}	

static void
owner_change_callback (NautilusFile *file,
                       GFile 	    *result_location,
		       GError        *error,
		       NautilusPropertiesWindow *window)
{
	char *owner;

	g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (window));
	g_assert (window->details->owner_change_file == file);

	owner = window->details->owner_change_owner;
	g_assert (owner != NULL);

	/* Report the error if it's an error. */
	eel_timed_wait_stop ((EelCancelCallback) cancel_owner_change_callback, window);
	nautilus_report_error_setting_owner (file, error, GTK_WINDOW (window));

	nautilus_file_unref (file);
	g_free (owner);

	window->details->owner_change_file = NULL;
	window->details->owner_change_owner = NULL;
	g_object_unref (G_OBJECT (window));
}

static void
cancel_owner_change_callback (NautilusPropertiesWindow *window)
{
	NautilusFile *file;
	char *owner;

	file = window->details->owner_change_file;
	g_assert (NAUTILUS_IS_FILE (file));

	owner = window->details->owner_change_owner;
	g_assert (owner != NULL);

	nautilus_file_cancel (file, (NautilusFileOperationCallback) owner_change_callback, window);

	nautilus_file_unref (file);
	g_free (owner);

	window->details->owner_change_file = NULL;
	window->details->owner_change_owner = NULL;
	g_object_unref (window);
}

static gboolean
schedule_owner_change_timeout (NautilusPropertiesWindow *window)
{
	NautilusFile *file;
	char *owner;

	g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (window));

	file = window->details->owner_change_file;
	g_assert (NAUTILUS_IS_FILE (file));

	owner = window->details->owner_change_owner;
	g_assert (owner != NULL);

	eel_timed_wait_start
		((EelCancelCallback) cancel_owner_change_callback,
		 window,
		 _("Cancel Owner Change?"),
		 GTK_WINDOW (window));

	nautilus_file_set_owner
		(file,  owner,
		 (NautilusFileOperationCallback) owner_change_callback, window);

	window->details->owner_change_timeout = 0;
	return FALSE;
}

static void
schedule_owner_change (NautilusPropertiesWindow *window,
		       NautilusFile       *file,
		       const char         *owner)
{
	g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (window));
	g_assert (window->details->owner_change_owner == NULL);
	g_assert (window->details->owner_change_file == NULL);
	g_assert (NAUTILUS_IS_FILE (file));

	window->details->owner_change_file = nautilus_file_ref (file);
	window->details->owner_change_owner = g_strdup (owner);
	g_object_ref (G_OBJECT (window));
	window->details->owner_change_timeout =
		g_timeout_add (CHOWN_CHGRP_TIMEOUT,
			       (GSourceFunc) schedule_owner_change_timeout,
			       window);
}

static void
unschedule_or_cancel_owner_change (NautilusPropertiesWindow *window)
{
	NautilusFile *file;
	char *owner;

	g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (window));

	file = window->details->owner_change_file;
	owner = window->details->owner_change_owner;

	g_assert ((file == NULL && owner == NULL) ||
		  (file != NULL && owner != NULL));

	if (file != NULL) {
		g_assert (NAUTILUS_IS_FILE (file));

		if (window->details->owner_change_timeout == 0) {
			nautilus_file_cancel (file,
					      (NautilusFileOperationCallback) owner_change_callback, window);
			eel_timed_wait_stop ((EelCancelCallback) cancel_owner_change_callback, window);
		}

		nautilus_file_unref (file);
		g_free (owner);

		window->details->owner_change_file = NULL;
		window->details->owner_change_owner = NULL;
		g_object_unref (G_OBJECT (window));
	}

	if (window->details->owner_change_timeout > 0) {
		g_assert (file != NULL);
		g_source_remove (window->details->owner_change_timeout);
		window->details->owner_change_timeout = 0;
	}
}

static void
changed_owner_callback (GtkComboBox *combo_box, NautilusFile* file)
{
	NautilusPropertiesWindow *window;
	char *owner_text;
	char **name_array;
	char *new_owner;
	char *cur_owner;

	g_assert (GTK_IS_COMBO_BOX (combo_box));
	g_assert (NAUTILUS_IS_FILE (file));

	owner_text = combo_box_get_active_entry (combo_box, 0);
        if (! owner_text)
	    return;
    	name_array = g_strsplit (owner_text, " - ", 2);
	new_owner = name_array[0];
	g_free (owner_text);
	cur_owner = nautilus_file_get_owner_name (file);

	if (strcmp (new_owner, cur_owner) != 0) {
		/* Try to change file owner. If this fails, complain to user. */
		window = NAUTILUS_PROPERTIES_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (combo_box), GTK_TYPE_WINDOW));

		unschedule_or_cancel_owner_change (window);
		schedule_owner_change (window, file, new_owner);
	}
	g_strfreev (name_array);
	g_free (cur_owner);
}

static void
synch_user_menu (GtkComboBox *combo_box, NautilusFile *file)
{
	GList *users;
	GList *node;
	GtkTreeModel *model;
	GtkListStore *store;
	GtkTreeIter iter;
	char *user_name;
	char *owner_name;
	int user_index;
	int owner_index;
	char **name_array;
	char *combo_text;

	g_assert (GTK_IS_COMBO_BOX (combo_box));
	g_assert (NAUTILUS_IS_FILE (file));

	if (nautilus_file_is_gone (file)) {
		return;
	}

	users = nautilus_get_user_names ();

	model = gtk_combo_box_get_model (combo_box);
	store = GTK_LIST_STORE (model);
	g_assert (GTK_IS_LIST_STORE (model));

	if (!tree_model_entries_equal (model, 1, users)) {
		/* Clear the contents of ComboBox in a wacky way because there
		 * is no function to clear all items and also no function to obtain
		 * the number of items in a combobox.
		 */
		gtk_list_store_clear (store);

		for (node = users, user_index = 0; node != NULL; node = node->next, ++user_index) {
			user_name = (char *)node->data;

			name_array = g_strsplit (user_name, "\n", 2);
			if (name_array[1] != NULL) {
				combo_text = g_strdup_printf ("%s - %s", name_array[0], name_array[1]);
			} else {
				combo_text = g_strdup (name_array[0]);
			}

			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter,
					    0, combo_text,
					    1, user_name,
					    -1);

			g_strfreev (name_array);
			g_free (combo_text);
		}
	}

	owner_name = nautilus_file_get_string_attribute (file, "owner");
	owner_index = tree_model_get_entry_index (model, 0, owner_name);

	/* If owner wasn't in list, we prepend it (with a separator). 
	 * This can happen if the owner is an id with no matching
	 * identifier in the passwords file.
	 */
	if (owner_index < 0 && owner_name != NULL) {
		if (users != NULL) {
			/* add separator */
			gtk_list_store_prepend (store, &iter);
			gtk_list_store_set (store, &iter,
					    0, "-",
					    1, NULL,
					    -1);
		}

		name_array = g_strsplit (owner_name, " - ", 2);
		if (name_array[1] != NULL) {
			user_name = g_strdup_printf ("%s\n%s", name_array[0], name_array[1]);
		} else {
			user_name = g_strdup (name_array[0]);
		}
		owner_index = 0;

		gtk_list_store_prepend (store, &iter);
		gtk_list_store_set (store, &iter,
				    0, owner_name,
				    1, user_name,
				    -1);

		g_free (user_name);
		g_strfreev (name_array);
	}

	gtk_combo_box_set_active (combo_box, owner_index);

	g_free (owner_name);
	g_list_free_full (users, g_free);
}	

static GtkComboBox*
attach_owner_combo_box (GtkGrid *grid,
		        GtkWidget *sibling,
		        NautilusFile *file)
{
	GtkComboBox *combo_box;

	combo_box = attach_combo_box (grid, sibling, TRUE);

	synch_user_menu (combo_box, file);

	/* Connect to signal to update menu when file changes. */
	g_signal_connect_object (file, "changed",
				 G_CALLBACK (synch_user_menu),
				 combo_box, G_CONNECT_SWAPPED);	
	g_signal_connect_data (combo_box, "changed",
			       G_CALLBACK (changed_owner_callback),
			       nautilus_file_ref (file),
			       (GClosureNotify)nautilus_file_unref, 0);

	return combo_box;
}

static gboolean
file_has_prefix (NautilusFile *file,
		 GList *prefix_candidates)
{
	GList *p;
	GFile *location, *candidate_location;

	location = nautilus_file_get_location (file);

	for (p = prefix_candidates; p != NULL; p = p->next) {
		if (file == p->data) {
			continue;
		}

		candidate_location = nautilus_file_get_location (NAUTILUS_FILE (p->data));
		if (g_file_has_prefix (location, candidate_location)) {
			g_object_unref (location);
			g_object_unref (candidate_location);
			return TRUE;
		}
		g_object_unref (candidate_location);
	}

	g_object_unref (location);

	return FALSE;
}

static void
directory_contents_value_field_update (NautilusPropertiesWindow *window)
{
	NautilusRequestStatus file_status, status;
	char *text, *temp;
	guint directory_count;
	guint file_count;
	guint total_count;
	guint unreadable_directory_count;
	goffset total_size;
	gboolean used_two_lines;
	NautilusFile *file;
	GList *l;
	guint file_unreadable;
	goffset file_size;

	g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (window));

	status = NAUTILUS_REQUEST_DONE;
	file_status = NAUTILUS_REQUEST_NOT_STARTED;
	total_count = window->details->total_count;
	total_size = window->details->total_size;
	unreadable_directory_count = FALSE;

	for (l = window->details->target_files; l; l = l->next) {
		file = NAUTILUS_FILE (l->data);

		if (file_has_prefix (file, window->details->target_files)) {
			/* don't count nested files twice */
			continue;
		}

		if (nautilus_file_is_directory (file)) {
			file_status = nautilus_file_get_deep_counts (file, 
					 &directory_count,
					 &file_count, 
					 &file_unreadable,
					 &file_size,
					 TRUE);
			total_count += (file_count + directory_count);
			total_size += file_size;
			
			if (file_unreadable) {
				unreadable_directory_count = TRUE;
			}
			
			if (file_status != NAUTILUS_REQUEST_DONE) {
				status = file_status;
			}
		} else {
			++total_count;
			total_size += nautilus_file_get_size (file);
		}
	}
	
	/* If we've already displayed the total once, don't do another visible
	 * count-up if the deep_count happens to get invalidated.
	 * But still display the new total, since it might have changed.
	 */
	if (window->details->deep_count_finished &&
	    status != NAUTILUS_REQUEST_DONE) {
		return;
	}

	text = NULL;
	used_two_lines = FALSE;
	
	if (total_count == 0) {
		switch (status) {
		case NAUTILUS_REQUEST_DONE:
			if (unreadable_directory_count == 0) {
				text = g_strdup (_("nothing"));
			} else {
				text = g_strdup (_("unreadable"));
			}
			
			break;
		default:
			text = g_strdup ("...");
		}
	} else {
		char *size_str;
		size_str = g_format_size (total_size);
		text = g_strdup_printf (ngettext("%'d item, with size %s",
						 "%'d items, totalling %s",
						 total_count),
					total_count, size_str);
		g_free (size_str);

		if (unreadable_directory_count != 0) {
			temp = text;
			text = g_strconcat (temp, "\n",
					    _("(some contents unreadable)"),
					    NULL);
			g_free (temp);
			used_two_lines = TRUE;
		}
	}

	gtk_label_set_text (window->details->directory_contents_value_field,
			    text);
	g_free (text);

	/* Also set the title field here, with a trailing carriage return &
	 * space if the value field has two lines. This is a hack to get the
	 * "Contents:" title to line up with the first line of the
	 * 2-line value. Maybe there's a better way to do this, but I
	 * couldn't think of one.
	 */
	text = g_strdup (_("Contents:"));
	if (used_two_lines) {
		temp = text;
		text = g_strconcat (temp, "\n ", NULL);
		g_free (temp);
	}
	gtk_label_set_text (window->details->directory_contents_title_field,
			    text);
	g_free (text);

	if (status == NAUTILUS_REQUEST_DONE) {
		window->details->deep_count_finished = TRUE;
	}
}

static gboolean
update_directory_contents_callback (gpointer data)
{
	NautilusPropertiesWindow *window;

	window = NAUTILUS_PROPERTIES_WINDOW (data);

	window->details->update_directory_contents_timeout_id = 0;
	directory_contents_value_field_update (window);

	return FALSE;
}

static void
schedule_directory_contents_update (NautilusPropertiesWindow *window)
{
	g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (window));

	if (window->details->update_directory_contents_timeout_id == 0) {
		window->details->update_directory_contents_timeout_id
			= g_timeout_add (DIRECTORY_CONTENTS_UPDATE_INTERVAL,
					 update_directory_contents_callback,
					 window);
	}
}

static GtkLabel *
attach_directory_contents_value_field (NautilusPropertiesWindow *window,
				       GtkGrid *grid,
				       GtkWidget *sibling)
{
	GtkLabel *value_field;
	GList *l;
	NautilusFile *file;

	value_field = attach_value_label (grid, sibling, "");

	g_assert (window->details->directory_contents_value_field == NULL);
	window->details->directory_contents_value_field = value_field;

	gtk_label_set_line_wrap (value_field, TRUE);
	
	/* Fill in the initial value. */
	directory_contents_value_field_update (window);
 
	for (l = window->details->target_files; l; l = l->next) {
		file = NAUTILUS_FILE (l->data);
		nautilus_file_recompute_deep_counts (file);
		
		g_signal_connect_object (file,
					 "updated_deep_count_in_progress",
					 G_CALLBACK (schedule_directory_contents_update),
					 window, G_CONNECT_SWAPPED);
	}
	
	return value_field;	
}

static GtkLabel *
attach_title_field (GtkGrid *grid,
		    const char *title)
{
	return attach_label (grid, NULL, title, FALSE, FALSE, FALSE, FALSE, TRUE);
}		      

#define INCONSISTENT_STATE_STRING \
	"\xE2\x80\x92"

static void
append_title_value_pair (NautilusPropertiesWindow *window,
			 GtkGrid *grid,
			 const char *title, 
 			 const char *file_attribute_name,
			 const char *inconsistent_state,
			 gboolean show_original)
{
	GtkLabel *title_label;
	GtkWidget *value;

	title_label = attach_title_field (grid, title);
	value = attach_value_field (window, grid, GTK_WIDGET (title_label),
				    file_attribute_name,
				    inconsistent_state,
				    show_original); 
	gtk_label_set_mnemonic_widget (title_label, value);
}

static void
append_title_and_ellipsizing_value (NautilusPropertiesWindow *window,
				    GtkGrid *grid,
				    const char *title,
				    const char *file_attribute_name,
				    const char *inconsistent_state,
				    gboolean show_original)
{
	GtkLabel *title_label;
	GtkWidget *value;

	title_label = attach_title_field (grid, title);
	value = attach_ellipsizing_value_field (window, grid,
						GTK_WIDGET (title_label),
						file_attribute_name,
						inconsistent_state,
						show_original);
	gtk_label_set_mnemonic_widget (title_label, value);
}

static void
append_directory_contents_fields (NautilusPropertiesWindow *window,
				  GtkGrid *grid)
{
	GtkLabel *title_field, *value_field;

	title_field = attach_title_field (grid, "");
	window->details->directory_contents_title_field = title_field;
	gtk_label_set_line_wrap (title_field, TRUE);

	value_field = attach_directory_contents_value_field 
		(window, grid, GTK_WIDGET (title_field));

	gtk_label_set_mnemonic_widget (title_field, GTK_WIDGET(value_field));
}

static GtkWidget *
create_page_with_hbox (GtkNotebook *notebook,
		       const char *title,
		       const char *help_uri)
{
	GtkWidget *hbox;

	g_assert (GTK_IS_NOTEBOOK (notebook));
	g_assert (title != NULL);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
	gtk_box_set_spacing (GTK_BOX (hbox), 12);
	gtk_notebook_append_page (notebook, hbox, gtk_label_new (title));
	g_object_set_data_full (G_OBJECT (hbox), "help-uri", g_strdup (help_uri), g_free);

	return hbox;
}

static GtkWidget *
create_page_with_vbox (GtkNotebook *notebook,
		       const char *title,
		       const char *help_uri)
{
	GtkWidget *vbox;

	g_assert (GTK_IS_NOTEBOOK (notebook));
	g_assert (title != NULL);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	gtk_notebook_append_page (notebook, vbox, gtk_label_new (title));
	g_object_set_data_full (G_OBJECT (vbox), "help-uri", g_strdup (help_uri), g_free);

	return vbox;
}		       

static GtkWidget *
append_blank_row (GtkGrid *grid)
{
	return GTK_WIDGET (attach_title_field (grid, ""));
}

static void
append_blank_slim_row (GtkGrid *grid)
{
	GtkWidget *w;
	PangoAttribute *attribute;
	PangoAttrList *attr_list;

	attr_list = pango_attr_list_new ();
	attribute = pango_attr_scale_new (0.30);
	pango_attr_list_insert (attr_list, attribute);

	w = gtk_label_new (NULL);
	gtk_label_set_attributes (GTK_LABEL (w), attr_list);
	gtk_widget_show (w);

	pango_attr_list_unref (attr_list);

	gtk_container_add (GTK_CONTAINER (grid), w);
}

static GtkWidget *
create_grid_with_standard_properties (void)
{
	GtkWidget *grid;

	grid = gtk_grid_new ();
	gtk_container_set_border_width (GTK_CONTAINER (grid), 6);
	gtk_grid_set_row_spacing (GTK_GRID (grid), ROW_PAD);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);	
	gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (grid);

	return grid;
}

static gboolean
is_merged_trash_directory (NautilusFile *file) 
{
	char *file_uri;
	gboolean result;

	file_uri = nautilus_file_get_uri (file);
	result = strcmp (file_uri, "trash:///") == 0;
	g_free (file_uri);

	return result;
}

static gboolean
is_computer_directory (NautilusFile *file)
{
	char *file_uri;
	gboolean result;
	
	file_uri = nautilus_file_get_uri (file);
	result = strcmp (file_uri, "computer:///") == 0;
	g_free (file_uri);
	
	return result;
}

static gboolean
is_network_directory (NautilusFile *file)
{
	char *file_uri;
	gboolean result;
	
	file_uri = nautilus_file_get_uri (file);
	result = strcmp (file_uri, "network:///") == 0;
	g_free (file_uri);
	
	return result;
}

static gboolean
is_burn_directory (NautilusFile *file)
{
	char *file_uri;
	gboolean result;
	
	file_uri = nautilus_file_get_uri (file);
	result = strcmp (file_uri, "burn:///") == 0;
	g_free (file_uri);
	
	return result;
}

static gboolean
should_show_custom_icon_buttons (NautilusPropertiesWindow *window) 
{
	if (is_multi_file_window (window)) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
should_show_file_type (NautilusPropertiesWindow *window) 
{
	if (!is_multi_file_window (window) 
	    && (is_merged_trash_directory (get_target_file (window)) ||
		is_computer_directory (get_target_file (window)) ||
		is_network_directory (get_target_file (window)) ||
		is_burn_directory (get_target_file (window)))) {
		return FALSE;
	}


	return TRUE;
}

static gboolean
should_show_location_info (NautilusPropertiesWindow *window) 
{
	if (!is_multi_file_window (window) 
	    && (is_merged_trash_directory (get_target_file (window)) ||
		is_computer_directory (get_target_file (window)) ||
		is_network_directory (get_target_file (window)) ||
		is_burn_directory (get_target_file (window)))) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
should_show_accessed_date (NautilusPropertiesWindow *window) 
{
	/* Accessed date for directory seems useless. If we some
	 * day decide that it is useful, we should separately
	 * consider whether it's useful for "trash:".
	 */
	if (file_list_all_directories (window->details->target_files)) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
should_show_link_target (NautilusPropertiesWindow *window)
{
	if (!is_multi_file_window (window)
	    && nautilus_file_is_symbolic_link (get_target_file (window))) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
should_show_free_space (NautilusPropertiesWindow *window)
{

	if (!is_multi_file_window (window)
	    && (is_merged_trash_directory (get_target_file (window)) ||
		is_computer_directory (get_target_file (window)) ||
		is_network_directory (get_target_file (window)) ||
		is_burn_directory (get_target_file (window)))) {
		return FALSE;
	}

	if (file_list_all_directories (window->details->target_files)) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
should_show_volume_usage (NautilusPropertiesWindow *window)
{
	NautilusFile 		*file;
	gboolean 		success = FALSE;
	
	if (is_multi_file_window (window)) {
		return FALSE;
	}

	file = get_original_file (window);

	if (file == NULL) {
		return FALSE;
	}

	if (nautilus_file_can_unmount (file)) {
		return TRUE;
	}

#ifdef TODO_GIO
	/* Look at is_mountpoint for activation uri */
#endif
	return success;
}

static void
paint_used_legend (GtkWidget *widget,
		   cairo_t *cr,
		   gpointer data)
{
	NautilusPropertiesWindow *window;
	gint width, height;
	GtkAllocation allocation;

	gtk_widget_get_allocation (widget, &allocation);
	
  	width  = allocation.width;
  	height = allocation.height;
  	
	window = NAUTILUS_PROPERTIES_WINDOW (data);

	cairo_rectangle  (cr,
			  2,
			  2,
			  width - 4,
			  height - 4);

	gdk_cairo_set_source_rgba (cr, &window->details->used_color);
	cairo_fill_preserve (cr);

	gdk_cairo_set_source_rgba (cr, &window->details->used_stroke_color);
	cairo_stroke (cr);
}

static void
paint_free_legend (GtkWidget *widget,
		   cairo_t *cr, gpointer data)
{
	NautilusPropertiesWindow *window;
	gint width, height;
	GtkAllocation allocation;

	window = NAUTILUS_PROPERTIES_WINDOW (data);
	gtk_widget_get_allocation (widget, &allocation);
	
  	width  = allocation.width;
  	height = allocation.height;
  
	cairo_rectangle (cr,
			 2,
			 2,
			 width - 4,
			 height - 4);

	gdk_cairo_set_source_rgba (cr, &window->details->free_color);
	cairo_fill_preserve(cr);

	gdk_cairo_set_source_rgba (cr, &window->details->free_stroke_color);
	cairo_stroke (cr);
}

static void
paint_pie_chart (GtkWidget *widget,
		 cairo_t *cr,
		 gpointer data)
{
  	
  	NautilusPropertiesWindow *window;
	gint width, height;
	double free, used;
	double angle1, angle2, split, xc, yc, radius;
	GtkAllocation allocation;
	GtkStyleContext *notebook_ctx;
	GdkRGBA bg_color;

	window = NAUTILUS_PROPERTIES_WINDOW (data);
	gtk_widget_get_allocation (widget, &allocation);

	width  = allocation.width;
  	height = allocation.height;

	notebook_ctx = gtk_widget_get_style_context (GTK_WIDGET (window->details->notebook));
	gtk_style_context_get_background_color (notebook_ctx,
						gtk_widget_get_state_flags (GTK_WIDGET (window->details->notebook)),
						&bg_color);

	cairo_save (cr);
	gdk_cairo_set_source_rgba (cr, &bg_color);
	cairo_paint (cr);
	cairo_restore (cr);

	free = (double)window->details->volume_free / (double)window->details->volume_capacity;
	used =  1.0 - free;

	angle1 = free * 2 * G_PI;
	angle2 = used * 2 * G_PI;
	split = (2 * G_PI - angle1) * .5;
	xc = width / 2;
	yc = height / 2;

	if (width < height) {
		radius = width / 2 - 8;
	} else {
		radius = height / 2 - 8;
	}
	
	if (angle1 != 2 * G_PI && angle1 != 0) {
		angle1 = angle1 + split;
	}
		
	if (angle2 != 2 * G_PI && angle2 != 0) {
		angle2 = angle2 - split;
	}
	
	if (used > 0) {
		if (free != 0) {
			cairo_move_to (cr,xc,yc);
		}
		
		cairo_arc (cr, xc, yc, radius, angle1, angle2);
		
		if (free != 0) {
			cairo_line_to (cr,xc,yc);
		}
		
		gdk_cairo_set_source_rgba (cr, &window->details->used_color);
		cairo_fill_preserve (cr);
		
		gdk_cairo_set_source_rgba (cr, &window->details->used_stroke_color);
		cairo_stroke (cr);
	}
	
	if (free > 0) {
		if (used != 0) {
			cairo_move_to (cr,xc,yc);
		}
	
		cairo_arc_negative (cr, xc, yc, radius, angle1, angle2);
	
		if (used != 0) {
			cairo_line_to (cr,xc,yc);
		}

		gdk_cairo_set_source_rgba (cr, &window->details->free_color);
		cairo_fill_preserve(cr);

		gdk_cairo_set_source_rgba (cr, &window->details->free_stroke_color);
		cairo_stroke (cr);
	}
}


/* Copied from gtk/gtkstyle.c */

static void
rgb_to_hls (gdouble *r,
            gdouble *g,
            gdouble *b)
{
  gdouble min;
  gdouble max;
  gdouble red;
  gdouble green;
  gdouble blue;
  gdouble h, l, s;
  gdouble delta;
  
  red = *r;
  green = *g;
  blue = *b;
  
  if (red > green)
    {
      if (red > blue)
        max = red;
      else
        max = blue;
      
      if (green < blue)
        min = green;
      else
        min = blue;
    }
  else
    {
      if (green > blue)
        max = green;
      else
        max = blue;
      
      if (red < blue)
        min = red;
      else
        min = blue;
    }
  
  l = (max + min) / 2;
  s = 0;
  h = 0;
  
  if (max != min)
    {
      if (l <= 0.5)
        s = (max - min) / (max + min);
      else
        s = (max - min) / (2 - max - min);
      
      delta = max -min;
      if (red == max)
        h = (green - blue) / delta;
      else if (green == max)
        h = 2 + (blue - red) / delta;
      else if (blue == max)
        h = 4 + (red - green) / delta;
      
      h *= 60;
      if (h < 0.0)
        h += 360;
    }
  
  *r = h;
  *g = l;
  *b = s;
}

static void
hls_to_rgb (gdouble *h,
            gdouble *l,
            gdouble *s)
{
  gdouble hue;
  gdouble lightness;
  gdouble saturation;
  gdouble m1, m2;
  gdouble r, g, b;
  
  lightness = *l;
  saturation = *s;
  
  if (lightness <= 0.5)
    m2 = lightness * (1 + saturation);
  else
    m2 = lightness + saturation - lightness * saturation;
  m1 = 2 * lightness - m2;
  
  if (saturation == 0)
    {
      *h = lightness;
      *l = lightness;
      *s = lightness;
    }
  else
    {
      hue = *h + 120;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;
      
      if (hue < 60)
        r = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        r = m2;
      else if (hue < 240)
        r = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        r = m1;
      
      hue = *h;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;
      
      if (hue < 60)
        g = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        g = m2;
      else if (hue < 240)
        g = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        g = m1;
      
      hue = *h - 120;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;
      
      if (hue < 60)
        b = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        b = m2;
      else if (hue < 240)
        b = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        b = m1;
      
      *h = r;
      *l = g;
      *s = b;
    }
}
static void
_pie_style_shade (GdkRGBA *a,
                  GdkRGBA *b,
                  gdouble   k)
{
  gdouble red;
  gdouble green;
  gdouble blue;
  
  red = a->red;
  green = a->green;
  blue = a->blue;
  
  rgb_to_hls (&red, &green, &blue);

  green *= k;
  if (green > 1.0)
    green = 1.0;
  else if (green < 0.0)
    green = 0.0;
  
  blue *= k;
  if (blue > 1.0)
    blue = 1.0;
  else if (blue < 0.0)
    blue = 0.0;
  
  hls_to_rgb (&red, &green, &blue);
  
  b->red = red;
  b->green = green;
  b->blue = blue;
  b->alpha = a->alpha;
}


static GtkWidget* 
create_pie_widget (NautilusPropertiesWindow *window)
{
	NautilusFile		*file;
	GtkGrid                 *grid;
	GtkStyleContext		*style;
	GtkWidget 		*pie_canvas;
	GtkWidget 		*used_canvas;
	GtkWidget 		*used_label;
	GtkWidget 		*free_canvas;
	GtkWidget 		*free_label;
	GtkWidget 		*capacity_label;
	GtkWidget 		*fstype_label;
	gchar			*capacity;
	gchar 			*used;
	gchar 			*free;
	const char		*fs_type;
	gchar			*uri;
	GFile *location;
	GFileInfo *info;
	
	capacity = g_format_size (window->details->volume_capacity);
	free 	 = g_format_size (window->details->volume_free);
	used 	 = g_format_size (window->details->volume_capacity - window->details->volume_free);	
	
	file = get_original_file (window);
	
	uri = nautilus_file_get_activation_uri (file);
	
	grid = GTK_GRID (gtk_grid_new ());
	gtk_container_set_border_width (GTK_CONTAINER (grid), 5);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 5);
	style = gtk_widget_get_style_context (GTK_WIDGET (grid));

	if (!gtk_style_context_lookup_color (style, "chart_rgba_1", &window->details->used_color)) {
		window->details->used_color.red = USED_FILL_R;
		window->details->used_color.green = USED_FILL_G;
		window->details->used_color.blue = USED_FILL_B;
		window->details->used_color.alpha = 1;
	}

	if (!gtk_style_context_lookup_color (style, "chart_rgba_2", &window->details->free_color)) {
		window->details->free_color.red = FREE_FILL_R;
		window->details->free_color.green = FREE_FILL_G;
		window->details->free_color.blue = FREE_FILL_B;
		window->details->free_color.alpha = 1;
	}

	_pie_style_shade (&window->details->used_color, &window->details->used_stroke_color, 0.7);
	_pie_style_shade (&window->details->free_color, &window->details->free_stroke_color, 0.7);
	
	pie_canvas = gtk_drawing_area_new ();
	gtk_widget_set_size_request (pie_canvas, 200, 200);

	used_canvas = gtk_drawing_area_new ();
	gtk_widget_set_valign (used_canvas, GTK_ALIGN_CENTER);
	gtk_widget_set_halign (used_canvas, GTK_ALIGN_CENTER);
	gtk_widget_set_size_request (used_canvas, 20, 20);
	/* Translators: "used" refers to the capacity of the filesystem */
	used_label = gtk_label_new (g_strconcat (used, " ", _("used"), NULL));

	free_canvas = gtk_drawing_area_new ();
	gtk_widget_set_valign (free_canvas, GTK_ALIGN_CENTER);
	gtk_widget_set_halign (free_canvas, GTK_ALIGN_CENTER);
	gtk_widget_set_size_request (free_canvas, 20, 20);
	/* Translators: "free" refers to the capacity of the filesystem */
	free_label = gtk_label_new (g_strconcat (free, " ", _("free"), NULL));  

	capacity_label = gtk_label_new (g_strconcat (_("Total capacity:"), " ", capacity, NULL));
	fstype_label = gtk_label_new (NULL);

	location = g_file_new_for_uri (uri);
	info = g_file_query_filesystem_info (location, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE,
					     NULL, NULL);
	if (info) {
		fs_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE);
		if (fs_type != NULL) {
			gtk_label_set_text (GTK_LABEL (fstype_label), g_strconcat (_("Filesystem type:"), " ", fs_type, NULL));
		}
		
		g_object_unref (info);
	}
	g_object_unref (location);
	
	g_free (uri);
	g_free (capacity);
	g_free (used);
	g_free (free);

	gtk_container_add_with_properties (GTK_CONTAINER (grid), pie_canvas,
					   "height", 4,
					   NULL);
	gtk_grid_attach_next_to (grid, used_canvas, pie_canvas,
				 GTK_POS_RIGHT, 1, 1);
	gtk_grid_attach_next_to (grid, used_label, used_canvas,
				 GTK_POS_RIGHT, 1, 1);

	gtk_grid_attach_next_to (grid, free_canvas, used_canvas,
				 GTK_POS_BOTTOM, 1, 1);
	gtk_grid_attach_next_to (grid, free_label, free_canvas,
				 GTK_POS_RIGHT, 1, 1);

	gtk_grid_attach_next_to (grid, capacity_label, free_canvas,
				 GTK_POS_BOTTOM, 2, 1);
	gtk_grid_attach_next_to (grid, fstype_label, capacity_label,
				 GTK_POS_BOTTOM, 2, 1);
	
	g_signal_connect (pie_canvas, "draw",
			  G_CALLBACK (paint_pie_chart), window);
	g_signal_connect (used_canvas, "draw",
			  G_CALLBACK (paint_used_legend), window);
	g_signal_connect (free_canvas, "draw",
			  G_CALLBACK (paint_free_legend), window);
	        
	return GTK_WIDGET (grid);
}

static GtkWidget*
create_volume_usage_widget (NautilusPropertiesWindow *window)
{
	GtkWidget *piewidget;
	gchar *uri;
	NautilusFile *file;
	GFile *location;
	GFileInfo *info;
	
	file = get_original_file (window);
	
	uri = nautilus_file_get_activation_uri (file);

	location = g_file_new_for_uri (uri);
	info = g_file_query_filesystem_info (location, "filesystem::*", NULL, NULL);

	if (info) {
		window->details->volume_capacity = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
		window->details->volume_free = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);

		g_object_unref (info);
	} else {
		window->details->volume_capacity = 0;		
		window->details->volume_free = 0;		
	}
	
	g_object_unref (location);
	
	piewidget = create_pie_widget (window);
	                   
        gtk_widget_show_all (piewidget);            
        
	return piewidget;
}

static void
create_basic_page (NautilusPropertiesWindow *window)
{
	GtkGrid *grid;
	GtkWidget *icon_aligner;
	GtkWidget *icon_pixmap_widget;
	GtkWidget *volume_usage;
	GtkWidget *hbox, *vbox;

	hbox = create_page_with_hbox (window->details->notebook, _("Basic"),
				      "help:gnome-help/nautilus-file-properties-basic");
	
	/* Icon pixmap */

	icon_pixmap_widget = create_image_widget (
		window, should_show_custom_icon_buttons (window));
	gtk_widget_show (icon_pixmap_widget);

	icon_aligner = gtk_alignment_new (1, 0, 0, 0);
	gtk_widget_show (icon_aligner);
	
	gtk_container_add (GTK_CONTAINER (icon_aligner), icon_pixmap_widget);
	gtk_box_pack_start (GTK_BOX (hbox), icon_aligner, FALSE, FALSE, 0);

	window->details->icon_chooser = NULL;

	/* Grid */

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (hbox), vbox);

	grid = GTK_GRID (create_grid_with_standard_properties ());
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (grid), FALSE, FALSE, 0);
	window->details->basic_grid = grid;

	/* Name label.  The text will be determined in update_name_field */
	window->details->name_label = attach_title_field (grid, NULL);

	/* Name field */
	window->details->name_field = NULL;
	update_name_field (window);

	/* Start with name field selected, if it's an entry. */
	if (NAUTILUS_IS_ENTRY (window->details->name_field)) {
		nautilus_entry_select_all (NAUTILUS_ENTRY (window->details->name_field));
		gtk_widget_grab_focus (GTK_WIDGET (window->details->name_field));
	}

	if (nautilus_desktop_item_properties_should_show (window->details->target_files)) {
		GtkSizeGroup *label_size_group;
		GtkWidget *box;

		label_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
		gtk_size_group_add_widget (label_size_group,
					   GTK_WIDGET (window->details->name_label));
		box = nautilus_desktop_item_properties_make_box (label_size_group,
								 window->details->target_files);

		gtk_grid_attach_next_to (window->details->basic_grid, box, 
					 GTK_WIDGET (window->details->name_label),
					 GTK_POS_BOTTOM, 2, 1);
	}

	if (should_show_file_type (window)) {
		append_title_and_ellipsizing_value (window, grid,
						    _("Type:"), 
						    "type",
						    INCONSISTENT_STATE_STRING,
						    FALSE);
	}

	if (should_show_link_target (window)) {
		append_title_and_ellipsizing_value (window, grid, 
						    _("Link target:"), 
						    "link_target",
						    INCONSISTENT_STATE_STRING,
						    FALSE);
	}

	if (is_multi_file_window (window) ||
	    nautilus_file_is_directory (get_target_file (window))) {
		append_directory_contents_fields (window, grid);
	} else {
		append_title_value_pair (window, grid, _("Size:"), 
					 "size_detail",
					 INCONSISTENT_STATE_STRING,
					 FALSE);
	}

	append_blank_row (grid);

	if (should_show_location_info (window)) {
		append_title_and_ellipsizing_value (window, grid, _("Location:"), 
						    "where",
						    INCONSISTENT_STATE_STRING,
						    TRUE);
		
		append_title_and_ellipsizing_value (window, grid, 
						    _("Volume:"), 
						    "volume",
						    INCONSISTENT_STATE_STRING,
						    FALSE);
	}

	if (should_show_accessed_date (window)) {
		append_blank_row (grid);

		append_title_value_pair (window, grid, _("Accessed:"), 
					 "date_accessed",
					 INCONSISTENT_STATE_STRING,
					 FALSE);
		append_title_value_pair (window, grid, _("Modified:"), 
					 "date_modified",
					 INCONSISTENT_STATE_STRING,
					 FALSE);
	}

	if (should_show_free_space (window)) {
		append_blank_row (grid);

		append_title_value_pair (window, grid, _("Free space:"), 
					 "free_space",
					 INCONSISTENT_STATE_STRING,
					 FALSE);
	}

	if (should_show_volume_usage (window)) {
		volume_usage = create_volume_usage_widget (window);
		gtk_container_add_with_properties (GTK_CONTAINER (grid), volume_usage,
						   "width", 2,
						   NULL);
	}
}

static gboolean 
files_has_directory (NautilusPropertiesWindow *window)
{
	GList *l;

	for (l = window->details->target_files; l != NULL; l = l->next) {
		NautilusFile *file;
		file = NAUTILUS_FILE (l->data);
		if (nautilus_file_is_directory (file)) {
			return TRUE;
		}
		
	}

	return FALSE;
}

static gboolean 
files_has_changable_permissions_directory (NautilusPropertiesWindow *window)
{
	GList *l;

	for (l = window->details->target_files; l != NULL; l = l->next) {
		NautilusFile *file;
		file = NAUTILUS_FILE (l->data);
		if (nautilus_file_is_directory (file) &&
		    nautilus_file_can_get_permissions (file) &&
		    nautilus_file_can_set_permissions (file)) {
			return TRUE;
		}
		
	}

	return FALSE;
}


static gboolean 
files_has_file (NautilusPropertiesWindow *window)
{
	GList *l;

	for (l = window->details->target_files; l != NULL; l = l->next) {
		NautilusFile *file;
		file = NAUTILUS_FILE (l->data);
		if (!nautilus_file_is_directory (file)) {
			return TRUE;
		}
		
	}

	return FALSE;
}

static void
start_long_operation (NautilusPropertiesWindow *window)
{
	if (window->details->long_operation_underway == 0) {
		/* start long operation */
		GdkCursor * cursor;
		
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), cursor);
		g_object_unref (cursor);
	}
	window->details->long_operation_underway ++;
}

static void
end_long_operation (NautilusPropertiesWindow *window)
{
	if (gtk_widget_get_window (GTK_WIDGET (window)) != NULL &&
	    window->details->long_operation_underway == 1) {
		/* finished !! */
		gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (window)), NULL);
	}
	window->details->long_operation_underway--;
}

static void
permission_change_callback (NautilusFile *file,
			    GFile *res_loc,
			    GError *error,
			    gpointer callback_data)
{
	NautilusPropertiesWindow *window;
	g_assert (callback_data != NULL);

	window = NAUTILUS_PROPERTIES_WINDOW (callback_data);
	end_long_operation (window);
	
	/* Report the error if it's an error. */
	nautilus_report_error_setting_permissions (file, error, NULL);

	g_object_unref (window);
}

static void
update_permissions (NautilusPropertiesWindow *window,
		    guint32 vfs_new_perm,
		    guint32 vfs_mask,
		    gboolean is_folder,
		    gboolean apply_to_both_folder_and_dir,
		    gboolean use_original)
{
	GList *l;
	
	for (l = window->details->target_files; l != NULL; l = l->next) {
		NautilusFile *file;
		guint32 permissions;

		file = NAUTILUS_FILE (l->data);

		if (!nautilus_file_can_get_permissions (file)) {
			continue;
		}
	
		if (!apply_to_both_folder_and_dir &&
		    ((nautilus_file_is_directory (file) && !is_folder) ||
		     (!nautilus_file_is_directory (file) && is_folder))) {
			continue;
		}

		permissions = nautilus_file_get_permissions (file);
		if (use_original) {
			gpointer ptr;
			if (g_hash_table_lookup_extended (window->details->initial_permissions,
							  file, NULL, &ptr)) {
				permissions = (permissions & ~vfs_mask) | (GPOINTER_TO_INT (ptr) & vfs_mask);
			}
		} else {
			permissions = (permissions & ~vfs_mask) | vfs_new_perm;
		}

		start_long_operation (window);
		g_object_ref (window);
		nautilus_file_set_permissions
			(file, permissions,
			 permission_change_callback,
			 window);
	}	
}

static gboolean
initial_permission_state_consistent (NautilusPropertiesWindow *window,
				     guint32 mask,
				     gboolean is_folder,
				     gboolean both_folder_and_dir)
{
	GList *l;
	gboolean first;
	guint32 first_permissions;

	first = TRUE;
	first_permissions = 0;
	for (l = window->details->target_files; l != NULL; l = l->next) {
		NautilusFile *file;
		guint32 permissions;

		file = l->data;
		
		if (!both_folder_and_dir &&
		    ((nautilus_file_is_directory (file) && !is_folder) ||
		     (!nautilus_file_is_directory (file) && is_folder))) {
			continue;
		}
		
		permissions = GPOINTER_TO_INT (g_hash_table_lookup (window->details->initial_permissions,
								    file));

		if (first) {
			if ((permissions & mask) != mask &&
			    (permissions & mask) != 0) {
				/* Not fully on or off -> inconsistent */
				return FALSE;
			}
				
			first_permissions = permissions;
			first = FALSE;
				
		} else if ((permissions & mask) != first_permissions) {
			/* Not same permissions as first -> inconsistent */
			return FALSE;
		}
	}
	return TRUE;
}

static void
permission_button_toggled (GtkToggleButton *button, 
			   NautilusPropertiesWindow *window)
{
	gboolean is_folder, is_special;
	guint32 permission_mask;
	gboolean inconsistent;
	gboolean on;
	
	permission_mask = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
							      "permission"));
	is_folder = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
							"is-folder"));
	is_special = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
							"is-special"));

	if (gtk_toggle_button_get_active (button)
	    && !gtk_toggle_button_get_inconsistent (button)) {
		/* Go to the initial state unless the initial state was 
		   consistent, or we support recursive apply */
		inconsistent = TRUE;
		on = TRUE;

		if (!window->details->has_recursive_apply &&
		    initial_permission_state_consistent (window, permission_mask, is_folder, is_special)) {
			inconsistent = FALSE;
			on = TRUE;
		}
	} else if (gtk_toggle_button_get_inconsistent (button)
		   && !gtk_toggle_button_get_active (button)) {
		inconsistent = FALSE;
		on = TRUE;
	} else {
		inconsistent = FALSE;
		on = FALSE;
	}
	
	g_signal_handlers_block_by_func (G_OBJECT (button), 
					 G_CALLBACK (permission_button_toggled),
					 window);

	gtk_toggle_button_set_active (button, on);
	gtk_toggle_button_set_inconsistent (button, inconsistent);

	g_signal_handlers_unblock_by_func (G_OBJECT (button), 
					   G_CALLBACK (permission_button_toggled),
					   window);

	update_permissions (window,
			    on?permission_mask:0,
			    permission_mask,
			    is_folder,
			    is_special,
			    inconsistent);
}

static void
permission_button_update (NautilusPropertiesWindow *window,
			  GtkToggleButton *button)
{
	GList *l;
	gboolean all_set;
	gboolean all_unset;
	gboolean all_cannot_set;
	gboolean is_folder, is_special;
	gboolean no_match;
	gboolean sensitive;
	guint32 button_permission;

	if (gtk_toggle_button_get_inconsistent (button) &&
	    window->details->has_recursive_apply) {
		/* Never change from an inconsistent state if we have dirs, even
		 * if the current state is now consistent, because its a useful
		 * state for recursive apply.
		 */
		return;
	}
	
	button_permission = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
								"permission"));
	is_folder = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
							"is-folder"));
	is_special = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
							 "is-special"));
	
	all_set = TRUE;
	all_unset = TRUE;
	all_cannot_set = TRUE;
	no_match = TRUE;
	for (l = window->details->target_files; l != NULL; l = l->next) {
		NautilusFile *file;
		guint32 file_permissions;

		file = NAUTILUS_FILE (l->data);

		if (!nautilus_file_can_get_permissions (file)) {
			continue;
		}

		if (!is_special &&
		    ((nautilus_file_is_directory (file) && !is_folder) ||
		     (!nautilus_file_is_directory (file) && is_folder))) {
			continue;
		}

		no_match = FALSE;
		
		file_permissions = nautilus_file_get_permissions (file);

		if ((file_permissions & button_permission) == button_permission) {
			all_unset = FALSE;
		} else if ((file_permissions & button_permission) == 0) {
			all_set = FALSE;
		} else {
			all_unset = FALSE;
			all_set = FALSE;
		}

		if (nautilus_file_can_set_permissions (file)) {
			all_cannot_set = FALSE;
		}
	}

	sensitive = !all_cannot_set;
	if (!is_folder) {
		/* Don't insitive files when we have recursive apply */
		sensitive |= window->details->has_recursive_apply;
	}

	
	g_signal_handlers_block_by_func (G_OBJECT (button), 
					 G_CALLBACK (permission_button_toggled),
					 window);

	gtk_toggle_button_set_active (button, !all_unset);
	/* if actually inconsistent, or default value for file buttons
	   if no files are selected. (useful for recursive apply) */
	gtk_toggle_button_set_inconsistent (button,
					    (!all_unset && !all_set) ||
					    (!is_folder && no_match));
	gtk_widget_set_sensitive (GTK_WIDGET (button), sensitive);

	g_signal_handlers_unblock_by_func (G_OBJECT (button), 
					   G_CALLBACK (permission_button_toggled),
					   window);
}

static void
set_up_permissions_checkbox (NautilusPropertiesWindow *window,
			     GtkWidget *check_button, 
			     guint32 permission,
			     gboolean is_folder)
{
	/* Load up the check_button with data we'll need when updating its state. */
        g_object_set_data (G_OBJECT (check_button), "permission", 
			   GINT_TO_POINTER (permission));
        g_object_set_data (G_OBJECT (check_button), "properties_window", 
			   window);
	g_object_set_data (G_OBJECT (check_button), "is-folder",
			   GINT_TO_POINTER (is_folder));
	
	window->details->permission_buttons = 
		g_list_prepend (window->details->permission_buttons,
				check_button);

	g_signal_connect_object (check_button, "toggled",
				 G_CALLBACK (permission_button_toggled),
				 window,
				 0);
}

static GtkWidget *
add_permissions_checkbox_with_label (NautilusPropertiesWindow *window,
				     GtkGrid *grid,
				     GtkWidget *sibling,
				     const char *label,
				     guint32 permission_to_check,
				     GtkLabel *label_for,
				     gboolean is_folder)
{
	GtkWidget *check_button;
	gboolean a11y_enabled;
	
	check_button = gtk_check_button_new_with_mnemonic (label);
	gtk_widget_show (check_button);

	if (sibling) {
		gtk_grid_attach_next_to (grid, check_button, sibling,
					 GTK_POS_RIGHT, 1, 1);
	} else {
		gtk_container_add (GTK_CONTAINER (grid), check_button);
	}

	set_up_permissions_checkbox (window, 
				     check_button, 
				     permission_to_check,
				     is_folder);

	a11y_enabled = GTK_IS_ACCESSIBLE (gtk_widget_get_accessible (check_button));
	if (a11y_enabled && label_for != NULL) {
		eel_accessibility_set_up_label_widget_relation (GTK_WIDGET (label_for),
								check_button);
	}

	return check_button;
}

static GtkWidget *
add_permissions_checkbox (NautilusPropertiesWindow *window,
			  GtkGrid *grid,
			  GtkWidget *sibling,
			  CheckboxType type,
			  guint32 permission_to_check,
			  GtkLabel *label_for,
			  gboolean is_folder)
{
	const gchar *label;

	if (type == PERMISSIONS_CHECKBOXES_READ) {
		label = _("_Read");
	} else if (type == PERMISSIONS_CHECKBOXES_WRITE) {
		label = _("_Write");
	} else {
		label = _("E_xecute");
	}

	return add_permissions_checkbox_with_label (window, grid, 
						    sibling,
						    label,
						    permission_to_check,
						    label_for,
						    is_folder);
}

enum {
	UNIX_PERM_SUID = S_ISUID,
	UNIX_PERM_SGID = S_ISGID,	
	UNIX_PERM_STICKY = 01000,	/* S_ISVTX not defined on all systems */
	UNIX_PERM_USER_READ = S_IRUSR,
	UNIX_PERM_USER_WRITE = S_IWUSR,
	UNIX_PERM_USER_EXEC = S_IXUSR,
	UNIX_PERM_USER_ALL = S_IRUSR | S_IWUSR | S_IXUSR,
	UNIX_PERM_GROUP_READ = S_IRGRP,
	UNIX_PERM_GROUP_WRITE = S_IWGRP,
	UNIX_PERM_GROUP_EXEC = S_IXGRP,
	UNIX_PERM_GROUP_ALL = S_IRGRP | S_IWGRP | S_IXGRP,
	UNIX_PERM_OTHER_READ = S_IROTH,
	UNIX_PERM_OTHER_WRITE = S_IWOTH,
	UNIX_PERM_OTHER_EXEC = S_IXOTH,
	UNIX_PERM_OTHER_ALL = S_IROTH | S_IWOTH | S_IXOTH
};

typedef enum {
	PERMISSION_READ  = (1<<0),
	PERMISSION_WRITE = (1<<1),
	PERMISSION_EXEC  = (1<<2)
} PermissionValue;

typedef enum {
	PERMISSION_USER,
	PERMISSION_GROUP,
	PERMISSION_OTHER
} PermissionType;

static guint32 vfs_perms[3][3] = {
	{UNIX_PERM_USER_READ, UNIX_PERM_USER_WRITE, UNIX_PERM_USER_EXEC},
	{UNIX_PERM_GROUP_READ, UNIX_PERM_GROUP_WRITE, UNIX_PERM_GROUP_EXEC},
	{UNIX_PERM_OTHER_READ, UNIX_PERM_OTHER_WRITE, UNIX_PERM_OTHER_EXEC},
};

static guint32 
permission_to_vfs (PermissionType type, PermissionValue perm)
{
	guint32 vfs_perm;
	g_assert (type >= 0 && type < 3);

	vfs_perm = 0;
	if (perm & PERMISSION_READ) {
		vfs_perm |= vfs_perms[type][0];
	}
	if (perm & PERMISSION_WRITE) {
		vfs_perm |= vfs_perms[type][1];
	}
	if (perm & PERMISSION_EXEC) {
		vfs_perm |= vfs_perms[type][2];
	}
	
	return vfs_perm;
}


static PermissionValue
permission_from_vfs (PermissionType type, guint32 vfs_perm)
{
	PermissionValue perm;
	g_assert (type >= 0 && type < 3);

	perm = 0;
	if (vfs_perm & vfs_perms[type][0]) {
		perm |= PERMISSION_READ;
	}
	if (vfs_perm & vfs_perms[type][1]) {
		perm |= PERMISSION_WRITE;
	}
	if (vfs_perm & vfs_perms[type][2]) {
		perm |= PERMISSION_EXEC;
	}
	
	return perm;
}

static void
permission_combo_changed (GtkWidget *combo, NautilusPropertiesWindow *window)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean is_folder, use_original;
	PermissionType type;
	int new_perm, mask;
	guint32 vfs_new_perm, vfs_mask;

	is_folder = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo), "is-folder"));
	type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo), "permission-type"));

	if (is_folder) {
		mask = PERMISSION_READ|PERMISSION_WRITE|PERMISSION_EXEC;
	} else {
		mask = PERMISSION_READ|PERMISSION_WRITE;
	}

	vfs_mask = permission_to_vfs (type, mask);
	
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
	
	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo),  &iter)) {
		return;
	}
	gtk_tree_model_get (model, &iter, 1, &new_perm, 2, &use_original, -1);
	vfs_new_perm = permission_to_vfs (type, new_perm);

	update_permissions (window, vfs_new_perm, vfs_mask,
			    is_folder, FALSE, use_original);
}

static void
permission_combo_add_multiple_choice (GtkComboBox *combo, GtkTreeIter *iter)
{
	GtkTreeModel *model;
	GtkListStore *store;
	gboolean found;

	model = gtk_combo_box_get_model (combo);
	store = GTK_LIST_STORE (model);

	found = FALSE;
	gtk_tree_model_get_iter_first (model, iter);
	do {
		gboolean multi;
		gtk_tree_model_get (model, iter, 2, &multi, -1);
		
		if (multi) {
			found = TRUE;
			break;
		}
	} while (gtk_tree_model_iter_next (model, iter));
	
	if (!found) {
		gtk_list_store_append (store, iter);
		gtk_list_store_set (store, iter, 0, "---", 1, 0, 2, TRUE, -1);
	}
}

static void
permission_combo_update (NautilusPropertiesWindow *window,
			 GtkComboBox *combo)
{
	PermissionType type;
	PermissionValue perm, all_dir_perm, all_file_perm, all_perm;
	gboolean is_folder, no_files, no_dirs, all_file_same, all_dir_same, all_same;
	gboolean all_dir_cannot_set, all_file_cannot_set, sensitive;
	GtkTreeIter iter;
	int mask;
	GtkTreeModel *model;
	GtkListStore *store;
	GList *l;
	gboolean is_multi;

	model = gtk_combo_box_get_model (combo);
	
	is_folder = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo), "is-folder"));
	type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo), "permission-type"));

	is_multi = FALSE;
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo),  &iter)) {
		gtk_tree_model_get (model, &iter, 2, &is_multi, -1);
	}

	if (is_multi && window->details->has_recursive_apply) {
		/* Never change from an inconsistent state if we have dirs, even
		 * if the current state is now consistent, because its a useful
		 * state for recursive apply.
		 */
		return;
	}
	
	no_files = TRUE;
	no_dirs = TRUE;
	all_dir_same = TRUE;
	all_file_same = TRUE;
	all_dir_perm = 0;
	all_file_perm = 0;
	all_dir_cannot_set = TRUE;
	all_file_cannot_set = TRUE;
	
	for (l = window->details->target_files; l != NULL; l = l->next) {
		NautilusFile *file;
		guint32 file_permissions;

		file = NAUTILUS_FILE (l->data);

		if (!nautilus_file_can_get_permissions (file)) {
			continue;
		}

		if (nautilus_file_is_directory (file)) {
			mask = PERMISSION_READ|PERMISSION_WRITE|PERMISSION_EXEC;
		} else {
			mask = PERMISSION_READ|PERMISSION_WRITE;
		}
		
		file_permissions = nautilus_file_get_permissions (file);

		perm = permission_from_vfs (type, file_permissions) & mask;

		if (nautilus_file_is_directory (file)) {
			if (no_dirs) {
				all_dir_perm = perm;
				no_dirs = FALSE;
			} else if (perm != all_dir_perm) {
				all_dir_same = FALSE;
			}
			
			if (nautilus_file_can_set_permissions (file)) {
				all_dir_cannot_set = FALSE;
			}
		} else {
			if (no_files) {
				all_file_perm = perm;
				no_files = FALSE;
			} else if (perm != all_file_perm) {
				all_file_same = FALSE;
			}
			
			if (nautilus_file_can_set_permissions (file)) {
				all_file_cannot_set = FALSE;
			}
		}
	}

	if (is_folder) {
		all_same = all_dir_same;
		all_perm = all_dir_perm;
	} else {
		all_same = all_file_same && !no_files;
		all_perm = all_file_perm;
	}

	store = GTK_LIST_STORE (model);
	if (all_same) {
		gboolean found;

		found = FALSE;
		gtk_tree_model_get_iter_first (model, &iter);
		do {
			int current_perm;
			gtk_tree_model_get (model, &iter, 1, &current_perm, -1);

			if (current_perm == all_perm) {
				found = TRUE;
				break;
			}
		} while (gtk_tree_model_iter_next (model, &iter));

		if (!found) {
			GString *str;
			str = g_string_new ("");
			
			if (!(all_perm & PERMISSION_READ)) {
				/* translators: this gets concatenated to "no read",
				 * "no access", etc. (see following strings)
				 */
				g_string_append (str, _("no "));
			}
			if (is_folder) {
				g_string_append (str, _("list"));
			} else {
				g_string_append (str, _("read"));
			}
			
			g_string_append (str, ", ");
			
			if (!(all_perm & PERMISSION_WRITE)) {
				g_string_append (str, _("no "));
			}
			if (is_folder) {
				g_string_append (str, _("create/delete"));
			} else {
				g_string_append (str, _("write"));
			}

			if (is_folder) {
				g_string_append (str, ", ");

				if (!(all_perm & PERMISSION_EXEC)) {
					g_string_append (str, _("no "));
				}
				g_string_append (str, _("access"));
			}
			
			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter,
					    0, str->str,
					    1, all_perm, -1);
			
			g_string_free (str, TRUE);
		}
	} else {
		permission_combo_add_multiple_choice (combo, &iter);
	}

	g_signal_handlers_block_by_func (G_OBJECT (combo), 
					 G_CALLBACK (permission_combo_changed),
					 window);
	
	gtk_combo_box_set_active_iter (combo, &iter);

	/* Also enable if no files found (for recursive
	   file changes when only selecting folders) */
	if (is_folder) {
		sensitive = !all_dir_cannot_set;
	} else {
		sensitive = !all_file_cannot_set ||
			window->details->has_recursive_apply;
	}
	gtk_widget_set_sensitive (GTK_WIDGET (combo), sensitive);

	g_signal_handlers_unblock_by_func (G_OBJECT (combo), 
					   G_CALLBACK (permission_combo_changed),
					   window);

}

static void
add_permissions_combo_box (NautilusPropertiesWindow *window, GtkGrid *grid,
			   PermissionType type, gboolean is_folder,
			   gboolean short_label)
{
	GtkWidget *combo;
	GtkLabel *label;
	GtkListStore *store;
	GtkCellRenderer *cell;
	GtkTreeIter iter;

	if (short_label) {
		label = attach_title_field (grid, _("Access:"));
	} else if (is_folder) {
		label = attach_title_field (grid, _("Folder access:"));
	} else {
		label = attach_title_field (grid, _("File access:"));
	}
	
	store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_INT, G_TYPE_BOOLEAN);
	combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));

	g_object_set_data (G_OBJECT (combo), "is-folder", GINT_TO_POINTER (is_folder));
	g_object_set_data (G_OBJECT (combo), "permission-type", GINT_TO_POINTER (type));

	if (is_folder) {
		if (type != PERMISSION_USER) {
			gtk_list_store_append (store, &iter);
			/* Translators: this is referred to the permissions
			 * the user has in a directory.
			 */
			gtk_list_store_set (store, &iter, 0, _("None"), 1, 0, -1);
		}
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, _("List files only"), 1, PERMISSION_READ, -1);
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, _("Access files"), 1, PERMISSION_READ|PERMISSION_EXEC, -1);
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, _("Create and delete files"), 1, PERMISSION_READ|PERMISSION_EXEC|PERMISSION_WRITE, -1);
	} else {
		if (type != PERMISSION_USER) {
			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter, 0, _("None"), 1, 0, -1);
		}
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, _("Read-only"), 1, PERMISSION_READ, -1);
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, _("Read and write"), 1, PERMISSION_READ|PERMISSION_WRITE, -1);
	}
	if (window->details->has_recursive_apply) {
		permission_combo_add_multiple_choice (GTK_COMBO_BOX (combo), &iter);
	}

	g_object_unref (store);

	window->details->permission_combos = 
		g_list_prepend (window->details->permission_combos,
				combo);

	g_signal_connect (combo, "changed", G_CALLBACK (permission_combo_changed), window);
	
	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell,
					"text", 0,
					NULL);
	
	gtk_label_set_mnemonic_widget (label, combo);
	gtk_widget_show (combo);

	gtk_grid_attach_next_to (grid, combo, GTK_WIDGET (label),
				 GTK_POS_RIGHT, 1, 1);
}


static GtkWidget *
append_special_execution_checkbox (NautilusPropertiesWindow *window,
				   GtkGrid *grid,
				   GtkWidget *sibling,
				   const char *label_text,
				   guint32 permission_to_check)
{
	GtkWidget *check_button;

	check_button = gtk_check_button_new_with_mnemonic (label_text);
	gtk_widget_show (check_button);

	if (sibling != NULL) {
		gtk_grid_attach_next_to (grid, check_button, sibling,
					 GTK_POS_RIGHT, 1, 1);
	} else {
		gtk_container_add_with_properties (GTK_CONTAINER (grid), check_button,
						   "left-attach", 1,
						   NULL);
	}

	set_up_permissions_checkbox (window, 
				     check_button, 
				     permission_to_check,
				     FALSE);
	g_object_set_data (G_OBJECT (check_button), "is-special",
			   GINT_TO_POINTER (TRUE));

	return check_button;
}

static void
append_special_execution_flags (NautilusPropertiesWindow *window, GtkGrid *grid)
{
	GtkWidget *title;

	append_blank_slim_row (grid);
	title = GTK_WIDGET (attach_title_field (grid, _("Special flags:")));

	append_special_execution_checkbox (window, grid, title, _("Set _user ID"), UNIX_PERM_SUID);
	append_special_execution_checkbox (window, grid, NULL, _("Set gro_up ID"), UNIX_PERM_SGID);
	append_special_execution_checkbox (window, grid, NULL, _("_Sticky"), UNIX_PERM_STICKY);
}

static gboolean
all_can_get_permissions (GList *file_list)
{
	GList *l;
	for (l = file_list; l != NULL; l = l->next) {
		NautilusFile *file;
		
		file = NAUTILUS_FILE (l->data);
		
		if (!nautilus_file_can_get_permissions (file)) {
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
all_can_set_permissions (GList *file_list)
{
	GList *l;
	for (l = file_list; l != NULL; l = l->next) {
		NautilusFile *file;
		
		file = NAUTILUS_FILE (l->data);

		if (!nautilus_file_can_set_permissions (file)) {
			return FALSE;
		}
	}

	return TRUE;
}

static GHashTable *
get_initial_permissions (GList *file_list)
{
	GHashTable *ret;
	GList *l;

	ret = g_hash_table_new (g_direct_hash,
				g_direct_equal);
	
	for (l = file_list; l != NULL; l = l->next) {
		guint32 permissions;
		NautilusFile *file;
		
		file = NAUTILUS_FILE (l->data);
		
		permissions = nautilus_file_get_permissions (file);
		g_hash_table_insert (ret, file,
				     GINT_TO_POINTER (permissions));
	}

	return ret;
}

static void
create_simple_permissions (NautilusPropertiesWindow *window, GtkGrid *page_grid)
{
	gboolean has_file, has_directory;
	GtkLabel *group_label;
	GtkLabel *owner_label;
	GtkLabel *execute_label;
	GtkWidget *value;
	GtkComboBox *group_combo_box;
	GtkComboBox *owner_combo_box;

	has_file = files_has_file (window);
	has_directory = files_has_directory (window);

	if (!is_multi_file_window (window) && nautilus_file_can_set_owner (get_target_file (window))) {
		owner_label = attach_title_field (page_grid, _("_Owner:"));
		/* Combo box in this case. */
		owner_combo_box = attach_owner_combo_box (page_grid,
							  GTK_WIDGET (owner_label),
							  get_target_file (window));
		gtk_label_set_mnemonic_widget (owner_label,
					       GTK_WIDGET (owner_combo_box));
	} else {
		owner_label = attach_title_field (page_grid, _("Owner:"));
		/* Static text in this case. */
		value = attach_value_field (window, 
					    page_grid, GTK_WIDGET (owner_label),
					    "owner",
					    INCONSISTENT_STATE_STRING,
					    FALSE); 
		gtk_label_set_mnemonic_widget (owner_label, value);
	}
	
	if (has_directory) {
		add_permissions_combo_box (window, page_grid,
					   PERMISSION_USER, TRUE, FALSE);
	}
	if (has_file || window->details->has_recursive_apply) {
		add_permissions_combo_box (window, page_grid,
					   PERMISSION_USER, FALSE, !has_directory);
	}

	append_blank_slim_row (page_grid);

	if (!is_multi_file_window (window) && nautilus_file_can_set_group (get_target_file (window))) {
		group_label = attach_title_field (page_grid, _("_Group:"));

		/* Combo box in this case. */
		group_combo_box = attach_group_combo_box (page_grid, GTK_WIDGET (group_label),
							  get_target_file (window));
		gtk_label_set_mnemonic_widget (group_label,
					       GTK_WIDGET (group_combo_box));
	} else {
		group_label = attach_title_field (page_grid, _("Group:"));

		/* Static text in this case. */
		value = attach_value_field (window, page_grid, 
					    GTK_WIDGET (group_label), 
					    "group",
					    INCONSISTENT_STATE_STRING,
					    FALSE); 
		gtk_label_set_mnemonic_widget (group_label, value);
	}
	
	if (has_directory) {
		add_permissions_combo_box (window, page_grid,
					   PERMISSION_GROUP, TRUE,
					   FALSE);
	}
	if (has_file || window->details->has_recursive_apply) {
		add_permissions_combo_box (window, page_grid,
					   PERMISSION_GROUP, FALSE,
					   !has_directory);
	}

	append_blank_slim_row (page_grid);

	group_label = attach_title_field (page_grid, _("Others"));
	
	if (has_directory) {
		add_permissions_combo_box (window, page_grid,
					   PERMISSION_OTHER, TRUE,
					   FALSE);
	}
	if (has_file || window->details->has_recursive_apply) {
		add_permissions_combo_box (window, page_grid,
					   PERMISSION_OTHER, FALSE,
					   !has_directory);
	}

	append_blank_slim_row (page_grid);
	
	execute_label = attach_title_field (page_grid, _("Execute:"));
	add_permissions_checkbox_with_label (window, page_grid,
					     GTK_WIDGET (execute_label),
					     _("Allow _executing file as program"),
					     UNIX_PERM_USER_EXEC|UNIX_PERM_GROUP_EXEC|UNIX_PERM_OTHER_EXEC,
					     execute_label, FALSE);
	
}

static void
create_permission_checkboxes (NautilusPropertiesWindow *window,
			      GtkGrid *page_grid,
			      gboolean is_folder)
{
	GtkLabel *owner_perm_label;
	GtkLabel *group_perm_label;
	GtkLabel *other_perm_label;
	GtkGrid *check_button_grid;
	GtkWidget *w;
	
	owner_perm_label = attach_title_field (page_grid, _("Owner:"));
	group_perm_label = attach_title_field (page_grid, _("Group:"));
	other_perm_label = attach_title_field (page_grid, _("Others:"));

	check_button_grid = GTK_GRID (create_grid_with_standard_properties ());
	gtk_widget_show (GTK_WIDGET (check_button_grid));

	gtk_grid_attach_next_to (page_grid, GTK_WIDGET (check_button_grid),
				 GTK_WIDGET (owner_perm_label),
				 GTK_POS_RIGHT, 1, 3);
	
	w = add_permissions_checkbox (window,
				      check_button_grid, 
				      NULL,
				      PERMISSIONS_CHECKBOXES_READ,
				      UNIX_PERM_USER_READ,
				      owner_perm_label,
				      is_folder);

	w = add_permissions_checkbox (window,
				      check_button_grid, 
				      w,
				      PERMISSIONS_CHECKBOXES_WRITE,
				      UNIX_PERM_USER_WRITE,
				      owner_perm_label,
				      is_folder);

	w = add_permissions_checkbox (window,
				      check_button_grid,
				      w,
				      PERMISSIONS_CHECKBOXES_EXECUTE,
				      UNIX_PERM_USER_EXEC,
				      owner_perm_label,
				      is_folder);

	w = add_permissions_checkbox (window,
				      check_button_grid, 
				      NULL,
				      PERMISSIONS_CHECKBOXES_READ,
				      UNIX_PERM_GROUP_READ,
				      group_perm_label,
				      is_folder);
	
	w = add_permissions_checkbox (window,
				      check_button_grid, 
				      w,
				      PERMISSIONS_CHECKBOXES_WRITE,
				      UNIX_PERM_GROUP_WRITE,
				      group_perm_label,
				      is_folder);
	
	w = add_permissions_checkbox (window,
				      check_button_grid, 
				      w,
				      PERMISSIONS_CHECKBOXES_EXECUTE,
				      UNIX_PERM_GROUP_EXEC,
				      group_perm_label,
				      is_folder);
	
	w = add_permissions_checkbox (window,
				      check_button_grid, 
				      NULL,
				      PERMISSIONS_CHECKBOXES_READ,
				      UNIX_PERM_OTHER_READ,
				      other_perm_label,
				      is_folder);
	
	w = add_permissions_checkbox (window,
				      check_button_grid, 
				      w,
				      PERMISSIONS_CHECKBOXES_WRITE,
				      UNIX_PERM_OTHER_WRITE,
				      other_perm_label,
				      is_folder);

	add_permissions_checkbox (window,
				  check_button_grid, 
				  w,
				  PERMISSIONS_CHECKBOXES_EXECUTE,
				  UNIX_PERM_OTHER_EXEC,
				  other_perm_label,
				  is_folder);
}

static void
create_advanced_permissions (NautilusPropertiesWindow *window, GtkGrid *page_grid)
{
	GtkLabel *group_label;
	GtkLabel *owner_label;
	GtkComboBox *group_combo_box;
	GtkComboBox *owner_combo_box;
	gboolean has_directory, has_file;

	if (!is_multi_file_window (window) && nautilus_file_can_set_owner (get_target_file (window))) {
		
		owner_label  = attach_title_field (page_grid, _("_Owner:"));
		/* Combo box in this case. */
		owner_combo_box = attach_owner_combo_box (page_grid,
							  GTK_WIDGET (owner_label),
							  get_target_file (window));
		gtk_label_set_mnemonic_widget (owner_label,
					       GTK_WIDGET (owner_combo_box));
	} else {
		GtkWidget *value;

		owner_label = attach_title_field (page_grid, _("Owner:"));
		/* Static text in this case. */
		value = attach_value_field (window, 
					    page_grid,
					    GTK_WIDGET (owner_label),
					    "owner",
					    INCONSISTENT_STATE_STRING,
					    FALSE); 
		gtk_label_set_mnemonic_widget (owner_label, value);
	}
	
	if (!is_multi_file_window (window) && nautilus_file_can_set_group (get_target_file (window))) {
		group_label = attach_title_field (page_grid, _("_Group:"));

		/* Combo box in this case. */
		group_combo_box = attach_group_combo_box (page_grid, GTK_WIDGET (group_label),
							  get_target_file (window));
		gtk_label_set_mnemonic_widget (group_label,
					       GTK_WIDGET (group_combo_box));
	} else {
		group_label = attach_title_field (page_grid, _("Group:"));

		/* Static text in this case. */
		attach_value_field (window, page_grid, GTK_WIDGET (group_label),
				    "group",
				    INCONSISTENT_STATE_STRING,
				    FALSE); 
	}

	append_blank_slim_row (page_grid);

	has_directory = files_has_directory (window);
	has_file = files_has_file (window);

	if (has_directory) {
		if (has_file || window->details->has_recursive_apply) {
			attach_title_field (page_grid, _("Folder Permissions:"));
		}
		create_permission_checkboxes (window, page_grid, TRUE);
	}


	if (has_file || window->details->has_recursive_apply) {
		if (has_directory) {
			attach_title_field (page_grid, _("File Permissions:"));
		}
		create_permission_checkboxes (window, page_grid, FALSE);
	}

	append_blank_slim_row (page_grid);
	append_special_execution_flags (window, page_grid);
	
	append_title_value_pair
		(window, page_grid, _("Text view:"), 
		 "permissions", INCONSISTENT_STATE_STRING,
		 FALSE);
}

static void
set_recursive_permissions_done (gboolean success,
				gpointer callback_data)
{
	NautilusPropertiesWindow *window;

	window = NAUTILUS_PROPERTIES_WINDOW (callback_data);
	end_long_operation (window);

	g_object_unref (window);
}


static void
apply_recursive_clicked (GtkWidget *recursive_button,
			 NautilusPropertiesWindow *window)
{
	guint32 file_permission, file_permission_mask;
	guint32 dir_permission, dir_permission_mask;
	guint32 vfs_mask, vfs_new_perm, p;
	GtkWidget *button, *combo;
	gboolean active, is_folder, is_special, use_original;
	GList *l;
	GtkTreeModel *model;
	GtkTreeIter iter;
	PermissionType type;
	int new_perm, mask;
	
	file_permission = 0;
	file_permission_mask = 0;
	dir_permission = 0;
	dir_permission_mask = 0;

	/* Advanced mode and execute checkbox: */
	for (l = window->details->permission_buttons; l != NULL; l = l->next) {
		button = l->data;
		
		if (gtk_toggle_button_get_inconsistent (GTK_TOGGLE_BUTTON (button))) {
			continue;
		}
		
		active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
		p = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
							"permission"));
		is_folder = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
								"is-folder"));
		is_special = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
								 "is-special"));
		
		if (is_folder || is_special) {
			dir_permission_mask |= p;
			if (active) {
				dir_permission |= p;
			}
		}
		if (!is_folder || is_special) {
			file_permission_mask |= p;
			if (active) {
				file_permission |= p;
			}
		}
	}
	/* Simple mode, minus exec checkbox */
	for (l = window->details->permission_combos; l != NULL; l = l->next) {
		combo = l->data;
		
		if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo),  &iter)) {
			continue;
		}
		
		type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo), "permission-type"));
		is_folder = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (combo),
								"is-folder"));
		
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
		gtk_tree_model_get (model, &iter, 1, &new_perm, 2, &use_original, -1);
		if (use_original) {
			continue;
		}
		vfs_new_perm = permission_to_vfs (type, new_perm);
		
		if (is_folder) {
			mask = PERMISSION_READ|PERMISSION_WRITE|PERMISSION_EXEC;
		} else {
			mask = PERMISSION_READ|PERMISSION_WRITE;
		}
		vfs_mask = permission_to_vfs (type, mask);
		
		if (is_folder) {
			dir_permission_mask |= vfs_mask;
			dir_permission |= vfs_new_perm;
		} else {
			file_permission_mask |= vfs_mask;
			file_permission |= vfs_new_perm;
		}
	}

	for (l = window->details->target_files; l != NULL; l = l->next) {
		NautilusFile *file;
		char *uri;

		file = NAUTILUS_FILE (l->data);

		if (nautilus_file_is_directory (file) &&
		    nautilus_file_can_set_permissions (file)) {
			uri = nautilus_file_get_uri (file);
			start_long_operation (window);
			g_object_ref (window);
			nautilus_file_set_permissions_recursive (uri,
								 file_permission,
								 file_permission_mask,
								 dir_permission,
								 dir_permission_mask,
								 set_recursive_permissions_done,
								 window);
			g_free (uri);
		}
	}
}

static void
create_permissions_page (NautilusPropertiesWindow *window)
{
	GtkWidget *vbox, *button, *hbox;
	GtkGrid *page_grid;
	char *file_name, *prompt_text;
	GList *file_list;

	vbox = create_page_with_vbox (window->details->notebook,
				      _("Permissions"),
				      "help:gnome-help/nautilus-file-properties-permissions");

	file_list = window->details->original_files;

	window->details->initial_permissions = NULL;
	
	if (all_can_get_permissions (file_list) && all_can_get_permissions (window->details->target_files)) {
		window->details->initial_permissions = get_initial_permissions (window->details->target_files);
		window->details->has_recursive_apply = files_has_changable_permissions_directory (window);
		
		if (!all_can_set_permissions (file_list)) {
			add_prompt_and_separator (
				vbox, 
				_("You are not the owner, so you cannot change these permissions."));
		}

		page_grid = GTK_GRID (create_grid_with_standard_properties ());

		gtk_widget_show (GTK_WIDGET (page_grid));
		gtk_box_pack_start (GTK_BOX (vbox), 
				    GTK_WIDGET (page_grid), 
				    TRUE, TRUE, 0);

		if (g_settings_get_boolean (nautilus_preferences, NAUTILUS_PREFERENCES_SHOW_ADVANCED_PERMISSIONS)) {
			create_advanced_permissions (window, page_grid);
		} else {
			create_simple_permissions (window, page_grid);
		}

		append_blank_slim_row (page_grid);
	
#ifdef HAVE_SELINUX
		append_title_value_pair
			(window, page_grid, _("SELinux context:"), 
			 "selinux_context", INCONSISTENT_STATE_STRING,
			 FALSE);
#endif
		append_title_value_pair
			(window, page_grid, _("Last changed:"), 
			 "date_permissions", INCONSISTENT_STATE_STRING,
			 FALSE);
	
		if (window->details->has_recursive_apply) {
			hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
			gtk_widget_show (hbox);

			gtk_container_add_with_properties (GTK_CONTAINER (page_grid), hbox,
							   "width", 2,
							   NULL);
		
			button = gtk_button_new_with_mnemonic (_("Apply Permissions to Enclosed Files"));
			gtk_widget_show (button);
			gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
			g_signal_connect (button, "clicked",
					  G_CALLBACK (apply_recursive_clicked),
					  window);
		}
	} else {
		if (!is_multi_file_window (window)) {
			file_name = nautilus_file_get_display_name (get_target_file (window));
			prompt_text = g_strdup_printf (_("The permissions of \"%s\" could not be determined."), file_name);
			g_free (file_name);
		} else {
			prompt_text = g_strdup (_("The permissions of the selected file could not be determined."));
		}
		
		add_prompt (vbox, prompt_text, TRUE);
		g_free (prompt_text);
	}
}

static void
append_extension_pages (NautilusPropertiesWindow *window)
{
	GList *providers;
	GList *p;
	
 	providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_PROPERTY_PAGE_PROVIDER);
	
	for (p = providers; p != NULL; p = p->next) {
		NautilusPropertyPageProvider *provider;
		GList *pages;
		GList *l;

		provider = NAUTILUS_PROPERTY_PAGE_PROVIDER (p->data);
		
		pages = nautilus_property_page_provider_get_pages 
			(provider, window->details->original_files);
		
		for (l = pages; l != NULL; l = l->next) {
			NautilusPropertyPage *page;
			GtkWidget *page_widget;
			GtkWidget *label;
			
			page = NAUTILUS_PROPERTY_PAGE (l->data);

			g_object_get (G_OBJECT (page), 
				      "page", &page_widget, "label", &label, 
				      NULL);
			
			gtk_notebook_append_page (window->details->notebook, 
						  page_widget, label);

			g_object_set_data (G_OBJECT (page_widget), 
					   "is-extension-page",
					   page);

			g_object_unref (page_widget);
			g_object_unref (label);

			g_object_unref (page);
		}

		g_list_free (pages);
	}

	nautilus_module_extension_list_free (providers);
}

static gboolean
should_show_permissions (NautilusPropertiesWindow *window) 
{
	NautilusFile *file;

	file = get_target_file (window);

	/* Don't show permissions for Trash and Computer since they're not
	 * really file system objects.
	 */
	if (!is_multi_file_window (window)
	    && (is_merged_trash_directory (file) ||
		is_computer_directory (file))) {
		return FALSE;
	}

	return TRUE;
}

static char *
get_pending_key (GList *file_list)
{
	GList *l;
	GList *uris;
	GString *key;
	char *ret;
	
	uris = NULL;
	for (l = file_list; l != NULL; l = l->next) {
		uris = g_list_prepend (uris, nautilus_file_get_uri (NAUTILUS_FILE (l->data)));
	}
	uris = g_list_sort (uris, (GCompareFunc)strcmp);

	key = g_string_new ("");
	for (l = uris; l != NULL; l = l->next) {
		g_string_append (key, l->data);
		g_string_append (key, ";");
	}

	g_list_free_full (uris, g_free);

	ret = key->str;
	g_string_free (key, FALSE);

	return ret;
}

static StartupData *
startup_data_new (GList *original_files, 
		  GList *target_files,
		  const char *pending_key,
		  GtkWidget *parent_widget,
		  const char *startup_id)
{
	StartupData *data;
	GList *l;

	data = g_new0 (StartupData, 1);
	data->original_files = nautilus_file_list_copy (original_files);
	data->target_files = nautilus_file_list_copy (target_files);
	data->parent_widget = parent_widget;
	data->startup_id = g_strdup (startup_id);
	data->pending_key = g_strdup (pending_key);
	data->pending_files = g_hash_table_new (g_direct_hash,
						g_direct_equal);

	for (l = data->target_files; l != NULL; l = l->next) {
		g_hash_table_insert (data->pending_files, l->data, l->data);
	}

	return data;
}

static void
startup_data_free (StartupData *data)
{
	nautilus_file_list_free (data->original_files);
	nautilus_file_list_free (data->target_files);
	g_hash_table_destroy (data->pending_files);
	g_free (data->pending_key);
	g_free (data->startup_id);
	g_free (data);
}

static void
file_changed_callback (NautilusFile *file, gpointer user_data)
{
	NautilusPropertiesWindow *window = NAUTILUS_PROPERTIES_WINDOW (user_data);

	if (!g_list_find (window->details->changed_files, file)) {
		nautilus_file_ref (file);
		window->details->changed_files = g_list_prepend (window->details->changed_files, file);
		
		schedule_files_update (window);
	}
}

static gboolean
is_a_special_file (NautilusFile *file)
{
	if (file == NULL ||
	    NAUTILUS_IS_DESKTOP_ICON_FILE (file) ||
	    nautilus_file_is_nautilus_link (file) ||
	    is_merged_trash_directory (file) ||
	    is_computer_directory (file)) {
		return TRUE;
	}
	return FALSE;
}

static gboolean
should_show_open_with (NautilusPropertiesWindow *window)
{
	NautilusFile *file;

	/* Don't show open with tab for desktop special icons (trash, etc)
	 * or desktop files. We don't get the open-with menu for these anyway.
	 *
	 * Also don't show it for folders. Changing the default app for folders
	 * leads to all sort of hard to understand errors.
	 */
	
	if (is_multi_file_window (window)) {
		if (!file_list_attributes_identical (window->details->original_files,
						     "mime_type")) {
			return FALSE;
		} else {
			
			GList *l;
			
			for (l = window->details->original_files; l; l = l->next) {
				file = NAUTILUS_FILE (l->data);
				if (nautilus_file_is_directory (file) ||
				    is_a_special_file (file)) {
					return FALSE;
				}
			}
		}		
	} else {
		file = get_original_file (window);
		if (nautilus_file_is_directory (file) ||
		    is_a_special_file (file)) {
			return FALSE;
		}
	}
	return TRUE;
}

static void
create_open_with_page (NautilusPropertiesWindow *window)
{
	GtkWidget *vbox;
	char *mime_type;
	char *uri = NULL;
	GList *uris = NULL;

	mime_type = nautilus_file_get_mime_type (get_target_file (window));

	if (!is_multi_file_window (window)) {
		uri = nautilus_file_get_uri (get_target_file (window));
		if (uri == NULL) {
			return;
		}
	} else {
		uris = window->details->original_files;
		if (uris == NULL) {
			return;
		}
	}

	vbox = nautilus_mime_application_chooser_new (uri, uris, mime_type);

	gtk_widget_show (vbox);
	g_free (mime_type);

	g_object_set_data_full (G_OBJECT (vbox), "help-uri", g_strdup ("help:gnome-help/files-open"), g_free);
	gtk_notebook_append_page (window->details->notebook, 
				  vbox, gtk_label_new (_("Open With")));
}


static NautilusPropertiesWindow *
create_properties_window (StartupData *startup_data)
{
	NautilusPropertiesWindow *window;
	GList *l;

	window = NAUTILUS_PROPERTIES_WINDOW (gtk_widget_new (NAUTILUS_TYPE_PROPERTIES_WINDOW, NULL));

	window->details->original_files = nautilus_file_list_copy (startup_data->original_files);
	
	window->details->target_files = nautilus_file_list_copy (startup_data->target_files);

	gtk_window_set_wmclass (GTK_WINDOW (window), "file_properties", "Nautilus");

	if (startup_data->parent_widget) {
		gtk_window_set_screen (GTK_WINDOW (window),
				       gtk_widget_get_screen (startup_data->parent_widget));
	}

	if (startup_data->startup_id) {
		gtk_window_set_startup_id (GTK_WINDOW (window), startup_data->startup_id);
	}

	gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);

	/* Set initial window title */
	update_properties_window_title (window);

	/* Start monitoring the file attributes we display. Note that some
	 * of the attributes are for the original file, and some for the
	 * target files.
	 */

	for (l = window->details->original_files; l != NULL; l = l->next) {
		NautilusFile *file;
		NautilusFileAttributes attributes;

		file = NAUTILUS_FILE (l->data);

		attributes =
			NAUTILUS_FILE_ATTRIBUTES_FOR_ICON |
			NAUTILUS_FILE_ATTRIBUTE_INFO |
			NAUTILUS_FILE_ATTRIBUTE_LINK_INFO;

		nautilus_file_monitor_add (file,
					   &window->details->original_files, 
					   attributes);	
	}
	
	for (l = window->details->target_files; l != NULL; l = l->next) {
		NautilusFile *file;
		NautilusFileAttributes attributes;

		file = NAUTILUS_FILE (l->data);
		
		attributes = 0;
		if (nautilus_file_is_directory (file)) {
			attributes |= NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS;
		}
		
		attributes |= NAUTILUS_FILE_ATTRIBUTE_INFO;
		nautilus_file_monitor_add (file, &window->details->target_files, attributes);
	}	
		
	for (l = window->details->target_files; l != NULL; l = l->next) {
		g_signal_connect_object (NAUTILUS_FILE (l->data),
					 "changed",
					 G_CALLBACK (file_changed_callback),
					 G_OBJECT (window),
					 0);
	}

	for (l = window->details->original_files; l != NULL; l = l->next) {
		g_signal_connect_object (NAUTILUS_FILE (l->data),
					 "changed",
					 G_CALLBACK (file_changed_callback),
					 G_OBJECT (window),
					 0);
	}

	/* Create the notebook tabs. */
	window->details->notebook = GTK_NOTEBOOK (gtk_notebook_new ());
	gtk_widget_show (GTK_WIDGET (window->details->notebook));
	gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))),
			    GTK_WIDGET (window->details->notebook),
			    TRUE, TRUE, 0);

	/* Create the pages. */
	create_basic_page (window);

	if (should_show_permissions (window)) {
		create_permissions_page (window);
	}

	if (should_show_open_with (window)) {
		create_open_with_page (window);
	}

	/* append pages from available views */
	append_extension_pages (window);

	gtk_dialog_add_buttons (GTK_DIALOG (window),
				GTK_STOCK_HELP, GTK_RESPONSE_HELP,
				GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
				NULL);

	/* FIXME - HIGificiation, should be done inside GTK+ */
	gtk_container_set_border_width (GTK_CONTAINER (window), 12);
	gtk_container_set_border_width (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (window))), 12);
	gtk_container_set_border_width (GTK_CONTAINER (gtk_dialog_get_action_area (GTK_DIALOG (window))), 0);
	gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))), 12);

	/* Update from initial state */
	properties_window_update (window, NULL);

	return window;
}

static GList *
get_target_file_list (GList *original_files)
{
	GList *ret;
	GList *l;
	
	ret = NULL;
	
	for (l = original_files; l != NULL; l = l->next) {
		NautilusFile *target;
		
		target = get_target_file_for_original_file (NAUTILUS_FILE (l->data));
		
		ret = g_list_prepend (ret, target);
	}

	ret = g_list_reverse (ret);

	return ret;
}

static void
add_window (NautilusPropertiesWindow *window)
{
	if (!is_multi_file_window (window)) {
		g_hash_table_insert (windows,
				     get_original_file (window), 
				     window);
		g_object_set_data (G_OBJECT (window), "window_key", 
				   get_original_file (window));
	}
}

static void
remove_window (NautilusPropertiesWindow *window)
{
	gpointer key;

	key = g_object_get_data (G_OBJECT (window), "window_key");
	if (key) {
		g_hash_table_remove (windows, key);
	}
}

static GtkWindow *
get_existing_window (GList *file_list)
{
	if (!file_list->next) {
		return g_hash_table_lookup (windows, file_list->data);
	}	

	return NULL;
}

static void
cancel_create_properties_window_callback (gpointer callback_data)
{
	remove_pending ((StartupData *)callback_data, TRUE, FALSE, TRUE);
}

static void
parent_widget_destroyed_callback (GtkWidget *widget, gpointer callback_data)
{
	g_assert (widget == ((StartupData *)callback_data)->parent_widget);
	
	remove_pending ((StartupData *)callback_data, TRUE, TRUE, FALSE);
}

static void
cancel_call_when_ready_callback (gpointer key,
				 gpointer value,
				 gpointer user_data)
{
	nautilus_file_cancel_call_when_ready 
		(NAUTILUS_FILE (key), 
		 is_directory_ready_callback, 
		 user_data);
}

static void
remove_pending (StartupData *startup_data,
		gboolean cancel_call_when_ready,
		gboolean cancel_timed_wait,
		gboolean cancel_destroy_handler)
{
	if (cancel_call_when_ready) {
		g_hash_table_foreach (startup_data->pending_files,
				      cancel_call_when_ready_callback,
				      startup_data);
				      
	}
	if (cancel_timed_wait) {
		eel_timed_wait_stop 
			(cancel_create_properties_window_callback, startup_data);
	}
	if (cancel_destroy_handler && startup_data->parent_widget) {
		g_signal_handlers_disconnect_by_func (startup_data->parent_widget,
						      G_CALLBACK (parent_widget_destroyed_callback),
						      startup_data);
	}

	g_hash_table_remove (pending_lists, startup_data->pending_key);

	startup_data_free (startup_data);
}

static void
is_directory_ready_callback (NautilusFile *file,
			     gpointer data)
{
	StartupData *startup_data;
	
	startup_data = data;
	
	g_hash_table_remove (startup_data->pending_files, file);

	if (g_hash_table_size (startup_data->pending_files) == 0) {
		NautilusPropertiesWindow *new_window;
		
		new_window = create_properties_window (startup_data);
		
		add_window (new_window);
		
		remove_pending (startup_data, FALSE, TRUE, TRUE);
		
		gtk_window_present (GTK_WINDOW (new_window));
	}
}


void
nautilus_properties_window_present (GList       *original_files,
				    GtkWidget   *parent_widget,
				    const gchar *startup_id) 
{
	GList *l, *next;
	GtkWidget *parent_window;
	StartupData *startup_data;
	GList *target_files;
	GtkWindow *existing_window;
	char *pending_key;

	g_return_if_fail (original_files != NULL);
	g_return_if_fail (parent_widget == NULL || GTK_IS_WIDGET (parent_widget));

	/* Create the hash tables first time through. */
	if (windows == NULL) {
		windows = g_hash_table_new (NULL, NULL);
	}
	
	if (pending_lists == NULL) {
		pending_lists = g_hash_table_new (g_str_hash, g_str_equal);
	}
	
	/* Look to see if there's already a window for this file. */
	existing_window = get_existing_window (original_files);
	if (existing_window != NULL) {
		if (parent_widget)
			gtk_window_set_screen (existing_window,
					       gtk_widget_get_screen (parent_widget));
		else if (startup_id)
			gtk_window_set_startup_id (existing_window, startup_id);

		gtk_window_present (existing_window);
		return;
	}


	pending_key = get_pending_key (original_files);
	
	/* Look to see if we're already waiting for a window for this file. */
	if (g_hash_table_lookup (pending_lists, pending_key) != NULL) {
		return;
	}

	target_files = get_target_file_list (original_files);

	startup_data = startup_data_new (original_files, 
					 target_files,
					 pending_key,
					 parent_widget,
					 startup_id);

	nautilus_file_list_free (target_files);
	g_free(pending_key);

	/* Wait until we can tell whether it's a directory before showing, since
	 * some one-time layout decisions depend on that info. 
	 */
	
	g_hash_table_insert (pending_lists, startup_data->pending_key, startup_data->pending_key);
	if (parent_widget) {
		g_signal_connect (parent_widget, "destroy",
				  G_CALLBACK (parent_widget_destroyed_callback), startup_data);

		parent_window = gtk_widget_get_ancestor (parent_widget, GTK_TYPE_WINDOW);
	} else
		parent_window = NULL;

	eel_timed_wait_start
		(cancel_create_properties_window_callback,
		 startup_data,
		 _("Creating Properties window."),
		 parent_window == NULL ? NULL : GTK_WINDOW (parent_window));

	for (l = startup_data->target_files; l != NULL; l = next) {
		next = l->next;
		nautilus_file_call_when_ready
			(NAUTILUS_FILE (l->data),
			 NAUTILUS_FILE_ATTRIBUTE_INFO,
			 is_directory_ready_callback,
			 startup_data);
	}
}

static void
real_response (GtkDialog *dialog,
	       int        response)
{
	GError *error = NULL;
	NautilusPropertiesWindow *window = NAUTILUS_PROPERTIES_WINDOW (dialog);
	GtkWidget *curpage;
	const char *helpuri;

	switch (response) {
	case GTK_RESPONSE_HELP:
		curpage = gtk_notebook_get_nth_page (window->details->notebook,
						     gtk_notebook_get_current_page (window->details->notebook));
		helpuri = g_object_get_data (G_OBJECT (curpage), "help-uri");
		gtk_show_uri (gtk_window_get_screen (GTK_WINDOW (dialog)),
			      helpuri ? helpuri : "help:gnome-help/files",
			      gtk_get_current_event_time (),
			      &error);
		if (error != NULL) {
			eel_show_error_dialog (_("There was an error displaying help."), error->message,
					       GTK_WINDOW (dialog));
			g_error_free (error);
		}
		break;

	case GTK_RESPONSE_NONE:
	case GTK_RESPONSE_CLOSE:
	case GTK_RESPONSE_DELETE_EVENT:
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;

	default:
		g_assert_not_reached ();
		break;
	}
}

static void
real_destroy (GtkWidget *object)
{
	NautilusPropertiesWindow *window;
	GList *l;

	window = NAUTILUS_PROPERTIES_WINDOW (object);

	remove_window (window);

	for (l = window->details->original_files; l != NULL; l = l->next) {
		nautilus_file_monitor_remove (NAUTILUS_FILE (l->data), &window->details->original_files);
	}
	nautilus_file_list_free (window->details->original_files);
	window->details->original_files = NULL;
	
	for (l = window->details->target_files; l != NULL; l = l->next) {
		nautilus_file_monitor_remove (NAUTILUS_FILE (l->data), &window->details->target_files);
	}
	nautilus_file_list_free (window->details->target_files);
	window->details->target_files = NULL;

	nautilus_file_list_free (window->details->changed_files);
	window->details->changed_files = NULL;
 
	window->details->name_field = NULL;

	g_list_free (window->details->permission_buttons);
	window->details->permission_buttons = NULL;

	g_list_free (window->details->permission_combos);
	window->details->permission_combos = NULL;

	if (window->details->initial_permissions) {
		g_hash_table_destroy (window->details->initial_permissions);
		window->details->initial_permissions = NULL;
	}

	g_list_free (window->details->value_fields);
	window->details->value_fields = NULL;

	if (window->details->update_directory_contents_timeout_id != 0) {
		g_source_remove (window->details->update_directory_contents_timeout_id);
		window->details->update_directory_contents_timeout_id = 0;
	}

	if (window->details->update_files_timeout_id != 0) {
		g_source_remove (window->details->update_files_timeout_id);
		window->details->update_files_timeout_id = 0;
	}

	GTK_WIDGET_CLASS (nautilus_properties_window_parent_class)->destroy (object);
}

static void
real_finalize (GObject *object)
{
	NautilusPropertiesWindow *window;

	window = NAUTILUS_PROPERTIES_WINDOW (object);

	g_list_free_full (window->details->mime_list, g_free);

	g_free (window->details->pending_name);

	G_OBJECT_CLASS (nautilus_properties_window_parent_class)->finalize (object);
}

/* converts
 *  file://foo/foobar/foofoo/bar
 * to
 *  foofoo/bar
 * if
 *  file://foo/foobar
 * is the parent
 *
 * It does not resolve any symlinks.
 * */
static char *
make_relative_uri_from_full (const char *uri,
			     const char *base_uri)
{
	g_assert (uri != NULL);
	g_assert (base_uri != NULL);

	if (g_str_has_prefix (uri, base_uri)) {
		uri += strlen (base_uri);
		if (*uri != '/') {
			return NULL;
		}

		while (*uri == '/') {
			uri++;
		}

		if (*uri != '\0') {
			return g_strdup (uri);
		}
	}

	return NULL;
}

/* icon selection callback to set the image of the file object to the selected file */
static void
set_icon (const char* icon_uri, NautilusPropertiesWindow *properties_window)
{
	NautilusFile *file;
	char *file_uri;
	char *icon_path;
	char *real_icon_uri;

	g_assert (icon_uri != NULL);
	g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (properties_window));

	icon_path = g_filename_from_uri (icon_uri, NULL, NULL);
	/* we don't allow remote URIs */
	if (icon_path != NULL) {
		GList *l;

		for (l = properties_window->details->original_files; l != NULL; l = l->next) {
			file = NAUTILUS_FILE (l->data);

			file_uri = nautilus_file_get_uri (file);

			if (nautilus_file_is_mime_type (file, "application/x-desktop")) {
				if (nautilus_link_local_set_icon (file_uri, icon_path)) {
					nautilus_file_invalidate_attributes (file,
									     NAUTILUS_FILE_ATTRIBUTE_INFO |
									     NAUTILUS_FILE_ATTRIBUTE_LINK_INFO);
				}
			} else {
				real_icon_uri = make_relative_uri_from_full (icon_uri, file_uri);
				if (real_icon_uri == NULL) {
					real_icon_uri = g_strdup (icon_uri);
				}
			
				nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL, real_icon_uri);
				nautilus_file_set_metadata (file, NAUTILUS_METADATA_KEY_ICON_SCALE, NULL, NULL);

				g_free (real_icon_uri);
			}

			g_free (file_uri);
		}

		g_free (icon_path);
	}
}

static void
update_preview_callback (GtkFileChooser *icon_chooser,
			 NautilusPropertiesWindow *window)
{
	GtkWidget *preview_widget;
	GdkPixbuf *pixbuf, *scaled_pixbuf;
	char *filename;
	double scale;

	pixbuf = NULL;

	filename = gtk_file_chooser_get_filename (icon_chooser);
	if (filename != NULL) {
		pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
	}

	if (pixbuf != NULL) {
		preview_widget = gtk_file_chooser_get_preview_widget (icon_chooser);
		gtk_file_chooser_set_preview_widget_active (icon_chooser, TRUE);

		if (gdk_pixbuf_get_width (pixbuf) > PREVIEW_IMAGE_WIDTH) {
			scale = (double)gdk_pixbuf_get_height (pixbuf) /
				gdk_pixbuf_get_width (pixbuf);

			scaled_pixbuf = gnome_desktop_thumbnail_scale_down_pixbuf
				(pixbuf,
				 PREVIEW_IMAGE_WIDTH,
				 scale * PREVIEW_IMAGE_WIDTH);
			g_object_unref (pixbuf);
			pixbuf = scaled_pixbuf;
		}

		gtk_image_set_from_pixbuf (GTK_IMAGE (preview_widget), pixbuf);
	} else {
		gtk_file_chooser_set_preview_widget_active (icon_chooser, FALSE);
	}

	g_free (filename);

	if (pixbuf != NULL) {
		g_object_unref (pixbuf);
	}
}

static void
custom_icon_file_chooser_response_cb (GtkDialog *dialog,
				      gint response,
				      NautilusPropertiesWindow *window)
{
	char *uri;

	switch (response) {
	case GTK_RESPONSE_NO:
		reset_icon (window);
		break;

	case GTK_RESPONSE_OK:
		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
		set_icon (uri, window);
		g_free (uri);
		break;

	default:
		break;
	}

	gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
select_image_button_callback (GtkWidget *widget,
			      NautilusPropertiesWindow *window)
{
	GtkWidget *dialog, *preview;
	GtkFileFilter *filter;
	GList *l;
	NautilusFile *file;
	char *uri;
	char *image_path;
	gboolean revert_is_sensitive;

	g_assert (NAUTILUS_IS_PROPERTIES_WINDOW (window));

	dialog = window->details->icon_chooser;

	if (dialog == NULL) {
		dialog = gtk_file_chooser_dialog_new (_("Select Custom Icon"), GTK_WINDOW (window),
						      GTK_FILE_CHOOSER_ACTION_OPEN,
						      GTK_STOCK_REVERT_TO_SAVED, GTK_RESPONSE_NO,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						      GTK_STOCK_OPEN, GTK_RESPONSE_OK,
						      NULL);
		gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (dialog), "/usr/share/pixmaps", NULL);
		gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

		filter = gtk_file_filter_new ();
		gtk_file_filter_add_pixbuf_formats (filter);
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

		preview = gtk_image_new ();
		gtk_widget_set_size_request (preview, PREVIEW_IMAGE_WIDTH, -1);
		gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (dialog), preview);
		gtk_file_chooser_set_use_preview_label (GTK_FILE_CHOOSER (dialog), FALSE);
		gtk_file_chooser_set_preview_widget_active (GTK_FILE_CHOOSER (dialog), FALSE);

		g_signal_connect (dialog, "update-preview",
				  G_CALLBACK (update_preview_callback), window);

		window->details->icon_chooser = dialog;

		g_object_add_weak_pointer (G_OBJECT (dialog),
					   (gpointer *) &window->details->icon_chooser);
	}

	/* it's likely that the user wants to pick an icon that is inside a local directory */
	if (g_list_length (window->details->original_files) == 1) {
		file = NAUTILUS_FILE (window->details->original_files->data);

		if (nautilus_file_is_directory (file)) {
			uri = nautilus_file_get_uri (file);

			image_path = g_filename_from_uri (uri, NULL, NULL);
			if (image_path != NULL) {
				gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), image_path);
				g_free (image_path);
			}

			g_free (uri);
		}
	}

	revert_is_sensitive = FALSE;
	for (l = window->details->original_files; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);
		image_path = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL);
		revert_is_sensitive = (image_path != NULL);
		g_free (image_path);

		if (revert_is_sensitive) {
			break;
		}
	}
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_NO, revert_is_sensitive);

	g_signal_connect (dialog, "response",
			  G_CALLBACK (custom_icon_file_chooser_response_cb), window);
	gtk_widget_show (dialog);
}

static void
nautilus_properties_window_class_init (NautilusPropertiesWindowClass *class)
{
	GtkBindingSet *binding_set;

	G_OBJECT_CLASS (class)->finalize = real_finalize;
	GTK_WIDGET_CLASS (class)->destroy = real_destroy;
	GTK_DIALOG_CLASS (class)->response = real_response;

	binding_set = gtk_binding_set_by_class (class);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0,
				      "close", 0);

	g_type_class_add_private (class, sizeof (NautilusPropertiesWindowDetails));
}

static void
nautilus_properties_window_init (NautilusPropertiesWindow *window)
{
	window->details = G_TYPE_INSTANCE_GET_PRIVATE (window, NAUTILUS_TYPE_PROPERTIES_WINDOW,
						       NautilusPropertiesWindowDetails);
}

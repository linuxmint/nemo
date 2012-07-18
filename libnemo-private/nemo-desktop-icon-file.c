/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-desktop-icon-file.c: Subclass of NemoFile to help implement the
   virtual desktop icons.
 
   Copyright (C) 2003 Red Hat, Inc.
  
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
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "nemo-desktop-icon-file.h"

#include "nemo-desktop-metadata.h"
#include "nemo-desktop-directory-file.h"
#include "nemo-directory-notify.h"
#include "nemo-directory-private.h"
#include "nemo-file-attributes.h"
#include "nemo-file-private.h"
#include "nemo-file-utilities.h"
#include "nemo-file-operations.h"
#include <eel/eel-glib-extensions.h>
#include "nemo-desktop-directory.h"
#include <glib/gi18n.h>
#include <string.h>
#include <gio/gio.h>

struct NemoDesktopIconFileDetails {
	NemoDesktopLink *link;
};

G_DEFINE_TYPE(NemoDesktopIconFile, nemo_desktop_icon_file, NEMO_TYPE_FILE)


static void
desktop_icon_file_monitor_add (NemoFile *file,
			       gconstpointer client,
			       NemoFileAttributes attributes)
{
	nemo_directory_monitor_add_internal
		(file->details->directory, file,
		 client, TRUE, attributes, NULL, NULL);
}

static void
desktop_icon_file_monitor_remove (NemoFile *file,
				  gconstpointer client)
{
	nemo_directory_monitor_remove_internal
		(file->details->directory, file, client);
}

static void
desktop_icon_file_call_when_ready (NemoFile *file,
				   NemoFileAttributes attributes,
				   NemoFileCallback callback,
				   gpointer callback_data)
{
	nemo_directory_call_when_ready_internal
		(file->details->directory, file,
		 attributes, FALSE, NULL, callback, callback_data);
}

static void
desktop_icon_file_cancel_call_when_ready (NemoFile *file,
					  NemoFileCallback callback,
					  gpointer callback_data)
{
	nemo_directory_cancel_callback_internal
		(file->details->directory, file,
		 NULL, callback, callback_data);
}

static gboolean
desktop_icon_file_check_if_ready (NemoFile *file,
				  NemoFileAttributes attributes)
{
	return nemo_directory_check_if_ready_internal
		(file->details->directory, file,
		 attributes);
}

static gboolean
desktop_icon_file_get_item_count (NemoFile *file, 
				  guint *count,
				  gboolean *count_unreadable)
{
	if (count != NULL) {
		*count = 0;
	}
	if (count_unreadable != NULL) {
		*count_unreadable = FALSE;
	}
	return TRUE;
}

static NemoRequestStatus
desktop_icon_file_get_deep_counts (NemoFile *file,
				   guint *directory_count,
				   guint *file_count,
				   guint *unreadable_directory_count,
				   goffset *total_size)
{
	if (directory_count != NULL) {
		*directory_count = 0;
	}
	if (file_count != NULL) {
		*file_count = 0;
	}
	if (unreadable_directory_count != NULL) {
		*unreadable_directory_count = 0;
	}
	if (total_size != NULL) {
		*total_size = 0;
	}

	return NEMO_REQUEST_DONE;
}

static gboolean
desktop_icon_file_get_date (NemoFile *file,
			    NemoDateType date_type,
			    time_t *date)
{
	NemoDesktopIconFile *desktop_file;

	desktop_file = NEMO_DESKTOP_ICON_FILE (file);

	return nemo_desktop_link_get_date (desktop_file->details->link,
					       date_type, date);
}

static char *
desktop_icon_file_get_where_string (NemoFile *file)
{
	return g_strdup (_("on the desktop"));
}

static void
nemo_desktop_icon_file_init (NemoDesktopIconFile *desktop_file)
{
	desktop_file->details =	G_TYPE_INSTANCE_GET_PRIVATE (desktop_file,
							     NEMO_TYPE_DESKTOP_ICON_FILE,
							     NemoDesktopIconFileDetails);
}

static void
update_info_from_link (NemoDesktopIconFile *icon_file)
{
	NemoFile *file;
	NemoDesktopLink *link;
	char *display_name;
	GMount *mount;
	
	file = NEMO_FILE (icon_file);
	
	link = icon_file->details->link;

	if (link == NULL) {
		return;
	}

	eel_ref_str_unref (file->details->mime_type);
	file->details->mime_type = eel_ref_str_get_unique ("application/x-nemo-link");
	file->details->type = G_FILE_TYPE_SHORTCUT;
	file->details->size = 0;
	file->details->has_permissions = FALSE;
	file->details->can_read = TRUE;
	file->details->can_write = TRUE;

	file->details->can_mount = FALSE;
	file->details->can_unmount = FALSE;
	file->details->can_eject = FALSE;
	if (file->details->mount) {
		g_object_unref (file->details->mount);
	}
	mount = nemo_desktop_link_get_mount (link);
	file->details->mount = mount;
	if (mount) {
		file->details->can_unmount = g_mount_can_unmount (mount);
		file->details->can_eject = g_mount_can_eject (mount);
	}
	
	file->details->file_info_is_up_to_date = TRUE;

	display_name = nemo_desktop_link_get_display_name (link);
	nemo_file_set_display_name (file,
					display_name, NULL, TRUE);
	g_free (display_name);

	if (file->details->icon != NULL) {
		g_object_unref (file->details->icon);
	}
	file->details->icon = nemo_desktop_link_get_icon (link);
	g_free (file->details->activation_uri);
	file->details->activation_uri = nemo_desktop_link_get_activation_uri (link);
	file->details->got_link_info = TRUE;
	file->details->link_info_is_up_to_date = TRUE;

	file->details->directory_count = 0;
	file->details->got_directory_count = TRUE;
	file->details->directory_count_is_up_to_date = TRUE;
}

void
nemo_desktop_icon_file_update (NemoDesktopIconFile *icon_file)
{
	NemoFile *file;
	
	update_info_from_link (icon_file);
	file = NEMO_FILE (icon_file);
	nemo_file_changed (file);
}

void
nemo_desktop_icon_file_remove (NemoDesktopIconFile *icon_file)
{
	NemoFile *file;
	GList list;

	icon_file->details->link = NULL;

	file = NEMO_FILE (icon_file);
	
	/* ref here because we might be removing the last ref when we
	 * mark the file gone below, but we need to keep a ref at
	 * least long enough to send the change notification. 
	 */
	nemo_file_ref (file);
	
	file->details->is_gone = TRUE;
	
	list.data = file;
	list.next = NULL;
	list.prev = NULL;
	
	nemo_directory_remove_file (file->details->directory, file);
	nemo_directory_emit_change_signals (file->details->directory, &list);
	
	nemo_file_unref (file);
}

NemoDesktopIconFile *
nemo_desktop_icon_file_new (NemoDesktopLink *link)
{
	NemoFile *file;
	NemoDirectory *directory;
	NemoDesktopIconFile *icon_file;
	GList list;
	char *name;

	directory = nemo_directory_get_by_uri (EEL_DESKTOP_URI);

	file = NEMO_FILE (g_object_new (NEMO_TYPE_DESKTOP_ICON_FILE, NULL));

#ifdef NEMO_FILE_DEBUG_REF
	printf("%10p ref'd\n", file);
	eazel_dump_stack_trace ("\t", 10);
#endif

	file->details->directory = directory;

	icon_file = NEMO_DESKTOP_ICON_FILE (file);
	icon_file->details->link = link;

	name = nemo_desktop_link_get_file_name (link);
	file->details->name = eel_ref_str_new (name);
	g_free (name);

	update_info_from_link (icon_file);

	nemo_desktop_update_metadata_from_keyfile (file, file->details->name);

	nemo_directory_add_file (directory, file);

	list.data = file;
	list.next = NULL;
	list.prev = NULL;
	nemo_directory_emit_files_added (directory, &list);

	return icon_file;
}

/* Note: This can return NULL if the link was recently removed (i.e. unmounted) */
NemoDesktopLink *
nemo_desktop_icon_file_get_link (NemoDesktopIconFile *icon_file)
{
	if (icon_file->details->link)
		return g_object_ref (icon_file->details->link);
	else
		return NULL;
}

static void
nemo_desktop_icon_file_unmount (NemoFile                   *file,
				    GMountOperation                *mount_op,
				    GCancellable                   *cancellable,
				    NemoFileOperationCallback   callback,
				    gpointer                        callback_data)
{
	NemoDesktopIconFile *desktop_file;
	GMount *mount;
	
	desktop_file = NEMO_DESKTOP_ICON_FILE (file);
	if (desktop_file) {
		mount = nemo_desktop_link_get_mount (desktop_file->details->link);
		if (mount != NULL) {
			nemo_file_operations_unmount_mount (NULL, mount, FALSE, TRUE);
		}
	}
	
}

static void
nemo_desktop_icon_file_eject (NemoFile                   *file,
				  GMountOperation                *mount_op,
				  GCancellable                   *cancellable,
				  NemoFileOperationCallback   callback,
				  gpointer                        callback_data)
{
	NemoDesktopIconFile *desktop_file;
	GMount *mount;
	
	desktop_file = NEMO_DESKTOP_ICON_FILE (file);
	if (desktop_file) {
		mount = nemo_desktop_link_get_mount (desktop_file->details->link);
		if (mount != NULL) {
			nemo_file_operations_unmount_mount (NULL, mount, TRUE, TRUE);
		}
	}
}

static void
nemo_desktop_icon_file_set_metadata (NemoFile           *file,
					 const char             *key,
					 const char             *value)
{
	nemo_desktop_set_metadata_string (file, file->details->name, key, value);
}

static void
nemo_desktop_icon_file_set_metadata_as_list (NemoFile           *file,
						 const char             *key,
						 char                  **value)
{
	nemo_desktop_set_metadata_stringv (file, file->details->name, key, (const gchar **) value);
}

static void
nemo_desktop_icon_file_class_init (NemoDesktopIconFileClass *klass)
{
	GObjectClass *object_class;
	NemoFileClass *file_class;

	object_class = G_OBJECT_CLASS (klass);
	file_class = NEMO_FILE_CLASS (klass);

	file_class->default_file_type = G_FILE_TYPE_DIRECTORY;
	
	file_class->monitor_add = desktop_icon_file_monitor_add;
	file_class->monitor_remove = desktop_icon_file_monitor_remove;
	file_class->call_when_ready = desktop_icon_file_call_when_ready;
	file_class->cancel_call_when_ready = desktop_icon_file_cancel_call_when_ready;
	file_class->check_if_ready = desktop_icon_file_check_if_ready;
	file_class->get_item_count = desktop_icon_file_get_item_count;
	file_class->get_deep_counts = desktop_icon_file_get_deep_counts;
	file_class->get_date = desktop_icon_file_get_date;
	file_class->get_where_string = desktop_icon_file_get_where_string;
	file_class->set_metadata = nemo_desktop_icon_file_set_metadata;
	file_class->set_metadata_as_list = nemo_desktop_icon_file_set_metadata_as_list;
	file_class->unmount = nemo_desktop_icon_file_unmount;
	file_class->eject = nemo_desktop_icon_file_eject;

	g_type_class_add_private (object_class, sizeof(NemoDesktopIconFileDetails));
}

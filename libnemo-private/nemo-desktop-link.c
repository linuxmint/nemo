/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-desktop-link.c: Class that handles the links on the desktop
    
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
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "nemo-desktop-link.h"
#include "nemo-desktop-link-monitor.h"
#include "nemo-desktop-icon-file.h"
#include "nemo-directory-private.h"
#include "nemo-desktop-directory.h"
#include "nemo-icon-names.h"
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-trash-monitor.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <string.h>

struct NemoDesktopLinkDetails {
	NemoDesktopLinkType type;
        char *filename;
	char *display_name;
	GFile *activation_location;
	GIcon *icon;

	NemoDesktopIconFile *icon_file;
	
	GObject *signal_handler_obj;
	gulong signal_handler;

	/* Just for mount icons: */
	GMount *mount;
};

G_DEFINE_TYPE(NemoDesktopLink, nemo_desktop_link, G_TYPE_OBJECT)

static void
create_icon_file (NemoDesktopLink *link)
{
	link->details->icon_file = nemo_desktop_icon_file_new (link);
}

static void
nemo_desktop_link_changed (NemoDesktopLink *link)
{
	if (link->details->icon_file != NULL) {
		nemo_desktop_icon_file_update (link->details->icon_file);
	}
}

static void
mount_changed_callback (GMount *mount, NemoDesktopLink *link)
{
	g_free (link->details->display_name);
	if (link->details->activation_location) {
		g_object_unref (link->details->activation_location);
	}
	if (link->details->icon) {
		g_object_unref (link->details->icon);
	}
	
	link->details->display_name = g_mount_get_name (mount);
	link->details->activation_location = g_mount_get_default_location (mount);
	link->details->icon = g_mount_get_icon (mount);
	
	nemo_desktop_link_changed (link);
}

static void
trash_state_changed_callback (NemoTrashMonitor *trash_monitor,
			      gboolean state,
			      gpointer callback_data)
{
	NemoDesktopLink *link;

	link = NEMO_DESKTOP_LINK (callback_data);
	g_assert (link->details->type == NEMO_DESKTOP_LINK_TRASH);

	if (link->details->icon) {
		g_object_unref (link->details->icon);
	}
	link->details->icon = nemo_trash_monitor_get_icon ();

	nemo_desktop_link_changed (link);
}

NemoDesktopLink *
nemo_desktop_link_new (NemoDesktopLinkType type)
{
	NemoDesktopLink *link;

	link = NEMO_DESKTOP_LINK (g_object_new (NEMO_TYPE_DESKTOP_LINK, NULL));

	link->details->type = type;
	switch (type) {
	case NEMO_DESKTOP_LINK_HOME:
		link->details->filename = g_strdup ("home");
		link->details->display_name = g_strdup (_("Home"));
		link->details->activation_location = g_file_new_for_path (g_get_home_dir ());
		link->details->icon = g_themed_icon_new (NEMO_ICON_HOME);
		
		break;

	case NEMO_DESKTOP_LINK_COMPUTER:
		link->details->filename = g_strdup ("computer");
		link->details->display_name = g_strdup (_("Computer"));
		link->details->activation_location = g_file_new_for_uri ("computer:///");
		/* TODO: This might need a different icon: */
		link->details->icon = g_themed_icon_new (NEMO_ICON_COMPUTER);

		break;

	case NEMO_DESKTOP_LINK_TRASH:
		link->details->filename = g_strdup ("trash");
		link->details->display_name = g_strdup (_("Trash"));
		link->details->activation_location = g_file_new_for_uri (EEL_TRASH_URI);
		link->details->icon = nemo_trash_monitor_get_icon ();

		link->details->signal_handler_obj = G_OBJECT (nemo_trash_monitor_get ());
		link->details->signal_handler =
			g_signal_connect_object (nemo_trash_monitor_get (), "trash_state_changed",
						 G_CALLBACK (trash_state_changed_callback), link, 0);
		break;

	case NEMO_DESKTOP_LINK_NETWORK:
		link->details->filename = g_strdup ("network");
		link->details->display_name = g_strdup (_("Network"));
		link->details->activation_location = g_file_new_for_uri ("network:///");
		link->details->icon = g_themed_icon_new (NEMO_ICON_NETWORK);
		
		break;

	default:
	case NEMO_DESKTOP_LINK_MOUNT:
		g_assert_not_reached();
	}

	create_icon_file (link);

	return link;
}

NemoDesktopLink *
nemo_desktop_link_new_from_mount (GMount *mount)
{
	NemoDesktopLink *link;
	GVolume *volume;
	char *name, *filename;

	link = NEMO_DESKTOP_LINK (g_object_new (NEMO_TYPE_DESKTOP_LINK, NULL));

	link->details->type = NEMO_DESKTOP_LINK_MOUNT;

	link->details->mount = g_object_ref (mount);

	/* We try to use the drive name to get somewhat stable filenames
	   for metadata */
	volume = g_mount_get_volume (mount);
	if (volume != NULL) {
		name = g_volume_get_name (volume);
		g_object_unref (volume);
	} else {
		name = g_mount_get_name (mount);
	}

	/* Replace slashes in name */
	filename = g_strconcat (g_strdelimit (name, "/", '-'), ".volume", NULL);
	link->details->filename =
		nemo_desktop_link_monitor_make_filename_unique (nemo_desktop_link_monitor_get (),
								    filename);
	g_free (filename);
	g_free (name);
	
	link->details->display_name = g_mount_get_name (mount);
	
	link->details->activation_location = g_mount_get_default_location (mount);
	link->details->icon = g_mount_get_icon (mount);
	
	link->details->signal_handler_obj = G_OBJECT (mount);
	link->details->signal_handler =
		g_signal_connect (mount, "changed",
				  G_CALLBACK (mount_changed_callback), link);
	
	create_icon_file (link);

	return link;
}

GMount *
nemo_desktop_link_get_mount (NemoDesktopLink *link)
{
	if (link->details->mount) {
		return g_object_ref (link->details->mount);
	}
	return NULL;
}

NemoDesktopLinkType
nemo_desktop_link_get_link_type (NemoDesktopLink *link)
{
	return link->details->type;
}

char *
nemo_desktop_link_get_file_name (NemoDesktopLink *link)
{
	return g_strdup (link->details->filename);
}

char *
nemo_desktop_link_get_display_name (NemoDesktopLink *link)
{
	return g_strdup (link->details->display_name);
}

GIcon *
nemo_desktop_link_get_icon (NemoDesktopLink *link)
{
	if (link->details->icon != NULL) {
		return g_object_ref (link->details->icon);
	}
	return NULL;
}

GFile *
nemo_desktop_link_get_activation_location (NemoDesktopLink *link)
{
	if (link->details->activation_location) {
		return g_object_ref (link->details->activation_location);
	}
	return NULL;
}

char *
nemo_desktop_link_get_activation_uri (NemoDesktopLink *link)
{
	if (link->details->activation_location) {
		return g_file_get_uri (link->details->activation_location);
	}
	return NULL;
}


gboolean
nemo_desktop_link_get_date (NemoDesktopLink *link,
				NemoDateType     date_type,
				time_t               *date)
{
	return FALSE;
}

gboolean
nemo_desktop_link_can_rename (NemoDesktopLink     *link)
{
	return (link->details->type == NEMO_DESKTOP_LINK_HOME ||
		link->details->type == NEMO_DESKTOP_LINK_TRASH ||
		link->details->type == NEMO_DESKTOP_LINK_NETWORK ||
		link->details->type == NEMO_DESKTOP_LINK_COMPUTER);
}

gboolean
nemo_desktop_link_rename (NemoDesktopLink     *link,
			      const char              *name)
{
	switch (link->details->type) {
	default:
		g_assert_not_reached ();
		/* FIXME: Do we want volume renaming?
		 * We didn't support that before. */
		break;
	}

	return TRUE;
}

static void
nemo_desktop_link_init (NemoDesktopLink *link)
{
	link->details = G_TYPE_INSTANCE_GET_PRIVATE (link,
						     NEMO_TYPE_DESKTOP_LINK,
						     NemoDesktopLinkDetails);
}

static void
desktop_link_finalize (GObject *object)
{
	NemoDesktopLink *link;

	link = NEMO_DESKTOP_LINK (object);

	if (link->details->signal_handler != 0) {
		g_signal_handler_disconnect (link->details->signal_handler_obj,
					     link->details->signal_handler);
	}

	if (link->details->icon_file != NULL) {
		nemo_desktop_icon_file_remove (link->details->icon_file);
		nemo_file_unref (NEMO_FILE (link->details->icon_file));
		link->details->icon_file = NULL;
	}

	if (link->details->type == NEMO_DESKTOP_LINK_MOUNT) {
		g_object_unref (link->details->mount);
	}

	g_free (link->details->filename);
	g_free (link->details->display_name);
	if (link->details->activation_location) {
		g_object_unref (link->details->activation_location);
	}
	if (link->details->icon) {
		g_object_unref (link->details->icon);
	}

	G_OBJECT_CLASS (nemo_desktop_link_parent_class)->finalize (object);
}

static void
nemo_desktop_link_class_init (NemoDesktopLinkClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = desktop_link_finalize;

	g_type_class_add_private (object_class, sizeof(NemoDesktopLinkDetails));
}

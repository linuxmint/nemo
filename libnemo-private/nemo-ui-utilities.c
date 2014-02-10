/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-ui-utilities.c - helper functions for GtkUIManager stuff

   Copyright (C) 2004 Red Hat, Inc.

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
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>

#include "nemo-ui-utilities.h"
#include "nemo-icon-info.h"
#include <eel/eel-graphic-effects.h>

#include <gio/gio.h>
#include <gtk/gtk.h>

void
nemo_ui_unmerge_ui (GtkUIManager *ui_manager,
			guint *merge_id,
			GtkActionGroup **action_group)
{
	if (*merge_id != 0) {
		gtk_ui_manager_remove_ui (ui_manager,
					  *merge_id);
		*merge_id = 0;
	}
	if (*action_group != NULL) {
		gtk_ui_manager_remove_action_group (ui_manager,
						    *action_group);
		*action_group = NULL;
	}
}
     
void
nemo_ui_prepare_merge_ui (GtkUIManager *ui_manager,
			      const char *name,
			      guint *merge_id,
			      GtkActionGroup **action_group)
{
	*merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	*action_group = gtk_action_group_new (name);
	gtk_action_group_set_translation_domain (*action_group, GETTEXT_PACKAGE);
	gtk_ui_manager_insert_action_group (ui_manager, *action_group, 0);
	g_object_unref (*action_group); /* owned by ui manager */
}

static void
extension_action_callback (GtkAction *action,
			   gpointer callback_data)
{
	nemo_menu_item_activate (NEMO_MENU_ITEM (callback_data));
}

GtkAction *
nemo_action_from_menu_item (NemoMenuItem *item)
{
	char *name, *label, *tip, *icon_name;
	gboolean sensitive, priority;
	GtkAction *action;
	GdkPixbuf *pixbuf;

	g_object_get (G_OBJECT (item),
		      "name", &name, "label", &label,
		      "tip", &tip, "icon", &icon_name,
		      "sensitive", &sensitive,
		      "priority", &priority,
		      NULL);

	action = gtk_action_new (name,
				 label,
				 tip,
				 NULL);

	if (icon_name != NULL) {
		pixbuf = nemo_ui_get_menu_icon (icon_name);
		if (pixbuf != NULL) {
			gtk_action_set_gicon (action, G_ICON (pixbuf));
			g_object_unref (pixbuf);
		}
	}

	gtk_action_set_sensitive (action, sensitive);
	g_object_set (action, "is-important", priority, NULL);

	g_signal_connect_data (action, "activate",
			       G_CALLBACK (extension_action_callback),
			       g_object_ref (item),
			       (GClosureNotify)g_object_unref, 0);

	g_free (name);
	g_free (label);
	g_free (tip);
	g_free (icon_name);

	return action;
}

GdkPixbuf *
nemo_ui_get_menu_icon (const char *icon_name)
{
	NemoIconInfo *info;
	GdkPixbuf *pixbuf;
	int size;

	size = nemo_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);

	if (g_path_is_absolute (icon_name)) {
		info = nemo_icon_info_lookup_from_path (icon_name, size);
	} else {
		info = nemo_icon_info_lookup_from_name (icon_name, size);
	}
	pixbuf = nemo_icon_info_get_pixbuf_nodefault_at_size (info, size);
	g_object_unref (info);

	return pixbuf;
}

char *
nemo_escape_action_name (const char *action_name,
			     const char *prefix)
{
	GString *s;

	if (action_name == NULL) {
		return NULL;
	}

	s = g_string_new (prefix);

	while (*action_name != 0) {
		switch (*action_name) {
		case '\\':
			g_string_append (s, "\\\\");
			break;
		case '/':
			g_string_append (s, "\\s");
			break;
		case '&':
			g_string_append (s, "\\a");
			break;
		case '"':
			g_string_append (s, "\\q");
			break;
		default:
			g_string_append_c (s, *action_name);
		}

		action_name ++;
	}
	return g_string_free (s, FALSE);
}

static GdkPixbuf *
nautilus_get_thumbnail_frame (void)
{
	static GdkPixbuf *thumbnail_frame = NULL;

	if (thumbnail_frame == NULL) {
		GInputStream *stream = g_resources_open_stream
			("/org/gnome/nautilus/icons/thumbnail_frame.png", 0, NULL);
		if (stream != NULL) {
			thumbnail_frame = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
			g_object_unref (stream);
		}
	}

	return thumbnail_frame;
}

#define NEMO_THUMBNAIL_FRAME_LEFT 3
#define NEMO_THUMBNAIL_FRAME_TOP 3
#define NEMO_THUMBNAIL_FRAME_RIGHT 3
#define NEMO_THUMBNAIL_FRAME_BOTTOM 3

void
nemo_ui_frame_image (GdkPixbuf **pixbuf)
{
	GdkPixbuf *pixbuf_with_frame, *frame;
	int left_offset, top_offset, right_offset, bottom_offset;
	int size;

	frame = nautilus_get_thumbnail_frame ();
	if (frame == NULL) {
		return;
	}

	size = MAX (gdk_pixbuf_get_width (*pixbuf),
		    gdk_pixbuf_get_height (*pixbuf));

	/* We don't want frames around small icons */
	if (size < 128 && gdk_pixbuf_get_has_alpha (*pixbuf)) {
		return;
	}

	left_offset = NEMO_THUMBNAIL_FRAME_LEFT;
	top_offset = NEMO_THUMBNAIL_FRAME_TOP;
	right_offset = NEMO_THUMBNAIL_FRAME_RIGHT;
	bottom_offset = NEMO_THUMBNAIL_FRAME_BOTTOM;

	pixbuf_with_frame = eel_embed_image_in_frame
		(*pixbuf, frame,
		 left_offset, top_offset, right_offset, bottom_offset);
	g_object_unref (*pixbuf);

	*pixbuf = pixbuf_with_frame;
}

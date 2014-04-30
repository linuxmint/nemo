/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-ui-utilities.c - helper functions for GtkUIManager stuff

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
   see <http://www.gnu.org/licenses/>.

   Authors: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>

#include "nautilus-ui-utilities.h"
#include "nautilus-icon-info.h"
#include <eel/eel-graphic-effects.h>

#include <gio/gio.h>
#include <gtk/gtk.h>

void
nautilus_ui_unmerge_ui (GtkUIManager *ui_manager,
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
nautilus_ui_prepare_merge_ui (GtkUIManager *ui_manager,
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
	nautilus_menu_item_activate (NAUTILUS_MENU_ITEM (callback_data));
}

GtkAction *
nautilus_action_from_menu_item (NautilusMenuItem *item,
				GtkWidget        *parent_widget)
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
		pixbuf = nautilus_ui_get_menu_icon (icon_name, parent_widget);
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
nautilus_ui_get_menu_icon (const char *icon_name,
			   GtkWidget  *parent_widget)
{
	NautilusIconInfo *info;
	GdkPixbuf *pixbuf;
	int size;
	int scale;

	size = nautilus_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
	scale = gtk_widget_get_scale_factor (parent_widget);

	if (g_path_is_absolute (icon_name)) {
		info = nautilus_icon_info_lookup_from_path (icon_name, size, scale);
	} else {
		info = nautilus_icon_info_lookup_from_name (icon_name, size, scale);
	}
	pixbuf = nautilus_icon_info_get_pixbuf_nodefault_at_size (info, size);
	g_object_unref (info);

	return pixbuf;
}

char *
nautilus_escape_action_name (const char *action_name,
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
		thumbnail_frame = gdk_pixbuf_new_from_resource ("/org/gnome/nautilus/icons/thumbnail_frame.png", NULL);
	}

	return thumbnail_frame;
}

#define NAUTILUS_THUMBNAIL_FRAME_LEFT 3
#define NAUTILUS_THUMBNAIL_FRAME_TOP 3
#define NAUTILUS_THUMBNAIL_FRAME_RIGHT 3
#define NAUTILUS_THUMBNAIL_FRAME_BOTTOM 3

void
nautilus_ui_frame_image (GdkPixbuf **pixbuf)
{
	GdkPixbuf *pixbuf_with_frame, *frame;
	int left_offset, top_offset, right_offset, bottom_offset;

	frame = nautilus_get_thumbnail_frame ();
	if (frame == NULL) {
		return;
	}

	left_offset = NAUTILUS_THUMBNAIL_FRAME_LEFT;
	top_offset = NAUTILUS_THUMBNAIL_FRAME_TOP;
	right_offset = NAUTILUS_THUMBNAIL_FRAME_RIGHT;
	bottom_offset = NAUTILUS_THUMBNAIL_FRAME_BOTTOM;

	pixbuf_with_frame = eel_embed_image_in_frame
		(*pixbuf, frame,
		 left_offset, top_offset, right_offset, bottom_offset);
	g_object_unref (*pixbuf);

	*pixbuf = pixbuf_with_frame;
}

static GdkPixbuf *filmholes_left = NULL;
static GdkPixbuf *filmholes_right = NULL;

static gboolean
ensure_filmholes (void)
{
	if (filmholes_left == NULL) {
		filmholes_left = gdk_pixbuf_new_from_resource ("/org/gnome/nautilus/icons/filmholes.png", NULL);
	}
	if (filmholes_right == NULL &&
	    filmholes_left != NULL) {
		filmholes_right = gdk_pixbuf_flip (filmholes_left, TRUE);
	}

	return (filmholes_left && filmholes_right);
}

void
nautilus_ui_frame_video (GdkPixbuf **pixbuf)
{
	int width, height;
	int holes_width, holes_height;
	int i;

	if (!ensure_filmholes ())
		return;

	width = gdk_pixbuf_get_width (*pixbuf);
	height = gdk_pixbuf_get_height (*pixbuf);
	holes_width = gdk_pixbuf_get_width (filmholes_left);
	holes_height = gdk_pixbuf_get_height (filmholes_left);

	for (i = 0; i < height; i += holes_height) {
		gdk_pixbuf_composite (filmholes_left, *pixbuf, 0, i,
				      MIN (width, holes_width),
				      MIN (height - i, holes_height),
				      0, i, 1, 1, GDK_INTERP_NEAREST, 255);
	}

	for (i = 0; i < height; i += holes_height) {
		gdk_pixbuf_composite (filmholes_right, *pixbuf,
				      width - holes_width, i,
				      MIN (width, holes_width),
				      MIN (height - i, holes_height),
				      width - holes_width, i,
				      1, 1, GDK_INTERP_NEAREST, 255);
	}
}

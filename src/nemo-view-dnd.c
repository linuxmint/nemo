/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-view-dnd.c: DnD helpers for NemoView
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000, 2001  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Ettore Perazzoli
 * 	    Darin Adler <darin@bentspoon.com>
 * 	    John Sullivan <sullivan@eazel.com>
 *          Pavel Cisler <pavel@eazel.com>
 */

#include <config.h>

#include "nemo-view-dnd.h"

#include "nemo-view.h"

#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>

#include <glib/gi18n.h>

#include <libnemo-private/nemo-clipboard.h>
#include <libnemo-private/nemo-dnd.h>

#define GET_ANCESTOR(obj) \
	GTK_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (obj), GTK_TYPE_WINDOW))

static inline void
view_widget_to_file_operation_position (NemoView *view,
                                        GdkPoint *position)
{
	NemoViewClass *class = NEMO_VIEW_GET_CLASS (view);

	if (class->widget_to_file_operation_position != NULL) {
		class->widget_to_file_operation_position (view, position);
	}
}

static void
view_widget_to_file_operation_position_xy (NemoView *view,
                                           int *x, int *y)
{
	GdkPoint position;

	position.x = *x;
	position.y = *y;
	view_widget_to_file_operation_position (view, &position);
	*x = position.x;
	*y = position.y;
}

typedef struct {
	NemoView *view;
	char *link_name;
	char *target_uri;
	char *url;
	GdkPoint point;
} NetscapeUrlDropLink;

static void
revert_slashes (char *string)
{
	while (*string != 0) {
		if (*string == '/') {
			*string = '\\';
		}
		string++;
	}
}

static void
handle_netscape_url_drop_link_cb (GObject *source_object,
				  GAsyncResult *res,
				  gpointer user_data)
{
	NetscapeUrlDropLink *data = user_data;
	char *link_name = data->link_name;
	char *link_display_name;
	gint screen_num;
	GFileInfo *info;
	char *icon_name = NULL;
	GdkScreen *screen;

	info = g_file_query_info_finish (G_FILE (source_object),
					 res, NULL);

	if (info != NULL) {
		GIcon *icon;
		const char * const *names;

		icon = g_file_info_get_icon (info);

		if (G_IS_THEMED_ICON (icon)) {
			names = g_themed_icon_get_names (G_THEMED_ICON (icon));
			icon_name = g_strdup (names[0]);
		}
	
		g_object_unref (info);
	}

	if (icon_name == NULL) {
		icon_name = g_strdup ("text-html");
	}

	link_display_name = g_strdup_printf (_("Link to %s"), link_name);

	/* The filename can't contain slashes, strip em.
	   (the basename of http://foo/ is http://foo/) */
	revert_slashes (link_name);

	screen = gtk_widget_get_screen (GTK_WIDGET (data->view));
	screen_num = gdk_screen_get_number (screen);

	nemo_link_local_create (data->target_uri,
				    link_name,
				    link_display_name,
				    icon_name,
				    data->url,
				    &data->point,
				    screen_num,
				    TRUE);

	g_free (link_display_name);
	g_free (icon_name);

	g_free (data->url);
	g_free (data->link_name);
	g_free (data->target_uri);

	g_object_unref (data->view);
	g_slice_free (NetscapeUrlDropLink, data);
}

void
nemo_view_handle_netscape_url_drop (NemoView  *view,
                                        const char    *encoded_url,
                                        const char    *target_uri,
                                        GdkDragAction  action,
                                        int            x,
                                        int            y)
{
	char *url, *title;
	char *link_name;
	GArray *points;
	char **bits;
	GList *uri_list = NULL;
	GFile *f;

	f = g_file_new_for_uri (target_uri);

	if (!g_file_is_native (f)) {
		eel_show_warning_dialog (_("Drag and drop is not supported."),
					 _("Drag and drop is only supported on local file systems."),
                                         GET_ANCESTOR (view));
		g_object_unref (f);
		return;
	}

	g_object_unref (f);

	/* _NETSCAPE_URL_ works like this: $URL\n$TITLE */
	bits = g_strsplit (encoded_url, "\n", 0);
	switch (g_strv_length (bits)) {
	case 0:
		g_strfreev (bits);
		return;
	case 1:
		url = bits[0];
		title = NULL;
		break;
	default:
		url = bits[0];
		title = bits[1];
	}

	f = g_file_new_for_uri (url);

	view_widget_to_file_operation_position_xy (view, &x, &y);

	/* We don't support GDK_ACTION_ASK or GDK_ACTION_PRIVATE
	 * and we don't support combinations either. */
	if ((action != GDK_ACTION_DEFAULT) &&
	    (action != GDK_ACTION_COPY) &&
	    (action != GDK_ACTION_MOVE) &&
	    (action != GDK_ACTION_LINK)) {
		eel_show_warning_dialog (_("Drag and drop is not supported."),
					 _("An invalid drag type was used."),
                                         GET_ANCESTOR (view));
		return;
	}

	if (action == GDK_ACTION_LINK) {
		if (g_strcmp0 (title, NULL) == 0) {
			link_name = g_file_get_basename (f);
		} else {
			link_name = g_strdup (title);
		}

		if (g_strcmp0 (link_name, NULL) != 0) {
			NetscapeUrlDropLink *data;

			data = g_slice_new0 (NetscapeUrlDropLink);
			data->link_name = link_name;
			data->point.x = x;
			data->point.y = y;
			data->view = g_object_ref (view);
			data->target_uri = g_strdup (target_uri);
			data->url = g_strdup (url);

			g_file_query_info_async (f,
						 G_FILE_ATTRIBUTE_STANDARD_ICON,
						 0, 0, NULL,
						 handle_netscape_url_drop_link_cb,
						 data);
		}
	} else {
		GdkPoint tmp_point = { 0, 0 };

		/* pass in a 1-item array of icon positions, relative to x, y */
		points = g_array_new (FALSE, TRUE, sizeof (GdkPoint));
		g_array_append_val (points, tmp_point);

		uri_list = g_list_append (uri_list, url);

		nemo_view_move_copy_items (view, uri_list, points,
                                               target_uri,
                                               action, x, y);

		g_list_free (uri_list);
		g_array_free (points, TRUE);
	}

	g_object_unref (f);
	g_strfreev (bits);
}

void
nemo_view_handle_uri_list_drop (NemoView  *view,
                                    const char    *item_uris,
                                    const char    *target_uri,
                                    GdkDragAction  action,
                                    int            x,
                                    int            y)
{
	gchar **uri_list;
	GList *real_uri_list = NULL;
	char *container_uri;
	int n_uris, i;
	GArray *points;

	if (item_uris == NULL) {
		return;
	}

	container_uri = NULL;
	if (target_uri == NULL) {
		container_uri = nemo_view_get_backing_uri (view);
		g_assert (container_uri != NULL);
	}

	if (action == GDK_ACTION_ASK) {
		action = nemo_drag_drop_action_ask
			(GTK_WIDGET (view),
			 GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);
		if (action == 0) {
			g_free (container_uri);
			return;
		}
	}

	/* We don't support GDK_ACTION_ASK or GDK_ACTION_PRIVATE
	 * and we don't support combinations either. */
	if ((action != GDK_ACTION_DEFAULT) &&
	    (action != GDK_ACTION_COPY) &&
	    (action != GDK_ACTION_MOVE) &&
	    (action != GDK_ACTION_LINK)) {
		eel_show_warning_dialog (_("Drag and drop is not supported."),
					 _("An invalid drag type was used."),
                                         GET_ANCESTOR (view));
		g_free (container_uri);
		return;
	}

	n_uris = 0;
	uri_list = g_uri_list_extract_uris (item_uris);
	for (i = 0; uri_list[i] != NULL; i++) {
		real_uri_list = g_list_append (real_uri_list, uri_list[i]);
		n_uris++;
	}
	g_free (uri_list);

	/* do nothing if no real uris are left */
	if (n_uris == 0) {
		g_free (container_uri);
		return;
	}

	if (n_uris == 1) {
		GdkPoint tmp_point = { 0, 0 };

		/* pass in a 1-item array of icon positions, relative to x, y */
		points = g_array_new (FALSE, TRUE, sizeof (GdkPoint));
		g_array_append_val (points, tmp_point);
	} else {
		points = NULL;
	}

	view_widget_to_file_operation_position_xy (view, &x, &y);

	nemo_view_move_copy_items (view, real_uri_list, points,
				       target_uri != NULL ? target_uri : container_uri,
				       action, x, y);

	g_list_free_full (real_uri_list, g_free);

	if (points != NULL)
		g_array_free (points, TRUE);

	g_free (container_uri);
}

void
nemo_view_handle_text_drop (NemoView  *view,
                                const char    *text,
                                const char    *target_uri,
                                GdkDragAction  action,
                                int            x,
                                int            y)
{
	int length;
	char *container_uri;
	GdkPoint pos;

	if (text == NULL) {
		return;
	}

	g_return_if_fail (action == GDK_ACTION_COPY);

	container_uri = NULL;
	if (target_uri == NULL) {
		container_uri = nemo_view_get_backing_uri (view);
		g_assert (container_uri != NULL);
	}

	length = strlen (text);

	pos.x = x;
	pos.y = y;
	view_widget_to_file_operation_position (view, &pos);

	nemo_view_new_file_with_initial_contents (
		view, target_uri != NULL ? target_uri : container_uri,
		/* Translator: This is the filename used for when you dnd text to a directory */
		_("dropped text.txt"),
		text, length, &pos);

	g_free (container_uri);
}

void
nemo_view_handle_raw_drop (NemoView *view,
                               const char   *raw_data,
                               int           length,
                               const char   *target_uri,
                               const char   *direct_save_uri,
                               GdkDragAction action,
                               int           x,
                               int           y)
{
	char *container_uri, *filename;
	GFile *direct_save_full;
	GdkPoint pos;

	if (raw_data == NULL) {
		return;
	}

	g_return_if_fail (action == GDK_ACTION_COPY);

	container_uri = NULL;
	if (target_uri == NULL) {
		container_uri = nemo_view_get_backing_uri (view);
		g_assert (container_uri != NULL);
	}

	pos.x = x;
	pos.y = y;
	view_widget_to_file_operation_position (view, &pos);

	filename = NULL;
	if (direct_save_uri != NULL) {
		direct_save_full = g_file_new_for_uri (direct_save_uri);
		filename = g_file_get_basename (direct_save_full);
	}
	if (filename == NULL) {
		/* Translator: This is the filename used for when you dnd raw
		 * data to a directory, if the source didn't supply a name.
		 */
		filename = g_strdup (_("dropped data"));
	}

	nemo_view_new_file_with_initial_contents (
		view, target_uri != NULL ? target_uri : container_uri,
		filename, raw_data, length, &pos);

	g_free (container_uri);
	g_free (filename);
}

void 
nemo_view_drop_proxy_received_uris (NemoView *view,
					const GList *source_uri_list,
					const char *target_uri,
					GdkDragAction action)
{
	char *container_uri;

	container_uri = NULL;
	if (target_uri == NULL) {
		container_uri = nemo_view_get_backing_uri (view);
		g_assert (container_uri != NULL);
	}

	if (action == GDK_ACTION_ASK) {
		action = nemo_drag_drop_action_ask
			(GTK_WIDGET (view),
			 GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);
		if (action == 0) {
			return;
		}
	}

	nemo_clipboard_clear_if_colliding_uris (GTK_WIDGET (view),
						    source_uri_list,
						    nemo_view_get_copied_files_atom (view));

	nemo_view_move_copy_items (view, source_uri_list, NULL,
				       target_uri != NULL ? target_uri : container_uri,
				       action, 0, 0);

	g_free (container_uri);
}

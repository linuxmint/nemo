/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-ui-utilities.h - helper functions for GtkUIManager stuff

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
#ifndef NAUTILUS_UI_UTILITIES_H
#define NAUTILUS_UI_UTILITIES_H

#include <gtk/gtk.h>
#include <libnautilus-extension/nautilus-menu-item.h>

void        nautilus_ui_unmerge_ui                 (GtkUIManager      *ui_manager,
						    guint             *merge_id,
						    GtkActionGroup   **action_group);
void        nautilus_ui_prepare_merge_ui           (GtkUIManager      *ui_manager,
						    const char        *name,
						    guint             *merge_id,
						    GtkActionGroup   **action_group);
GtkAction * nautilus_action_from_menu_item         (NautilusMenuItem  *item,
						    GtkWidget         *parent_widget);

GdkPixbuf * nautilus_ui_get_menu_icon              (const char        *icon_name,
						    GtkWidget         *parent_widget);

char * nautilus_escape_action_name                 (const char        *action_name,
						    const char        *prefix);
void   nautilus_ui_frame_image                     (GdkPixbuf        **pixbuf);
void   nautilus_ui_frame_video                     (GdkPixbuf        **pixbuf);

#endif /* NAUTILUS_UI_UTILITIES_H */

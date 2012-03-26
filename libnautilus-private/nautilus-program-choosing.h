/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-program-choosing.h - functions for selecting and activating
 				 programs for opening/viewing particular files.

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

   Author: John Sullivan <sullivan@eazel.com>
*/

#ifndef NAUTILUS_PROGRAM_CHOOSING_H
#define NAUTILUS_PROGRAM_CHOOSING_H

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <libnautilus-private/nautilus-file.h>

typedef void (*NautilusApplicationChoiceCallback) (GAppInfo                      *application,
						   gpointer			  callback_data);

void nautilus_launch_application                 (GAppInfo                          *application,
						  GList                             *files,
						  GtkWindow                         *parent_window);
void nautilus_launch_application_by_uri          (GAppInfo                          *application,
						  GList                             *uris,
						  GtkWindow                         *parent_window);
void nautilus_launch_application_for_mount       (GAppInfo                          *app_info,
						  GMount                            *mount,
						  GtkWindow                         *parent_window);
void nautilus_launch_application_from_command    (GdkScreen                         *screen,
						  const char                        *command_string,
						  gboolean                           use_terminal,
						  ...) G_GNUC_NULL_TERMINATED;
void nautilus_launch_application_from_command_array (GdkScreen                         *screen,
						     const char                        *command_string,
						     gboolean                           use_terminal,
						     const char * const *               parameters);
void nautilus_launch_desktop_file		 (GdkScreen                         *screen,
						  const char                        *desktop_file_uri,
						  const GList                       *parameter_uris,
						  GtkWindow                         *parent_window);
						  
#endif /* NAUTILUS_PROGRAM_CHOOSING_H */

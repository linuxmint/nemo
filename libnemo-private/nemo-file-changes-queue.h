/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.
  
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

   Author: Pavel Cisler <pavel@eazel.com>  
*/

#ifndef NEMO_FILE_CHANGES_QUEUE_H
#define NEMO_FILE_CHANGES_QUEUE_H

#include <gdk/gdk.h>
#include <gio/gio.h>

void nemo_file_changes_queue_file_added                      (GFile      *location);
void nemo_file_changes_queue_file_changed                    (GFile      *location);
void nemo_file_changes_queue_file_removed                    (GFile      *location);
void nemo_file_changes_queue_file_moved                      (GFile      *from,
								  GFile      *to);
void nemo_file_changes_queue_schedule_position_set           (GFile      *location,
								  GdkPoint    point,
								  int         screen);
void nemo_file_changes_queue_schedule_position_remove        (GFile      *location);

void nemo_file_changes_consume_changes                       (gboolean    consume_all);


#endif /* NEMO_FILE_CHANGES_QUEUE_H */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-monitor.h: file and directory change monitoring for nemo
 
   Copyright (C) 2000, 2001 Eazel, Inc.
  
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
  
   Authors: Seth Nickell <seth@eazel.com>
            Darin Adler <darin@bentspoon.com>
*/

#ifndef NEMO_MONITOR_H
#define NEMO_MONITOR_H

#include <glib.h>
#include <gio/gio.h>

typedef struct NemoMonitor NemoMonitor;

gboolean         nemo_monitor_active    (void);
NemoMonitor *nemo_monitor_directory (GFile *location);
void             nemo_monitor_cancel    (NemoMonitor *monitor);

#endif /* NEMO_MONITOR_H */

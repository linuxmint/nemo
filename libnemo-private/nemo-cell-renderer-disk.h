/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 
   Copyright (C) 2007 Martin Wehner
  
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

   Author: Martin Wehner <martin.wehner@gmail.com>
*/

#ifndef NEMO_CELL_RENDERER_DISK_H
#define NEMO_CELL_RENDERER_DISK_H

#include <gtk/gtk.h>

#define NEMO_TYPE_CELL_RENDERER_DISK nemo_cell_renderer_disk_get_type()
#define NEMO_CELL_RENDERER_DISK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_CELL_RENDERER_DISK, NemoCellRendererDisk))
#define NEMO_CELL_RENDERER_DISK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_CELL_RENDERER_DISK, NemoCellRendererDiskClass))
#define NEMO_IS_CELL_RENDERER_DISK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_CELL_RENDERER_DISK))
#define NEMO_IS_CELL_RENDERER_DISK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_CELL_RENDERER_DISK))
#define NEMO_CELL_RENDERER_DISK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_CELL_RENDERER_DISK, NemoCellRendererDiskClass))


typedef struct _NemoCellRendererDisk NemoCellRendererDisk;
typedef struct _NemoCellRendererDiskClass NemoCellRendererDiskClass;

struct _NemoCellRendererDisk {
	GtkCellRendererText parent;
    guint disk_full_percent;
    gboolean show_disk_full_percent;
};

struct _NemoCellRendererDiskClass {
	GtkCellRendererTextClass parent_class;
};

GType		 nemo_cell_renderer_disk_get_type (void);
GtkCellRenderer *nemo_cell_renderer_disk_new      (void);

#endif /* NEMO_CELL_RENDERER_DISK_H */

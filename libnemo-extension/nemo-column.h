/*
 *  nemo-column.h - Info columns exported by 
 *                      NemoColumnProvider objects.
 *
 *  Copyright (C) 2003 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 * 
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

#ifndef NEMO_COLUMN_H
#define NEMO_COLUMN_H

#include <glib-object.h>
#include "nemo-extension-types.h"

G_BEGIN_DECLS

#define NEMO_TYPE_COLUMN            (nemo_column_get_type())
#define NEMO_COLUMN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_COLUMN, NemoColumn))
#define NEMO_COLUMN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_COLUMN, NemoColumnClass))
#define NEMO_INFO_IS_COLUMN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_COLUMN))
#define NEMO_INFO_IS_COLUMN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), NEMO_TYPE_COLUMN))
#define NEMO_COLUMN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NEMO_TYPE_COLUMN, NemoColumnClass))

typedef struct _NemoColumn        NemoColumn;
typedef struct _NemoColumnDetails NemoColumnDetails;
typedef struct _NemoColumnClass   NemoColumnClass;

struct _NemoColumn {
	GObject parent;

	NemoColumnDetails *details;
};

struct _NemoColumnClass {
	GObjectClass parent;
};

GType             nemo_column_get_type        (void);
NemoColumn *  nemo_column_new             (const char     *name,
						   const char     *attribute,
						   const char     *label,
						   const char     *description);

/* NemoColumn has the following properties:
 *   name (string)        - the identifier for the column
 *   attribute (string)   - the file attribute to be displayed in the 
 *                          column
 *   label (string)       - the user-visible label for the column
 *   description (string) - a user-visible description of the column
 *   xalign (float)       - x-alignment of the column 
 */

G_END_DECLS

#endif

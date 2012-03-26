/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-conflict-dialog: dialog that handles file conflicts
   during transfer operations.

   Copyright (C) 2008, Cosimo Cecchi

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
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
   
   Authors: Cosimo Cecchi <cosimoc@gnome.org>
*/

#ifndef NAUTILUS_FILE_CONFLICT_DIALOG_H
#define NAUTILUS_FILE_CONFLICT_DIALOG_H

#include <glib-object.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#define NAUTILUS_TYPE_FILE_CONFLICT_DIALOG \
	(nautilus_file_conflict_dialog_get_type ())
#define NAUTILUS_FILE_CONFLICT_DIALOG(o) \
	(G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_FILE_CONFLICT_DIALOG,\
				     NautilusFileConflictDialog))
#define NAUTILUS_FILE_CONFLICT_DIALOG_CLASS(k) \
	(G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_FILE_CONFLICT_DIALOG,\
				 NautilusFileConflictDialogClass))
#define NAUTILUS_IS_FILE_CONFLICT_DIALOG(o) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_FILE_CONFLICT_DIALOG))
#define NAUTILUS_IS_FILE_CONFLICT_DIALOG_CLASS(k) \
	(G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_FILE_CONFLICT_DIALOG))
#define NAUTILUS_FILE_CONFLICT_DIALOG_GET_CLASS(o) \
	(G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_FILE_CONFLICT_DIALOG,\
				    NautilusFileConflictDialogClass))

typedef struct _NautilusFileConflictDialog        NautilusFileConflictDialog;
typedef struct _NautilusFileConflictDialogClass   NautilusFileConflictDialogClass;
typedef struct _NautilusFileConflictDialogDetails NautilusFileConflictDialogDetails;

struct _NautilusFileConflictDialog {
	GtkDialog parent;
	NautilusFileConflictDialogDetails *details;
};

struct _NautilusFileConflictDialogClass {
	GtkDialogClass parent_class;
};

enum
{
	CONFLICT_RESPONSE_SKIP = 1,
	CONFLICT_RESPONSE_REPLACE = 2,
	CONFLICT_RESPONSE_RENAME = 3,
};

GType nautilus_file_conflict_dialog_get_type (void) G_GNUC_CONST;

GtkWidget* nautilus_file_conflict_dialog_new              (GtkWindow *parent,
							   GFile *source,
							   GFile *destination,
							   GFile *dest_dir);
char*      nautilus_file_conflict_dialog_get_new_name     (NautilusFileConflictDialog *dialog);
gboolean   nautilus_file_conflict_dialog_get_apply_to_all (NautilusFileConflictDialog *dialog);

#endif /* NAUTILUS_FILE_CONFLICT_DIALOG_H */

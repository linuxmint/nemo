/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-file-conflict-dialog: dialog that handles file conflicts
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

#ifndef NEMO_FILE_CONFLICT_DIALOG_H
#define NEMO_FILE_CONFLICT_DIALOG_H

#include <glib-object.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#define NEMO_TYPE_FILE_CONFLICT_DIALOG \
	(nemo_file_conflict_dialog_get_type ())
#define NEMO_FILE_CONFLICT_DIALOG(o) \
	(G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_FILE_CONFLICT_DIALOG,\
				     NemoFileConflictDialog))
#define NEMO_FILE_CONFLICT_DIALOG_CLASS(k) \
	(G_TYPE_CHECK_CLASS_CAST((k), NEMO_TYPE_FILE_CONFLICT_DIALOG,\
				 NemoFileConflictDialogClass))
#define NEMO_IS_FILE_CONFLICT_DIALOG(o) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((o), NEMO_TYPE_FILE_CONFLICT_DIALOG))
#define NEMO_IS_FILE_CONFLICT_DIALOG_CLASS(k) \
	(G_TYPE_CHECK_CLASS_TYPE ((k), NEMO_TYPE_FILE_CONFLICT_DIALOG))
#define NEMO_FILE_CONFLICT_DIALOG_GET_CLASS(o) \
	(G_TYPE_INSTANCE_GET_CLASS ((o), NEMO_TYPE_FILE_CONFLICT_DIALOG,\
				    NemoFileConflictDialogClass))

typedef struct _NemoFileConflictDialog        NemoFileConflictDialog;
typedef struct _NemoFileConflictDialogClass   NemoFileConflictDialogClass;
typedef struct _NemoFileConflictDialogDetails NemoFileConflictDialogDetails;

struct _NemoFileConflictDialog {
	GtkDialog parent;
	NemoFileConflictDialogDetails *details;
};

struct _NemoFileConflictDialogClass {
	GtkDialogClass parent_class;
};

enum
{
	CONFLICT_RESPONSE_SKIP = 1,
	CONFLICT_RESPONSE_REPLACE = 2,
	CONFLICT_RESPONSE_RENAME = 3,
};

GType nemo_file_conflict_dialog_get_type (void) G_GNUC_CONST;

GtkWidget* nemo_file_conflict_dialog_new              (GtkWindow *parent,
							   GFile *source,
							   GFile *destination,
							   GFile *dest_dir);
char*      nemo_file_conflict_dialog_get_new_name     (NemoFileConflictDialog *dialog);
gboolean   nemo_file_conflict_dialog_get_apply_to_all (NemoFileConflictDialog *dialog);

#endif /* NEMO_FILE_CONFLICT_DIALOG_H */
